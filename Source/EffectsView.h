#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// EffectsView
//
// Effects-chain matrix — same column-per-step layout as MatrixView.
// Rows (top → bottom):
//   — harmonic stretch —
//   stretch ratio    (0.5 – 2.0,   drag horizontal)   paramId 0
//   — frequency shift —
//   freq shift       (-200–+200 Hz, drag horizontal)   paramId 1
//   — waveshaping —
//   fold amount      (0.0 – 1.0,   drag horizontal)   paramId 2
//   — karplus-strong —
//   ks decay         (0.0 – 1.0,   drag horizontal)   paramId 3
//   ks tune          (50 – 2000 Hz, drag horizontal)   paramId 4
//   — reverb —
//   reverb size      (0.0 – 1.0,   drag horizontal)   paramId 5
//   reverb damp      (0.0 – 1.0,   drag horizontal)   paramId 6
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
        int          paramId; // see header comment for mapping (0-6)
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
