"""
HunyuanVideo-Foley Standalone API

Adapted from MMAudio API structure for consistency.
Provides FastAPI endpoints for video-to-audio generation using HunyuanVideo-Foley.
"""

import os
import sys
import logging
import tracemalloc
import gc
from pathlib import Path
from typing import Optional
import hashlib
import tempfile
import time
from datetime import datetime
import threading
import psutil
from collections import OrderedDict, deque
from dataclasses import dataclass
from typing import Any, Dict

import torch
import numpy as np
import av
from fastapi import FastAPI, UploadFile, File, Form, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
import torchaudio

# Add HunyuanVideo-Foley path to sys.path
HYVF_PATH = Path("/workspace/model-tests/repos/HunyuanVideo-Foley")
sys.path.insert(0, str(HYVF_PATH))

try:
    from hunyuanvideo_foley.utils.model_utils import load_model, denoise_process
    from hunyuanvideo_foley.utils.feature_utils import feature_process
    from hunyuanvideo_foley.utils.config_utils import load_config
except ImportError as e:
    logging.error(f"Failed to import HunyuanVideo-Foley: {e}")
    logging.error("Please check HYVF_PATH in the script")
    sys.exit(1)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Ring buffer for storing recent log messages
LOG_BUFFER_SIZE = 500
log_buffer = deque(maxlen=LOG_BUFFER_SIZE)

class BufferHandler(logging.Handler):
    """Custom log handler that stores logs in memory buffer"""
    def emit(self, record):
        try:
            log_entry = {
                "timestamp": datetime.fromtimestamp(record.created).strftime('%Y-%m-%d %H:%M:%S'),
                "level": record.levelname,
                "message": self.format(record),
                "module": record.module
            }
            log_buffer.append(log_entry)
        except Exception:
            self.handleError(record)

# Add buffer handler to root logger
buffer_handler = BufferHandler()
buffer_handler.setFormatter(logging.Formatter('%(levelname)s - %(message)s'))
logging.getLogger().addHandler(buffer_handler)

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

