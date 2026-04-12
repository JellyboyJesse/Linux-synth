#include "WaveformDisplay.h"
#include "WireframeLookAndFeel.h"
#include <cmath>

static constexpr float kTwoPiW = 6.283185307179586f;

WaveformDisplay::WaveformDisplay(SynthSharedState& sharedState)
    : state(sharedState)
{
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        displayAmps[i] = state.liveAmplitudes[i].load(std::memory_order_relaxed);

    startTimerHz(33);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::timerCallback()
{
    for (int i = 0; i < NUM_OSCILLATORS; ++i)
        displayAmps[i] = state.liveAmplitudes[i].load(std::memory_order_relaxed);
    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    const auto  bounds  = getLocalBounds().toFloat().reduced(1.0f);
    const float cornerR = 4.0f;

    // Background + outline
    g.setColour(Palette::background());
    g.fillRoundedRectangle(bounds, cornerR);
    g.setColour(Palette::dimOutline().withAlpha(0.5f));
    g.drawRoundedRectangle(bounds, cornerR, 1.0f);

    // Composite waveform path — one full period
    static constexpr int kPts = 320;
    const float midY   = bounds.getCentreY();
    const float scaleY = bounds.getHeight() * 0.42f;

    juce::Path path;
    for (int i = 0; i <= kPts; ++i)
    {
        const float x = float(i) / float(kPts);
        float y = 0.0f;
        for (int h = 0; h < NUM_OSCILLATORS; ++h)
            y += displayAmps[h] * std::sin(kTwoPiW * float(h + 1) * x);

        const float px = bounds.getX() + x * bounds.getWidth();
        const float py = midY - y * scaleY;
        if (i == 0) path.startNewSubPath(px, py);
        else        path.lineTo(px, py);
    }

    g.setColour(Palette::accent());
    g.strokePath(path, juce::PathStrokeType(1.5f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // Label
    g.setColour(Palette::dimOutline());
    g.setFont(juce::Font(10.0f));
    g.drawText("Waveform", bounds.reduced(6.0f, 3.0f),
               juce::Justification::topLeft, false);
}
