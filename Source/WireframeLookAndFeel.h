#pragma once
#include <JuceHeader.h>

//==============================================================================
// Palette
//==============================================================================
namespace Palette
{
    inline juce::Colour background() { return juce::Colour(0xFFD6E4F0); }
    inline juce::Colour accent()     { return juce::Colour(0xFF2E6E9E); }
    inline juce::Colour text()       { return juce::Colour(0xFF1A3F5C); }
    inline juce::Colour outline()    { return juce::Colour(0xFF2E6E9E); }
    inline juce::Colour dimOutline() { return juce::Colour(0xFF8AAFC8); }
    inline juce::Colour fillActive() { return juce::Colour(0x402E6E9E); } // accent @25%
}

//==============================================================================
// WireframeLookAndFeel
//
// Blueprint / wireframe aesthetic:
//   • Flat background, no gradients, no drop shadows
//   • All components outlined only (rounded rectangles)
//   • Active / selected state fills with accent colour
//   • Single type weight, sentence-case labels
//==============================================================================
class WireframeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WireframeLookAndFeel();
    ~WireframeLookAndFeel() override = default;

    //==========================================================================
    // Buttons
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button&   button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isMouseOverButton,
                        bool isButtonDown) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isMouseOverButton,
                          bool isButtonDown) override;

    //==========================================================================
    // Sliders
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawLinearSliderBackground(juce::Graphics& g,
                                    int x, int y, int width, int height,
                                    float sliderPos, float minSliderPos, float maxSliderPos,
                                    juce::Slider::SliderStyle style,
                                    juce::Slider& slider) override;

    void drawLinearSliderThumb(juce::Graphics& g,
                               int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle style,
                               juce::Slider& slider) override;

    int  getSliderThumbRadius(juce::Slider&) override { return 7; }

    //==========================================================================
    // Labels
    //==========================================================================
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

private:
    juce::Font uiFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WireframeLookAndFeel)
};
