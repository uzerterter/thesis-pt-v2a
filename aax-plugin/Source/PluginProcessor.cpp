#include "PluginProcessor.h"
#include "PluginEditor.h"

PtV2AProcessor::PtV2AProcessor()
: AudioProcessor (BusesProperties()
                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

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
    // Try to find Python 3 executable
    // Priority: python3 > python (if version 3.x)
    
#if JUCE_MAC
    // macOS: Try common locations
    juce::StringArray pythonCandidates = {
        "/usr/bin/python3",
        "/usr/local/bin/python3",
        "/opt/homebrew/bin/python3",
        "/Library/Frameworks/Python.framework/Versions/3.12/bin/python3",
        "python3"  // System PATH
    };
#elif JUCE_WINDOWS
    // Windows: Try common locations
    juce::StringArray pythonCandidates = {
        "python",      // Usually Python 3 on modern Windows
        "python3",
        "py -3"        // Python Launcher
    };
#else
    juce::StringArray pythonCandidates = { "python3", "python" };
#endif

    // Test each candidate
    for (const auto& candidate : pythonCandidates)
    {
        juce::ChildProcess testProcess;
        if (testProcess.start (candidate + " --version"))
        {
            juce::String output = testProcess.readAllProcessOutput().trim();
            if (output.contains ("Python 3."))
            {
                DBG ("Found Python: " + candidate + " (" + output + ")");
                return candidate;
            }
        }
    }
    
    DBG ("ERROR: No Python 3 executable found!");
    return "python3";  // Fallback
}

juce::File PtV2AProcessor::getAPIClientScript()
{
    // Path to standalone_api_client.py relative to plugin
    // Assumes plugin is in: aax-plugin/
    // Script is in: companion/standalone_api_client.py
    
    // Get plugin binary location
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory();
    
    DBG ("Plugin directory: " + pluginDir.getFullPathName());
    
    // Navigate to thesis-pt-v2a root based on platform-specific build structure
    juce::File thesisRoot;
    
#if JUCE_WINDOWS
    // Windows AAX build structure:
    // From: build/pt_v2a_artefacts/Debug/AAX/pt_v2a.aaxplugin/Contents/x64/pt_v2a.aaxplugin
    // To:   thesis-pt-v2a/companion/standalone_api_client.py
    
    // Try different navigation paths for different build locations
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
    
    // Also try from installed location
    auto candidate3 = pluginDir.getParentDirectory()      // Contents
                                .getParentDirectory()      // pt_v2a.aaxplugin
                                .getParentDirectory()      // Plug-Ins
                                .getParentDirectory()      // Audio
                                .getParentDirectory();     // May vary
    
    if (candidate1.getChildFile("companion").exists())
        thesisRoot = candidate1;
    else if (candidate2.getChildFile("companion").exists())
        thesisRoot = candidate2;
    else if (candidate3.getChildFile("companion").exists())
        thesisRoot = candidate3;
    else
        thesisRoot = candidate1;  // Fallback to first candidate
        
#elif JUCE_MAC
    // macOS AAX build structure:
    // From: aax-plugin/Builds/MacOSX/build/Debug/pt_v2a.aaxplugin/Contents/MacOS/pt_v2a
    // To:   thesis-pt-v2a/companion/standalone_api_client.py
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
    
    DBG ("Thesis root candidate: " + thesisRoot.getFullPathName());
    
    auto scriptPath = thesisRoot.getChildFile ("companion")
                                 .getChildFile ("standalone_api_client.py");
    
    if (scriptPath.existsAsFile())
    {
        DBG ("Found API client script: " + scriptPath.getFullPathName());
        return scriptPath;
    }
    
    DBG ("Script not found at: " + scriptPath.getFullPathName());
    
    // Fallback: Try relative to current working directory
    auto cwdScript = juce::File::getCurrentWorkingDirectory()
                         .getChildFile ("companion")
                         .getChildFile ("standalone_api_client.py");
    
    if (cwdScript.existsAsFile())
    {
        DBG ("Found API client script in CWD: " + cwdScript.getFullPathName());
        return cwdScript;
    }
    
    DBG ("Script not found at CWD: " + cwdScript.getFullPathName());
    
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
            DBG ("Found API client script via fallback: " + fallbackFile.getFullPathName());
            return fallbackFile;
        }
        DBG ("Script not found at fallback: " + path);
    }
#endif
    
    DBG ("ERROR: API client script not found in any location!");
    DBG ("Current working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
    return juce::File();
}

