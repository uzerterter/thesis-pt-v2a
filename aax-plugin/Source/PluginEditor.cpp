#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// Constructor - Initialize GUI Components
//==============================================================================
PtV2AEditor::PtV2AEditor (PtV2AProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    // Configure text input for prompt
    prompt.setMultiLine (true);  // Allow line breaks
    prompt.setReturnKeyStartsNewLine (true);  // Enter = new line
    prompt.setScrollbarsShown (true);  // Show scrollbar if needed
    promptLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (promptLabel);
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

    // Configure settings button with click handler
    settingsButton.onClick = [this] { showCredentialDialog(); };
        addAndMakeVisible (settingsButton);

    // First-launch check: show dialog if credentials are missing
    if (processor.getCloudflareClientSecret().isEmpty())
    {
    // Delay dialog to avoid showing during plugin initialization
    juce::Timer::callAfterDelay (500, [this]
    {
        showCredentialDialog();
    });
}
    
    // Configure video offset label (deprecated TODO remove in future)
    // videoOffsetLabel.setJustificationType (juce::Justification::centredLeft);
    // addAndMakeVisible (videoOffsetLabel);
    
    // Configure video offset input (deprecated TODO remove in future)
    // videoOffsetInput.setMultiLine (false);
    // videoOffsetInput.setReturnKeyStartsNewLine (false);
    // videoOffsetInput.setTextToShowWhenEmpty ("e.g., 00:02 (leave empty if video starts at timeline beginning)", juce::Colours::grey);
    // addAndMakeVisible (videoOffsetInput);
    
    // Configure negative prompt label
    negativePromptLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (negativePromptLabel);
    
    // Configure negative prompt input
    negativePromptInput.setMultiLine (true);
    negativePromptInput.setReturnKeyStartsNewLine (true);
    negativePromptInput.setScrollbarsShown (true);
    negativePromptInput.setTextToShowWhenEmpty ("voices, music, melody, singing, speech,interference", juce::Colours::grey);
    negativePromptInput.setText ("voices, music, melody, singing, speech, interference");  // Set default value
    addAndMakeVisible (negativePromptInput);
    
    // Configure seed label
    seedLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (seedLabel);
    
    // Configure seed input
    seedInput.setMultiLine (false);
    seedInput.setReturnKeyStartsNewLine (false);
    seedInput.setTextToShowWhenEmpty ("42", juce::Colours::grey);
    seedInput.setText ("42");  // Set default value
    addAndMakeVisible (seedInput);
    
    // Configure generation mode radio buttons (V2A vs T2A)
    v2aModeButton.setRadioGroupId (1001);
    v2aModeButton.setToggleState (true, juce::dontSendNotification);  // Default: V2A mode
    v2aModeButton.onClick = [this] { handleGenerationModeChange(); };
    addAndMakeVisible (v2aModeButton);
    
    t2aModeButton.setRadioGroupId (1001);
    t2aModeButton.onClick = [this] { handleGenerationModeChange(); };
    addAndMakeVisible (t2aModeButton);
    
    // Configure duration dropdown (for T2A mode)
    durationLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (durationLabel);
    
    durationComboBox.addItem ("4s", 1);
    durationComboBox.addItem ("5s", 2);
    durationComboBox.addItem ("6s", 3);
    durationComboBox.addItem ("7s", 4);
    durationComboBox.addItem ("8s", 5);
    durationComboBox.addItem ("9s", 6);
    durationComboBox.addItem ("10s", 7);
    durationComboBox.addItem ("11s", 8);
    durationComboBox.addItem ("12s", 9);
    durationComboBox.setSelectedId (5, juce::dontSendNotification);  // Default: 8s
    durationComboBox.setEnabled (false);  // Initially disabled (V2A mode)
    addAndMakeVisible (durationComboBox);
    
    // Configure high precision mode toggle (deprecated TODO remove in future)
    // highPrecisionModeToggle.setToggleState (false, juce::dontSendNotification);  // Default: off (bfloat16)
    // addAndMakeVisible (highPrecisionModeToggle);
    
    // Configure model selection labels
    modelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modelLabel);
    
    modelSizeLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modelSizeLabel);
    
    // Configure model provider ComboBox
    modelProviderComboBox.addItem ("MMAudio", 1);
    modelProviderComboBox.addItem ("HunyuanVideo-Foley", 2);
    modelProviderComboBox.setSelectedId (1, juce::dontSendNotification);  // Default: MMAudio
    modelProviderComboBox.onChange = [this] { handleModelProviderChange(); };
    addAndMakeVisible (modelProviderComboBox);
    
    // Configure model size ComboBox (initial values for MMAudio)
    modelSizeComboBox.addItem ("Large", 1);
    modelSizeComboBox.addItem ("Medium", 2);
    modelSizeComboBox.addItem ("Small", 3);
    modelSizeComboBox.setSelectedId (1, juce::dontSendNotification);  // Default: Large
    addAndMakeVisible (modelSizeComboBox);

    // Set fixed window size (not resizable in Pro Tools)
    // Pro Tools plugins typically have fixed UI layouts
    setResizable (true, true);
    setResizeLimits (600, 380, 1200, 660);  // min/max width/height
    setSize (900, 520);  // Width x Height in pixels (increased for model selection row)
}

