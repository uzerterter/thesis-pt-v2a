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
    
    // Configure render dummy button with click handler
    renderDummyButton.onClick = [this]
    {
        handleRenderDummyButtonClicked();
    };
    addAndMakeVisible (renderDummyButton);
    
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
    // Step 2: Start async timeline selection read (non-blocking!)
    //==========================================================================
    renderButton.setButtonText ("Reading Selection...");
    renderButton.setEnabled (false);
    
    // Start async PTSL process - timer will handle the rest!
    // The workflow continues in handleTimelineSelectionResult() after PTSL finishes
    startTimelineSelectionRead();
}

// Workflow now continues asynchronously in:
//   1. timerCallback() - polls PTSL process status
//   2. handleTimelineSelectionResult() - continues after PTSL finishes

//==============================================================================
// Event Handler - Render Dummy Button Click (Presentation Mode)
//==============================================================================
void PtV2AEditor::handleRenderDummyButtonClicked()
{
    juce::Logger::writeToLog ("=== Render Dummy Button Clicked (Presentation Mode) ===");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    // Disable button during processing
    renderDummyButton.setEnabled (false);
    renderDummyButton.setButtonText ("Generating...");
    
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
        
        renderDummyButton.setEnabled (true);
        renderDummyButton.setButtonText ("Render (dummy video)");
        return;
    }
    
    //==========================================================================
    // Step 2: Use predefined test video from temp/pt_v2a directory
    //==========================================================================
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto ptv2aDir = tempDir.getChildFile ("ptv2a");
    juce::File dummyVideo = ptv2aDir.getChildFile ("test_video.mp4");
    
    if (!dummyVideo.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Dummy Video Not Found",
            "Test video file not found at:\n\n" + dummyVideo.getFullPathName() + "\n\n"
            "Please place a test video file at:\n" + 
            ptv2aDir.getFullPathName() + "\\test_video.mp4",
            "OK"
        );
        
        renderDummyButton.setEnabled (true);
        renderDummyButton.setButtonText ("Render (dummy video)");
        return;
    }
    
    juce::Logger::writeToLog ("Using dummy video from temp: " + dummyVideo.getFullPathName());
    
    //==========================================================================
    // Step 3: Generate audio from dummy video
    //==========================================================================
    juce::Logger::writeToLog ("Starting audio generation with dummy video...");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    
    juce::String generationError;
    juce::String outputPath = processor.generateAudioFromVideo (
        dummyVideo,
        prompt.getText(),
        PtV2AProcessor::DEFAULT_NEGATIVE_PROMPT,
        PtV2AProcessor::DEFAULT_SEED,
        &generationError
    );
    
    //==========================================================================
    // Step 4: Handle generation result
    //==========================================================================
    if (outputPath.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: Audio generation failed (dummy video)");
        juce::Logger::writeToLog ("Error: " + generationError);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Failed",
            "Failed to generate audio:\n\n" + generationError,
            "OK"
        );
        
        renderDummyButton.setEnabled (true);
        renderDummyButton.setButtonText ("Render (dummy video)");
        return;
    }
    
    //==========================================================================
    // Success! Background process started
    //==========================================================================
    juce::Logger::writeToLog ("=== Generation Process Started (Dummy Video) ===");
    juce::Logger::writeToLog ("Process running in background: " + outputPath);
    
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Audio Generation Started",
        "Audio generation started in background!\n\n"
        "Video: test_video.mp4 (from temp/pt_v2a/)\n"
        "Prompt: " + prompt.getText() + "\n\n"
        "The process will:\n"
        "1. Generate audio via MMAudio API \n"
        "2. Import audio to Pro Tools timeline via PTSL\n\n"
        "⏳ Please wait 1-2 minutes, then check the Pro Tools timeline\n"
        "    for a new audio track.",
        "OK"
    );
    
    // Restore button state
    renderDummyButton.setEnabled (true);
    renderDummyButton.setButtonText ("Render (dummy video)");
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
    
    // Button row: Render buttons and Open Log button
    auto buttonRow = r.removeFromTop (28);
    
    // Render Audio button: 160px wide, left-aligned
    renderButton.setBounds (buttonRow.removeFromLeft (160));
    
    // 10px spacing between buttons
    buttonRow.removeFromLeft (10);
    
    // Render (dummy video) button: 200px wide, next to Render Audio
    renderDummyButton.setBounds (buttonRow.removeFromLeft (200));
    
    // 10px spacing between buttons
    buttonRow.removeFromLeft (10);
    
    // Open Log button: 120px wide, at the end
    openLogButton.setBounds (buttonRow.removeFromLeft (120));
    
    // Future components can be added below by continuing to use r.removeFromTop()
    // Example for Phase 2:
    //   r.removeFromTop (10);  // spacing
    //   negativePromptLabel.setBounds (r.removeFromTop (20));
    //   negativePrompt.setBounds (r.removeFromTop (28));
}

