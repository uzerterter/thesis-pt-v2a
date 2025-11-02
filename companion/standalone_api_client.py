#!/usr/bin/env python3
"""
Standalone MMAudio API Client

A client script that uses the standalone MMAudio API server for video-to-audio generation.

Usage:
    # Interactive Mode (current)
    python standalone_api_client.py
    
    # CLI Mode (Pro Tools ready)
    python standalone_api_client.py --video /path/to/video.mp4 --prompt "ocean waves"
    
    # Full CLI Mode
    python standalone_api_client.py \
        --video /path/to/video.mp4 \
        --prompt "drums and bass" \
        --negative-prompt "voices, music" \
        --seed 42 \
        --output /path/to/output.flac

Features:
- Interactive parameter input (prompt, negative prompt, seed)
- CLI arguments for Pro Tools integration
- Automatic video duration detection
- Progress feedback and timing information
- Compatible with existing video test files
- Multiple video format support (MP4, MOV, AVI)
"""

import requests
import time
import os
import argparse
import sys
import json
import subprocess
import shutil
import platform
from pathlib import Path
from typing import Optional, Tuple, Dict

# Configuration
DEFAULT_API_URL = "http://localhost:8000"
#DEFAULT_VIDEO_PATH = "/mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4"
DEFAULT_VIDEO_PATH = r"C:\Users\Ludenbold\Desktop\Master_Thesis\Implementation\model-tests\data\MMAudio_examples\noSound\sora_galloping.mp4"

# Supported video formats for Pro Tools integration
SUPPORTED_VIDEO_FORMATS = {'.mp4', '.mov', '.avi', '.mkv', '.webm', '.flv', '.wmv', '.m4v'}

# =============================================================================
# Pro Tools Timeline Selection & FFmpeg Video Trimming
# =============================================================================

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
                    'error': 'FFmpeg not found (imageio-ffmpeg not installed and not in system PATH)'
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
                'source': source,  # Already set earlier (imageio-ffmpeg or system)
                'error': None
            }
        else:
            return {
                'available': False,
                'version': None,
                'path': None,
                'error': f'FFmpeg check failed: {result.stderr}'
            }
            
    except subprocess.TimeoutExpired:
        return {
            'available': False,
            'version': None,
            'path': None,
            'error': 'FFmpeg check timed out'
        }
    except Exception as e:
        return {
            'available': False,
            'version': None,
            'path': None,
            'error': f'FFmpeg check error: {str(e)}'
        }


def timecode_to_seconds(timecode: str, fps: float = 30.0) -> float:
    """
    Convert Pro Tools timecode format to seconds.
    
    Pro Tools uses "HH:MM:SS:FF" format where FF is frame number.
    
    Args:
        timecode (str): Timecode string (e.g., "00:00:10:15")
        fps (float): Frame rate (default: 30.0, common in NTSC video)
    
    Returns:
        float: Time in seconds
    
    Example:
        >>> timecode_to_seconds("00:00:10:00", fps=30.0)
        10.0
        >>> timecode_to_seconds("00:01:00:15", fps=30.0)
        60.5
    
    Note:
        Based on py-ptsl's util.timecode_info() function which handles
        various frame rates (23.976, 24, 25, 29.97, 30, etc.)
    """
    try:
        parts = timecode.split(':')
        if len(parts) != 4:
            raise ValueError(f"Invalid timecode format: {timecode}")
        
        hours, minutes, seconds, frames = map(int, parts)
        
        # Convert to total seconds
        total_seconds = (
            hours * 3600 +
            minutes * 60 +
            seconds +
            frames / fps
        )
        
        return total_seconds
        
    except (ValueError, IndexError) as e:
        raise ValueError(f"Failed to parse timecode '{timecode}': {e}")


