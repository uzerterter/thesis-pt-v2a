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
    
    // Disable button during processing
    renderButton.setEnabled (false);
    renderButton.setButtonText ("Checking...");
    
    //==========================================================================
    // Step 1: Check API availability
    //==========================================================================
    if (!processor.isAPIAvailable (PtV2AProcessor::DEFAULT_API_URL))
    {
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
    // Step 2: Check FFmpeg availability
    //==========================================================================
    renderButton.setButtonText ("Checking FFmpeg...");
    
    if (!processor.isFFmpegAvailable())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "FFmpeg Not Found",
            "FFmpeg is required for timeline selection support.\n\n"
            "Please install FFmpeg and add it to your system PATH.\n\n"
            "Download: https://ffmpeg.org/download.html\n"
            "After installation, restart Pro Tools.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    //==========================================================================
    // Step 3: Get timeline selection from Pro Tools (Phase 3B)
    //==========================================================================
    renderButton.setButtonText ("Reading Selection...");
    
    auto selection = processor.getVideoSelectionInfo();
    
    if (!selection.success)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Timeline Selection Error",
            "Could not read timeline selection from Pro Tools:\n\n" +
            selection.errorMessage + "\n\n"
            "Make sure:\n"
            "1. Pro Tools is running\n"
            "2. You have a timeline selection (In/Out points)\n"
            "3. PTSL is enabled in Pro Tools preferences",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    juce::Logger::writeToLog ("Timeline selection: " + 
                              juce::String (selection.durationSeconds, 2) + "s");
    
    //==========================================================================
    // Step 4: Validate duration (strict 10-second limit)
    //==========================================================================
    renderButton.setButtonText ("Validating...");
    
    juce::String durationError;
    if (!processor.validateVideoDuration (selection.durationSeconds, 10.0f, &durationError))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Selection Too Long",
            "Timeline selection is too long:\n\n" +
            durationError + "\n\n"
            "MMAudio works best with 8-10 second clips.\n"
            "Please shorten your timeline selection.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    //==========================================================================
    // Step 5: Get video file path from Pro Tools
    //==========================================================================
    renderButton.setButtonText ("Finding Video...");
    
    juce::String videoError;
    juce::String videoPath = processor.getVideoFileFromProTools (&videoError);
    
    if (videoPath.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "No Video Found",
            "Could not find video file in Pro Tools session:\n\n" +
            videoError + "\n\n"
            "Make sure:\n"
            "1. Your Pro Tools session has a video track\n"
            "2. Video file is imported to the session",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    juce::Logger::writeToLog ("Video file: " + videoPath);
    
    //==========================================================================
    // Step 6: Trim video to selected range (Phase 3B)
    //==========================================================================
    renderButton.setButtonText ("Trimming Video...");
    
    juce::String trimError;
    juce::String trimmedPath = processor.trimVideoSegment (
        videoPath,
        selection.inSeconds,
        selection.outSeconds,
        &trimError
    );
    
    if (trimmedPath.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Video Trimming Failed",
            "Could not trim video segment:\n\n" + trimError + "\n\n"
            "This may be due to:\n"
            "- FFmpeg error\n"
            "- Invalid video file\n"
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    juce::Logger::writeToLog ("Trimmed video: " + trimmedPath);
    
    //==========================================================================
    // Step 7: Generate audio from trimmed video
    //==========================================================================
    renderButton.setButtonText ("Generating Audio...");
    juce::Logger::writeToLog ("Starting audio generation...");
    juce::Logger::writeToLog ("Selection: " + selection.inTime + " - " + selection.outTime);
    juce::Logger::writeToLog ("Duration: " + juce::String (selection.durationSeconds, 2) + "s");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    juce::String generationError;
    juce::String outputPath = processor.generateAudioFromVideo (
        juce::File (trimmedPath),
        prompt.getText(),
        PtV2AProcessor::DEFAULT_NEGATIVE_PROMPT,
        PtV2AProcessor::DEFAULT_SEED,
        &generationError
    );
    
    //==========================================================================
    // Step 8: Cleanup temporary trimmed video file
    //==========================================================================
    juce::File (trimmedPath).deleteFile();
    juce::Logger::writeToLog ("Cleaned up trimmed video: " + trimmedPath);
    
    //==========================================================================
    // Step 9: Handle generation result
    //==========================================================================
    if (outputPath.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: Audio generation failed");
        juce::Logger::writeToLog ("Error: " + generationError);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Failed",
            "Failed to generate audio:\n\n" + generationError,
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    //==========================================================================
    // Success! Audio generated and imported to Pro Tools
    //==========================================================================
    juce::Logger::writeToLog ("=== Generation Successful ===");
    juce::Logger::writeToLog ("Output: " + outputPath);
    
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Generation Complete!",
        "Audio generated successfully from timeline selection!\n\n"
        "Selection: " + selection.inTime + " - " + selection.outTime + "\n"
        "Duration: " + juce::String (selection.durationSeconds, 1) + "s\n\n"
        "The audio has been imported to Pro Tools session.\n"
        "Check the Pro Tools timeline for the new audio track.",
        "OK"
    );
    
    // Restore button state
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
