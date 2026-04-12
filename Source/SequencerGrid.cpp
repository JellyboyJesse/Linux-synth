#include "SequencerGrid.h"
#include "WireframeLookAndFeel.h"

SequencerGrid::SequencerGrid(SynthSharedState& sharedState)
    : state(sharedState)
{
}

//==============================================================================
void SequencerGrid::setCurrentPlayStep(int step)
{
    if (currentPlayStep != step) { currentPlayStep = step; repaint(); }
}

void SequencerGrid::setSelectedStep(int step)
{
    if (selectedStep != step) { selectedStep = step; repaint(); }
}

//==============================================================================
void SequencerGrid::resized()
{
    const int gridW = getWidth()  - labelW;
    const int gridH = getHeight() - headerH;
    cellW = gridW / NUM_STEPS;
    cellH = gridH / NUM_SEMITONES;
}

//==============================================================================
juce::Rectangle<int> SequencerGrid::cellBounds(int col, int semitone) const
{
    // semitone 0 = bottom, 11 = top → invert for screen y
    const int row = (NUM_SEMITONES - 1) - semitone;
    return { labelW + col * cellW, headerH + row * cellH, cellW, cellH };
}

juce::Rectangle<int> SequencerGrid::headerBounds(int col) const
{
    return { labelW + col * cellW, 0, cellW, headerH };
}

int SequencerGrid::columnFromX(int x) const
{
    if (x < labelW) return -1;
    const int col = (x - labelW) / (cellW > 0 ? cellW : 1);
    return (col >= 0 && col < NUM_STEPS) ? col : -1;
}

int SequencerGrid::semitoneFromY(int y) const
{
    if (y < headerH) return -1;
    const int row = (y - headerH) / (cellH > 0 ? cellH : 1);
    if (row < 0 || row >= NUM_SEMITONES) return -1;
    return (NUM_SEMITONES - 1) - row; // invert: row 0 = top = semitone 11
}

//==============================================================================
void SequencerGrid::paint(juce::Graphics& g)
{
    const float cornerR = 4.0f;

    // Background
    g.fillAll(Palette::background());

    // --- Step column headers -----------------------------------------------
    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const auto hb = headerBounds(col).toFloat().reduced(2.0f, 2.0f);
        const bool isPlaying  = (col == currentPlayStep);
        const bool isSelected = (col == selectedStep);

        if (isPlaying)
        {
            g.setColour(Palette::accent());
            g.fillRoundedRectangle(hb, cornerR);
        }
        else if (isSelected)
        {
            g.setColour(Palette::fillActive());
            g.fillRoundedRectangle(hb, cornerR);
        }

        g.setColour(isPlaying ? Palette::background() : Palette::outline());
        g.drawRoundedRectangle(hb, cornerR, 1.5f);

        g.setColour(isPlaying ? Palette::background() : Palette::text());
        g.setFont(juce::Font(12.0f));
        g.drawFittedText(juce::String("Step ") + juce::String(col + 1),
                         headerBounds(col), juce::Justification::centred, 1);
    }

    // --- Note-name labels on the left --------------------------------------
    g.setFont(juce::Font(11.0f));
    for (int semi = 0; semi < NUM_SEMITONES; ++semi)
    {
        const int    row    = (NUM_SEMITONES - 1) - semi; // visual row from top
        const auto   labelR = juce::Rectangle<int>(0, headerH + row * cellH, labelW, cellH);
        const bool   isSharp = (juce::String(NOTE_NAMES[semi]).contains("#"));

        g.setColour(isSharp ? Palette::dimOutline() : Palette::text());
        g.drawFittedText(juce::String(NOTE_NAMES[semi]) + "4",
                         labelR, juce::Justification::centredRight, 1);
    }

    // --- Pitch cells --------------------------------------------------------
    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const int   stepPitch  = state.stepPitch[col].load(std::memory_order_relaxed);
        const bool  colPlaying = (col == currentPlayStep);
        const bool  colSelect  = (col == selectedStep);

        for (int semi = 0; semi < NUM_SEMITONES; ++semi)
        {
            const bool   isActive = (semi == stepPitch);
            const auto   cellR    = cellBounds(col, semi).toFloat().reduced(1.5f);

            if (isActive)
            {
                // Filled accent cell = the pitch selected for this step
                g.setColour(colPlaying ? Palette::accent()
                                       : Palette::accent().withAlpha(0.75f));
                g.fillRoundedRectangle(cellR, cornerR);
            }

            // Outline
            const bool highlight = colPlaying || (isActive && colSelect);
            g.setColour(highlight       ? Palette::accent()
                        : isActive       ? Palette::accent().withAlpha(0.5f)
                                         : Palette::dimOutline().withAlpha(0.4f));
            g.drawRoundedRectangle(cellR, cornerR, highlight ? 1.5f : 0.75f);
        }

        // Thick column outline for currently playing / selected step
        if (colPlaying || colSelect)
        {
            const int gridH    = getHeight() - headerH;
            const auto colRect = juce::Rectangle<float>(
                float(labelW + col * cellW) + 1.5f,
                float(headerH)            + 1.5f,
                float(cellW)              - 3.0f,
                float(gridH)              - 3.0f);
            g.setColour(colPlaying ? Palette::accent()
                                   : Palette::accent().withAlpha(0.5f));
            g.drawRoundedRectangle(colRect, cornerR, colPlaying ? 2.0f : 1.5f);
        }
    }

    // Outer border
    g.setColour(Palette::outline());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.5f);
}

//==============================================================================
void SequencerGrid::mouseDown(const juce::MouseEvent& e)
{
    const int col = columnFromX(e.x);
    if (col < 0) return;

    // Step-header click: select step only, don't change pitch
    if (e.y < headerH)
    {
        selectedStep = col;
        if (onStepSelected) onStepSelected(col);
        repaint();
        return;
    }

    const int semi = semitoneFromY(e.y);
    if (semi < 0) return;

    // Pitch-cell click: assign pitch and select step
    state.stepPitch[col].store(semi, std::memory_order_relaxed);
    selectedStep = col;

    if (onStepSelected) onStepSelected(col);
    if (onPitchChanged) onPitchChanged(col, semi);
    repaint();
}