bool PtV2AProcessor::isAPIAvailable (const juce::String& apiUrl)
{
    // Simple health check: try to reach API root endpoint
    juce::URL healthCheck (apiUrl + "/");
    
    // Use JUCE's URL class to make HTTP GET request
    auto inputStream = healthCheck.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (5000)  // 5 second timeout
    );
    
    if (inputStream != nullptr)
    {
        juce::String response = inputStream->readEntireStreamAsString();
        
        // Check if response contains expected API identifier
        if (response.contains ("MMAudio") || response.contains ("status"))
        {
            DBG ("API is available at: " + apiUrl);
            return true;
        }
    }
    
    DBG ("API not available at: " + apiUrl);
    return false;
}

juce::String PtV2AProcessor::generateAudioFromVideo (
    const juce::File& videoFile,
    const juce::String& prompt,
    const juce::String& negativePrompt,
    int seed,
    juce::String* errorMessage)
{
    DBG ("=== MMAudio Generation Started ===");
    DBG ("Video: " + videoFile.getFullPathName());
    DBG ("Prompt: " + prompt);
    DBG ("Negative Prompt: " + negativePrompt);
    DBG ("Seed: " + juce::String (seed));
    
    // Validate inputs
    if (!videoFile.existsAsFile())
    {
        juce::String error = "Video file does not exist: " + videoFile.getFullPathName();
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Get Python executable
    auto pythonExe = getPythonExecutable();
    
    // Get API client script
    auto scriptFile = getAPIClientScript();
    if (!scriptFile.existsAsFile())
    {
        juce::String error = "API client script not found.\n\n";
        error += "Please check the Pro Tools console for detailed search paths.\n\n";
        error += "Expected location:\n";
        error += "C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\thesis-pt-v2a\\companion\\standalone_api_client.py";
        
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Build command line
    // Format: python3 standalone_api_client.py --video <path> --prompt <text> --quiet
    juce::StringArray args;
    args.add (pythonExe);
    args.add (scriptFile.getFullPathName());
    args.add ("--video");
    args.add (videoFile.getFullPathName());
    
    if (prompt.isNotEmpty())
    {
        args.add ("--prompt");
        args.add (prompt);
    }
    
    if (negativePrompt.isNotEmpty())
    {
        args.add ("--negative-prompt");
        args.add (negativePrompt);
    }
    
    args.add ("--seed");
    args.add (juce::String (seed));
    
    args.add ("--temp");               // Use temp directory for Pro Tools compatibility
    args.add ("--import-to-protools"); // Auto-import to timeline via PTSL
    args.add ("--quiet");              // Minimal output for parsing
    
    // Build command string
    juce::String command;
    for (int i = 0; i < args.size(); ++i)
    {
        // Quote arguments with spaces
        if (args[i].containsChar (' '))
            command += "\"" + args[i] + "\"";
        else
            command += args[i];
        
        if (i < args.size() - 1)
            command += " ";
    }
    
    DBG ("Executing command: " + command);
    
    // Execute subprocess - simple approach without file redirection
    juce::ChildProcess apiProcess;
    if (!apiProcess.start (command))
    {
        juce::String error = "Failed to start API client process";
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Wait for process to complete (with timeout)
    // MMAudio generation typically takes 60-120 seconds
    const int timeoutMs = 300000;  // 5 minutes
    bool completed = apiProcess.waitForProcessToFinish (timeoutMs);
    
    if (!completed)
    {
        apiProcess.kill();
        juce::String error = "API request timed out after " + juce::String (timeoutMs / 1000) + " seconds";
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Read stdout only (stderr will be lost, but safer for Pro Tools)
    juce::String output = apiProcess.readAllProcessOutput().trim();
    
    DBG ("API client output: " + output);
    
    // Check exit code
    int exitCode = apiProcess.getExitCode();
    if (exitCode != 0)
    {
        // Python script failed - output contains error message
        juce::String error = "API client failed with exit code " + juce::String (exitCode);
        
        if (output.isNotEmpty())
        {
            error += "\n\n" + output;
        }
        else
        {
            error += "\n\nNo error message from script. Check:\n";
            error += "1. Python is installed and in PATH\n";
            error += "2. API server is running on localhost:8000\n";
            error += "3. Video file exists and is accessible\n";
            error += "4. Required Python packages are installed";
        }
        
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Parse output path (in quiet mode, it's just the file path)
    juce::File outputFile (output);
    
    if (!outputFile.existsAsFile())
    {
        juce::String error = "Generated audio file not found: " + output;
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    DBG ("=== Generation Successful ===");
    DBG ("Output file: " + outputFile.getFullPathName());
    DBG ("File size: " + juce::String (outputFile.getSize() / 1024) + " KB");
    
    return outputFile.getFullPathName();
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PtV2AProcessor();
}
