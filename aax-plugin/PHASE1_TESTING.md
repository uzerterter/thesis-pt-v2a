# Phase 1: Python Subprocess Integration - Testing Guide

## ✅ Implementation Complete!

### What We Built:

1. **API Communication Layer** in `PluginProcessor.cpp`:
   - `generateAudioFromVideo()` - Main API call function
   - `isAPIAvailable()` - Health check for API server
   - `getAPIClientScript()` - Locates Python client script
   - `getPythonExecutable()` - Finds Python 3 interpreter

2. **UI Integration** in `PluginEditor.cpp`:
   - Enhanced render button with full API workflow
   - API availability check before generation
   - User feedback with alert dialogs
   - Status messages during generation

3. **Error Handling**:
   - Python executable detection (macOS/Windows/Linux)
   - Script path resolution (relative + absolute fallback)
   - API availability check with 5s timeout
   - Video file validation
   - Exit code checking
   - Timeout handling (5 minutes for generation)

---

## 🧪 Testing Steps

### Prerequisites:

1. **API Server Running:**

   ```bash
   docker restart mmaudio-api
   # Wait 10 seconds for startup
   docker logs -f mmaudio-api  # Verify it's running
   ```

2. **Test Video Available:**

   ```bash
   # Default test video location:
   ls -lh /mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4
   ```

3. **Python Client Accessible:**

   ```bash
   python3 /mnt/disk1/users/ludwig/ludwig-thesis/thesis-pt-v2a/companion/standalone_api_client.py --help
   ```

---

### Test 1: Build the Plugin

```bash
cd /mnt/disk1/users/ludwig/ludwig-thesis/thesis-pt-v2a/aax-plugin

# Configure CMake (if not done yet)
cmake -B Builds -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build Builds --config Debug

# Check build success
ls -lh Builds/pt_v2a_artefacts/Debug/AAX/pt_v2a.aaxplugin
```

**Expected Output:**

- Build completes without errors

- AAX plugin bundle created in `Builds/pt_v2a_artefacts/Debug/AAX/`

---

### Test 2: API Availability Check

**In Plugin Code:**
The plugin automatically checks API availability when "Render Audio" is clicked.

**Manual Test:**

```bash
curl http://localhost:8000/
# Expected: {"message":"MMAudio Standalone API","status":"running",...}

curl http://localhost:8000/models
# Expected: {"available_models":[...],"loaded_models":[...]}
```

---

### Test 3: Python Executable Detection

**Debug Output (check JUCE console):**
When plugin loads, you should see:

```
Found Python: /usr/bin/python3 (Python 3.12.x)
Found API client script: /path/to/standalone_api_client.py
```

**Manual Test:**

```bash
# Test Python detection logic
/usr/bin/python3 --version
# or
python3 --version

# Test Python client directly
python3 companion/standalone_api_client.py \
  --video model-tests/data/MMAudio_examples/noSound/sora_beach.mp4 \
  --prompt "ocean waves" \
  --quiet
# Expected: /path/to/generated_audio_xxxxx_42.flac
```

---

### Test 4: End-to-End Generation (Plugin UI)

1. **Load Plugin in Pro Tools** (or JUCE AudioPluginHost for testing):

   ```bash
   # Quick test without Pro Tools (if you have AudioPluginHost)
   /path/to/AudioPluginHost Builds/pt_v2a_artefacts/Debug/AAX/pt_v2a.aaxplugin
   ```

2. **Plugin UI Workflow:**
   - Enter prompt: `"ocean waves and seagulls"`
   - Click "Render Audio"
   - Wait for "Checking API..." → "Generating..." (60-120s)
   - Success dialog appears with output file path

3. **Check Console Logs:**

   ```
   === Render Button Clicked ===
   Prompt: ocean waves and seagulls
   === MMAudio Generation Started ===
   Video: /path/to/sora_beach.mp4
   Prompt: ocean waves and seagulls
   Negative Prompt: voices, music
   Seed: 42
   Executing command: python3 /path/to/standalone_api_client.py --video ... --quiet
   API client output: /path/to/generated_audio_xxxxx_42.flac
   === Generation Successful ===
   Output file: /path/to/generated_audio_xxxxx_42.flac
   File size: 1234 KB
   ```

4. **Verify Generated Audio:**

   ```bash
   ls -lh /path/to/generated_audio_*.flac
   # Play audio to verify
   ffplay /path/to/generated_audio_xxxxx_42.flac
   ```

---

### Test 5: Error Scenarios

#### Test 5a: API Not Running

```bash
docker stop mmaudio-api
```
- Click "Render Audio" in plugin
- **Expected:** Alert dialog: "MMAudio API is not running! Please start..."

#### Test 5b: Invalid Video Path

- Edit `PluginEditor.cpp` line with test video path to non-existent file
- Rebuild plugin
- Click "Render Audio"
- **Expected:** Alert dialog: "Test video file not found: ..."

#### Test 5c: Python Not Found

- Edit `getPythonExecutable()` to return invalid path
- Rebuild plugin
- Click "Render Audio"
- **Expected:** Error: "Failed to start API client process"

#### Test 5d: API Timeout

