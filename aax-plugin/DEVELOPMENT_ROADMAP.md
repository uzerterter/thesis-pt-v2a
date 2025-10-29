# AAX Plugin Development Roadmap
## MMAudio Pro Tools Integration - Minimal Working Plugin

**Goal:** Create a functional AAX plugin that allows Pro Tools users to select a video clip (5-12s) from their timeline and generate audio using the MMAudio API.

---

## 📋 Current State Assessment

### ✅ Already Implemented
- **MMAudio Standalone API** (FastAPI server)
  - Content-based video caching (32GB, 90min TTL)
  - Background TTL cleanup thread
  - Model caching in VRAM
  - `/generate` endpoint with comprehensive parameters
  - Cache statistics and monitoring (`/cache/stats`)
  - Port 8000, running in Docker container

- **Python CLI Client** (`standalone_api_client.py`)
  - Interactive and CLI modes
  - Pro Tools ready (quiet mode for subprocess integration)
  - Video validation (MP4, MOV, AVI, etc.)
  - Comprehensive error handling

- **Basic AAX Plugin Structure**
  - JUCE-based AAX plugin skeleton
  - CMake build configuration
  - Basic UI (prompt input, render button)
  - Stereo audio I/O support
  - Pass-through audio processing

### 🔧 What's Missing
- Video extraction from Pro Tools timeline
- API communication from C++ plugin
- Async audio generation with progress feedback
- Generated audio import back into Pro Tools
- Error handling and user feedback
- Parameter persistence (prompt, seed, etc.)

---

## 🎯 Development Phases

### **Phase 1: Core Infrastructure** (Foundation)
**Goal:** Establish basic API communication and video handling

#### Tasks:
1. **Video Export from Pro Tools**
   - [ ] Research AAA (AAX Audio Access) API for timeline access
   - [ ] Implement video region selection mechanism
   - [ ] Export selected video region to temporary file
   - [ ] Validate video duration (5-12s constraint)
   - [ ] Handle video format conversion if needed

2. **API Client Integration**
   - [ ] **Decision Point:** HTTP client vs subprocess approach
     - **Option A (Recommended):** Use Python subprocess
       ```cpp
       // C++ calls: python standalone_api_client.py --video /tmp/clip.mov --quiet
       // Parse output path from stdout
       ```
       ✅ Pros: Reuse existing robust client, easier debugging
       ❌ Cons: Python dependency, subprocess overhead
     
     - **Option B:** Native C++ HTTP client (JUCE or cpr)
       ```cpp
       // Direct HTTP POST to localhost:8000/generate
       // Handle multipart/form-data, file upload, etc.
       ```
       ✅ Pros: No external dependencies, faster
       ❌ Cons: More complex, need to reimplement error handling
   
   - [ ] Implement chosen API communication method
   - [ ] Test basic video → audio generation flow
   - [ ] Handle API connection errors gracefully

3. **File Management**
   - [ ] Create temporary video export directory
   - [ ] Manage generated audio files
   - [ ] Clean up temporary files after import
   - [ ] Handle file paths cross-platform (macOS/Windows)

**Deliverable:** Plugin can export video, call API, receive generated audio file path

---

### **Phase 2: User Interface** (UX/Interaction)
**Goal:** Create intuitive UI for parameter control and feedback

#### Tasks:
1. **Parameter Input UI**
   - [ ] Prompt text input (existing, enhance)
   - [ ] Negative prompt input
   - [ ] Seed input (with random button)
   - [ ] Duration display (from selected region)
   - [ ] Advanced options (collapsible):
     - [ ] Model selection dropdown
     - [ ] Steps slider (10-50)
     - [ ] CFG strength slider (1.0-10.0)

2. **Status & Progress UI**
   - [ ] Status indicator component
     - 🔴 Idle
     - 🟡 Exporting video...
     - 🟡 Generating audio... (with progress if possible)
     - 🟢 Complete!
     - 🔴 Error: [message]
   - [ ] Progress bar (indeterminate or percentage-based)
   - [ ] Generation time estimate
   - [ ] Cancel button (abort API request)

