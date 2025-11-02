#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @class PtV2AProcessor
 * @brief Pro Tools Video-to-Audio Plugin Processor
 * 
 * This AudioProcessor handles the core plugin logic:
 * - Audio pass-through (no real-time processing)
 * - Video-to-Audio generation via MMAudio API
 * - Integration with Pro Tools via PTSL (Pro Tools Scripting Library)
 * - Embedded Python runtime for API communication
 * 
 * Architecture:
 *   User Action (GUI) → generateAudioFromVideo() → Python subprocess → MMAudio API
 *   → Audio file → PTSL Import → Pro Tools Session
 */
class PtV2AProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    // Construction / Destruction
    //==============================================================================
    
    PtV2AProcessor();
    ~PtV2AProcessor() override = default;

    //==============================================================================
    // Audio Processing Lifecycle (JUCE AudioProcessor Interface)
    //==============================================================================
    
    /** 
     * Called before playback starts
     * Prepares the processor for audio processing with given sample rate and block size
     * @param sampleRate      The sample rate that will be used for processing (e.g., 48000 Hz)
     * @param samplesPerBlock The maximum number of samples that will be in each processing block
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    
    /** 
     * Called after playback stops
     * Release any resources allocated in prepareToPlay()
     * Note: This is a pass-through plugin, so no resources need to be released
     */
    void releaseResources() override;
    
    /** 
     * Validate audio bus layout (channel configuration)
     * This plugin requires matching input/output channel counts (mono or stereo only)
     * @param layouts The proposed bus layout to validate
     * @return true if layout is supported, false otherwise
     */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    
    /** 
     * Process audio block in real-time
     * This is a utility plugin that does NOT modify audio (pass-through only)
     * Audio generation happens asynchronously via MMAudio API, not in real-time
     * @param buffer Audio buffer to process (input/output)
     * @param midiMessages MIDI messages (unused - plugin doesn't process MIDI)
     */
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    //==============================================================================
    // Editor (GUI) Management
    //==============================================================================
    
    /** Creates the plugin GUI editor */
    juce::AudioProcessorEditor* createEditor() override;
    
    /** Returns true if plugin has a GUI editor */
    bool hasEditor() const override { return true; }

    //==============================================================================
    // Plugin Metadata (JUCE AudioProcessor Interface)
    //==============================================================================
    
    /** Plugin name displayed in Pro Tools */
    const juce::String getName() const override { return "PTV2A"; }  // Match CMakeLists.txt PRODUCT_NAME
    
    /** This plugin does not accept MIDI input */
    bool acceptsMidi() const override { return false; }
    
    /** This plugin does not produce MIDI output */
    bool producesMidi() const override { return false; }
    
    /** This is not a MIDI effect plugin */
    bool isMidiEffect() const override { return false; }
    
    /** No audio tail/reverb - plugin stops immediately when audio stops */
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    // Preset/Program Management (JUCE AudioProcessor Interface)
    //==============================================================================
    // This plugin does not use presets/programs - all state is in the GUI
    
    /** Number of programs/presets (1 = no program support) */
    int getNumPrograms() override { return 1; }
    
    /** Currently selected program index (always 0) */
    int getCurrentProgram() override { return 0; }
    
    /** Set current program (no-op - no presets) */
    void setCurrentProgram (int index) override { juce::ignoreUnused(index); }
    
    /** Get program name (empty - no presets) */
    const juce::String getProgramName (int index) override { juce::ignoreUnused(index); return {}; }
    
    /** Change program name (no-op - no presets) */
    void changeProgramName (int index, const juce::String& newName) override 
    { 
        juce::ignoreUnused(index, newName); 
    }

    //==============================================================================
    // State Serialization (JUCE AudioProcessor Interface)
    //==============================================================================
    
    /** 
     * Save plugin state to binary data
     * Called when Pro Tools session is saved
     * TODO: Currently stores empty JSON - should save UI parameters (prompt, seed, etc.)
     * @param destData Output buffer to write state to
     */
    void getStateInformation (juce::MemoryBlock& destData) override;
    
    /** 
     * Restore plugin state from binary data
     * Called when Pro Tools session is loaded
     * TODO: Currently ignores data - should restore UI parameters
     * @param data Pointer to state data
     * @param sizeInBytes Size of state data in bytes
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // MMAudio API Integration (Custom Plugin Functionality)
    //==============================================================================
    
    // Default configuration values
    static constexpr int DEFAULT_SEED = 42;                    ///< Default random seed for reproducibility
    static const juce::String DEFAULT_NEGATIVE_PROMPT;         ///< Default sounds to avoid ("voices, music")
    static const juce::String DEFAULT_API_URL;                 ///< Default MMAudio API endpoint
    
    /** 
     * Generate audio from video file using MMAudio API
     * 
     * This method spawns a Python subprocess that:
     * 1. Uploads video to MMAudio API (FastAPI server)
     * 2. Sends text prompt with generation parameters
     * 3. Waits for audio generation (can take 30-60 seconds)
     * 4. Downloads generated audio file (WAV format)
     * 5. Automatically imports audio into Pro Tools via PTSL
     * 
     * The process is non-blocking - GUI remains responsive during generation.
     * 
     * @param videoFile         Path to input video file (MP4, MOV, AVI, MKV, etc.)
     * @param prompt            Text prompt describing desired audio (e.g., "thunder and rain")
     * @param negativePrompt    Sounds to avoid (default: "voices, music")
     * @param seed              Random seed for reproducibility (default: 42)
     * @param errorMessage      [OUT] Pointer to string that will receive error details on failure
     *                          Pass nullptr if you don't need error details
     * 
     * @return Path to generated audio file (WAV) on success, empty string on failure
     *         Check errorMessage parameter for failure reason
     * 
     * @note This is an asynchronous operation - the method spawns a subprocess
     *       and returns immediately. Check process output for completion status.
     */
    juce::String generateAudioFromVideo (
        const juce::File& videoFile,
        const juce::String& prompt,
        const juce::String& negativePrompt = DEFAULT_NEGATIVE_PROMPT,
        int seed = DEFAULT_SEED,
        juce::String* errorMessage = nullptr
    );
    
    /** 
     * Check if MMAudio API server is reachable
     * Sends HTTP GET request to /health endpoint
     * 
     * @param apiUrl API base URL (default: "http://localhost:8000")
     * @return true if API responds successfully, false otherwise
     */
    bool isAPIAvailable (const juce::String& apiUrl = DEFAULT_API_URL);
    
    /** 
     * Get path to Python API client script (standalone_api_client.py)
     * Searches in following locations (in order):
     * 1. Embedded in plugin bundle: Resources/python/Scripts/standalone_api_client.py
     * 2. Development fallback: ../../companion/standalone_api_client.py
     * 
     * @return File object pointing to API client script
     */
    juce::File getAPIClientScript();
    
    /** 
     * Get path to Python executable
     * Searches in following locations (in order):
     * 1. Embedded Python: Resources/python/python.exe (self-contained)
     * 2. System Python: "python.exe" (fallback, may lack dependencies)
     * 
     * @return Path to python.exe
     */
    juce::String getPythonExecutable();
    
    //==============================================================================
    // Logging
    //==============================================================================
    
    /**
     * Initialize file-based logging system
     * Creates log file in user's AppData directory
     * Should be called once during plugin initialization
     * 
     * Log file location:
     *   Windows: C:\Users\[username]\AppData\Roaming\ldegenhardt\PTV2A\PTV2A.log
     *   macOS: ~/Library/Application Support/ldegenhardt/PTV2A/PTV2A.log
     * 
     * @return true if logger initialized successfully
     */
    static bool initializeLogger();
    
    /**
     * Get path to current log file
     * @return Full path to log file, or empty string if logger not initialized
     */
    static juce::File getLogFile();

private:
    //==============================================================================
    // Private Members
    //==============================================================================
    
    // File logger instance (shared across all plugin instances)
    static std::unique_ptr<juce::FileLogger> fileLogger;
    
    // Currently no persistent state - all parameters are transient in GUI
    // TODO: Add state variables if we want to persist UI settings:
    //   - juce::String lastPrompt;
    //   - juce::String lastNegativePrompt;
    //   - int lastSeed;
    //   - juce::File lastVideoFile;
    
    //==============================================================================
    // JUCE Leak Detector (Debug builds only)
    // Ensures no memory leaks in this class
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AProcessor)
};