//==============================================================================
// Event Handler - Render Button Click
//==============================================================================
void PtV2AEditor::handleRenderButtonClicked()
{
    juce::Logger::writeToLog ("=== Render Button Clicked ===");
    juce::Logger::writeToLog ("Prompt: " + prompt.getText());
    juce::Logger::writeToLog ("Mode: " + juce::String (isT2AMode ? "T2A" : "V2A"));
    
    // T2A mode validation and workflow
    if (isT2AMode)
    {
        // Validate model selection
        if (modelProviderComboBox.getSelectedId() != 1)  // Not MMAudio
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Invalid Model Selection",
                "T2A mode only supports MMAudio.\n\n"
                "HunyuanVideo-Foley requires video input (V2A mode).",
                "OK"
            );
            return;
        }
        
        // Start T2A workflow (text-to-audio without video)
        handleT2ARenderButtonClicked();
        return;
    }
    
    // Disable button during processing
    renderButton.setEnabled (false);
    renderButton.setButtonText ("Checking...");
    
    //==========================================================================
    // Step 1: Check API availability (uses config.json for cloudflared support)
    //==========================================================================
    // Determine which API to check based on selected provider
    int providerSelectedId = modelProviderComboBox.getSelectedId();
    juce::String serviceName = (providerSelectedId == 2) ? "hunyuan" : "mmaudio";
    juce::String providerDisplayName = (providerSelectedId == 2) ? "HunyuanVideo-Foley" : "MMAudio";
    
    juce::String apiUrl = processor.getConfiguredAPIUrl (serviceName);
    if (!processor.isAPIAvailable (apiUrl))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "API Not Available",
            providerDisplayName + " API is not running!\n\n"
            "Please start the API server\n"
            "Trying to connect to: " + apiUrl,
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
// Event Handler - T2A Render Button Click
//==============================================================================
void PtV2AEditor::handleT2ARenderButtonClicked()
{
    juce::Logger::writeToLog ("=== T2A Render Button Clicked ===");
    
    // Parse duration from dropdown (e.g., "8s" -> 8.0f)
    juce::String durationText = durationComboBox.getText();
    float duration = durationText.dropLastCharacters(1).getFloatValue();  // Remove "s" suffix
    
    juce::Logger::writeToLog ("T2A Duration: " + juce::String(duration, 1) + "s");
    
    // Store duration for later use
    t2aDuration = duration;
    
    // Disable button during operation
    renderButton.setEnabled (false);
    renderButton.setButtonText ("Checking API...");
    
    // Check MMAudio API availability (T2A only supports MMAudio)
    juce::String apiUrl = processor.getConfiguredAPIUrl ("mmaudio");
    if (!processor.isAPIAvailable (apiUrl))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "API Not Available",
            "MMAudio API is not running!\n\n"
            "Please start the API server\n"
            "Trying to connect to: " + apiUrl,
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        return;
    }
    
    // Start async timeline-only read (T2A doesn't need video clips)
    renderButton.setButtonText ("Reading Selection...");
    startTimelineSelectionReadOnly();
}

//==============================================================================
// JUCE Component Lifecycle - Paint
//==============================================================================
void PtV2AEditor::paint (juce::Graphics& g)
{
    // Fill background with dark grey
    g.fillAll (juce::Colours::darkgrey);
}

