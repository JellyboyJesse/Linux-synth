#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// MatrixView
//
// A grid component where columns = sequencer steps and rows = parameters.
// Intended to be placed inside a juce::Viewport so it can scroll vertically
// if taller than the available area.
//
// Row layout (top → bottom):
//   [column headers]
//   — pitch sequencer —        (section header)
//   pitch                      (5×8 mini-note grid)
//   — harmonics —              (section header)
//   H1 / H2 / H3 / H4         (drag-to-set amplitude bars)
//   sub                        (sub-oscillator amplitude bar, rootHz × 0.5)
//   — envelope —               (section header)
//   attack                     (drag horizontal slider, 0–1000 ms)
//   decay                      (drag horizontal slider, 0–2000 ms)
//
// Interaction:
//   • Click a row label  → select that row (highlights label)
//   • Click a step header → select that column (highlights header)
//   • Click any cell      → selects that column as active step
//   • Drag amplitude / slider cells → updates SynthSharedState atomics live
//
// Callback:
//   onStepSelected(step)  — fired when a step column becomes active
//==============================================================================
class MatrixView : public juce::Component
{
public:
    explicit MatrixView(SynthSharedState& sharedState);
    ~MatrixView() override = default;

    // Returns the natural content height — use this to size the component
    // before placing it in a Viewport.
    int getPreferredHeight() const;

    // Called by MainComponent's timer to update the playing-step highlight.
    void setCurrentPlayStep(int step);

    std::function<void(int step)> onStepSelected;

    //==========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    SynthSharedState& state;

    //==========================================================================
    // Row types and layout
    //==========================================================================
    enum class RowType { SectionHeader, Pitch, HarmonicBar, SubOscBar,
                         AttackSlider, DecaySlider, DurationSlider, ChildPitch };

    struct RowInfo
    {
        RowType      type;
        juce::String label;
        int          paramIndex; // osc index (0-3) for Bar/Morph; 0=attack/1=decay for Env
        int          y;          // top y in component coordinates
        int          h;          // row height
    };

    std::vector<RowInfo> rows;
    void buildRows();

    //==========================================================================
    // Layout constants
    //==========================================================================
    static constexpr int kLabelW        = 80;
    static constexpr int kColHeaderH    = 30;
    static constexpr int kSectionH      = 20;
    static constexpr int kPitchH        = 96;  // 8 mini rows × 12 px
    static constexpr int kBarH          = 40;
    static constexpr int kSliderH       = 30;
    static constexpr int kMiniNoteH     = 12;  // height of one pitch mini-row
    static constexpr int kChildPitchH   = 78;  // child step count (16px) + mini-grid (56px) + padding
    static constexpr int kChildCountH   = 16;  // step-count selector at top of child row
    static constexpr int kToggleSize    = 14;  // +/- expand toggle in column header

    // 8-note chromatic pitch grid (semitones 0–7, bottom=0=C4, top=7=G4)
    static constexpr int kPitchRows   = PITCH_GRID_ROWS; // == 8

    //==========================================================================
    // Hit-test helpers
    //==========================================================================
    int  colFromX(int x) const;         // -1 if in label area
    int  rowIndexFromY(int y) const;    // index into rows[], -1 if col header

    // For pitch rows: which mini note row within a pitch cell? (0=top, 7=bottom)
    // Returns the semitone (0-7) that row corresponds to.
    int  pitchSemitoneFromY(int cellY, int cellH, int mouseY) const;

    // Get/set parameter value for a cell (normalised 0-1 for sliders)
    float getCellValue(const RowInfo& row, int step) const;
    void  setCellValue(const RowInfo& row, int step, float normalised);
    float normFromRaw(const RowInfo& row, float raw) const;
    float rawFromNorm(const RowInfo& row, float norm) const;

    //==========================================================================
    // Drawing helpers
    //==========================================================================
    int colX(int col) const;    // left edge of step column
    int colW() const;           // column width

    void drawColumnHeaders(juce::Graphics& g) const;
    void drawSectionHeader(juce::Graphics& g, const RowInfo& r) const;
    void drawPitchRow     (juce::Graphics& g, const RowInfo& r) const;
    void drawBarRow       (juce::Graphics& g, const RowInfo& r) const;
    void drawSliderRow    (juce::Graphics& g, const RowInfo& r) const;
    void drawChildPitchRow(juce::Graphics& g, const RowInfo& r) const;

    //==========================================================================
    // Selection state
    //==========================================================================
    int selectedRowIndex  = -1;  // index into rows[]
    int selectedCol       = -1;  // 0-4
    int currentPlayStep   = -1;

    // Per-column expand state for child step UI (local UI only, not in SynthState)
    std::array<bool, NUM_STEPS> columnExpanded {};

    //==========================================================================
    // Drag state
    //==========================================================================
    struct DragState
    {
        bool  active      = false;
        int   rowIndex    = -1;
        int   col         = -1;
        int   startX      = 0;
        int   startY      = 0;
        float startValue  = 0.0f; // normalised 0-1
    } drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixView)
};
