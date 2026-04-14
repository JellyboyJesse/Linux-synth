#include "EffectsView.h"
#include "WireframeLookAndFeel.h"

EffectsView::EffectsView(SynthSharedState& sharedState)
    : state(sharedState)
{
    buildRows();
}

//==============================================================================
void EffectsView::buildRows()
{
    rows.clear();
    int y = kColHeaderH;

    auto push = [&](RowType t, const char* lbl, int param, int h)
    {
        rows.push_back({ t, juce::String(lbl), param, y, h });
        y += h;
    };

    push(RowType::SectionHeader, "harmonic stretch",  0, kSectionH);
    push(RowType::Slider,        "stretch ratio",     0, kSliderH);

    push(RowType::SectionHeader, "frequency shift",   0, kSectionH);
    push(RowType::Slider,        "freq shift",        1, kSliderH);

    push(RowType::SectionHeader, "waveshaping",       0, kSectionH);
    push(RowType::Slider,        "fold amount",       2, kSliderH);

    push(RowType::SectionHeader, "granular",          0, kSectionH);
    push(RowType::Slider,        "gran mix",          3, kSliderH);
    push(RowType::Slider,        "grain size",        4, kSliderH);
    push(RowType::Slider,        "density",           5, kSliderH);
    push(RowType::Slider,        "pitch scatter",     6, kSliderH);
    push(RowType::Slider,        "feedback",          7, kSliderH);

    push(RowType::SectionHeader, "shimmer reverb",    0, kSectionH);
    push(RowType::Slider,        "reverb size",       8, kSliderH);
    push(RowType::Slider,        "reverb damp",       9, kSliderH);
    push(RowType::Slider,        "shimmer amt",      10, kSliderH);
    push(RowType::Slider,        "shimmer tune",     11, kSliderH);
}

int EffectsView::getPreferredHeight() const
{
    if (rows.empty()) return kColHeaderH;
    return rows.back().y + rows.back().h;
}

void EffectsView::setCurrentPlayStep(int step)
{
    if (currentPlayStep != step) { currentPlayStep = step; repaint(); }
}

//==============================================================================
// Geometry
//==============================================================================
int EffectsView::colW() const
{
    return (getWidth() - kLabelW) / NUM_STEPS;
}

int EffectsView::colX(int col) const
{
    return kLabelW + col * colW();
}

int EffectsView::colFromX(int x) const
{
    if (x < kLabelW) return -1;
    const int cw = colW();
    if (cw <= 0) return -1;
    const int c = (x - kLabelW) / cw;
    return (c >= 0 && c < NUM_STEPS) ? c : -1;
}

int EffectsView::rowFromY(int y) const
{
    if (y < kColHeaderH) return -1;
    for (int i = 0; i < int(rows.size()); ++i)
        if (y >= rows[i].y && y < rows[i].y + rows[i].h)
            return i;
    return -1;
}

//==============================================================================
// Parameter value helpers
//
// paramId: 0=stretch(0.5-2.0) 1=freqShift(-200-+200) 2=foldAmount(0-1)
//          3=granMix(0-1) 4=granSize(10-500ms) 5=density(1-40/s)
//          6=pitchScatter(0-1) 7=feedback(0-0.95)
//          8=reverbSize(0-1) 9=reverbDamp(0-1)
//          10=shimmerAmt(0-1) 11=shimmerTune(-1-1)
//==============================================================================
float EffectsView::normToRaw(int paramId, float norm) const
{
    const float n = juce::jlimit(0.0f, 1.0f, norm);
    switch (paramId)
    {
        case 0:  return 0.5f + n * 1.5f;         // stretch   0.5–2.0
        case 1:  return n * 400.0f - 200.0f;     // freqShift -200–+200
        case 2:  return n;                        // foldAmount 0–1
        case 3:  return n;                        // granMix    0–1
        case 4:  return 10.0f + n * 490.0f;      // granSize  10–500 ms
        case 5:  return 1.0f + n * 39.0f;        // density    1–40/s
        case 6:  return n;                        // pitchScatter 0–1
        case 7:  return n * 0.95f;               // feedback   0–0.95
        case 8:  return n;                        // reverbSize 0–1
        case 9:  return n;                        // reverbDamp 0–1
        case 10: return n;                        // shimmerAmt 0–1
        case 11: return n * 2.0f - 1.0f;         // shimmerTune -1–1
        default: return 0.0f;
    }
}