//==============================================================================
// JUCE Component Lifecycle - Layout
//==============================================================================
void PtV2AEditor::resized()
{
    // Layout components with 24px margin around edges
    auto r = getLocalBounds().reduced (24);
    
    // Prompt text input: full width, 28px height, at top
    auto promptRow = r.removeFromTop (28);
    promptLabel.setBounds (promptRow.removeFromLeft (65));
    promptRow.removeFromLeft (10);    
    prompt.setBounds (promptRow);

    // 20px spacing between components
    r.removeFromTop (20);
 
    // Negative prompt row: Label + Input field
    auto negativePromptRow = r.removeFromTop (28);
    negativePromptLabel.setBounds (negativePromptRow.removeFromLeft (65));
    negativePromptRow.removeFromLeft (10);
    negativePromptInput.setBounds (negativePromptRow);

    // 20px spacing before next row
    r.removeFromTop (20);
    
    // Seed and generation mode row: Label + Input + Radio Buttons + Duration
    auto seedRow = r.removeFromTop (28);
    seedLabel.setBounds (seedRow.removeFromLeft (65));
    seedRow.removeFromLeft (10);
    seedInput.setBounds (seedRow.removeFromLeft (120));
    seedRow.removeFromLeft (20);
    
    // Radio buttons for V2A/T2A mode
    v2aModeButton.setBounds (seedRow.removeFromLeft (140));
    seedRow.removeFromLeft (10);
    t2aModeButton.setBounds (seedRow.removeFromLeft (130));
    seedRow.removeFromLeft (20);
    
    // Duration controls (only active in T2A mode)
    durationLabel.setBounds (seedRow.removeFromLeft (70));
    seedRow.removeFromLeft (5);
    durationComboBox.setBounds (seedRow.removeFromLeft (80));
    
    // 20px spacing before model selection
    r.removeFromTop (20);
    
    // Model selection row: Label + Provider ComboBox + Label + Size ComboBox
    auto modelRow = r.removeFromTop (28);
    modelLabel.setBounds (modelRow.removeFromLeft (65));
    modelRow.removeFromLeft (10);
    modelProviderComboBox.setBounds (modelRow.removeFromLeft (200));
    modelRow.removeFromLeft (20);
    modelSizeLabel.setBounds (modelRow.removeFromLeft (40));
    modelRow.removeFromLeft (10);
    modelSizeComboBox.setBounds (modelRow.removeFromLeft (180));
    modelRow.removeFromLeft (20);
    // highPrecisionModeToggle.setBounds (modelRow); // (deprecated TODO remove in future)


    // 30px spacing before next row
    r.removeFromTop (30);
    
    // Video offset row: Label + Input field (deprecated TODO remove in future)
    // auto offsetRow = r.removeFromTop (28);
    // Label: 220px wide (deprecated TODO remove in future)
    // videoOffsetLabel.setBounds (offsetRow.removeFromLeft (220));
    // 10px spacing between label and input
    // offsetRow.removeFromLeft (10);
    // Input field: fixed 80px width (deprecated TODO remove in future)
    // videoOffsetInput.setBounds (offsetRow.removeFromLeft (80));

    // 60px spacing before buttons section
    r.removeFromTop (60);

    // Button row: center the button group so left/right margins are equal
    auto buttonRow = r.removeFromTop (28);

    const int renderW   = 160;
    const int dummyW    = 200;
    const int openLogW  = 120;
    const int gap1      = 20;  // between render and dummy
    const int gap2      = 20;  // between dummy and openLog

    const int totalWidth = renderW + gap1 + dummyW + gap2 + openLogW;
    int startX = buttonRow.getX() + juce::jmax (0, (buttonRow.getWidth() - totalWidth) / 2);
    int y = buttonRow.getY();
    int h = buttonRow.getHeight();

    renderButton.setBounds (startX, y, renderW, h);
    startX += renderW + gap1;
    // renderDummyButton.setBounds (startX, y, dummyW, h); // (deprecated TODO remove in future)
    startX += dummyW + gap2;
    openLogButton.setBounds (startX, y, openLogW, h);

    // Settings button at bottom-right
    auto settingsRow = r.removeFromBottom (28);
    r.removeFromBottom (10);  // Spacing
    settingsRow.removeFromLeft (getWidth() - 200);  // Align right
    settingsButton.setBounds (settingsRow.removeFromLeft (180));

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

//==============================================================================
// Async PTSL - Timeline Selection Read Only (T2A workflow - no video required)
//==============================================================================

void PtV2AEditor::startTimelineSelectionReadOnly()
{
    juce::Logger::writeToLog ("=== Starting Async Timeline Selection Read (T2A - No Video) ===");
    
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
    
    // Build command: Use get_timeline_selection (no video clip check)
    juce::StringArray commandArray;
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");
    commandArray.add (scriptFile.getFullPathName());
    commandArray.add ("--action");
    commandArray.add ("get_video_selection");  // T2A: timeline only, no video file lookup
    
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
    
    juce::Logger::writeToLog ("Timeline selection started (T2A mode), timer polling every " + 
                              juce::String (TIMER_INTERVAL_MS) + "ms");
}

//==============================================================================
// Async PTSL - Cursor Position Read (T2A workflow)


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
        
        case AsyncState::ReadingClipBounds:
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
                juce::Logger::writeToLog ("ERROR: Clip bounds read timed out after " + 
                                          juce::String (PTSL_TIMEOUT_MS) + "ms");
                
                stopTimer();
                ptslProcess->kill();
                ptslProcess.reset();
                currentAsyncState = AsyncState::Idle;
                
                juce::Logger::writeToLog ("Clip bounds read timed out - aborting");
                
                renderButton.setEnabled (true);
                renderButton.setButtonText ("Render Audio");
                return;
            }
            
            // Check if process is still running
            if (ptslProcess->isRunning())
            {
                // Still running - keep waiting (non-blocking)
                return;
            }
            
            // Process finished! Read output
            juce::Logger::writeToLog ("Clip bounds read finished after " + 
                                      juce::String (elapsed.inMilliseconds()) + "ms");
            
            auto output = ptslProcess->readAllProcessOutput();
            ptslProcess.reset();
            
            // Pass output to handler for parsing
            handleClipBoundsResult (output);
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
        float inSeconds = (float) (double) obj->getProperty ("in_seconds");
        float outSeconds = (float) (double) obj->getProperty ("out_seconds");
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
        
        // Store timeline selection for audio import
        timelineInTime = inTime;
        timelineInSeconds = inSeconds;
        timelineOutSeconds = outSeconds;
        juce::Logger::writeToLog ("Stored timeline in-time: " + timelineInTime);
        juce::Logger::writeToLog ("Stored timeline in/out seconds: " + juce::String (inSeconds) + "s - " + juce::String (outSeconds) + "s");
        
        //======================================================================
        // T2A MODE: Skip video processing, start generation directly
        //======================================================================
        if (isT2AMode)
        {
            juce::Logger::writeToLog ("=== T2A Mode: Starting text-only audio generation ===");
            juce::Logger::writeToLog ("Duration: " + juce::String (t2aDuration, 1) + "s");
            juce::Logger::writeToLog ("Import position: " + inTime + " (" + juce::String (inSeconds, 2) + "s)");
            juce::Logger::writeToLog ("Prompt: " + prompt.getText());
            
            renderButton.setButtonText ("Generating Audio...");
            
            // Start T2A generation (no video processing needed)
            startT2AAudioGeneration (prompt.getText(), t2aDuration);
            return;  // Exit here - T2A workflow complete
        }
        
        //======================================================================
        // V2A MODE: Continue with video processing
        //======================================================================
        juce::Logger::writeToLog ("=== V2A Mode: Processing video clip ===");
        
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
        // Step 3.5: Check if clip is trimmed (compare clip duration vs source duration)
        //======================================================================
        // Get source video duration via FFprobe
        float sourceVideoDuration = getSourceVideoDuration (videoPath);
        
        if (sourceVideoDuration > 0.0f)
        {
            juce::Logger::writeToLog ("Source video duration: " + juce::String (sourceVideoDuration, 2) + "s");
            juce::Logger::writeToLog ("Clip duration: " + juce::String (durationSeconds, 2) + "s");
            
            // Check if clip is trimmed (with 0.5s tolerance for rounding)
            if (durationSeconds < (sourceVideoDuration - 0.5f))
            {
                juce::Logger::writeToLog ("→ Clip is TRIMMED (shorter than source)");
                clipIsTrimmed = true;
            }
            else
            {
                juce::Logger::writeToLog ("→ Clip uses FULL source video");
                clipIsTrimmed = false;
            }
        }
        else
        {
            juce::Logger::writeToLog ("WARNING: Could not determine source video duration, assuming not trimmed");
            clipIsTrimmed = false;
        }
        
        //======================================================================
        // Step 4: Determine trimming workflow
        //======================================================================
        // Check user preferences (deprecated TODO remove in future)
        // juce::String manualOffset = videoOffsetInput.getText().trim();
        // bool hasManualOffset = manualOffset.isNotEmpty();
        
        // Decision logic (priority order):
        // 1. Manual offset + trimmed clip → Read clip bounds FIRST (needed for calculation), then use manual offset
        // 2. Manual offset + untrimmed clip → Use manual offset directly (no clip bounds needed)
        // 3. Auto-detect (trimmed, no manual offset) → Read clip bounds
        // 4. Full video (untrimmed, no manual offset) → Use entire source
        
        // if (hasManualOffset && clipIsTrimmed) (deprecated TODO remove in future)
        //{
            // SPECIAL CASE: Manual offset on trimmed clip
            // Need to read clip bounds first for correct calculation in Python
            // Formula: source_start = clip_source_start + (timeline_pos - clip_timeline_start)
           // juce::Logger::writeToLog ("Manual offset on TRIMMED clip - reading clip bounds first...");
            // juce::Logger::writeToLog ("Manual offset value: " + manualOffset);
            
            // renderButton.setButtonText ("Reading Clip Bounds...");
            
            // Store video and prompt for later use (after clip bounds are read)
            // currentVideoPath = videoPath;
            // currentPrompt = prompt.getText();
            
            // Start async clip bounds reading
            // handleClipBoundsResult() will detect manual offset and proceed accordingly
            // startClipBoundsRead (videoPath);
            // return;  // Exit here, will continue in handleClipBoundsResult()
        //}
        // else if (hasManualOffset) (deprecated TODO remove in future)
        // {
            // Manual offset on UNTRIMMED clip - can proceed directly
           //  juce::Logger::writeToLog ("Using manual offset workflow (untrimmed): " + manualOffset);
            
            // CRITICAL: Reset clip bounds from previous renders
            // Otherwise old clip bounds will be passed to Python instead of manual offset
            // clipStartSeconds = -1.0f;
            // clipEndSeconds = -1.0f;
        // }
        if (clipIsTrimmed)
        {
            juce::Logger::writeToLog ("Clip is trimmed - automatically reading clip bounds from Pro Tools...");
        
            
            renderButton.setButtonText ("Reading Clip Bounds...");
            
            // Store video and prompt for later use (after clip bounds are read)
            currentVideoPath = videoPath;
            currentPrompt = prompt.getText();
            
            // Start async clip bounds reading
            // The timer will continue running, and handleClipBoundsResult() will proceed to generation
            startClipBoundsRead (videoPath);
            return;  // Exit here, will continue in handleClipBoundsResult()
        }
        else
        {
            // Clip not trimmed and no manual offset - use full video
            juce::Logger::writeToLog ("Clip not trimmed - using full source video");
            
            // CRITICAL: Reset clip bounds from previous renders
            // Otherwise old clip bounds will be passed to Python instead of using full video
            clipStartSeconds = -1.0f;
            clipEndSeconds = -1.0f;
        }
        
        //======================================================================
        // Step 5: Start async audio generation (NO PTSL import yet!)
        //======================================================================
        renderButton.setButtonText ("Generating Audio...");
        juce::Logger::writeToLog ("Starting async audio generation...");
        juce::Logger::writeToLog ("Selected clip: " + inTime + " - " + outTime);
        juce::Logger::writeToLog ("Duration: " + juce::String (durationSeconds, 2) + "s");
        juce::Logger::writeToLog ("Video file: " + videoPath);
        juce::Logger::writeToLog ("Prompt: " + prompt.getText());
        
        // Start async audio generation (will poll for output file)
        startAudioGeneration (videoPath, prompt.getText());
        
        // Log generation start (no modal popup to avoid freezing)
        juce::Logger::writeToLog ("✅ Audio generation process started");
        juce::Logger::writeToLog ("    Selection: " + inTime + " - " + outTime);
        juce::Logger::writeToLog ("    Duration: " + juce::String (durationSeconds, 1) + "s");
        juce::Logger::writeToLog ("    Plugin will poll for completion and auto-import");
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
    
    // Get video clip offset from UI (if specified) (deprecated TODO remove in future)
    // juce::String videoOffset = videoOffsetInput.getText().trim();
    
    // Log clip bounds status
    if (clipStartSeconds >= 0.0f && clipEndSeconds >= 0.0f)
    {
        juce::Logger::writeToLog ("Using clip bounds: " + juce::String (clipStartSeconds, 3) + "s - " + juce::String (clipEndSeconds, 3) + "s");
    }
    // else if (videoOffset.isNotEmpty()) (deprecated TODO remove in future)
    // {
      //  juce::Logger::writeToLog ("Using manual video offset: " + videoOffset);
    // }
    else
    {
        juce::Logger::writeToLog ("No trimming - using full video");
    }
    
    // Read advanced parameters from UI
    juce::String negativePrompt = negativePromptInput.getText().trim();
    if (negativePrompt.isEmpty())
        negativePrompt = "voices, music";  // Default if empty
    
    juce::String seedText = seedInput.getText().trim();
    int seed = seedText.isEmpty() ? 42 : seedText.getIntValue();  // Default seed = 42
    
    // bool useHighPrecision = highPrecisionModeToggle.getToggleState(); // (deprecated TODO remove in future)
    
    // Read model selection from UI
    int providerSelectedId = modelProviderComboBox.getSelectedId();
    PtV2AProcessor::ModelProvider modelProvider = (providerSelectedId == 2) 
        ? PtV2AProcessor::ModelProvider::HunyuanVideoFoley 
        : PtV2AProcessor::ModelProvider::MMAudio;
    
    juce::String modelSize = modelSizeComboBox.getText();
    
    juce::String providerName = (modelProvider == PtV2AProcessor::ModelProvider::MMAudio) ? "MMAudio" : "HunyuanVideo-Foley";
    juce::Logger::writeToLog ("Model selection: " + providerName + " / " + modelSize);
    juce::Logger::writeToLog ("Advanced params: negative_prompt=\"" + negativePrompt + "\", seed=" + juce::String(seed) + ", full_precision=" + "false");
    
    // Call processor to start generation (returns immediately with expected output path)
    juce::String errorMessage;
    expectedAudioOutputPath = processor.generateAudioFromVideo (
        juce::File (videoPath),
        promptText,
        negativePrompt,  // User-controlled negative prompt
        seed,            // User-controlled seed
        modelProvider,   // NEW: Selected model provider (MMAudio / HunyuanVideo-Foley)
        modelSize,       // NEW: Selected model size
        "",              // No manual offset (deprecated TODO remove in future),
        timelineInSeconds,
        timelineOutSeconds,
        false,            // autoDetectClipBounds = false (legacy workflow, causes deadlock)
        clipStartSeconds, // Pass clip bounds if available
        clipEndSeconds,   // Pass clip bounds if available
        false,            // High precision mode flag (deprecated TODO remove in future)
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

//==============================================================================
// Async T2A Audio Generation (text-only, no video)
//==============================================================================

void PtV2AEditor::startT2AAudioGeneration (const juce::String& promptText, float duration)
{
    juce::Logger::writeToLog ("=== Starting T2A Audio Generation (text-only) ===");
    juce::Logger::writeToLog ("Prompt: " + promptText);
    juce::Logger::writeToLog ("Duration: " + juce::String (duration, 1) + "s");
    
    // Store parameters
    currentPrompt = promptText;
    
    // Read advanced parameters from UI
    juce::String negativePrompt = negativePromptInput.getText().trim();
    if (negativePrompt.isEmpty())
        negativePrompt = "voices, music";  // Default if empty
    
    juce::String seedText = seedInput.getText().trim();
    int seed = seedText.isEmpty() ? 42 : seedText.getIntValue();
    
    // T2A only supports MMAudio
    PtV2AProcessor::ModelProvider modelProvider = PtV2AProcessor::ModelProvider::MMAudio;
    juce::String modelSize = modelSizeComboBox.getText();
    
    juce::Logger::writeToLog ("Model: MMAudio / " + modelSize);
    juce::Logger::writeToLog ("Advanced params: negative_prompt=\"" + negativePrompt + "\", seed=" + juce::String(seed));
    
    // Call processor to start T2A generation (no video, just text + duration)
    juce::String errorMessage;
    expectedAudioOutputPath = processor.generateAudioTextOnly (
        promptText,
        duration,
        negativePrompt,
        seed,
        modelSize,
        &errorMessage
    );
    
    if (expectedAudioOutputPath.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: Failed to start T2A generation: " + errorMessage);
        
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Generation Failed",
            "Failed to start T2A audio generation:\n\n" + errorMessage,
            "OK"
        );
        
        renderButton.setEnabled (true);
        renderButton.setButtonText ("Render Audio");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    juce::Logger::writeToLog ("Expected T2A output: " + expectedAudioOutputPath);
    juce::Logger::writeToLog ("Starting polling for output file...");
    
    // Switch to GeneratingAudio state (same polling as V2A)
    currentAsyncState = AsyncState::GeneratingAudio;
    asyncOperationStartTime = juce::Time::getCurrentTime();
    
    // Start timer to poll for output file
    if (!isTimerRunning())
        startTimer (TIMER_INTERVAL_MS);
}

//==============================================================================
// Clip Bounds Reading (Async Phase 1 of Auto-Trim Workflow)
//==============================================================================

void PtV2AEditor::startClipBoundsRead (const juce::String& videoPath)
{
    // Phase 1: Read clip boundaries asynchronously via PTSL
    // This is SAFE because it doesn't block the main thread
    // Once complete, handleClipBoundsResult() will store bounds and proceed to generation
    
    juce::Logger::writeToLog ("=== Starting Async Clip Bounds Read ===");
    juce::Logger::writeToLog ("Video: " + videoPath);
    
    // Store video path for later use in Phase 2
    currentVideoPath = videoPath;
    
    // Get Python executable and script
    juce::String pythonExe = processor.getPythonExecutable();
    juce::File scriptFile = processor.getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: API client script not found");
        juce::Logger::writeToLog ("Clip bounds read aborted - script not found");
        return;
    }
    
    // Build command: python -X utf8 standalone_api_client.py --action get_clip_bounds
    juce::StringArray commandArray;
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");
    commandArray.add (scriptFile.getFullPathName());
    commandArray.add ("--action");
    commandArray.add ("get_clip_bounds");
    
    juce::String command = commandArray.joinIntoString (" ");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Start async process
    ptslProcess = std::make_unique<juce::ChildProcess>();
    if (!ptslProcess->start (commandArray))
    {
        juce::Logger::writeToLog ("ERROR: Failed to start clip bounds process");
        juce::Logger::writeToLog ("Clip bounds read aborted");
        ptslProcess.reset();
        return;
    }
    
    // Update state and reset timer
    currentAsyncState = AsyncState::ReadingClipBounds;
    asyncOperationStartTime = juce::Time::getCurrentTime();
    juce::Logger::writeToLog ("Reading clip boundaries... (async)");
    
    // Timer should already be running from timeline selection
    // If not (called standalone), start it
    if (!isTimerRunning())
        startTimer (TIMER_INTERVAL_MS);
    
    juce::Logger::writeToLog ("Clip bounds read started (async)");
}