# ========== CACHE SYSTEM ==========
#
# ┌────────────────────────────────────────────────────────────────────────────┐
# │                           CACHE ARCHITECTURE                               │
# ├────────────────────────────────────────────────────────────────────────────┤
# │ TIER 1: MODEL_CACHE (VRAM/GPU Memory)                                      │
# │ ├─ Purpose: Cache loaded PyTorch models to avoid expensive model loading   │
# │ ├─ Storage: GPU VRAM                                                       │
# │ ├─ Lifetime: Until server restart                                          │
# │ ├─ Key: model_size (e.g., "hunyuanvideo-foley-xl")                         │
# │ └─ Value: (model_dict, cfg)                                                │
# │                                                                            │
# │ TIER 2: SMART_VIDEO_CACHE (System RAM)                                     │
# │ ├─ Purpose: Cache preprocessed video features with intelligent eviction    │
# │ ├─ Storage: System RAM (configurable, default 32GB limit)                  │
# │ ├─ Lifetime: LRU + TTL hybrid (90 minutes default)                         │
# │ ├─ Key: MD5(video_path + duration + file_mtime)                            │
# │ └─ Value: Preprocessed video features (visual_feats, text_feats, etc.)     │
# │                                                                            │
# │ TIER 3: CACHE_DIR (Disk Storage)                                           │
# │ ├─ Purpose: Temporary storage for generated audio files                    │
# │ ├─ Storage: Local filesystem                                               │
# │ ├─ Lifetime: Auto-cleanup (5 minutes immediate, 2 hours fallback)          │
# │ ├─ Key: Timestamp-based filenames                                          │
# │ └─ Value: Generated WAV audio files                                        │
# └────────────────────────────────────────────────────────────────────────────┘
#
# SMART EVICTION STRATEGIES:
# 1. LRU (Least Recently Used): Removes oldest accessed videos when size limit hit
# 2. TTL (Time To Live): Background thread cleans expired videos every 15 minutes
# 3. Size-Based: Intelligent memory estimation and size-aware eviction
# 4. Thread-Safe: Concurrent access from multiple FastAPI workers
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
    data: Any                   # The cached video features
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
    
    def _estimate_tensor_size_mb(self, video_features) -> float:
        """
        Estimate memory usage of video features in MB
        
        Recursively counts all tensors in the video features object.
        """
        try:
            size_bytes = 0
            
            # Recursively find all tensors in video features
            def count_tensors(obj, visited=None):
                if visited is None:
                    visited = set()
                
                obj_id = id(obj)
                if obj_id in visited:
                    return 0
                visited.add(obj_id)
                
                total = 0
                if isinstance(obj, torch.Tensor):
                    total += obj.element_size() * obj.nelement()
                elif isinstance(obj, (list, tuple)):
                    for item in obj:
                        total += count_tensors(item, visited)
                elif isinstance(obj, dict):
                    for value in obj.values():
                        total += count_tensors(value, visited)
                elif hasattr(obj, '__dict__'):
                    for value in obj.__dict__.values():
                        total += count_tensors(value, visited)
                
                return total
            
            size_bytes = count_tensors(video_features)
            
            # Add overhead for Python objects and CUDA alignment (~20%)
            size_bytes = int(size_bytes * 1.2)
            
            size_mb = size_bytes / (1024 * 1024)
            
            # Sanity check: if estimation seems too low, use conservative fallback
            if size_mb < 50:
                logger.warning(f"Size estimation seems low ({size_mb:.1f}MB), using conservative fallback")
                size_mb = 500.0
            
            return size_mb
            
        except Exception as e:
            logger.error(f"Size estimation failed: {e}, using fallback")
            # Fallback: conservative estimate for safety
            return 500.0
    
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
            
            # Explicitly delete tensors to free memory
            del entry.data
            del entry
            
            logger.info(f"🕒 TTL EVICTED: {key[:8]}... (age: {(current_time - entry.created_at)/60:.1f}min)")
    
    def _background_ttl_cleanup(self):
        """Background thread for periodic TTL cleanup (runs even when API is idle)"""
        # Run cleanup every 15 minutes or 1/6 of TTL (whichever is larger)
        cleanup_interval = max(self.ttl_seconds / 6, 900)  # Minimum 15 minutes
        
        while not self._stop_cleanup.is_set():
            self._stop_cleanup.wait(cleanup_interval)
            
            if self._stop_cleanup.is_set():
                break
            
            # Perform TTL cleanup with lock
            with self._lock:
                self._cleanup_expired()
    
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
            
            # Explicitly delete tensors to free memory
            del entry.data
            del entry
            
            logger.info(f"🗑️ LRU EVICTED: {key[:8]}... (freed: {entry.estimated_size_mb:.1f}MB)")
    
    def get(self, key: str) -> Optional[Any]:
        """Get cached video with LRU update"""
        with self._lock:
            self._cleanup_expired()  # Clean expired entries first
            
            if key in self.cache:
                entry = self.cache[key]
                entry.last_accessed = time.time()
                entry.access_count += 1
                self.stats['hits'] += 1
                # Move to end (most recently used)
                self.cache.move_to_end(key)
                return entry.data
            else:
                self.stats['misses'] += 1
                return None
    
    def put(self, key: str, video_features: Any):
        """Cache video with size and TTL management"""
        with self._lock:
            current_time = time.time()
            
            # Measure size estimation time for performance tuning
            size_estimation_start = time.time()
            estimated_size = self._estimate_tensor_size_mb(video_features)
            size_estimation_time = time.time() - size_estimation_start
            
            # Remove existing entry if present
            if key in self.cache:
                old_entry = self.cache.pop(key)
                self.stats['current_size_mb'] -= old_entry.estimated_size_mb
                self.stats['total_entries'] -= 1
            
            # Create new cache entry
            entry = CacheEntry(
                data=video_features,
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
            
            logger.info(f"💾 CACHED: {key[:8]}... ({estimated_size:.1f}MB, total: {self.stats['current_size_mb']:.1f}MB) | Size estimation: {size_estimation_time*1000:.1f}ms")
    
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
        """Clear all cached entries and force garbage collection"""
        with self._lock:
            # Delete all entries explicitly before clearing
            for key, entry in list(self.cache.items()):
                del entry.data
                del entry
            
            self.cache.clear()
            self.stats = {
                'hits': 0,
                'misses': 0,
                'evictions_lru': 0,
                'evictions_ttl': 0,
                'current_size_mb': 0,
                'total_entries': 0
            }
            
            # Force garbage collection to free memory immediately
            gc.collect()
            logger.info("🧹 Cache cleared and garbage collected")
    
    def shutdown(self):
        """Stop background cleanup thread (called on server shutdown)"""
        self._stop_cleanup.set()
        if self._cleanup_thread.is_alive():
            self._cleanup_thread.join(timeout=5)
            logger.info("🛑 TTL cleanup thread stopped")

# Cache Configuration (Environment Variables supported)
VIDEO_CACHE_MAX_GB = float(os.getenv('VIDEO_CACHE_MAX_GB', '32'))  # 32GB default max size
VIDEO_CACHE_TTL_MIN = int(os.getenv('VIDEO_CACHE_TTL_MIN', '60'))   # 60 minutes default TTL

# Initialize caches
MODEL_CACHE = {}    # VRAM: {model_size: (model_dict, cfg)}
SMART_VIDEO_CACHE = SmartVideoCache(max_size_gb=VIDEO_CACHE_MAX_GB, ttl_minutes=VIDEO_CACHE_TTL_MIN)
CACHE_DIR = Path("./cache")  # Disk: temporary audio files
CACHE_DIR.mkdir(exist_ok=True)

# Cache cleanup configuration
CACHE_RETENTION_HOURS = 2  # Delete audio files older than 2 hours
CLEANUP_INTERVAL_MINUTES = 30  # Run cleanup every 30 minutes

def cleanup_old_cache_files():
    """Remove audio files older than CACHE_RETENTION_HOURS"""
    try:
        current_time = time.time()
        retention_seconds = CACHE_RETENTION_HOURS * 3600
        
        for file_path in CACHE_DIR.glob("audio_*.wav"):
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

# ========== FASTAPI APPLICATION ==========
app = FastAPI(
    title="HunyuanVideo-Foley Standalone API",
    description="High-performance video-to-audio generation API using HunyuanVideo-Foley",
    version="1.0.0"
)

# Enable CORS for web requests
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["GET", "POST", "PUT", "DELETE"],
    allow_headers=["*"],
)

