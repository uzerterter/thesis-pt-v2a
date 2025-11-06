#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Forward declaration to avoid circular include
// (PluginProcessor.h already includes this file)
class PtV2AProcessor;

/**
 * @class PtV2AEditor
 * @brief Plugin GUI displayed in Pro Tools
 * 
 * This is the visual interface that appears when user opens the plugin in Pro Tools.
 * 
 * Current UI Elements (Phase 1 - Prototype):
 *   - Text input for audio generation prompt
 *   - "Render Audio" button to trigger generation
 * 
 * Workflow:
 *   1. User enters text prompt (e.g., "thunder and rain")
 *   2. User clicks "Render Audio"
 *   3. Editor validates API availability
 *   4. Editor reads timeline selection via PTSL (asynchronously!)
 *   5. Editor finds video file in Pro Tools session
 *   6. Editor calls processor.generateAudioFromVideo()
 *   7. Shows progress feedback via button text
 *   8. Displays success/error dialog
 * 
 * Async PTSL Communication:
 *   - Uses Timer to poll PTSL process (non-blocking!)
 *   - Keeps Pro Tools responsive during PTSL calls
 *   - Prevents deadlock when PTSL waits for Pro Tools response
 * 
 * @note This GUI is embedded directly in Pro Tools - NOT a standalone window
 */
