# Migration zu py-ptsl

## Warum migrieren?

### Probleme mit aktueller Implementierung:
1. **Pro Tools Crash**: Plugin ruft System-Python auf → fehlt venv-Dependencies → Crash
2. **Wartbarkeit**: Custom PTSL-Code schwer zu warten
3. **Features**: Fehlende PTSL-Commands müssten manuell implementiert werden

### Vorteile von py-ptsl:
1. **Stabilität**: Professionelle, getestete Library
2. **API**: Eleganter als JSON-in-Protobuf
3. **Type Safety**: `.pyi` Dateien für IDE-Support
4. **Features**: Alle PTSL-Commands bereits implementiert
5. **Community**: Aktiv maintained, Issues werden gefixt

## Migration-Schritte

### 1. Environment Setup

```powershell
cd companion
.\setup_v2.ps1
```

Das installiert:
- py-ptsl aus `external/py-ptsl`
- grpcio, protobuf, soundfile
- Alles in `venv_ptsl` Environment

### 2. Code-Anpassungen

#### Alt (ptsl_client.py):
```python
from ptsl_integration.ptsl_client import import_audio_to_pro_tools

success = import_audio_to_pro_tools("audio.flac")
```

#### Neu (ptsl_client_v2.py):
```python
from ptsl_integration.ptsl_client_v2 import import_audio_to_pro_tools

success = import_audio_to_pro_tools("audio.flac")  # Gleiche API!
```

**Keine Änderungen im Plugin nötig!** Die API ist kompatibel.

### 3. Plugin-Anpassung (wichtig!)

In `PluginProcessor.cpp`, `getPythonExecutable()`:

```cpp
juce::StringArray pythonCandidates = {
    // PRIORITÄT: venv Python!
    "C:\\Users\\Ludenbold\\Desktop\\Master_Thesis\\Implementation\\thesis-pt-v2a\\companion\\venv_ptsl\\Scripts\\python.exe",
    "python",
    "python3"
};
```

**Oder dynamisch**:
```cpp
// Get path relative to thesis root
auto venvPython = thesisRoot.getChildFile("companion")
                           .getChildFile("venv_ptsl")
                           .getChildFile("Scripts")
                           .getChildFile("python.exe");

if (venvPython.existsAsFile()) {
    return venvPython.getFullPathName();
}
```

### 4. Test Migration

```powershell
# Terminal 1: Start MMAudio API
cd ..\standalone-API
python main.py

# Terminal 2: Test v2 client
cd companion
.\venv_ptsl\Scripts\Activate.ps1
python standalone_api_client.py --video test.mp4 --import-to-protools
```

### 5. Plugin Recompile

```powershell
cd ..\build
cmake --build . --config Debug
```

## API-Vergleich

### Connection

#### Alt:
```python
client = PTSLClient()
if client.connect():
    # ... operations
    client.disconnect()
```

#### Neu (py-ptsl):
```python
with open_engine(company_name="MyCompany", application_name="MyApp") as engine:
    # ... operations
    # Automatic disconnect!
```

### Audio Import

#### Alt:
```python
result = client.import_audio_to_timeline("audio.wav")
if result:
    print("Success!")
```

#### Neu (py-ptsl):
```python
engine.import_audio(
    file_list=["audio.wav"],
    audio_destination=MediaDestination.MD_NewTrack,
    audio_location=MediaLocation.ML_SessionStart
)
# Exceptions raised on error
```

### Session Info

#### Alt:
```python
name = client.get_session_name()
```

#### Neu (py-ptsl):
```python
name = engine.session_name()  # Eleganter!
```

## Was bleibt erhalten?

Unsere **gesamte Dokumentation** ist weiterhin wertvoll:
- **FLAC→WAV Problematik**: Dokumentiert und gelöst
- **PTSL Konzepte**: Session ID, Version Matching, etc.
- **Error Handling**: PT_InvalidParameter, SDK_VersionMismatch, etc.
- **Path Normalization**: Forward slashes, Long Path Names

Diese Erkenntnisse helfen beim **Verständnis** von py-ptsl!

## Nächste Schritte

1. ✅ `setup_v2.ps1` ausführen
2. ✅ `ptsl_client_v2.py` testen
3. ⏳ Plugin-Code anpassen (venv Python verwenden)
4. ⏳ Plugin neu kompilieren
5. ⏳ End-to-End Test

## Rollback-Plan

Falls Probleme auftreten:
- Alte `ptsl_client.py` bleibt unverändert
- Plugin kann weiter alten Code verwenden
- Einfach nicht `--import-to-protools` flag verwenden

## Fragen?

- **py-ptsl Docs**: `external/py-ptsl/docs/`
- **Examples**: `external/py-ptsl/examples/`
- **Our custom docs**: Alle in `ptsl_client.py` (v1)