def get_video_timeline_selection(
    company_name: str = "Master Thesis",
    app_name: str = "PT V2A Plugin"
) -> Dict[str, any]:
    """
    Get current timeline selection from Pro Tools using py-ptsl.
    
    Uses py-ptsl's Engine.get_timeline_selection() to read In/Out points
    from Pro Tools timeline.
    
    Args:
        company_name (str): Company name for PTSL connection
        app_name (str): Application name for PTSL connection
    
    Returns:
        dict: {
            'success': bool,
            'in_time': str (timecode),
            'out_time': str (timecode),
            'duration_seconds': float,
            'error': str or None
        }
    
    Example:
        >>> result = get_video_timeline_selection()
        >>> if result['success']:
        >>>     print(f"Selection: {result['in_time']} - {result['out_time']}")
        >>>     print(f"Duration: {result['duration_seconds']:.2f}s")
    
    Note:
        Oriented on py-ptsl implementation:
        - Uses Engine.get_timeline_selection() with TimeCode format
        - Handles PTSL connection and error handling
        - Returns timecode strings in "HH:MM:SS:FF" format
    """
    try:
        # Import py-ptsl (lazy import to avoid startup overhead)
        from ptsl import open_engine
        import ptsl.PTSL_pb2 as pt
        
        # Connect to Pro Tools via PTSL
        with open_engine(
            company_name=company_name,
            application_name=app_name
        ) as engine:
            # Get timeline selection using py-ptsl
            # Returns tuple of (in_time, out_time) as timecode strings
            in_time, out_time = engine.get_timeline_selection(format=pt.TimeCode)
            
            # Get session frame rate for accurate conversion
            session_rate = engine.session_timecode_rate()
            
            # Convert frame rate enum to FPS number
            # Based on py-ptsl's util.timecode_info()
            fps_map = {
                pt.STCR_Fps24: 24.0,
                pt.STCR_Fps23976: 23.976,
                pt.STCR_Fps25: 25.0,
                pt.STCR_Fps2997: 29.97,
                pt.STCR_Fps2997Drop: 29.97,
                pt.STCR_Fps30: 30.0,
                pt.STCR_Fps30Drop: 30.0,
                pt.STCR_Fps48: 48.0,
                pt.STCR_Fps50: 50.0,
                pt.STCR_Fps5994: 59.94,
                pt.STCR_Fps60: 60.0,
            }
            fps = fps_map.get(session_rate, 30.0)  # Default to 30 if unknown
            
            # Convert to seconds
            in_seconds = timecode_to_seconds(in_time, fps)
            out_seconds = timecode_to_seconds(out_time, fps)
            duration = out_seconds - in_seconds
            
            return {
                'success': True,
                'in_time': in_time,
                'out_time': out_time,
                'in_seconds': in_seconds,
                'out_seconds': out_seconds,
                'duration_seconds': duration,
                'fps': fps,
                'error': None
            }
            
    except ImportError as e:
        return {
            'success': False,
            'in_time': None,
            'out_time': None,
            'duration_seconds': None,
            'error': f'py-ptsl not available: {e}'
        }
    except Exception as e:
        return {
            'success': False,
            'in_time': None,
            'out_time': None,
            'duration_seconds': None,
            'error': f'PTSL error: {str(e)}'
        }