class PtV2AEditor : public juce::AudioProcessorEditor,
                    private juce::Timer  // For async PTSL polling
{
public:
    //==============================================================================
    // Construction / Destruction
    //==============================================================================
    
    /**
     * Constructor - creates and initializes all GUI components
     * @param p Reference to the processor (provides business logic)
     */
    explicit PtV2AEditor (PtV2AProcessor& p);
    
    /**
     * Destructor - JUCE handles component cleanup automatically
     */
    ~PtV2AEditor() override = default;

    //==============================================================================
    // JUCE Component Lifecycle
    //==============================================================================
    
    /**
     * Paint the component background
     * Called automatically by JUCE when component needs redrawing
     * @param g Graphics context for drawing
     */
    void paint (juce::Graphics& g) override;
    
    /**
     * Layout child components when window is resized
     * Called automatically by JUCE when component size changes
     * Sets positions and sizes of all UI elements (prompt, button, etc.)
     */
    void resized() override;

private:
    //==============================================================================
    // Member Variables
    //==============================================================================
    
    /** Reference to the processor (business logic and API calls) */
    PtV2AProcessor& processor;

    //==============================================================================
    // GUI Components
    //==============================================================================
    
    /**
     * Text input for audio generation prompt
     * Example prompts: "thunder and rain", "footsteps on wood", "car engine starting"
     */
    juce::TextEditor prompt;
    
    /**
     * Button to trigger audio generation
     * States:
     *   - "Render Audio" (default, ready)
     *   - "Checking API..." (validating connection)
     *   - "Generating..." (processing, disabled)
     * 
     * TODO Add cancel functionality for long operations
     */
    juce::TextButton renderButton { "Render Audio" };
    
    /**
     * Button to trigger audio generation with dummy video (for presentation)
     * Uses predefined test video file instead of Pro Tools timeline extraction
     * 
     * States:
     *   - "Render (dummy video)" (default, ready)
     *   - "Generating..." (processing, disabled)
     */
    juce::TextButton renderDummyButton { "Render (dummy video)" };
    
    /**
     * Button to open log file in default text editor
     * Useful for debugging and viewing generation history
     */
    juce::TextButton openLogButton { "Open Log" };
    
    /**
     * Label for video selection dropdown
     */
    juce::Label videoLabel { {}, "Select Video:" };
    
    /**
     * Dropdown to select which video to use for generation
     * Populated dynamically when videos are found in Pro Tools session
     * 
     * Usage:
     *   - Automatically populated when timeline selection is read
     *   - Shows filename of each video
     *   - User selects which video to generate audio for
     *   - First video selected by default
     */
    juce::ComboBox videoComboBox;
    
    /** Storage for available video file paths */
    juce::StringArray availableVideoPaths;
    
    //==============================================================================
    // Event Handlers
    //==============================================================================
    
    /** 
     * Handle render button click - main workflow entry point
     * 
     * Steps:
     *   1. Validate API availability (HTTP health check)
     *   2. Locate test video file (hardcoded for Phase 1)
     *   3. Call processor.generateAudioFromVideo() with prompt
     *   4. Update button state during processing
     *   5. Show success/error dialog
     * 
     * @note This is called on the GUI thread - processor handles async Python subprocess
     * 
     * TODO Phase 2: Replace hardcoded video path with FileChooser dialog
     * TODO Phase 3: Extract video from Pro Tools timeline instead of file selector
     */
    void handleRenderButtonClicked();
    
    /**
     * Handle render dummy button click - simplified workflow for presentation
     * 
     * Steps:
     *   1. Validate API availability
     *   2. Use predefined test video (sora_galloping.mp4)
     *   3. Generate audio and import to Pro Tools
     * 
     * @note Skips Pro Tools timeline extraction - uses hardcoded video path
     */
    void handleRenderDummyButtonClicked();
    
    /**
     * Handle open log button click
     * Opens the log file in the system's default text editor
     * Shows error if log file doesn't exist
     */
    void handleOpenLogButtonClicked();
    
    //==============================================================================
    // Async PTSL Communication
    //==============================================================================
    
    /**
     * Timer callback - checks PTSL process status every 100ms
     * 
     * This allows Pro Tools to remain responsive while waiting for PTSL.
     * When process finishes, reads output and continues workflow.
     * 
     * States:
     *   - Process running: Do nothing, Pro Tools stays responsive
     *   - Process finished: Read output, stop timer, continue workflow
     *   - Timeout: Kill process, show error
     */
    void timerCallback() override;
    
    /**
     * Start async timeline selection read
     * Launches PTSL process and starts timer for polling
     */
    void startTimelineSelectionRead();
    
    /**
     * Handle timeline selection result (called from timer callback)
     * @param output Raw output from PTSL process (JSON string)
     */
    void handleTimelineSelectionResult (const juce::String& output);
    
    /**
     * Start async audio generation and polling
     * Launches Python process and starts timer to poll for output file
     * @param videoPath Path to video file
     * @param promptText User's audio prompt
     */
    void startAudioGeneration (const juce::String& videoPath, const juce::String& promptText);
    
    /**
     * Check if audio generation is complete (output file exists)
     * Called from timer callback
     */
    void checkAudioGenerationComplete();
    
    /**
     * Import generated audio to Pro Tools (async with PTSL)
     * @param audioPath Path to generated audio file
     */
    void startAudioImport (const juce::String& audioPath);
    
    /**
     * Handle audio import result (called from timer callback)
     * @param output Raw output from PTSL import process
     */
    void handleAudioImportResult (const juce::String& output);
    
    //==============================================================================
    // Async State
    //==============================================================================
    
    /** Current async operation state */
    enum class AsyncState
    {
        Idle,                    // No operation in progress
        ReadingTimeline,         // Reading timeline selection via PTSL
        GeneratingAudio,         // Python generating audio (polling for output file)
        ImportingAudio          // Importing audio to Pro Tools via PTSL
    };
    
    AsyncState currentAsyncState = AsyncState::Idle;
    
    /** PTSL process handle (for async timeline selection and import) */
    std::unique_ptr<juce::ChildProcess> ptslProcess;
    
    /** Time when current async operation started (for timeout detection) */
    juce::Time asyncOperationStartTime;
    
    /** Expected output file path (for audio generation polling) */
    juce::String expectedAudioOutputPath;
    
    /** Video path and prompt (stored for generation process) */
    juce::String currentVideoPath;
    juce::String currentPrompt;
    
    /** Timeline selection timecode (stored for audio import positioning) */
    juce::String timelineInTime;  // e.g., "00:00:07:00"
    
    /** Timeout for PTSL calls (milliseconds) */
    static constexpr int PTSL_TIMEOUT_MS = 10000;  // 10 seconds
    
    /** Timeout for audio generation (milliseconds) */
    static constexpr int GENERATION_TIMEOUT_MS = 180000;  // 3 minutes
    
    /** Timer polling interval (milliseconds) */
    static constexpr int TIMER_INTERVAL_MS = 100;  // Check every 100ms

    //==============================================================================
    // JUCE Leak Detector (Debug builds only)
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AEditor)
};
