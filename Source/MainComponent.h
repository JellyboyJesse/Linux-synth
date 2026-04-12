#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include "AudioEngine.h"
#include "WireframeLookAndFeel.h"
#include "MatrixView.h"
#include "EffectsView.h"
#include "WaveformDisplay.h"
#include "TransportBar.h"

//==============================================================================
// MainComponent
//
// Layout (top → bottom):
//   TransportBar  — transport controls
//   TabBar        — "Notes" / "Effects" toggle buttons (36 px)
//   ContentView   — Viewport containing MatrixView  (Notes tab)
//                 — Viewport containing EffectsView (Effects tab)
//   WaveformDisplay — full-width waveform strip (80 px)
//
// A 33 Hz Timer drives the playing-step highlight in both views.
//==============================================================================
class MainComponent : public juce::Component,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    //==========================================================================
    SynthSharedState         synthState;
    juce::AudioDeviceManager deviceManager;
    AudioEngine              audioEngine { synthState };

    WireframeLookAndFeel     wireframeLAF;

    TransportBar             transportBar    { synthState };

    // Tab buttons
    juce::TextButton         tabNotes   { "Notes"   };
    juce::TextButton         tabEffects { "Effects" };

    // Notes tab
    MatrixView               matrixView  { synthState };
    juce::Viewport           matrixViewport;

    // Effects tab
    EffectsView              effectsView { synthState };
    juce::Viewport           effectsViewport;

    WaveformDisplay          waveformDisplay { synthState };

    int  lastReportedStep = -1;
    bool showingEffects   = false;

    void setupAudio();
    void showTab(bool effects);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
