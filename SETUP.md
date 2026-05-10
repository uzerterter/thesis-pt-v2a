# Setup Guide — Generation Backends

This guide explains how to set up the two AI audio generation APIs on a new machine:

| API | Model | Port |
|-----|-------|------|
| MMAudio | [MMAudio](https://github.com/hkchengrex/MMAudio) | 8000 |
| HunyuanVideo-Foley | [HunyuanVideo-Foley](https://github.com/Tencent-Hunyuan/HunyuanVideo-Foley) | 8001 |

Both APIs accept a video file (and optional text prompt) and return generated audio.

---

## Prerequisites

Make sure the following are installed on the host machine before running setup:

| Requirement | Install guide |
|-------------|--------------|
| **NVIDIA GPU** (≥16 GB VRAM recommended) | — |
| **NVIDIA drivers** (≥535) | https://www.nvidia.com/drivers |
| **Docker** + **Docker Compose** (v2+) | https://docs.docker.com/engine/install |
| **NVIDIA Container Toolkit** | https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html |
| **git** | `apt install git` |
| **huggingface-cli** (for weight downloads) | `pip install huggingface-hub` |

Verify Docker can see your GPU:
```bash
docker run --rm --gpus all nvidia/cuda:12.4.1-base-ubuntu22.04 nvidia-smi
```

---

## Directory Structure

The setup script will create the following layout relative to where `thesis-pt-v2a/` lives:

```
<parent>/
├── thesis-pt-v2a/               ← this repository
│   ├── Dockerfile
│   ├── docker-compose.generation.yml
│   ├── setup.sh
│   ├── standalone-API/          ← MMAudio FastAPI server
│   ├── hunyuanvideo-foley-API/  ← HunyuanVideo-Foley FastAPI server
│   └── shared/                  ← shared config
│
├── model-tests/
│   ├── repos/
│   │   ├── MMAudio/             ← cloned by setup.sh
│   │   └── HunyuanVideo-Foley/  ← cloned by setup.sh
│   └── models/
│       └── HunyuanVideo-Foley/  ← weights downloaded by setup.sh (~20 GB)
│
└── conda-envs/
    ├── mmaudio/                 ← Python env created by setup.sh
    └── hyvf/                    ← Python env created by setup.sh
```

Inside the Docker containers, `<parent>/` is mounted as `/workspace`.

---

## Quick Start

Clone the repository and run the setup script:

```bash
# 1. Clone this repo into any directory
git clone <repo-url> thesis-pt-v2a
cd thesis-pt-v2a

# 2. Run the automated setup (takes 20–40 min, mostly downloads)
bash setup.sh

# 3. Start the APIs
docker compose -f docker-compose.generation.yml up -d
```

That's it. The APIs will start automatically.

---

## What setup.sh Does

| Step | What happens |
|------|-------------|
| 1 | Creates `../model-tests/repos/`, `../model-tests/models/`, `../conda-envs/` |
| 2 | Clones MMAudio and HunyuanVideo-Foley repos |
| 3 | Downloads HunyuanVideo-Foley weights (~20 GB via `huggingface-cli`) |
| 4 | Builds the Docker image (`generation-api:latest`) |
| 5 | Creates `mmaudio` conda env (Python 3.11 + PyTorch + MMAudio) |
| 5 | Creates `hyvf` conda env (Python 3.10 + PyTorch + HunyuanVideo-Foley) |

MMAudio weights (~4 GB) are downloaded automatically on the first API call.

---

## Verifying the Setup

After `docker compose ... up -d`, check both APIs are running:

```bash
curl http://localhost:8000/health   # MMAudio
curl http://localhost:8001/health   # HunyuanVideo-Foley
```

Expected response:
```json
{"status": "ok", "service": "MMAudio Standalone API", "device": "cuda", "version": "1.0.0"}
```

---

## Useful Commands

```bash
# Start APIs
docker compose -f docker-compose.generation.yml up -d

# Stop APIs
docker compose -f docker-compose.generation.yml down

# View live logs (both services)
docker compose -f docker-compose.generation.yml logs -f

# View logs for one service
docker compose -f docker-compose.generation.yml logs -f mmaudio-api

# Open a shell inside a running container (for debugging)
docker exec -it mmaudio-api /bin/bash

# Rebuild the Docker image after Dockerfile changes
docker compose -f docker-compose.generation.yml build
```

---

## Configuration

All settings can be overridden via environment variables. Edit `docker-compose.generation.yml`
and uncomment/add the relevant `environment:` entries:

| Variable | Default | Description |
|----------|---------|-------------|
| `MMAUDIO_PATH` | `/workspace/model-tests/repos/MMAudio` | Path to MMAudio repo inside container |
| `HYVF_PATH` | `/workspace/model-tests/repos/HunyuanVideo-Foley` | Path to HYVF repo inside container |
| `HYVF_WEIGHTS_PATH` | `/workspace/model-tests/models/HunyuanVideo-Foley` | Path to HYVF weights inside container |
| `FORCE_DEVICE` | `auto` | Force `cuda`, `cpu`, or `mps` |
| `FORCE_DTYPE` | `bfloat16` | Precision: `float32`, `bfloat16`, `float16` |
| `VIDEO_CACHE_MAX_GB` | `32` | Max RAM for video cache |
| `API_PORT` | `8000` / `8001` | Port the server listens on |

Full list: see [`CONFIGURATION.md`](./CONFIGURATION.md) and [`shared/config.py`](./shared/config.py).

---

## Troubleshooting

**"could not select device driver nvidia"**  
→ NVIDIA Container Toolkit is not installed or not configured for Docker.  
→ Follow: https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html

**"conda activate mmaudio: command not found"**  
→ The conda environment wasn't created. Re-run `bash setup.sh` — it will skip steps already done and retry the env creation.

**"No module named 'mmaudio'"**  
→ The MMAudio repo wasn't installed into the conda env. Inside the container:
```bash
conda run -n mmaudio pip install -e /workspace/model-tests/repos/MMAudio
```

**"No module named 'hunyuanvideo_foley'"**  
→ Same issue for HYVF:
```bash
conda run -n hyvf pip install -e /workspace/model-tests/repos/HunyuanVideo-Foley
```

**API returns 500 on first request (MMAudio)**  
→ MMAudio downloads its weights on the first call. This can take a few minutes. Check logs:
```bash
docker compose -f docker-compose.generation.yml logs -f mmaudio-api
```

**HunyuanVideo-Foley weights not found**  
→ Download them manually:
```bash
huggingface-cli download tencent/HunyuanVideo-Foley \
    --local-dir ../model-tests/models/HunyuanVideo-Foley
```

---

## Notes

- The conda environments (`mmaudio`, `hyvf`) are stored on the **host** in `../conda-envs/` and mounted into the container. This means they persist across Docker image rebuilds — you only need to create them once.
- If you update the `requirements.txt` files, re-run the pip install steps inside the container (no need to recreate the whole env).
- The APIs share the same Docker image (`generation-api:latest`) but run in separate containers with separate conda environments.