void PtV2AEditor::handleClipBoundsResult (const juce::String& output)
{
    // Phase 1 complete: Parse JSON output with clip boundaries
    juce::Logger::writeToLog ("=== Handling Clip Bounds Result ===");
    juce::Logger::writeToLog ("Output: " + output);
    
    // Extract JSON from output (last JSON line)
    auto lines = juce::StringArray::fromLines (output);
    juce::String jsonOutput;
    for (const auto& line : lines)
    {
        if (line.trimStart().startsWith ("{"))
        {
            jsonOutput = line.trim();
        }
    }
    
    if (jsonOutput.isEmpty())
    {
        juce::Logger::writeToLog ("ERROR: No JSON in clip bounds output");
        juce::Logger::writeToLog ("Failed to parse clip bounds - aborting");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    // Parse JSON: {"success": true, "start_seconds": 5.005, "end_seconds": 10.844, ...}
    auto jsonResult = juce::JSON::parse (jsonOutput);
    if (jsonResult.isVoid())
    {
        juce::Logger::writeToLog ("ERROR: Failed to parse clip bounds JSON");
        juce::Logger::writeToLog ("Invalid clip bounds response - aborting");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    bool success = jsonResult.getProperty ("success", false);
    if (!success)
    {
        juce::String error = jsonResult.getProperty ("error", "Unknown error");
        juce::Logger::writeToLog ("ERROR: Clip bounds read failed: " + error);
        juce::Logger::writeToLog ("Aborting clip bounds read");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    // Extract clip boundaries
    clipStartSeconds = (float) jsonResult.getProperty ("start_seconds", -1.0);
    clipEndSeconds = (float) jsonResult.getProperty ("end_seconds", -1.0);
    
    juce::Logger::writeToLog ("✅ Clip bounds read successfully");
    juce::Logger::writeToLog ("  Start: " + juce::String (clipStartSeconds, 3) + "s");
    juce::Logger::writeToLog ("  End: " + juce::String (clipEndSeconds, 3) + "s");
    
    if (clipStartSeconds < 0.0f || clipEndSeconds < 0.0f)
    {
        juce::Logger::writeToLog ("ERROR: Invalid clip bounds");
        juce::Logger::writeToLog ("Clip boundaries are invalid - aborting");
        currentAsyncState = AsyncState::Idle;
        return;
    }
    
    // Phase 1 complete! Now proceed to Phase 2: Background generation with clip bounds
    currentAsyncState = AsyncState::Idle;  // Reset before starting generation
    juce::Logger::writeToLog ("Proceeding to audio generation with clip bounds");
    
    // Start audio generation (will use clipStartSeconds and clipEndSeconds)
    startAudioGeneration (currentVideoPath, currentPrompt); 
    renderButton.setButtonText ("Generating Audio...");
}

//==============================================================================
// Source Video Duration Check
//==============================================================================

float PtV2AEditor::getSourceVideoDuration (const juce::String& videoPath)
{
    // Use Python script to call FFprobe and get video duration
    juce::Logger::writeToLog ("=== Checking Source Video Duration ===");
    juce::Logger::writeToLog ("Video: " + videoPath);
    
    // Get Python executable path
    juce::String pythonExe = processor.getPythonExecutable();
    juce::Logger::writeToLog ("Python: " + pythonExe);
    
    // Get API client script
    juce::File scriptFile = processor.getAPIClientScript();
    if (!scriptFile.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: API client script not found at: " + scriptFile.getFullPathName());
        return 0.0f;
    }
    
    juce::Logger::writeToLog ("Script: " + scriptFile.getFullPathName());
    
    // Build command: python standalone_api_client.py --action get_duration --video "path"
    juce::StringArray args;
    args.add (pythonExe);
    args.add ("-X");
    args.add ("utf8");
    args.add (scriptFile.getFullPathName());
    args.add ("--action");
    args.add ("get_duration");
    args.add ("--video");
    args.add (videoPath);
    
    juce::Logger::writeToLog ("Command: " + args.joinIntoString (" "));
    
    // Execute command (synchronous, should be fast)
    juce::ChildProcess process;
    if (!process.start (args))
    {
        juce::Logger::writeToLog ("ERROR: Failed to start Python process");
        return 0.0f;
    }
    
    // Wait for completion (max 5 seconds)
    bool finished = process.waitForProcessToFinish (5000);
    if (!finished)
    {
        juce::Logger::writeToLog ("ERROR: FFprobe check timed out");
        process.kill();
        return 0.0f;
    }
    
    // Read output
    juce::String output = process.readAllProcessOutput();
    juce::Logger::writeToLog ("FFprobe output: " + output);
    
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
        juce::Logger::writeToLog ("ERROR: No JSON found in output");
        return 0.0f;
    }
    
    // Parse JSON response: {"success": true, "duration": 60.0}
    auto jsonResult = juce::JSON::parse (jsonOutput);
    if (jsonResult.isVoid())
    {
        juce::Logger::writeToLog ("ERROR: Failed to parse JSON response");
        return 0.0f;
    }
    
    bool success = jsonResult.getProperty ("success", false);
    if (!success)
    {
        juce::String error = jsonResult.getProperty ("error", "Unknown error");
        juce::Logger::writeToLog ("ERROR: FFprobe failed: " + error);
        return 0.0f;
    }
    
    float duration = (float) jsonResult.getProperty ("duration", 0.0);
    juce::Logger::writeToLog ("Source video duration: " + juce::String (duration, 2) + "s");
    
    return duration;
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
    
    // expectedAudioOutputPath now contains the OUTPUT DIRECTORY (not specific file)
    // Find the newest .wav file in that directory
    juce::File outputDir (expectedAudioOutputPath);
    
    if (!outputDir.isDirectory())
    {
        // Fallback: treat as file path (backwards compatibility)
        juce::File outputFile (expectedAudioOutputPath);
        if (!outputFile.existsAsFile())
        {
            // Still generating, keep waiting
            int64_t elapsedMs = elapsed.inMilliseconds();
            if (elapsedMs > 0 && (elapsedMs / 1000) % 10 == 0 && (elapsedMs % 1000) < TIMER_INTERVAL_MS)
            {
                juce::Logger::writeToLog ("Still generating... (" + 
                                          juce::String (elapsedMs / 1000) + "s elapsed)");
            }
            return;
        }
        
        // File found! (backwards compatibility path)
        juce::Logger::writeToLog ("✓ Audio generation complete after " + 
                                  juce::String (elapsed.inSeconds()) + "s");
        juce::Logger::writeToLog ("Output file: " + expectedAudioOutputPath);
        
        stopTimer();
        renderButton.setButtonText ("Importing Audio...");
        startAudioImport (expectedAudioOutputPath);
        return;
    }
    
    // Find newest .wav file in directory (server-generated filename with prompt snippet)
    juce::File newestWavFile;
    juce::Time newestTime;
    
    for (auto& entry : juce::RangedDirectoryIterator (outputDir, false, "*.wav"))
    {
        auto file = entry.getFile();
        auto modTime = file.getLastModificationTime();
        
        if (newestWavFile == juce::File() || modTime > newestTime)
        {
            newestWavFile = file;
            newestTime = modTime;
        }
    }
    
    // Check if a wav file was created after we started generation
    if (newestWavFile != juce::File() && newestTime >= asyncOperationStartTime)
    {
        // Found the generated audio file!
        juce::Logger::writeToLog ("✓ Audio generation complete after " + 
                                  juce::String (elapsed.inSeconds()) + "s");
        juce::Logger::writeToLog ("Output file: " + newestWavFile.getFullPathName());
        juce::Logger::writeToLog ("Filename: " + newestWavFile.getFileName());
        
        stopTimer();
        renderButton.setButtonText ("Importing Audio...");
        startAudioImport (newestWavFile.getFullPathName());
    }
    else
    {
        // Still generating, keep waiting
        int64_t elapsedMs = elapsed.inMilliseconds();
        if (elapsedMs > 0 && (elapsedMs / 1000) % 10 == 0 && (elapsedMs % 1000) < TIMER_INTERVAL_MS)
        {
            juce::Logger::writeToLog ("Still generating... (" + 
                                      juce::String (elapsedMs / 1000) + "s elapsed)");
        }
    }
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
    
    // Add timecode position (same for both T2A and V2A - uses timeline selection start)
    juce::String importTimecode = timelineInTime;
    if (importTimecode.isNotEmpty())
    {
        juce::String modeLabel = isT2AMode ? "T2A" : "V2A";
        juce::Logger::writeToLog ("Import position (" + modeLabel + "): " + importTimecode);
    }
    
    if (importTimecode.isNotEmpty())
    {
        commandArray.add ("--timecode");
        commandArray.add (importTimecode);
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
            "-> Generation complete\n"
            "-> Imported to timeline\n\n"
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

//==============================================================================
// Generation Mode Change Handler (V2A <-> T2A)
//==============================================================================
void PtV2AEditor::handleGenerationModeChange()
{
    isT2AMode = t2aModeButton.getToggleState();
    
    juce::Logger::writeToLog ("=== Generation Mode Changed ===");
    juce::Logger::writeToLog ("New mode: " + juce::String (isT2AMode ? "T2A (Text Only)" : "V2A (Video-to-Audio)"));
    
    // Duration controls: only enabled in T2A mode
    durationComboBox.setEnabled (isT2AMode);
    durationLabel.setEnabled (isT2AMode);
    
    // Model provider handling: HunyuanVideo-Foley not compatible with T2A
    if (isT2AMode)
    {
        // If HunyuanVideo-Foley is selected, switch to MMAudio
        if (modelProviderComboBox.getSelectedId() == 2)  // HunyuanVideo-Foley
        {
            juce::Logger::writeToLog ("T2A mode: Switching from HunyuanVideo-Foley to MMAudio");
            modelProviderComboBox.setSelectedId (1, juce::sendNotification);  // Switch to MMAudio
        }
        
        // T2A mode: Lock model provider to MMAudio only (disable dropdown)
        modelProviderComboBox.setEnabled (false);
        juce::Logger::writeToLog ("T2A mode: Model provider locked to MMAudio");
    }
    else
    {
        // V2A mode: Enable model provider dropdown for user selection
        modelProviderComboBox.setEnabled (true);
        juce::Logger::writeToLog ("V2A mode: Model provider dropdown enabled");
    }
    
    repaint();
}

//==============================================================================
// Model Selection Handler
//==============================================================================
void PtV2AEditor::handleModelProviderChange()
{
    // Get selected provider (1=MMAudio, 2=HunyuanVideo-Foley)
    int selectedId = modelProviderComboBox.getSelectedId();
    
    // Clear current model size options
    modelSizeComboBox.clear();
    
    if (selectedId == 1)  // MMAudio
    {
        // MMAudio model sizes
        modelSizeComboBox.addItem ("Large", 1);
        modelSizeComboBox.addItem ("Medium", 2);
        modelSizeComboBox.addItem ("Small", 3);
        modelSizeComboBox.setSelectedId (1, juce::dontSendNotification);  // Default: Large
        
        juce::Logger::writeToLog ("Model provider changed to: MMAudio");
    }
    else if (selectedId == 2)  // HunyuanVideo-Foley
    {
        // HunyuanVideo-Foley model sizes
        modelSizeComboBox.addItem ("XXL", 1);
        modelSizeComboBox.addItem ("XL", 2);
        modelSizeComboBox.setSelectedId (2, juce::dontSendNotification);  // Default: XL
        
        juce::Logger::writeToLog ("Model provider changed to: HunyuanVideo-Foley");
    }
}

//==============================================================================
// Cloudflare Access Credential Dialog
//==============================================================================
void PtV2AEditor::showCredentialDialog()
{
    // Create AlertWindow on heap for async modal state
    auto* credentialWindow = new juce::AlertWindow (
        "API Access Credentials",
        "Enter your Access Token Secret.\n\n"
        "Do not change the Client ID unless advised.\n"
        "You can test the API connection before saving.",
        juce::MessageBoxIconType::NoIcon
    );
    
    // Pre-fill with current values
    auto currentId = processor.getCloudflareClientId();
    auto currentSecret = processor.getCloudflareClientSecret();
    
    // Client ID: Read-only, pre-filled (user doesn't need to change this)
    credentialWindow->addTextEditor ("clientId", currentId, "Client ID:");
    if (auto* idEditor = credentialWindow->getTextEditor ("clientId"))
    {
        idEditor->setReadOnly (false);
       // idEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colours::lightgrey);
    }
    
    // Client Secret: Editable, this is what user needs to input
    credentialWindow->addTextEditor ("clientSecret", currentSecret, "Client Secret:");
    
    credentialWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    credentialWindow->addButton ("Test Connection", 2);
    credentialWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    
    // Use async modal state (non-blocking, Pro Tools safe!)
    credentialWindow->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, credentialWindow] (int result)
        {
            if (result == 2)  // Test Connection
            {
                auto testId = credentialWindow->getTextEditorContents ("clientId");
                auto testSecret = credentialWindow->getTextEditorContents ("clientSecret");
                
                renderButton.setButtonText ("Testing...");
                renderButton.setEnabled (false);
                
                juce::String error;
                bool valid = processor.testCloudflareCredentials (testId, testSecret, &error);
                
                renderButton.setButtonText ("Render Audio");
                renderButton.setEnabled (true);
                
                if (valid)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::InfoIcon,
                        "Connection Successful",
                        "Credentials are valid! Save them for API Access.",
                        "OK"
                    );
                    
                    // Show dialog again to let user save
                    juce::Timer::callAfterDelay (100, [this] { showCredentialDialog(); });
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Connection Failed",
                        "Could not connect to API:\n\n" + error,
                        "OK"
                    );
                    
                    // Show dialog again
                    juce::Timer::callAfterDelay (100, [this] { showCredentialDialog(); });
                }
            }
            else if (result == 1)  // Save
            {
                auto newId = credentialWindow->getTextEditorContents ("clientId");
                auto newSecret = credentialWindow->getTextEditorContents ("clientSecret");
                
                if (processor.saveCloudflareCredentials (newId, newSecret))
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::InfoIcon,
                        "Credentials Saved",
                        "API Access credentials saved successfully.",
                        "OK"
                    );
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Save Failed",
                        "Could not save credentials.\n"
                        "Check file permissions.",
                        "OK"
                    );
                }
            }
            
            // JUCE will delete the window automatically
        }
    ), true);
}