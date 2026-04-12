#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// TransportBar
//
// Horizontal bar containing:
//   [Play / Stop]   BPM: [slider  40–200]  Volume: [slider 0–1]
//
// All interaction writes directly to SynthSharedState atomics.
// The parent can also register an onPlayStateChanged callback to know when
// play/stop is toggled (e.g. to reset the sequencer UI step highlight).
//==============================================================================
class TransportBar : public juce::Component
{
public:
    explicit TransportBar(SynthSharedState& sharedState);
    ~TransportBar() override = default;

    std::function<void(bool isPlaying)> onPlayStateChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SynthSharedState& state;

    juce::TextButton  playButton  { "Play" };

    juce::Label bpmLabel   { {}, "BPM" };
    juce::Slider bpmSlider;
    juce::Label bpmValue;

    juce::Label volLabel   { {}, "Volume" };
    juce::Slider volSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
