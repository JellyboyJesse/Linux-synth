#include "HarmonicEditor.h"
#include "WireframeLookAndFeel.h"
#include <cmath>

static constexpr float kTwoPiH = 6.283185307179586f;

HarmonicEditor::HarmonicEditor(SynthSharedState& sharedState)
    : state(sharedState)
{
    addAndMakeVisible(customToggle);

    customToggle.setButtonText("Custom");
    customToggle.setToggleState(false, juce::dontSendNotification);

    customToggle.onClick = [this]
    {
        if (selectedStep < 0) return; // global preset — no toggle meaning

        const bool nowCustom = customToggle.getToggleState();
        state.stepIsCustom[selectedStep].store(nowCustom, std::memory_order_relaxed);

        // Initialise step amplitudes from global if switching to custom for first time
        if (nowCustom)
        {
            for (int i = 0; i < NUM_OSCILLATORS; ++i)
            {
                const float cur = state.stepAmplitudes[selectedStep][i]
                                       .load(std::memory_order_relaxed);
                // Only init if still at default zero (not previously edited)
                if (cur == 0.0f && i == 0)
                {
                    const float glob = state.globalAmplitudes[i].load(std::memory_order_relaxed);
                    state.stepAmplitudes[selectedStep][i].store(glob, std::memory_order_relaxed);
                }
            }
        }
        if (onStateChanged) onStateChanged();
        repaint();
    };

    startTimerHz(33); // ~30 fps waveform refresh
}

HarmonicEditor::~HarmonicEditor()
{
    stopTimer();
}

//==============================================================================
void HarmonicEditor::setSelectedStep(int step)
{
    selectedStep = step;
    updateToggleFromState();
    repaint();
}

void HarmonicEditor::updateToggleFromState()
{
    if (selectedStep < 0)
    {
        customToggle.setVisible(false);
        return;
    }
    customToggle.setVisible(true);
    const bool isCustom = state.stepIsCustom[selectedStep].load(std::memory_order_relaxed);
    customToggle.setToggleState(isCustom, juce::dontSendNotification);
}

//==============================================================================
void HarmonicEditor::resized()
{
    const int toggleH = 32;
    const int waveH   = 80;
    const int pad     = 8;
    const int h       = getHeight();
    const int w       = getWidth();

    toggleArea = { pad, h - toggleH - pad, w - 2 * pad, toggleH };
    waveArea   = { pad, h - toggleH - pad - waveH - pad,
                   w - 2 * pad, waveH };
    barsArea   = { pad, pad,
                   w - 2 * pad, waveArea.getY() - 2 * pad };

    customToggle.setBounds(toggleArea);
}

//==============================================================================
bool HarmonicEditor::isEditable() const
{
    if (selectedStep < 0) return true; // global preset always editable
    return state.stepIsCustom[selectedStep].load(std::memory_order_relaxed);
}

float HarmonicEditor::getAmplitude(int barIndex) const
{
    if (selectedStep < 0)
        return state.globalAmplitudes[barIndex].load(std::memory_order_relaxed);

    const bool custom = state.stepIsCustom[selectedStep].load(std::memory_order_relaxed);
    if (custom)
        return state.stepAmplitudes[selectedStep][barIndex].load(std::memory_order_relaxed);
    else
        return state.globalAmplitudes[barIndex].load(std::memory_order_relaxed);
}

void HarmonicEditor::setAmplitude(int barIndex, float amp)
{
    amp = juce::jlimit(0.0f, 1.0f, amp);
    if (selectedStep < 0)
    {
        state.globalAmplitudes[barIndex].store(amp, std::memory_order_relaxed);
    }
    else if (state.stepIsCustom[selectedStep].load(std::memory_order_relaxed))
    {
        state.stepAmplitudes[selectedStep][barIndex].store(amp, std::memory_order_relaxed);
    }
}

//==============================================================================
int HarmonicEditor::barIndexFromX(int x) const
{
    if (!barsArea.contains(x, barsArea.getCentreY())) return -1;
    const int barW = barsArea.getWidth() / NUM_OSCILLATORS;
    const int idx  = (x - barsArea.getX()) / (barW > 0 ? barW : 1);
    return (idx >= 0 && idx < NUM_OSCILLATORS) ? idx : -1;
}

//==============================================================================
void HarmonicEditor::mouseDown(const juce::MouseEvent& e)
{
    dragBarIndex = barIndexFromX(e.x);
    if (dragBarIndex < 0 || !isEditable()) { dragBarIndex = -1; return; }
    dragStartY   = e.y;
    dragStartAmp = getAmplitude(dragBarIndex);
}

void HarmonicEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragBarIndex < 0) return;
    const float dragRange = float(barsArea.getHeight());
    const float delta     = float(dragStartY - e.y) / (dragRange > 0.0f ? dragRange : 1.0f);
    setAmplitude(dragBarIndex, dragStartAmp + delta);
    repaint();
}

//==============================================================================
void HarmonicEditor::timerCallback()
{
    // Refresh live amplitudes for waveform display
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        displayAmps[i] = state.liveAmplitudes[i].load(std::memory_order_relaxed);
    repaint(waveArea);
}

//==============================================================================
void HarmonicEditor::drawBars(juce::Graphics& g) const
{
    const int   barW    = barsArea.getWidth() / NUM_OSCILLATORS;
    const int   bH      = barsArea.getHeight();
    const float cornerR = 4.0f;
    const float pad     = 4.0f;

    g.setFont(juce::Font(11.0f));

    for (int i = 0; i < NUM_OSCILLATORS; ++i)
    {
        const float amp     = getAmplitude(i);
        const auto  barRect = juce::Rectangle<float>(
            float(barsArea.getX() + i * barW) + pad,
            float(barsArea.getY()),
            float(barW) - pad * 2.0f,
            float(bH));

        // Track outline
        g.setColour(Palette::dimOutline().withAlpha(0.4f));
        g.drawRoundedRectangle(barRect, cornerR, 1.0f);

        // Filled portion (from bottom)
        if (amp > 0.0f)
        {
            const float fillH   = barRect.getHeight() * amp;
            const auto  fillR   = barRect.withTop(barRect.getBottom() - fillH);
            const bool  editable = isEditable();
            g.setColour(editable ? Palette::accent().withAlpha(0.7f)
                                 : Palette::dimOutline().withAlpha(0.5f));
            g.fillRoundedRectangle(fillR, cornerR);
            g.setColour(editable ? Palette::accent() : Palette::dimOutline());
            g.drawRoundedRectangle(fillR, cornerR, 1.0f);
        }

        // Bar label: "H1" … "H4"
        g.setColour(Palette::text());
        g.drawFittedText("H" + juce::String(i + 1),
                         juce::Rectangle<int>(int(barRect.getX()),
                                             barsArea.getBottom() - 18,
                                             int(barRect.getWidth()), 16),
                         juce::Justification::centred, 1);

        // Amplitude value above bar
        g.setColour(Palette::dimOutline());
        g.drawFittedText(juce::String(amp, 2),
                         juce::Rectangle<int>(int(barRect.getX()),
                                             barsArea.getY() + 2,
                                             int(barRect.getWidth()), 14),
                         juce::Justification::centred, 1);
    }
}

void HarmonicEditor::drawWaveform(juce::Graphics& g) const
{
    const auto bounds = waveArea.toFloat();
    const float cornerR = 4.0f;

    // Outline box
    g.setColour(Palette::dimOutline().withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);

    // Build one-period composite path
    const int   pts  = 256;
    const float midY = bounds.getCentreY();
    const float scaleY = bounds.getHeight() * 0.42f; // 84% of half-height

    juce::Path path;
    for (int i = 0; i <= pts; ++i)
    {
        const float phase = float(i) / float(pts);  // 0..1
        float y = 0.0f;
        for (int h = 0; h < NUM_OSCILLATORS; ++h)
            y += displayAmps[h] * std::sin(kTwoPiH * float(h + 1) * phase);

        const float px = bounds.getX() + phase * bounds.getWidth();
        const float py = midY - y * scaleY;
        if (i == 0) path.startNewSubPath(px, py);
        else        path.lineTo(px, py);
    }

    // Draw path — stroked, no fill
    g.setColour(Palette::accent());
    g.strokePath(path, juce::PathStrokeType(1.5f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // Label
    g.setColour(Palette::dimOutline());
    g.setFont(juce::Font(10.0f));
    g.drawText("Waveform", bounds.reduced(4.0f, 2.0f),
               juce::Justification::topLeft, false);
}

//==============================================================================
void HarmonicEditor::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());

    // Panel title
    g.setColour(Palette::text());
    g.setFont(juce::Font(13.0f));
    const juce::String title = (selectedStep < 0)
        ? "Global preset"
        : ("Step " + juce::String(selectedStep + 1)
           + (state.stepIsCustom[selectedStep].load() ? " — custom" : " — inherited"));
    g.drawFittedText(title, 8, 0, getWidth() - 16, 20,
                     juce::Justification::centredLeft, 1);

    drawBars(g);
    drawWaveform(g);

    // Outer border
    g.setColour(Palette::outline());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.5f);
}
