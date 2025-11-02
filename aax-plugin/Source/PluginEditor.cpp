#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// Constructor - Initialize GUI Components
//==============================================================================
PtV2AEditor::PtV2AEditor (PtV2AProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    // Configure text input for prompt
    prompt.setMultiLine (false);                              // Single line input
    prompt.setReturnKeyStartsNewLine (false);                 // Enter key doesn't add newline
    prompt.setTextToShowWhenEmpty ("Enter prompt…", juce::Colours::grey);
    addAndMakeVisible (prompt);

    // Configure render button with click handler
    renderButton.onClick = [this]
    {
        handleRenderButtonClicked();
    };
    addAndMakeVisible (renderButton);
    
    // Configure open log button with click handler
    openLogButton.onClick = [this]
    {
        handleOpenLogButtonClicked();
    };
    addAndMakeVisible (openLogButton);

    // Set fixed window size (not resizable in Pro Tools)
    // Pro Tools plugins typically have fixed UI layouts
    setResizable (false, false);
    setSize (900, 300);  // Width x Height in pixels
}

//==============================================================================
// Event Handler - Render Button Click
//==============================================================================
void PtV2AEditor::handleRenderButtonClicked()
{
    juce::Logger::writeToLog ("=== Render Button Clicked ===");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    // Step 1: Validate API availability before starting generation
    // Prevents wasting time if API server is not running
    renderButton.setEnabled (false);
    renderButton.setButtonText ("Checking API...");
    
    if (!processor.isAPIAvailable (PtV2AProcessor::DEFAULT_API_URL))
    {
        // API not reachable - show error and restore button
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "API Not Available",
            "MMAudio API is not running!\n\n"
            "Please start the API server\n"
            "Or check if it's running on http://localhost:8000",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    //==========================================================================
    // Step 2: Locate test video file
    //==========================================================================
    // PHASE 1 LIMITATION: Uses hardcoded test video path
    // TODO Phase 2: Implement FileChooser dialog for user video selection
    // TODO Phase 3: Extract video directly from Pro Tools timeline (PTSL API)
    
    juce::File testVideo;
    
    // Strategy: Try to locate test video in multiple known locations
    // This is fragile but acceptable for Phase 1 prototype
    
    // Get plugin executable location
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory();
    
#if JUCE_WINDOWS
    // Windows: Navigate up from build directory to find project root
    // Build path: thesis-pt-v2a/build/pt_v2a_artefacts/Debug/AAX/pt_v2a.aaxplugin/Contents/x64/
    // Target:     thesis-pt-v2a/
    auto projectRoot = pluginDir;
    
    // Search up to 8 levels to find thesis-pt-v2a root
    // Identified by presence of "companion" and "aax-plugin" subdirectories
    for (int i = 0; i < 8; ++i)
    {
        if (projectRoot.getChildFile("companion").exists() &&
            projectRoot.getChildFile("aax-plugin").exists())
        {
            juce::Logger::writeToLog ("Found project root: " + projectRoot.getFullPathName());
            break;
        }
        projectRoot = projectRoot.getParentDirectory();
    }
    
    // Try relative path from project root to test data
    auto testVideoPath = projectRoot.getChildFile("..\\..\\..\\model-tests\\data\\MMAudio_examples\\noSound\\sora_beach.mp4");
    if (testVideoPath.existsAsFile())
    {
        testVideo = testVideoPath;
        juce::Logger::writeToLog ("Using relative test video: " + testVideo.getFullPathName());
    }
    else
    {
        // Fallback: Absolute path (only works on your development machine!)
        testVideo = juce::File("C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\model-tests\\data\\MMAudio_examples\\noSound\\sora_beach.mp4");
        juce::Logger::writeToLog ("Using absolute fallback path: " + testVideo.getFullPathName());
    }
#else
    // macOS/Linux: Hardcoded absolute path
    testVideo = juce::File("/mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4");
#endif
    
    // Validate test video exists
    if (!testVideo.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: Test video not found at: " + testVideo.getFullPathName());
        
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
    
    //==========================================================================
    // Step 3: Call MMAudio API to generate audio
    //==========================================================================
    renderButton.setButtonText ("Generating...");
    juce::Logger::writeToLog ("Starting audio generation...");
    juce::Logger::writeToLog ("Video: " + testVideo.getFullPathName());
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    // Call processor with default parameters
    // Using constants from PtV2AProcessor instead of hardcoded values
    juce::String errorMessage;
    juce::String outputPath = processor.generateAudioFromVideo (
        testVideo,
        prompt.getText(),
        PtV2AProcessor::DEFAULT_NEGATIVE_PROMPT,  // "voices, music"
        PtV2AProcessor::DEFAULT_SEED,             // 42
        &errorMessage
    );
    
    //==========================================================================
    // Step 4: Handle generation result
    //==========================================================================
    if (outputPath.isEmpty())
    {
        // Generation failed - show error details
        juce::Logger::writeToLog ("ERROR: Audio generation failed");
        juce::Logger::writeToLog ("Error: " + errorMessage);
        
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
    
    //==========================================================================
    // Success! Audio file generated and imported to Pro Tools
    //==========================================================================
    juce::Logger::writeToLog ("=== Generation Successful ===");
    juce::Logger::writeToLog ("Output: " + outputPath);
    
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Generation Complete!",
        "Audio generated successfully!\n\n"
        "The audio has been imported to Pro Tools session.\n"
        "Check the Pro Tools timeline for the new audio track.",
        "OK"
    );
    
    // Restore button state
    // Note: This happens immediately, not after alert is closed (async alert)
    renderButton.setEnabled (true);
    renderButton.setButtonText ("Render Audio");
}

//==============================================================================
// Event Handler - Open Log Button Click
//==============================================================================
void PtV2AEditor::handleOpenLogButtonClicked()
{
    juce::Logger::writeToLog ("=== Open Log Button Clicked ===");
    
    // Get log file path from processor
    auto logFile = PtV2AProcessor::getLogFile();
    
    if (!logFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Log File Not Found",
            "Log file does not exist yet.\n\n"
            "The log file will be created automatically when the plugin starts.\n"
            "Try using the plugin first, then check the log.",
            "OK"
        );
        return;
    }
    
    // Open log file in default text editor
    // Windows: Opens with Notepad or associated .log editor
    // macOS: Opens with TextEdit or associated app
    if (!logFile.startAsProcess())
    {
        // Fallback: Show log file location in message box
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Log File Location",
            "Could not open log file automatically.\n\n"
            "Log file location:\n" + logFile.getFullPathName() + "\n\n"
            "You can open it manually with any text editor.",
            "OK"
        );
    }
    
    juce::Logger::writeToLog ("Log file opened: " + logFile.getFullPathName());
}

