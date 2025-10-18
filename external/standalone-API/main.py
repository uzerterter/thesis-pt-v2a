"""
MMAudio Standalone API

A high-performance FastAPI server for video-to-audio generation using MMAudio.
Optimized for speed with intelligent caching and minimal overhead.

Features:
- Direct MMAudio integration (no ComfyUI overhead)
- Optimized video loading with frame caching
- Model caching for faster repeated inference
- RESTful API for easy integration
- Built for AAX plugin integration
"""

import os
import sys
import logging
from pathlib import Path
from typing import Optional
import hashlib
import pickle
import tempfile
import time
from datetime import datetime

import torch
import numpy as np
import av
from fastapi import FastAPI, UploadFile, File, Form, HTTPException
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn

# Add MMAudio to path - adjust this path as needed
MMAUDIO_PATH = Path("/workspace/model-tests/repos/MMAudio")
sys.path.insert(0, str(MMAUDIO_PATH))

try:
    from mmaudio.eval_utils import ModelConfig, all_model_cfg, generate, load_video, setup_eval_logging
    from mmaudio.model.flow_matching import FlowMatching  
    from mmaudio.model.networks import MMAudio, get_my_mmaudio
    from mmaudio.model.utils.features_utils import FeaturesUtils
    import torchaudio
except ImportError as e:
    logging.error(f"Failed to import MMAudio: {e}")
    logging.error("Please check MMAUDIO_PATH in the script")
    sys.exit(1)

# Configure logging
setup_eval_logging()
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Global configuration
torch.backends.cuda.matmul.allow_tf32 = True
torch.backends.cudnn.allow_tf32 = True

device = 'cpu'
if torch.cuda.is_available():
    device = 'cuda'
elif torch.backends.mps.is_available():
    device = 'mps'
else:
    logger.warning('CUDA/MPS not available, running on CPU')

dtype = torch.bfloat16

# Global model cache
MODEL_CACHE = {}
VIDEO_CACHE = {}  # RAM-based video cache
CACHE_DIR = Path("./cache")
CACHE_DIR.mkdir(exist_ok=True)

app = FastAPI(
    title="MMAudio Standalone API",
    description="High-performance video-to-audio generation API",
    version="1.0.0"
)

# Enable CORS for web requests
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

def get_cached_model(model_name: str = 'large_44k_v2') -> tuple[MMAudio, FeaturesUtils, ModelConfig]:
    """Load and cache MMAudio model - using exact demo.py logic"""
    if model_name in MODEL_CACHE:
        logger.info(f"Using cached model: {model_name}")
        return MODEL_CACHE[model_name]
    
    logger.info(f"Loading model: {model_name}")
    start_time = time.time()
    
    # Exact logic from demo.py
    if model_name not in all_model_cfg:
        raise ValueError(f'Unknown model variant: {model_name}')
    
    model: ModelConfig = all_model_cfg[model_name]
    model.download_if_needed()
    seq_cfg = model.seq_cfg

    # load a pretrained model (from demo.py)
    net: MMAudio = get_my_mmaudio(model.model_name).to(device, dtype).eval()
    net.load_weights(torch.load(model.model_path, map_location=device, weights_only=True))
    
    feature_utils = FeaturesUtils(tod_vae_ckpt=model.vae_path,
                                  synchformer_ckpt=model.synchformer_ckpt,
                                  enable_conditions=True,
                                  mode=model.mode,
                                  bigvgan_vocoder_ckpt=model.bigvgan_16k_path,
                                  need_vae_encoder=False)
    feature_utils = feature_utils.to(device, dtype).eval()
    
    load_time = time.time() - start_time
    logger.info(f"Model loaded in {load_time:.2f}s")
    
    # Cache the complete setup
    MODEL_CACHE[model_name] = (net, feature_utils, seq_cfg)
    return net, feature_utils, seq_cfg

