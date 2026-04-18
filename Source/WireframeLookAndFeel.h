#pragma once
#include <JuceHeader.h>

//==============================================================================
// Palette — amber and ink on warm off-white
//==============================================================================
namespace Palette
{
    // Core colours
    inline juce::Colour background() { return juce::Colour(0xFFF5F3EE); } // warm off-white
    inline juce::Colour surface()    { return juce::Colour(0xFFECEAE4); } // inactive cells
    inline juce::Colour dark()       { return juce::Colour(0xFF1A1A18); } // near-black
    inline juce::Colour accent()     { return juce::Colour(0xFFE8A020); } // amber
    inline juce::Colour section()    { return juce::Colour(0xFFC4340A); } // deep red-orange
    inline juce::Colour mid()        { return juce::Colour(0xFF88887E); } // mid grey-brown

    // Semantic aliases kept for backward compatibility
    inline juce::Colour text()       { return dark(); }
    inline juce::Colour outline()    { return dark().withAlpha(0.15f); }
    inline juce::Colour dimOutline() { return mid(); }
    inline juce::Colour fillActive() { return accent().withAlpha(0.15f); }
    inline juce::Colour border()     { return dark().withAlpha(0.15f); }
}

//==============================================================================
// WireframeLookAndFeel
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

    int  getSliderThumbRadius(juce::Slider&) override { return 4; }

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
