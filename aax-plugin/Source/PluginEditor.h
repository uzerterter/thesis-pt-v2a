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
 *   4. Editor finds test video file (hardcoded for Phase 1)
 *   5. Editor calls processor.generateAudioFromVideo()
 *   6. Shows progress feedback via button text
 *   7. Displays success/error dialog
 * 
 * Future Enhancements:
 *   - Video file selector dialog
 *   - Negative prompt input
 *   - Seed parameter control
 *   - Progress bar for generation status
 *   - Cancel button for long operations
 *   - Pro Tools timeline video extraction
 * 
 * @note This GUI is embedded directly in Pro Tools - NOT a standalone window
 */
class PtV2AEditor : public juce::AudioProcessorEditor
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
    // JUCE Leak Detector (Debug builds only)
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AEditor)
};