3. **Action Buttons**
   - [ ] "Generate Audio" button (replaces current stub)
   - [ ] "Import to Timeline" button (after generation complete)
   - [ ] "Settings" button (API URL, timeout, etc.)
   - [ ] "Clear Cache" button (call `/cache/clear` endpoint)

4. **Visual Polish**
   - [ ] Modern color scheme (dark theme for Pro Tools integration)
   - [ ] Consistent spacing and padding
   - [ ] Tooltips for all controls
   - [ ] Keyboard shortcuts (Enter = Generate, Esc = Cancel)

**Deliverable:** Fully functional UI with all controls and real-time feedback

---

### **Phase 3: Async Processing** (Performance)
**Goal:** Non-blocking audio generation without freezing Pro Tools

#### Tasks:
1. **Threading Architecture**
   - [ ] Move API calls to background thread
     ```cpp
     class GenerationThread : public juce::Thread
     {
         void run() override
         {
             // Call API, monitor progress
             // Use MessageManager for UI updates
         }
     };
     ```
   - [ ] Implement thread-safe communication with UI
   - [ ] Handle thread cancellation gracefully

2. **Progress Monitoring**
   - [ ] Poll API for generation status (if available)
   - [ ] Update progress bar in real-time
   - [ ] Display estimated time remaining
   - [ ] Handle timeout scenarios (default 5 minutes)

3. **State Management**
   - [ ] Define plugin states: Idle, Processing, Complete, Error
   - [ ] Prevent multiple simultaneous generations
   - [ ] Disable/enable UI controls based on state
   - [ ] Persist state across plugin window open/close

**Deliverable:** Smooth async audio generation without blocking Pro Tools UI

---

### **Phase 4: Audio Import** (Integration)
**Goal:** Seamlessly import generated audio back into Pro Tools

#### Tasks:
1. **Timeline Integration**
   - [ ] Research AAX API for audio import
   - [ ] Create new audio track (if needed)
   - [ ] Import generated audio file at original video location
   - [ ] Match audio length to video region
   - [ ] Set proper audio gain/levels

2. **Alignment & Synchronization**
   - [ ] Ensure sample-accurate alignment with video
   - [ ] Handle different sample rates (44.1k vs 48k vs 96k)
   - [ ] Preserve timeline position and track routing
   - [ ] Option to replace existing audio or create new track

3. **Post-Import Actions**
   - [ ] Auto-select imported audio region
   - [ ] Display confirmation message
   - [ ] Clean up temporary files
   - [ ] Log import details for debugging

**Deliverable:** One-click audio generation → automatic timeline import

---

### **Phase 5: Error Handling & Polish** (Robustness)
**Goal:** Production-ready plugin with comprehensive error handling

#### Tasks:
1. **Error Scenarios**
   - [ ] API server not running
     - Show clear error message
     - Provide instructions to start API
     - Offer retry mechanism
   - [ ] Video export failed
     - Check Pro Tools timeline state
     - Validate selected region
     - Handle edge cases (no video track, empty region, etc.)
   - [ ] API request timeout
     - Allow user-configurable timeout
     - Show elapsed time
     - Graceful cancellation
   - [ ] Invalid video duration (< 5s or > 12s)
     - Show warning with actual duration
     - Suggest adjusting selection
     - Optional: auto-trim to valid range
   - [ ] Generated audio format mismatch
     - Handle FLAC → WAV conversion if needed
     - Ensure sample rate compatibility
   - [ ] Disk space issues
     - Check available space before export
     - Clean up old temporary files

2. **User Feedback**
   - [ ] Detailed error messages (no cryptic technical jargon)
   - [ ] Actionable suggestions for each error type
   - [ ] Log errors to Pro Tools system log
   - [ ] Optional: send error reports (privacy-aware)

3. **Configuration & Preferences**
   - [ ] Persistent settings storage (XML or JSON)
     - API URL (default: localhost:8000)
     - Default prompt / negative prompt
     - Preferred model
     - Timeout duration
     - Temporary file directory
   - [ ] Settings panel in plugin UI
   - [ ] Reset to defaults button

