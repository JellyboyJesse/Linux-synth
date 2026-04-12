#include "MainComponent.h"

MainComponent::MainComponent()
{
    juce::LookAndFeel::setDefaultLookAndFeel(&wireframeLAF);

    // ---- Transport ----------------------------------------------------------
    addAndMakeVisible(transportBar);
    transportBar.onPlayStateChanged = [this](bool isPlaying)
    {
        if (!isPlaying)
        {
            lastReportedStep = -1;
            matrixView .setCurrentPlayStep(-1);
            effectsView.setCurrentPlayStep(-1);
        }
    };

    // ---- Tab buttons --------------------------------------------------------
    tabNotes.setClickingTogglesState(true);
    tabNotes.setRadioGroupId(1);
    tabNotes.setToggleState(true, juce::dontSendNotification);
    tabNotes.onClick = [this] { showTab(false); };
    addAndMakeVisible(tabNotes);

    tabEffects.setClickingTogglesState(true);
    tabEffects.setRadioGroupId(1);
    tabEffects.onClick = [this] { showTab(true); };
    addAndMakeVisible(tabEffects);

    // ---- Notes (MatrixView) viewport ----------------------------------------
    matrixViewport.setViewedComponent(&matrixView, false);
    matrixViewport.setScrollBarsShown(true, false);
    matrixViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    matrixView.onStepSelected = [](int) {};
    addAndMakeVisible(matrixViewport);

    // ---- Effects viewport ---------------------------------------------------
    effectsViewport.setViewedComponent(&effectsView, false);
    effectsViewport.setScrollBarsShown(true, false);
    effectsViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    effectsView.onStepSelected = [](int) {};
    effectsViewport.setVisible(false);
    addAndMakeVisible(effectsViewport);

    // ---- Waveform -----------------------------------------------------------
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
void MainComponent::showTab(bool effects)
{
    showingEffects = effects;
    tabNotes  .setToggleState(!effects, juce::dontSendNotification);
    tabEffects.setToggleState( effects, juce::dontSendNotification);
    matrixViewport .setVisible(!effects);
    effectsViewport.setVisible( effects);
}

//==============================================================================
void MainComponent::timerCallback()
{
    const int step = synthState.currentPlayStep.load(std::memory_order_relaxed);
    if (step != lastReportedStep)
    {
        lastReportedStep = step;
        matrixView .setCurrentPlayStep(step);
        effectsView.setCurrentPlayStep(step);
    }
}

//==============================================================================
void MainComponent::resized()
{
    static constexpr int kPad        = 8;
    static constexpr int kTransportH = 52;
    static constexpr int kTabH       = 36;
    static constexpr int kWaveH      = 80;

    const int w = getWidth();
    const int h = getHeight();

    transportBar.setBounds(kPad, kPad, w - 2 * kPad, kTransportH);

    const int tabY  = kPad + kTransportH + kPad;
    const int tabW  = 100;
    tabNotes  .setBounds(kPad,          tabY, tabW, kTabH);
    tabEffects.setBounds(kPad + tabW + 4, tabY, tabW, kTabH);

    const int contentY = tabY + kTabH + kPad;
    const int waveY    = h - kWaveH - kPad;
    const int contentH = waveY - contentY - kPad;

    // Size both views to their natural preferred height (viewport clips/scrolls)
    const int notesW   = w - 2 * kPad;
    matrixView .setSize(notesW, juce::jmax(matrixView .getPreferredHeight(), contentH));
    effectsView.setSize(notesW, juce::jmax(effectsView.getPreferredHeight(), contentH));

    matrixViewport .setBounds(kPad, contentY, notesW, contentH);
    effectsViewport.setBounds(kPad, contentY, notesW, contentH);

    waveformDisplay.setBounds(kPad, waveY, w - 2 * kPad, kWaveH);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());
}
