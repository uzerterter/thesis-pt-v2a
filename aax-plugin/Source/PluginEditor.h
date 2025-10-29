#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PtV2AProcessor;

class PtV2AEditor : public juce::AudioProcessorEditor
{
public:
    explicit PtV2AEditor (PtV2AProcessor& p);
    ~PtV2AEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    PtV2AProcessor& processor;

    juce::TextEditor prompt;
    juce::TextButton renderButton { "Render Audio" };
    
    /** Handle render button click with full API integration */
    void handleRenderButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AEditor)
};
