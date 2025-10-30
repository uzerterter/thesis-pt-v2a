"""
MMAudio Standalone API

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
import asyncio
import threading
import psutil
from collections import OrderedDict
from dataclasses import dataclass
from typing import Any, Dict, Optional

import torch
import numpy as np
import av
from fastapi import FastAPI, UploadFile, File, Form, HTTPException, BackgroundTasks
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

# ========== CACHE SYSTEM ==========
#
# ┌────────────────────────────────────────────────────────────────────────────┐
# │                           CACHE ARCHITECTURE                               │
# ├────────────────────────────────────────────────────────────────────────────┤
# │ TIER 1: MODEL_CACHE (VRAM/GPU Memory)                                      │
# │ ├─ Purpose: Cache loaded PyTorch models to avoid expensive model loading   │
# │ ├─ Storage: GPU VRAM                                                       │
# │ ├─ Lifetime: Until server restart                                          │
# │ ├─ Key: model_name (e.g., "large_44k_v2")                                  │              
# │ └─ Value: (MMAudio network, FeaturesUtils, ModelConfig)                    │
# │                                                                            │
# │ TIER 2: SMART_VIDEO_CACHE (System RAM)                                     │
# │ ├─ Purpose: Cache preprocessed video tensors with intelligent eviction     │
# │ ├─ Storage: System RAM (configurable, default 32GB limit)                  │
# │ ├─ Lifetime: LRU + TTL hybrid (90 minutes default)                         │
# │ ├─ Key: MD5(video_path + duration + file_mtime)                            │
# │ └─ Value: Preprocessed video_info (clip_frames, sync_frames, etc.)         │
# │                                                                            │
# │ TIER 3: CACHE_DIR (Disk Storage)                                           │
# │ ├─ Purpose: Temporary storage for generated audio files                    │
# │ ├─ Storage: Local filesystem                                               │
# │ ├─ Lifetime: Auto-cleanup (5 minutes immediate, 2 hours fallback)          │
# │ ├─ Key: Timestamp-based filenames                                          │
# │ └─ Value: Generated FLAC audio files                                       │
# └────────────────────────────────────────────────────────────────────────────┘
#
# SMART EVICTION STRATEGIES:
# 1. LRU (Least Recently Used): Removes oldest accessed videos when size limit hit
# 2. TTL (Time To Live): Background thread cleans expired videos every 15 minutes
# 3. Size-Based: Intelligent memory estimation and size-aware eviction
# 4. Thread-Safe: Concurrent access from multiple FastAPI workers
#
#
# CONFIGURATION:
# - VIDEO_CACHE_MAX_GB: Maximum RAM for video cache (default: 32GB)
# - VIDEO_CACHE_TTL_MIN: TTL for cached videos (default: 90 minutes)
# - Set via environment variables for production deployment
#
# MONITORING:
# - GET /cache/stats: Detailed cache statistics and memory usage
# - POST /cache/clear: Clear video cache (preserve models)
# - POST /cache/clear-all: Full cache reset including models

@dataclass
class CacheEntry:
    """Cache entry with metadata for LRU + TTL + Size tracking"""
    data: Any                   # The cached video_info object
    created_at: float           # Unix timestamp when cached
    last_accessed: float        # Unix timestamp of last access (LRU)
    estimated_size_mb: float    # Estimated memory usage in MB
    access_count: int = 0       # Number of times accessed (statistics)

class SmartVideoCache:
    """
    Hybrid LRU + TTL + Size-Limit Video Cache
    
    Features:
    - LRU Eviction: Removes least recently used videos when size limit reached
    - TTL Expiry: Automatic cleanup of old videos (configurable)
    - Size Limit: Maximum RAM usage with smart eviction
    - Statistics: Hit rate, memory usage, eviction metrics
    - Thread-safe: Can be accessed from multiple FastAPI workers
    """
    
    def __init__(self, max_size_gb: float = 32, ttl_minutes: int = 90):
        self.max_size_bytes = max_size_gb * 1024 * 1024 * 1024  # Convert to bytes
        self.ttl_seconds = ttl_minutes * 60
        self.cache: OrderedDict[str, CacheEntry] = OrderedDict()
        self.stats = {
            'hits': 0,
            'misses': 0,
            'evictions_lru': 0,
            'evictions_ttl': 0,
            'current_size_mb': 0,
            'total_entries': 0
        }
        self._lock = threading.RLock()  # Thread-safe access
        self._stop_cleanup = threading.Event()
        
        # Start background TTL cleanup thread
        self._cleanup_thread = threading.Thread(target=self._background_ttl_cleanup, daemon=True)
        self._cleanup_thread.start()
        logger.info(f"🕒 TTL cleanup thread started (interval: {max(ttl_minutes // 6, 5)} minutes)")
    
    def _estimate_tensor_size_mb(self, video_info) -> float:
        """Estimate memory usage of video_info object in MB"""
        try:
            # Rough estimation based on typical video tensor sizes
            # clip_frames: [T, H, W, C] float32 tensors
            # sync_frames: [T, H, W, C] float32 tensors
            size_bytes = 0
            
            if hasattr(video_info, 'clip_frames') and video_info.clip_frames is not None:
                # Typical: [8-16 frames, 224, 224, 3] * 4 bytes/float32
                frames_size = video_info.clip_frames.numel() * 4  # float32 = 4 bytes
                size_bytes += frames_size
            
            if hasattr(video_info, 'sync_frames') and video_info.sync_frames is not None:
                sync_size = video_info.sync_frames.numel() * 4
                size_bytes += sync_size
            
            # Add overhead for Python objects (~20%)
            size_bytes = int(size_bytes * 1.2)
            return size_bytes / (1024 * 1024)  # Convert to MB
            
        except Exception:
            # Fallback estimation: ~200MB per video (conservative)
            return 200.0
    
    def _cleanup_expired(self):
        """Remove expired entries based on TTL"""
        current_time = time.time()
        expired_keys = []
        
        for key, entry in self.cache.items():
            if current_time - entry.created_at > self.ttl_seconds:
                expired_keys.append(key)
        
        for key in expired_keys:
            entry = self.cache.pop(key)
            self.stats['evictions_ttl'] += 1
            self.stats['current_size_mb'] -= entry.estimated_size_mb
            self.stats['total_entries'] -= 1
            logger.info(f"🕒 TTL EVICTED: {key[:8]}... (age: {(current_time - entry.created_at)/60:.1f}min)")
    
    def _background_ttl_cleanup(self):
        """Background thread for periodic TTL cleanup (runs even when API is idle)"""
        # Run cleanup every 15 minutes or 1/6 of TTL (whichever is larger)
        cleanup_interval = max(self.ttl_seconds / 6, 300)  # Minimum 5 minutes
        
        while not self._stop_cleanup.is_set():
            self._stop_cleanup.wait(cleanup_interval)
            
            if self._stop_cleanup.is_set():
                break
            
            # Perform TTL cleanup with lock
            with self._lock:
                if self.cache:
                    before_count = len(self.cache)
                    before_size = self.stats['current_size_mb']
                    self._cleanup_expired()
                    after_count = len(self.cache)
                    
                    if before_count != after_count:
                        freed_mb = before_size - self.stats['current_size_mb']
                        logger.info(f"🧹 Background TTL cleanup: {before_count - after_count} videos evicted, {freed_mb:.1f}MB freed")
    
    def _enforce_size_limit(self):
        """Remove LRU entries if size limit exceeded"""
        current_size_bytes = self.stats['current_size_mb'] * 1024 * 1024
        
        while current_size_bytes > self.max_size_bytes and self.cache:
            # Remove least recently used (first item in OrderedDict)
            key, entry = self.cache.popitem(last=False)  # FIFO = LRU in our usage
            current_size_bytes -= entry.estimated_size_mb * 1024 * 1024
            self.stats['evictions_lru'] += 1
            self.stats['current_size_mb'] -= entry.estimated_size_mb
            self.stats['total_entries'] -= 1
            logger.info(f"🗑️ LRU EVICTED: {key[:8]}... (freed: {entry.estimated_size_mb:.1f}MB)")
    
    def get(self, key: str) -> Optional[Any]:
        """Get cached video with LRU update"""
        with self._lock:
            self._cleanup_expired()  # Clean expired entries first
            
            if key in self.cache:
                # Cache hit: Update access time and move to end (most recent)
                entry = self.cache.pop(key)
                entry.last_accessed = time.time()
                entry.access_count += 1
                self.cache[key] = entry  # Move to end of OrderedDict
                
                self.stats['hits'] += 1
                return entry.data
            else:
                # Cache miss
                self.stats['misses'] += 1
                return None
    
    def put(self, key: str, video_info: Any):
        """Cache video with size and TTL management"""
        with self._lock:
            current_time = time.time()
            estimated_size = self._estimate_tensor_size_mb(video_info)
            
            # Remove existing entry if present
            if key in self.cache:
                old_entry = self.cache.pop(key)
                self.stats['current_size_mb'] -= old_entry.estimated_size_mb
                self.stats['total_entries'] -= 1
            
            # Create new cache entry
            entry = CacheEntry(
                data=video_info,
                created_at=current_time,
                last_accessed=current_time,
                estimated_size_mb=estimated_size
            )
            
            # Add to cache (end of OrderedDict = most recent)
            self.cache[key] = entry
            self.stats['current_size_mb'] += estimated_size
            self.stats['total_entries'] += 1
            
            # Enforce limits
            self._cleanup_expired()
            self._enforce_size_limit()
            
            logger.info(f"💾 CACHED: {key[:8]}... ({estimated_size:.1f}MB, total: {self.stats['current_size_mb']:.1f}MB)")
    
    def get_stats(self) -> Dict:
        """Get cache statistics"""
        with self._lock:
            hit_rate = self.stats['hits'] / (self.stats['hits'] + self.stats['misses']) if (self.stats['hits'] + self.stats['misses']) > 0 else 0
            return {
                **self.stats,
                'hit_rate_percent': round(hit_rate * 100, 2),
                'max_size_gb': self.max_size_bytes / (1024**3),
                'ttl_minutes': self.ttl_seconds / 60,
                'utilization_percent': round((self.stats['current_size_mb'] * 1024**2) / self.max_size_bytes * 100, 2)
            }
    
    def clear(self):
        """Clear all cached entries"""
        with self._lock:
            self.cache.clear()
            self.stats = {
                'hits': 0,
                'misses': 0,
                'evictions_lru': 0,
                'evictions_ttl': 0,
                'current_size_mb': 0,
                'total_entries': 0
            }
    
    def shutdown(self):
        """Stop background cleanup thread (called on server shutdown)"""
        self._stop_cleanup.set()
        if self._cleanup_thread.is_alive():
            self._cleanup_thread.join(timeout=5)
            logger.info("🛑 TTL cleanup thread stopped")

# Cache Configuration (Environment Variables supported)
VIDEO_CACHE_MAX_GB = float(os.getenv('VIDEO_CACHE_MAX_GB', '32'))  # 32GB default
VIDEO_CACHE_TTL_MIN = int(os.getenv('VIDEO_CACHE_TTL_MIN', '90'))   # 90 minutes default

# Initialize caches
MODEL_CACHE = {}    # VRAM: {model_name: (net, feature_utils, seq_cfg)}
SMART_VIDEO_CACHE = SmartVideoCache(max_size_gb=VIDEO_CACHE_MAX_GB, ttl_minutes=VIDEO_CACHE_TTL_MIN)
CACHE_DIR = Path("./cache")  # Disk: temporary audio files
CACHE_DIR.mkdir(exist_ok=True)

# Cache cleanup configuration
CACHE_RETENTION_HOURS = 2  # Delete files older than 2 hours
CLEANUP_INTERVAL_MINUTES = 30  # Run cleanup every 30 minutes

def cleanup_old_cache_files():
    """Remove audio files older than CACHE_RETENTION_HOURS"""
    try:
        current_time = time.time()
        retention_seconds = CACHE_RETENTION_HOURS * 3600
        
        for file_path in CACHE_DIR.glob("audio_*.flac"):
            if file_path.is_file():
                file_age = current_time - file_path.stat().st_mtime
                if file_age > retention_seconds:
                    file_path.unlink()
                    logger.info(f"Cleaned up old cache file: {file_path.name}")
    except Exception as e:
        logger.error(f"Cache cleanup error: {e}")

def schedule_cleanup():
    """Background thread for periodic cache cleanup"""
    while True:
        time.sleep(CLEANUP_INTERVAL_MINUTES * 60)
        cleanup_old_cache_files()

# Start cleanup thread on startup
cleanup_thread = threading.Thread(target=schedule_cleanup, daemon=True)
cleanup_thread.start()

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
    """
    MODEL CACHE MECHANISM:
    
    CACHE HIT (model already loaded):
    - Returns cached model from VRAM/RAM instantly
    
    CACHE MISS (first time loading model):
    - Downloads model weights if needed (only once ever)
    - Loads PyTorch model into VRAM
    - Initializes feature extraction utilities
    - Stores in MODEL_CACHE for future requests
    
    Memory Management:
    - Models stay in VRAM until server restart
    - Multiple models can be cached simultaneously
    - No automatic eviction (assumes sufficient VRAM)
    """
    # CACHE HIT: Return immediately from VRAM
    if model_name in MODEL_CACHE:
        logger.info(f"🚀 CACHE HIT: Using cached model '{model_name}' from VRAM")
        return MODEL_CACHE[model_name]
    
    # CACHE MISS: Load model from disk into VRAM
    logger.info(f"💾 CACHE MISS: Loading model '{model_name}' into VRAM...")
    start_time = time.time()
    
    # Validate model exists (logic from mmauudio demo.py)
    if model_name not in all_model_cfg:
        raise ValueError(f'Unknown model variant: {model_name}')
    
    model: ModelConfig = all_model_cfg[model_name]
    model.download_if_needed()  # Download weights if not present
    seq_cfg = model.seq_cfg

    # Step 1: Load main MMAudio network into VRAM  (logic from mmaudio demo.py)
    net: MMAudio = get_my_mmaudio(model.model_name).to(device, dtype).eval()
    net.load_weights(torch.load(model.model_path, map_location=device, weights_only=True))
    
    # Step 2: Load feature extraction utilities into VRAM
    # - VAE encoder/decoder for latent space
    # - Synchformer for video-audio synchronization
    # - BigVGAN vocoder for final audio generation
    feature_utils = FeaturesUtils(tod_vae_ckpt=model.vae_path,
                                  synchformer_ckpt=model.synchformer_ckpt,
                                  enable_conditions=True,
                                  mode=model.mode,
                                  bigvgan_vocoder_ckpt=model.bigvgan_16k_path,
                                  need_vae_encoder=False)
    feature_utils = feature_utils.to(device, dtype).eval()
    
    load_time = time.time() - start_time
    logger.info(f"✅ Model '{model_name}' loaded in {load_time:.2f}s and cached in VRAM")
    
    # Cache complete model setup for future requests
    MODEL_CACHE[model_name] = (net, feature_utils, seq_cfg)
    return net, feature_utils, seq_cfg

def load_video_optimized(video_path: Path, duration_sec: float):
    """
    SMART VIDEO CACHE MECHANISM:
    
    Features:
    - Content-Based Caching: Cache key based on video file content, not filename
    - LRU Eviction: Automatically removes least recently used videos when memory limit reached
    - TTL Expiry: Videos expire after configurable time (default: 90 minutes)
    - Size Tracking: Monitors actual memory usage with intelligent estimation
    - Thread-safe: Multiple concurrent requests handled safely
    - Statistics: Tracks hit rates, evictions, memory usage
    
    CACHE KEY GENERATION:
    - MD5 hash of: first_1MB_of_video + duration + file_size
    - Content-based: Same video content always produces same cache key
    - Duration-aware: Different durations create separate cache entries
    
    CACHE HIT:
    - Returns preprocessed video tensors instantly from RAM
    - Updates LRU order (moves to most recently used)
    - Works even if same video uploaded with different temporary filenames
    
    CACHE MISS:
    - Processes video through MMAudio pipeline
    - Estimates memory usage for smart eviction
    - Auto-evicts old/unused videos if memory limit exceeded
    """
    # Generate content-based cache key (first 1MB + duration + file size)
    try:
        file_size = video_path.stat().st_size
        with open(video_path, 'rb') as f:
            # Read first 1MB for content-based hashing (fast but unique)
            content_sample = f.read(1024 * 1024)  # 1MB sample
        
        # Create hash from content sample + duration + file size
        cache_key_data = f"{hashlib.md5(content_sample).hexdigest()}_{duration_sec}_{file_size}"
        cache_key = hashlib.md5(cache_key_data.encode()).hexdigest()
        
    except Exception as e:
        # Fallback to path-based caching if content reading fails
        logger.warning(f"Content-based caching failed, using path-based: {e}")
        cache_key = hashlib.md5(f"{video_path}_{duration_sec}_{video_path.stat().st_mtime}".encode()).hexdigest()
    
    # Try to get from smart cache
    cached_video = SMART_VIDEO_CACHE.get(cache_key)
    
    if cached_video is not None:
        # CACHE HIT: Return from smart cache
        logger.info(f"🚀 SMART CACHE HIT: '{video_path.name}' (key: {cache_key[:8]}...)")
        return cached_video
    
    # CACHE MISS: Process video and add to smart cache
    logger.info(f"💾 SMART CACHE MISS: Processing '{video_path.name}'...")
    start_time = time.time()
    
    # Process video using MMAudio's native function
    video_info = load_video(video_path, duration_sec)
    
    load_time = time.time() - start_time
    logger.info(f"✅ Video '{video_path.name}' processed in {load_time:.2f}s")
    
    # Add to smart cache (handles size limits, TTL, LRU automatically)
    SMART_VIDEO_CACHE.put(cache_key, video_info)
    
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

@app.get("/cache/stats")
async def get_cache_stats():
    """Get comprehensive cache statistics and system memory info"""
    # System memory info
    memory = psutil.virtual_memory()
    
    # GPU memory info (if available)
    gpu_info = {}
    if torch.cuda.is_available():
        gpu_info = {
            "gpu_memory_allocated_gb": round(torch.cuda.memory_allocated() / (1024**3), 2),
            "gpu_memory_reserved_gb": round(torch.cuda.memory_reserved() / (1024**3), 2),
            "gpu_memory_free_gb": round((torch.cuda.get_device_properties(0).total_memory - torch.cuda.memory_reserved()) / (1024**3), 2),
            "gpu_memory_total_gb": round(torch.cuda.get_device_properties(0).total_memory / (1024**3), 2),
            "gpu_utilization_percent": round(torch.cuda.memory_reserved() / torch.cuda.get_device_properties(0).total_memory * 100, 2)
        }
    
    return {
        "video_cache": SMART_VIDEO_CACHE.get_stats(),
        "model_cache": {
            "loaded_models": len(MODEL_CACHE),
            "model_names": list(MODEL_CACHE.keys())
        },
        "system_memory": {
            "total_gb": round(memory.total / (1024**3), 2),
            "available_gb": round(memory.available / (1024**3), 2),
            "used_gb": round(memory.used / (1024**3), 2),
            "usage_percent": memory.percent
        },
        "gpu_memory": gpu_info,
        "cache_config": {
            "video_cache_max_gb": VIDEO_CACHE_MAX_GB,
            "video_cache_ttl_minutes": VIDEO_CACHE_TTL_MIN
        }
    }

@app.post("/cache/clear")
async def clear_cache():
    """Clear video cache (keeps model cache for performance)"""
    SMART_VIDEO_CACHE.clear()
    return {
        "message": "Video cache cleared successfully",
        "model_cache_preserved": True
    }

@app.post("/cache/clear-all")
async def clear_all_cache():
    """Clear both video and model cache (full reset)"""
    SMART_VIDEO_CACHE.clear()
    MODEL_CACHE.clear()
    
    # Force GPU memory cleanup
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
    
    return {
        "message": "All caches cleared successfully",
        "gpu_memory_cleared": torch.cuda.is_available()
    }

def cleanup_file_after_delay(file_path: Path, delay_minutes: int = 5):
    """Delete file after specified delay (for immediate cleanup after download)"""
    def delayed_cleanup():
        time.sleep(delay_minutes * 60)
        try:
            if file_path.exists():
                file_path.unlink()
                logger.info(f"Immediate cleanup: {file_path.name}")
        except Exception as e:
            logger.error(f"Immediate cleanup error: {e}")
    
    threading.Thread(target=delayed_cleanup, daemon=True).start()

@app.post("/generate")
async def generate_audio(
    background_tasks: BackgroundTasks,
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
        
        # Schedule cleanup of generated audio file after 5 minutes
        cleanup_file_after_delay(output_path, delay_minutes=5)
        
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