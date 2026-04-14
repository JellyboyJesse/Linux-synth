#include "MatrixView.h"
#include "WireframeLookAndFeel.h"
#include "ScaleTable.h"

//==============================================================================
MatrixView::MatrixView(SynthSharedState& sharedState)
    : state(sharedState)
{
    buildRows();
}

//==============================================================================
void MatrixView::buildRows()
{
    rows.clear();
    int y = kColHeaderH; // rows start below the column-header strip

    auto push = [&](RowType t, const char* lbl, int param, int h)
    {
        rows.push_back({ t, juce::String(lbl), param, y, h });
        y += h;
    };

    push(RowType::SectionHeader, "pitch sequencer", 0, kSectionH);
    push(RowType::Pitch,         "pitch",           0, kPitchH);
    push(RowType::SectionHeader, "child steps",     0, kSectionH);
    push(RowType::ChildPitch,    "child",           0, kChildPitchH);

    push(RowType::SectionHeader, "harmonics",       0, kSectionH);
    push(RowType::HarmonicBar,   "H1",              0, kBarH);
    push(RowType::HarmonicBar,   "H2",              1, kBarH);
    push(RowType::HarmonicBar,   "H3",              2, kBarH);
    push(RowType::HarmonicBar,   "H4",              3, kBarH);
    push(RowType::SubOscBar,     "sub",             0, kBarH);

    push(RowType::SectionHeader,  "envelope",        0, kSectionH);
    push(RowType::AttackSlider,   "attack",          0, kSliderH);
    push(RowType::DecaySlider,    "decay",           0, kSliderH);
    push(RowType::DurationSlider, "step duration",   0, kSliderH);
}

int MatrixView::getPreferredHeight() const
{
    if (rows.empty()) return kColHeaderH;
    const auto& last = rows.back();
    return last.y + last.h;
}

//==============================================================================
void MatrixView::setCurrentPlayStep(int step)
{
    if (currentPlayStep != step) { currentPlayStep = step; repaint(); }
}

//==============================================================================
// Layout helpers
//==============================================================================
int MatrixView::colX(int col) const
{
    return kLabelW + col * colW();
}

int MatrixView::colW() const
{
    const int avail = getWidth() - kLabelW;
    return avail / NUM_STEPS;
}

int MatrixView::colFromX(int x) const
{
    if (x < kLabelW) return -1;
    const int cw = colW();
    if (cw <= 0) return -1;
    const int c = (x - kLabelW) / cw;
    return (c >= 0 && c < NUM_STEPS) ? c : -1;
}

int MatrixView::rowIndexFromY(int y) const
{
    if (y < kColHeaderH) return -1; // in column header
    for (int i = 0; i < int(rows.size()); ++i)
        if (y >= rows[i].y && y < rows[i].y + rows[i].h)
            return i;
    return -1;
}

int MatrixView::pitchSemitoneFromY(int cellY, int cellH, int mouseY) const
{
    const int localY = mouseY - cellY;
    const int row    = juce::jlimit(0, kPitchRows - 1,
                                    localY * kPitchRows / (cellH > 0 ? cellH : 1));
    // Row 0 (top) = semitone 7 (G4), row 7 (bottom) = semitone 0 (C4)
    return (kPitchRows - 1) - row;
}

//==============================================================================
// Parameter value helpers
//==============================================================================
float MatrixView::normFromRaw(const RowInfo& row, float raw) const
{
    switch (row.type)
    {
        case RowType::HarmonicBar:
        case RowType::SubOscBar:      return juce::jlimit(0.0f, 1.0f, raw);
        case RowType::AttackSlider:   return juce::jlimit(0.0f, 1.0f, raw / 1000.0f);
        case RowType::DecaySlider:    return juce::jlimit(0.0f, 1.0f, raw / 2000.0f);
        case RowType::DurationSlider: return juce::jlimit(0.0f, 1.0f, (raw - 0.1f) / 7.9f);
        default: return 0.0f;  // ChildPitch not drag-controlled
    }
}

