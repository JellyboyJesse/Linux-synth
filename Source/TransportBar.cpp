#include "TransportBar.h"
#include "WireframeLookAndFeel.h"

TransportBar::TransportBar(SynthSharedState& sharedState)
    : state(sharedState)
{
    // ---- Play / Stop button ------------------------------------------------
    addAndMakeVisible(playButton);
    playButton.setClickingTogglesState(true);
    playButton.setToggleState(false, juce::dontSendNotification);

    playButton.onClick = [this]
    {
        const bool nowPlaying = playButton.getToggleState();
        playButton.setButtonText(nowPlaying ? "Stop" : "Play");
        state.isPlaying.store(nowPlaying, std::memory_order_relaxed);

        if (!nowPlaying)
            state.currentPlayStep.store(-1, std::memory_order_relaxed);

        if (onPlayStateChanged) onPlayStateChanged(nowPlaying);
    };

    // ---- BPM slider --------------------------------------------------------
    addAndMakeVisible(bpmLabel);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(bpmSlider);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bpmSlider.setRange(40.0, 200.0, 1.0);
    bpmSlider.setValue(double(state.bpm.load()), juce::dontSendNotification);

    addAndMakeVisible(bpmValue);
    bpmValue.setText(juce::String(int(state.bpm.load())), juce::dontSendNotification);
    bpmValue.setJustificationType(juce::Justification::centredLeft);
    bpmValue.setColour(juce::Label::textColourId, juce::Colours::white);

    bpmSlider.onValueChange = [this]
    {
        const float bpm = float(bpmSlider.getValue());
        state.bpm.store(bpm, std::memory_order_relaxed);
        bpmValue.setText(juce::String(int(bpm)), juce::dontSendNotification);
    };

    // ---- Volume slider -----------------------------------------------------
    addAndMakeVisible(volLabel);
    volLabel.setJustificationType(juce::Justification::centredRight);
    volLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(volSlider);
    volSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volSlider.setRange(0.0, 1.0, 0.01);
    volSlider.setValue(double(state.masterGain.load()), juce::dontSendNotification);
    volSlider.setSkewFactorFromMidPoint(0.5);

    volSlider.onValueChange = [this]
    {
        state.masterGain.store(float(volSlider.getValue()), std::memory_order_relaxed);
    };
}

//==============================================================================
void TransportBar::resized()
{
    const int h    = getHeight();
    const int pad  = 8;
    const int btnW = 80;
    const int lblW = 54;
    const int valW = 38;

    int x = pad;

    playButton.setBounds(x, pad, btnW, h - 2 * pad);
    x += btnW + pad * 2;

    bpmLabel.setBounds(x, pad, lblW, h - 2 * pad);
    x += lblW + 4;

    const int sldW = 140;
    bpmSlider.setBounds(x, pad, sldW, h - 2 * pad);
    x += sldW + 4;

    bpmValue.setBounds(x, pad, valW, h - 2 * pad);
    x += valW + pad * 2;

    volLabel.setBounds(x, pad, lblW, h - 2 * pad);
    x += lblW + 4;

    volSlider.setBounds(x, pad, sldW, h - 2 * pad);
}

//==============================================================================
void TransportBar::paint(juce::Graphics& g)
{
    // Dark background with 10px radius on top corners only
    const auto bounds = getLocalBounds().toFloat();
    juce::Path p;
    p.addRoundedRectangle(bounds.getX(), bounds.getY(),
                          bounds.getWidth(), bounds.getHeight(),
                          10.0f, 10.0f,
                          true,  // top-left
                          true,  // top-right
                          false, // bottom-left
                          false);// bottom-right
    g.setColour(Palette::dark());
    g.fillPath(p);
}
