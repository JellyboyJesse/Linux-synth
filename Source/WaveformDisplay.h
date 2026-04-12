#pragma once
#include <JuceHeader.h>
#include "SynthState.h"

//==============================================================================
// WaveformDisplay
//
// Full-width strip showing the composite waveform of the 4 sine oscillators
// at their current live amplitudes.  Repaints at 33 fps via a juce::Timer.
//
// Visual: outlined rounded rectangle, single stroked path, no fill.
//==============================================================================
class WaveformDisplay : public juce::Component,
                        public juce::Timer
{
public:
    explicit WaveformDisplay(SynthSharedState& sharedState);
    ~WaveformDisplay() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    SynthSharedState& state;
    std::array<float, NUM_OSCILLATORS> displayAmps {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
