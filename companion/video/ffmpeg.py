"""
FFmpeg operations for video processing

Provides:
- FFmpeg availability detection (imageio-ffmpeg or system)
- Video segment trimming with fast copy codec
- Video duration detection using FFprobe
"""

import subprocess
import shutil
import tempfile
import json
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


def get_video_duration(video_path: str) -> Dict[str, any]:
    """
    Get video file duration using FFprobe.
    
    Uses FFprobe (part of FFmpeg) to read video metadata.
    
    Args:
        video_path (str): Path to video file
    
    Returns:
        dict: {
            'success': bool,
            'duration': float or None (duration in seconds),
            'error': str or None
        }
    
    Example:
        >>> result = get_video_duration("video.mp4")
        >>> if result['success']:
        >>>     print(f"Video duration: {result['duration']}s")
        >>> else:
        >>>     print(f"Error: {result['error']}")
    
    Note:
        Uses FFprobe's JSON output format for reliable parsing
    """
    try:
        # Validate input file
        video_path_obj = Path(video_path)
        if not video_path_obj.exists():
            return {
                'success': False,
                'duration': None,
                'error': f'Video file not found: {video_path}'
            }
        
        # Check FFmpeg availability (FFprobe is part of FFmpeg)
        ffmpeg_check = check_ffmpeg_available()
        if not ffmpeg_check['available']:
            return {
                'success': False,
                'duration': None,
                'error': f'FFmpeg/FFprobe not available: {ffmpeg_check["error"]}'
            }
        
        # Try to find FFprobe (usually in same directory as FFmpeg)
        ffmpeg_path = Path(ffmpeg_check['path'])
        ffprobe_path = ffmpeg_path.parent / ('ffprobe.exe' if ffmpeg_path.suffix == '.exe' else 'ffprobe')
        
        # If ffprobe not found next to ffmpeg, try system PATH
        if not ffprobe_path.exists():
            ffprobe_exe = shutil.which('ffprobe')
            if ffprobe_exe:
                ffprobe_path = Path(ffprobe_exe)
        
        # If FFprobe is available, use it (preferred method)
        if ffprobe_path.exists():
            # Build FFprobe command to get duration
            # -v quiet: suppress warnings
            # -print_format json: output as JSON
            # -show_format: show container/format info (includes duration)
            cmd = [
                str(ffprobe_path),
                '-v', 'quiet',
                '-print_format', 'json',
                '-show_format',
                str(video_path)
            ]
            
            # Execute FFprobe
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10  # 10 second timeout
            )
            
            if result.returncode != 0:
                return {
                    'success': False,
                    'duration': None,
                    'error': f'FFprobe failed: {result.stderr}'
                }
            
            # Parse JSON output
            try:
                data = json.loads(result.stdout)
                duration_str = data.get('format', {}).get('duration')
                
                if duration_str is None:
                    return {
                        'success': False,
                        'duration': None,
                        'error': 'Duration not found in FFprobe output'
                    }
                
                duration = float(duration_str)
                
                return {
                    'success': True,
                    'duration': duration,
                    'error': None
                }
                
            except (json.JSONDecodeError, ValueError, KeyError) as e:
                return {
                    'success': False,
                    'duration': None,
                    'error': f'Failed to parse FFprobe output: {e}'
                }
        
        # Fallback: Use FFmpeg to get duration (works when FFprobe is not available)
        # FFmpeg prints duration in stderr when analyzing input file
        else:
            cmd = [
                str(ffmpeg_path),
                '-i', str(video_path),
                '-f', 'null',
                '-'
            ]
            
            # Execute FFmpeg
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10
            )
            
            # Parse duration from stderr (FFmpeg prints: "Duration: HH:MM:SS.ms")
            try:
                stderr = result.stderr
                duration_match = None
                for line in stderr.split('\n'):
                    if 'Duration:' in line:
                        # Extract duration string like "00:01:23.45"
                        import re
                        match = re.search(r'Duration:\s*(\d+):(\d+):(\d+)\.(\d+)', line)
                        if match:
                            hours, minutes, seconds, centiseconds = match.groups()
                            duration = int(hours) * 3600 + int(minutes) * 60 + int(seconds) + int(centiseconds) / 100
                            return {
                                'success': True,
                                'duration': duration,
                                'error': None
                            }
                
                return {
                    'success': False,
                    'duration': None,
                    'error': 'Could not extract duration from FFmpeg output'
                }
                
            except (ValueError, AttributeError) as e:
                return {
                    'success': False,
                    'duration': None,
                    'error': f'Failed to parse FFmpeg output: {str(e)}'
                }
        
    except subprocess.TimeoutExpired:
        return {
            'success': False,
            'duration': None,
            'error': 'FFprobe timed out (10s limit)'
        }
    except Exception as e:
        return {
            'success': False,
            'duration': None,
            'error': f'Duration check error: {str(e)}'
        }