4. **Testing & Validation**
   - [ ] Unit tests for API communication
   - [ ] Integration tests with real Pro Tools session
   - [ ] Stress testing (multiple rapid generations)
   - [ ] Cross-platform testing (macOS + Windows)
   - [ ] Test with various video formats and durations

**Deliverable:** Robust, user-friendly plugin ready for production use

---

## 🛠️ Technical Decisions to Make

### Decision 1: API Communication Method
**When:** Phase 1, Task 2

**Options:**
- **A. Python Subprocess (Recommended for MVP)**
  - Reuse `standalone_api_client.py` (already tested)
  - Example:
    ```cpp
    juce::ChildProcess apiCall;
    juce::String cmd = "python3 /path/to/standalone_api_client.py "
                       "--video /tmp/video.mp4 --prompt 'drums' --quiet";
    apiCall.start(cmd);
    juce::String outputPath = apiCall.readAllProcessOutput();
    ```
  - **Pros:** Fast to implement, robust error handling already done
  - **Cons:** Python dependency, subprocess overhead (~200ms)

- **B. Native C++ HTTP Client**
  - Use JUCE's `juce::URL::createInputStream()` or external library (cpr, cpp-httplib)
  - **Pros:** No external dependencies, potentially faster
  - **Cons:** Need to implement multipart/form-data upload, error handling, etc.

**Recommendation:** Start with **Option A** for rapid prototyping, migrate to **Option B** if performance becomes critical.

---

### Decision 2: Video Extraction Method
**When:** Phase 1, Task 1

**Options:**
- **A. AAX Audio Suite Style** (Offline processing)
  - User selects region in Pro Tools
  - Plugin exports video as temporary file
  - Processes offline, then imports result
  - **Pros:** Standard Pro Tools workflow, simpler implementation
  - **Cons:** Requires user selection before plugin opens

- **B. Real-Time Region Detection**
  - Plugin reads current timeline position
  - Auto-detects video tracks and regions
  - Offers region selection dropdown in UI
  - **Pros:** More integrated UX
  - **Cons:** Complex AAX API usage, may not be possible with AAX restrictions

**Recommendation:** Start with **Option A** (Audio Suite style), investigate **Option B** for future enhancement.

---

### Decision 3: Progress Feedback Granularity
**When:** Phase 3, Task 2

**Options:**
- **A. Indeterminate Progress** ("Generating audio... please wait")
  - Simple spinner/activity indicator
  - No percentage or time estimate
  - **Pros:** Easy to implement
  - **Cons:** Poor UX for long generations (60-90s)

- **B. Estimated Progress** (based on typical generation time)
  - Use average generation time from past requests
  - Show estimated time remaining
  - **Pros:** Better UX, feels more responsive
  - **Cons:** Estimates may be inaccurate

- **C. Real-Time Progress from API**
  - Modify API to support progress polling endpoint
  - Show actual generation percentage (e.g., "Step 18/25")
  - **Pros:** Most accurate, best UX
  - **Cons:** Requires API modifications, more complex

**Recommendation:** Start with **Option A**, upgrade to **Option B** or **C** based on user feedback.

---

## 📦 Dependencies & Requirements

### Runtime Requirements
- **Pro Tools:** 2020.11+ (for AAX SDK compatibility)
- **Operating System:** macOS 10.15+ or Windows 10+
- **API Server:** MMAudio Standalone API running on localhost:8000
- **Python:** 3.8+ (if using subprocess approach)
- **CUDA/GPU:** Required for API server (not for plugin itself)

### Build Requirements
- **CMake:** 3.21+
- **Compiler:** Xcode 12+ (macOS) or Visual Studio 2019+ (Windows)
- **JUCE:** 7.0+ (submodule in external/JUCE)
- **AAX SDK:** Latest from Avid Developer account
- **AAX_SDK_PATH:** Environment variable pointing to AAX SDK