//==============================================================================
// JUCE Component Lifecycle - Paint
//==============================================================================
void PtV2AEditor::paint (juce::Graphics& g)
{
    // Fill background with dark grey
    // Pro Tools typically uses dark UI themes, so this matches the aesthetic
    g.fillAll (juce::Colours::darkgrey);
}

//==============================================================================
// JUCE Component Lifecycle - Layout
//==============================================================================
void PtV2AEditor::resized()
{
    // Layout components with 12px margin around edges
    auto r = getLocalBounds().reduced (12);
    
    // Prompt text input: full width, 28px height, at top
    prompt.setBounds (r.removeFromTop (28));
    
    // 10px spacing between components
    r.removeFromTop (10);
    
    // Render button: 160px wide, 28px height, left-aligned
    auto buttonRow = r.removeFromTop (28);
    renderButton.setBounds (buttonRow.removeFromLeft (160));
    
    // 10px spacing between buttons
    buttonRow.removeFromLeft (10);
    
    // Open Log button: 120px wide, same height, next to render button
    openLogButton.setBounds (buttonRow.removeFromLeft (120));
    
    // Future components can be added below by continuing to use r.removeFromTop()
    // Example for Phase 2:
    //   r.removeFromTop (10);  // spacing
    //   negativePromptLabel.setBounds (r.removeFromTop (20));
    //   negativePrompt.setBounds (r.removeFromTop (28));
}
