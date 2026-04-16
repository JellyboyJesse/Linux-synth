#include "AudioEngine.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307179586f;
static constexpr float kPi    = 3.141592653589793f;

// Replace any non-finite value with 0 to prevent NaN/Inf propagation.
static inline float sanitise(float v) noexcept { return std::isfinite(v) ? v : 0.0f; }

static constexpr float kLogHarm[NUM_OSCILLATORS] = {
    0.0f, 0.693147181f, 1.098612289f, 1.386294361f
};

//==============================================================================
AudioEngine::AudioEngine(SynthSharedState& sharedState)
    : state(sharedState), gran(std::make_unique<GranularState>())
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

    const float bpm   = juce::jlimit(40.0f, 200.0f,
                            state.bpm.load(std::memory_order_relaxed));
    const float beats = juce::jlimit(0.1f, 8.0f,
                            state.stepDuration[step].load(std::memory_order_relaxed));
    samplesThisStep = juce::jmax(1, int(beats * 60.0f / bpm * sampleRate));

    const int rootNote = state.globalRootNote.load(std::memory_order_relaxed);
    const int pitch    = state.stepPitch[step].load(std::memory_order_relaxed);
    currentRootHz = semitoneToHz(pitch + rootNote);

    // Capture current gain before applying new step's envelope parameters,
    // so the next attack ramps from the live amplitude rather than from zero.
    envStartGain = envelopeGain();

    const float atkMs = state.stepAttack[step].load(std::memory_order_relaxed);
    const float decMs = state.stepDecay[step] .load(std::memory_order_relaxed);
    envAttackSamps = int(juce::jmax(0.0f, atkMs) / 1000.0f * sampleRate);
    envDecaySamps  = int(juce::jmax(0.0f, decMs) / 1000.0f * sampleRate);
    envSamplePos   = 0;

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

    subOsc.rampStartAmp  = subOsc.currentAmp;
    subOsc.rampTargetAmp = state.subAmp[step].load(std::memory_order_relaxed);
    subOsc.rampDuration  = samplesThisStep;
    subOsc.rampProgress  = 0;

    effectRampProgress = 0;
    effectRampDuration = samplesThisStep;

    fxStretch   .trigger(state.stretchRatio[step].load(std::memory_order_relaxed));
    fxFreqShift .trigger(state.freqShift[step]   .load(std::memory_order_relaxed));
    fxFoldAmount.trigger(state.foldAmount[step]  .load(std::memory_order_relaxed));

    fxGranMix     .trigger(state.granularMix[step]         .load(std::memory_order_relaxed));
    fxGranSize    .trigger(state.granularSize[step]         .load(std::memory_order_relaxed));
    fxGranDensity .trigger(state.granularDensity[step]      .load(std::memory_order_relaxed));
    fxGranScatter .trigger(state.granularPitchScatter[step] .load(std::memory_order_relaxed));
    fxGranFeedback.trigger(state.granularFeedback[step]     .load(std::memory_order_relaxed));

    fxReverbSize .trigger(state.reverbSize[step]   .load(std::memory_order_relaxed));
    fxReverbDamp .trigger(state.reverbDamp[step]   .load(std::memory_order_relaxed));
    fxShimmerAmt .trigger(state.shimmerAmount[step].load(std::memory_order_relaxed));
    fxShimmerTune.trigger(state.shimmerTune[step]  .load(std::memory_order_relaxed));

    // Child step setup
    if (state.childEnabled[step].load(std::memory_order_relaxed))
    {
        childMode           = true;
        currentParentStep   = step;
        currentChildStep    = 0;
        totalChildSteps     = juce::jlimit(1, NUM_STEPS,
                                  state.childStepCount[step].load(std::memory_order_relaxed));
        childSamplesPerStep = juce::jmax(1, samplesThisStep / totalChildSteps);
        childSampleCounter  = 0;
        // Override pitch with first child step pitch
        const int cPitch = state.childPitch[step][0].load(std::memory_order_relaxed);
        currentRootHz = semitoneToHz(cPitch + rootNote);
        DBG("[AudioEngine] child mode: step=" << step << " count=" << totalChildSteps
            << " spStep=" << childSamplesPerStep);
    }
    else
    {
        childMode = false;
    }
}

