#pragma once
#include <JuceHeader.h>
#include "SynthState.h"

//==============================================================================
// AudioEngine
//
// Real-time audio callback — no allocations, no locks.
//
// Signal flow (per sample):
//   oscillator sum  →  envelope  →  harmonic stretch + freq shift
//   →  phase drift  →  Karplus-Strong resonator  →  master gain  →  output
//
// All effect parameters are morphed using the same linear ramp duration as
// the oscillator amplitude ramps (samplesThisStep from stepDuration + BPM).
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
    // Per-oscillator private state
    //==========================================================================
    struct OscPrivate
    {
        float phase         = 0.0f;
        float frequency     = 440.0f;
        float currentAmp    = 0.0f;
        float rampStartAmp  = 0.0f;
        float rampTargetAmp = 0.0f;
        int   rampProgress  = 0;
        int   rampDuration  = 22050;
    };
    std::array<OscPrivate, NUM_OSCILLATORS> oscs;

    float currentRootHz = 440.0f; // set at each step trigger; used for per-sample freq calc

    //==========================================================================
    // Sequencer clock
    //==========================================================================
    int  currentStep     = -1;
    int  sampleCounter   = 0;
    int  samplesThisStep = 22050;
    bool wasPlaying      = false;

    //==========================================================================
    // Amplitude envelope
    //==========================================================================
    int  envSamplePos   = 0;
    int  envAttackSamps = 0;
    int  envDecaySamps  = 0;

    float envelopeGain() const noexcept
    {
        if (envAttackSamps > 0 && envSamplePos < envAttackSamps)
            return float(envSamplePos) / float(envAttackSamps);
        const int dp = envSamplePos - envAttackSamps;
        if (envDecaySamps > 0 && dp < envDecaySamps)
            return 1.0f - float(dp) / float(envDecaySamps);
        return envDecaySamps > 0 ? 0.0f : 1.0f;
    }

    //==========================================================================
    // Effects chain — morph state (audio thread only)
    //==========================================================================
    struct FxParam
    {
        float current;
        float rampFrom;
        float rampTo;
        explicit FxParam(float def) noexcept : current(def), rampFrom(def), rampTo(def) {}
        void trigger(float target) noexcept { rampFrom = current; rampTo = target; }
        void update(float t) noexcept { current = rampFrom + (rampTo - rampFrom) * t; }
    };

    FxParam fxStretch  { 1.0f   }; // harmonic stretch ratio  0.5–2.0
    FxParam fxFreqShift{ 0.0f   }; // global freq shift Hz   -200–+200
    FxParam fxPhaseRand{ 0.0f   }; // phase randomisation     0–1
    FxParam fxKsDecay  { 0.0f   }; // Karplus-Strong feedback 0–1
    FxParam fxKsTune   { 220.0f }; // Karplus-Strong tune Hz  50–2000

    // Shared ramp counter for all effects (same timing as oscillator ramps)
    int effectRampProgress = 0;
    int effectRampDuration = 22050;

    //==========================================================================
    // Phase randomisation — continuous per-oscillator LCG drift
    //==========================================================================
    std::array<uint32_t, NUM_OSCILLATORS> phaseDriftLcg {
        { 0x12345678u, 0x9ABCDEF0u, 0x55AA55AAu, 0xFEDCBA98u }
    };

    //==========================================================================
    // Karplus-Strong resonator — pre-allocated, no heap
    //==========================================================================
    static constexpr int kKsMaxDelay     = 2048;
    static constexpr int kKsCrossfadeLen = 64;

    float ksBuffer[kKsMaxDelay] {};  // zero-initialised
    int   ksWritePos        = 0;
    float ksPrevSample      = 0.0f;
    int   ksDelaySamples    = 200;
    int   ksNewDelaySamples = 200;
    int   ksCrossfadeProg   = kKsCrossfadeLen; // start settled
    bool  ksWasActive       = false;

    //==========================================================================
    // Helper
    //==========================================================================
    void triggerStep(int step);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
