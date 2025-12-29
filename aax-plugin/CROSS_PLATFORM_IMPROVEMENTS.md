# Cross-Platform Improvements - Optional Tasks

This document lists optional improvements for better cross-platform compatibility. These are **not critical** for basic Mac support but represent best practices.

## 1. Subprocess Command Execution

### Current State
Some `ChildProcess.start()` calls use **String commands** with manual quoting:

```cpp
// PluginProcessor.cpp (multiple locations)
juce::String command = "\"" + pythonExe + "\" ";
command += "\"" + scriptFile.getFullPathName() + "\" ";
command += "--action check_ffmpeg";

juce::ChildProcess process;
process.start(command);  // String - relies on shell parsing
```

### Potential Issue
- **Windows:** Uses `cmd.exe` to parse the command string
- **Mac/Linux:** Uses `sh` to parse the command string
- Different shell behaviors can cause issues with special characters, paths with spaces, etc.

### Recommended Improvement
Use **StringArray** for all commands (already done in PluginEditor.cpp):

```cpp
// Better approach (cross-platform)
juce::StringArray commandArray;
commandArray.add(pythonExe);
commandArray.add("-X");
commandArray.add("utf8");
commandArray.add(scriptFile.getFullPathName());
commandArray.add("--action");
commandArray.add("check_ffmpeg");

juce::ChildProcess process;
process.start(commandArray);  // No shell - direct execution
```

### Benefits
- ✅ No shell injection risks
- ✅ Consistent behavior on Windows/Mac/Linux
- ✅ Handles paths with spaces automatically
- ✅ No manual quoting needed

### Locations to Update (Non-Critical)
1. `PluginProcessor.cpp:920` - `checkFFmpegAvailability()`
2. `PluginProcessor.cpp:1156` - `getVideoFileFromProTools()`
3. `PluginProcessor.cpp:1270` - `prepareVideoSegment()`
4. `PluginProcessor.cpp:1359` - (another command)
5. `PluginProcessor.cpp:1452` - (another command)

**Note:** Current implementation works because paths are properly quoted. This is an enhancement for robustness.

---

## 2. Temp Directory Strategy (Mac-Specific)

### Current Implementation
```python
# companion/video/ffmpeg.py
temp_dir = Path(tempfile.gettempdir()) / "pt_v2a"
```

### Mac Consideration
- `/tmp` is periodically cleaned by macOS (every 3 days for files not accessed)
- Better practice: Use `~/Library/Caches/` for Mac

### Suggested Enhancement
```python
import platform

if platform.system() == "Darwin":  # macOS
    cache_dir = Path.home() / "Library" / "Caches" / "PTV2A"
else:
    cache_dir = Path(tempfile.gettempdir()) / "pt_v2a"

cache_dir.mkdir(parents=True, exist_ok=True)
```

**Note:** Current implementation is fine for short-lived temp files (video preprocessing takes <1 minute).

---

## 3. Log File Locations

### Current Implementation (Already Correct!)
```cpp
// PluginProcessor.cpp - Uses JUCE's cross-platform paths
auto logDir = juce::File::getSpecialLocation(
    juce::File::userApplicationDataDirectory
).getChildFile("PTV2A");
```

**Resolves to:**
- **Windows:** `C:\Users\[username]\AppData\Roaming\PTV2A\`
- **Mac:** `~/Library/Application Support/PTV2A/`

✅ **No changes needed** - already cross-platform!

---

## 4. File Path Separators (Python)

### Current State
Most code uses `pathlib.Path` (cross-platform):
```python
from pathlib import Path
output_path = Path(temp_dir) / "output.mp4"  # ✅ Works everywhere
```

### Remaining `os.path.join` Usages
Some older code still uses:
```python
import os
output_path = os.path.join(temp_dir, "output.mp4")  # ⚠️ Works, but old style
```

### Recommended (Low Priority)
Convert all `os.path.join` to `Path()` for consistency:
```python
# Old
output = os.path.join(dir, file)

# New
output = Path(dir) / file
```

---

## 5. Python Package Availability (Mac)

### Critical Packages for Mac
All required packages have **Mac wheels** available:

| Package | Mac ARM64 | Mac x86_64 | Universal2 |
|---------|-----------|------------|------------|
| torch | ✅ | ✅ | ✅ |
| torchvision | ✅ | ✅ | ✅ |
| torchaudio | ✅ | ✅ | ✅ |
| httpx | ✅ | ✅ | ✅ |
| soundfile | ✅ | ✅ | ✅ |
| numpy | ✅ | ✅ | ✅ |
| imageio-ffmpeg | ✅ | ✅ | ✅ |
| grpcio | ✅ | ✅ | ✅ |

### Installation Command (Mac)
```bash
# For Universal2 Python
python3 -m pip install \
    torch torchvision torchaudio \
    httpx soundfile numpy imageio-ffmpeg grpcio

