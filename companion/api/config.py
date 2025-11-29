"""
Shared configuration constants for video-to-audio API clients
"""

# =============================================================================
# Shared Settings (Both MMAudio and HunyuanVideo-Foley)
# =============================================================================

# Supported video formats for Pro Tools integration
SUPPORTED_VIDEO_FORMATS = {
    '.mp4', '.mov', '.avi', '.mkv', 
    '.webm', '.flv', '.wmv', '.m4v'
}

# Common generation parameters
DEFAULT_NEGATIVE_PROMPT = "voices, music, melody, singing, speech,interference"
DEFAULT_SEED = 42

# Output configuration
DEFAULT_OUTPUT_FORMAT = "wav"  # "wav" or "flac"
DEFAULT_TIMEOUT = 300  # seconds

# Video preprocessing
VIDEO_DOWNSCALE_THRESHOLD_MB = 2.0  # Downscale videos larger than this (MB) to 480p for faster upload

# FFmpeg encoding settings
FFMPEG_CRF_QUALITY = 25        # CRF quality (0-51, lower=better, 25=very good)
FFMPEG_PRESET = "ultrafast"    # Encoding speed preset (ultrafast/veryfast/fast/medium)
FFMPEG_TARGET_HEIGHT = 480     # Downscale target height in pixels (480p)

# =============================================================================
# MMAudio-Specific Settings (16kHz output, port 8000)
# =============================================================================

MMAUDIO_DEFAULT_API_URL = "http://localhost:8000"
MMAUDIO_DEFAULT_NUM_STEPS = 25
MMAUDIO_DEFAULT_CFG_STRENGTH = 4.5
MMAUDIO_DEFAULT_MODEL = "large_44k_v2"

# =============================================================================
# HunyuanVideo-Foley-Specific Settings (48kHz output, port 8001)
# =============================================================================

HYVF_DEFAULT_API_URL = "http://localhost:8001"
HYVF_DEFAULT_NUM_STEPS = 50
HYVF_DEFAULT_CFG_STRENGTH = 4.5
HYVF_DEFAULT_MODEL_SIZE = "xxl"  # "xl" or "xxl"