def get_video_file_from_protools(
    company_name: str = "Master Thesis",
    app_name: str = "PT V2A Plugin"
) -> Dict[str, any]:
    """
    Get video file path from Pro Tools session using py-ptsl.
    
    Uses py-ptsl's Engine.get_file_location() with Video_Files filter
    to find video files in the current session.
    
    Args:
        company_name (str): Company name for PTSL connection
        app_name (str): Application name for PTSL connection
    
    Returns:
        dict: {
            'success': bool,
            'video_path': str or None,
            'video_files': list of str (all found videos),
            'error': str or None
        }
    
    Example:
        >>> result = get_video_file_from_protools()
        >>> if result['success']:
        >>>     print(f"Video: {result['video_path']}")
    
    Note:
        Oriented on py-ptsl implementation:
        - Uses Engine.get_file_location(filters=[pt.Video_Files])
        - Returns first video file found
        - Handles multiple video files case
    """
    try:
        # Import py-ptsl
        from ptsl import open_engine
        import ptsl.PTSL_pb2 as pt
        
        # Connect to Pro Tools via PTSL
        with open_engine(
            company_name=company_name,
            application_name=app_name
        ) as engine:
            # Get video file locations using py-ptsl
            # Returns list of FileLocation objects
            file_locations = engine.get_file_location(filters=[pt.Video_Files])
            
            if not file_locations:
                return {
                    'success': False,
                    'video_path': None,
                    'video_files': [],
                    'error': 'No video files found in Pro Tools session'
                }
            
            # Extract file paths from FileLocation objects
            video_paths = [loc.full_path for loc in file_locations]
            
            # Return first video file (most common case)
            return {
                'success': True,
                'video_path': video_paths[0],
                'video_files': video_paths,
                'error': None
            }
            
    except ImportError as e:
        return {
            'success': False,
            'video_path': None,
            'video_files': [],
            'error': f'py-ptsl not available: {e}'
        }
    except Exception as e:
        return {
            'success': False,
            'video_path': None,
            'video_files': [],
            'error': f'PTSL error: {str(e)}'
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
                'error': f"FFmpeg not available: {ffmpeg_check['error']}"
            }
        
        # Use the FFmpeg path (embedded or system)
        ffmpeg_exe = ffmpeg_check['path']
        
        # Calculate duration
        duration = end_seconds - start_seconds
        
        # Generate output path if not provided
        if output_path is None:
            import tempfile
            temp_dir = Path(tempfile.gettempdir()) / "pt_v2a_trimmed"
            temp_dir.mkdir(exist_ok=True, parents=True)
            
            timestamp = int(time.time())
            output_filename = f"trimmed_{timestamp}{video_path_obj.suffix}"
            output_path = str(temp_dir / output_filename)
        
        # Build FFmpeg command
        # Use embedded/detected FFmpeg path instead of relying on PATH
        # -ss: start time, -to: end time, -c copy: copy codec (no re-encode)
        cmd = [
            ffmpeg_exe,  # Use detected FFmpeg path (embedded or system)
            '-y',  # Overwrite output file
            '-ss', str(start_seconds),
            '-to', str(end_seconds),
            '-i', str(video_path),
            '-c', 'copy',  # Fast copy, no re-encoding
            output_path
        ]
        
        # Execute FFmpeg
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60  # 60s timeout for trimming
        )
        
        if result.returncode != 0:
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': f'FFmpeg failed: {result.stderr}'
            }
        
        # Verify output file exists
        if not Path(output_path).exists():
            return {
                'success': False,
                'output_path': None,
                'duration': None,
                'error': 'FFmpeg succeeded but output file not found'
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


def validate_video_duration(duration_seconds: float, max_duration: float = 10.0) -> Dict[str, any]:
    """
    Validate video duration against maximum allowed duration.
    
    MMAudio was trained on 8-second clips, so we enforce a 10-second
    maximum to ensure good results.
    
    Args:
        duration_seconds (float): Video duration in seconds
        max_duration (float): Maximum allowed duration (default: 10.0s)
    
    Returns:
        dict: {
            'valid': bool,
            'duration': float,
            'max_duration': float,
            'error': str or None
        }
    
    Example:
        >>> result = validate_video_duration(8.5)
        >>> if result['valid']:
        >>>     print("Duration OK")
        >>> else:
        >>>     print(f"Too long: {result['error']}")
    
    Note:
        Based on MMAudio training data (8s clips), we enforce strict
        10s maximum. NO fallback to full video - this is intentional
        to ensure quality results.
    """
    if duration_seconds <= 0:
        return {
            'valid': False,
            'duration': duration_seconds,
            'max_duration': max_duration,
            'error': f'Invalid duration: {duration_seconds:.2f}s (must be positive)'
        }
    
    if duration_seconds > max_duration:
        return {
            'valid': False,
            'duration': duration_seconds,
            'max_duration': max_duration,
            'error': f'Duration {duration_seconds:.2f}s exceeds maximum {max_duration:.2f}s'
        }
    
    return {
        'valid': True,
        'duration': duration_seconds,
        'max_duration': max_duration,
        'error': None
    }


def parse_arguments():
    """Parse command line arguments for CLI mode and Pro Tools integration"""
    parser = argparse.ArgumentParser(
        description="MMAudio Standalone API Client - Generate audio from video using AI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode
  python standalone_api_client.py
  
  # CLI mode (Pro Tools ready)
  python standalone_api_client.py --video /tmp/protools_clip.mov --prompt "drums"
  
  # Full CLI mode with all options
  python standalone_api_client.py \\
    --video /path/to/video.mp4 \\
    --prompt "ocean waves and seagulls" \\
    --negative-prompt "voices, music" \\
    --seed 42 \\
    --output /path/to/custom_output.flac \\
    --duration 10.0 \\
    --steps 30 \\
    --cfg-strength 5.0
        """
    )
    
    # Video input (required for CLI mode)
    parser.add_argument(
        '--video', '-v',
        type=str,
        help='Path to input video file (MP4, MOV, AVI, etc.)'
    )
    
    # Generation parameters
    parser.add_argument(
        '--prompt', '-p',
        type=str,
        default='',
        help='Text prompt describing desired audio (default: empty)'
    )
    
    parser.add_argument(
        '--negative-prompt', '-n',
        type=str,
        default='voices, music',
        help='Negative prompt to avoid certain sounds (default: "voices, music")'
    )
    
    parser.add_argument(
        '--seed', '-s',
        type=int,
        default=42,
        help='Random seed for reproducible results (default: 42)'
    )
    
    # Output options
    parser.add_argument(
        '--output', '-o',
        type=str,
        help='Output file path (default: auto-generated in ./standalone-API_outputs/)'
    )
    
    parser.add_argument(
        '--temp',
        action='store_true',
        help='Use system temp directory for output (useful when called from Pro Tools)'
    )
    
    parser.add_argument(
        '--import-to-protools',
        action='store_true',
        help='Automatically import generated audio to Pro Tools timeline via PTSL'
    )
    
    # Advanced parameters
    parser.add_argument(
        '--duration', '-d',
        type=float,
        help='Duration in seconds (default: auto-detect from video)'
    )
    
    parser.add_argument(
        '--model',
        type=str,
        default='large_44k_v2',
        help='Model variant to use (default: large_44k_v2)'
    )
    
    parser.add_argument(
        '--steps',
        type=int,
        default=25,
        help='Number of generation steps (default: 25)'
    )
    
    parser.add_argument(
        '--cfg-strength',
        type=float,
        default=4.5,
        help='CFG strength for prompt guidance (default: 4.5)'
    )
    
    # API options
    parser.add_argument(
        '--api-url',
        type=str,
        default=DEFAULT_API_URL,
        help=f'API server URL (default: {DEFAULT_API_URL})'
    )
    
    parser.add_argument(
        '--timeout',
        type=int,
        default=300,
        help='Request timeout in seconds (default: 300)'
    )
    
    # Utility options
    parser.add_argument(
        '--quiet', '-q',
        action='store_true',
        help='Minimal output (useful for Pro Tools integration)'
    )
    
    parser.add_argument(
        '--verbose', '-V',
        action='store_true',
        help='Verbose output with detailed progress'
    )
    
    # CLI Actions for C++ Plugin Integration
    parser.add_argument(
        '--action',
        type=str,
        choices=['generate', 'check_ffmpeg', 'get_video_selection', 'get_video_file', 'trim_video', 'validate_duration'],
        default='generate',
        help='Action to perform (default: generate)'
    )
    
    # Trimming parameters
    parser.add_argument(
        '--start-time',
        type=float,
        help='Start time in seconds (for trim_video action)'
    )
    
    parser.add_argument(
        '--end-time',
        type=float,
        help='End time in seconds (for trim_video action)'
    )
    
    parser.add_argument(
        '--max-duration',
        type=float,
        default=10.0,
        help='Maximum video duration in seconds (default: 10.0)'
    )
    
    return parser.parse_args()

def validate_video_file(video_path: str) -> Path:
    """Validate video file exists and has supported format"""
    video_path = Path(video_path)
    
    if not video_path.exists():
        raise FileNotFoundError(f"Video file not found: {video_path}")
    
    if not video_path.is_file():
        raise ValueError(f"Path is not a file: {video_path}")
    
    if video_path.suffix.lower() not in SUPPORTED_VIDEO_FORMATS:
        supported = ', '.join(sorted(SUPPORTED_VIDEO_FORMATS))
        raise ValueError(f"Unsupported video format '{video_path.suffix}'. Supported formats: {supported}")
    
    return video_path

def get_video_path_interactive() -> str:
    """Interactive video path selection"""
    print("\n📹 Video File Selection")
    print("=" * 40)
    
    while True:
        video_input = input(f"🎬 Video Path (Default: {Path(DEFAULT_VIDEO_PATH)}): ").strip()
        
        if not video_input:
            video_path = DEFAULT_VIDEO_PATH
        else:
            video_path = video_input
        
        try:
            validated_path = validate_video_file(video_path)
            file_size_mb = validated_path.stat().st_size / (1024 * 1024)
            print(f"✅ Video file: {validated_path.name} ({file_size_mb:.1f} MB)")
            return str(validated_path)
        except (FileNotFoundError, ValueError) as e:
            print(f"❌ {e}")
            print("   Please try again with a valid video file path.")
            continue

def get_user_inputs_interactive():
    """Interactive parameter input for generation settings"""
    print("\n🎵 Generation Parameters")
    print("=" * 40)

    prompt = input("🎵 Prompt (Default: ''): ").strip()
    negative_prompt = input("❌ Negative Prompt (Default: 'voices, music'): ").strip()
    if not negative_prompt:
        negative_prompt = "voices, music"
    
    while True:
        seed_input = input("🎲 Seed (Default: 42): ").strip()
        if not seed_input:
            seed = 42
            break
        try:
            seed = int(seed_input)
            break
        except ValueError:
            print("   ⚠️  Please enter a valid number.")
    

    print(f"\n✅ Parameters set:")
    print(f"   Prompt: '{prompt}'")
    print(f"   Negative Prompt: '{negative_prompt}'")
    print(f"   Seed: {seed}")
    
    return prompt, negative_prompt, seed

def check_api_health(api_url: str, quiet: bool = False) -> bool:
    """Check if the API server is reachable"""
    try:
        response = requests.get(f"{api_url}/", timeout=10)
        response.raise_for_status()
        return True
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"❌ API not reachable: {e}")
        return False

def get_available_models(api_url: str, quiet: bool = False) -> Optional[dict]:
    """Get available models from the API"""
    try:
        response = requests.get(f"{api_url}/models", timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"⚠️  Could not fetch models: {e}")
        return None

def create_output_directory(use_temp=False):
    """Create output directory
    
    Args:
        use_temp: If True, use system temp directory (for Pro Tools). 
                  If False, use ./standalone-API_outputs (default)
    """
    if use_temp:
        import tempfile
        temp_base = Path(tempfile.gettempdir()) / "pt_v2a_outputs"
        temp_base.mkdir(exist_ok=True, parents=True)
        return temp_base
    else:
        output_dir = Path("./standalone-API_outputs")
        output_dir.mkdir(exist_ok=True)
        return output_dir

def generate_audio(
    api_url: str,
    video_path: str,
    prompt: str,
    negative_prompt: str,
    seed: int,
    model_name: str = "large_44k_v2",
    duration: Optional[float] = None,
    num_steps: int = 25,
    cfg_strength: float = 4.5,
    output_path: Optional[str] = None,
    use_temp: bool = False,
    timeout: int = 300,
    quiet: bool = False,
    verbose: bool = False
) -> Optional[str]:
    """Generate audio using the mmaudio Standalone API"""
    
    if not os.path.exists(video_path):
        if not quiet:
            print(f"❌ Video file not found: {video_path}")
        return None
    
    if not quiet:
        print(f"\n🚀 Sending request to API...")
        print(f"   Video: {Path(video_path).name}")
        print(f"   Model: {model_name}")
        print(f"   Duration: {'auto-detect' if duration is None else f'{duration}s'}")
        if verbose:
            print(f"   Prompt: '{prompt}'")
            print(f"   Negative Prompt: '{negative_prompt}'")
            print(f"   Seed: {seed}")
            print(f"   Steps: {num_steps}")
            print(f"   CFG Strength: {cfg_strength}")
    
    # Prepare request data
    data = {
        "prompt": prompt,
        "negative_prompt": negative_prompt,
        "seed": seed,
        "model_name": model_name,
        "num_steps": num_steps,
        "cfg_strength": cfg_strength
    }
    
    if duration is not None:
        data["duration"] = duration
    
    start_time = time.time()
    
    try:
        with open(video_path, 'rb') as video_file:
            files = {"video": (Path(video_path).name, video_file, "video/mp4")}
            
            if not quiet:
                print("⏳ Processing... (this may take a minute)")
            
            response = requests.post(
                f"{api_url}/generate",
                files=files,
                data=data,
                timeout=timeout
            )
            response.raise_for_status()
        
        total_time = time.time() - start_time
        
        # Get metadata from headers
        generation_time = response.headers.get('X-Generation-Time', 'unknown')
        actual_duration = response.headers.get('X-Duration', 'unknown')
        used_seed = response.headers.get('X-Seed', seed)
        
        if not quiet:
            print(f"\n✅ Audio generated successfully!")
            print(f"   Total time: {total_time:.2f}s")
            print(f"   Generation time: {generation_time}s")
            print(f"   Video duration: {actual_duration}s")
            print(f"   Seed used: {used_seed}")
        
        # Determine output path
        if output_path:
            final_output_path = Path(output_path)
            # Ensure output directory exists
            final_output_path.parent.mkdir(parents=True, exist_ok=True)
        else:
            # Auto-generate output filename
            output_dir = create_output_directory(use_temp=use_temp)
            timestamp = int(time.time())
            # MMAudio API returns FLAC - saved as FLAC, converted to WAV by PTSL client if needed
            output_filename = f"generated_audio_{timestamp}_{seed}.flac"
            final_output_path = output_dir / output_filename

        # Save audio file
        with open(final_output_path, 'wb') as f:
            f.write(response.content)
        
        # Always return absolute path (important for Pro Tools plugin integration)
        absolute_path = final_output_path.absolute()
        
        if not quiet:
            print(f"📁 Audio saved as: {absolute_path}")
            print(f"📊 File size: {len(response.content) / 1024:.1f} KB")
        
        return str(absolute_path)
        
    except requests.exceptions.Timeout:
        if not quiet:
            print("❌ Request timed out. The server might be overloaded.")
        return None
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"❌ Request failed: {e}")
            if hasattr(e, 'response') and e.response:
                print(f"   Server response: {e.response.text}")
        return None
    except Exception as e:
        if not quiet:
            print(f"❌ Unexpected error: {e}")
        return None

def main():
    # Parse command line arguments  
    args = parse_arguments()
    
    # Determine operation mode
    quiet = args.quiet
    verbose = args.verbose
    
    # =============================================================================
    # Handle CLI Actions
    # =============================================================================
    
    if args.action == 'check_ffmpeg':
        """Check FFmpeg availability"""
        result = check_ffmpeg_available()
        print(json.dumps(result))
        return 0 if result['available'] else 1
    
    elif args.action == 'get_video_selection':
        """Get timeline selection from Pro Tools"""
        result = get_video_timeline_selection()
        print(json.dumps(result))
        return 0 if result['success'] else 1
    
    elif args.action == 'get_video_file':
        """Get video file path from Pro Tools"""
        result = get_video_file_from_protools()
        print(json.dumps(result))
        return 0 if result['success'] else 1
    
    elif args.action == 'trim_video':
        """Trim video segment"""
        if not args.video:
            print(json.dumps({
                'success': False,
                'error': '--video argument required for trim_video action'
            }))
            return 1
        
        if args.start_time is None or args.end_time is None:
            print(json.dumps({
                'success': False,
                'error': '--start-time and --end-time required for trim_video action'
            }))
            return 1
        
        result = trim_video_segment(
            video_path=args.video,
            start_seconds=args.start_time,
            end_seconds=args.end_time,
            output_path=args.output
        )
        print(json.dumps(result))
        return 0 if result['success'] else 1
    
    elif args.action == 'validate_duration':
        """Validate video duration"""
        if args.duration is None:
            print(json.dumps({
                'valid': False,
                'error': '--duration argument required for validate_duration action'
            }))
            return 1
        
        result = validate_video_duration(
            duration_seconds=args.duration,
            max_duration=args.max_duration
        )
        print(json.dumps(result))
        return 0 if result['valid'] else 1
    
    # =============================================================================
    # Standard Generation Mode (action == 'generate')
    # =============================================================================
    
    is_cli_mode = args.video is not None
    
    if not quiet:
        print("🌐 MMAudio Standalone API Client")
        print("=" * 50)
        if is_cli_mode:
            print("📋 CLI Mode (Pro Tools Ready)")
        else:
            print("🎮 Interactive Mode")
        print()
    
    try:
        # === CLI MODE ===
        if is_cli_mode:
            # Validate video file
            try:
                video_path_obj = validate_video_file(args.video)
                video_path = str(video_path_obj)
            except (FileNotFoundError, ValueError) as e:
                if not quiet:
                    print(f"❌ Video validation failed: {e}")
                return 1
            
            # Use CLI parameters
            prompt = args.prompt
            negative_prompt = args.negative_prompt  
            seed = args.seed
            
            if verbose and not quiet:
                file_size_mb = video_path_obj.stat().st_size / (1024 * 1024)
                print(f"📹 Video: {video_path_obj.name} ({file_size_mb:.1f} MB)")
        
        # === INTERACTIVE MODE ===
        else:
            # Get video path interactively
            video_path = get_video_path_interactive()
            
            # Get generation parameters interactively
            prompt, negative_prompt, seed = get_user_inputs_interactive()
        
        # === COMMON PROCESSING ===
        
        # Check API health
        if not quiet:
            print(f"\n🔗 Checking API connection to {args.api_url}...")
        
        if not check_api_health(args.api_url, quiet=quiet):
            if not quiet:
                print("\n💡 Make sure the API server is running on server:")
                print("   docker restart mmaudio-api")
                print("   #  python main.py")
            return 1
        
        if not quiet:
            print("✅ API is online!")
        
        # Get available models (optional info)
        if verbose and not quiet:
            models_info = get_available_models(args.api_url, quiet=quiet)
            if models_info:
                loaded_models = models_info.get("loaded_models", [])
                if loaded_models:
                    print(f"📦 Loaded models: {', '.join(loaded_models)}")
                else:
                    print("📦 No models loaded yet (will load on first request)")
        
        # Generate audio
        output_file = generate_audio(
            api_url=args.api_url,
            video_path=video_path,
            prompt=prompt,
            negative_prompt=negative_prompt,
            seed=seed,
            model_name=args.model,
            duration=args.duration,
            num_steps=args.steps,
            cfg_strength=args.cfg_strength,
            output_path=args.output,
            use_temp=args.temp,
            timeout=args.timeout,
            quiet=quiet,
            verbose=verbose
        )
        
        if output_file:
            # Import to Pro Tools if requested
            if args.import_to_protools:
                if not quiet:
                    print(f"\n📥 Importing to Pro Tools timeline...")
                
                try:
                    # Import PTSL client (now using py-ptsl based implementation)
                    from ptsl_integration import import_audio_to_pro_tools
                    
                    success = import_audio_to_pro_tools(
                        audio_path=output_file,
                        location="SessionStart"
                    )
                    
                    if success:
                        if not quiet:
                            print(f"✅ Audio imported to Pro Tools timeline!")
                    else:
                        if not quiet:
                            print(f"⚠️  PTSL import failed - audio file saved but not imported")
                except Exception as e:
                    if not quiet:
                        print(f"⚠️  PTSL import failed: {e}")
                        print(f"   Audio file saved at: {output_file}")
            
            if quiet:
                # Pro Tools integration: just print the output path
                print(output_file)
            else:
                print(f"\n🎉 Success! Audio generated and saved.")
                print(f"   Output: {output_file}")
            return 0
        else:
            if not quiet:
                print(f"\n❌ Audio generation failed.")
            return 1
            
    except KeyboardInterrupt:
        if not quiet:
            print(f"\n\n⚠️  Operation cancelled by user.")
        return 1
    except Exception as e:
        # Print errors to STDOUT (not stderr) so Pro Tools plugin can see them
        error_msg = f"ERROR: {e}"
        
        # In quiet mode, print to stdout so plugin can read it
        print(error_msg)
        
        return 1

if __name__ == "__main__":
    exit(main())