# For Apple Silicon optimization
python3 -m pip install \
    --index-url https://download.pytorch.org/whl/cpu \
    torch torchvision torchaudio
```

---

## 6. PTSL Availability (Mac)

### Pro Tools | Script Library (PTSL)
- ✅ **Available on Mac** (same as Windows)
- Default port: **31416** (both platforms)
- Location: Enabled in Pro Tools preferences

### Testing PTSL on Mac
```bash
# Check if PTSL is listening
lsof -i :31416

# Test connection
python3 -c "import grpc; channel = grpc.insecure_channel('localhost:31416'); print('✓ PTSL reachable')"
```

---

## 7. AAX Plugin Bundle Structure

### Platform Differences (Already Handled!)
```
Windows:
  PTV2A.aaxplugin/
    Contents/
      x64/
        PTV2A.aaxplugin  # Binary (.dll)
      Resources/
        python/
          python.exe

Mac:
  PTV2A.aaxplugin/
    Contents/
      MacOS/
        PTV2A            # Binary (Mach-O)
      Resources/
        python/
          bin/python3
```

✅ **Code already handles this** via `getParentDirectory()` navigation.

---

## 8. Code Signing Differences

### Windows (PACE)
- Self-signed with PACE Eden
- Single `.aaxplugin` binary
- Signature embedded in binary

### Mac (PACE + Apple)
- **PACE signing:** Required (same as Windows)
- **Apple codesign:** Optional for development, **required for distribution**
- **Notarization:** Required for Gatekeeper (App Store distribution)

### Development Workflow (Mac)
```bash
# 1. PACE sign (required)
/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool sign \
    --account YOUR_ACCOUNT \
    --wcguid YOUR_WCGUID \
    --in PTV2A.aaxplugin \
    --out PTV2A.aaxplugin

# 2. Apple codesign (optional for dev, required for release)
codesign --force --deep --sign "Developer ID Application: Your Name" \
    PTV2A.aaxplugin

# 3. Notarization (required for Gatekeeper - release only)
xcrun notarytool submit PTV2A.aaxplugin.zip \
    --apple-id your@email.com \
    --password app-specific-password \
    --team-id TEAMID
```

**For thesis:** Only PACE signing needed (skip Apple codesign/notarization).

---

## 9. FFmpeg Binary Differences

### Current Implementation (Excellent!)
```python
# companion/video/ffmpeg.py
try:
    import imageio_ffmpeg
    ffmpeg_path = imageio_ffmpeg.get_ffmpeg_exe()  # ✅ Cross-platform
except ImportError:
    ffmpeg_path = shutil.which('ffmpeg')  # ✅ Fallback to system
```

### Platform Details
- **Windows:** `ffmpeg.exe` (PE binary)
- **Mac:** `ffmpeg` (Mach-O Universal binary)
- **imageio-ffmpeg:** Provides correct binary for platform automatically

✅ **No changes needed** - already optimal!

---

## 10. Testing Checklist

### Mac Build Testing
- [ ] Build on **Intel Mac** (x86_64)
- [ ] Build on **Apple Silicon Mac** (arm64)
- [ ] Verify Universal Binary with `file` command
- [ ] Test embedded Python execution
- [ ] Test FFmpeg video processing
- [ ] Test Pro Tools PTSL integration
- [ ] Verify PACE signature
- [ ] Test plugin loading in Pro Tools

### Cross-Platform Feature Testing
- [ ] Video import (MP4, MOV)
- [ ] V2A generation
- [ ] Sound recommendations (BBC API)
- [ ] Timeline selection
- [ ] Audio import to Pro Tools
- [ ] Credential management
- [ ] Settings persistence

---

## Summary

| Priority | Task | Status | Effort |
|----------|------|--------|--------|
| ✅ **Done** | Universal Binary CMake | Implemented | N/A |
| ✅ **Done** | Remove hardcoded paths | Implemented | N/A |
| ✅ **Done** | Mac Python paths | Implemented | N/A |
| ✅ **Done** | imageio-ffmpeg added | Implemented | N/A |
| ⚠️ **Optional** | StringArray commands | Not critical | 1-2 hours |
| ⚠️ **Optional** | Mac cache directory | Not critical | 30 mins |
| ⚠️ **Optional** | Path() instead of os.path | Cosmetic | 30 mins |
| 🎯 **Required** | Mac testing | **TODO** | 2-4 hours |
| 🎯 **Required** | PACE signing on Mac | **TODO** | 1 hour |

---

## Conclusion

The codebase is **well-prepared** for Mac porting. Critical changes are complete:
- ✅ Universal Binary support
- ✅ Platform-agnostic path handling
- ✅ Cross-platform dependencies
- ✅ Mac Python structure support

**Next step:** Build and test on actual Mac hardware!
