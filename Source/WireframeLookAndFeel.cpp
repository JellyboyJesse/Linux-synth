#include "WireframeLookAndFeel.h"

WireframeLookAndFeel::WireframeLookAndFeel()
{
    // --- Global colour overrides ------------------------------------------
    setColour(juce::ResizableWindow    ::backgroundColourId,  Palette::background());
    setColour(juce::TextButton         ::buttonColourId,      Palette::background());
    setColour(juce::TextButton         ::buttonOnColourId,    Palette::accent());
    setColour(juce::TextButton         ::textColourOffId,     Palette::text());
    setColour(juce::TextButton         ::textColourOnId,      Palette::background());
    setColour(juce::ToggleButton       ::textColourId,        Palette::text());
    setColour(juce::Slider             ::backgroundColourId,  Palette::background());
    setColour(juce::Slider             ::thumbColourId,       Palette::accent());
    setColour(juce::Slider             ::trackColourId,       Palette::accent());
    setColour(juce::Slider             ::textBoxTextColourId, Palette::text());
    setColour(juce::Slider             ::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label              ::textColourId,        Palette::text());
    setColour(juce::Label              ::backgroundColourId,  juce::Colours::transparentBlack);

    uiFont = juce::Font(14.0f);
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
    const auto bounds  = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn    = button.getToggleState();
    const float radius = 5.0f;

    if (isOn || isButtonDown)
        g.setColour(Palette::accent());
    else if (isMouseOverButton)
        g.setColour(Palette::fillActive());
    else
        g.setColour(Palette::background());

    g.fillRoundedRectangle(bounds, radius);

    g.setColour(isOn || isButtonDown ? Palette::accent() : Palette::outline());
    g.drawRoundedRectangle(bounds, radius, 1.5f);
}

void WireframeLookAndFeel::drawButtonText(
    juce::Graphics& g,
    juce::TextButton& button,
    bool /*isMouseOverButton*/,
    bool /*isButtonDown*/)
{
    const bool isOn = button.getToggleState();
    g.setColour(isOn ? Palette::background() : Palette::text());
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
    const auto bounds  = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn    = button.getToggleState();
    const float radius = 5.0f;

    if (isOn)
        g.setColour(Palette::accent());
    else if (isMouseOverButton)
        g.setColour(Palette::fillActive());
    else
        g.setColour(Palette::background());

    g.fillRoundedRectangle(bounds, radius);
    g.setColour(Palette::outline());
    g.drawRoundedRectangle(bounds, radius, 1.5f);

    g.setColour(isOn ? Palette::background() : Palette::text());
    g.setFont(uiFont);
    g.drawFittedText(button.getButtonText(),
                     button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

//==============================================================================
// Sliders — horizontal and vertical linear sliders
//==============================================================================
void WireframeLookAndFeel::drawLinearSliderBackground(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float /*sliderPos*/, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle /*style*/,
    juce::Slider& /*slider*/)
{
    const auto trackBounds = juce::Rectangle<float>(
        float(x), float(y), float(width), float(height));
    const float radius = 3.0f;

    g.setColour(Palette::background());
    g.fillRoundedRectangle(trackBounds, radius);
    g.setColour(Palette::dimOutline());
    g.drawRoundedRectangle(trackBounds, radius, 1.0f);
}

void WireframeLookAndFeel::drawLinearSliderThumb(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    const float thumbRadius = float(getSliderThumbRadius(slider));

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

    g.setColour(Palette::background());
    g.fillEllipse(cx - thumbRadius, cy - thumbRadius,
                  thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour(Palette::accent());
    g.drawEllipse(cx - thumbRadius, cy - thumbRadius,
                  thumbRadius * 2.0f, thumbRadius * 2.0f, 2.0f);
    // Centre dot
    g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
}

void WireframeLookAndFeel::drawLinearSlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    drawLinearSliderBackground(g, x, y, width, height,
                               sliderPos, minSliderPos, maxSliderPos, style, slider);

    // Draw filled track segment from min edge to thumb
    const float radius = 3.0f;
    if (style == juce::Slider::LinearHorizontal)
    {
        const float fillW = sliderPos - float(x);
        if (fillW > 0.0f)
        {
            g.setColour(Palette::accent().withAlpha(0.5f));
            g.fillRoundedRectangle(float(x), float(y), fillW, float(height), radius);
        }
    }
    else // LinearVertical
    {
        const float trackBottom = float(y) + float(height);
        const float fillH = trackBottom - sliderPos;
        if (fillH > 0.0f)
        {
            g.setColour(Palette::accent().withAlpha(0.5f));
            g.fillRoundedRectangle(float(x), sliderPos, float(width), fillH, radius);
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
