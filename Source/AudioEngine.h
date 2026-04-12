#pragma once
#include <JuceHeader.h>
#include "SynthState.h"

//==============================================================================
// AudioEngine
//
// Real-time audio callback — no allocations, no locks.
//
// Features:
//   • 4 sine oscillators with per-oscillator independent linear-ramp morph
//     (rampDuration[i] = samplesPerStep / morphSpeed[i][step])
//   • BPM-driven step sequencer clock (sample-accurate)
//   • Per-step linear attack / decay amplitude envelope
//==============================================================================
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(SynthSharedState& sharedState);
    ~AudioEngine() override;

    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int                 numInputChannels,
        float* const*       outputChannelData,
        int                 numOutputChannels,
        int                 numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    SynthSharedState& state;
    float sampleRate = 44100.0f;

    //==========================================================================
    // Per-oscillator private state (audio thread only)
    //==========================================================================
    struct OscPrivate
    {
        float phase          = 0.0f;
        float frequency      = 440.0f;
        float currentAmp     = 0.0f;
        float rampStartAmp   = 0.0f;
        float rampTargetAmp  = 0.0f;
        int   rampProgress   = 0;
        int   rampDuration   = 22050; // in samples; per-oscillator
    };
    std::array<OscPrivate, NUM_OSCILLATORS> oscs;

    //==========================================================================
    // Sequencer clock
    //==========================================================================
    int  currentStep    = -1;
    int  sampleCounter  = 0;
    int  samplesPerStep = 22050;
    bool wasPlaying     = false;

    //==========================================================================
    // Per-step amplitude envelope (linear attack → decay)
    //==========================================================================
    int  envSamplePos   = 0;
    int  envAttackSamps = 0;
    int  envDecaySamps  = 0;

    // Returns envelope gain [0,1] for the current sample position.
    // attack=0 → instant on; decay=0 → no decay (sustained).
    float envelopeGain() const noexcept
    {
        if (envAttackSamps > 0 && envSamplePos < envAttackSamps)
            return float(envSamplePos) / float(envAttackSamps);

        const int decayPos = envSamplePos - envAttackSamps;
        if (envDecaySamps > 0 && decayPos < envDecaySamps)
            return 1.0f - float(decayPos) / float(envDecaySamps);

        return envDecaySamps > 0 ? 0.0f : 1.0f; // silent after decay, or sustained
    }

    //==========================================================================
    // Helpers
    //==========================================================================
    void triggerStep(int step);
    int  calcSamplesPerStep(float bpm) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
