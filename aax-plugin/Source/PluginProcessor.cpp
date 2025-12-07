#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Static constant definitions
//==============================================================================
const juce::String PtV2AProcessor::DEFAULT_NEGATIVE_PROMPT = "voices, music";
const juce::String PtV2AProcessor::DEFAULT_API_URL = "http://localhost:8000";

//==============================================================================
// Static member initialization
//==============================================================================
std::unique_ptr<juce::FileLogger> PtV2AProcessor::fileLogger = nullptr;

//==============================================================================
// Constructor
//==============================================================================
PtV2AProcessor::PtV2AProcessor()
: AudioProcessor (BusesProperties()
                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize file logger on first plugin instance
    // This ensures logs are captured from plugin startup
    initializeLogger();
}

void PtV2AProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) {}
void PtV2AProcessor::releaseResources() {}

bool PtV2AProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Require same channel count on in/out and allow mono or stereo
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void PtV2AProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    // Pass-through: do not modify audio
    juce::ignoreUnused (buffer);
}

juce::AudioProcessorEditor* PtV2AProcessor::createEditor()
{
    return new PtV2AEditor (*this);
}

void PtV2AProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // minimal: store nothing yet
    juce::MemoryOutputStream (destData, true).writeString ("{}");
}

void PtV2AProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// MMAudio API Integration Implementation
//==============================================================================

juce::String PtV2AProcessor::getPythonExecutable()
{
    // Use embedded Python bundled with the plugin
    // This makes the plugin completely self-contained with all dependencies
    
    // Get plugin binary location
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory(); // This is x64/ directory
    
    // Go up to Contents/ directory, then find Resources/
    // Structure: PTV2A.aaxplugin/Contents/x64/PTV2A.aaxplugin (binary)
    //                                    /Resources/python/python.exe
    auto contentsDir = pluginDir.getParentDirectory(); // Go from x64/ to Contents/
    
    juce::Logger::writeToLog ("=== Python Executable Search ===");
    juce::Logger::writeToLog ("Plugin binary directory: " + pluginDir.getFullPathName());
    juce::Logger::writeToLog ("Plugin Contents directory: " + contentsDir.getFullPathName());
    
    // Platform-specific Python executable name and location
#if JUCE_WINDOWS
    // Windows: python.exe in Contents/Resources/python/
    auto embeddedPythonExe = contentsDir.getChildFile("Resources")
                                        .getChildFile("python")
                                        .getChildFile("python.exe");
#elif JUCE_MAC
    // macOS: python3 or python in Contents/Resources/python/ or Contents/Resources/python/bin/
    auto embeddedPythonExe = contentsDir.getChildFile("Resources")
                                        .getChildFile("python")
                                        .getChildFile("python3");
    
    // If python3 not found, try python
    if (!embeddedPythonExe.existsAsFile())
        embeddedPythonExe = contentsDir.getChildFile("Resources")
                                       .getChildFile("python")
                                       .getChildFile("python");
    
    // Some Python distributions put binary in bin/ subdirectory
    if (!embeddedPythonExe.existsAsFile())
        embeddedPythonExe = contentsDir.getChildFile("Resources")
                                       .getChildFile("python")
                                       .getChildFile("bin")
                                       .getChildFile("python3");
#else
    // Linux: python3 in Contents/Resources/python/
    auto embeddedPythonExe = contentsDir.getChildFile("Resources")
                                        .getChildFile("python")
                                        .getChildFile("python3");
#endif
    
    juce::Logger::writeToLog ("Checking embedded Python: " + embeddedPythonExe.getFullPathName());
    
    if (embeddedPythonExe.existsAsFile())
    {
        juce::Logger::writeToLog ("✓ Using embedded Python from plugin Resources");
        juce::Logger::writeToLog ("Python path: " + embeddedPythonExe.getFullPathName());
        return embeddedPythonExe.getFullPathName();
    }
    
    juce::Logger::writeToLog ("⚠ Embedded Python not found!");
    juce::Logger::writeToLog ("Expected at: " + embeddedPythonExe.getFullPathName());
    juce::Logger::writeToLog ("Make sure Resources/python/ is copied to the plugin bundle");
    juce::Logger::writeToLog ("⚠ Falling back to system Python");
    
    // Fallback: Try system Python (will likely fail without dependencies)
#if JUCE_WINDOWS
    juce::Logger::writeToLog ("Using system Python: python.exe");
    return "python.exe";
#else
    juce::Logger::writeToLog ("Using system Python: python3");
    return "python3";
#endif
}

