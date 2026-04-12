#pragma once
#include <JuceHeader.h>
#include "SynthState.h"

//==============================================================================
// AudioEngine
//
// Implements juce::AudioIODeviceCallback.  Runs entirely on the real-time
// audio thread; no allocations, no locks.
//
// Responsibilities:
//   • 4 sine-wave oscillators with continuous phase accumulators
//   • Sample-accurate BPM clock that advances the sequencer
//   • Linear-ramp morph between amplitude snapshots (linearRampToValueAtTime
//     semantics: reaches the target exactly at the next step boundary)
//   • Writes live amplitude values back to SynthSharedState for the UI
//==============================================================================
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(SynthSharedState& sharedState);
    ~AudioEngine() override;

    //==========================================================================
    // AudioIODeviceCallback
    //==========================================================================
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

    //==========================================================================
    // Audio-thread-private state  (no synchronisation needed)
    //==========================================================================
    float sampleRate = 44100.0f;

    struct OscPrivate
    {
        float phase           = 0.0f;   // radians
        float frequency       = 440.0f; // Hz
        float currentAmp      = 0.0f;   // the value we're actually rendering
        float rampStartAmp    = 0.0f;
        float rampTargetAmp   = 0.0f;
    };
    std::array<OscPrivate, NUM_OSCILLATORS> oscs;

    // Sequencer clock
    int   currentStep    = -1;
    int   sampleCounter  = 0;   // samples elapsed in current step
    int   samplesPerStep = 22050;

    // Morph ramp
    int   rampProgress   = 0;   // samples into current ramp
    int   rampDuration   = 22050;

    bool  wasPlaying     = false;

    //==========================================================================
    // Helpers  (called only from audio thread)
    //==========================================================================
    void  triggerStep(int step);
    int   calcSamplesPerStep(float bpm) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
