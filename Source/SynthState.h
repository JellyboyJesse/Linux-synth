#pragma once
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
static constexpr int NUM_OSCILLATORS = 4;
static constexpr int NUM_STEPS       = 5;
static constexpr int NUM_SEMITONES   = 12;

// 8-note chromatic pitch grid (C4 … G4)
static constexpr int PITCH_GRID_ROWS = 8;

// Note names (semitone 0=C … 11=B)
static const char* const NOTE_NAMES[NUM_SEMITONES] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/// Convert semitone index (0=C4 … 11=B4) to frequency in Hz.
inline float semitoneToHz(int semitone)
{
    const int midi = 60 + semitone;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

//==============================================================================
// SynthSharedState
//
// All mutable state shared between the UI thread and the real-time audio
// thread.  Every field is std::atomic — no locks required.
//
//  UI → audio : stepAmplitudes, stepPitch, stepIsCustom, globalAmplitudes,
//               stepDuration, stepAttack, stepDecay,
//               stretchRatio, freqShift, phaseRand, ksDecay, ksTune,
//               bpm, masterGain, isPlaying
//  audio → UI : currentPlayStep, liveAmplitudes
//==============================================================================
struct SynthSharedState
{
    // Per-step amplitude targets:  [step][osc]
    std::array<std::array<std::atomic<float>, NUM_OSCILLATORS>, NUM_STEPS> stepAmplitudes;

    // Per-step root pitch: semitone index 0..11
    std::array<std::atomic<int>,  NUM_STEPS> stepPitch;

    // Per-step custom flag (true → use stepAmplitudes, false → use globalAmplitudes)
    std::array<std::atomic<bool>, NUM_STEPS> stepIsCustom;

    // Global (inherited) amplitude preset
    std::array<std::atomic<float>, NUM_OSCILLATORS> globalAmplitudes;

    // Per-step beat duration in beats (free float, 0.1–8.0).
    // Governs both the step clock length and the amplitude ramp duration.
    // e.g. 1.0 = one quarter-note, 0.5 = eighth-note, 2.0 = half-note.
    std::array<std::atomic<float>, NUM_STEPS> stepDuration;

    // Effects chain — per step (morphed on audio thread like amplitudes)
    std::array<std::atomic<float>, NUM_STEPS> stretchRatio; // 0.5–2.0, default 1.0
    std::array<std::atomic<float>, NUM_STEPS> freqShift;    // -200–+200 Hz, default 0.0
    std::array<std::atomic<float>, NUM_STEPS> phaseRand;    // 0.0–1.0, default 0.0
    std::array<std::atomic<float>, NUM_STEPS> ksDecay;      // 0.0–1.0, default 0.0 (bypassed)
    std::array<std::atomic<float>, NUM_STEPS> ksTune;       // 50–2000 Hz, default 220.0

    // Per-step envelope: attack 0–1000 ms, decay 0–2000 ms
    // decay == 0 → no decay (sustained at peak throughout step)
    std::array<std::atomic<float>, NUM_STEPS> stepAttack;
    std::array<std::atomic<float>, NUM_STEPS> stepDecay;

    // Transport
    std::atomic<float> bpm        { 120.0f };
    std::atomic<float> masterGain { 0.7f   };
    std::atomic<bool>  isPlaying  { false  };

    // Audio → UI feedback
    std::atomic<int>   currentPlayStep { -1 };
    std::array<std::atomic<float>, NUM_OSCILLATORS> liveAmplitudes;

    SynthSharedState()
    {
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            // Stagger default pitches within the 8-note grid range
            stepPitch[s]   .store((s * 2) % PITCH_GRID_ROWS, std::memory_order_relaxed);
            stepIsCustom[s].store(true,                       std::memory_order_relaxed);
            stepDuration[s]  .store(1.0f,   std::memory_order_relaxed);
            stretchRatio[s]  .store(1.0f,   std::memory_order_relaxed);
            freqShift[s]     .store(0.0f,   std::memory_order_relaxed);
            phaseRand[s]     .store(0.0f,   std::memory_order_relaxed);
            ksDecay[s]       .store(0.0f,   std::memory_order_relaxed);
            ksTune[s]        .store(220.0f, std::memory_order_relaxed);
            stepAttack[s]  .store(0.0f, std::memory_order_relaxed);
            stepDecay[s]   .store(0.0f, std::memory_order_relaxed);
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