float MatrixView::rawFromNorm(const RowInfo& row, float norm) const
{
    const float n = juce::jlimit(0.0f, 1.0f, norm);
    switch (row.type)
    {
        case RowType::HarmonicBar:
        case RowType::SubOscBar:      return n;
        case RowType::AttackSlider:   return n * 1000.0f;
        case RowType::DecaySlider:    return n * 2000.0f;
        case RowType::DurationSlider: return 0.1f + n * 7.9f;
        default: return 0.0f;
    }
}

float MatrixView::getCellValue(const RowInfo& row, int step) const
{
    const int osc = row.paramIndex;
    switch (row.type)
    {
        case RowType::SubOscBar:
            return normFromRaw(row,
                state.subAmp[step].load(std::memory_order_relaxed));
        case RowType::HarmonicBar:
            return normFromRaw(row,
                state.stepAmplitudes[step][osc].load(std::memory_order_relaxed));
        case RowType::AttackSlider:
            return normFromRaw(row,
                state.stepAttack[step].load(std::memory_order_relaxed));
        case RowType::DecaySlider:
            return normFromRaw(row,
                state.stepDecay[step].load(std::memory_order_relaxed));
        case RowType::DurationSlider:
            return normFromRaw(row,
                state.stepDuration[step].load(std::memory_order_relaxed));
        default: return 0.0f;
    }
}

void MatrixView::setCellValue(const RowInfo& row, int step, float normalised)
{
    const float raw = rawFromNorm(row, normalised);
    const int   osc = row.paramIndex;
    switch (row.type)
    {
        case RowType::SubOscBar:
            state.subAmp[step].store(raw, std::memory_order_relaxed);
            break;
        case RowType::HarmonicBar:
            state.stepAmplitudes[step][osc].store(raw, std::memory_order_relaxed);
            state.stepIsCustom[step].store(true, std::memory_order_relaxed);
            break;
        case RowType::AttackSlider:
            state.stepAttack[step].store(raw, std::memory_order_relaxed);
            break;
        case RowType::DecaySlider:
            state.stepDecay[step].store(raw, std::memory_order_relaxed);
            break;
        case RowType::DurationSlider:
            state.stepDuration[step].store(raw, std::memory_order_relaxed);
            break;
        default: break;
    }
}

