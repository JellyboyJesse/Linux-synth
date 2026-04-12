#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include "AudioEngine.h"
#include "WireframeLookAndFeel.h"
#include "SequencerGrid.h"
#include "HarmonicEditor.h"
#include "TransportBar.h"

//==============================================================================
// MainComponent
//
// Top-level content component.  Owns:
//   • SynthSharedState  — the shared lock-free state object
//   • AudioDeviceManager — JUCE audio device (ALSA / JACK)
//   • AudioEngine        — the real-time audio callback
//   • WireframeLookAndFeel — applied globally
//   • All UI sub-components
//
// A juce::Timer (30 ms) polls the audio-thread feedback fields and updates
// the UI step-highlight and waveform display.
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
    // Shared state + engine (construction order matters)
    //==========================================================================
    SynthSharedState         synthState;
    juce::AudioDeviceManager deviceManager;
    AudioEngine              audioEngine { synthState };

    //==========================================================================
    // Look and feel
    //==========================================================================
    WireframeLookAndFeel wireframeLAF;

    //==========================================================================
    // UI components
    //==========================================================================
    TransportBar    transportBar { synthState };
    SequencerGrid   sequencerGrid { synthState };
    HarmonicEditor  harmonicEditor { synthState };

    //==========================================================================
    // State tracked by the UI-update timer
    //==========================================================================
    int  lastReportedStep = -1;
    int  selectedStep     = -1;

    //==========================================================================
    // Helpers
    //==========================================================================
    void setupAudio();
    void onStepSelected(int step);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
