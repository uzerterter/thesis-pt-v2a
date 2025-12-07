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
    // juce::TextButton renderDummyButton { "Render (dummy video)" }; // (deprecated TODO remove in future)
    
    /**
     * Button to open log file in default text editor
     * Useful for debugging and viewing generation history
     */
    juce::TextButton openLogButton { "Open Log" };

    /**
     * Button to open Cloudflare Access settings dialog
     * Allows user to enter CF Access credentials for secure API access
     */
    juce::TextButton settingsButton { "API Settings" };
    
    /**
     * Label for video clip offset input (deprecated TODO remove in future)
     */
   // juce::Label videoOffsetLabel { {}, "Video Clip Start (to be removed):" };
    
    /**
     * Text input for video clip start position on timeline
     * Format: Timecode (e.g., "00:02" or "00:00:02:00")
     * 
     * Purpose:
     *   When user wants to render only part of a video clip, they need to
     *   specify where the video clip starts on the Pro Tools timeline.
     *   
     * Example:
     *   - Video clip placed at 00:00:02:00 on timeline
     *   - User selects region from 00:00:05:00 to 00:00:12:00
     *   - User enters "00:02" in this field
     *   - System calculates: offset_in_video = 5 - 2 = 3 seconds
     *   - FFmpeg trims video: -ss 3 -t 7 (from second 3, duration 7)
     * 
     * Notes:
     *   - Leave empty if video clip starts at 00:00:00:00 (timeline beginning)
     *   - Format is flexible: "02", "00:02", "00:00:02:00" all work
     *   - Used only when trimming video with FFmpeg
     *   - System automatically detects trimmed clips when possible
     */
    // juce::TextEditor videoOffsetInput; (deprecated TODO remove in future)
    
    //==============================================================================
    // Advanced Generation Parameters
    //==============================================================================
        
    /**
     * Text input for audio generation prompt
     * Example prompts: "thunder and rain", "footsteps on wood", "car engine starting"
     */
    juce::TextEditor prompt;
    juce::Label promptLabel { {}, "Prompt:" };


    /**
     * Negative prompt input field
     * 
     * Specifies audio elements to avoid during generation (e.g., "voices, music").
     * Helps guide the model away from unwanted audio characteristics.
     * 
     * Default: "voices, music" (generates only sound effects)
     */
    juce::TextEditor negativePromptInput;
    juce::Label negativePromptLabel { {}, "Negative:" };
    
    /**
     * Random seed input field
     * 
     * Controls randomness in audio generation. Same seed = reproducible results.
     * 
     * Default: 42
     * Format: Integer (e.g., "42", "12345")
     */
    juce::TextEditor seedInput;
    juce::Label seedLabel { {}, "Seed:" };

    // Choice between V2A and T2A generation modes:
    juce::ToggleButton v2aModeButton { "V2A (from Video)" };
    juce::ToggleButton t2aModeButton { "T2A (Text Only)" };
    juce::Label durationLabel { {}, "Duration:" };
    juce::ComboBox durationComboBox;       
    /**
     * High precision mode toggle (deprecated TODO remove in future)
     * 
     * When enabled:
     *   - Uses torch.float32 (higher quality, slower, more memory)
     * When disabled:
     *   - Uses torch.bfloat16 (default, faster, less memory)
     * 
     * Default: Off (bfloat16)
     * 
     * Note: With RTX A6000 (48GB VRAM), float32 is recommended for production use
     */
    // juce::ToggleButton highPrecisionModeToggle { "High Precision Mode (float32)" };
    
    //==============================================================================
    // Model Selection
    //==============================================================================
    
    /**
     * Model provider selection dropdown
     * Options: "MMAudio", "HunyuanVideo-Foley"
     * 
     * Determines which API to use:
     *   - MMAudio: Port 8000, 16kHz/44.1kHz, general audio generation
     *   - HunyuanVideo-Foley: Port 8001, 48kHz, professional Foley sounds
     */
    juce::Label modelLabel { {}, "Model:" };
    juce::ComboBox modelProviderComboBox;
    
    /**
     * Model size selection dropdown
     * Options depend on selected provider:
     *   - MMAudio: "Large (44.1kHz)", "Medium (44.1kHz)", "Small (16kHz)"
     *   - HunyuanVideo-Foley: "XL (8-12GB)", "XXL (16-20GB)"
     * 
     * Dynamically updated when provider changes
     */
    juce::Label modelSizeLabel { {}, "Size:" };
    juce::ComboBox modelSizeComboBox;
    
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
    
    /**
     * Handle model provider ComboBox change
     * Updates model size ComboBox options based on selected provider
     */
    void handleModelProviderChange();
    
    /**
     * Handle generation mode change (V2A <-> T2A)
     * Updates UI state: enables/disables duration selection and model provider
     */
    void handleGenerationModeChange();
    
    /**
     * Handle T2A render button click - text-to-audio workflow without video
     * 
     * Steps:
     *   1. Parse duration from dropdown (e.g., "8s" -> 8.0f)
     *   2. Check API availability
     *   3. Read timeline selection from Pro Tools
     *   4. Generate audio via T2A API (no video input)
     *   5. Import generated audio at timeline selection start
     * 
     * @note Now uses timeline selection like V2A for consistent behavior
     */
    void handleT2ARenderButtonClicked();
    
    /**
     * Show Cloudflare Access credential dialog
     * Allows user to enter Client ID and Client Secret for secure API access
     * Saves credentials to config.json file in plugin bundle
     */
    void showCredentialDialog();


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
     * Start async timeline selection read (V2A workflow - includes video clip check)
     * Launches PTSL process with get_video_info and starts timer for polling
     */
    void startTimelineSelectionRead();
    
    /**
     * Start async timeline selection read (T2A workflow - timeline only, no video)
     * Launches PTSL process with get_timeline_selection and starts timer for polling
     */
    void startTimelineSelectionReadOnly();
    
    /**
     * Handle timeline selection result (called from timer callback)
     * @param output Raw output from PTSL process (JSON string)
     */
    void handleTimelineSelectionResult (const juce::String& output);
    
    /**
     * Start async audio generation and polling (V2A workflow)
     * Launches Python process and starts timer to poll for output file
     * @param videoPath Path to video file
     * @param promptText User's audio prompt
     */
    void startAudioGeneration (const juce::String& videoPath, const juce::String& promptText);
    
    /**
     * Start async T2A audio generation (text-only, no video)
     * Launches Python process and starts timer to poll for output file
     * @param promptText User's audio prompt
     * @param duration Duration in seconds (4-12s)
     */
    void startT2AAudioGeneration (const juce::String& promptText, float duration);
    
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
    
    /**
     * Start async clip bounds reading via PTSL (Phase 1 of auto-trim workflow).
     * Launches Python script with --action get_clip_bounds to read clip boundaries.
     * Returns quickly - actual result handling happens in handleClipBoundsResult().
     * 
     * @param videoPath Path to source video file (stored for later generation)
     */
    void startClipBoundsRead (const juce::String& videoPath);
    
    /**
     * Handle clip bounds result (called from timer callback).
     * Parses JSON output with clip boundaries and stores in member variables.
     * Then proceeds to Phase 2: background audio generation with clip bounds.
     * 
     * @param output Raw JSON output from Python get_clip_bounds action
     */
    void handleClipBoundsResult (const juce::String& output);
    
    /**
     * Get source video file duration via FFprobe
     * @param videoPath Path to video file
     * @return Duration in seconds, or 0.0f if failed
     */
    float getSourceVideoDuration (const juce::String& videoPath);
    
    //==============================================================================
    // Async State
    //==============================================================================
    
    /** Current async operation state */
    enum class AsyncState
    {
        Idle,                    // No operation in progress
        ReadingTimeline,         // Reading timeline selection via PTSL
        ReadingClipBounds,       // Reading clip boundaries via PTSL (for auto-trim detection)
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
    
    /** Timeline selection in seconds (for video trimming calculations) */
    float timelineInSeconds = 0.0f;
    float timelineOutSeconds = 0.0f;
    
    /** Clip boundaries in seconds (for auto-trim workflow) */
    float clipStartSeconds = -1.0f;  // -1 = not set
    float clipEndSeconds = -1.0f;    // -1 = not set
    
    /** Flag indicating if video clip is trimmed (shorter than source) */
    bool clipIsTrimmed = false;
    
    /** Current generation mode: true = T2A (text-only), false = V2A (video-to-audio) */
    bool isT2AMode = false;
    
    /** T2A duration selected by user (4-12s) */
    float t2aDuration = 8.0f;
    
    /** Timeout for PTSL calls (milliseconds) */
    static constexpr int PTSL_TIMEOUT_MS = 10000;  // 10 seconds
    
    /** Timeout for audio generation (milliseconds) */
    static constexpr int GENERATION_TIMEOUT_MS = 120000;  // 2 minutes
    
    /** Timer polling interval (milliseconds) */
    static constexpr int TIMER_INTERVAL_MS = 100;  // Check every 100ms

    //==============================================================================
    // JUCE Leak Detector (Debug builds only)
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AEditor)
};
