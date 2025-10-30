#include "PluginEditor.h"
#include "PluginProcessor.h"

PtV2AEditor::PtV2AEditor (PtV2AProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    prompt.setTextToShowWhenEmpty ("Enter prompt…", juce::Colours::grey);
    addAndMakeVisible (prompt);

    renderButton.onClick = [this]
    {
        handleRenderButtonClicked();
    };
    addAndMakeVisible (renderButton);

    setResizable (false, false);
    setSize (420, 140);
}

void PtV2AEditor::handleRenderButtonClicked()
{
    juce::Logger::writeToLog ("=== Render Button Clicked ===");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    // Check if API is available first
    renderButton.setEnabled (false);
    renderButton.setButtonText ("Checking API...");
    
    if (!processor.isAPIAvailable())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "API Not Available",
            "MMAudio API is not running!\n\n"
            "Please start the API server:\n"
            "  docker restart mmaudio-api\n\n"
            "Or check if it's running on http://localhost:8000",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // For now, use a test video file
    // TODO: In Phase 2, extract actual video from Pro Tools timeline
    
    // Try to find test video relative to plugin or in known locations
    juce::File testVideo;
    
    // Get plugin location and navigate to project root
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory();
    
#if JUCE_WINDOWS
    // Navigate to thesis-pt-v2a root from build directory
    auto projectRoot = pluginDir;
    for (int i = 0; i < 8; ++i)  // Go up enough levels to find project root
    {
        if (projectRoot.getChildFile("companion").exists() &&
            projectRoot.getChildFile("aax-plugin").exists())
            break;
        projectRoot = projectRoot.getParentDirectory();
    }
    
    // Try relative path from project root
    auto testVideoPath = projectRoot.getChildFile("..\\..\\..\\model-tests\\data\\MMAudio_examples\\noSound\\sora_beach.mp4");
    if (testVideoPath.existsAsFile())
        testVideo = testVideoPath;
    else
        testVideo = juce::File("C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\model-tests\\data\\MMAudio_examples\\noSound\\sora_beach.mp4");
#else
    // macOS/Linux paths
    testVideo = juce::File("/mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4");
#endif
    
    if (!testVideo.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Test Video Not Found",
            "Test video file not found:\n" + testVideo.getFullPathName() + "\n\n"
            "Please place a test video at this location, or update the path in PluginEditor.cpp.\n\n"
            "Expected: sora_beach.mp4 in model-tests\\data\\MMAudio_examples\\noSound\\",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    renderButton.setButtonText ("Generating...");
    
    // Call MMAudio API
    juce::String errorMessage;
    juce::String outputPath = processor.generateAudioFromVideo (
        testVideo,
        prompt.getText(),
        "voices, music",  // Default negative prompt
        42,               // Default seed
        &errorMessage
    );
    
    if (outputPath.isEmpty())
    {
        // Generation failed
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Failed",
            "Failed to generate audio:\n\n" + errorMessage,
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Success!
    juce::Logger::writeToLog ("=== Generation Successful ===");
    juce::Logger::writeToLog ("Output: " + outputPath);
    
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Generation Complete!",
        "Audio generated successfully!\n\n"
        "Output file:\n" + outputPath + "\n\n"
        "Next: Import to Pro Tools timeline (Phase 4)",
        "OK"
    );
    
    renderButton.setEnabled (true);
    renderButton.setButtonText ("Render Audio");
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