float EffectsView::rawToNorm(int paramId, float raw) const
{
    switch (paramId)
    {
        case 0:  return juce::jlimit(0.0f, 1.0f, (raw - 0.5f) / 1.5f);
        case 1:  return juce::jlimit(0.0f, 1.0f, (raw + 200.0f) / 400.0f);
        case 2:  return juce::jlimit(0.0f, 1.0f, raw);
        case 3:  return juce::jlimit(0.0f, 1.0f, raw);
        case 4:  return juce::jlimit(0.0f, 1.0f, (raw - 10.0f) / 490.0f);
        case 5:  return juce::jlimit(0.0f, 1.0f, (raw - 1.0f) / 39.0f);
        case 6:  return juce::jlimit(0.0f, 1.0f, raw);
        case 7:  return juce::jlimit(0.0f, 1.0f, raw / 0.95f);
        case 8:  return juce::jlimit(0.0f, 1.0f, raw);
        case 9:  return juce::jlimit(0.0f, 1.0f, raw);
        case 10: return juce::jlimit(0.0f, 1.0f, raw);
        case 11: return juce::jlimit(0.0f, 1.0f, (raw + 1.0f) / 2.0f);
        default: return 0.0f;
    }
}

float EffectsView::getRaw(int paramId, int step) const
{
    switch (paramId)
    {
        case 0:  return state.stretchRatio[step]        .load(std::memory_order_relaxed);
        case 1:  return state.freqShift[step]           .load(std::memory_order_relaxed);
        case 2:  return state.foldAmount[step]          .load(std::memory_order_relaxed);
        case 3:  return state.granularMix[step]         .load(std::memory_order_relaxed);
        case 4:  return state.granularSize[step]        .load(std::memory_order_relaxed);
        case 5:  return state.granularDensity[step]     .load(std::memory_order_relaxed);
        case 6:  return state.granularPitchScatter[step].load(std::memory_order_relaxed);
        case 7:  return state.granularFeedback[step]    .load(std::memory_order_relaxed);
        case 8:  return state.reverbSize[step]          .load(std::memory_order_relaxed);
        case 9:  return state.reverbDamp[step]          .load(std::memory_order_relaxed);
        case 10: return state.shimmerAmount[step]       .load(std::memory_order_relaxed);
        case 11: return state.shimmerTune[step]         .load(std::memory_order_relaxed);
        default: return 0.0f;
    }
}

void EffectsView::setRaw(int paramId, int step, float raw)
{
    switch (paramId)
    {
        case 0:  state.stretchRatio[step]        .store(raw, std::memory_order_relaxed); break;
        case 1:  state.freqShift[step]           .store(raw, std::memory_order_relaxed); break;
        case 2:  state.foldAmount[step]          .store(raw, std::memory_order_relaxed); break;
        case 3:  state.granularMix[step]         .store(raw, std::memory_order_relaxed); break;
        case 4:  state.granularSize[step]        .store(raw, std::memory_order_relaxed); break;
        case 5:  state.granularDensity[step]     .store(raw, std::memory_order_relaxed); break;
        case 6:  state.granularPitchScatter[step].store(raw, std::memory_order_relaxed); break;
        case 7:  state.granularFeedback[step]    .store(raw, std::memory_order_relaxed); break;
        case 8:  state.reverbSize[step]          .store(raw, std::memory_order_relaxed); break;
        case 9:  state.reverbDamp[step]          .store(raw, std::memory_order_relaxed); break;
        case 10: state.shimmerAmount[step]       .store(raw, std::memory_order_relaxed); break;
        case 11: state.shimmerTune[step]         .store(raw, std::memory_order_relaxed); break;
        default: break;
    }
}

juce::String EffectsView::formatVal(int paramId, float raw) const
{
    switch (paramId)
    {
        case 0: return juce::String(raw, 2) + "\xc3\x97"; // ×
        case 1:
        {
            const int hz = int(raw);
            return (hz >= 0 ? "+" : "") + juce::String(hz) + " Hz";
        }
        case 2:  return juce::String(int(raw * 100.0f)) + "%";
        case 3:  return juce::String(int(raw * 100.0f)) + "%";
        case 4:  return juce::String(int(raw)) + " ms";
        case 5:  return juce::String(int(raw)) + "/s";
        case 6:  return juce::String(int(raw * 100.0f)) + "%";
        case 7:  return juce::String(int(raw * 100.0f)) + "%";
        case 8:  return juce::String(int(raw * 100.0f)) + "%";
        case 9:  return juce::String(raw, 2);
        case 10: return juce::String(int(raw * 100.0f)) + "%";
        case 11: return juce::String(raw, 2);
        default: return {};
    }
}

