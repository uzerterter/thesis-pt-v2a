# Phase 4: PTSL Integration - Testing Guide

## ✅ What's Been Implemented

### 1. PTSL Python Client (`ptsl_integration/ptsl_client.py`)
- ✅ Connection to Pro Tools PTSL server (port 31416)
- ✅ RegisterConnection command
- ✅ Import audio to timeline command
- ✅ Task status polling
- ✅ JSON-based request/response handling

### 2. Integration into `standalone_api_client.py`
- ✅ New flag: `--import-to-protools`
- ✅ Auto-import after audio generation
- ✅ Fallback if PTSL not available

### 3. Plugin Integration (`PluginProcessor.cpp`)
- ✅ Added `--import-to-protools` flag to Python call
- ✅ Audio generates → imports automatically

## 🧪 Testing Steps

### Test 1: PTSL Connection
```powershell
cd companion/ptsl_integration
..\venv_ptsl\Scripts\Activate.ps1
python -c "from ptsl_client import PTSLClient; c = PTSLClient(); print('✅ OK' if c.connect() else '❌ Failed')"
```

**Expected:** `✅ OK`

### Test 2: Manual Audio Import (with existing file)
```powershell
cd companion/ptsl_integration
python test_integration.py
```

**Expected:**
- Connects to Pro Tools
- Finds test audio in temp directory
- Imports to Pro Tools timeline
- Creates new track with audio

### Test 3: End-to-End (Python Script)
```powershell
cd companion
python standalone_api_client.py `
    --video "path/to/video.mp4" `
    --prompt "test audio" `
    --import-to-protools
```

**Expected:**
1. Generates audio via MMAudio API
2. Saves to temp directory
3. Imports to Pro Tools timeline automatically
4. Shows success message

### Test 4: Full Plugin Test (After Compilation)

**Prerequisites:**
1. Pro Tools is running
2. PTSL is enabled (Setup > Preferences > MIDI > Enable PTSL)
3. A session is open in Pro Tools
4. MMAudio API server is running (localhost:8000)

**Steps:**
1. Compile plugin:
   ```powershell
   cd build
   cmake --build . --config Debug --target pt_v2a_AAX
   ```

2. Load plugin in Pro Tools

3. Click "Render Audio"

**Expected:**
- Plugin shows "Generating..." 
- After ~60-120 seconds: "Generation Complete!"
- Audio appears on new track in Pro Tools timeline
- Success dialog shows path to audio file

## 🐛 Troubleshooting

### "PTSL: Connection failed - Connection refused"
**Solution:**
- Check Pro Tools is running
- Verify PTSL is enabled: Setup > Preferences > MIDI
- Restart Pro Tools if needed

### "PTSL import failed - audio file saved but not imported"
**Solution:**
- Ensure a Pro Tools session is open
- Check PTSL connection with Test 1
- Verify audio file path is absolute

### "No test audio file found in temp directory"
**Solution:**
- Run the plugin once to generate audio
- Or place a test .flac file in: `C:\Users\LUDENB~1\AppData\Local\Temp\pt_v2a_outputs\`

### Plugin shows "Generation Failed"
**Solution:**
1. Check MMAudio API is running: `http://localhost:8000/health`
2. Check Python environment has `requests` installed
3. Look at Pro Tools console for detailed errors

## 📊 Current Limitations

1. **No video clip selection from timeline yet**
   - Currently uses hardcoded test video
   - Phase 5 will add timeline video clip detection

2. **Fixed import location**
   - Always imports at SessionStart
   - Future: Import at video clip location

3. **No progress indication during import**
   - User sees "Generation Complete!" immediately
   - Import happens in background (usually < 5 seconds)

## 🎯 Next Steps (Phase 5)

1. **Video Clip Selection:**
   - Use PTSL to query active clips on timeline
   - Filter for video clips
   - Get clip location and duration
   - Pass video file path to MMAudio API

2. **Smart Placement:**
   - Import audio at same timeline position as video
   - Match duration to video length
   - Place on track below video track

3. **UI Improvements:**
   - Show import progress
   - Option to disable auto-import
   - Manual track selection

## ✅ Success Criteria

Phase 4 is complete when:
- [x] PTSL connection works
- [x] Audio can be imported programmatically
- [x] Plugin triggers auto-import
- [ ] Full end-to-end test passes (needs compilation)
- [ ] Audio appears correctly on Pro Tools timeline

## 📝 Notes

- PTSL uses JSON-based request/response system
- All commands go through `SendGrpcRequest()`
- Import is async - requires task status polling
- Protobuf files must be regenerated if PTSL.proto changes