//==============================================================================
// Async PTSL Communication Implementation
//==============================================================================

void PtV2AEditor::startTimelineSelectionRead()
{
    juce::Logger::writeToLog ("=== Starting Async Timeline Selection Read ===");
    
    auto pythonExe = processor.getPythonExecutable();
    auto scriptFile = processor.getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: API client script not found");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Script Error",
            "API client script not found.\n\n"
            "Please check plugin installation.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Build command: Direct Python call with -X utf8 flag
    // Use get_video_info to get BOTH timeline selection AND video path in one call
    juce::StringArray commandArray;
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");
    commandArray.add (scriptFile.getFullPathName());
    commandArray.add ("--action");
    commandArray.add ("get_video_info");
    
    juce::Logger::writeToLog ("Starting PTSL process (async)...");
    juce::Logger::writeToLog ("Command: " + commandArray.joinIntoString (" "));
    
    // Create and start process
    ptslProcess = std::make_unique<juce::ChildProcess>();
    
    if (!ptslProcess->start (commandArray))
    {
        juce::Logger::writeToLog ("ERROR: Failed to start PTSL process");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Process Error",
            "Failed to start Python process for timeline selection.\n\n"
            "Please check plugin installation.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        ptslProcess.reset();
        return;
    }
    
    // Record start time for timeout detection
    asyncOperationStartTime = juce::Time::getCurrentTime();
    currentAsyncState = AsyncState::ReadingTimeline;
    
    // Start timer to poll process status every 100ms
    startTimer (TIMER_INTERVAL_MS);
    
    juce::Logger::writeToLog ("Timeline selection started, timer polling every " + 
                              juce::String (TIMER_INTERVAL_MS) + "ms");
}

void PtV2AEditor::timerCallback()
{
    auto elapsed = juce::Time::getCurrentTime() - asyncOperationStartTime;
    
    // Handle different async states
    switch (currentAsyncState)
    {
        case AsyncState::ReadingTimeline:
        {
            // Safety check
            if (!ptslProcess)
            {
                stopTimer();
                currentAsyncState = AsyncState::Idle;
                return;
            }
            
            // Check for timeout
            if (elapsed.inMilliseconds() > PTSL_TIMEOUT_MS)
            {
                juce::Logger::writeToLog ("ERROR: Timeline selection timed out after " + 
                                          juce::String (PTSL_TIMEOUT_MS) + "ms");
                
                stopTimer();
                ptslProcess->kill();
                ptslProcess.reset();
                currentAsyncState = AsyncState::Idle;
                
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Timeout",
                    "Timeline selection read timed out.\n\n"
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
            
            // Check if process is still running
            if (ptslProcess->isRunning())
            {
                // Still running - keep waiting, Pro Tools stays responsive!
                return;
            }
            
            // Process finished! Read output and stop timer
            juce::Logger::writeToLog ("Timeline selection finished after " + 
                                      juce::String (elapsed.inMilliseconds()) + "ms");
            
            auto output = ptslProcess->readAllProcessOutput();
            ptslProcess.reset();
            
            juce::Logger::writeToLog ("Timeline selection output:");
            juce::Logger::writeToLog (output);
            
            // Pass output to handler for parsing
            handleTimelineSelectionResult (output);
            break;
        }
        
        case AsyncState::GeneratingAudio:
        {
            // Poll for output file existence
            checkAudioGenerationComplete();
            break;
        }
        
        case AsyncState::ImportingAudio:
        {
            // Safety check
            if (!ptslProcess)
            {
                stopTimer();
                currentAsyncState = AsyncState::Idle;
                return;
            }
            
            // Check for timeout
            if (elapsed.inMilliseconds() > PTSL_TIMEOUT_MS)
            {
                juce::Logger::writeToLog ("ERROR: Audio import timed out after " + 
                                          juce::String (PTSL_TIMEOUT_MS) + "ms");
                
                stopTimer();
                ptslProcess->kill();
                ptslProcess.reset();
                currentAsyncState = AsyncState::Idle;
                
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Import Timeout",
                    "Audio import to Pro Tools timed out.\n\n"
                    "The audio file was generated successfully but could not be imported.\n"
                    "You can manually import it from the temp directory.",
                    "OK"
                );
                
                renderButton.setEnabled (true);
                renderButton.setButtonText ("Render Audio");
                return;
            }
            
            // Check if process is still running
            if (ptslProcess->isRunning())
            {
                // Still running - keep waiting
                return;
            }
            
            // Process finished!
            juce::Logger::writeToLog ("Audio import finished after " + 
                                      juce::String (elapsed.inMilliseconds()) + "ms");
            
            auto output = ptslProcess->readAllProcessOutput();
            ptslProcess.reset();
            
            juce::Logger::writeToLog ("Import output:");
            juce::Logger::writeToLog (output);
            
            // Handle import result
            handleAudioImportResult (output);
            break;
        }
        
        case AsyncState::Idle:
        default:
            // Nothing to do
            stopTimer();
            break;
    }
}