//==============================================================================
// Mouse handling
//==============================================================================
void EffectsView::mouseDown(const juce::MouseEvent& e)
{
    drag.active = false;

    const int col = colFromX(e.x);

    // Column header click
    if (e.y < kColHeaderH && col >= 0)
    {
        selectedCol = col;
        if (onStepSelected) onStepSelected(col);
        repaint();
        return;
    }

    // Row label click
    if (e.x < kLabelW)
    {
        selectedRowIndex = rowFromY(e.y);
        repaint();
        return;
    }

    if (col < 0) return;

    const int ri = rowFromY(e.y);
    if (ri < 0) return;
    const RowInfo& row = rows[size_t(ri)];

    selectedCol      = col;
    selectedRowIndex = ri;
    if (onStepSelected) onStepSelected(col);

    if (row.type == RowType::SectionHeader) { repaint(); return; }

    // Start drag
    drag.active    = true;
    drag.rowIndex  = ri;
    drag.col       = col;
    drag.startX    = e.x;
    drag.startNorm = rawToNorm(row.paramId, getRaw(row.paramId, col));
    repaint();
}

void EffectsView::mouseDrag(const juce::MouseEvent& e)
{
    if (!drag.active || drag.rowIndex < 0) return;
    const RowInfo& row = rows[size_t(drag.rowIndex)];

    const float trackW = float(juce::jmax(1, colW() - 8));
    const float delta  = float(e.x - drag.startX) / trackW;
    const float newNorm = juce::jlimit(0.0f, 1.0f, drag.startNorm + delta);
    setRaw(row.paramId, drag.col, normToRaw(row.paramId, newNorm));
    repaint();
}

//==============================================================================
// Drawing
//==============================================================================
void EffectsView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());
    drawColumnHeaders(g);

    for (int i = 0; i < int(rows.size()); ++i)
    {
        const RowInfo& row = rows[size_t(i)];
        if (row.type == RowType::SectionHeader)
            drawSectionHeader(g, row);
        else
            drawSliderRow(g, row);
    }
}

void EffectsView::drawColumnHeaders(juce::Graphics& g) const
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

        g.setColour(isPlay ? Palette::background() : Palette::text());
        g.drawFittedText("Step " + juce::String(col + 1),
                         hb.toNearestInt(), juce::Justification::centred, 1);
    }

    g.setColour(Palette::dimOutline().withAlpha(0.4f));
    g.drawHorizontalLine(kColHeaderH, float(kLabelW), float(getWidth()));
}

void EffectsView::drawSectionHeader(juce::Graphics& g, const RowInfo& r) const
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

void EffectsView::drawSliderRow(juce::Graphics& g, const RowInfo& r) const
{
    const float cornerR = 3.0f;
    const int   cw      = colW();
    const bool  rowSel  = (selectedRowIndex >= 0
                           && rows[size_t(selectedRowIndex)].y == r.y);

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

        const float raw  = getRaw(r.paramId, col);
        const float norm = rawToNorm(r.paramId, raw);

        const float vPad  = float(r.h) * 0.25f;
        const auto  trackR = juce::Rectangle<float>(
            float(colX(col)) + 4.0f, float(r.y) + vPad,
            float(cw) - 8.0f, float(r.h) - vPad * 2.0f);

        // Track outline
        g.setColour(hilight ? Palette::accent()
                    : colSel ? Palette::accent().withAlpha(0.5f)
                             : Palette::dimOutline().withAlpha(0.4f));
        g.drawRoundedRectangle(trackR, cornerR, hilight ? 1.5f : 0.75f);

        // Filled portion
        if (norm > 0.001f)
        {
            const auto fillR = trackR.withWidth(trackR.getWidth() * norm);
            g.setColour(colPlay ? Palette::accent()
                                : Palette::accent().withAlpha(hilight ? 0.85f : 0.5f));
            g.fillRoundedRectangle(fillR, cornerR);
        }

        // Value label
        g.setColour(Palette::dimOutline());
        g.setFont(juce::Font(9.0f));
        g.drawFittedText(formatVal(r.paramId, raw),
                         trackR.toNearestInt(), juce::Justification::centred, 1);
    }

    // Row separator
    g.setColour(Palette::dimOutline().withAlpha(0.2f));
    g.drawHorizontalLine(r.y + r.h - 1, float(kLabelW), float(getWidth()));
}
