# HunyuanVideo-Foley API Client

Standalone CLI client for generating Foley audio from video using the HunyuanVideo-Foley API.

## Overview

This client is analogous to `standalone_api_client.py` (MMAudio) but specifically targets the HunyuanVideo-Foley API running on port 8001.

**Key Differences from MMAudio:**
- **Port**: 8001 (HunyuanVideo-Foley) vs 8000 (MMAudio)
- **Sample Rate**: 48kHz (professional Foley) vs 44.1kHz (CD quality)
- **Model Selection**: `xl`/`xxl` variants vs single `large_44k_v2` model
- **Use Case**: Foley sound effects vs general audio/music
- **VRAM**: 8-20GB (size-dependent) vs ~10GB (fixed)

## Quick Start

### Interactive Mode

```bash
python3 hunyuanvideo_foley_api_client.py
```

### CLI Mode (Pro Tools Integration)

```bash
# Basic usage
python3 hunyuanvideo_foley_api_client.py \
  --video /path/to/video.mp4 \
  --prompt "footsteps on gravel"

# Full parameters
python3 hunyuanvideo_foley_api_client.py \
  --video /path/to/video.mp4 \
  --prompt "rain and thunder" \
  --negative-prompt "voices, music" \
  --seed 42 \
  --model-size xxl \
  --steps 50 \
  --cfg-strength 4.5 \
  --output /path/to/output.wav
```

## Model Selection

**XL Model** (`--model-size xl`)
- Faster generation (~30-40% faster)
- Lower VRAM (8-12GB)
- Good quality for most use cases
- Best for: Real-time workflows, quick iteration

**XXL Model** (`--model-size xxl`, default)
- Higher quality audio
- Higher VRAM (16-20GB)
- Slower generation
- Best for: Final renders, complex sound design

## API Configuration

The client connects to the HunyuanVideo-Foley API by default:

```bash
# Default (local API)
--api-url http://localhost:8001

# Remote API
--api-url http://your-server:8001
```

## Output

Generated audio files are saved to `./hunyuanvideo-foley-outputs/` with naming pattern:
```
hyvf_{model_size}_{timestamp}_{seed}.{format}
```

Examples:
- `hyvf_xxl_1700000000_42.wav`
- `hyvf_xl_1700000001_123.flac`

## Pro Tools Integration

Same workflow as MMAudio client:

```bash
python3 hunyuanvideo_foley_api_client.py \
  --video /tmp/protools_clip.mov \
  --prompt "footsteps" \
  --import-to-protools \
  --video-offset "00:00:05:00"
```

## Comparison with MMAudio

| Feature | HunyuanVideo-Foley | MMAudio |
|---------|-------------------|---------|
| **Port** | 8001 | 8000 |
| **Sample Rate** | 48kHz | 44.1kHz |
| **Models** | xl, xxl | large_44k_v2 |
| **VRAM** | 8-20GB | ~10GB |
| **Steps** | 50 (default) | 25 (default) |
| **Use Case** | Foley/SFX | Music/General |
| **Output Dir** | `./hunyuanvideo-foley-outputs/` | `./standalone-API_outputs/` |

## Module Structure

```
companion/
├── hunyuanvideo_foley_api_client.py   # Main CLI client (this file)
├── api/
│   ├── hunyuanvideo_foley_client.py   # HunyuanVideo-Foley API functions
│   ├── client.py                      # MMAudio API functions
│   └── config.py                      # Shared configuration
├── video/                             # Shared video utilities
└── ptsl_integration/                  # Shared Pro Tools integration
```

## Architecture Notes

**Current Design** (Phase 1):
- Separate client scripts for each model (`standalone_api_client.py`, `hunyuanvideo_foley_api_client.py`)
- Shared utility modules (`video/`, `ptsl_integration/`)
- Model-specific API clients (`api/client.py`, `api/hunyuanvideo_foley_client.py`)

**Future Design** (Phase 2 - Optional):
- Unified `UnifiedAPIClient` in `companion/api/client.py`
- Model routing via `--model` parameter
- Legacy scripts remain as wrappers
- Better for A/B testing and batch processing

## Testing

```bash
# 1. Check API health
curl http://localhost:8001/

# 2. List available models
curl http://localhost:8001/models

# 3. Test XL model (faster)
python3 hunyuanvideo_foley_api_client.py \
  --video test.mp4 \
  --prompt "door slam" \
  --model-size xl

# 4. Test XXL model (higher quality)
python3 hunyuanvideo_foley_api_client.py \
  --video test.mp4 \
  --prompt "door slam" \
  --model-size xxl

# 5. Compare with MMAudio
python3 standalone_api_client.py \
  --video test.mp4 \
  --prompt "door slam"
```

## Dependencies

- `requests`: HTTP client for API communication
- `ffmpeg`: Video processing (trimming, validation)
- Shared modules: `api/`, `video/`, `ptsl_integration/`

## Troubleshooting

**API not reachable:**
```bash
# Check if API is running
docker ps | grep hunyuanvideo-foley-api

# Check API health
curl http://localhost:8001/

# Check API logs
docker exec hunyuanvideo-foley-api python main.py 2>&1 | tail -50
```

**Model loading slow:**
- First generation loads model into VRAM (~30-60s)
- Subsequent generations are fast (model cached)
- Check VRAM: `nvidia-smi`

**Generation timeout:**
- Increase `--timeout` (default: 300s)
- Try `xl` model (faster)
- Check GPU usage with `nvidia-smi`

## See Also

- **MMAudio Client**: `standalone_api_client.py` - Music/general audio generation
- **API Documentation**: `thesis-pt-v2a/hunyuanvideo-foley-API/main.py` - Full API reference
- **Docker Setup**: `docker-compose.yml` - Service configuration