def get_cached_model(model_size: str = 'xxl'):
    """
    MODEL CACHE MECHANISM with model-size-aware caching
    
    Cache key includes model size (xl/xxl) to allow caching both variants.
    
    Example cache keys:
    - "hunyuanvideo-foley-xl"
    - "hunyuanvideo-foley-xxl"
    
    CACHE HIT (model already loaded):
    - Returns cached model from VRAM instantly
    
    CACHE MISS (first time loading model):
    - Downloads model weights if needed (only once ever)
    - Loads PyTorch model into VRAM
    - Initializes feature extraction utilities
    - Stores in MODEL_CACHE for future requests
    
    Memory Impact:
    - XL: ~8-12GB VRAM
    - XXL: ~16-20GB VRAM
    - Both cached: ~28GB total (system has 47GB - OK)
    
    Args:
        model_size: "xl" or "xxl"
    
    Returns:
        tuple: (model_dict, cfg)
    """
    cache_key = f"hunyuanvideo-foley-{model_size}"
    
    # CACHE HIT: Return immediately from VRAM
    if cache_key in MODEL_CACHE:
        logger.info(f"🚀 CACHE HIT: Using cached model '{cache_key}' from VRAM")
        return MODEL_CACHE[cache_key]
    
    # CACHE MISS: Load model from disk into VRAM
    logger.info(f"💾 CACHE MISS: Loading model '{cache_key}' into VRAM...")
    start_time = time.time()
    
    # Validate model size
    if model_size not in ['xl', 'xxl']:
        raise ValueError(f"Invalid model_size: {model_size}. Must be 'xl' or 'xxl'")
    
    # Load config
    config_path = HYVF_PATH / f"configs/hunyuanvideo-foley-{model_size}.yaml"
    if not config_path.exists():
        raise ValueError(f"Config not found: {config_path}")
    
    cfg = load_config(str(config_path))
    
    # Load model
    model_dict = load_model(
        model_path=str(HYVF_PATH / "weights"),  # Adjust if weights are elsewhere
        config=cfg,
        device=device
    )
    
    load_time = time.time() - start_time
    logger.info(f"✅ Model '{cache_key}' loaded in {load_time:.2f}s and cached in VRAM")
    logger.info(f"📊 Sample rate: {cfg.get('sample_rate', 48000)} Hz")
    
    # Cache model
    MODEL_CACHE[cache_key] = (model_dict, cfg)
    return model_dict, cfg