void PtV2AEditor::handleTimelineSelectionResult (const juce::String& output)
{
    // Parse output
    if (output.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: No output from PTSL process");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Timeline Selection Error",
            "No output from PTSL process.\n\n"
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Extract JSON from output (might have debug lines)
    auto lines = juce::StringArray::fromLines (output);
    juce::String jsonOutput;
    for (const auto& line : lines)
    {
        if (line.trimStart().startsWith ("{"))
        {
            jsonOutput = line.trim();
            break;
        }
    }
    
    if (jsonOutput.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: No JSON response in PTSL output");
        juce::Logger::writeToLog ("Full output: " + output);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Timeline Selection Error",
            "No JSON response found in PTSL output.\n\n"
            "Check the log file for details.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Parse JSON (now includes video_path from combined action!)
    auto json = juce::JSON::parse (jsonOutput);
    if (auto* obj = json.getDynamicObject())
    {
        bool success = obj->getProperty ("success");
        juce::String inTime = obj->getProperty ("in_time").toString();
        juce::String outTime = obj->getProperty ("out_time").toString();
        float durationSeconds = (float) (double) obj->getProperty ("duration_seconds");
        juce::String videoPath = obj->getProperty ("video_path").toString();
        juce::String errorMessage = obj->getProperty ("error").toString();
        
        if (!success)
        {
            juce::Logger::writeToLog ("Timeline selection FAILED: " + errorMessage);
            
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Timeline Selection Error",
                "Could not read timeline selection:\n\n" +
                errorMessage + "\n\n"
                "Please:\n"
                "1. Select a video clip in Pro Tools (use Selector Tool)\n"
                "2. Make sure the clip is 5-12 seconds long\n"
                "3. Pro Tools must be running with PTSL enabled",
                "OK"
            );
            
            renderButton.setEnabled (true);
            renderButton.setButtonText ("Render Audio");
            return;
        }
        
        juce::Logger::writeToLog ("Timeline selection SUCCESS: " + inTime + " - " + outTime);
        juce::Logger::writeToLog ("Duration: " + juce::String (durationSeconds, 2) + "s");
        
        // Store timeline in-time for audio import positioning
        timelineInTime = inTime;
        juce::Logger::writeToLog ("Stored timeline in-time: " + timelineInTime);
        
        // Validate duration: 5-12 seconds
        if (durationSeconds < 5.0f)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Selection Too Short",
                juce::String::formatted (
                    "Timeline selection is only %.2f seconds.\n\n"
                    "MMAudio requires clip selections between 5-12 seconds.\n\n"
                    "Please:\n"
                    "1. Select a longer video clip\n"
                    "2. Or extend your current selection (In/Out points)",
                    durationSeconds
                ),
                "OK"
            );
            
            renderButton.setEnabled (true);
            renderButton.setButtonText ("Render Audio");
            return;
        }
        
        if (durationSeconds > 12.0f)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Selection Too Long",
                juce::String::formatted (
                    "Timeline selection is %.2f seconds.\n\n"
                    "MMAudio requires clip selections between 5-12 seconds.\n\n"
                    "Please:\n"
                    "1. Select a shorter video clip\n"
                    "2. Or reduce your current selection (In/Out points)",
                    durationSeconds
                ),
                "OK"
            );
            
            renderButton.setEnabled (true);
            renderButton.setButtonText ("Render Audio");
            return;
        }
        
        // Duration is valid! Continue with workflow...
        juce::Logger::writeToLog ("Selection duration valid: " + 
                                  juce::String (durationSeconds, 2) + "s");
        
        //======================================================================
        // Step 3: Video file path (already retrieved in combined action!)
        //======================================================================
        // NOTE: We already have videoPath from the combined get_video_info action
        // No need for separate getVideoFileFromProTools() call (avoids second timeout!)
        
        if (videoPath.isEmpty())
        {
            juce::Logger::writeToLog ("ERROR: No video path in combined result");
            
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "No Video Found",
                "Could not find video file in Pro Tools session:\n\n" +
                errorMessage + "\n\n"
                "Make sure:\n"
                "1. Your Pro Tools session has a video track\n"
                "2. Video file is imported to the session",
                "OK"
            );
            
            renderButton.setEnabled (true);
            renderButton.setButtonText ("Render Audio");
            currentAsyncState = AsyncState::Idle;
            return;
        }
        
        juce::Logger::writeToLog ("Video file: " + videoPath);
        
        //======================================================================
        // Step 4: Start async audio generation (NO PTSL import yet!)
        //======================================================================
        renderButton.setButtonText ("Generating Audio...");
        juce::Logger::writeToLog ("Starting async audio generation...");
        juce::Logger::writeToLog ("Selected clip: " + inTime + " - " + outTime);
        juce::Logger::writeToLog ("Duration: " + juce::String (durationSeconds, 2) + "s");
        juce::Logger::writeToLog ("Video file: " + videoPath);
        juce::Logger::writeToLog ("Prompt: " + prompt.getText());
        
        // Start async audio generation (will poll for output file)
        startAudioGeneration (videoPath, prompt.getText());
        
        // Show info message
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Audio Generation Started!",
            "Audio generation started!\n\n"
            "Selection: " + inTime + " - " + outTime + "\n"
            "Duration: " + juce::String (durationSeconds, 1) + "s\n"
            "Video: " + juce::File(videoPath).getFileName() + "\n"
            "Prompt: " + prompt.getText() + "\n\n"
            "The plugin will:\n"
            "1. Generate audio via MMAudio API (~60-90s)\n"
            "2. Automatically import to Pro Tools timeline\n\n"
            "⏳ You'll see a notification when complete!",
            "OK"
        );
    }
    else
    {
        juce::Logger::writeToLog ("ERROR: Failed to parse JSON response");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Parse Error",
            "Failed to parse timeline selection response.\n\n"
            "Check the log file for details.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        currentAsyncState = AsyncState::Idle;
    }
}

