#!/usr/bin/env python3
"""
Standalone MMAudio API Client

A client script that uses the standalone MMAudio API server for fast video-to-audio generation.
This replaces the ComfyUI-based approach with a direct API call for better performance.

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
from pathlib import Path
from typing import Optional

# Configuration
DEFAULT_API_URL = "http://localhost:8000"
#DEFAULT_VIDEO_PATH = "/mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4"
DEFAULT_VIDEO_PATH = r"C:\Users\Ludenbold\Desktop\Master_Thesis\Implementation\model-tests\data\MMAudio_examples\noSound\sora_galloping.mp4"

# Supported video formats for Pro Tools integration
SUPPORTED_VIDEO_FORMATS = {'.mp4', '.mov', '.avi', '.mkv', '.webm', '.flv', '.wmv', '.m4v'}

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
    is_cli_mode = args.video is not None
    quiet = args.quiet
    verbose = args.verbose
    
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
                print("\n💡 Make sure the API server is running:")
                print("   docker restart mmaudio-api")
                print("   # or manually: python main.py")
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