#!/bin/bash
# ==============================================================================
# setup.sh — Bootstrap script for thesis-pt-v2a generation backends
#
# Run this script from inside the thesis-pt-v2a/ directory:
#   bash setup.sh
#
# What this does:
#   1. Creates the required directory structure in the parent directory
#   2. Clones MMAudio and HunyuanVideo-Foley model repositories
#   3. Builds the Docker image
#   4. Creates the 'mmaudio' and 'hyvf' conda environments inside the container
#   5. Downloads HunyuanVideo-Foley weights inside the container (~20 GB)
#
# After this completes, start the APIs with:
#   docker compose -f docker-compose.generation.yml up -d
# ==============================================================================

set -e  # Exit on any error

# Resolve paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        thesis-pt-v2a — Generation Backend Setup             ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "  Repo:        $SCRIPT_DIR"
echo "  Parent dir:  $PARENT_DIR"
echo ""

# ==============================================================================
# STEP 1: Create directory structure
# ==============================================================================
echo "── Step 1/5: Creating directory structure"
mkdir -p "$PARENT_DIR/model-tests/repos"
mkdir -p "$PARENT_DIR/model-tests/models"
mkdir -p "$PARENT_DIR/conda-envs"
echo "   ✓ Directories ready"

# ==============================================================================
# STEP 2: Clone model repositories
# ==============================================================================
echo ""
echo "── Step 2/5: Cloning model repositories"

# MMAudio
if [ -d "$PARENT_DIR/model-tests/repos/MMAudio/.git" ]; then
    echo "   ✓ MMAudio already cloned — skipping"
else
    echo "   Cloning MMAudio..."
    git clone https://github.com/hkchengrex/MMAudio.git \
        "$PARENT_DIR/model-tests/repos/MMAudio"
    echo "   ✓ MMAudio cloned"
fi

# HunyuanVideo-Foley
if [ -d "$PARENT_DIR/model-tests/repos/HunyuanVideo-Foley/.git" ]; then
    echo "   ✓ HunyuanVideo-Foley already cloned — skipping"
else
    echo "   Cloning HunyuanVideo-Foley..."
    git clone https://github.com/Tencent-Hunyuan/HunyuanVideo-Foley.git \
        "$PARENT_DIR/model-tests/repos/HunyuanVideo-Foley"
    echo "   ✓ HunyuanVideo-Foley cloned"
fi

# ==============================================================================
# STEP 3: Build Docker image
# ==============================================================================
echo ""
echo "── Step 3/5: Building Docker image (generation-api)"
cd "$SCRIPT_DIR"
docker compose -f docker-compose.generation.yml build
echo "   ✓ Docker image built"

# ==============================================================================
# STEP 4: Create conda environments inside the container
# ==============================================================================
echo ""
echo "── Step 4/5: Creating conda environments (this may take a few minutes)"

# --- mmaudio environment ---
if [ -d "$PARENT_DIR/conda-envs/mmaudio" ]; then
    echo "   ✓ 'mmaudio' conda env already exists — skipping"
else
    echo "   Creating 'mmaudio' environment (Python 3.11 + PyTorch cu124)..."
    docker compose -f docker-compose.generation.yml run --rm mmaudio-api /bin/bash -c "
        set -e
        source /opt/miniforge/etc/profile.d/conda.sh
        mamba create -n mmaudio python=3.11 -y
        conda run -n mmaudio pip install \
            torch torchvision torchaudio \
            --index-url https://download.pytorch.org/whl/cu124
        conda run -n mmaudio pip install -e /workspace/model-tests/repos/MMAudio
        conda run -n mmaudio pip install \
            -r /workspace/thesis-pt-v2a/standalone-API/requirements.txt
        echo 'mmaudio environment OK'
    "
    echo "   ✓ 'mmaudio' environment created"
fi

# --- hyvf environment ---
if [ -d "$PARENT_DIR/conda-envs/hyvf" ]; then
    echo "   ✓ 'hyvf' conda env already exists — skipping"
else
    echo "   Creating 'hyvf' environment (Python 3.10 + PyTorch cu124)..."
    docker compose -f docker-compose.generation.yml run --rm hunyuanvideo-foley-api /bin/bash -c "
        set -e
        source /opt/miniforge/etc/profile.d/conda.sh
        mamba create -n hyvf python=3.10 -y
        conda run -n hyvf pip install \
            torch torchvision torchaudio \
            --index-url https://download.pytorch.org/whl/cu124
        conda run -n hyvf pip install -e /workspace/model-tests/repos/HunyuanVideo-Foley
        conda run -n hyvf pip install \
            -r /workspace/thesis-pt-v2a/hunyuanvideo-foley-API/requirements.txt
        echo 'hyvf environment OK'
    "
    echo "   ✓ 'hyvf' environment created"
fi

# ==============================================================================
# STEP 5: Download HunyuanVideo-Foley weights inside the container
#
# Weights are downloaded using huggingface-cli from the hyvf conda environment
# (no host-side huggingface-cli required — pip is used inside the container).
# Download destination: ../model-tests/models/HunyuanVideo-Foley/ (host disk)
# ==============================================================================
echo ""
echo "── Step 5/5: Checking HunyuanVideo-Foley model weights"

HYVF_WEIGHTS_DIR="$PARENT_DIR/model-tests/models/HunyuanVideo-Foley"
if [ -d "$HYVF_WEIGHTS_DIR" ] && [ -n "$(ls -A "$HYVF_WEIGHTS_DIR" 2>/dev/null)" ]; then
    echo "   ✓ HunyuanVideo-Foley weights already present — skipping"
else
    echo "   Downloading HunyuanVideo-Foley weights (~20 GB)..."
    echo "   (This may take a while depending on your connection)"
    docker compose -f docker-compose.generation.yml run --rm hunyuanvideo-foley-api /bin/bash -c "
        set -e
        source /opt/miniforge/etc/profile.d/conda.sh
        conda run -n hyvf pip install -q huggingface_hub
        conda run -n hyvf huggingface-cli download tencent/HunyuanVideo-Foley \
            --local-dir /workspace/model-tests/models/HunyuanVideo-Foley
        echo 'weights download OK'
    "
    echo "   ✓ Weights downloaded to $HYVF_WEIGHTS_DIR"
fi

echo "   ℹ  MMAudio weights auto-download on first API call (~4 GB)"

# ==============================================================================
# Cleanup: Remove any leftover named containers from the env creation steps.
#
# 'docker compose run' can leave named containers behind when container_name is
# set in the compose file. These must be removed before 'docker compose up' can
# create the actual service containers.
#
# NOTE: The conda environments (mmaudio, hyvf) are stored in ../conda-envs/ on
# the HOST filesystem via the volume mount — removing containers does NOT delete
# them. They persist across container removals and Docker image rebuilds.
# ==============================================================================
echo ""
echo "── Cleanup: Removing setup containers (envs are safe on host disk)"
docker rm -f gen-mmaudio-api gen-hyvf-api 2>/dev/null || true
echo "   ✓ Done"

# ==============================================================================
# Done
# ==============================================================================
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    ✅ Setup complete!                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "  Start the APIs:"
echo "    docker compose -f docker-compose.generation.yml up -d"
echo ""
echo "  Check health:"
echo "    curl http://localhost:8000/health   # MMAudio"
echo "    curl http://localhost:8001/health   # HunyuanVideo-Foley"
echo ""
echo "  View logs:"
echo "    docker compose -f docker-compose.generation.yml logs -f"
echo ""
