#pragma once
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// Compile-time constants
//==============================================================================
static constexpr int NUM_OSCILLATORS = 4;
static constexpr int NUM_STEPS       = 5;
static constexpr int NUM_SEMITONES   = 12;

// Note names for grid labels (row 0 = C4 = bottom, row 11 = B4 = top)
static const char* const NOTE_NAMES[NUM_SEMITONES] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/// Convert semitone index (0=C4 … 11=B4) to frequency in Hz.
inline float semitoneToHz(int semitone)
{
    // C4 = MIDI 60, A4 = MIDI 69 = 440 Hz
    const int midi = 60 + semitone;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

//==============================================================================
// SynthSharedState
//
// All mutable state shared between the UI thread and the real-time audio
// thread.  Every field is std::atomic so no locks are needed; individual
// fields are written by one thread and read by the other.
//
//   UI  → audio:  stepAmplitudes, stepPitch, stepIsCustom,
//                 globalAmplitudes, bpm, masterGain, isPlaying
//   audio → UI:   currentPlayStep, liveAmplitudes
//==============================================================================
struct SynthSharedState
{
    // Per-step amplitude targets:  [step 0..4][osc 0..3]
    std::array<std::array<std::atomic<float>, NUM_OSCILLATORS>, NUM_STEPS> stepAmplitudes;

    // Per-step root pitch:  semitone index 0..11
    std::array<std::atomic<int>,  NUM_STEPS> stepPitch;

    // Per-step custom flag: true → use stepAmplitudes, false → use globalAmplitudes
    std::array<std::atomic<bool>, NUM_STEPS> stepIsCustom;

    // Global (inherited) amplitude preset
    std::array<std::atomic<float>, NUM_OSCILLATORS> globalAmplitudes;

    // Transport
    std::atomic<float> bpm        { 120.0f };
    std::atomic<float> masterGain { 0.7f   };
    std::atomic<bool>  isPlaying  { false  };

    // Audio → UI feedback
    std::atomic<int>   currentPlayStep { -1 };          // -1 while stopped
    std::array<std::atomic<float>, NUM_OSCILLATORS> liveAmplitudes; // live morph values

    SynthSharedState()
    {
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            // Stagger default pitches across octave so each step sounds different
            stepPitch[s]   .store((s * 3) % NUM_SEMITONES, std::memory_order_relaxed);
            stepIsCustom[s].store(false,                   std::memory_order_relaxed);
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

    // Non-copyable (atomics cannot be copied)
    SynthSharedState(const SynthSharedState&)            = delete;
    SynthSharedState& operator=(const SynthSharedState&) = delete;
};
