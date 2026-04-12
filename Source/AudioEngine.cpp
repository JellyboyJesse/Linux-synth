#include "AudioEngine.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307179586f;

AudioEngine::AudioEngine(SynthSharedState& sharedState)
    : state(sharedState)
{
    // Initialise oscillator frequencies to harmonics of C4
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        const float rootHz    = semitoneToHz(0); // C4
        oscs[i].frequency     = rootHz * float(i + 1); // harmonics 1×,2×,3×,4×
        oscs[i].currentAmp    = state.globalAmplitudes[i].load(std::memory_order_relaxed);
        oscs[i].rampStartAmp  = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = oscs[i].currentAmp;
    }
}

AudioEngine::~AudioEngine() = default;

//==============================================================================
int AudioEngine::calcSamplesPerStep(float bpm) const
{
    // 1 step = 1 quarter-note
    const float secondsPerBeat = 60.0f / juce::jlimit(40.0f, 200.0f, bpm);
    return juce::jmax(1, static_cast<int>(sampleRate * secondsPerBeat));
}

//==============================================================================
void AudioEngine::triggerStep(int step)
{
    currentStep = step;
    state.currentPlayStep.store(step, std::memory_order_relaxed);

    const bool   isCustom = state.stepIsCustom[step].load(std::memory_order_relaxed);
    const int    semitone = state.stepPitch[step]   .load(std::memory_order_relaxed);
    const float  rootHz   = semitoneToHz(semitone);

    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        // Amplitude: start ramp from wherever we currently are
        oscs[i].rampStartAmp = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = isCustom
            ? state.stepAmplitudes[step][i].load(std::memory_order_relaxed)
            : state.globalAmplitudes[i]   .load(std::memory_order_relaxed);

        // Frequency jumps immediately — step sequencer semantics
        oscs[i].frequency = rootHz * float(i + 1);
    }

    rampProgress = 0;
    rampDuration = samplesPerStep;
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate   = static_cast<float>(device->getCurrentSampleRate());
    samplesPerStep = calcSamplesPerStep(state.bpm.load());
    sampleCounter  = 0;
    rampProgress   = 0;
    rampDuration   = samplesPerStep;
    wasPlaying     = false;
    currentStep    = -1;
}

void AudioEngine::audioDeviceStopped()
{
    state.currentPlayStep.store(-1, std::memory_order_relaxed);
    currentStep  = -1;
    wasPlaying   = false;
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
    // Zero all output channels first
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch])
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    if (numOutputChannels == 0 || numSamples == 0)
        return;

    float* outL = outputChannelData[0];
    float* outR = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    // Read transport parameters once per callback
    const bool  playing    = state.isPlaying .load(std::memory_order_relaxed);
    const float bpm        = state.bpm       .load(std::memory_order_relaxed);
    const float masterGain = state.masterGain.load(std::memory_order_relaxed);

    samplesPerStep = calcSamplesPerStep(bpm);

    // Detect play start / stop transitions
    if (playing && !wasPlaying)
    {
        // Fresh start: jump to step 0 immediately
        sampleCounter = 0;
        triggerStep(0);
    }
    else if (!playing && wasPlaying)
    {
        state.currentPlayStep.store(-1, std::memory_order_relaxed);
        currentStep = -1;
    }
    wasPlaying = playing;

    // ---- Per-sample loop -------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        // Advance sequencer clock
        if (playing)
        {
            if (sampleCounter >= samplesPerStep)
            {
                sampleCounter = 0;
                triggerStep((currentStep + 1) % NUM_STEPS);
            }
            ++sampleCounter;
        }

        // Compute linear ramp interpolation factor t ∈ [0,1]
        // Reaches 1.0 exactly at the next step boundary.
        const float t = (rampDuration > 0)
            ? juce::jlimit(0.0f, 1.0f, float(rampProgress) / float(rampDuration))
            : 1.0f;
        if (rampProgress < rampDuration)
            ++rampProgress;

        // Synthesise — sum of 4 sine oscillators
        float sample = 0.0f;
        for (int i = 0; i < NUM_OSCILLATORS; ++i)
        {
            oscs[i].currentAmp = oscs[i].rampStartAmp
                + (oscs[i].rampTargetAmp - oscs[i].rampStartAmp) * t;

            sample += std::sin(oscs[i].phase) * oscs[i].currentAmp;

            oscs[i].phase += kTwoPi * oscs[i].frequency / sampleRate;
            if (oscs[i].phase >= kTwoPi)
                oscs[i].phase -= kTwoPi;
        }

        sample *= masterGain;

        outL[n] = sample;
        if (outR) outR[n] = sample;
    }

    // Write live amplitudes for the waveform display (once per callback)
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        state.liveAmplitudes[i].store(oscs[i].currentAmp, std::memory_order_relaxed);
}