//==============================================================================
// Mouse handling
//==============================================================================
void MatrixView::mouseDown(const juce::MouseEvent& e)
{
    drag.active = false;

    const int col = colFromX(e.x);

    // Expand toggle in column header (top-right corner of each step header)
    if (e.y < kColHeaderH && col >= 0)
    {
        const int toggleX = colX(col) + colW() - kToggleSize - 2;
        const int toggleY = (kColHeaderH - kToggleSize) / 2;
        if (e.x >= toggleX && e.x < toggleX + kToggleSize
            && e.y >= toggleY && e.y < toggleY + kToggleSize)
        {
            columnExpanded[size_t(col)] = !columnExpanded[size_t(col)];
            if (!columnExpanded[size_t(col)])
                state.childEnabled[col].store(false, std::memory_order_relaxed);
            repaint();
            return;
        }

        selectedCol = col;
        if (onStepSelected) onStepSelected(col);
        repaint();
        return;
    }

    // Click on row label
    if (e.x < kLabelW)
    {
        selectedRowIndex = rowIndexFromY(e.y);
        repaint();
        return;
    }

    if (col < 0) return;

    const int ri = rowIndexFromY(e.y);
    if (ri < 0) return;
    const RowInfo& row = rows[size_t(ri)];

    // Always select this column as active step
    selectedCol      = col;
    selectedRowIndex = ri;
    if (onStepSelected) onStepSelected(col);

    if (row.type == RowType::SectionHeader || row.type == RowType::ChildPitch)
    {
        repaint();
        return;
    }

    if (row.type == RowType::Pitch)
    {
        const int semi = pitchSemitoneFromY(row.y, row.h, e.y);
        state.stepPitch[col].store(semi, std::memory_order_relaxed);
        repaint();
        return;
    }

    if (row.type == RowType::ChildPitch)
    {
        if (!columnExpanded[size_t(col)]) { repaint(); return; }

        const int cw = colW();
        const int cx = colX(col);

        // Step count selector (top kChildCountH px of the cell)
        if (e.y >= row.y && e.y < row.y + kChildCountH)
        {
            const int cur = state.childStepCount[col].load(std::memory_order_relaxed);
            state.childStepCount[col].store((cur % NUM_STEPS) + 1, std::memory_order_relaxed);
            repaint();
            return;
        }

        // Child pitch mini-grid
        const int gridY = row.y + kChildCountH + 2;
        const int gridH = row.h - kChildCountH - 4;
        const int stepCount = juce::jlimit(1, NUM_STEPS,
            state.childStepCount[col].load(std::memory_order_relaxed));
        const int childCellW = cw / stepCount;

        if (e.x >= cx && e.x < cx + cw && e.y >= gridY && e.y < gridY + gridH)
        {
            const int cs = (e.x - cx) / juce::jmax(1, childCellW);
            if (cs >= 0 && cs < stepCount)
            {
                const int miniH = gridH / kPitchRows;
                const int mrow  = (e.y - gridY) / juce::jmax(1, miniH);
                const int semi  = (kPitchRows - 1) - mrow;
                if (semi >= 0 && semi < kPitchRows)
                {
                    state.childPitch[col][cs].store(semi, std::memory_order_relaxed);
                    state.childEnabled[col] .store(true,  std::memory_order_relaxed);
                    repaint();
                }
            }
        }
        return;
    }

    // Draggable rows: record start state
    drag.active     = true;
    drag.rowIndex   = ri;
    drag.col        = col;
    drag.startX     = e.x;
    drag.startY     = e.y;
    drag.startValue = getCellValue(row, col);
    repaint();
}

void MatrixView::mouseDrag(const juce::MouseEvent& e)
{
    if (!drag.active || drag.rowIndex < 0) return;
    const RowInfo& row = rows[size_t(drag.rowIndex)];

    float newNorm = drag.startValue;

    if (row.type == RowType::HarmonicBar || row.type == RowType::SubOscBar)
    {
        // Vertical drag: up = higher amplitude
        const float delta = float(drag.startY - e.y) / float(juce::jmax(1, row.h));
        newNorm = drag.startValue + delta;
    }
    else
    {
        // Horizontal drag for sliders
        const float trackW = float(juce::jmax(1, colW() - 8));
        const float delta  = float(e.x - drag.startX) / trackW;
        newNorm = drag.startValue + delta;
    }

    setCellValue(row, drag.col, newNorm);
    repaint();
}

//==============================================================================
// resized — nothing dynamic needed; geometry computed on demand
//==============================================================================
void MatrixView::resized() {}

//==============================================================================
// paint
//==============================================================================
void MatrixView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());
    drawColumnHeaders(g);

    for (int i = 0; i < int(rows.size()); ++i)
    {
        const RowInfo& row = rows[size_t(i)];
        switch (row.type)
        {
            case RowType::SectionHeader:            drawSectionHeader(g, row);  break;
            case RowType::Pitch:                    drawPitchRow(g, row);       break;
            case RowType::ChildPitch:               drawChildPitchRow(g, row);  break;
            case RowType::HarmonicBar:
            case RowType::SubOscBar:                drawBarRow(g, row);         break;
            case RowType::AttackSlider:
            case RowType::DecaySlider:
            case RowType::DurationSlider:           drawSliderRow(g, row);      break;
        }
    }
}

