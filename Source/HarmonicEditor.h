#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// HarmonicEditor
//
// Shows the amplitude snapshot for the currently selected step (or the global
// preset when selectedStep == -1).
//
// Layout (top-to-bottom):
//   • 4 vertical EQ-bar sliders (one per oscillator)
//   • Waveform display — composite sine wave, updated in real time
//   • Custom / Inherited toggle button
//
// The EQ bars are drawn manually in paint(); mouseDown/mouseDrag handle
// dragging a bar to set its amplitude.  All writes go directly to the
// SynthSharedState atomics, so they are immediately visible to the audio thread.
//
// A juce::Timer (30 ms) drives waveform repaints during playback.
//==============================================================================
class HarmonicEditor : public juce::Component,
                       public juce::Timer
{
public:
    explicit HarmonicEditor(SynthSharedState& sharedState);
    ~HarmonicEditor() override;

    //==========================================================================
    // Called by MainComponent when step selection changes
    //==========================================================================
    void setSelectedStep(int step); // -1 = show / edit global preset

    //==========================================================================
    // Callback — notify parent when custom flag changes
    //==========================================================================
    std::function<void()> onStateChanged;

    //==========================================================================
    // Component / Timer overrides
    //==========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void timerCallback() override;

private:
    SynthSharedState& state;
    int selectedStep = -1;

    // Layout zones (set in resized())
    juce::Rectangle<int> barsArea;
    juce::Rectangle<int> waveArea;
    juce::Rectangle<int> toggleArea;

    // Custom / Inherited toggle
    juce::ToggleButton customToggle { "Custom" };

    // Bar drag tracking
    int  dragBarIndex = -1;
    int  dragStartY   = 0;
    float dragStartAmp = 0.0f;

    // Cached live amplitudes for the waveform path
    std::array<float, NUM_OSCILLATORS> displayAmps {};

    //==========================================================================
    // Helpers
    //==========================================================================
    int  barIndexFromX(int x) const;
    void setAmplitude(int barIndex, float amp);
    float getAmplitude(int barIndex) const;
    bool  isEditable() const; // false when "inherited" and a step is selected

    void drawBars(juce::Graphics& g) const;
    void drawWaveform(juce::Graphics& g) const;
    void updateToggleFromState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicEditor)
};