juce::File PtV2AProcessor::getAPIClientScript()
{
    // For embedded Python: Script is bundled in plugin Resources/python/Scripts/
    // This makes the plugin self-contained and portable
    
    // Get plugin binary location
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory(); // This is x64/ directory
    
    // Go up to Contents/ directory
    auto contentsDir = pluginDir.getParentDirectory(); // Go from x64/ to Contents/
    
    juce::Logger::writeToLog ("=== API Client Script Search ===");
    
    // Try embedded script first (production/installed builds)
    // Structure: PTV2A.aaxplugin/Contents/Resources/python/Scripts/standalone_api_client.py
    auto embeddedScript = contentsDir.getChildFile("Resources")
                                     .getChildFile("python")
                                     .getChildFile("Scripts")
                                     .getChildFile("standalone_api_client.py");
    
    juce::Logger::writeToLog ("Checking embedded script: " + embeddedScript.getFullPathName());
    
    if (embeddedScript.existsAsFile())
    {
        juce::Logger::writeToLog ("✓ Using embedded script from plugin Resources");
        juce::Logger::writeToLog ("Script path: " + embeddedScript.getFullPathName());
        return embeddedScript;
    }
    
    juce::Logger::writeToLog ("⚠ Embedded script not found, trying external paths (development fallback)");
    
    // Fallback: External companion directory (for development builds)
    juce::File thesisRoot;
    
#if JUCE_WINDOWS
    // Windows AAX build structure:
    // From: build/pt_v2a_artefacts/Debug/AAX/pt_v2a.aaxplugin/Contents/x64/pt_v2a.aaxplugin
    // To:   thesis-pt-v2a/companion/standalone_api_client.py
    
    auto candidate1 = pluginDir.getParentDirectory()      // x64
                                .getParentDirectory()      // Contents
                                .getParentDirectory()      // pt_v2a.aaxplugin
                                .getParentDirectory()      // AAX
                                .getParentDirectory()      // Debug
                                .getParentDirectory()      // pt_v2a_artefacts
                                .getParentDirectory()      // build
                                .getParentDirectory();     // thesis-pt-v2a
    
    auto candidate2 = pluginDir.getParentDirectory()      // Contents
                                .getParentDirectory()      // pt_v2a.aaxplugin
                                .getParentDirectory()      // AAX
                                .getParentDirectory()      // Debug
                                .getParentDirectory()      // pt_v2a_artefacts
                                .getParentDirectory()      // build
                                .getParentDirectory();     // thesis-pt-v2a
    
    if (candidate1.getChildFile("companion").exists())
        thesisRoot = candidate1;
    else if (candidate2.getChildFile("companion").exists())
        thesisRoot = candidate2;
    else
        thesisRoot = candidate1;  // Fallback
        
#elif JUCE_MAC
    // macOS AAX build structure
    thesisRoot = pluginDir.getParentDirectory()  // Contents
                          .getParentDirectory()  // pt_v2a.aaxplugin
                          .getParentDirectory()  // Debug
                          .getParentDirectory()  // build
                          .getParentDirectory()  // MacOSX
                          .getParentDirectory()  // Builds
                          .getParentDirectory()  // aax-plugin
                          .getParentDirectory(); // thesis-pt-v2a
#else
    // Linux fallback
    thesisRoot = pluginDir.getParentDirectory()
                          .getParentDirectory()
                          .getParentDirectory()
                          .getParentDirectory();
#endif
    
    juce::Logger::writeToLog ("Thesis root candidate: " + thesisRoot.getFullPathName());
    
    auto scriptPath = thesisRoot.getChildFile ("companion")
                                 .getChildFile ("standalone_api_client.py");
    
    if (scriptPath.existsAsFile())
    {
        juce::Logger::writeToLog ("✓ Found API client script (external/development): " + scriptPath.getFullPathName());
        return scriptPath;
    }
    
    juce::Logger::writeToLog ("Script not found at: " + scriptPath.getFullPathName());
    
    // Last resort: Try relative to current working directory
    auto cwdScript = juce::File::getCurrentWorkingDirectory()
                         .getChildFile ("companion")
                         .getChildFile ("standalone_api_client.py");
    
    if (cwdScript.existsAsFile())
    {
        juce::Logger::writeToLog ("✓ Found API client script in CWD: " + cwdScript.getFullPathName());
        return cwdScript;
    }
    
    juce::Logger::writeToLog ("Script not found at CWD: " + cwdScript.getFullPathName());
    
#if JUCE_WINDOWS
    // Windows-specific fallback: Try known absolute paths
    juce::StringArray fallbackPaths = {
        "C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\thesis-pt-v2a\\companion\\standalone_api_client.py",
        "C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\thesis-pt-v2a\\companion\\standalone_api_client.py"
    };
    
    for (const auto& path : fallbackPaths)
    {
        juce::File fallbackFile (path);
        if (fallbackFile.existsAsFile())
        {
            juce::Logger::writeToLog ("✓ Found API client script via fallback: " + fallbackFile.getFullPathName());
            return fallbackFile;
        }
        juce::Logger::writeToLog ("Script not found at fallback: " + path);
    }
#endif
    
    juce::Logger::writeToLog ("❌ ERROR: API client script not found in any location!");
    juce::Logger::writeToLog ("Current working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
    return juce::File();
}

juce::String PtV2AProcessor::getConfiguredAPIUrl (const juce::String& service)
{
    juce::Logger::writeToLog ("=== Loading API URL from config.json ===");
    
    // Get config.json path from embedded Python resources
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory();
    auto contentsDir = pluginDir.getParentDirectory();
    
    auto configFile = contentsDir.getChildFile("Resources")
                                 .getChildFile("python")
                                 .getChildFile("Lib")
                                 .getChildFile("site-packages")
                                 .getChildFile("api")
                                 .getChildFile("config.json");
    
    juce::Logger::writeToLog ("Config path: " + configFile.getFullPathName());
    
    if (!configFile.existsAsFile())
    {
        juce::Logger::writeToLog ("⚠️ config.json not found, using default URL");
        return DEFAULT_API_URL;
    }
    
    // Read and parse JSON
    auto jsonText = configFile.loadFileAsString();
    auto json = juce::JSON::parse (jsonText);
    
    if (auto* root = json.getDynamicObject())
    {
        bool useCloudflared = root->getProperty ("use_cloudflared");
        juce::Logger::writeToLog ("use_cloudflared: " + juce::String(useCloudflared ? "true" : "false"));
        
        if (auto* services = root->getProperty ("services").getDynamicObject())
        {
            if (auto* serviceConfig = services->getProperty (service).getDynamicObject())
            {
                juce::String apiUrl;
                
                if (useCloudflared)
                {
                    apiUrl = serviceConfig->getProperty ("api_url_cloudflared").toString();
                    if (apiUrl.isEmpty())
                        apiUrl = serviceConfig->getProperty ("api_url_direct").toString();
                }
                else
                {
                    apiUrl = serviceConfig->getProperty ("api_url_direct").toString();
                }
                
                if (apiUrl.isNotEmpty())
                {
                    juce::Logger::writeToLog ("✓ Loaded API URL: " + apiUrl);
                    return apiUrl;
                }
            }
        }
    }
    
    juce::Logger::writeToLog ("⚠️ Failed to parse config.json, using default URL");
    return DEFAULT_API_URL;
}

bool PtV2AProcessor::isAPIAvailable (const juce::String& apiUrl)
{
    // Skip C++ HTTP check for Cloudflare URLs
    // JUCE's WinHTTP has SSL/TLS issues with Cloudflare Access
    // Python scripts handle Cloudflare authentication properly
    if (apiUrl.contains ("cloudflare") || apiUrl.contains ("linwig.de"))
    {
        juce::Logger::writeToLog ("Cloudflare URL detected, skipping C++ health check (Python handles auth)");
        return true;  // Python will do the actual check with proper SSL/auth
    }
    
    // Simple health check for localhost: try to reach API root endpoint
    juce::URL healthCheck (apiUrl + "/");
    int statusCode = 0;
    
    juce::Logger::writeToLog ("Checking local API: " + apiUrl);
    
    auto inputStream = healthCheck.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (3000)
            .withNumRedirectsToFollow (0)
            .withStatusCode (&statusCode)
    );
    
    if (inputStream != nullptr && statusCode >= 200 && statusCode < 300)
    {
        juce::String response = inputStream->readEntireStreamAsString();
        if (response.contains ("MMAudio") || response.contains ("status") || response.contains ("HunyuanVideo"))
        {
            juce::Logger::writeToLog ("✓ Local API available");
            return true;
        }
    }
    
    juce::Logger::writeToLog ("✗ Local API not available (status: " + juce::String (statusCode) + ")");
    return false;
}