//==============================================================================
// Drawing helpers
//==============================================================================
void MatrixView::drawColumnHeaders(juce::Graphics& g) const
{
    const float cornerR = 4.0f;
    g.setFont(juce::Font(12.0f));

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const bool isPlay = (col == currentPlayStep);
        const bool isSel  = (col == selectedCol);
        const auto hb = juce::Rectangle<float>(
            float(colX(col)) + 2.0f, 2.0f,
            float(colW()) - 4.0f, float(kColHeaderH) - 4.0f);

        if (isPlay)
            g.setColour(Palette::accent());
        else if (isSel)
            g.setColour(Palette::fillActive());
        else
            g.setColour(Palette::background());
        g.fillRoundedRectangle(hb, cornerR);

        g.setColour(isPlay ? Palette::accent() : Palette::outline());
        g.drawRoundedRectangle(hb, cornerR, 1.5f);

        // Shift label left to make room for toggle
        const auto labelR = hb.withTrimmedRight(float(kToggleSize + 2));
        g.setColour(isPlay ? Palette::background() : Palette::text());
        g.drawFittedText("Step " + juce::String(col + 1),
                         labelR.toNearestInt(), juce::Justification::centred, 1);

        // +/- expand toggle
        const bool expanded = columnExpanded[size_t(col)];
        const auto toggleR = juce::Rectangle<float>(
            hb.getRight() - float(kToggleSize),
            hb.getCentreY() - float(kToggleSize) * 0.5f,
            float(kToggleSize), float(kToggleSize));
        g.setColour(expanded ? Palette::accent()
                             : Palette::dimOutline().withAlpha(0.6f));
        g.drawRoundedRectangle(toggleR, 2.0f, 1.0f);
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(expanded ? "-" : "+",
                         toggleR.toNearestInt(), juce::Justification::centred, 1);
    }

    // Separator line
    g.setColour(Palette::dimOutline().withAlpha(0.4f));
    g.drawHorizontalLine(kColHeaderH, float(kLabelW), float(getWidth()));
}

void MatrixView::drawSectionHeader(juce::Graphics& g, const RowInfo& r) const
{
    const auto bounds = juce::Rectangle<int>(0, r.y, getWidth(), r.h).toFloat();
    g.setColour(Palette::accent().withAlpha(0.12f));
    g.fillRect(bounds);
    g.setColour(Palette::accent().withAlpha(0.4f));
    g.drawHorizontalLine(r.y, 0.0f, float(getWidth()));

    g.setColour(Palette::accent());
    g.setFont(juce::Font(11.0f));
    g.drawFittedText(r.label.toUpperCase(),
                     juce::Rectangle<int>(kLabelW + 4, r.y, getWidth() - kLabelW - 8, r.h),
                     juce::Justification::centredLeft, 1);
}

void MatrixView::drawPitchRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR  = 3.0f;
    const int   cw       = colW();

    // Row label
    const bool rowSel = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);
    g.setColour(rowSel ? Palette::accent() : Palette::text());
    g.setFont(juce::Font(11.0f));
    g.drawFittedText(r.label,
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const int stepPitch = state.stepPitch[col].load(std::memory_order_relaxed);
        const bool colSel   = (col == selectedCol);
        const bool colPlay  = (col == currentPlayStep);
        const int  cx       = colX(col);

        for (int mrow = 0; mrow < kPitchRows; ++mrow)
        {
            // mrow 0 = top = semitone 7, mrow 7 = bottom = semitone 0
            const int semi      = (kPitchRows - 1) - mrow;
            const bool isActive = (semi == stepPitch);
            const float mx = float(cx) + 3.0f;
            const float my = float(r.y) + float(mrow) * float(r.h) / float(kPitchRows) + 1.0f;
            const float mw = float(cw) - 6.0f;
            const float mh = float(r.h) / float(kPitchRows) - 2.0f;
            const auto  cell = juce::Rectangle<float>(mx, my, mw, mh);

            if (isActive)
            {
                g.setColour(colPlay ? Palette::accent()
                                    : Palette::accent().withAlpha(0.75f));
                g.fillRoundedRectangle(cell, cornerR);
            }

            // Highlight intersection of selected row×column (pitch row is special: entire row)
            const bool intersect = colSel && rowSel && isActive;
            g.setColour(intersect ? Palette::accent()
                        : isActive ? Palette::accent().withAlpha(0.5f)
                                   : Palette::dimOutline().withAlpha(0.35f));
            g.drawRoundedRectangle(cell, cornerR, intersect ? 1.5f : 0.75f);
        }

        // Column outline
        if (colPlay || colSel)
        {
            const auto colRect = juce::Rectangle<float>(
                float(cx) + 1.5f, float(r.y) + 1.5f,
                float(cw) - 3.0f, float(r.h) - 3.0f);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(0.5f));
            g.drawRoundedRectangle(colRect, cornerR, colPlay ? 2.0f : 1.5f);
        }
    }
}

