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

    // Expand toggle in column header — 20×20 hit area centred on the visual toggle
    if (e.y < kColHeaderH && col >= 0)
    {
        // Centre of the visual toggle (matches drawColumnHeaders)
        const int hbRight  = colX(col) + colW() - 2;   // hb.getRight()
        const int toggleCX = hbRight - kToggleSize / 2;
        const int toggleCY = kColHeaderH / 2;
        const int half     = kToggleHitSize / 2;
        if (e.x >= toggleCX - half && e.x < toggleCX + half
            && e.y >= toggleCY - half && e.y < toggleCY + half)
        {
            columnExpanded[size_t(col)] = !columnExpanded[size_t(col)];
            if (!columnExpanded[size_t(col)])
                state.childEnabled[col].store(false, std::memory_order_relaxed);
            resized();
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

    if (row.type == RowType::SectionHeader)
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

        // Step count +/- strip (top kChildCountH px of the cell)
        if (e.y >= row.y && e.y < row.y + kChildCountH)
        {
            const int cur  = state.childStepCount[col].load(std::memory_order_relaxed);
            const int btnW = juce::jmax(1, cw / 3);
            const int localX = e.x - cx;
            if (localX < btnW)
                state.childStepCount[col].store(juce::jmax(1, cur - 1),
                                                std::memory_order_relaxed);
            else if (localX >= cw - btnW)
                state.childStepCount[col].store(juce::jmin(NUM_STEPS, cur + 1),
                                                std::memory_order_relaxed);
            repaint();
            return;
        }

        // Child pitch mini-grid (below the step-count strip)
        const int gridY     = row.y + kChildCountH + 2;
        const int gridH     = row.h - kChildCountH - 4;
        const int stepCount = juce::jlimit(1, NUM_STEPS,
            state.childStepCount[col].load(std::memory_order_relaxed));
        const int childCellW = juce::jmax(1, cw / stepCount);

        if (e.x >= cx && e.x < cx + cw && e.y >= gridY && e.y < gridY + gridH)
        {
            const int cs = juce::jlimit(0, stepCount - 1, (e.x - cx) / childCellW);
            const int miniH = juce::jmax(1, gridH / kPitchRows);
            const int mrow  = juce::jlimit(0, kPitchRows - 1, (e.y - gridY) / miniH);
            const int semi  = (kPitchRows - 1) - mrow;
            state.childPitch[col][cs].store(semi, std::memory_order_relaxed);
            state.childEnabled[col] .store(true,  std::memory_order_relaxed);
            repaint();
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
            g.setColour(Palette::accent().withAlpha(0.18f));
        else
            g.setColour(Palette::surface());
        g.fillRoundedRectangle(hb, cornerR);

        if (!isPlay)
        {
            g.setColour(Palette::border());
            g.drawRoundedRectangle(hb, cornerR, 0.5f);
        }

        // Shift label left to make room for toggle
        const auto labelR = hb.withTrimmedRight(float(kToggleSize + 2));
        g.setFont(isPlay ? juce::Font(11.0f, juce::Font::bold) : juce::Font(11.0f));
        g.setColour(isPlay ? Palette::dark() : (isSel ? Palette::dark() : Palette::mid()));
        g.drawFittedText("Step " + juce::String(col + 1),
                         labelR.toNearestInt(), juce::Justification::centred, 1);

        // +/- expand toggle
        const bool expanded = columnExpanded[size_t(col)];
        const auto toggleR = juce::Rectangle<float>(
            hb.getRight() - float(kToggleSize),
            hb.getCentreY() - float(kToggleSize) * 0.5f,
            float(kToggleSize), float(kToggleSize));
        g.setColour(expanded ? Palette::accent() : Palette::border());
        g.drawRoundedRectangle(toggleR, 2.0f, 0.75f);
        g.setColour(expanded ? Palette::dark() : Palette::mid());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(expanded ? "-" : "+",
                         toggleR.toNearestInt(), juce::Justification::centred, 1);
    }

    // Separator line
    g.setColour(Palette::border());
    g.drawHorizontalLine(kColHeaderH, float(kLabelW), float(getWidth()));
}

void MatrixView::drawSectionHeader(juce::Graphics& g, const RowInfo& r) const
{
    const float pillH = float(r.h) - 4.0f;
    const auto  pill  = juce::Rectangle<float>(4.0f, float(r.y) + 2.0f,
                                               float(getWidth()) - 8.0f, pillH);
    g.setColour(Palette::section());
    g.fillRoundedRectangle(pill, pillH * 0.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText(r.label.toUpperCase(),
                     pill.withLeft(pill.getX() + 12.0f).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void MatrixView::drawPitchRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR = 3.0f;
    const int   cw      = colW();

    // Row label
    const bool rowSel = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);
    g.setColour(rowSel ? Palette::accent() : Palette::mid());
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
            const int semi      = (kPitchRows - 1) - mrow;
            const bool isActive = (semi == stepPitch);
            const float mx = float(cx) + 2.0f;
            const float my = float(r.y) + float(mrow) * float(r.h) / float(kPitchRows) + 0.5f;
            const float mw = float(cw) - 4.0f;
            const float mh = float(r.h) / float(kPitchRows) - 1.0f;
            const auto  cell = juce::Rectangle<float>(mx, my, mw, mh);

            // Cell background
            g.setColour(isActive ? (colPlay ? Palette::accent()
                                            : Palette::accent().withAlpha(0.75f))
                                 : Palette::surface());
            g.fillRoundedRectangle(cell, cornerR);

            // Cell outline
            g.setColour(isActive ? Palette::accent()
                        : (colPlay || colSel) ? Palette::border()
                                              : Palette::border().withAlpha(0.5f));
            g.drawRoundedRectangle(cell, cornerR, isActive ? 1.0f : 0.5f);
        }

        // Column highlight ring for playing/selected
        if (colPlay || colSel)
        {
            const auto colRect = juce::Rectangle<float>(
                float(cx) + 1.0f, float(r.y) + 1.0f,
                float(cw) - 2.0f, float(r.h) - 2.0f);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(0.5f));
            g.drawRoundedRectangle(colRect, cornerR, colPlay ? 1.5f : 1.0f);
        }
    }
}

