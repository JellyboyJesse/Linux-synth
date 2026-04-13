#include "AudioEngine.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307179586f;
static constexpr float kPi    = 3.141592653589793f;

// log(i+1) for i = 0..3 — used for per-sample harmonic stretch calculation
static constexpr float kLogHarm[NUM_OSCILLATORS] = {
    0.0f,            // log(1)
    0.693147181f,    // log(2)
    1.098612289f,    // log(3)
    1.386294361f     // log(4)
};

//==============================================================================
AudioEngine::AudioEngine(SynthSharedState& sharedState)
    : state(sharedState)
{
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        oscs[i].frequency     = currentRootHz * float(i + 1);
        oscs[i].currentAmp    = state.globalAmplitudes[i].load(std::memory_order_relaxed);
        oscs[i].rampStartAmp  = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = oscs[i].currentAmp;
    }
    subOsc.frequency  = currentRootHz * 0.5f;
    subOsc.currentAmp = 0.0f;
}

AudioEngine::~AudioEngine() = default;

//==============================================================================
void AudioEngine::triggerStep(int step)
{
    currentStep = step;
    state.currentPlayStep.store(step, std::memory_order_relaxed);

    // Step duration → sample count
    const float bpm   = juce::jlimit(40.0f, 200.0f,
                            state.bpm.load(std::memory_order_relaxed));
    const float beats = juce::jlimit(0.1f, 8.0f,
                            state.stepDuration[step].load(std::memory_order_relaxed));
    samplesThisStep = juce::jmax(1, int(beats * 60.0f / bpm * sampleRate));

    // Root pitch (globalRootNote transposes all steps uniformly)
    const int rootNote = state.globalRootNote.load(std::memory_order_relaxed);
    const int pitch    = state.stepPitch[step].load(std::memory_order_relaxed);
    currentRootHz = semitoneToHz(pitch + rootNote);

    // Envelope
    const float atkMs = state.stepAttack[step].load(std::memory_order_relaxed);
    const float decMs = state.stepDecay[step] .load(std::memory_order_relaxed);
    envAttackSamps = int(juce::jmax(0.0f, atkMs) / 1000.0f * sampleRate);
    envDecaySamps  = int(juce::jmax(0.0f, decMs) / 1000.0f * sampleRate);
    envSamplePos   = 0;

    // Harmonic oscillator amplitude ramps
    const bool isCustom = state.stepIsCustom[step].load(std::memory_order_relaxed);
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        oscs[i].rampStartAmp  = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = isCustom
            ? state.stepAmplitudes[step][i].load(std::memory_order_relaxed)
            : state.globalAmplitudes[i]    .load(std::memory_order_relaxed);
        oscs[i].rampDuration  = samplesThisStep;
        oscs[i].rampProgress  = 0;
    }

    // Sub-oscillator amplitude ramp
    subOsc.rampStartAmp  = subOsc.currentAmp;
    subOsc.rampTargetAmp = state.subAmp[step].load(std::memory_order_relaxed);
    subOsc.rampDuration  = samplesThisStep;
    subOsc.rampProgress  = 0;

    // Effect ramp (same duration as oscillator ramps)
    effectRampProgress = 0;
    effectRampDuration = samplesThisStep;

    fxStretch   .trigger(state.stretchRatio[step].load(std::memory_order_relaxed));
    fxFreqShift .trigger(state.freqShift[step]   .load(std::memory_order_relaxed));
    fxFoldAmount.trigger(state.foldAmount[step]  .load(std::memory_order_relaxed));
    fxKsDecay   .trigger(state.ksDecay[step]     .load(std::memory_order_relaxed));
    fxKsTune    .trigger(state.ksTune[step]      .load(std::memory_order_relaxed));
    fxReverbSize.trigger(state.reverbSize[step]  .load(std::memory_order_relaxed));
    fxReverbDamp.trigger(state.reverbDamp[step]  .load(std::memory_order_relaxed));
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate         = float(device->getCurrentSampleRate());
    samplesThisStep    = 22050;
    sampleCounter      = 0;
    effectRampProgress = 0;
    effectRampDuration = 22050;
    envSamplePos       = 0;
    envAttackSamps     = 0;
    envDecaySamps      = 0;
    wasPlaying         = false;
    currentStep        = -1;
    ksWasActive        = false;
    ksCrossfadeProg    = kKsCrossfadeLen;
    reverbWasActive    = false;
}

void AudioEngine::audioDeviceStopped()
{
    state.currentPlayStep.store(-1, std::memory_order_relaxed);
    currentStep = -1;
    wasPlaying  = false;
}

