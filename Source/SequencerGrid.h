#pragma once
#include <JuceHeader.h>
#include "SynthState.h"
#include <functional>

//==============================================================================
// SequencerGrid
//
// A 5-column × 12-row pitch grid.
//
// Layout:
//   • Column headers (one per step) — click to select a step without changing pitch
//   • Note-name labels on the left
//   • Pitch cells — click to select step AND assign that pitch to it
//
// Pitch rows: row 0 at the bottom = C4 (semitone 0),
//             row 11 at the top   = B4 (semitone 11)
//
// Callbacks (set by MainComponent):
//   onStepSelected(step)              — user clicked a step header or pitch cell
//   onPitchChanged(step, semitone)    — user assigned a new pitch to a step
//==============================================================================
class SequencerGrid : public juce::Component
{
public:
    explicit SequencerGrid(SynthSharedState& sharedState);
    ~SequencerGrid() override = default;

    //==========================================================================
    // State setters — called from UI/timer thread
    //==========================================================================
    void setCurrentPlayStep(int step);   // -1 = stopped
    void setSelectedStep(int step);      // -1 = none

    //==========================================================================
    // Callbacks
    //==========================================================================
    std::function<void(int step)>             onStepSelected;
    std::function<void(int step, int semi)>   onPitchChanged;

    //==========================================================================
    // Component overrides
    //==========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    SynthSharedState& state;

    int currentPlayStep = -1;
    int selectedStep    = -1;

    // Geometry (computed in resized())
    int headerH = 28;   // step-header row height
    int labelW  = 32;   // note-name label column width
    int cellW   = 0;
    int cellH   = 0;

    // Hit-test helpers
    int columnFromX(int x) const;     // returns 0..4 or -1
    int semitoneFromY(int y) const;   // returns 0..11 or -1, 0=bottom

    juce::Rectangle<int> cellBounds(int col, int semitone) const;
    juce::Rectangle<int> headerBounds(int col) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SequencerGrid)
};