void MatrixView::drawBarRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR = 3.0f;
    const int   cw      = colW();
    const bool  rowSel  = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);

    // Row label
    g.setColour(rowSel ? Palette::accent() : Palette::mid());
    g.setFont(juce::Font(11.0f));
    g.drawFittedText(r.label,
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const bool colSel  = (col == selectedCol);
        const bool colPlay = (col == currentPlayStep);

        const float norm = getCellValue(r, col);
        const float cx   = float(colX(col));
        const float pad  = 4.0f;
        const auto  cellR = juce::Rectangle<float>(
            cx + pad, float(r.y) + pad,
            float(cw) - pad * 2.0f, float(r.h) - pad * 2.0f);

        // Cell background
        g.setColour(Palette::surface());
        g.fillRoundedRectangle(cellR, cornerR);

        // Accent fill from bottom
        if (norm > 0.001f)
        {
            const float fillH = cellR.getHeight() * norm;
            const auto  fillR = cellR.withTop(cellR.getBottom() - fillH);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(0.75f));
            g.fillRoundedRectangle(fillR, cornerR);
        }

        // Cell outline
        g.setColour((colPlay || colSel) ? Palette::accent().withAlpha(0.6f)
                                        : Palette::border());
        g.drawRoundedRectangle(cellR, cornerR, (colPlay || colSel) ? 1.0f : 0.5f);

        // Value text
        g.setColour(Palette::dark());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(juce::String(norm, 2), cellR.toNearestInt(),
                         juce::Justification::centredTop, 1);
    }
}

