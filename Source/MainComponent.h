#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include "AudioEngine.h"
#include "WireframeLookAndFeel.h"
#include "MatrixView.h"
#include "WaveformDisplay.h"
#include "TransportBar.h"

//==============================================================================
// MainComponent
//
// Top-level content component.
//
// Layout (top → bottom):
//   TransportBar     — fixed height, full width
//   Viewport         — fills remaining space above waveform; contains MatrixView
//   WaveformDisplay  — fixed height strip at the bottom
//
// A 33 Hz Timer polls audio-thread feedback and drives the playing-step
// highlight in the MatrixView.
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
    // Construction order matters: state before engine, engine before UI
    //==========================================================================
    SynthSharedState         synthState;
    juce::AudioDeviceManager deviceManager;
    AudioEngine              audioEngine { synthState };

    WireframeLookAndFeel     wireframeLAF;

    TransportBar             transportBar    { synthState };
    MatrixView               matrixView      { synthState };
    juce::Viewport           matrixViewport;
    WaveformDisplay          waveformDisplay { synthState };

    int lastReportedStep = -1;

    void setupAudio();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