void AudioEngine::triggerChildPitch(int parentStep, int childIdx)
{
    envStartGain = envelopeGain(); // preserve continuity across child transitions
    const int rootNote = state.globalRootNote.load(std::memory_order_relaxed);
    const int pitch    = state.childPitch[parentStep][childIdx].load(std::memory_order_relaxed);
    currentRootHz  = semitoneToHz(pitch + rootNote);
    envSamplePos   = 0; // re-trigger envelope for each child step
    DBG("[AudioEngine] child pitch: parent=" << parentStep << " child=" << childIdx
        << " hz=" << currentRootHz);
}

//==============================================================================
void AudioEngine::resetFxState() noexcept
{
    // Clear FDN delay lines, filter states, and shimmer buffer
    for (int c = 0; c < 4; ++c)
    {
        std::fill(fdnBuf[c], fdnBuf[c] + kFdnMaxDelay, 0.0f);
        fdnFiltSt[c] = 0.0f;
    }
    std::fill(shimmerBuf, shimmerBuf + kShimmerBufSize, 0.0f);
    shimmerReadA    = 0.0f;
    shimmerReadB    = float(kShimmerBufSize / 2);
    shimmerWritePos = 0;
    reverbWasActive = false;

    // Clear granular engine
    for (auto& g : gran->grains) g.active = false;
    std::fill(gran->captureBuffer, gran->captureBuffer + kGranBufSize, 0.0f);
    gran->writeHead             = 0;
    gran->samplesSinceLastGrain = 0;
    gran->prevOutL              = 0.0f;
    gran->prevOutR              = 0.0f;
    granWasActive = false;

    // Reset oscillator phases so sin() gets clean arguments immediately
    for (auto& osc : oscs) osc.phase = 0.0f;
    subOsc.phase = 0.0f;

    DBG("[AudioEngine] NaN/Inf detected — fx state hard reset");
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate         = float(device->getCurrentSampleRate());
    samplesThisStep    = 22050;
    sampleCounter      = 0;
    effectRampProgress = 0;
    effectRampDuration = 22050;
    envSamplePos = envAttackSamps = envDecaySamps = 0;
    envStartGain = 1.0f;
    wasPlaying    = false;
    currentStep   = -1;
    childMode     = false;
    reverbWasActive = granWasActive = false;
    shimmerReadA  = 0.0f;
    shimmerReadB  = float(kShimmerBufSize / 2);
    shimmerWritePos = 0;
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
        childMode   = false;
    }
    wasPlaying = playing;

    // ---- Per-sample loop ---------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        // ---- Sequencer clock -----------------------------------------------
        if (playing)
        {
            // Main parent-step clock
            if (sampleCounter >= samplesThisStep)
            {
                sampleCounter = 0;
                childMode     = false;
                triggerStep((currentStep + 1) % NUM_STEPS);
            }
            ++sampleCounter;

            // Child-step sub-clock (runs in parallel with sampleCounter)
            if (childMode)
            {
                ++childSampleCounter;
                if (childSampleCounter >= childSamplesPerStep)
                {
                    childSampleCounter = 0;
                    ++currentChildStep;
                    if (currentChildStep < totalChildSteps)
                        triggerChildPitch(currentParentStep, currentChildStep);
                    // If we've exhausted child steps the parent clock will
                    // fire next, ending child mode via triggerStep.
                }
            }
        }

        // ---- Effect ramp ---------------------------------------------------
        const float effectT = effectRampDuration > 0
            ? juce::jlimit(0.0f, 1.0f,
                  float(effectRampProgress) / float(effectRampDuration))
            : 1.0f;
        if (effectRampProgress < effectRampDuration) ++effectRampProgress;

        fxStretch   .update(effectT);
        fxFreqShift .update(effectT);
        fxFoldAmount.update(effectT);
        fxGranMix     .update(effectT);
        fxGranSize    .update(effectT);
        fxGranDensity .update(effectT);
        fxGranScatter .update(effectT);
        fxGranFeedback.update(effectT);
        fxReverbSize .update(effectT);
        fxReverbDamp .update(effectT);
        fxShimmerAmt .update(effectT);
        fxShimmerTune.update(effectT);

        // ---- Envelope ------------------------------------------------------
        const float envGain = envelopeGain();
        ++envSamplePos;

        // ---- Oscillator synthesis ------------------------------------------
        float sample = 0.0f;
        for (int i = 0; i < NUM_OSCILLATORS; ++i)
        {
            const float ampT = oscs[i].rampDuration > 0
                ? juce::jlimit(0.0f, 1.0f,
                      float(oscs[i].rampProgress) / float(oscs[i].rampDuration))
                : 1.0f;
            if (oscs[i].rampProgress < oscs[i].rampDuration) ++oscs[i].rampProgress;

            oscs[i].currentAmp = oscs[i].rampStartAmp
                + (oscs[i].rampTargetAmp - oscs[i].rampStartAmp) * ampT;

            const float stretchedHarm = std::exp(kLogHarm[i] * fxStretch.current);
            oscs[i].frequency = currentRootHz * stretchedHarm + fxFreqShift.current;

            sample += std::sin(oscs[i].phase) * oscs[i].currentAmp;

            oscs[i].phase += kTwoPi * oscs[i].frequency / sampleRate;
            // Wrap bidirectionally: negative phase occurs when freqShift pulls
            // frequency below zero, causing unbounded negative drift → NaN.
            if      (oscs[i].phase >= kTwoPi) oscs[i].phase -= kTwoPi;
            else if (oscs[i].phase <  0.0f)   oscs[i].phase += kTwoPi;
        }

        // Sub-oscillator (rootHz × 0.5)
        {
            const float ampT = subOsc.rampDuration > 0
                ? juce::jlimit(0.0f, 1.0f,
                      float(subOsc.rampProgress) / float(subOsc.rampDuration))
                : 1.0f;
            if (subOsc.rampProgress < subOsc.rampDuration) ++subOsc.rampProgress;
            subOsc.currentAmp = subOsc.rampStartAmp
                + (subOsc.rampTargetAmp - subOsc.rampStartAmp) * ampT;
            subOsc.frequency = currentRootHz * 0.5f + fxFreqShift.current;
            sample += std::sin(subOsc.phase) * subOsc.currentAmp;
            subOsc.phase += kTwoPi * subOsc.frequency / sampleRate;
            if      (subOsc.phase >= kTwoPi) subOsc.phase -= kTwoPi;
            else if (subOsc.phase <  0.0f)   subOsc.phase += kTwoPi;
        }

        sample *= envGain;
        sample = sanitise(sample); // stop NaN entering granular/FDN

        // ---- Wavefolder ----------------------------------------------------
        if (fxFoldAmount.current > 0.001f)
        {
            const float foldGain = 1.0f + fxFoldAmount.current * 7.0f;
            sample = sanitise(std::asin(std::sin(sample * kPi * foldGain)) / kPi);
        }

        // ---- Granular engine -----------------------------------------------
        const bool granActive = fxGranMix.current > 0.001f;

        if (granWasActive && !granActive)
        {
            for (auto& g : gran->grains) g.active = false;
            gran->prevOutL = gran->prevOutR = 0.0f;
        }
        granWasActive = granActive;

        float granL = sample, granR = sample;

        if (granActive)
        {
            // Write into capture buffer (with feedback)
            const float prevMono = (gran->prevOutL + gran->prevOutR) * 0.5f;
            gran->captureBuffer[gran->writeHead] =
                sanitise(sample + fxGranFeedback.current * prevMono);
            gran->writeHead = (gran->writeHead + 1) % kGranBufSize;

            // Grain spawning — skip if too many grains active (CPU guard)
            ++gran->samplesSinceLastGrain;
            const int grainInterval = juce::jmax(1,
                int(sampleRate / juce::jmax(0.001f, fxGranDensity.current)));

            if (gran->samplesSinceLastGrain >= grainInterval)
            {
                gran->samplesSinceLastGrain = 0;

                // Count active grains before spawning
                int activeCount = 0;
                for (const auto& g : gran->grains)
                    if (g.active) ++activeCount;

                if (activeCount < kMaxActiveGrains)
                {
                    for (auto& g : gran->grains)
                    {
                        if (g.active) continue;
                        const int sizeSamps = juce::jlimit(
                            int(0.01f  * sampleRate),
                            int(0.5f   * sampleRate),
                            int(fxGranSize.current * 0.001f * sampleRate));
                        g.size = sizeSamps;
                        g.age  = 0;
                        int rh = gran->writeHead - sizeSamps;
                        if (rh < 0) rh += kGranBufSize;
                        g.readHead = float(rh);

                        // Random pitch scatter
                        granLcg = granLcg * 1664525u + 1013904223u;
                        const float r = float(granLcg >> 8) / float(1u << 24);
                        g.pitchRatio = std::pow(2.0f,
                            fxGranScatter.current * (r - 0.5f));

                        // Random pan
                        granLcg = granLcg * 1664525u + 1013904223u;
                        const float pan = float(granLcg >> 8) / float(1u << 24);
                        g.panL = std::cos(pan * kPi * 0.5f);
                        g.panR = std::sin(pan * kPi * 0.5f);

                        g.active = true;
                        break;
                    }
                }
            }

            // Process active grains
            float outL = 0.0f, outR = 0.0f;
            for (auto& g : gran->grains)
            {
                if (!g.active) continue;
                const float phase = float(g.age) / float(g.size);
                const float env   = 0.5f * (1.0f - std::cos(kTwoPi * phase));

                const int   pos0  = int(g.readHead) % kGranBufSize;
                const int   pos1  = (pos0 + 1) % kGranBufSize;
                const float frac  = g.readHead - float(int(g.readHead));
                const float s = gran->captureBuffer[pos0] * (1.0f - frac)
                              + gran->captureBuffer[pos1] * frac;

                outL += s * env * g.panL;
                outR += s * env * g.panR;

                g.readHead += g.pitchRatio;
                if (g.readHead >= float(kGranBufSize))
                    g.readHead -= float(kGranBufSize);

                if (++g.age >= g.size) g.active = false;
            }

            gran->prevOutL = outL;
            gran->prevOutR = outR;
            const float mix = fxGranMix.current;
            granL = sample * (1.0f - mix) + outL * mix;
            granR = sample * (1.0f - mix) + outR * mix;
        }

        // ---- Shimmer reverb ------------------------------------------------
        const bool reverbActive = fxReverbSize.current > 0.001f;

        if (reverbWasActive && !reverbActive)
        {
            for (int c = 0; c < 4; ++c)
            {
                std::fill(fdnBuf[c], fdnBuf[c] + kFdnMaxDelay, 0.0f);
                fdnFiltSt[c] = 0.0f;
            }
            std::fill(shimmerBuf, shimmerBuf + kShimmerBufSize, 0.0f);
            shimmerReadA = 0.0f;
            shimmerReadB = float(kShimmerBufSize / 2);
            shimmerWritePos = 0;
        }
        reverbWasActive = reverbActive;

        float sampleL = granL, sampleR = granR;

        if (reverbActive)
        {
            // Mono input to FDN (average of granular stereo)
            const float fdnIn = (granL + granR) * 0.5f;

            // Read FDN delay lines
            float y[4];
            for (int c = 0; c < 4; ++c)
            {
                const int rp = (fdnWrite[c] - kFdnDelays[c]
                                + kFdnMaxDelay * 2) % kFdnMaxDelay;
                y[c] = fdnBuf[c][rp];
            }

            // One-pole lowpass damping
            const float damp = fxReverbDamp.current;
            float yf[4];
            for (int c = 0; c < 4; ++c)
            {
                yf[c]        = (1.0f - damp) * y[c] + damp * fdnFiltSt[c];
                fdnFiltSt[c] = sanitise(yf[c]); // IIR — one NaN circulates forever
                yf[c]        = fdnFiltSt[c];
            }

            // Hadamard H4 × 0.5 feedback mixing
            const float gain = fxReverbSize.current;
            float fb[4];
            fb[0] = 0.5f * (+yf[0] + yf[1] + yf[2] + yf[3]);
            fb[1] = 0.5f * (+yf[0] - yf[1] + yf[2] - yf[3]);
            fb[2] = 0.5f * (+yf[0] + yf[1] - yf[2] - yf[3]);
            fb[3] = 0.5f * (+yf[0] - yf[1] - yf[2] + yf[3]);

            // Shimmer pitch shifter in feedback path
            float pitchShifted = 0.0f;
            const float shimAmt = fxShimmerAmt.current;
            if (shimAmt > 0.001f)
            {
                // Write mono FDN output to shimmer buffer
                const float fdnMono = (yf[0] + yf[1] + yf[2] + yf[3]) * 0.25f;
                shimmerBuf[shimmerWritePos] = fdnMono;
                shimmerWritePos = (shimmerWritePos + 1) & (kShimmerBufSize - 1);

                // Hann-windowed read from two pointers
                auto readLerp = [&](float rp) -> float
                {
                    const int   ip   = int(rp) & (kShimmerBufSize - 1);
                    const int   ip1  = (ip + 1) & (kShimmerBufSize - 1);
                    const float frac = rp - float(int(rp));
                    return shimmerBuf[ip] * (1.0f - frac) + shimmerBuf[ip1] * frac;
                };

                auto shimPhase = [&](float rp) -> float
                {
                    const float dist = float(shimmerWritePos) - rp;
                    const float wrapped = dist < 0.0f
                        ? dist + float(kShimmerBufSize)
                        : dist;
                    return wrapped / float(kShimmerBufSize);
                };

                const float phA = shimPhase(shimmerReadA);
                const float phB = shimPhase(shimmerReadB);
                const float wA  = 0.5f * (1.0f - std::cos(kTwoPi * phA));
                const float wB  = 0.5f * (1.0f - std::cos(kTwoPi * phB));
                pitchShifted = readLerp(shimmerReadA) * wA
                             + readLerp(shimmerReadB) * wB;

                // Advance read pointers at pitch-shifted rate
                const float pitchRate = std::pow(2.0f, fxShimmerTune.current);
                shimmerReadA = std::fmod(shimmerReadA + pitchRate,
                                         float(kShimmerBufSize));
                shimmerReadB = std::fmod(shimmerReadA + float(kShimmerBufSize / 2),
                                         float(kShimmerBufSize));
            }

            // Write to FDN delay lines (dry + Hadamard feedback + shimmer)
            for (int c = 0; c < 4; ++c)
            {
                fdnBuf[c][fdnWrite[c]] =
                    sanitise(fdnIn + fb[c] * gain + pitchShifted * shimAmt);
                fdnWrite[c] = (fdnWrite[c] + 1) % kFdnMaxDelay;
            }

            // Stereo wet: ch0+ch2 = L, ch1+ch3 = R
            const float wetL = (yf[0] + yf[2]) * 0.5f;
            const float wetR = (yf[1] + yf[3]) * 0.5f;
            sampleL = granL * (1.0f - gain) + wetL * gain;
            sampleR = granR * (1.0f - gain) + wetR * gain;
        }

        // ---- Master gain + sanity shield -----------------------------------
        // If any upstream stage produced a non-finite value, hard-clear all
        // delay-based state so the corruption doesn't carry to future steps.
        if (!std::isfinite(sampleL) || !std::isfinite(sampleR))
        {
            resetFxState();
            sampleL = sampleR = 0.0f;
        }
        outL[n] = sampleL * masterGain;
        if (outR) outR[n] = sampleR * masterGain;
    }

    // Publish live amplitudes for waveform display
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        state.liveAmplitudes[i].store(oscs[i].currentAmp, std::memory_order_relaxed);
}