def load_video_optimized(video_path: Path, duration_sec: float):
    """Use MMAudio's native load_video function with RAM caching"""
    cache_key = hashlib.md5(f"{video_path}_{duration_sec}_{video_path.stat().st_mtime}".encode()).hexdigest()
    
    # Check RAM cache first
    if cache_key in VIDEO_CACHE:
        logger.info(f"Loading video from RAM cache: {cache_key[:8]}...")
        return VIDEO_CACHE[cache_key]
    
    logger.info(f"Loading video: {video_path.name}")
    start_time = time.time()
    
    # Use MMAudio's native load_video function (from demo.py)
    video_info = load_video(video_path, duration_sec)
    
    load_time = time.time() - start_time
    logger.info(f"Video loaded in {load_time:.2f}s")
    
    # Cache in RAM for future use
    VIDEO_CACHE[cache_key] = video_info
    
    return video_info

@app.get("/")
async def root():
    """API health check"""
    return {
        "message": "MMAudio Standalone API",
        "status": "running",
        "device": device,
        "version": "1.0.0"
    }

@app.get("/models")
async def list_models():
    """List available MMAudio models"""
    return {
        "available_models": list(all_model_cfg.keys()),
        "loaded_models": list(MODEL_CACHE.keys())
    }

@app.post("/generate")
async def generate_audio(
    video: UploadFile = File(...),
    prompt: str = Form(""),
    negative_prompt: str = Form(""),
    seed: int = Form(42),
    duration: Optional[float] = Form(None),
    model_name: str = Form("large_44k_v2"),
    num_steps: int = Form(25),
    cfg_strength: float = Form(4.5)
):
    """Generate audio from video"""
    try:
        # Save uploaded video to temporary file
        with tempfile.NamedTemporaryFile(delete=False, suffix='.mp4') as tmp_file:
            content = await video.read()
            tmp_file.write(content)
            tmp_video_path = Path(tmp_file.name)
        
        # Auto-detect duration if not provided
        if duration is None:
            with av.open(tmp_video_path) as container:
                stream = container.streams.video[0]
                duration = float(stream.duration * stream.time_base)
            logger.info(f"Auto-detected duration: {duration:.2f}s")
        
        # Load model (cached)
        net, feature_utils, seq_cfg = get_cached_model(model_name)
        
        # Load and process video using MMAudio's native function
        video_info = load_video_optimized(tmp_video_path, duration)
        
        # Exact logic from demo.py for video processing
        clip_frames = video_info.clip_frames
        sync_frames = video_info.sync_frames
        duration = video_info.duration_sec
        
        # Apply demo.py logic: unsqueeze for batch dimension
        clip_frames = clip_frames.unsqueeze(0)
        sync_frames = sync_frames.unsqueeze(0)
        
        # Update sequence configuration (from demo.py)
        seq_cfg.duration = duration
        net.update_seq_lengths(seq_cfg.latent_seq_len, seq_cfg.clip_seq_len, seq_cfg.sync_seq_len)
        
        # Setup generation exactly like demo.py
        rng = torch.Generator(device=device)
        rng.manual_seed(seed)
        fm = FlowMatching(min_sigma=0, inference_mode='euler', num_steps=num_steps)
        
        logger.info(f"Generating audio: prompt='{prompt}', duration={duration:.2f}s")
        start_time = time.time()

        # Generate audio with no_grad (like demo.py)
        with torch.no_grad():
            audios = generate(clip_frames,
                              sync_frames, [prompt],
                              negative_text=[negative_prompt] if negative_prompt else None,
                              feature_utils=feature_utils,
                              net=net,
                              fm=fm,
                              rng=rng,
                              cfg_strength=cfg_strength)

        generation_time = time.time() - start_time
        logger.info(f"Audio generated in {generation_time:.2f}s")

        # Save audio exactly like demo.py
        audio = audios.float().cpu()[0]
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        output_filename = f"audio_{timestamp}_{seed}.flac"  # Use FLAC like demo.py
        output_path = CACHE_DIR / output_filename
        
        # Use torchaudio.save exactly like demo.py (no backend parameter)
        torchaudio.save(output_path, audio, seq_cfg.sampling_rate)
        
        # Clean up temporary video file
        tmp_video_path.unlink()
        
        return FileResponse(
            output_path,
            media_type="audio/flac",  # FLAC instead of WAV
            filename=output_filename,
            headers={
                "X-Generation-Time": str(generation_time),
                "X-Duration": str(video_info.duration_sec),
                "X-Seed": str(seed)
            }
        )
        
    except Exception as e:
        logger.error(f"Generation failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    logger.info("Starting MMAudio Standalone API...")
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")