void MatrixView::drawSliderRow(juce::Graphics& g, const RowInfo& r) const
{
    const int  cw     = colW();
    const bool rowSel = (selectedRowIndex >= 0 && rows[size_t(selectedRowIndex)].y == r.y);

    // Row label
    g.setColour(rowSel ? Palette::accent() : Palette::mid());
    g.setFont(juce::Font(10.0f));
    g.drawFittedText(r.label,
                     juce::Rectangle<int>(0, r.y, kLabelW - 4, r.h),
                     juce::Justification::centredRight, 1);

    for (int col = 0; col < NUM_STEPS; ++col)
    {
        const bool colSel  = (col == selectedCol);
        const bool colPlay = (col == currentPlayStep);

        const float norm = getCellValue(r, col);
        const float cx   = float(colX(col));
        const float cy   = float(r.y) + float(r.h) * 0.5f;

        // Cell background
        g.setColour(colPlay ? Palette::accent().withAlpha(0.08f) : Palette::surface());
        g.fillRect(juce::Rectangle<float>(cx, float(r.y), float(cw), float(r.h)));

        // 3px centred track
        const float trackX = cx + 6.0f;
        const float trackW = float(cw) - 12.0f;
        const float trackH = 3.0f;
        const float trackY = cy - trackH * 0.5f;

        g.setColour(Palette::border());
        g.fillRoundedRectangle(trackX, trackY, trackW, trackH, trackH * 0.5f);

        if (norm > 0.001f)
        {
            g.setColour(Palette::accent());
            g.fillRoundedRectangle(trackX, trackY, trackW * norm, trackH, trackH * 0.5f);
        }

        // 8px dark thumb dot
        const float thumbR  = 4.0f;
        const float thumbCX = trackX + trackW * norm;
        g.setColour(Palette::dark());
        g.fillEllipse(thumbCX - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);

        // Value label above track
        const float raw = rawFromNorm(r, norm);
        juce::String valStr;
        if      (r.type == RowType::AttackSlider)   valStr = juce::String(int(raw)) + "ms";
        else if (r.type == RowType::DecaySlider)    valStr = juce::String(int(raw)) + "ms";
        else if (r.type == RowType::DurationSlider) valStr = juce::String(raw, 2) + "b";

        g.setColour(Palette::dark());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(valStr,
                         juce::Rectangle<float>(cx, float(r.y), float(cw),
                                                cy - trackH * 0.5f).toNearestInt(),
                         juce::Justification::centred, 1);
    }

    // Row separator
    g.setColour(Palette::border());
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

        // ---- Expanded: step-count selector with -/+ buttons ----------------
        const int stepCount = juce::jlimit(1, NUM_STEPS,
            state.childStepCount[col].load(std::memory_order_relaxed));
        const bool childOn = state.childEnabled[col].load(std::memory_order_relaxed);

        const int btnW   = juce::jmax(1, cw / 3);
        const int stripY = r.y + 2;
        const int stripH = kChildCountH - 2;

        // "-" button
        const auto minusBtnR = juce::Rectangle<int>(cx + 2, stripY, btnW - 2, stripH).toFloat();
        const bool canDec = stepCount > 1;
        g.setColour(canDec ? Palette::dimOutline().withAlpha(0.5f)
                           : Palette::dimOutline().withAlpha(0.2f));
        g.drawRoundedRectangle(minusBtnR, 2.0f, 0.75f);
        g.setColour(canDec ? Palette::text() : Palette::dimOutline().withAlpha(0.3f));
        g.setFont(juce::Font(9.0f));
        g.drawFittedText("-", minusBtnR.toNearestInt(), juce::Justification::centred, 1);

        // Count label in middle third
        const auto labelR = juce::Rectangle<int>(cx + btnW, stripY, cw - 2 * btnW, stripH);
        g.setColour(childOn ? Palette::accent().withAlpha(0.25f)
                            : Palette::dimOutline().withAlpha(0.12f));
        g.fillRoundedRectangle(labelR.toFloat(), 2.0f);
        g.setColour(childOn ? Palette::accent() : Palette::dimOutline());
        g.drawRoundedRectangle(labelR.toFloat(), 2.0f, 0.75f);
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(juce::String(stepCount),
                         labelR, juce::Justification::centred, 1);

        // "+" button
        const auto plusBtnR = juce::Rectangle<int>(cx + cw - btnW, stripY, btnW - 2, stripH).toFloat();
        const bool canInc = stepCount < NUM_STEPS;
        g.setColour(canInc ? Palette::dimOutline().withAlpha(0.5f)
                           : Palette::dimOutline().withAlpha(0.2f));
        g.drawRoundedRectangle(plusBtnR, 2.0f, 0.75f);
        g.setColour(canInc ? Palette::text() : Palette::dimOutline().withAlpha(0.3f));
        g.setFont(juce::Font(9.0f));
        g.drawFittedText("+", plusBtnR.toNearestInt(), juce::Justification::centred, 1);

        // ---- Child pitch mini-grid ------------------------------------------
        const int gridY    = r.y + kChildCountH + 2;
        const int gridH    = r.h - kChildCountH - 4;
        const int cellW    = juce::jmax(1, cw / stepCount);
        const int miniH    = juce::jmax(1, gridH / kPitchRows);
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
    g.setColour(Palette::border());
    g.drawHorizontalLine(r.y + r.h - 1, float(kLabelW), float(getWidth()));
}
