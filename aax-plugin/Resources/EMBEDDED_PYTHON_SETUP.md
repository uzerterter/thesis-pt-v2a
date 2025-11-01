# Embedded Python Setup for AAX Plugin

The plugin uses **embedded Python 3.12** with all dependencies bundled for a self-contained distribution.

## Why Embedded Python?

- ✅ **Self-contained**: No system Python required
- ✅ **Stable**: No conflicts with other Python installations
- ✅ **Portable**: Plugin works on any system
- ✅ **Professional**: How commercial plugins handle scripting

## Setup Instructions

### 1. Download Python Embedded Distribution

Download **python-3.12.x-embed-amd64.zip** (Windows) from:
https://www.python.org/downloads/windows/

**Direct link (Python 3.12.0):**
https://www.python.org/ftp/python/3.12.0/python-3.12.0-embed-amd64.zip

Extract to: `aax-plugin/Resources/python/`

### 2. Enable `site-packages`

Edit `python/python312._pth` and **uncomment** this line:
```
import site
```

### 3. Install pip

Download `get-pip.py`:
```bash
cd aax-plugin/Resources/python
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python.exe get-pip.py
```

### 4. Install Dependencies

```bash
cd aax-plugin/Resources/python

# Install py-ptsl (editable for development)
python.exe -m pip install -e ..\..\..\external\py-ptsl

# Install other dependencies
python.exe -m pip install grpcio protobuf soundfile

# Optional: Install poetry (if py-ptsl requires it)
python.exe -m pip install poetry
```

### 5. Setup Symlinks (Development Only)

For development, use symlinks to avoid copying files:

**Windows (requires Admin or Developer Mode):**
```cmd
cd aax-plugin\Resources\python\Lib\site-packages
mklink /D ptsl_integration C:\Users\<YOU>\Desktop\Master_Thesis\Implementation\thesis-pt-v2a\companion\ptsl_integration

cd ..\..\Scripts
mklink standalone_api_client.py ..\..\..\..\companion\standalone_api_client.py
```

**Alternative: Let CMake copy files** (automatic on build):
CMake will automatically copy files from `companion/` to `Resources/` on each build.

### 6. Verify Setup

Test that embedded Python can import all modules:

```bash
cd aax-plugin/Resources/python

# Test py-ptsl
python.exe -c "import ptsl; print('✅ py-ptsl works')"

# Test ptsl_integration
python.exe -c "from ptsl_integration import import_audio_to_pro_tools; print('✅ ptsl_integration works')"

# Test PTSL connection (Pro Tools must be running!)
python.exe -c "from ptsl_integration import import_audio_to_pro_tools; import_audio_to_pro_tools('test.wav')"
```

## Directory Structure

```
aax-plugin/Resources/python/
├── python.exe                    # Python 3.12 embedded
├── python312.dll
├── python312._pth                # Modified: import site enabled
├── get-pip.py
├── Lib/
│   └── site-packages/
│       ├── py-ptsl/              # Installed via pip install -e
│       ├── ptsl_integration/     # Symlink → companion/ (dev)
│       ├── grpcio/
│       ├── protobuf/
│       └── soundfile/
└── Scripts/
    └── standalone_api_client.py  # Symlink → companion/ (dev)
```

## Production Deployment

For plugin distribution:

1. **Remove symlinks** and replace with real copies:
   ```bash
   cd aax-plugin/Resources/python
   cmake --build ../../build --target pt_v2a_AAX
   # CMake will copy files automatically
   ```

2. **Bundle with plugin**: JUCE copies `Resources/` folder into `.aaxplugin` bundle

3. **Result**: Fully self-contained plugin with Python runtime

## Troubleshooting

### "ModuleNotFoundError: No module named 'ptsl'"
- py-ptsl not installed
- Run: `python.exe -m pip install -e ..\..\..\external\py-ptsl`

### "ModuleNotFoundError: No module named 'ptsl_integration'"
- Symlink broken or CMake didn't copy files
- Check: `Lib/site-packages/ptsl_integration/` exists
- Recreate symlink or rebuild with CMake

### "Python executable not found"
- Embedded Python not extracted to correct location
- Should be at: `aax-plugin/Resources/python/python.exe`

### Pro Tools crashes when using plugin
- Check that subprocess is **non-blocking** (no `waitForProcessToFinish`)
- Embedded Python must be in `Resources/python/`
- Check Pro Tools console for debug output

## File Sizes

- **python-3.12.0-embed-amd64.zip**: ~11 MB
- **After pip install dependencies**: ~150 MB
- **Git repository**: Excluded (use .gitignore)

## Links

- Python Embedded Distributions: https://www.python.org/downloads/windows/
- py-ptsl: https://github.com/iluvcapra/py-ptsl
- pip documentation: https://pip.pypa.io/
