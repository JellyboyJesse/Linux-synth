#include "MainComponent.h"

MainComponent::MainComponent()
{
    juce::LookAndFeel::setDefaultLookAndFeel(&wireframeLAF);

    // Transport bar
    addAndMakeVisible(transportBar);
    transportBar.onPlayStateChanged = [this](bool isPlaying)
    {
        if (!isPlaying)
        {
            lastReportedStep = -1;
            matrixView.setCurrentPlayStep(-1);
        }
    };

    // Matrix view inside a viewport
    matrixViewport.setViewedComponent(&matrixView, false);
    matrixViewport.setScrollBarsShown(true, false);
    matrixViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible(matrixViewport);

    matrixView.onStepSelected = [](int /*step*/) {};

    // Waveform display
    addAndMakeVisible(waveformDisplay);

    setupAudio();
    startTimerHz(33);

    setSize(940, 700);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback(&audioEngine);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

//==============================================================================
void MainComponent::setupAudio()
{
    const juce::String err = deviceManager.initialise(0, 2, nullptr, true);
    if (err.isNotEmpty())
        juce::Logger::writeToLog("Audio init error: " + err);

    deviceManager.addAudioCallback(&audioEngine);
}

//==============================================================================
void MainComponent::timerCallback()
{
    const int step = synthState.currentPlayStep.load(std::memory_order_relaxed);
    if (step != lastReportedStep)
    {
        lastReportedStep = step;
        matrixView.setCurrentPlayStep(step);
    }
}

//==============================================================================
void MainComponent::resized()
{
    static constexpr int kPad        = 8;
    static constexpr int kTransportH = 52;
    static constexpr int kWaveH      = 80;

    const int w = getWidth();
    const int h = getHeight();

    transportBar.setBounds(kPad, kPad, w - 2 * kPad, kTransportH);

    const int matrixY = kPad + kTransportH + kPad;
    const int waveY   = h - kWaveH - kPad;
    const int matrixH = waveY - matrixY - kPad;

    // Size the actual MatrixView to its preferred height (may exceed viewport)
    const int prefH = matrixView.getPreferredHeight();
    matrixView.setSize(w - 2 * kPad, juce::jmax(prefH, matrixH));
    matrixViewport.setBounds(kPad, matrixY, w - 2 * kPad, matrixH);

    waveformDisplay.setBounds(kPad, waveY, w - 2 * kPad, kWaveH);
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());
}