void MatrixView::drawBarRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR = 3.0f;
    const int   cw      = colW();
    const bool  rowSel  = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);

    // Row label
    g.setColour(rowSel ? Palette::accent() : Palette::text());
    g.setFont(juce::Font(11.0f));
    g.drawFittedText(r.label,
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const bool colSel  = (col == selectedCol);
        const bool colPlay = (col == currentPlayStep);
        const bool hilight = colSel && rowSel;

        const float norm = getCellValue(r, col);
        const float cx = float(colX(col));
        const float trackPad = 5.0f;
        const auto  trackR = juce::Rectangle<float>(
            cx + trackPad, float(r.y) + trackPad,
            float(cw) - trackPad * 2.0f, float(r.h) - trackPad * 2.0f);

        // Track outline
        g.setColour(hilight ? Palette::accent()
                    : colSel ? Palette::accent().withAlpha(0.5f)
                             : Palette::dimOutline().withAlpha(0.4f));
        g.drawRoundedRectangle(trackR, cornerR, hilight ? 1.5f : 0.75f);

        // Filled bar (bottom → up)
        if (norm > 0.001f)
        {
            const float fillH = trackR.getHeight() * norm;
            const auto  fillR = trackR.withTop(trackR.getBottom() - fillH);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(hilight ? 0.85f : 0.6f));
            g.fillRoundedRectangle(fillR, cornerR);
        }

        // Value text
        g.setColour(Palette::dimOutline());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(juce::String(norm, 2), trackR.toNearestInt(),
                         juce::Justification::centredTop, 1);
    }
}

void MatrixView::drawSliderRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR = 3.0f;
    const int   cw      = colW();
    const bool  rowSel  = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);

    // Row label
    g.setColour(rowSel ? Palette::accent() : Palette::text());
    g.setFont(juce::Font(10.0f));
    g.drawFittedText(r.label,
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const bool colSel  = (col == selectedCol);
        const bool colPlay = (col == currentPlayStep);
        const bool hilight = colSel && rowSel;

        const float norm = getCellValue(r, col);
        const float cx = float(colX(col));
        const float vPad = float(r.h) * 0.25f;
        const auto  trackR = juce::Rectangle<float>(
            cx + 4.0f, float(r.y) + vPad,
            float(cw) - 8.0f, float(r.h) - vPad * 2.0f);

        // Track outline
        g.setColour(hilight ? Palette::accent()
                    : colSel ? Palette::accent().withAlpha(0.5f)
                             : Palette::dimOutline().withAlpha(0.4f));
        g.drawRoundedRectangle(trackR, cornerR, hilight ? 1.5f : 0.75f);

        // Filled portion (left → right)
        if (norm > 0.001f)
        {
            const auto fillR = trackR.withWidth(trackR.getWidth() * norm);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(hilight ? 0.85f : 0.5f));
            g.fillRoundedRectangle(fillR, cornerR);
        }

        // Value label
        const float raw = rawFromNorm(r, norm);
        juce::String valStr;
        if      (r.type == RowType::AttackSlider)   valStr = juce::String(int(raw)) + "ms";
        else if (r.type == RowType::DecaySlider)    valStr = juce::String(int(raw)) + "ms";
        else if (r.type == RowType::DurationSlider) valStr = juce::String(raw, 2) + " beats";

        g.setColour(Palette::dimOutline());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(valStr, trackR.toNearestInt(),
                         juce::Justification::centred, 1);
    }

    // Row separator
    g.setColour(Palette::dimOutline().withAlpha(0.2f));
    g.drawHorizontalLine(r.y + r.h - 1, float(kLabelW), float(getWidth()));
}

