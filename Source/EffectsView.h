#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// EffectsView
//
// Effects-chain matrix — same column-per-step layout as MatrixView.
// Rows (top → bottom):
//   — harmonic stretch —  stretch ratio                         paramId 0
//   — frequency shift —   freq shift                            paramId 1
//   — waveshaping —       fold amount                           paramId 2
//   — granular —          mix, grain size, density,
//                         pitch scatter, feedback               paramId 3–7
//   — shimmer reverb —    reverb size, reverb damp,
//                         shimmer amount, shimmer tune          paramId 8–11
//
// Writes directly to SynthSharedState atomics; the audio engine morphs these
// using the same stepDuration ramp as harmonic amplitudes.
//==============================================================================
class EffectsView : public juce::Component
{
public:
    explicit EffectsView(SynthSharedState& sharedState);
    ~EffectsView() override = default;

    int  getPreferredHeight() const;
    void setCurrentPlayStep(int step);

    std::function<void(int)> onStepSelected;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    SynthSharedState& state;

    //==========================================================================
    // Row definitions
    //==========================================================================
    enum class RowType { SectionHeader, Slider };

    struct RowInfo
    {
        RowType      type;
        juce::String label;
        int          paramId; // see header comment for mapping (0–11)
        int          y, h;
    };

    std::vector<RowInfo> rows;
    void buildRows();

    //==========================================================================
    // Layout constants (match MatrixView for visual consistency)
    //==========================================================================
    static constexpr int kLabelW     = 80;
    static constexpr int kColHeaderH = 30;
    static constexpr int kSectionH   = 20;
    static constexpr int kSliderH    = 30;

    int colX(int col) const;
    int colW()        const;
    int colFromX(int x) const;
    int rowFromY(int y) const;

    //==========================================================================
    // Parameter value helpers
    //==========================================================================
    float getRaw (int paramId, int step) const;
    void  setRaw (int paramId, int step, float raw);
    float normToRaw(int paramId, float norm) const;
    float rawToNorm(int paramId, float raw)  const;
    juce::String formatVal(int paramId, float raw) const;

    //==========================================================================
    // Drawing
    //==========================================================================
    void drawColumnHeaders(juce::Graphics& g) const;
    void drawSectionHeader(juce::Graphics& g, const RowInfo& r) const;
    void drawSliderRow    (juce::Graphics& g, const RowInfo& r) const;

    //==========================================================================
    // Selection / drag state
    //==========================================================================
    int selectedRowIndex = -1;
    int selectedCol      = -1;
    int currentPlayStep  = -1;

    struct Drag
    {
        bool  active    = false;
        int   rowIndex  = -1;
        int   col       = -1;
        int   startX    = 0;
        float startNorm = 0.0f;
    } drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsView)
};
