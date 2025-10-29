#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PtV2AProcessor : public juce::AudioProcessor
{
public:
    PtV2AProcessor();
    ~PtV2AProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "PT V2A Prototype"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // MMAudio API Integration
    //==============================================================================
    
    /** 
     * Generate audio from video file using MMAudio API
     * 
     * @param videoFile         Path to input video file (MP4, MOV, AVI, etc.)
     * @param prompt            Text prompt describing desired audio
     * @param negativePrompt    Sounds to avoid (default: "voices, music")
     * @param seed              Random seed for reproducibility (default: 42)
     * @param errorMessage      Output parameter for error details
     * @return                  Path to generated audio file, or empty string on failure
     */
    juce::String generateAudioFromVideo (
        const juce::File& videoFile,
        const juce::String& prompt,
        const juce::String& negativePrompt = "voices, music",
        int seed = 42,
        juce::String* errorMessage = nullptr
    );
    
    /** Check if MMAudio API is reachable at given URL */
    bool isAPIAvailable (const juce::String& apiUrl = "http://localhost:8000");
    
    /** Get path to Python API client script */
    juce::File getAPIClientScript();
    
    /** Get path to Python executable */
    juce::String getPythonExecutable();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PtV2AProcessor)
};
