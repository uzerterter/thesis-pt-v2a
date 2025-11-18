"""
HunyuanVideo-Foley API client for video-to-audio generation

Provides HTTP client functions for interacting with the HunyuanVideo-Foley standalone API.
Analogous to api/client.py but adapted for HunyuanVideo-Foley specific parameters.
"""

import os
import time
import tempfile
from pathlib import Path
from typing import Optional

import requests


def check_api_health(api_url: str = "http://localhost:8001", quiet: bool = False) -> bool:
    """
    Check if the HunyuanVideo-Foley API server is reachable.
    
    Args:
        api_url (str): API server URL (default: http://localhost:8001)
        quiet (bool): Suppress error messages
    
    Returns:
        bool: True if API is reachable, False otherwise
    
    Example:
        >>> if check_api_health():
        >>>     print("HunyuanVideo-Foley API is online!")
    """
    try:
        response = requests.get(f"{api_url}/", timeout=10)
        response.raise_for_status()
        return True
    except requests.exceptions.RequestException as e:
        if not quiet:
            print(f"❌ API not reachable: {e}")
        return False


def get_available_models(api_url: str = "http://localhost:8001", quiet: bool = False) -> Optional[dict]:
    """
    Get available models from the HunyuanVideo-Foley API.
    
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
    model_size: str = "xxl",  # "xl" or "xxl" (HunyuanVideo-Foley specific)
    duration: Optional[float] = None,
    num_steps: int = 50,
    cfg_strength: float = 4.5,
    output_format: str = "wav",
    full_precision: bool = False,  # Use float32 instead of float16
    output_path: Optional[str] = None,
    use_temp: bool = False,
    timeout: int = 300,
    quiet: bool = False,
    verbose: bool = False
) -> Optional[str]:
    """
    Generate audio from video using the HunyuanVideo-Foley Standalone API.
    
    Args:
        api_url (str): API server URL (default: http://localhost:8001)
        video_path (str): Path to input video file
        prompt (str): Text prompt describing desired Foley audio
        negative_prompt (str): Negative prompt to avoid certain sounds
        seed (int): Random seed for reproducibility
        model_size (str): "xl" (faster, 8-12GB VRAM) or "xxl" (higher quality, 16-20GB VRAM)
        duration (float, optional): Duration in seconds (default: auto-detect)
        num_steps (int): Number of generation steps (default: 50)
        cfg_strength (float): CFG strength for prompt guidance (default: 4.5)
        output_format (str): "wav" or "flac" (default: "wav" for Pro Tools)
        full_precision (bool): Use float32 instead of bfloat16 (default: False)
                              NOTE: Effect is minimal - model is hardcoded to bfloat16→float32.
                              Only affects feature/latent precision. ~10-20% more VRAM.
        output_path (str, optional): Custom output path
        use_temp (bool): Use system temp directory for output
        timeout (int): Request timeout in seconds (default: 300)
        quiet (bool): Minimal output
        verbose (bool): Detailed output
    
    Returns:
        str or None: Path to generated audio file if successful, None otherwise
    
    Example:
        >>> audio_path = generate_audio(
        >>>     api_url="http://localhost:8001",
        >>>     video_path="video.mp4",
        >>>     prompt="footsteps on gravel",
        >>>     negative_prompt="voices, music",
        >>>     seed=42,
        >>>     model_size="xxl"
        >>> )
        >>> if audio_path:
        >>>     print(f"Generated 48kHz audio: {audio_path}")
    """
    
    if not os.path.exists(video_path):
        if not quiet:
            print(f"❌ Video file not found: {video_path}")
        return None
    
    if not quiet:
        print(f"\n🚀 Sending request to HunyuanVideo-Foley API...")
        print(f"   Video: {Path(video_path).name}")
        print(f"   Model Size: {model_size.upper()}")
        print(f"   Duration: {'auto-detect' if duration is None else f'{duration}s'}")
        print(f"   Precision: {'float32 (full)' if full_precision else 'bfloat16 (default)'}")
        if verbose:
            print(f"   Prompt: '{prompt}'")
            print(f"   Negative Prompt: '{negative_prompt}'")
            print(f"   Seed: {seed}")
            print(f"   Steps: {num_steps}")
            print(f"   CFG Strength: {cfg_strength}")
            print(f"   Output Format: {output_format}")
    
    # Prepare request data (HunyuanVideo-Foley specific parameters)
    data = {
        "prompt": prompt,
        "negative_prompt": negative_prompt,
        "seed": seed,
        "model_size": model_size,  # HunyuanVideo-Foley uses model_size instead of model_name
        "num_steps": num_steps,
        "cfg_strength": cfg_strength,
        "output_format": output_format,
        "full_precision": str(full_precision).lower()  # Convert bool to string for form data
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
        sample_rate = response.headers.get('X-Sample-Rate', '48000')
        
        if not quiet:
            print(f"\n✅ Audio generated successfully!")
            print(f"   Total time: {total_time:.2f}s")
            print(f"   Generation time: {generation_time}s")
            print(f"   Video duration: {actual_duration}s")
            print(f"   Seed used: {used_seed}")
            print(f"   Format: {output_format_used}")
            print(f"   Sample Rate: {sample_rate} Hz (professional Foley standard)")
        
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
                output_dir = Path("./hunyuanvideo-foley-outputs")
                output_dir.mkdir(exist_ok=True)
            
            timestamp = int(time.time())
            file_ext = output_format.lower()
            output_filename = f"hyvf_{model_size}_{timestamp}_{seed}.{file_ext}"
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
