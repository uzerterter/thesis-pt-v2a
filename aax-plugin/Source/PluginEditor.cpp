#include "PluginEditor.h"
#include "PluginProcessor.h"

PtV2AEditor::PtV2AEditor (PtV2AProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    prompt.setTextToShowWhenEmpty ("Enter prompt…", juce::Colours::grey);
    addAndMakeVisible (prompt);

    renderButton.onClick = [this]
    {
        // TODO: call your companion app over localhost
        juce::Logger::writeToLog ("Render clicked: " + prompt.getText());
    };
    addAndMakeVisible (renderButton);

    setResizable (false, false);
    setSize (420, 140);
}

void PtV2AEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void PtV2AEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    prompt.setBounds (r.removeFromTop (28));
    r.removeFromTop (10);
    renderButton.setBounds (r.removeFromTop (28).withWidth (160));
}