- Temporarily reduce timeout in `generateAudioFromVideo()` to 10 seconds
- Click "Render Audio"
- **Expected:** Error after 10s: "API request timed out after 10 seconds"

---

## 📊 Success Criteria

✅ **Build:** Plugin compiles without errors  
✅ **Detection:** Python and script are found automatically  
✅ **API Check:** Plugin detects when API is unavailable  
✅ **Generation:** Successfully generates audio from test video  
✅ **Feedback:** User sees clear status messages and errors  
✅ **Timeout:** Long generations don't freeze Pro Tools UI  
✅ **Output:** Generated FLAC file exists and is playable  

---

## 🐛 Known Issues & Limitations

### Current Limitations:

1. **Hardcoded Test Video:**
   - Currently uses fixed test video path
   - **TODO Phase 2:** Extract actual video from Pro Tools timeline

2. **Synchronous UI:**
   - "Generating..." button blocks UI thread
   - **TODO Phase 3:** Move to background thread with progress updates

3. **No Progress Indication:**
   - User sees "Generating..." for 60-120 seconds with no updates
   - **TODO Phase 3:** Add progress bar or time estimate

4. **Fixed Parameters:**
   - Negative prompt: hardcoded to "voices, music"
   - Seed: hardcoded to 42
   - **TODO Phase 2:** Add UI controls for these parameters

5. **No Audio Import:**
   - Generated audio file path is shown in dialog, but not imported to timeline
   - **TODO Phase 4:** Implement automatic timeline import

### Platform-Specific Notes:

#### macOS:

- Python 3 usually at `/usr/bin/python3` or `/opt/homebrew/bin/python3`
- AAX plugins go in: `~/Library/Application Support/Avid/Audio/Plug-Ins/`

#### Windows:

- Python detection tries `python`, `python3`, and `py -3`
- Subprocess command syntax may need adjustment for Windows paths with spaces

#### Linux:

- Python 3 usually at `/usr/bin/python3`
- AAX plugins for Pro Tools on Linux are rare (most use VST3)

---

## 🔍 Debugging Tips

### Enable Verbose Logging:

```cpp
// In PluginProcessor.cpp, add more DBG() statements:
DBG ("Current working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
DBG ("Python executable: " + getPythonExecutable());
DBG ("Script path: " + getAPIClientScript().getFullPathName());
```

### Check JUCE Console Output:

- In Pro Tools: Window → Show Console
- In AudioPluginHost: Console output in terminal
- Look for `DBG()` messages and errors

### Test Python Client Standalone:

```bash
# Dry-run to verify Python client works
cd /mnt/disk1/users/ludwig/ludwig-thesis/thesis-pt-v2a
python3 companion/standalone_api_client.py \
  --video model-tests/data/MMAudio_examples/noSound/sora_beach.mp4 \
  --prompt "test" \
  --quiet

# If this fails, fix Python client before testing plugin
```

### Check API Logs:

```bash
# Real-time API logs
docker logs -f mmaudio-api

# Look for:
# - Video upload: "💾 SMART CACHE MISS: Processing 'tmp....mp4'..."
# - Generation: "Generating audio: prompt='...', duration=..."
# - Cache stats: "💾 CACHED: ... (167.1MB, total: ...)"
```

---

## ✅ Next Steps (Phase 2 & Beyond)

Once this phase is working:

1. **Phase 2 - Enhanced UI:**
   - Add negative prompt input field
   - Add seed input (with random button)
   - Add status label (instead of button text changes)
   - Add settings panel for API URL

2. **Phase 3 - Async Processing:**
   - Create `GenerationThread` class
   - Move API call to background thread
   - Add progress bar with time estimate
   - Implement cancellation

3. **Phase 4 - Timeline Integration:**
   - Research AAX API for video extraction
   - Replace test video with actual Pro Tools video region
   - Implement audio import to timeline
   - Sample-accurate alignment

4. **Phase 5 - Production Polish:**
   - Cross-platform testing (macOS + Windows)
   - Comprehensive error handling
   - User documentation
   - Installer package

---

## 📞 Support

**If you encounter issues:**

1. Check API is running: `curl http://localhost:8000/`
2. Check Python works: `python3 --version`
3. Check Python client works: `python3 companion/standalone_api_client.py --help`
4. Review JUCE console logs for detailed error messages
5. Check Docker logs: `docker logs mmaudio-api`

**Common Fixes:**

- **"Python not found"**: Install Python 3.8+ and add to PATH
- **"Script not found"**: Check file exists at `/path/to/companion/standalone_api_client.py`
- **"API not available"**: `docker restart mmaudio-api` and wait 10s
- **"Generation timeout"**: Check GPU is available in Docker, or increase timeout

---

## 🎉 Completion Checklist

Before moving to Phase 2:

- [ ] Plugin builds successfully
- [ ] Python executable is detected automatically
- [ ] API client script is found
- [ ] API health check works
- [ ] Test video generates audio successfully
- [ ] Generated audio file is playable
- [ ] Error messages are clear and helpful
- [ ] Console logs show detailed debug information

**When all boxes are checked, Phase 1 is complete!** 🚀
