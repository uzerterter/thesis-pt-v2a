"""
MMAudio API client for video-to-audio generation

Provides HTTP client functions for interacting with the MMAudio standalone API.
"""

import os
import time
import tempfile
from pathlib import Path
from typing import Optional

import requests

from .config import (
    DEFAULT_API_URL,
    DEFAULT_MODEL,
    DEFAULT_NUM_STEPS,
    DEFAULT_CFG_STRENGTH,
    DEFAULT_OUTPUT_FORMAT,
    DEFAULT_TIMEOUT,
)


def check_api_health(api_url: str = DEFAULT_API_URL, quiet: bool = False) -> bool:
    """
    Check if the API server is reachable.
    
    Args:
        api_url (str): API server URL
        quiet (bool): Suppress error messages
    
    Returns:
        bool: True if API is reachable, False otherwise
    
    Example:
        >>> if check_api_health():
        >>>     print("API is online!")
    """
    try:
        response = requests.get(f"{api_url}/", timeout=10)
        response.raise_for_status()
        return True
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"❌ API not reachable: {e}")
        return False


def get_available_models(api_url: str = DEFAULT_API_URL, quiet: bool = False) -> Optional[dict]:
    """
    Get available models from the API.
    
    Args:
        api_url (str): API server URL
        quiet (bool): Suppress error messages
    
    Returns:
        dict or None: Model information if successful, None otherwise
    
    Example:
        >>> models = get_available_models()
        >>> if models:
        >>>     print(f"Available: {models['available_models']}")
        >>>     print(f"Loaded: {models['loaded_models']}")
    """
    try:
        response = requests.get(f"{api_url}/models", timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"❌ Failed to get models: {e}")
        return None


def generate_audio(
    api_url: str,
    video_path: str,
    prompt: str,
    negative_prompt: str,
    seed: int,
    model_name: str = DEFAULT_MODEL,
    duration: Optional[float] = None,
    num_steps: int = DEFAULT_NUM_STEPS,
    cfg_strength: float = DEFAULT_CFG_STRENGTH,
    output_format: str = DEFAULT_OUTPUT_FORMAT,
    output_path: Optional[str] = None,
    use_temp: bool = False,
    timeout: int = DEFAULT_TIMEOUT,
    quiet: bool = False,
    verbose: bool = False,
    full_precision: bool = False
) -> Optional[str]:
    """
    Generate audio from video using the MMAudio Standalone API.
    
    Args:
        api_url (str): API server URL
        video_path (str): Path to input video file
        prompt (str): Text prompt describing desired audio
        negative_prompt (str): Negative prompt to avoid certain sounds
        seed (int): Random seed for reproducibility
        model_name (str): Model variant to use (default: "large_44k_v2")
        duration (float, optional): Duration in seconds (default: auto-detect)
        num_steps (int): Number of generation steps (default: 25)
        cfg_strength (float): CFG strength for prompt guidance (default: 4.5)
        output_format (str): "flac" or "wav" (default: "wav" for Pro Tools)
        output_path (str, optional): Custom output path
        use_temp (bool): Use system temp directory for output
        timeout (int): Request timeout in seconds (default: 300)
        quiet (bool): Minimal output
        verbose (bool): Detailed output
    
    Returns:
        str or None: Path to generated audio file if successful, None otherwise
    
    Example:
        >>> audio_path = generate_audio(
        >>>     api_url="http://localhost:8000",
        >>>     video_path="video.mp4",
        >>>     prompt="ocean waves",
        >>>     negative_prompt="voices, music",
        >>>     seed=42
        >>> )
        >>> if audio_path:
        >>>     print(f"Generated: {audio_path}")
    """
    
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
            print(f"   Output Format: {output_format}")
    
    # Prepare request data
    data = {
        "prompt": prompt,
        "negative_prompt": negative_prompt,
        "seed": seed,
        "model_name": model_name,
        "num_steps": num_steps,
        "cfg_strength": cfg_strength,
        "output_format": output_format,
        "full_precision": full_precision
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
        output_format_used = response.headers.get('X-Output-Format', output_format)
        
        if not quiet:
            print(f"\n✅ Audio generated successfully!")
            print(f"   Total time: {total_time:.2f}s")
            print(f"   Generation time: {generation_time}s")
            print(f"   Video duration: {actual_duration}s")
            print(f"   Seed used: {used_seed}")
            print(f"   Format: {output_format_used}")
        
        # Determine output path
        if output_path:
            final_output_path = Path(output_path)
            # Ensure output directory exists
            final_output_path.parent.mkdir(parents=True, exist_ok=True)
        else:
            # Auto-generate output filename with correct extension
            if use_temp:
                output_dir = Path(tempfile.gettempdir()) / "pt_v2a_outputs"
                output_dir.mkdir(exist_ok=True, parents=True)
            else:
                output_dir = Path("./standalone-API_outputs")
                output_dir.mkdir(exist_ok=True)
            
            timestamp = int(time.time())
            file_ext = output_format.lower()
            output_filename = f"generated_audio_{timestamp}_{seed}.{file_ext}"
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
