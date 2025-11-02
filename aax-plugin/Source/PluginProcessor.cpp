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
            juce::Logger::writeToLog ("API is available at: " + apiUrl);
            return true;
        }
    }
    
    juce::Logger::writeToLog ("API not available at: " + apiUrl);
    return false;
}

juce::String PtV2AProcessor::generateAudioFromVideo (
    const juce::File& videoFile,
    const juce::String& prompt,
    const juce::String& negativePrompt,
    int seed,
    juce::String* errorMessage)
{
    juce::Logger::writeToLog ("=== MMAudio Generation Started ===");
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
    
    // Get API client script
    auto scriptFile = getAPIClientScript();
    if (!scriptFile.existsAsFile())
    {
        juce::String error = "API client script not found.\n\n";
        
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    // Get script directory (for working directory)
    juce::File scriptDir = scriptFile.getParentDirectory();
    juce::String scriptPath = scriptDir.getFullPathName();
    
    juce::Logger::writeToLog ("Script directory: " + scriptPath);
    
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
    
    juce::Logger::writeToLog ("Executing command: " + command);
    
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
        juce::Logger::writeToLog ("ERROR: " + error);
        if (errorMessage != nullptr)
            *errorMessage = error;
        return {};
    }
    
    juce::Logger::writeToLog ("✓ Python process started successfully (running in background)");
    juce::Logger::writeToLog ("   The script will generate audio and import to Pro Tools automatically.");
    juce::Logger::writeToLog ("   Check Pro Tools timeline in ~60-120 seconds for the imported audio.");
    
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
    
    // Build command: python standalone_api_client.py --action get_video_selection
    juce::String command = "\"" + pythonExe + "\" ";
    command += "\"" + scriptFile.getFullPathName() + "\" ";
    command += "--action get_video_selection";
    
    juce::Logger::writeToLog ("Executing timeline selection command...");
    juce::Logger::writeToLog ("Command: " + command);
    
    // Create child process
    juce::ChildProcess process;
    
    if (!process.start (command))
    {
        result.errorMessage = "Failed to start Python process";
        juce::Logger::writeToLog ("ERROR: " + result.errorMessage);
        return result;
    }
    
    // Wait for completion (PTSL calls can take 1-2 seconds)
    if (!process.waitForProcessToFinish (10000))  // 10 second timeout
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
    
    // Read output
    auto output = process.readAllProcessOutput().trim();
    juce::Logger::writeToLog ("Python output: " + output);
    
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