def load_video_optimized(video_path: Path, prompt: str, model_dict: dict, cfg: dict, negative_prompt: Optional[str] = None):
    """
    SMART VIDEO CACHE MECHANISM for HunyuanVideo-Foley:
    
    Features:
    - Content-Based Caching: Cache key based on video file content, not filename
    - LRU Eviction: Automatically removes least recently used videos when memory limit reached
    - TTL Expiry: Videos expire after configurable time (default: 90 minutes)
    - Size Tracking: Monitors actual memory usage with intelligent estimation
    - Thread-safe: Multiple concurrent requests handled safely
    - Statistics: Tracks hit rates, evictions, memory usage
    
    CACHE KEY GENERATION:
    - MD5 hash of: first_1MB_of_video + prompt + negative_prompt
    - Content-based: Same video content + prompt always produces same cache key
    - Prompt-aware: Different prompts create separate cache entries
    
    CACHE HIT:
    - Returns preprocessed video features instantly from RAM
    - Updates LRU order (moves to most recently used)
    - Works even if same video uploaded with different temporary filenames
    
    CACHE MISS:
    - Processes video through HunyuanVideo-Foley feature_process
    - Estimates memory usage for smart eviction
    - Auto-evicts old/unused videos if memory limit exceeded
    
    Returns:
        tuple: (visual_feats, text_feats, audio_len_in_s)
    """
    # Generate content-based cache key (first 1MB + prompt + negative_prompt)
    try:
        file_size = video_path.stat().st_size
        with open(video_path, 'rb') as f:
            # Read first 1MB for content-based hashing (fast but unique)
            content_sample = f.read(1024 * 1024)  # 1MB sample
        
        # Create hash from content sample + prompt + negative_prompt
        neg_prompt_str = negative_prompt or ""
        cache_key_data = f"{hashlib.md5(content_sample).hexdigest()}_{prompt}_{neg_prompt_str}_{file_size}"
        cache_key = hashlib.md5(cache_key_data.encode()).hexdigest()
        
    except Exception as e:
        # Fallback to path-based caching if content reading fails
        logger.warning(f"Content-based caching failed, using path-based: {e}")
        cache_key = hashlib.md5(f"{video_path}_{prompt}_{negative_prompt}_{video_path.stat().st_mtime}".encode()).hexdigest()
    
    # Try to get from smart cache
    cached_features = SMART_VIDEO_CACHE.get(cache_key)
    
    if cached_features is not None:
        # CACHE HIT: Return from smart cache
        logger.info(f"🚀 SMART CACHE HIT: '{video_path.name}' (key: {cache_key[:8]}...)")
        return cached_features
    
    # CACHE MISS: Process video and add to smart cache
    logger.info(f"💾 SMART CACHE MISS: Processing '{video_path.name}'...")
    start_time = time.time()
    
    # Process video using HunyuanVideo-Foley's feature_process
    try:
        visual_feats, text_feats, audio_len_in_s = feature_process(
            str(video_path),
            prompt,
            model_dict,
            cfg,
            neg_prompt=negative_prompt
        )
        
        # Validate features
        if visual_feats is None or text_feats is None:
            raise ValueError(f"Feature processing returned None for '{video_path.name}'")
            
    except Exception as e:
        logger.error(f"❌ Video feature processing failed for '{video_path.name}': {e}")
        raise ValueError(f"Failed to process video '{video_path.name}': {str(e)}") from e
    
    load_time = time.time() - start_time
    logger.info(f"✅ Video '{video_path.name}' processed in {load_time:.2f}s (audio length: {audio_len_in_s:.2f}s)")
    
    # Package features for caching
    features = (visual_feats, text_feats, audio_len_in_s)
    
    # Add to smart cache (handles size limits, TTL, LRU automatically)
    SMART_VIDEO_CACHE.put(cache_key, features)
    
    return features

