#pragma once
#include <JuceHeader.h>
#include <memory>
#include "SynthState.h"

//==============================================================================
// AudioEngine — real-time audio callback, no allocations, no locks.
//
// Signal flow (per sample):
//   Oscillator sum (H1–H4 + sub)
//   → envelope
//   → harmonic stretch + freq shift  (per-oscillator, during synthesis)
//   → wavefolder
//   → granular engine
//   → shimmer reverb (FDN + pitch-shifted feedback)
//   → master gain → stereo output
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
    // Per-oscillator state
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
    OscPrivate subOsc;
    float currentRootHz = 440.0f;

    //==========================================================================
    // Sequencer clock
    //==========================================================================
    int  currentStep     = -1;
    int  sampleCounter   = 0;
    int  samplesThisStep = 22050;
    bool wasPlaying      = false;

    // Child step sub-clock
    bool childMode           = false;
    int  currentParentStep   = 0;
    int  currentChildStep    = 0;
    int  totalChildSteps     = 1;
    int  childSamplesPerStep = 22050;
    int  childSampleCounter  = 0;

    //==========================================================================
    // Amplitude envelope
    //==========================================================================
    static constexpr float kEnvMin = 0.1f;  // floor: never absolute silence

    int   envSamplePos   = 0;
    int   envAttackSamps = 0;
    int   envDecaySamps  = 0;
    float envStartGain   = 1.0f; // gain at moment of step/child trigger

    float envelopeGain() const noexcept
    {
        // No shaping: hold at 1.0 permanently
        if (envAttackSamps == 0 && envDecaySamps == 0)
            return 1.0f;

        // Attack: ramp from envStartGain → 1.0 over attackSamps
        if (envAttackSamps > 0 && envSamplePos < envAttackSamps)
        {
            const float t = float(envSamplePos) / float(envAttackSamps);
            return envStartGain + (1.0f - envStartGain) * t;
        }

        // Decay: ramp from 1.0 → kEnvMin over decaySamps
        const int dp = envSamplePos - envAttackSamps;
        if (envDecaySamps > 0 && dp < envDecaySamps)
        {
            const float t = float(dp) / float(envDecaySamps);
            return 1.0f - (1.0f - kEnvMin) * t;
        }

        // After decay: hold at minimum (never absolute zero)
        return kEnvMin;
    }

    //==========================================================================
    // Effects morphing — FxParam (audio thread only)
    //==========================================================================
    struct FxParam
    {
        float current, rampFrom, rampTo;
        explicit FxParam(float def) noexcept : current(def), rampFrom(def), rampTo(def) {}
        void trigger(float t) noexcept { rampFrom = current; rampTo = t; }
        void update(float t)  noexcept { current = rampFrom + (rampTo - rampFrom) * t; }
    };

    FxParam fxStretch    { 1.0f   };
    FxParam fxFreqShift  { 0.0f   };
    FxParam fxFoldAmount { 0.0f   };

    FxParam fxGranMix     { 0.0f  };
    FxParam fxGranSize    { 80.0f };  // ms
    FxParam fxGranDensity { 8.0f  };  // grains/s
    FxParam fxGranScatter { 0.0f  };  // 0–1
    FxParam fxGranFeedback{ 0.0f  };  // 0–0.95

    FxParam fxReverbSize  { 0.3f  };
    FxParam fxReverbDamp  { 0.5f  };
    FxParam fxShimmerAmt  { 0.15f };
    FxParam fxShimmerTune { 1.0f  };  // -1–1; 1 = oct up

    int effectRampProgress = 0;
    int effectRampDuration = 22050;

    //==========================================================================
    // Granular engine — heap-allocated to avoid stack overflow
    //==========================================================================
    static constexpr int kGranBufSize   = 44100; // 1 s at 44100
    static constexpr int kMaxGrains     = 16;
    static constexpr int kMaxActiveGrains = 12;  // CPU guard: skip spawn above this

    struct GrainState
    {
        bool  active    = false;
        float readHead  = 0.0f;
        float pitchRatio= 1.0f;
        int   size      = 4410;
        int   age       = 0;
        float panL      = 1.0f;
        float panR      = 0.0f;
    };

    struct GranularState
    {
        float      captureBuffer[kGranBufSize] {};
        int        writeHead              = 0;
        GrainState grains[kMaxGrains]     {};
        int        samplesSinceLastGrain  = 0;
        float      prevOutL               = 0.0f;
        float      prevOutR               = 0.0f;
    };

    std::unique_ptr<GranularState> gran;

    uint32_t granLcg     = 0xDEADBEEFu;
    bool     granWasActive = false;

    //==========================================================================
    // FDN shimmer reverb — pre-allocated, no heap
    //
    // 4-channel FDN: prime delays 1471/1699/1877/2053
    // Hadamard H4 × 0.5 feedback matrix, one-pole lowpass per channel
    // Pitch shifter in feedback path: 512-sample circular buffer,
    //   two Hann-windowed read pointers at rate pow(2, shimmerTune)
    // Stereo out: ch0+ch2 → L, ch1+ch3 → R
    //==========================================================================
    static constexpr int kFdnDelays[4]   = { 1471, 1699, 1877, 2053 };
    static constexpr int kFdnMaxDelay    = 2054;
    static constexpr int kShimmerBufSize = 512;

    float fdnBuf[4][kFdnMaxDelay]   {};
    int   fdnWrite[4]               = { 0, 0, 0, 0 };
    float fdnFiltSt[4]              = { 0.0f, 0.0f, 0.0f, 0.0f };

    float shimmerBuf[kShimmerBufSize] {};
    int   shimmerWritePos             = 0;
    float shimmerReadA                = 0.0f;
    float shimmerReadB                = float(kShimmerBufSize / 2);

    bool  reverbWasActive = false;

    //==========================================================================
    // Helpers
    //==========================================================================
    void triggerStep(int step);
    void triggerChildPitch(int parentStep, int childIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