### Optional Libraries (for native HTTP client)
- **cpr:** Modern C++ HTTP client (https://github.com/libcpr/cpr)
- **cpp-httplib:** Single-header HTTP library (https://github.com/yhirose/cpp-httplib)

---

## 🚀 Quick Start Guide (For Each Phase)

### Phase 1 - Getting Started
1. **Choose API communication method** (subprocess vs HTTP)
2. **Implement video export stub:**
   ```cpp
   // In PluginProcessor.h
   void exportVideoRegion(const juce::File& outputFile);
   ```
3. **Test API call from plugin:**
   ```cpp
   // In render button onClick
   auto videoFile = juce::File("/tmp/test_video.mp4");
   auto audioFile = callMMAudioAPI(videoFile, prompt.getText());
   juce::Logger::writeToLog("Generated: " + audioFile.getFullPathName());
   ```
4. **Verify end-to-end flow** with test video file

### Phase 2 - UI Development
1. **Sketch UI layout** (pen & paper or Figma)
2. **Implement UI components** in `PluginEditor.cpp`
3. **Wire up event handlers** (buttons, text inputs, etc.)
4. **Test UI responsiveness** and accessibility

### Phase 3 - Threading
1. **Create GenerationThread class**
2. **Move API call to background thread**
3. **Use MessageManager::callAsync()** for UI updates
4. **Test thread cancellation** and cleanup

### Phase 4 - Audio Import
1. **Research AAX Audio Import API** (check Avid documentation)
2. **Implement `importAudioToTimeline()` function**
3. **Test with various Pro Tools session configurations**
4. **Verify sample-accurate alignment**

### Phase 5 - Polish
1. **Add error handling** to all critical paths
2. **Create settings panel** with persistent storage
3. **Write user documentation** (README, tooltips)
4. **Comprehensive testing** (automated + manual)

---

## 📊 Success Metrics

### Minimal Working Plugin (MVP) Criteria:
✅ **Functionality:**
- [ ] Export 5-12s video region from Pro Tools
- [ ] Generate audio via MMAudio API (with prompt)
- [ ] Import generated audio back to timeline
- [ ] Complete workflow in < 2 minutes (for 10s video)

✅ **User Experience:**
- [ ] Clear status feedback at each step
- [ ] Graceful error handling (no crashes)
- [ ] Intuitive UI (no manual required for basic use)
- [ ] Non-blocking (Pro Tools remains responsive)

✅ **Stability:**
- [ ] No memory leaks in 10+ consecutive generations
- [ ] Handles API server disconnection gracefully
- [ ] Proper cleanup of temporary files
- [ ] Cross-platform compatibility (macOS + Windows)

---

## 🔮 Future Enhancements (Post-MVP)

### Nice-to-Have Features:
- **Batch Processing:** Generate audio for multiple video regions at once
- **History Panel:** Browse and reuse previously generated audio
- **Preset System:** Save/load favorite prompts and settings
- **A/B Comparison:** Generate multiple variations with different seeds
- **Waveform Preview:** Show generated audio waveform before import
- **Drag & Drop:** Drag video files directly into plugin UI
- **Cloud API Support:** Connect to remote MMAudio API server
- **Collaborative Features:** Share prompts/settings with team members

### Advanced Integrations:
- **Timeline Automation:** Auto-detect all video regions and batch generate
- **Markers & Metadata:** Embed generation parameters in Pro Tools markers
- **Sidechain Analysis:** Use video motion/brightness to influence audio
- **MIDI Trigger:** Generate audio on MIDI note input
- **VST3/AU Versions:** Expand beyond AAX for other DAWs

---

## 📝 Next Immediate Action

### **Recommended Starting Point:**

**Task:** Implement Python subprocess API client integration (Phase 1, Task 2, Option A)

**Why:** 
- Fastest path to working prototype
- Reuses tested `standalone_api_client.py`
- Minimal risk, easy to debug

**Steps:**
1. Add subprocess helper class to `PluginProcessor.cpp`
2. Wire up "Render" button to call subprocess
3. Test with hardcoded video file
4. Display result in JUCE console log

**Estimated Time:** 1-2 hours

**Would you like me to implement this first step now?**

