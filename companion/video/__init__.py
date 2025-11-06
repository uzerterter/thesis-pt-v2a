"""
Video processing utilities for PT V2A plugin

Modules:
- ffmpeg: FFmpeg operations (checking, trimming)
- validation: Video file validation
"""

from .ffmpeg import check_ffmpeg_available, trim_video_segment, get_video_duration
from .validation import validate_video_duration, validate_video_file

__all__ = [
    'check_ffmpeg_available',
    'trim_video_segment',
    'get_video_duration',
    'validate_video_duration',
    'validate_video_file',
]
