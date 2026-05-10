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
#   3. Downloads HunyuanVideo-Foley model weights from HuggingFace
#   4. Builds the Docker image
#   5. Creates the 'mmaudio' and 'hyvf' conda environments inside the container
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
# STEP 3: Download model weights
# ==============================================================================
echo ""
echo "── Step 3/5: Downloading model weights"

# HunyuanVideo-Foley weights (~20 GB from HuggingFace)
HYVF_WEIGHTS_DIR="$PARENT_DIR/model-tests/models/HunyuanVideo-Foley"
if [ -d "$HYVF_WEIGHTS_DIR" ] && [ -n "$(ls -A "$HYVF_WEIGHTS_DIR" 2>/dev/null)" ]; then
    echo "   ✓ HunyuanVideo-Foley weights already present — skipping"
else
    echo "   Downloading HunyuanVideo-Foley weights (~20 GB)..."
    if command -v huggingface-cli &>/dev/null; then
        huggingface-cli download tencent/HunyuanVideo-Foley \
            --local-dir "$HYVF_WEIGHTS_DIR"
        echo "   ✓ Weights downloaded to $HYVF_WEIGHTS_DIR"
    else
        echo ""
        echo "   ⚠️  huggingface-cli not found."
        echo "   Install it with:  pip install huggingface-hub"
        echo "   Then run manually:"
        echo ""
        echo "     huggingface-cli download tencent/HunyuanVideo-Foley \\"
        echo "         --local-dir $HYVF_WEIGHTS_DIR"
        echo ""
        echo "   Continuing setup — you can download weights later."
    fi
fi

# MMAudio weights: downloaded automatically on first API call (~4 GB)
echo "   ℹ  MMAudio weights auto-download on first API call (~4 GB)"

# ==============================================================================
# STEP 4: Build Docker image
# ==============================================================================
echo ""
echo "── Step 4/5: Building Docker image (generation-api)"
cd "$SCRIPT_DIR"
docker compose -f docker-compose.generation.yml build
echo "   ✓ Docker image built"

# ==============================================================================
# STEP 5: Create conda environments inside the container
# ==============================================================================
echo ""
echo "── Step 5/5: Creating conda environments (this may take a few minutes)"

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