@app.get("/")
async def root():
    """API health check"""
    return {
        "message": "HunyuanVideo-Foley Standalone API",
        "status": "running",
        "device": device,
        "version": "1.0.0"
    }

@app.get("/models")
async def list_models():
    """List available models"""
    return {
        "available_models": ["hunyuanvideo-foley-xl", "hunyuanvideo-foley-xxl"],
        "loaded_models": list(MODEL_CACHE.keys())
    }

@app.get("/logs")
async def get_logs(limit: int = 100, level: Optional[str] = None):
    """Get recent log entries"""
    limit = min(limit, LOG_BUFFER_SIZE)
    logs = list(log_buffer)
    
    if level:
        level_upper = level.upper()
        logs = [log for log in logs if log["level"] == level_upper]
    
    logs.reverse()
    return {
        "total_entries": len(logs),
        "returned_entries": min(limit, len(logs)),
        "buffer_size": LOG_BUFFER_SIZE,
        "filter_level": level.upper() if level else None,
        "logs": logs[:limit]
    }

@app.get("/logs/tail")
async def tail_logs(lines: int = 50):
    """Get recent logs in plain text format"""
    lines = min(lines, LOG_BUFFER_SIZE)
    logs = list(log_buffer)[-lines:]
    
    text_output = []
    text_output.append("=" * 80)
    text_output.append(f"HunyuanVideo-Foley API Logs (last {len(logs)} entries)")
    text_output.append("=" * 80)
    text_output.append("")
    
    for log_entry in logs:
        text_output.append(f"[{log_entry['timestamp']}] {log_entry['level']:8} | {log_entry['message']}")
    
    text_output.append("")
    text_output.append("=" * 80)
    
    return StreamingResponse(
        iter(["\n".join(text_output)]),
        media_type="text/plain",
        headers={"Content-Disposition": "inline"}
    )

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
    """Clear both video and model cache (full reset with explicit memory cleanup)"""
    
    # Clear video cache (already has proper cleanup with gc)
    SMART_VIDEO_CACHE.clear()
    
    # Clear model cache WITH EXPLICIT DELETION
    logger.info(f"Clearing {len(MODEL_CACHE)} cached models...")
    for model_name, (model_dict, cfg) in list(MODEL_CACHE.items()):
        logger.info(f"Deleting model '{model_name}' from VRAM...")
        # Explicitly delete PyTorch models to free VRAM
        del model_dict
        del cfg
    
    MODEL_CACHE.clear()
    
    # Force garbage collection (multiple passes for thorough cleanup)
    logger.info("Running garbage collection...")
    collected_objects = 0
    for i in range(3):
        collected = gc.collect()
        collected_objects += collected
        logger.info(f"GC pass {i+1}/3: collected {collected} objects")
    
    # Clear GPU memory
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
        logger.info("GPU cache cleared")
    
    # Get memory info after cleanup
    process = psutil.Process()
    mem_after = process.memory_info()
    
    logger.info(f"✅ All caches cleared | RAM after: {mem_after.rss / 1024**3:.2f} GB")
    
    return {
        "message": "All caches cleared successfully",
        "gpu_memory_cleared": torch.cuda.is_available(),
        "gc_objects_collected": collected_objects,
        "ram_after_gb": round(mem_after.rss / 1024**3, 2),
        "note": "Models will need to be reloaded on next request (30-60s delay)"
    }