juce::String PtV2AProcessor::generateAudioFromVideo (
    const juce::File& videoFile,
    const juce::String& prompt,
    const juce::String& negativePrompt,
    int seed,
    ModelProvider modelProvider,
    const juce::String& modelSize,
    const juce::String& videoClipOffset,
    float timelineInSeconds,
    float timelineOutSeconds,
    bool autoDetectClipBounds,
    float clipStartSeconds,
    float clipEndSeconds,
    bool fullPrecision,
    juce::String* errorMessage)
{
    // Log provider-specific information
    juce::String providerName = (modelProvider == ModelProvider::MMAudio) ? "MMAudio" : "HunyuanVideo-Foley";
    juce::Logger::writeToLog ("=== " + providerName + " Generation Started ===");
    juce::Logger::writeToLog ("Model: " + providerName + " / " + modelSize);
    juce::Logger::writeToLog ("Video: " + videoFile.getFullPathName());
    juce::Logger::writeToLog ("Prompt: " + prompt);
    juce::Logger::writeToLog ("Negative Prompt: " + negativePrompt);
    juce::Logger::writeToLog ("Seed: " + juce::String (seed));
    
    // Validate inputs
    if (!videoFile.existsAsFile())
    {
        juce::String error = "Video file does not exist: " + videoFile.getFullPathName();
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Get Python executable
    auto pythonExe = getPythonExecutable();
    
    // Get API client script based on selected provider
    juce::File scriptFile;
    if (modelProvider == ModelProvider::MMAudio)
    {
        scriptFile = getAPIClientScript();  // Uses standalone_api_client.py
        juce::Logger::writeToLog ("Using MMAudio client script: " + scriptFile.getFullPathName());
    }
    else if (modelProvider == ModelProvider::HunyuanVideoFoley)
    {
        // Get hunyuanvideo_foley_api_client.py from companion directory
        // First get MMAudio script to find companion directory
        auto mmAudioScript = getAPIClientScript();
        auto companionDir = mmAudioScript.getParentDirectory();
        scriptFile = companionDir.getChildFile ("hunyuanvideo_foley_api_client.py");
        juce::Logger::writeToLog ("Using HunyuanVideo-Foley client script: " + scriptFile.getFullPathName());
    }
    
    if (!scriptFile.existsAsFile())
    {
        juce::String error = "API client script not found: " + scriptFile.getFullPathName();
        
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Get script directory (for working directory)
    juce::File scriptDir = scriptFile.getParentDirectory();
    juce::String scriptPath = scriptDir.getFullPathName();
    
    juce::Logger::writeToLog ("Script directory: " + scriptPath);
    
    // Build command line arguments using StringArray for clean direct execution
    juce::StringArray commandArray;
    
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");  // Force UTF-8 mode
    commandArray.add (scriptFile.getFullPathName());
    
    commandArray.add ("--video");
    commandArray.add (videoFile.getFullPathName());
    
    if (prompt.isNotEmpty())
    {
        commandArray.add ("--prompt");
        commandArray.add (prompt);
    }
    
    if (negativePrompt.isNotEmpty())
    {
        commandArray.add ("--negative-prompt");
        commandArray.add (negativePrompt);
    }
    
    commandArray.add ("--seed");
    commandArray.add (juce::String (seed));
    
    // Add model-specific arguments
    if (modelProvider == ModelProvider::MMAudio)
    {
        // MMAudio uses --model <model_name>
        // UI values: "Large" → "large_44k_v2", "Medium" → "medium_44k", "Small" → "small_16k"
        juce::String modelArg;
        if (modelSize.contains ("Large"))
            modelArg = "large_44k_v2";
        else if (modelSize.contains ("Medium"))
            modelArg = "medium_44k";
        else if (modelSize.contains ("Small"))
            modelArg = "small_16k";
        else
            modelArg = "large_44k_v2";  // Default fallback
        
        commandArray.add ("--model");
        commandArray.add (modelArg);
        juce::Logger::writeToLog ("MMAudio model: " + modelArg);
    }
    else if (modelProvider == ModelProvider::HunyuanVideoFoley)
    {
        // HunyuanVideo-Foley uses --model-size <xl|xxl>
        // UI values: "XL (8-12GB VRAM)" → "xl", "XXL (16-20GB VRAM)" → "xxl"
        juce::String modelSizeArg;
        if (modelSize.contains ("XL") && !modelSize.contains ("XXL"))
            modelSizeArg = "xl";
        else if (modelSize.contains ("XXL"))
            modelSizeArg = "xxl";
        else
            modelSizeArg = "xxl";  // Default fallback
        
        commandArray.add ("--model-size");
        commandArray.add (modelSizeArg);
        juce::Logger::writeToLog ("HunyuanVideo-Foley model size: " + modelSizeArg);
    }
    
    // WORKFLOW 1: Manual offset WITH clip bounds (trimmed clip + manual offset)
    // Priority: Manual offset > Clip bounds > Auto-detect
    if (videoClipOffset.isNotEmpty() && clipStartSeconds >= 0.0f)
    {
        // Special case: Manual offset on trimmed clip
        // Need BOTH clip bounds (where clip starts in source) AND manual offset (timeline position)
        // Python will calculate: source_start = clip_source_start + (timeline_start - clip_timeline_start)
        commandArray.add ("--video-offset");
        commandArray.add (videoClipOffset);
        commandArray.add ("--timeline-start");
        commandArray.add (juce::String (timelineInSeconds));
        commandArray.add ("--timeline-end");
        commandArray.add (juce::String (timelineOutSeconds));
        commandArray.add ("--clip-start-seconds");
        commandArray.add (juce::String (clipStartSeconds, 3));
        commandArray.add ("--clip-end-seconds");
        commandArray.add (juce::String (clipEndSeconds, 3));
        juce::Logger::writeToLog ("Manual Offset (trimmed clip): " + videoClipOffset);
        juce::Logger::writeToLog ("Clip Bounds: " + juce::String (clipStartSeconds, 3) + "s - " + juce::String (clipEndSeconds, 3) + "s");
        juce::Logger::writeToLog ("Timeline: " + juce::String (timelineInSeconds) + "s - " + juce::String (timelineOutSeconds) + "s");
    }
    // WORKFLOW 2: Clip bounds only (auto-detect, trimmed clip, no manual offset)
    else if (clipStartSeconds >= 0.0f && clipEndSeconds >= 0.0f)
    {
        commandArray.add ("--clip-start-seconds");
        commandArray.add (juce::String (clipStartSeconds, 3));
        commandArray.add ("--clip-end-seconds");
        commandArray.add (juce::String (clipEndSeconds, 3));
        juce::Logger::writeToLog ("Clip Bounds (seconds): " + juce::String (clipStartSeconds, 3) + " - " + juce::String (clipEndSeconds, 3));
    }
    // WORKFLOW 3: Manual offset WITHOUT clip bounds (untrimmed clip + manual offset)
    else if (videoClipOffset.isNotEmpty())
    {
        commandArray.add ("--video-offset");
        commandArray.add (videoClipOffset);
        juce::Logger::writeToLog ("Video Clip Offset: " + videoClipOffset);
        
        // Also pass timeline selection times (in seconds) for trimming calculation
        commandArray.add ("--timeline-start");
        commandArray.add (juce::String (timelineInSeconds));
        commandArray.add ("--timeline-end");
        commandArray.add (juce::String (timelineOutSeconds));
        juce::Logger::writeToLog ("Timeline Selection (seconds): " + juce::String (timelineInSeconds) + " - " + juce::String (timelineOutSeconds));
    }
    // WORKFLOW 3 (LEGACY): Auto-detect in background (unsafe from plugin, causes deadlock)
    else if (autoDetectClipBounds)
    {
        commandArray.add ("--auto-detect-clip-bounds");
        juce::Logger::writeToLog ("⚠️ WARNING: Auto-detect clip boundaries in background (may cause deadlock from plugin!)");
    }
    
    // Generate output directory path (filename will be generated server-side with prompt snippet)
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto outputsDir = tempDir.getChildFile ("pt_v2a_outputs");
    outputsDir.createDirectory();
    
    // NOTE: We no longer generate a specific filename here
    // The server will generate a descriptive name: {prompt_snippet}_{seed}_{model}_{timestamp}.wav
    // Python client will save using the server-provided filename
    // We just specify the output directory via --temp flag (client will use temp dir)
    
    // Don't pass --output (client will auto-generate path in temp dir with server name)
    // The client will print the actual output path to stdout, which we'll read
    
    commandArray.add ("--output-format");
    commandArray.add ("wav");  // Pro Tools compatible
    
    commandArray.add ("--temp");  // Use temp directory (pt_v2a_outputs)
    
    // Add full precision flag if enabled
    if (fullPrecision)
    {
        commandArray.add ("--full-precision");
        juce::Logger::writeToLog ("Using full precision mode (float32)");
    }
    else
    {
        juce::Logger::writeToLog ("Using default precision (bfloat16)");
    }
    
    // NOTE: No --import-to-protools flag - plugin will handle PTSL import async!
    // NOTE: Removed --quiet for debugging - we want to see Python output!
    // commandArray.add ("--quiet");  // Minimal output for parsing
    
    juce::Logger::writeToLog ("Starting audio generation (background process)...");
    juce::Logger::writeToLog ("Output directory: " + outputsDir.getFullPathName());
    juce::Logger::writeToLog ("Server will generate filename with prompt snippet");
    juce::Logger::writeToLog ("Command: " + commandArray.joinIntoString (" "));
    
    // Execute subprocess in BACKGROUND (non-blocking)
    // CRITICAL: We use a fire-and-forget approach
    // We start the process and immediately return, letting Python run independently
    // The Editor will poll for the output file to detect completion
    
    // Start process with JUCE ChildProcess
    // We allocate on heap so we can control its lifetime
    auto* backgroundProcess = new juce::ChildProcess();
    
    if (!backgroundProcess->start (commandArray))
    {
        delete backgroundProcess;
        juce::String error = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    juce::Logger::writeToLog ("✓ Python process started successfully (running independently)");
    
    // IMPORTANT: We intentionally leak this ChildProcess object!
    // If we delete it, the process gets killed
    // Python will run independently and we poll for the file instead
    // The OS will clean up the process when it finishes
    // Memory leak is acceptable here (one-time allocation per generation)
    
    // NOTE: Server generates filename with prompt snippet, so we can't predict the exact name
    // Instead, we return the output directory path
    // The Editor will poll for the NEWEST .wav file in this directory
    juce::Logger::writeToLog ("Output directory: " + outputsDir.getFullPathName());
    juce::Logger::writeToLog ("Editor will poll for newest WAV file in directory...");
    
    if (errorMessage != nullptr)
        *errorMessage = "";  // Clear any previous error
    
    // Return the output directory path (Editor will find the newest file there)
    return outputsDir.getFullPathName();
}

//==============================================================================
// T2A Audio Generation (text-only, no video)
//==============================================================================

juce::String PtV2AProcessor::generateAudioTextOnly (
    const juce::String& prompt,
    float duration,
    const juce::String& negativePrompt,
    int seed,
    const juce::String& modelSize,
    juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== T2A Generation Started (text-only) ===");
    juce::Logger::writeToLog ("Model: MMAudio / " + modelSize);
    juce::Logger::writeToLog ("Duration: " + juce::String (duration, 1) + "s");
    juce::Logger::writeToLog ("Prompt: " + prompt);
    juce::Logger::writeToLog ("Negative Prompt: " + negativePrompt);
    juce::Logger::writeToLog ("Seed: " + juce::String (seed));
    
    // Validate inputs
    if (duration < 4.0f || duration > 12.0f)
    {
        juce::String error = "T2A duration must be 4-12 seconds, got: " + juce::String (duration, 1) + "s";
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Get Python executable and script
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();  // Uses standalone_api_client.py (supports T2A)
    
    if (!scriptFile.existsAsFile())
    {
        juce::String error = "API client script not found: " + scriptFile.getFullPathName();
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    juce::File scriptDir = scriptFile.getParentDirectory();
    juce::Logger::writeToLog ("Script directory: " + scriptDir.getFullPathName());
    
    // Build command line arguments (NO --video parameter for T2A)
    juce::StringArray commandArray;
    
    commandArray.add (pythonExe);
    commandArray.add ("-X");
    commandArray.add ("utf8");
    commandArray.add (scriptFile.getFullPathName());
    
    // T2A mode: NO --video parameter, but ADD --duration
    commandArray.add ("--duration");
    commandArray.add (juce::String (duration, 1));
    
    if (prompt.isNotEmpty())
    {
        commandArray.add ("--prompt");
        commandArray.add (prompt);
    }
    
    if (negativePrompt.isNotEmpty())
    {
        commandArray.add ("--negative-prompt");
        commandArray.add (negativePrompt);
    }
    
    commandArray.add ("--seed");
    commandArray.add (juce::String (seed));
    
    // MMAudio model argument
    juce::String modelArg;
    if (modelSize.contains ("Large"))
        modelArg = "large_44k_v2";
    else if (modelSize.contains ("Medium"))
        modelArg = "medium_44k";
    else if (modelSize.contains ("Small"))
        modelArg = "small_16k";
    else
        modelArg = "large_44k_v2";  // Default
    
    commandArray.add ("--model");
    commandArray.add (modelArg);
    juce::Logger::writeToLog ("MMAudio model: " + modelArg);
    
    // Output settings
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto outputsDir = tempDir.getChildFile ("pt_v2a_outputs");
    outputsDir.createDirectory();
    
    commandArray.add ("--output-format");
    commandArray.add ("wav");
    
    commandArray.add ("--temp");  // Use temp directory
    
    commandArray.add ("--verbose");  // Enable verbose output for debugging
    
    juce::Logger::writeToLog ("Starting T2A audio generation (background process)...");
    juce::Logger::writeToLog ("Output directory: " + outputsDir.getFullPathName());
    juce::Logger::writeToLog ("Command: " + commandArray.joinIntoString (" "));
    
    // Start background process
    auto* backgroundProcess = new juce::ChildProcess();
    
    if (!backgroundProcess->start (commandArray))
    {
        delete backgroundProcess;
        juce::String error = "Failed to start Python process for T2A generation";
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    juce::Logger::writeToLog ("✓ T2A process started successfully");
    
    // Intentionally leak process (runs independently)
    // Editor will poll for output file
    
    if (errorMessage != nullptr)
        *errorMessage = "";
    
    // Return output directory path
    return outputsDir.getFullPathName();
}

//==============================================================================
// Logging Implementation
//==============================================================================

bool PtV2AProcessor::initializeLogger()
{
    // Only initialize once (singleton pattern)
    if (fileLogger != nullptr)
        return true;
    
    // Get user's app data directory
    // Windows: C:\Users\[username]\AppData\Roaming\PTV2A\
    // macOS: ~/Library/Application Support/PTV2A/
    auto logDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("PTV2A");
    
    // Ensure directory exists
    if (!logDir.exists())
    {
        auto result = logDir.createDirectory();
        if (result.failed())
        {
            // Can't create directory - logging will fail (use console as fallback)
            std::cerr << "Failed to create log directory: " << result.getErrorMessage() << std::endl;
            return false;
        }
    }
    
    // Clean up old rotated logs (industry standard: keep last 30 days)
    // This prevents unlimited log file accumulation over time
    // JUCE's FileLogger creates .log.1, .log.2, etc. when rotating
    auto now = juce::Time::getCurrentTime();
    int deletedCount = 0;
    
    for (auto logFile : logDir.findChildFiles (juce::File::findFiles, false, "*.log*"))
    {
        // Calculate file age in days
        auto fileAge = now - logFile.getCreationTime();
        
        // Delete logs older than 30 days (industry standard)
        if (fileAge.inDays() > 30)
        {
            if (logFile.deleteFile())
                deletedCount++;
        }
    }
    
    if (deletedCount > 0)
        std::cout << "Cleaned up " << deletedCount << " old log files" << std::endl;
    
    // Create log file: PTV2A.log
    auto logFile = logDir.getChildFile ("PTV2A.log");
    
    // Create FileLogger instance
    // Parameters: logFile, welcomeMessage, maxInitialFileSizeBytes
    fileLogger = std::make_unique<juce::FileLogger> (
        logFile,
        "PTV2A Plugin Log",
        1024 * 1024 * 5  // 5 MB max log file size (then rotates)
    );
    
    // Set as default logger for all juce::Logger::writeToLog() calls
    juce::Logger::setCurrentLogger (fileLogger.get());
    
    // Write startup message
    juce::Logger::writeToLog ("===========================================");
    juce::Logger::writeToLog ("PTV2A Plugin Started");
    juce::Logger::writeToLog ("Log file: " + logFile.getFullPathName());
    juce::Logger::writeToLog ("Timestamp: " + juce::Time::getCurrentTime().toString (true, true));
    juce::Logger::writeToLog ("===========================================");
    
    return true;
}

juce::File PtV2AProcessor::getLogFile()
{
    if (fileLogger == nullptr)
        return juce::File();
    
    // Get log file path from logger
    return fileLogger->getLogFile();
}

//==============================================================================
// Phase 3B: Timeline Selection & Video Trimming Implementation
//==============================================================================

bool PtV2AProcessor::isFFmpegAvailable()
{
    juce::Logger::writeToLog ("=== Checking FFmpeg Availability ===");
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::Logger::writeToLog ("ERROR: API client script not found: " + scriptFile.getFullPathName());
        return false;
    }
    
    // Build command: python standalone_api_client.py --action check_ffmpeg
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action check_ffmpeg";
    
    juce::Logger::writeToLog ("Executing FFmpeg check command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        juce::Logger::writeToLog ("ERROR: Failed to start Python process");
        return false;
    }
    
    // Wait for completion (should be fast, <1 second)
    if (!process.waitForProcessToFinish (5000))  // 5 second timeout
    {
        juce::Logger::writeToLog ("ERROR: FFmpeg check timed out");
        process.kill();
        return false;
    }
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        bool available = obj->getProperty ("available");
        auto version = obj->getProperty ("version").toString();
        auto source = obj->getProperty ("source").toString();
        auto error = obj->getProperty ("error").toString();
        
        if (available)
        {
            juce::Logger::writeToLog ("=== FFmpeg Check SUCCESS ===");
            juce::Logger::writeToLog ("Version: " + version);
            juce::Logger::writeToLog ("Source: " + source);
            return true;
        }
        else
        {
            juce::Logger::writeToLog ("=== FFmpeg Check FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + error);
            return false;
        }
    }
    
    juce::Logger::writeToLog ("ERROR: Failed to parse FFmpeg check response");
    juce::Logger::writeToLog ("Raw output was: " + output);
    return false;
}

PtV2AProcessor::VideoSelectionInfo PtV2AProcessor::getVideoSelectionInfo()
{
    VideoSelectionInfo result;
    result.success = false;
    
    juce::Logger::writeToLog ("=== Getting Video Timeline Selection ===");
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        result.errorMessage = "API client script not found";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        return result;
    }
    
    // Get script directory (for working directory)
    juce::File scriptDir = scriptFile.getParentDirectory();
    juce::String scriptPath = scriptDir.getFullPathName();
    
    // Create log file for Python stderr output (separate from stdout)
    auto logDir = getLogFile().getParentDirectory();
    auto pythonLogFile = logDir.getChildFile ("python_stderr.log");
    juce::String pythonLogPath = pythonLogFile.getFullPathName();
    
    juce::Logger::writeToLog ("Python stderr will be logged to: " + pythonLogPath);
    
    // Build command: Direct Python call with -X utf8 flag (NO cmd.exe needed!)
    // Python's -X utf8 flag forces UTF-8 mode without environment variables
    juce::StringArray commandArray;
    commandArray.add (pythonExe);
    commandArray.add ("-X");           // Python option prefix
    commandArray.add ("utf8");         // Force UTF-8 mode (Python 3.7+)
    commandArray.add (scriptFile.getFullPathName());
    commandArray.add ("--action");
    commandArray.add ("get_video_selection");
    
    juce::Logger::writeToLog ("Executing timeline selection command...");
    juce::Logger::writeToLog ("Python: " + pythonExe);
    juce::Logger::writeToLog ("Args: " + commandArray.joinIntoString (" "));
    
    // Create child process
    juce::ChildProcess process;
    
    // Start process directly with array (NO shell wrapper!)
    // JUCE will capture stdout/stderr automatically
    if (!process.start (commandArray))
    {
        result.errorMessage = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        return result;
    }
    
    juce::Logger::writeToLog ("Python process started, waiting for completion...");
    
    // CRITICAL: Wait for process to finish FIRST, then read output!
    // Reading output while process is running can cause deadlock with PTSL
    if (!process.waitForProcessToFinish (5000))  // 5 second timeout
    {
        result.errorMessage = "Timeline selection read timed out";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        juce::Logger::writeToLog ("Make sure:");
        juce::Logger::writeToLog ("1. Pro Tools is running");
        juce::Logger::writeToLog ("2. You have a timeline selection (In/Out points)");
        juce::Logger::writeToLog ("3. PTSL is enabled in Pro Tools preferences");
        process.kill();
        return result;
    }
    
    juce::Logger::writeToLog ("Process finished, reading output...");
    
    // NOW read output after process has completed
    juce::String output = process.readAllProcessOutput();
    
    // Log output to file for debugging AND display
    if (output.isNotEmpty())
    {
        pythonLogFile.replaceWithText (output);
        juce::Logger::writeToLog ("Python output captured:");
        juce::Logger::writeToLog (output);
    }
    else
    {
        result.errorMessage = "No output from Python script";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        return result;
    }
    
    // Extract JSON from output (might have debug lines before/after)
    // Look for lines starting with { (JSON response)
    auto lines = juce::StringArray::fromLines (output);
    juce::String jsonOutput;
    for (const auto& line : lines)
    {
        if (line.trimStart().startsWith ("{"))
        {
            jsonOutput = line.trim();
            break;  // Found JSON response
        }
    }
    
    if (jsonOutput.isEmpty())
    {
        result.errorMessage = "No JSON response found in Python output";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        juce::Logger::writeToLog ("Full output was: " + output);
        return result;
    }
    
    juce::Logger::writeToLog ("Extracted JSON: " + jsonOutput);
    output = jsonOutput;  // Use extracted JSON for parsing
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        result.success = obj->getProperty ("success");
        result.inTime = obj->getProperty ("in_time").toString();
        result.outTime = obj->getProperty ("out_time").toString();
        result.durationSeconds = (float) (double) obj->getProperty ("duration_seconds");
        result.inSeconds = (float) (double) obj->getProperty ("in_seconds");
        result.outSeconds = (float) (double) obj->getProperty ("out_seconds");
        result.fps = (float) (double) obj->getProperty ("fps");
        result.errorMessage = obj->getProperty ("error").toString();
        
        if (result.success)
        {
            juce::Logger::writeToLog ("=== Timeline Selection SUCCESS ===");
            juce::Logger::writeToLog ("Timeline: " + result.inTime + " - " + result.outTime);
            juce::Logger::writeToLog ("Duration: " + juce::String (result.durationSeconds, 2) + "s");
            juce::Logger::writeToLog ("FPS: " + juce::String (result.fps, 2));
        }
        else
        {
            juce::Logger::writeToLog ("=== Timeline Selection FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
            juce::Logger::writeToLog ("Make sure:");
            juce::Logger::writeToLog ("1. Pro Tools is running");
            juce::Logger::writeToLog ("2. You have a timeline selection (In/Out points)");
            juce::Logger::writeToLog ("3. PTSL is enabled in Pro Tools preferences");
        }
        
        return result;
    }
    
    result.errorMessage = "Failed to parse timeline selection response";
    juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
    juce::Logger::writeToLog ("Raw output was: " + output);
    return result;
}

juce::String PtV2AProcessor::getVideoFileFromProTools(juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== Getting Video File from Pro Tools ===");
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::String errorMsg = "API client script not found";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Build command: python standalone_api_client.py --action get_video_file
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action get_video_file";
    
    juce::Logger::writeToLog ("Executing video file lookup command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        juce::String errorMsg = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Wait for completion (PTSL calls can take 1-2 seconds)
    if (!process.waitForProcessToFinish (10000))  // 10 second timeout
    {
        juce::String errorMsg = "Video file lookup timed out";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        process.kill();
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        bool success = obj->getProperty ("success");
        auto videoPath = obj->getProperty ("video_path").toString();
        auto errorFromJson = obj->getProperty ("error").toString();
        
        if (success && videoPath.isNotEmpty())
        {
            juce::Logger::writeToLog ("=== Video File Lookup SUCCESS ===");
            juce::Logger::writeToLog ("Video path: " + videoPath);
            if (errorMessage != nullptr)
                *errorMessage = "";
            return videoPath;
        }
        else
        {
            juce::Logger::writeToLog ("=== Video File Lookup FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + errorFromJson);
            if (errorMessage != nullptr)
                *errorMessage = errorFromJson;
            return juce::String();
        }
    }
    
    juce::String errorMsg = "Failed to parse video file response";
    juce::Logger::writeToLog ("ERROR: " + errorMsg);
    juce::Logger::writeToLog ("Raw output was: " + output);
    if (errorMessage != nullptr)
        *errorMessage = errorMsg;
    return juce::String();
}

juce::String PtV2AProcessor::trimVideoSegment(
    const juce::String& videoPath,
    float startSeconds,
    float endSeconds,
    juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== Trimming Video Segment ===");
    juce::Logger::writeToLog ("Video: " + videoPath);
    juce::Logger::writeToLog ("Range: " + juce::String (startSeconds, 2) + "s - " + juce::String (endSeconds, 2) + "s");
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::String errorMsg = "API client script not found";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Build command: python standalone_api_client.py --action trim_video 
    //                --video "path" --start-time X --end-time Y
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action trim_video ";
    command += "--video \"" + videoPath + "\" ";
    command += "--start-time " + juce::String (startSeconds) + " ";
    command += "--end-time " + juce::String (endSeconds);
    
    juce::Logger::writeToLog ("Executing video trim command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        juce::String errorMsg = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Wait for completion (FFmpeg trimming can take 1-3 seconds)
    if (!process.waitForProcessToFinish (60000))  // 60 second timeout
    {
        juce::String errorMsg = "Video trimming timed out";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        process.kill();
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return juce::String();
    }
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        bool success = obj->getProperty ("success");
        auto outputPath = obj->getProperty ("output_path").toString();
        auto errorFromJson = obj->getProperty ("error").toString();
        
        if (success && outputPath.isNotEmpty())
        {
            juce::Logger::writeToLog ("=== Video Trim SUCCESS ===");
            juce::Logger::writeToLog ("Output path: " + outputPath);
            if (errorMessage != nullptr)
                *errorMessage = "";
            return outputPath;
        }
        else
        {
            juce::Logger::writeToLog ("=== Video Trim FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + errorFromJson);
            if (errorMessage != nullptr)
                *errorMessage = errorFromJson;
            return juce::String();
        }
    }
    
    juce::String errorMsg = "Failed to parse video trimming response";
    juce::Logger::writeToLog ("ERROR: " + errorMsg);
    juce::Logger::writeToLog ("Raw output was: " + output);
    if (errorMessage != nullptr)
        *errorMessage = errorMsg;
    return juce::String();
}

float PtV2AProcessor::getVideoDuration(
    const juce::String& videoPath,
    juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== Getting Video Duration ===");
    juce::Logger::writeToLog ("Video: " + videoPath);
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::String errorMsg = "API client script not found";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return -1.0f;
    }
    
    // Build command: python standalone_api_client.py --action get_duration --video "path"
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action get_duration ";
    command += "--video \"" + videoPath + "\"";
    
    juce::Logger::writeToLog ("Executing duration check command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        juce::String errorMsg = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return -1.0f;
    }
    
    // Wait for completion (should be fast, <2 seconds)
    if (!process.waitForProcessToFinish (10000))  // 10 second timeout
    {
        juce::String errorMsg = "Duration check timed out";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        process.kill();
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return -1.0f;
    }
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        bool success = obj->getProperty ("success");
        double duration = (double) obj->getProperty ("duration");
        auto errorFromJson = obj->getProperty ("error").toString();
        
        if (success && duration > 0.0)
        {
            juce::Logger::writeToLog ("=== Duration Check SUCCESS ===");
            juce::Logger::writeToLog ("Duration: " + juce::String (duration, 2) + "s");
            if (errorMessage != nullptr)
                *errorMessage = "";
            return (float) duration;
        }
        else
        {
            juce::Logger::writeToLog ("=== Duration Check FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + errorFromJson);
            if (errorMessage != nullptr)
                *errorMessage = errorFromJson;
            return -1.0f;
        }
    }
    
    juce::String errorMsg = "Failed to parse duration check response";
    juce::Logger::writeToLog ("ERROR: " + errorMsg);
    juce::Logger::writeToLog ("Raw output was: " + output);
    if (errorMessage != nullptr)
        *errorMessage = errorMsg;
    return -1.0f;
}

bool PtV2AProcessor::validateVideoDuration(
    float durationSeconds,
    float maxDuration,
    juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== Validating Video Duration ===");
    juce::Logger::writeToLog ("Duration: " + juce::String (durationSeconds, 2) + "s (max: " + 
         juce::String (maxDuration, 2) + "s)");
    
    auto pythonExe = getPythonExecutable();
    auto scriptFile = getAPIClientScript();
    
    if (!scriptFile.existsAsFile())
    {
        juce::String errorMsg = "API client script not found";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return false;
    }
    
    // Build command: python standalone_api_client.py --action validate_duration 
    //                --duration X --max-duration Y
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action validate_duration ";
    command += "--duration " + juce::String (durationSeconds) + " ";
    command += "--max-duration " + juce::String (maxDuration);
    
    juce::Logger::writeToLog ("Executing duration validation command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        juce::String errorMsg = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return false;
    }
    
    // Wait for completion (should be instant)
    if (!process.waitForProcessToFinish (5000))  // 5 second timeout
    {
        juce::String errorMsg = "Duration validation timed out";
        juce::Logger::writeToLog ("ERROR: " + errorMsg);
        process.kill();
        if (errorMessage != nullptr)
            *errorMessage = errorMsg;
        return false;
    }
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
    // Parse JSON response
    auto json = juce::JSON::parse (output);
    if (auto* obj = json.getDynamicObject())
    {
        bool valid = obj->getProperty ("valid");
        auto errorFromJson = obj->getProperty ("error").toString();
        
        if (valid)
        {
            juce::Logger::writeToLog ("=== Duration Validation SUCCESS ===");
            juce::Logger::writeToLog ("Duration is valid");
            if (errorMessage != nullptr)
                *errorMessage = "";
            return true;
        }
        else
        {
            juce::Logger::writeToLog ("=== Duration Validation FAILED ===");
            juce::Logger::writeToLog ("ERROR: " + errorFromJson);
            if (errorMessage != nullptr)
                *errorMessage = errorFromJson;
            return false;
        }
    }
    
    juce::String errorMsg = "Failed to parse duration validation response";
    juce::Logger::writeToLog ("ERROR: " + errorMsg);
    juce::Logger::writeToLog ("Raw output was: " + output);
    if (errorMessage != nullptr)
        *errorMessage = errorMsg;
    return false;
}

//==============================================================================
// Plugin Instance Creation
//==============================================================================

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PtV2AProcessor();
}