//==============================================================================
// Async Audio Generation Implementation
//==============================================================================

void PtV2AEditor::startAudioGeneration (const juce::String& videoPath, const juce::String& promptText)
{
    juce::Logger::writeToLog ("=== Starting Async Audio Generation ===");
    juce::Logger::writeToLog ("Video: " + videoPath);
    juce::Logger::writeToLog ("Prompt: " + promptText);
    
    // Store parameters for later use
    currentVideoPath = videoPath;
    currentPrompt = promptText;
    
    // Call processor to start generation (returns immediately with expected output path)
    juce::String errorMessage;
    expectedAudioOutputPath = processor.generateAudioFromVideo (
        juce::File (videoPath),
        promptText,
        PtV2AProcessor::DEFAULT_NEGATIVE_PROMPT,
        PtV2AProcessor::DEFAULT_SEED,
        &errorMessage
    );
    
    if (expectedAudioOutputPath.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: Failed to start audio generation: " + errorMessage);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Failed",
            "Failed to start audio generation:\n\n" + errorMessage,
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    juce::Logger::writeToLog ("Expected output: " + expectedAudioOutputPath);
    juce::Logger::writeToLog ("Starting polling for output file...");
    
    // Switch to GeneratingAudio state
    currentAsyncState = AsyncState::GeneratingAudio;
    asyncOperationStartTime = juce::Time::getCurrentTime();
    
    // Timer is already running from previous state, just continue polling
    if (!isTimerRunning())
        startTimer (TIMER_INTERVAL_MS);
}

void PtV2AEditor::checkAudioGenerationComplete()
{
    // Check for timeout
    auto elapsed = juce::Time::getCurrentTime() - asyncOperationStartTime;
    if (elapsed.inMilliseconds() > GENERATION_TIMEOUT_MS)
    {
        juce::Logger::writeToLog ("ERROR: Audio generation timed out after " + 
                                  juce::String (GENERATION_TIMEOUT_MS / 1000) + "s");
        
        stopTimer();
        currentAsyncState = AsyncState::Idle;
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Timeout",
            "Audio generation timed out after 3 minutes.\n\n"
            "This might indicate:\n"
            "- API server is not responding\n"
            "- Network connection issues\n"
            "- Video is too complex to process\n\n"
            "Check the log file for details.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Check if output file exists
    juce::File outputFile (expectedAudioOutputPath);
    if (!outputFile.existsAsFile())
    {
        // Still generating, keep waiting
        // Log every 10 seconds
        int64_t elapsedMs = elapsed.inMilliseconds();
        if (elapsedMs > 0 && (elapsedMs / 1000) % 10 == 0 && (elapsedMs % 1000) < TIMER_INTERVAL_MS)
        {
            juce::Logger::writeToLog ("Still generating... (" + 
                                      juce::String (elapsedMs / 1000) + "s elapsed)");
        }
        return;
    }
    
    // Output file exists! Generation complete
    juce::Logger::writeToLog ("✓ Audio generation complete after " + 
                              juce::String (elapsed.inSeconds()) + "s");
    juce::Logger::writeToLog ("Output file: " + expectedAudioOutputPath);
    
    // Stop timer (will restart for import)
    stopTimer();
    
    // Now start async PTSL import
    renderButton.setButtonText ("Importing Audio...");
    startAudioImport (expectedAudioOutputPath);
}

void PtV2AEditor::startAudioImport (const juce::String& audioPath)
{
    juce::Logger::writeToLog ("=== Starting Async Audio Import ===");
    juce::Logger::writeToLog ("Audio file: " + audioPath);
    
    auto pythonExe = processor.getPythonExecutable();
    auto scriptFile = processor.getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: API client script not found");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Script Error",
            "API client script not found.\n\n"
            "Audio was generated successfully at:\n" + audioPath + "\n\n"
            "But automatic import failed.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    // Build command: python script.py --action import_audio --audio-path <path> --timecode <tc>
    juce::StringArray commandArray;
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");
    commandArray.add (scriptFile.getFullPathName());
    commandArray.add ("--action");
    commandArray.add ("import_audio");
    commandArray.add ("--audio-path");
    commandArray.add (audioPath);
    
    // Add timecode position if available
    if (timelineInTime.isNotEmpty())
    {
        commandArray.add ("--timecode");
        commandArray.add (timelineInTime);
        juce::Logger::writeToLog ("Import position: " + timelineInTime);
    }
    else
    {
        juce::Logger::writeToLog ("Warning: No timeline position available, importing at session start");
    }
    
    juce::Logger::writeToLog ("Starting PTSL import process...");
    juce::Logger::writeToLog ("Command: " + commandArray.joinIntoString (" "));
    
    // Create and start process
    ptslProcess = std::make_unique<juce::ChildProcess>();
    
    if (!ptslProcess->start (commandArray))
    {
        juce::Logger::writeToLog ("ERROR: Failed to start PTSL import process");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Import Error",
            "Failed to start audio import process.\n\n"
            "Audio was generated successfully at:\n" + audioPath + "\n\n"
            "You can manually import it to Pro Tools.",
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        ptslProcess.reset();
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    // Record start time for timeout detection
    asyncOperationStartTime = juce::Time::getCurrentTime();
    currentAsyncState = AsyncState::ImportingAudio;
    
    // Start timer to poll process status
    startTimer (TIMER_INTERVAL_MS);
    
    juce::Logger::writeToLog ("PTSL import started, timer polling...");
}

void PtV2AEditor::handleAudioImportResult (const juce::String& output)
{
    stopTimer();
    currentAsyncState = AsyncState::Idle;
    
    juce::Logger::writeToLog ("=== Audio Import Result ===");
    juce::Logger::writeToLog (output);
    
    // Parse output (look for success indicator)
    bool success = output.contains ("success") && output.contains ("true");
    
    if (success)
    {
        juce::Logger::writeToLog ("✓ Audio successfully imported to Pro Tools timeline!");
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Success!",
            "Audio generated and imported to Pro Tools timeline!\n\n"
            "✓ Generation complete\n"
            "✓ Imported to timeline\n\n"
            "Check your Pro Tools session for the new audio track.",
            "OK"
        );
    }
    else
    {
        juce::Logger::writeToLog ("⚠️ Audio import may have failed");
        juce::Logger::writeToLog ("Output: " + output);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Import Issue",
            "Audio was generated successfully, but import might have failed.\n\n"
            "Check the log file for details.\n"
            "You may need to manually import the audio file.",
            "OK"
        );
    }
    
    renderButton.setEnabled (true);
    renderButton.setButtonText ("Render Audio");
}