//==============================================================================
void MatrixView::drawChildPitchRow(juce::Graphics& g, const RowInfo& r) const
{
    // Row label
    g.setColour(Palette::dimOutline());
    g.setFont(juce::Font(9.0f));
    g.drawFittedText("child",
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    const int cw = colW();
    const int rootNote = state.globalRootNote.load(std::memory_order_relaxed);
    const int scaleIdx = state.globalScale   .load(std::memory_order_relaxed);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const int cx = colX(col);

        if (!columnExpanded[size_t(col)])
        {
            // Collapsed: show a faint "+" hint centred in the cell
            g.setColour(Palette::dimOutline().withAlpha(0.2f));
            g.setFont(juce::Font(10.0f));
            g.drawFittedText("+", juce::Rectangle<int>(cx, r.y, cw, r.h),
                             juce::Justification::centred, 1);
            continue;
        }

        // ---- Expanded: step-count selector ----------------------------------
        const int stepCount = juce::jlimit(1, NUM_STEPS,
            state.childStepCount[col].load(std::memory_order_relaxed));
        const bool childOn = state.childEnabled[col].load(std::memory_order_relaxed);

        const auto countR = juce::Rectangle<int>(cx + 2, r.y + 2, cw - 4, kChildCountH - 2);
        g.setColour(childOn ? Palette::accent().withAlpha(0.25f)
                            : Palette::dimOutline().withAlpha(0.15f));
        g.fillRoundedRectangle(countR.toFloat(), 2.0f);
        g.setColour(childOn ? Palette::accent() : Palette::dimOutline());
        g.drawRoundedRectangle(countR.toFloat(), 2.0f, 0.75f);
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(juce::String(stepCount) + " steps",
                         countR, juce::Justification::centred, 1);

        // ---- Child pitch mini-grid ------------------------------------------
        const int gridY    = r.y + kChildCountH + 2;
        const int gridH    = r.h - kChildCountH - 4;
        const int cellW    = cw / stepCount;
        const int miniH    = gridH / kPitchRows;
        const float cornerR = 2.0f;

        for (int cs = 0; cs < stepCount; ++cs)
        {
            const int childPitch = state.childPitch[col][cs].load(std::memory_order_relaxed);
            const int ccx = cx + cs * cellW;

            for (int mrow = 0; mrow < kPitchRows; ++mrow)
            {
                // mrow 0 = top = semitone (kPitchRows-1), mrow (kPitchRows-1) = semitone 0
                const int semi     = (kPitchRows - 1) - mrow;
                const bool isActive = (semi == childPitch);

                // Scale membership for dimming non-scale tones
                const bool inScale = ScaleTable::isInScale(
                    (semi + rootNote) % 12, scaleIdx, rootNote % 12);

                const auto cell = juce::Rectangle<float>(
                    float(ccx) + 1.5f,
                    float(gridY) + float(mrow * miniH) + 0.5f,
                    float(cellW) - 3.0f,
                    float(miniH) - 1.0f);

                // Fill active note
                if (isActive)
                {
                    g.setColour(Palette::accent().withAlpha(childOn ? 0.85f : 0.55f));
                    g.fillRoundedRectangle(cell, cornerR);
                }

                // Outline — in-scale notes brighter
                const float outlineAlpha = isActive ? 1.0f
                                         : inScale  ? 0.35f
                                                    : 0.15f;
                g.setColour(isActive ? Palette::accent()
                                     : Palette::dimOutline().withAlpha(outlineAlpha));
                g.drawRoundedRectangle(cell, cornerR, isActive ? 1.0f : 0.5f);
            }

            // Vertical divider between child steps
            if (cs > 0)
            {
                g.setColour(Palette::dimOutline().withAlpha(0.25f));
                g.drawVerticalLine(ccx, float(gridY), float(gridY + gridH));
            }
        }
    }

    // Row separator
    g.setColour(Palette::dimOutline().withAlpha(0.2f));
    g.drawHorizontalLine(r.y + r.h - 1, float(kLabelW), float(getWidth()));
}