def cleanup_file_after_delay(file_path: Path, delay_minutes: int = 5):
    """Delete file after specified delay"""
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
    seed: int = Form(0),
    model_size: str = Form("xxl"),  # "xl" or "xxl"
    num_steps: int = Form(50),
    cfg_strength: float = Form(4.5),
    output_format: str = Form("wav")
):
    """
    Generate audio from video using HunyuanVideo-Foley
    
    Args:
        video: Input video file
        prompt: Text prompt describing desired audio
        negative_prompt: Negative prompt (optional)
        seed: Random seed for reproducibility
        model_size: "xl" (faster, 8-12GB VRAM) or "xxl" (higher quality, 16-20GB VRAM)
        num_steps: Number of inference steps (default: 50)
        cfg_strength: Classifier-free guidance strength (default: 4.5)
        output_format: Output format ("wav" or "flac")
    """
    try:
        # Save uploaded video to temporary file
        with tempfile.NamedTemporaryFile(delete=False, suffix='.mp4') as tmp_file:
            content = await video.read()
            tmp_file.write(content)
            tmp_video_path = Path(tmp_file.name)
        
        # Frame check: disabled by default. Set VIDEO_FRAME_CHECK=1/true/yes/on to enable quick first-frame decode.
        FRAME_CHECK_ENABLED = os.getenv("VIDEO_FRAME_CHECK", "false").strip().lower() in ("1", "true", "yes", "on")
        
        validation_start = time.time()
        duration_actual = 0.0
        try:
            with av.open(str(tmp_video_path)) as container:
                if not container.streams.video:
                    raise HTTPException(status_code=400, detail="Video file has no video stream")
                
                stream = container.streams.video[0]
                
                # Get duration
                try:
                    if stream.duration is not None and stream.time_base is not None:
                        duration_actual = float(stream.duration * stream.time_base)
                    elif container.duration is not None:
                        duration_actual = float(container.duration / av.time_base)
                    else:
                        duration_actual = 0.0
                except Exception as e:
                    logger.warning(f"Could not determine duration: {e}, defaulting to 0.0")
                    duration_actual = 0.0
                
                logger.info(f"📊 Video duration: {duration_actual:.2f}s")
                
                # Optional frame check
                if FRAME_CHECK_ENABLED:
                    frame_check_start = time.time()
                    try:
                        for frame in container.decode(video=0):
                            logger.info(f"✅ Frame check passed: decoded frame {frame.width}x{frame.height}")
                            break
                    except Exception as e:
                        raise HTTPException(
                            status_code=400, 
                            detail=f"Video file appears corrupted (frame decode failed): {e}"
                        )
                    frame_check_time = time.time() - frame_check_start
                    logger.info(f"⏱️  Frame validation: {frame_check_time*1000:.1f}ms")
                else:
                    logger.info("⏭️  Frame validation skipped (VIDEO_FRAME_CHECK not enabled)")

        except HTTPException:
            raise
        except Exception as e:
            raise HTTPException(status_code=400, detail=f"Failed to read video: {e}")
        
        validation_total_time = time.time() - validation_start
        logger.info(f"⏱️  Total validation time: {validation_total_time*1000:.1f}ms")

        # Validate duration limits
        if duration_actual < 1:
            raise HTTPException(status_code=400, detail=f"Video too short: {duration_actual:.2f}s (minimum: 1s)")
        if duration_actual > 30:
            logger.warning(f"Video is long ({duration_actual:.2f}s), processing may take time")

        # Validate model size
        if model_size not in ["xl", "xxl"]:
            raise HTTPException(status_code=400, detail=f"Invalid model_size: {model_size}. Must be 'xl' or 'xxl'")
        
        # Load model (cached in VRAM)
        model_dict, cfg = get_cached_model(model_size)
        
        # Set seed for reproducibility
        torch.manual_seed(seed)
        if torch.cuda.is_available():
            torch.cuda.manual_seed_all(seed)
        
        logger.info(f"🎬 Generating audio: prompt='{prompt}', model={model_size}, seed={seed}, steps={num_steps}, cfg={cfg_strength}")
        start_time = time.time()
        
        # Process video features (cached in RAM with smart eviction)
        visual_feats, text_feats, audio_len_in_s = load_video_optimized(
            tmp_video_path,
            prompt,
            model_dict,
            cfg,
            negative_prompt=negative_prompt if negative_prompt else None
        )
        
        feature_time = time.time() - start_time
        logger.info(f"⏱️  Feature processing: {feature_time:.2f}s (audio length: {audio_len_in_s:.2f}s)")
        
        # Generate audio
        denoise_start = time.time()
        with torch.no_grad():
            audio, sample_rate = denoise_process(
                visual_feats,
                text_feats,
                audio_len_in_s,
                model_dict,
                cfg,
                guidance_scale=cfg_strength,
                num_inference_steps=num_steps
            )
        
        denoise_time = time.time() - denoise_start
        generation_time = time.time() - start_time
        logger.info(f"⏱️  Audio denoising: {denoise_time:.2f}s")
        logger.info(f"✅ Total generation time: {generation_time:.2f}s at {sample_rate} Hz")
        
        # Save audio
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        audio_filename = f"audio_{timestamp}_{seed}.{output_format}"
        audio_path = CACHE_DIR / audio_filename
        
        # audio is shape [batch, channels, samples] - take first batch item
        if isinstance(audio, torch.Tensor):
            if audio.dim() == 3:
                audio_to_save = audio[0].cpu()
            else:
                audio_to_save = audio.cpu()
        else:
            audio_to_save = torch.tensor(audio[0]) if len(audio.shape) == 3 else torch.tensor(audio)
        
        torchaudio.save(str(audio_path), audio_to_save, sample_rate)
        logger.info(f"💾 Audio file saved: {audio_path.name} ({audio_to_save.shape})")
        
        # Schedule cleanup (delete after 5 minutes)
        cleanup_file_after_delay(audio_path, delay_minutes=5)
        
        # Clean up temporary video
        try:
            tmp_video_path.unlink()
        except Exception as e:
            logger.warning(f"Failed to delete temp video: {e}")
        
        return FileResponse(
            audio_path,
            media_type=f"audio/{output_format}",
            filename=audio_filename,
            headers={
                "X-Generation-Time": str(generation_time),
                "X-Feature-Time": str(feature_time),
                "X-Denoise-Time": str(denoise_time),
                "X-Seed": str(seed),
                "X-Sample-Rate": str(sample_rate),
                "X-Model-Size": model_size,
                "X-Audio-Length": str(audio_len_in_s)
            }
        )
        
    except HTTPException:
        # Clean up temp file on validation errors
        if 'tmp_video_path' in locals() and tmp_video_path.exists():
            tmp_video_path.unlink()
        raise
        
    except Exception as e:
        # Clean up temp file on any error
        if 'tmp_video_path' in locals() and tmp_video_path.exists():
            tmp_video_path.unlink()
        
        logger.error(f"❌ Generation failed: {e}")
        import traceback
        logger.error(traceback.format_exc())
        raise HTTPException(status_code=500, detail=str(e))

