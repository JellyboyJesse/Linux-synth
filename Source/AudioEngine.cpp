#include "AudioEngine.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307179586f;

// log(i+1) for i = 0..3 — used for per-sample harmonic stretch calculation
static constexpr float kLogHarm[NUM_OSCILLATORS] = {
    0.0f,            // log(1)
    0.693147181f,    // log(2)
    1.098612289f,    // log(3)
    1.386294361f     // log(4)
};

// Max per-sample phase drift when phaseRand = 1.0 (approx 0.3 mrad → gentle shimmer)
static constexpr float kPhaseNoiseFactor = 0.0003f;

//==============================================================================
AudioEngine::AudioEngine(SynthSharedState& sharedState)
    : state(sharedState)
{
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        oscs[i].frequency    = currentRootHz * float(i + 1);
        oscs[i].currentAmp   = state.globalAmplitudes[i].load(std::memory_order_relaxed);
        oscs[i].rampStartAmp = oscs[i].currentAmp;
        oscs[i].rampTargetAmp= oscs[i].currentAmp;
    }
}

AudioEngine::~AudioEngine() = default;

//==============================================================================
void AudioEngine::triggerStep(int step)
{
    currentStep = step;
    state.currentPlayStep.store(step, std::memory_order_relaxed);

    // Step duration → sample count (reads BPM fresh so tempo changes take effect)
    const float bpm   = juce::jlimit(40.0f, 200.0f,
                            state.bpm.load(std::memory_order_relaxed));
    const float beats = juce::jlimit(0.1f, 8.0f,
                            state.stepDuration[step].load(std::memory_order_relaxed));
    samplesThisStep = juce::jmax(1, int(beats * 60.0f / bpm * sampleRate));

    // Root pitch (jumps immediately; frequency recomputed per-sample from morphed stretch)
    currentRootHz = semitoneToHz(state.stepPitch[step].load(std::memory_order_relaxed));

    // Envelope
    const float atkMs = state.stepAttack[step].load(std::memory_order_relaxed);
    const float decMs = state.stepDecay[step] .load(std::memory_order_relaxed);
    envAttackSamps = int(juce::jmax(0.0f, atkMs) / 1000.0f * sampleRate);
    envDecaySamps  = int(juce::jmax(0.0f, decMs) / 1000.0f * sampleRate);
    envSamplePos   = 0;

    // Oscillator amplitude ramps
    const bool isCustom = state.stepIsCustom[step].load(std::memory_order_relaxed);
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        oscs[i].rampStartAmp  = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = isCustom
            ? state.stepAmplitudes[step][i].load(std::memory_order_relaxed)
            : state.globalAmplitudes[i]    .load(std::memory_order_relaxed);
        oscs[i].rampDuration  = samplesThisStep;
        oscs[i].rampProgress  = 0;
        // oscs[i].frequency computed per-sample from effects morph — not set here
    }

    // Effect ramp (same duration as oscillator ramps)
    effectRampProgress = 0;
    effectRampDuration = samplesThisStep;

    fxStretch  .trigger(state.stretchRatio[step].load(std::memory_order_relaxed));
    fxFreqShift.trigger(state.freqShift[step]   .load(std::memory_order_relaxed));
    fxPhaseRand.trigger(state.phaseRand[step]   .load(std::memory_order_relaxed));
    fxKsDecay  .trigger(state.ksDecay[step]     .load(std::memory_order_relaxed));
    fxKsTune   .trigger(state.ksTune[step]      .load(std::memory_order_relaxed));

    // Instant phase randomisation at step boundary (deterministic LCG, seeded per step)
    const float prand = fxPhaseRand.rampTo;
    if (prand > 0.0f)
    {
        uint32_t seed = uint32_t(step) * 2654435761u;
        for (int i = 0; i < NUM_OSCILLATORS; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            const float r = float(seed >> 8) / float(1u << 24); // [0, 1)
            oscs[i].phase += prand * r * kTwoPi;
        }
    }
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate      = float(device->getCurrentSampleRate());
    samplesThisStep = 22050;
    sampleCounter   = 0;
    effectRampProgress = 0;
    effectRampDuration = 22050;
    envSamplePos    = 0;
    envAttackSamps  = 0;
    envDecaySamps   = 0;
    wasPlaying      = false;
    currentStep     = -1;
    ksWasActive     = false;
    ksCrossfadeProg = kKsCrossfadeLen;
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

    // Play / stop transitions
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

        fxStretch  .update(effectT);
        fxFreqShift.update(effectT);
        fxPhaseRand.update(effectT);
        fxKsDecay  .update(effectT);
        fxKsTune   .update(effectT);

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

            // Frequency: harmonic stretch (pow(i+1, stretch)) + shift
            // Use exp(log(i+1) * stretch) to avoid std::pow
            const float stretchedHarm = std::exp(kLogHarm[i] * fxStretch.current);
            oscs[i].frequency = currentRootHz * stretchedHarm + fxFreqShift.current;

            // Phase drift (continuous per-oscillator LCG noise scaled by phaseRand)
            phaseDriftLcg[i] = phaseDriftLcg[i] * 1664525u + 1013904223u;
            const float noise = float(int32_t(phaseDriftLcg[i])) * (1.0f / 2147483648.0f);
            oscs[i].phase += fxPhaseRand.current * noise * kPhaseNoiseFactor;

            sample += std::sin(oscs[i].phase) * oscs[i].currentAmp;

            oscs[i].phase += kTwoPi * oscs[i].frequency / sampleRate;
            if (oscs[i].phase >= kTwoPi) oscs[i].phase -= kTwoPi;
        }

        sample *= envGain;

        // ---- Karplus-Strong resonator ---------------------------------------
        const bool ksActive = fxKsDecay.current > 0.001f;

        if (ksWasActive && !ksActive)
        {
            // Transitioning to bypassed — clear buffer to avoid stale feedback
            std::fill(ksBuffer, ksBuffer + kKsMaxDelay, 0.0f);
            ksPrevSample = 0.0f;
        }
        ksWasActive = ksActive;

        if (ksActive)
        {
            // Target delay length from morphed tune
            const int targetDelay = juce::jlimit(1, kKsMaxDelay - 1,
                int(sampleRate / juce::jmax(1.0f, fxKsTune.current)));

            if (targetDelay != ksNewDelaySamples)
            {
                ksNewDelaySamples = targetDelay;
                ksCrossfadeProg   = 0;
            }

            // Advance crossfade (64-sample linear blend between old and new delay)
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

            // One-pole lowpass in feedback path
            const float filtered = 0.5f * (ksRead + ksPrevSample);
            ksPrevSample = ksRead;

            // Write: dry signal + filtered feedback
            ksBuffer[ksWritePos] = sample + filtered * fxKsDecay.current;
            ksWritePos = (ksWritePos + 1) % kKsMaxDelay;

            // Mix dry + resonator output
            sample = sample + filtered * fxKsDecay.current;
        }

        // Master gain applied last
        sample *= masterGain;

        outL[n] = sample;
        if (outR) outR[n] = sample;
    }

    // Publish live amplitudes for waveform display
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        state.liveAmplitudes[i].store(oscs[i].currentAmp, std::memory_order_relaxed);
}
