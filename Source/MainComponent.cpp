#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Apply wireframe look and feel globally
    juce::LookAndFeel::setDefaultLookAndFeel(&wireframeLAF);

    // ---- Transport bar -------------------------------------------------------
    addAndMakeVisible(transportBar);

    transportBar.onPlayStateChanged = [this](bool isPlaying)
    {
        if (!isPlaying)
        {
            // Reset step highlight when stopped
            lastReportedStep = -1;
            sequencerGrid.setCurrentPlayStep(-1);
        }
    };

    // ---- Sequencer grid -------------------------------------------------------
    addAndMakeVisible(sequencerGrid);

    sequencerGrid.onStepSelected = [this](int step) { onStepSelected(step); };

    sequencerGrid.onPitchChanged = [this](int /*step*/, int /*semi*/)
    {
        // Pitch is already written to state atomics in SequencerGrid::mouseDown.
        // Nothing extra needed here.
    };

    // ---- Harmonic editor -----------------------------------------------------
    addAndMakeVisible(harmonicEditor);

    harmonicEditor.onStateChanged = [this]()
    {
        sequencerGrid.repaint(); // custom/inherited toggle may affect display
    };

    // ---- Audio ---------------------------------------------------------------
    setupAudio();

    // ---- UI update timer -----------------------------------------------------
    startTimerHz(33);

    setSize(940, 580);
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
    const juce::String err = deviceManager.initialise(
        0,       // numInputChannels
        2,       // numOutputChannels
        nullptr, // savedState XML
        true);   // selectDefaultDeviceOnFailure

    if (err.isNotEmpty())
    {
        // Non-fatal: log to stderr and continue; the device manager will
        // fall back to a null device so the app still opens.
        juce::Logger::writeToLog("Audio device error: " + err);
    }

    deviceManager.addAudioCallback(&audioEngine);
}

//==============================================================================
void MainComponent::onStepSelected(int step)
{
    selectedStep = step;
    sequencerGrid.setSelectedStep(step);
    harmonicEditor.setSelectedStep(step);
}

//==============================================================================
void MainComponent::timerCallback()
{
    const int playStep = synthState.currentPlayStep.load(std::memory_order_relaxed);
    if (playStep != lastReportedStep)
    {
        lastReportedStep = playStep;
        sequencerGrid.setCurrentPlayStep(playStep);
    }
    // Waveform repaint is handled by HarmonicEditor's own Timer
}

//==============================================================================
void MainComponent::resized()
{
    const int pad       = 10;
    const int transportH = 52;
    const int w         = getWidth();
    const int h         = getHeight();

    transportBar.setBounds(pad, pad, w - 2 * pad, transportH);

    const int contentY = pad + transportH + pad;
    const int contentH = h - contentY - pad;
    const int gridW    = juce::roundToInt(float(w - 3 * pad) * 0.58f);
    const int editorW  = w - 3 * pad - gridW;

    sequencerGrid  .setBounds(pad,          contentY, gridW,   contentH);
    harmonicEditor .setBounds(pad * 2 + gridW, contentY, editorW, contentH);
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background());
}