# ========== MEMORY PROFILING ==========
@app.get("/memory/profile")
async def get_memory_profile():
    """Get detailed memory profiling information"""
    from collections import Counter
    
    try:
        # Force garbage collection before profiling
        gc.collect()
        
        # Process memory info
        process = psutil.Process()
        mem_info = process.memory_info()
        
        # System memory
        system_mem = psutil.virtual_memory()
        
        # Count Python objects by type
        obj_counts = Counter()
        obj_sizes = {}
        
        for obj in gc.get_objects():
            obj_type = type(obj).__name__
            obj_counts[obj_type] += 1
            
            try:
                size = sys.getsizeof(obj)
                if obj_type not in obj_sizes:
                    obj_sizes[obj_type] = 0
                obj_sizes[obj_type] += size
            except:
                pass
        
        # Sort by size and get top 30
        top_objects = sorted(obj_sizes.items(), key=lambda x: x[1], reverse=True)[:30]
        
        # GPU memory (if available)
        gpu_memory = {}
        if torch.cuda.is_available():
            gpu_memory = {
                "allocated_gb": round(torch.cuda.memory_allocated() / (1024**3), 2),
                "reserved_gb": round(torch.cuda.memory_reserved() / (1024**3), 2),
                "total_gb": round(torch.cuda.get_device_properties(0).total_memory / (1024**3), 2),
            }
        
        # tracemalloc stats (if enabled)
        tracemalloc_stats = {}
        if tracemalloc.is_tracing():
            snapshot = tracemalloc.take_snapshot()
            top_stats = snapshot.statistics('lineno')[:10]
            tracemalloc_stats = {
                "enabled": True,
                "top_allocations": [
                    {
                        "file": str(stat.traceback),
                        "size_mb": round(stat.size / 1024 / 1024, 2),
                        "count": stat.count
                    }
                    for stat in top_stats
                ]
            }
        else:
            tracemalloc_stats = {"enabled": False}
        
        result = {
            "process_memory": {
                "rss_gb": round(mem_info.rss / 1024 / 1024 / 1024, 2),
                "vms_gb": round(mem_info.vms / 1024 / 1024 / 1024, 2),
            },
            "system_memory": {
                "total_gb": round(system_mem.total / 1024 / 1024 / 1024, 2),
                "available_gb": round(system_mem.available / 1024 / 1024 / 1024, 2),
                "used_gb": round(system_mem.used / 1024 / 1024 / 1024, 2),
                "percent": system_mem.percent
            },
            "gpu_memory": gpu_memory,
            "top_objects": [
                {
                    "type": obj_type,
                    "total_size_mb": round(size / 1024 / 1024, 2),
                    "count": obj_counts[obj_type],
                    "avg_size_kb": round(size / obj_counts[obj_type] / 1024, 2) if obj_counts[obj_type] > 0 else 0
                }
                for obj_type, size in top_objects
            ],
            "tracemalloc": tracemalloc_stats,
            "cache_stats": {
                "model_cache_entries": len(MODEL_CACHE),
                "video_cache_entries": SMART_VIDEO_CACHE.stats['total_entries'],
                "video_cache_size_mb": round(SMART_VIDEO_CACHE.stats['current_size_mb'], 2)
            }
        }
        
        return result
        
    except Exception as e:
        logger.error(f"Memory profiling failed: {e}")
        import traceback
        logger.error(traceback.format_exc())
        raise HTTPException(status_code=500, detail=str(e))

# ========== RUN APPLICATION ==========
if __name__ == "__main__":
    tracemalloc.start()
    logger.info("Starting HunyuanVideo-Foley Standalone API...")
    logger.info("tracemalloc enabled for memory profiling")
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")