//==============================================================================
void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const*,
    int,
    float* const*  outputChannelData,
    int            numOutputChannels,
    int            numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch])
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    if (numOutputChannels == 0 || numSamples == 0) return;

    float* outL = outputChannelData[0];
    float* outR = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    const bool  playing    = state.isPlaying .load(std::memory_order_relaxed);
    const float masterGain = state.masterGain.load(std::memory_order_relaxed);

    if (playing && !wasPlaying)
    {
        sampleCounter = 0;
        triggerStep(0);
    }
    else if (!playing && wasPlaying)
    {
        state.currentPlayStep.store(-1, std::memory_order_relaxed);
        currentStep = -1;
    }
    wasPlaying = playing;

    // ---- Per-sample loop ---------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        // Advance sequencer clock
        if (playing)
        {
            if (sampleCounter >= samplesThisStep)
            {
                sampleCounter = 0;
                triggerStep((currentStep + 1) % NUM_STEPS);
            }
            ++sampleCounter;
        }

        // Effect ramp t (shared across all FxParams)
        const float effectT = effectRampDuration > 0
            ? juce::jlimit(0.0f, 1.0f,
                  float(effectRampProgress) / float(effectRampDuration))
            : 1.0f;
        if (effectRampProgress < effectRampDuration) ++effectRampProgress;

        fxStretch   .update(effectT);
        fxFreqShift .update(effectT);
        fxFoldAmount.update(effectT);
        fxKsDecay   .update(effectT);
        fxKsTune    .update(effectT);
        fxReverbSize.update(effectT);
        fxReverbDamp.update(effectT);

        // Envelope gain
        const float envGain = envelopeGain();
        ++envSamplePos;

        // ---- Oscillator synthesis -------------------------------------------
        float sample = 0.0f;
        for (int i = 0; i < NUM_OSCILLATORS; ++i)
        {
            // Amplitude ramp (per-oscillator)
            const float ampT = oscs[i].rampDuration > 0
                ? juce::jlimit(0.0f, 1.0f,
                      float(oscs[i].rampProgress) / float(oscs[i].rampDuration))
                : 1.0f;
            if (oscs[i].rampProgress < oscs[i].rampDuration)
                ++oscs[i].rampProgress;

            oscs[i].currentAmp = oscs[i].rampStartAmp
                + (oscs[i].rampTargetAmp - oscs[i].rampStartAmp) * ampT;

            // Frequency: harmonic stretch via exp(log(i+1)*stretch) + shift
            const float stretchedHarm = std::exp(kLogHarm[i] * fxStretch.current);
            oscs[i].frequency = currentRootHz * stretchedHarm + fxFreqShift.current;

            sample += std::sin(oscs[i].phase) * oscs[i].currentAmp;

            oscs[i].phase += kTwoPi * oscs[i].frequency / sampleRate;
            if (oscs[i].phase >= kTwoPi) oscs[i].phase -= kTwoPi;
        }

        // ---- Sub-oscillator (rootHz × 0.5) ----------------------------------
        {
            const float ampT = subOsc.rampDuration > 0
                ? juce::jlimit(0.0f, 1.0f,
                      float(subOsc.rampProgress) / float(subOsc.rampDuration))
                : 1.0f;
            if (subOsc.rampProgress < subOsc.rampDuration)
                ++subOsc.rampProgress;

            subOsc.currentAmp = subOsc.rampStartAmp
                + (subOsc.rampTargetAmp - subOsc.rampStartAmp) * ampT;

            // Sub sits at half the root frequency; freq shift applies
            subOsc.frequency = currentRootHz * 0.5f + fxFreqShift.current;

            sample += std::sin(subOsc.phase) * subOsc.currentAmp;

            subOsc.phase += kTwoPi * subOsc.frequency / sampleRate;
            if (subOsc.phase >= kTwoPi) subOsc.phase -= kTwoPi;
        }

        sample *= envGain;

        // ---- Wavefolder -----------------------------------------------------
        // asin(sin(x * pi * (1 + fold*7))) / pi
        // Bypassed (and no tonal change) when foldAmount ~ 0
        if (fxFoldAmount.current > 0.001f)
        {
            const float foldGain = 1.0f + fxFoldAmount.current * 7.0f;
            sample = std::asin(std::sin(sample * kPi * foldGain)) / kPi;
        }

        // ---- Karplus-Strong resonator ---------------------------------------
        const bool ksActive = fxKsDecay.current > 0.001f;

        if (ksWasActive && !ksActive)
        {
            std::fill(ksBuffer, ksBuffer + kKsMaxDelay, 0.0f);
            ksPrevSample = 0.0f;
        }
        ksWasActive = ksActive;

        if (ksActive)
        {
            const int targetDelay = juce::jlimit(1, kKsMaxDelay - 1,
                int(sampleRate / juce::jmax(1.0f, fxKsTune.current)));

            if (targetDelay != ksNewDelaySamples)
            {
                ksNewDelaySamples = targetDelay;
                ksCrossfadeProg   = 0;
            }

            float alpha = 1.0f;
            if (ksCrossfadeProg < kKsCrossfadeLen)
            {
                alpha = float(ksCrossfadeProg) / float(kKsCrossfadeLen);
                ++ksCrossfadeProg;
                if (ksCrossfadeProg >= kKsCrossfadeLen)
                    ksDelaySamples = ksNewDelaySamples;
            }

            const int rOld = ((ksWritePos - ksDelaySamples)    + kKsMaxDelay * 2) % kKsMaxDelay;
            const int rNew = ((ksWritePos - ksNewDelaySamples)  + kKsMaxDelay * 2) % kKsMaxDelay;
            const float ksRead = ksBuffer[rOld] * (1.0f - alpha) + ksBuffer[rNew] * alpha;

            const float filtered = 0.5f * (ksRead + ksPrevSample);
            ksPrevSample = ksRead;

            ksBuffer[ksWritePos] = sample + filtered * fxKsDecay.current;
            ksWritePos = (ksWritePos + 1) % kKsMaxDelay;

            sample = sample + filtered * fxKsDecay.current;
        }

        // ---- FDN Reverb (4-channel Hadamard network) -----------------------
        const bool reverbActive = fxReverbSize.current > 0.001f;

        if (reverbWasActive && !reverbActive)
        {
            for (int c = 0; c < 4; ++c)
            {
                std::fill(fdnBuf[c], fdnBuf[c] + kFdnMaxDelay, 0.0f);
                fdnFiltSt[c] = 0.0f;
            }
        }
        reverbWasActive = reverbActive;

        float sampleL = sample;
        float sampleR = sample;

        if (reverbActive)
        {
            // Read delay lines
            float y[4];
            for (int c = 0; c < 4; ++c)
            {
                const int rp = (fdnWrite[c] - kFdnDelays[c] + kFdnMaxDelay * 2) % kFdnMaxDelay;
                y[c] = fdnBuf[c][rp];
            }

            // One-pole lowpass damping per channel: y_filt = (1-d)*y + d*prev
            const float damp = fxReverbDamp.current;
            float yf[4];
            for (int c = 0; c < 4; ++c)
            {
                yf[c]        = (1.0f - damp) * y[c] + damp * fdnFiltSt[c];
                fdnFiltSt[c] = yf[c];
            }

            // Hadamard H4 × 0.5 feedback mixing:
            //  fb[0] = 0.5*(+yf0 +yf1 +yf2 +yf3)
            //  fb[1] = 0.5*(+yf0 -yf1 +yf2 -yf3)
            //  fb[2] = 0.5*(+yf0 +yf1 -yf2 -yf3)
            //  fb[3] = 0.5*(+yf0 -yf1 -yf2 +yf3)
            const float gain = fxReverbSize.current;
            float fb[4];
            fb[0] = 0.5f * (+yf[0] + yf[1] + yf[2] + yf[3]);
            fb[1] = 0.5f * (+yf[0] - yf[1] + yf[2] - yf[3]);
            fb[2] = 0.5f * (+yf[0] + yf[1] - yf[2] - yf[3]);
            fb[3] = 0.5f * (+yf[0] - yf[1] - yf[2] + yf[3]);

            // Write: dry + feedback scaled by reverbSize
            for (int c = 0; c < 4; ++c)
            {
                fdnBuf[c][fdnWrite[c]] = sample + fb[c] * gain;
                fdnWrite[c] = (fdnWrite[c] + 1) % kFdnMaxDelay;
            }

            // Stereo wet output: L = ch0+ch2, R = ch1+ch3
            const float wetL = (yf[0] + yf[2]) * 0.5f;
            const float wetR = (yf[1] + yf[3]) * 0.5f;

            sampleL = sample * (1.0f - gain) + wetL * gain;
            sampleR = sample * (1.0f - gain) + wetR * gain;
        }

        // Master gain applied last
        outL[n] = sampleL * masterGain;
        if (outR) outR[n] = sampleR * masterGain;
    }

    // Publish live amplitudes for waveform display
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        state.liveAmplitudes[i].store(oscs[i].currentAmp, std::memory_order_relaxed);
}
