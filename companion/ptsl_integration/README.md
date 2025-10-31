# PTSL Integration Module

Pro Tools Scripting Library (PTSL) integration for automatic audio import to Pro Tools timeline.

## 📁 Structure

```
ptsl_integration/
├── __init__.py           # Package initialization
├── ptsl_client.py        # PTSL gRPC client implementation
├── PTSL_pb2.py          # Generated protobuf message definitions
├── PTSL_pb2_grpc.py     # Generated gRPC service stubs
├── requirements.txt      # Python dependencies
├── setup.ps1            # Setup script
└── README.md            # This file
```

## 🚀 Quick Start

### 1. Setup (First Time Only)

```powershell
cd thesis-pt-v2a/companion/ptsl_integration
..\venv_ptsl\Scripts\Activate.ps1
.\setup.ps1
```

This will:
- Install required packages (grpcio, grpcio-tools, protobuf)
- Generate Python code from PTSL.proto
- Test connection to Pro Tools

### 2. Enable PTSL in Pro Tools

1. Open Pro Tools
2. Go to **Setup > Preferences > MIDI**
3. Enable **"Enable PTSL (Pro Tools Scripting Library)"**
4. Restart Pro Tools

### 3. Test Connection

```python
from ptsl_integration import PTSLClient

client = PTSLClient()
if client.connect():
    print("✅ Connected to Pro Tools!")
    client.disconnect()
```

### 4. Import Audio to Timeline

```python
from ptsl_integration import import_audio_to_pro_tools

success = import_audio_to_pro_tools(
    audio_path="C:/path/to/audio.flac",
    location="SessionStart"
)
```

## 📚 API Reference

### `PTSLClient`

Main class for PTSL communication.

#### Methods:

**`connect(company_name, app_name) -> bool`**
- Establish connection to Pro Tools PTSL server
- Returns `True` if successful

**`import_audio_to_timeline(audio_file_path, location, destination) -> Optional[str]`**
- Import audio file to Pro Tools timeline
- **Parameters:**
  - `audio_file_path`: Absolute path to audio file
  - `location`: Timeline position (`"SessionStart"`, `"SongStart"`, `"Selection"`, `"Spot"`)
  - `destination`: Import target (`"NewTrack"`, `"ClipList"`)
- Returns success message or `None` if failed

**`disconnect()`**
- Close PTSL connection

### `import_audio_to_pro_tools(audio_path, location, host, port) -> bool`

Convenience function for one-shot audio import.

## 🔧 Integration with standalone_api_client.py

Add PTSL import to the audio generation workflow:

```python
# In standalone_api_client.py, after audio generation:
if args.import_to_protools:
    sys.path.insert(0, str(Path(__file__).parent / "ptsl_integration"))
    from ptsl_integration import import_audio_to_pro_tools
    
    success = import_audio_to_pro_tools(output_file)
    if success:
        print("✅ Audio imported to Pro Tools!")
```

## ⚠️ Requirements

### System Requirements:
- **Pro Tools**: 2024.6.0 or later (with PTSL support)
- **Python**: 3.8 or later
- **Windows**: 10/11

### Pro Tools Session:
- Must have an **active Pro Tools session open**
- PTSL must be enabled in preferences
- Port 31416 must not be blocked by firewall

## 🐛 Troubleshooting

### "ModuleNotFoundError: No module named 'PTSL_pb2'"
- Run `setup.ps1` to generate protobuf files
- Make sure you're in the correct directory

### "PTSL: Connection failed - Connection refused"
- Check if Pro Tools is running
- Verify PTSL is enabled: **Setup > Preferences > MIDI**
- Check firewall settings for port 31416

### "PTSL: Import failed with status: StatusType_Failure"
- Ensure a Pro Tools session is open
- Verify audio file path is absolute and file exists
- Check audio file format is supported (.wav, .flac, .mp3, etc.)

### "Task timeout after 60 seconds"
- Large audio files may take longer to import
- Check Pro Tools is not frozen or busy
- Verify disk space is available for audio file copy

## 📖 References

- [PTSL Documentation](../../../Avid/PTSL_SDK_CPP.2025.06.0.1178210/DOCUMENTATION.html)
- [PTSL.proto](../../../Avid/PTSL_SDK_CPP.2025.06.0.1178210/Source/PTSL.proto)
- [gRPC Python Guide](https://grpc.io/docs/languages/python/quickstart/)

## 🎯 Next Steps

1. ✅ Basic PTSL connection
2. ✅ Audio import to timeline
3. 🚧 Video clip selection from timeline
4. 🚧 Smart placement (import audio at video location)
5. 🚧 Error handling and retry logic
