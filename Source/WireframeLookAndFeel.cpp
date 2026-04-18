#include "WireframeLookAndFeel.h"

WireframeLookAndFeel::WireframeLookAndFeel()
{
    setColour(juce::ResizableWindow    ::backgroundColourId,     Palette::background());
    setColour(juce::TextButton         ::buttonColourId,         Palette::surface());
    setColour(juce::TextButton         ::buttonOnColourId,       Palette::accent());
    setColour(juce::TextButton         ::textColourOffId,        Palette::dark());
    setColour(juce::TextButton         ::textColourOnId,         juce::Colours::white);
    setColour(juce::ToggleButton       ::textColourId,           Palette::dark());
    setColour(juce::Slider             ::backgroundColourId,     Palette::surface());
    setColour(juce::Slider             ::thumbColourId,          Palette::dark());
    setColour(juce::Slider             ::trackColourId,          Palette::accent());
    setColour(juce::Slider             ::textBoxTextColourId,    Palette::dark());
    setColour(juce::Slider             ::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label              ::textColourId,           Palette::dark());
    setColour(juce::Label              ::backgroundColourId,     juce::Colours::transparentBlack);

    uiFont = juce::Font(11.0f);
}

//==============================================================================
// Buttons
//==============================================================================
void WireframeLookAndFeel::drawButtonBackground(
    juce::Graphics& g,
    juce::Button&   button,
    const juce::Colour& /*backgroundColour*/,
    bool isMouseOverButton,
    bool isButtonDown)
{
    const auto  bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool  isOn   = button.getToggleState();
    const float radius = 6.0f;

    if (isOn || isButtonDown)
        g.setColour(Palette::accent());
    else if (isMouseOverButton)
        g.setColour(Palette::surface().brighter(0.04f));
    else
        g.setColour(Palette::surface());

    g.fillRoundedRectangle(bounds, radius);

    // Only show border when not active
    if (!isOn && !isButtonDown)
    {
        g.setColour(Palette::border());
        g.drawRoundedRectangle(bounds, radius, 0.5f);
    }
}

void WireframeLookAndFeel::drawButtonText(
    juce::Graphics& g,
    juce::TextButton& button,
    bool /*isMouseOverButton*/,
    bool /*isButtonDown*/)
{
    const bool isOn = button.getToggleState();
    g.setColour(isOn ? juce::Colours::white : Palette::dark());
    g.setFont(uiFont);
    g.drawFittedText(button.getButtonText(),
                     button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

void WireframeLookAndFeel::drawToggleButton(
    juce::Graphics& g,
    juce::ToggleButton& button,
    bool isMouseOverButton,
    bool isButtonDown)
{
    const auto  bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool  isOn   = button.getToggleState();
    const float radius = 6.0f;

    if (isOn)
        g.setColour(Palette::accent());
    else if (isMouseOverButton)
        g.setColour(Palette::surface().brighter(0.04f));
    else
        g.setColour(Palette::surface());

    g.fillRoundedRectangle(bounds, radius);

    if (!isOn)
    {
        g.setColour(Palette::border());
        g.drawRoundedRectangle(bounds, radius, 0.5f);
    }

    g.setColour(isOn ? juce::Colours::white : Palette::dark());
    g.setFont(uiFont);
    g.drawFittedText(button.getButtonText(),
                     button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

//==============================================================================
// Sliders — slim 3px track, colAccent fill, colDark 8px thumb dot
//==============================================================================
void WireframeLookAndFeel::drawLinearSliderBackground(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float /*sliderPos*/, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle /*style*/,
    juce::Slider& /*slider*/)
{
    const float trackH  = 3.0f;
    const float trackY  = float(y) + float(height) * 0.5f - trackH * 0.5f;
    const auto  track   = juce::Rectangle<float>(float(x), trackY, float(width), trackH);

    g.setColour(Palette::border());
    g.fillRoundedRectangle(track, trackH * 0.5f);
}

void WireframeLookAndFeel::drawLinearSliderThumb(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle style,
    juce::Slider& /*slider*/)
{
    const float thumbR = 4.0f; // 8px diameter

    float cx, cy;
    if (style == juce::Slider::LinearHorizontal)
    {
        cx = sliderPos;
        cy = float(y) + float(height) * 0.5f;
    }
    else
    {
        cx = float(x) + float(width) * 0.5f;
        cy = sliderPos;
    }

    g.setColour(Palette::dark());
    g.fillEllipse(cx - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);
}

void WireframeLookAndFeel::drawLinearSlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    const float trackH  = 3.0f;
    const float trackY  = float(y) + float(height) * 0.5f - trackH * 0.5f;
    const auto  track   = juce::Rectangle<float>(float(x), trackY, float(width), trackH);

    // Background
    g.setColour(Palette::border());
    g.fillRoundedRectangle(track, trackH * 0.5f);

    // Fill — from min edge to thumb position
    if (style == juce::Slider::LinearHorizontal)
    {
        const float fillW = sliderPos - float(x);
        if (fillW > 0.0f)
        {
            g.setColour(Palette::accent());
            g.fillRoundedRectangle(float(x), trackY, fillW, trackH, trackH * 0.5f);
        }
    }
    else
    {
        const float fillH = (float(y) + float(height)) - sliderPos;
        if (fillH > 0.0f)
        {
            g.setColour(Palette::accent());
            g.fillRoundedRectangle(float(x), sliderPos, float(width), fillH, trackH * 0.5f);
        }
    }

    drawLinearSliderThumb(g, x, y, width, height,
                          sliderPos, minSliderPos, maxSliderPos, style, slider);
}

//==============================================================================
// Labels
//==============================================================================
void WireframeLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    g.drawFittedText(label.getText(),
                     label.getLocalBounds().reduced(2, 0),
                     label.getJustificationType(), 1,
                     label.getMinimumHorizontalScale());
}

juce::Font WireframeLookAndFeel::getLabelFont(juce::Label&)
{
    return uiFont;
}

juce::Font WireframeLookAndFeel::getTextButtonFont(juce::TextButton&, int /*buttonHeight*/)
{
    return uiFont;
}
