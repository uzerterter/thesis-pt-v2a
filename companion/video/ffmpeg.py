"""
FFmpeg operations for video processing

Provides:
- FFmpeg availability detection (imageio-ffmpeg or system)
- Video segment trimming with fast copy codec
"""

import subprocess
import shutil
import tempfile
from pathlib import Path
from typing import Dict, Optional


def check_ffmpeg_available() -> Dict[str, any]:
    """
    Check if FFmpeg is available via imageio-ffmpeg package.
    
    Uses imageio-ffmpeg which provides cross-platform FFmpeg binaries.
    Falls back to system PATH if imageio-ffmpeg is not installed.
    
    Returns:
        dict: {
            'available': bool,
            'version': str or None,
            'path': str or None,
            'source': 'imageio-ffmpeg' or 'system',
            'error': str or None
        }
    
    Example:
        >>> result = check_ffmpeg_available()
        >>> if result['available']:
        >>>     print(f"FFmpeg {result['version']} from {result['source']}")
        >>> else:
        >>>     print(f"FFmpeg not available: {result['error']}")
    """
    try:
        # Priority 1: Use imageio-ffmpeg (cross-platform, bundled with pip)
        try:
            import imageio_ffmpeg
            ffmpeg_path = imageio_ffmpeg.get_ffmpeg_exe()
            source = 'imageio-ffmpeg'
        except ImportError:
            # Priority 2: Fallback to system PATH
            ffmpeg_path = shutil.which('ffmpeg')
            source = 'system'
            
            if not ffmpeg_path:
                return {
                    'available': False,
                    'version': None,
                    'path': None,
                    'source': None,
                    'error': 'FFmpeg not found in system PATH and imageio-ffmpeg not installed'
                }
        
        # Get version info
        result = subprocess.run(
            [ffmpeg_path, '-version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        if result.returncode == 0:
            # Extract version from first line (e.g., "ffmpeg version 4.4.2")
            version_line = result.stdout.split('\n')[0]
            version = version_line.split(' ')[2] if len(version_line.split(' ')) > 2 else 'unknown'
            
            return {
                'available': True,
                'version': version,
                'path': ffmpeg_path,
                'source': source,
                'error': None
            }
        else:
            return {
                'available': False,
                'version': None,
                'path': ffmpeg_path,
                'source': source,
                'error': f'FFmpeg found but returned error code {result.returncode}'
            }
            
    except subprocess.TimeoutExpired:
        return {
            'available': False,
            'version': None,
            'path': None,
            'source': None,
            'error': 'FFmpeg check timed out'
        }
    except Exception as e:
        return {
            'available': False,
            'version': None,
            'path': None,
            'source': None,
            'error': f'FFmpeg check error: {str(e)}'
        }


def trim_video_segment(
    video_path: str,
    start_seconds: float,
    end_seconds: float,
    output_path: Optional[str] = None
) -> Dict[str, any]:
    """
    Trim video segment using FFmpeg with fast copy codec.
    
    Uses FFmpeg's `-c copy` to avoid re-encoding (fast, no quality loss).
    
    Args:
        video_path (str): Path to source video file
        start_seconds (float): Start time in seconds
        end_seconds (float): End time in seconds
        output_path (str, optional): Output path (auto-generated if None)
    
    Returns:
        dict: {
            'success': bool,
            'output_path': str or None,
            'duration': float,
            'error': str or None
        }
    
    Example:
        >>> result = trim_video_segment(
        >>>     "video.mp4",
        >>>     start_seconds=5.0,
        >>>     end_seconds=10.0
        >>> )
        >>> if result['success']:
        >>>     print(f"Trimmed video: {result['output_path']}")
    
    Note:
        - Uses `-c copy` for fast processing (no re-encoding)
        - Oriented on FFmpeg best practices for video trimming
        - Duration is calculated, not read from output file
    """
    try:
        # Validate input file
        video_path_obj = Path(video_path)
        if not video_path_obj.exists():
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': f'Video file not found: {video_path}'
            }
        
        # Check FFmpeg availability and get path
        ffmpeg_check = check_ffmpeg_available()
        if not ffmpeg_check['available']:
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': f'FFmpeg not available: {ffmpeg_check["error"]}'
            }
        
        # Use the FFmpeg path (embedded or system)
        ffmpeg_exe = ffmpeg_check['path']
        
        # Calculate duration
        duration = end_seconds - start_seconds
        
        # Generate output path if not provided
        if output_path is None:
            # Use temp directory for trimmed video
            temp_dir = Path(tempfile.gettempdir()) / "pt_v2a"
            temp_dir.mkdir(exist_ok=True, parents=True)
            
            # Generate unique filename
            timestamp = int(Path(video_path).stat().st_mtime)
            output_path = str(temp_dir / f"trimmed_{timestamp}_{start_seconds}_{end_seconds}.mp4")
        
        # Build FFmpeg command
        # Use embedded/detected FFmpeg path instead of relying on PATH
        # -ss: start time, -to: end time, -c copy: copy codec (no re-encode)
        cmd = [
            ffmpeg_exe,
            '-y',  # Overwrite output file
            '-ss', str(start_seconds),
            '-to', str(end_seconds),
            '-i', str(video_path),
            '-c', 'copy',  # Fast copy codec (no re-encode)
            output_path
        ]
        
        # Execute FFmpeg
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60  # 60 second timeout for trimming
        )
        
        if result.returncode != 0:
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': f'FFmpeg trimming failed: {result.stderr}'
            }
        
        # Verify output file exists
        if not Path(output_path).exists():
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': f'FFmpeg completed but output file not found: {output_path}'
            }
        
        return {
            'success': True,
            'output_path': output_path,
            'duration': duration,
            'error': None
        }
        
    except subprocess.TimeoutExpired:
        return {
            'success': False,
            'output_path': None,
            'duration': None,
            'error': 'FFmpeg trimming timed out (60s limit)'
        }
    except Exception as e:
        return {
            'success': False,
            'output_path': None,
            'duration': None,
            'error': f'Trimming error: {str(e)}'
        }
