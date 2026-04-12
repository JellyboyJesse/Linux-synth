#include "AudioEngine.h"
#include <cmath>

static constexpr float kTwoPi = 6.283185307179586f;

AudioEngine::AudioEngine(SynthSharedState& sharedState)
    : state(sharedState)
{
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        const float rootHz    = semitoneToHz(0);
        oscs[i].frequency     = rootHz * float(i + 1);
        oscs[i].currentAmp    = state.globalAmplitudes[i].load(std::memory_order_relaxed);
        oscs[i].rampStartAmp  = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = oscs[i].currentAmp;
    }
}

AudioEngine::~AudioEngine() = default;

//==============================================================================
void AudioEngine::triggerStep(int step)
{
    currentStep = step;
    state.currentPlayStep.store(step, std::memory_order_relaxed);

    // Compute this step's sample length from its beat duration and current BPM.
    // Read BPM fresh so tempo changes take effect at the next step boundary.
    const float bpm      = juce::jlimit(40.0f, 200.0f, state.bpm.load(std::memory_order_relaxed));
    const float beats    = juce::jlimit(0.1f,  8.0f,
                               state.stepDuration[step].load(std::memory_order_relaxed));
    samplesThisStep = juce::jmax(1, int(beats * 60.0f / bpm * sampleRate));

    const bool  isCustom = state.stepIsCustom[step].load(std::memory_order_relaxed);
    const int   semitone = state.stepPitch[step]   .load(std::memory_order_relaxed);
    const float rootHz   = semitoneToHz(semitone);

    // Attack / decay envelope (ms → samples)
    const float attackMs = state.stepAttack[step].load(std::memory_order_relaxed);
    const float decayMs  = state.stepDecay[step] .load(std::memory_order_relaxed);
    envAttackSamps = int(juce::jmax(0.0f, attackMs) / 1000.0f * sampleRate);
    envDecaySamps  = int(juce::jmax(0.0f, decayMs)  / 1000.0f * sampleRate);
    envSamplePos   = 0;

    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        oscs[i].rampStartAmp = oscs[i].currentAmp;
        oscs[i].rampTargetAmp = isCustom
            ? state.stepAmplitudes[step][i].load(std::memory_order_relaxed)
            : state.globalAmplitudes[i]    .load(std::memory_order_relaxed);

        // All oscillators ramp over the full step duration
        oscs[i].rampDuration = samplesThisStep;
        oscs[i].rampProgress = 0;

        oscs[i].frequency = rootHz * float(i + 1);
    }
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate      = float(device->getCurrentSampleRate());
    samplesThisStep = 22050; // will be recomputed on first triggerStep()
    sampleCounter   = 0;
    envSamplePos   = 0;
    envAttackSamps = 0;
    envDecaySamps  = 0;
    wasPlaying     = false;
    currentStep    = -1;
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

    if (numOutputChannels == 0 || numSamples == 0)
        return;

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

        // Compute envelope gain for this sample
        const float envGain = envelopeGain();
        ++envSamplePos;

        // Sum oscillators — each has its own independent ramp
        float sample = 0.0f;
        for (int i = 0; i < NUM_OSCILLATORS; ++i)
        {
            // Per-oscillator linear ramp (independent rampProgress/rampDuration)
            const float t = oscs[i].rampDuration > 0
                ? juce::jlimit(0.0f, 1.0f,
                      float(oscs[i].rampProgress) / float(oscs[i].rampDuration))
                : 1.0f;

            if (oscs[i].rampProgress < oscs[i].rampDuration)
                ++oscs[i].rampProgress;

            oscs[i].currentAmp = oscs[i].rampStartAmp
                + (oscs[i].rampTargetAmp - oscs[i].rampStartAmp) * t;

            sample += std::sin(oscs[i].phase) * oscs[i].currentAmp;

            oscs[i].phase += kTwoPi * oscs[i].frequency / sampleRate;
            if (oscs[i].phase >= kTwoPi)
                oscs[i].phase -= kTwoPi;
        }

        sample *= envGain * masterGain;

        outL[n] = sample;
        if (outR) outR[n] = sample;
    }

    // Publish live amplitudes for the waveform display (once per callback)
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        state.liveAmplitudes[i].store(oscs[i].currentAmp, std::memory_order_relaxed);
}
