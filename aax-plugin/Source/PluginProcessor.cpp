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
    auto pluginDir = pluginFile.getParentDirectory();
    
    DBG ("=== Python Executable Search ===");
    DBG ("Plugin directory: " + pluginDir.getFullPathName());
    
    // Path to embedded Python in plugin Resources
    auto embeddedPythonExe = pluginDir.getChildFile("Resources")
                                      .getChildFile("python")
                                      .getChildFile("python.exe");
    
    DBG ("Checking embedded Python: " + embeddedPythonExe.getFullPathName());
    
    if (embeddedPythonExe.existsAsFile())
    {
        DBG ("✓ Using embedded Python from plugin Resources");
        return embeddedPythonExe.getFullPathName();
    }
    
    DBG ("⚠ Embedded Python not found!");
    DBG ("Expected at: " + embeddedPythonExe.getFullPathName());
    DBG ("Make sure Resources/python/ is copied to the plugin bundle");
    
    // Fallback: Try system Python (will likely fail without dependencies)
    return "python.exe";
}

juce::File PtV2AProcessor::getAPIClientScript()
{
    // For embedded Python: Script is bundled in plugin Resources/python/Scripts/
    // This makes the plugin self-contained and portable
    
    // Get plugin binary location
    auto pluginFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto pluginDir = pluginFile.getParentDirectory();
    
    DBG ("Plugin directory: " + pluginDir.getFullPathName());
    
    // Try embedded script first (production/installed builds)
    auto embeddedScript = pluginDir.getChildFile("Resources")
                                   .getChildFile("python")
                                   .getChildFile("Scripts")
                                   .getChildFile("standalone_api_client.py");
    
    DBG ("Checking embedded script: " + embeddedScript.getFullPathName());
    
    if (embeddedScript.existsAsFile())
    {
        DBG ("✓ Using embedded script from plugin Resources");
        return embeddedScript;
    }
    
    DBG ("⚠ Embedded script not found, trying external paths (development fallback)");
    
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
    
    DBG ("Thesis root candidate: " + thesisRoot.getFullPathName());
    
    auto scriptPath = thesisRoot.getChildFile ("companion")
                                 .getChildFile ("standalone_api_client.py");
    
    if (scriptPath.existsAsFile())
    {
        DBG ("Found API client script (external): " + scriptPath.getFullPathName());
        return scriptPath;
    }
    
    DBG ("Script not found at: " + scriptPath.getFullPathName());
    
    // Last resort: Try relative to current working directory
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
    
    // Get script directory (for working directory)
    juce::File scriptDir = scriptFile.getParentDirectory();
    juce::String scriptPath = scriptDir.getFullPathName();
    
    DBG ("Script directory: " + scriptPath);
    
    // Build command line arguments
    juce::StringArray args;
    
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
    
    // Build command - simple and clean!
    // Embedded Python has everything: py-ptsl in site-packages, ptsl_integration in site-packages
    // No PYTHONPATH needed - the plugin is completely self-contained
    juce::String command;
    
#if JUCE_WINDOWS
    // Windows: Just cd to script directory and run
    command = "cmd /c \"";
    command += "cd /d \"" + scriptPath + "\" && ";  // Change to script directory
    command += "\"" + pythonExe + "\" ";            // Embedded Python executable
    command += "\"" + scriptFile.getFileName() + "\"";  // Script (we're in the right dir)
#else
    // macOS/Linux: Same simple approach
    command = "cd \"" + scriptPath + "\" && ";
    command += "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFileName() + "\"";
#endif
    
    // Add arguments (skip first arg which is the script path)
    for (int i = 1; i < args.size(); ++i)
    {
        command += " ";
        // Quote arguments with spaces
        if (args[i].containsChar (' '))
            command += "\"" + args[i] + "\"";
        else
            command += args[i];
    }
    
#if JUCE_WINDOWS
    command += "\"";  // Close cmd /c quote
#endif
    
    DBG ("Executing command: " + command);
    
    // Execute subprocess in BACKGROUND (non-blocking)
    // This is CRITICAL: If we wait for the Python process to finish, Pro Tools freezes
    // and PTSL cannot respond, causing a deadlock/crash!
    //
    // Instead: Start Python process and return immediately.
    // The Python script will handle everything (MMAudio API + PTSL import)
    juce::ChildProcess apiProcess;
    if (!apiProcess.start (command))
    {
        juce::String error = "Failed to start API client process";
        DBG ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    DBG ("✓ Python process started successfully (running in background)");
    DBG ("   The script will generate audio and import to Pro Tools automatically.");
    DBG ("   Check Pro Tools timeline in ~60-120 seconds for the imported audio.");
    
    // Return success message immediately
    // Note: We don't know the output path yet, but the script will handle it
    juce::String successMessage = "Audio generation started in background.\n\n";
    successMessage += "The process will:\n";
    successMessage += "1. Generate audio via MMAudio API (~60s)\n";
    successMessage += "2. Import audio to Pro Tools timeline via PTSL\n\n";
    successMessage += "Check your Pro Tools timeline in 1-2 minutes.";
    
    if (errorMessage != nullptr)
        *errorMessage = "";  // Clear any previous error
    
    // Return placeholder path (actual path will be in temp folder)
    // The Python script handles everything, so we just return success
    return "background_generation_in_progress";
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
            // Can't create directory - logging will fail
            DBG ("Failed to create log directory: " + result.getErrorMessage());
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
        DBG ("Cleaned up " + juce::String(deletedCount) + " old log files");
    
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
// Plugin Instance Creation
//==============================================================================

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PtV2AProcessor();
}
