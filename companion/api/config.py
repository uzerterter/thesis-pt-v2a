"""
Configuration constants for MMAudio API client
"""

# API Configuration
DEFAULT_API_URL = "http://localhost:8000"

# Supported video formats for Pro Tools integration
SUPPORTED_VIDEO_FORMATS = {
    '.mp4', '.mov', '.avi', '.mkv', 
    '.webm', '.flv', '.wmv', '.m4v'
}

# Default generation parameters
DEFAULT_NEGATIVE_PROMPT = "voices, music"
DEFAULT_SEED = 42
DEFAULT_NUM_STEPS = 25
DEFAULT_CFG_STRENGTH = 4.5
DEFAULT_MODEL = "large_44k_v2"

# Output configuration
DEFAULT_OUTPUT_FORMAT = "wav"  # "wav" or "flac"
DEFAULT_TIMEOUT = 300  # seconds
