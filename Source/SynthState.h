#pragma once
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
static constexpr int NUM_OSCILLATORS = 4;
static constexpr int NUM_STEPS       = 5;
static constexpr int NUM_SEMITONES   = 12;
static constexpr int PITCH_GRID_ROWS = 8;

static const char* const NOTE_NAMES[NUM_SEMITONES] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

inline float semitoneToHz(int semitone)
{
    const int midi = 60 + semitone;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

//==============================================================================
// SynthSharedState — all lock-free shared state (UI ↔ audio thread)
//
//  UI → audio: amplitudes, pitch, sub, duration, effects, granular, reverb,
//              shimmer, child step config, transport
//  audio → UI: currentPlayStep, liveAmplitudes
//==============================================================================
struct SynthSharedState
{
    // ---- Notes ---------------------------------------------------------------
    std::array<std::array<std::atomic<float>, NUM_OSCILLATORS>, NUM_STEPS> stepAmplitudes;
    std::array<std::atomic<int>,   NUM_STEPS> stepPitch;
    std::array<std::atomic<bool>,  NUM_STEPS> stepIsCustom;
    std::array<std::atomic<float>, NUM_OSCILLATORS> globalAmplitudes;
    std::array<std::atomic<float>, NUM_STEPS> subAmp;        // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> stepDuration;  // 0.1–8.0 beats
    std::array<std::atomic<float>, NUM_STEPS> stepAttack;    // 0–1000 ms
    std::array<std::atomic<float>, NUM_STEPS> stepDecay;     // 0–2000 ms

    // ---- Harmonic effects (per step, morphed) --------------------------------
    std::array<std::atomic<float>, NUM_STEPS> stretchRatio;  // 0.5–2.0
    std::array<std::atomic<float>, NUM_STEPS> freqShift;     // -200–+200 Hz
    std::array<std::atomic<float>, NUM_STEPS> foldAmount;    // 0.0–1.0

    // ---- Granular engine (per step, morphed) ---------------------------------
    std::array<std::atomic<float>, NUM_STEPS> granularMix;          // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> granularSize;         // 10–500 ms
    std::array<std::atomic<float>, NUM_STEPS> granularDensity;      // 1–40 grains/s
    std::array<std::atomic<float>, NUM_STEPS> granularPitchScatter; // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> granularFeedback;     // 0.0–0.95

    // ---- Shimmer reverb (per step, morphed) ----------------------------------
    std::array<std::atomic<float>, NUM_STEPS> reverbSize;    // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> reverbDamp;    // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> shimmerAmount; // 0.0–1.0
    std::array<std::atomic<float>, NUM_STEPS> shimmerTune;   // -1.0–1.0 (1.0 = oct up)

    // ---- Nested child steps (per step) --------------------------------------
    std::array<std::atomic<bool>, NUM_STEPS> childEnabled;   // false = off
    std::array<std::atomic<int>,  NUM_STEPS> childStepCount; // 1–5
    std::array<std::array<std::atomic<int>, NUM_STEPS>, NUM_STEPS> childPitch; // semitone 0–7

    // ---- Global tuning -------------------------------------------------------
    std::atomic<int> globalRootNote { 0 };  // 0=C … 11=B
    std::atomic<int> globalScale    { 0 };  // index into ScaleTable::kScales

    // ---- Transport -----------------------------------------------------------
    std::atomic<float> bpm        { 120.0f };
    std::atomic<float> masterGain { 0.7f   };
    std::atomic<bool>  isPlaying  { false  };

    // ---- Audio → UI ----------------------------------------------------------
    std::atomic<int> currentPlayStep { -1 };
    std::array<std::atomic<float>, NUM_OSCILLATORS> liveAmplitudes;

    SynthSharedState()
    {
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            stepPitch[s]   .store((s * 2) % PITCH_GRID_ROWS, std::memory_order_relaxed);
            stepIsCustom[s].store(true,   std::memory_order_relaxed);
            subAmp[s]      .store(0.0f,   std::memory_order_relaxed);
            stepDuration[s].store(1.0f,   std::memory_order_relaxed);
            stepAttack[s]  .store(10.0f,  std::memory_order_relaxed);  // 10 ms
            stepDecay[s]   .store(500.0f, std::memory_order_relaxed);  // 500 ms

            stretchRatio[s].store(1.0f,   std::memory_order_relaxed);
            freqShift[s]   .store(0.0f,   std::memory_order_relaxed);
            foldAmount[s]  .store(0.0f,   std::memory_order_relaxed);

            granularMix[s]         .store(0.0f,  std::memory_order_relaxed);
            granularSize[s]        .store(80.0f, std::memory_order_relaxed);
            granularDensity[s]     .store(8.0f,  std::memory_order_relaxed);
            granularPitchScatter[s].store(0.0f,  std::memory_order_relaxed);
            granularFeedback[s]    .store(0.0f,  std::memory_order_relaxed);

            reverbSize[s]   .store(0.3f,  std::memory_order_relaxed);
            reverbDamp[s]   .store(0.5f,  std::memory_order_relaxed);
            shimmerAmount[s].store(0.15f, std::memory_order_relaxed);
            shimmerTune[s]  .store(1.0f,  std::memory_order_relaxed);

            childEnabled[s]  .store(false, std::memory_order_relaxed);
            childStepCount[s].store(3,     std::memory_order_relaxed);
            for (int c = 0; c < NUM_STEPS; ++c)
                childPitch[s][c].store(0, std::memory_order_relaxed);

            for (int o = 0; o < NUM_OSCILLATORS; ++o)
                stepAmplitudes[s][o].store(o == 0 ? 0.8f : 0.0f, std::memory_order_relaxed);
        }
        for (int o = 0; o < NUM_OSCILLATORS; ++o)
        {
            const float def = o == 0 ? 0.8f : 0.0f;
            globalAmplitudes[o].store(def, std::memory_order_relaxed);
            liveAmplitudes  [o].store(def, std::memory_order_relaxed);
        }
    }

    SynthSharedState(const SynthSharedState&)            = delete;
    SynthSharedState& operator=(const SynthSharedState&) = delete;
};
