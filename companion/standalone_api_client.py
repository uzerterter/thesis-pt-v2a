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

Note:
    This file has been refactored. Core functionality moved to:
    - api/client.py: API communication
    - api/config.py: Configuration constants
    - video/ffmpeg.py: FFmpeg operations
    - video/validation.py: Video validation
    - ptsl_integration/timeline.py: Timeline operations
    - ptsl_integration/video.py: Video file detection
"""

import argparse
import sys
import json
from pathlib import Path
from typing import Optional

# Import from refactored modules
from api import (
    generate_audio,
    check_api_health,
    get_available_models,
    DEFAULT_API_URL,
    SUPPORTED_VIDEO_FORMATS,
)
from api.config import (
    DEFAULT_NEGATIVE_PROMPT,
    DEFAULT_SEED,
    DEFAULT_NUM_STEPS,
    DEFAULT_CFG_STRENGTH,
    DEFAULT_MODEL,
    DEFAULT_OUTPUT_FORMAT,
    DEFAULT_TIMEOUT,
)
from video import (
    check_ffmpeg_available,
    trim_video_segment,
    validate_video_duration,
    validate_video_file,
)
from ptsl_integration import (
    get_video_timeline_selection,
    timecode_to_seconds,
    get_video_file_from_protools,
    import_audio_to_pro_tools,
)

# Legacy configuration (for backwards compatibility)
DEFAULT_VIDEO_PATH = r"C:\Users\Ludenbold\Desktop\Master_Thesis\Implementation\model-tests\data\MMAudio_examples\noSound\sora_galloping.mp4"

# =============================================================================
# CLI Functions (parse arguments, interactive mode, main entry point)
# =============================================================================

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
        default=DEFAULT_NEGATIVE_PROMPT,
        help=f'Negative prompt to avoid certain sounds (default: "{DEFAULT_NEGATIVE_PROMPT}")'
    )
    
    parser.add_argument(
        '--seed', '-s',
        type=int,
        default=DEFAULT_SEED,
        help=f'Random seed for reproducible results (default: {DEFAULT_SEED})'
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
        default=DEFAULT_MODEL,
        help=f'Model variant to use (default: {DEFAULT_MODEL})'
    )
    
    parser.add_argument(
        '--steps',
        type=int,
        default=DEFAULT_NUM_STEPS,
        help=f'Number of generation steps (default: {DEFAULT_NUM_STEPS})'
    )
    
    parser.add_argument(
        '--cfg-strength',
        type=float,
        default=DEFAULT_CFG_STRENGTH,
        help=f'CFG strength for prompt guidance (default: {DEFAULT_CFG_STRENGTH})'
    )
    
    parser.add_argument(
        '--output-format',
        type=str,
        choices=['flac', 'wav'],
        default=DEFAULT_OUTPUT_FORMAT,
        help=f'Output audio format: "flac" (smaller) or "wav" (Pro Tools compatible, default: {DEFAULT_OUTPUT_FORMAT})'
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
        default=DEFAULT_TIMEOUT,
        help=f'Request timeout in seconds (default: {DEFAULT_TIMEOUT})'
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
        choices=['generate', 'check_ffmpeg', 'get_video_selection', 'get_video_file', 'get_video_info', 'trim_video', 'validate_duration', 'get_duration', 'import_audio'],
        default='generate',
        help='Action to perform (default: generate)'
    )
    
    # Audio import parameter
    parser.add_argument(
        '--audio-path',
        type=str,
        help='Path to audio file (for import_audio action)'
    )
    
    # Audio import timecode position
    parser.add_argument(
        '--timecode',
        type=str,
        help='Timecode position for import_audio action (e.g., "00:00:07:00")'
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


def get_video_path_interactive() -> str:
    """Interactive video path selection"""
    print("\n📹 Video File Selection")
    print("=" * 40)
    
    while True:
        video_input = input(f"🎬 Video Path (Default: {Path(DEFAULT_VIDEO_PATH).name}): ").strip()
        
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
    negative_prompt = input(f"❌ Negative Prompt (Default: '{DEFAULT_NEGATIVE_PROMPT}'): ").strip()
    if not negative_prompt:
        negative_prompt = DEFAULT_NEGATIVE_PROMPT
    
    while True:
        seed_input = input(f"🎲 Seed (Default: {DEFAULT_SEED}): ").strip()
        if not seed_input:
            seed = DEFAULT_SEED
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


def main():
    """Main entry point for the standalone API client"""
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
        print(f"=== DEBUG: get_video_selection action START ===", file=sys.stderr)
        sys.stderr.flush()
        
        result = get_video_timeline_selection()
        
        print(f"=== DEBUG: PTSL call completed, result={result['success']} ===", file=sys.stderr)
        sys.stderr.flush()
        
        # Print JSON to stdout (will be in log file)
        print(json.dumps(result))
        sys.stdout.flush()  # Force flush to ensure JSON is written
        
        print(f"=== DEBUG: JSON output sent, exiting... ===", file=sys.stderr)
        sys.stderr.flush()
        
        # Force immediate exit to avoid hanging
        sys.exit(0 if result['success'] else 1)
    
    elif args.action == 'get_video_info':
        """Get timeline selection AND video file in one PTSL call (faster!)"""
        print(f"=== DEBUG: get_video_info action START ===", file=sys.stderr)
        sys.stderr.flush()
        
        # Get timeline selection
        selection = get_video_timeline_selection()
        
        print(f"=== DEBUG: Timeline selection: {selection['success']} ===", file=sys.stderr)
        sys.stderr.flush()
        
        if not selection['success']:
            # Return error from timeline selection
            print(json.dumps(selection))
            sys.stdout.flush()
            sys.exit(1)
        
        # Get video file path, passing timeline selection for validation
        video_file = get_video_file_from_protools(
            timeline_in_seconds=selection.get('in_seconds'),
            timeline_out_seconds=selection.get('out_seconds')
        )
        
        print(f"=== DEBUG: Video file lookup: {video_file['success']} ===", file=sys.stderr)
        sys.stderr.flush()
        
        # Combine results into single response
        combined_result = {
            'success': selection['success'] and video_file['success'],
            # Timeline selection fields
            'in_time': selection.get('in_time'),
            'out_time': selection.get('out_time'),
            'in_seconds': selection.get('in_seconds'),
            'out_seconds': selection.get('out_seconds'),
            'duration_seconds': selection.get('duration_seconds'),
            'fps': selection.get('fps'),
            # Video file fields
            'video_path': video_file.get('video_path'),
            'video_files': video_file.get('video_files', []),
            'video_count': video_file.get('video_count', 0),
            # Error from whichever failed (if any)
            'error': video_file.get('error') if not video_file['success'] else selection.get('error')
        }
        
        print(json.dumps(combined_result))
        sys.stdout.flush()
        
        print(f"=== DEBUG: Combined result sent, exiting... ===", file=sys.stderr)
        sys.stderr.flush()
        
        # Force immediate exit
        sys.exit(0 if combined_result['success'] else 1)
    
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
    
    elif args.action == 'get_duration':
        """Get video file duration using FFprobe"""
        print(f"=== DEBUG: get_duration action ===", file=sys.stderr)
        print(f"Video path: {args.video}", file=sys.stderr)
        
        if not args.video:
            error_response = {
                'success': False,
                'error': '--video argument required for get_duration action'
            }
            print(f"ERROR: {error_response['error']}", file=sys.stderr)
            print(json.dumps(error_response))
            return 1
        
        from video import get_video_duration
        print(f"Calling get_video_duration()...", file=sys.stderr)
        result = get_video_duration(args.video)
        print(f"Result: {result}", file=sys.stderr)
        print(json.dumps(result))
        return 0 if result['success'] else 1
    
    elif args.action == 'import_audio':
        """Import audio file to Pro Tools timeline"""
        print(f"=== DEBUG: import_audio action START ===", file=sys.stderr)
        sys.stderr.flush()
        
        if not args.audio_path:
            error_result = {
                'success': False,
                'error': '--audio-path argument required for import_audio action'
            }
            print(f"ERROR: {error_result['error']}", file=sys.stderr)
            print(json.dumps(error_result))
            sys.exit(1)
        
        print(f"Audio path: {args.audio_path}", file=sys.stderr)
        
        # Get timecode if provided
        timecode = args.timecode if hasattr(args, 'timecode') and args.timecode else None
        if timecode:
            print(f"Import timecode: {timecode}", file=sys.stderr)
        else:
            print(f"Import timecode: Not specified (will use session start)", file=sys.stderr)
        
        sys.stderr.flush()
        
        # Import to Pro Tools timeline
        try:
            success = import_audio_to_pro_tools(
                audio_path=args.audio_path,
                timecode=timecode  # Pass timecode to import function
            )
            
            result = {
                'success': success,
                'audio_path': args.audio_path
            }
            
            if not success:
                result['error'] = 'PTSL import returned False'
            
            print(f"=== DEBUG: Import result: {success} ===", file=sys.stderr)
            sys.stderr.flush()
            
            print(json.dumps(result))
            sys.stdout.flush()
            sys.exit(0 if success else 1)
            
        except Exception as e:
            import traceback
            error_msg = str(e)
            traceback_str = traceback.format_exc()
            
            print(f"ERROR: Import exception: {error_msg}", file=sys.stderr)
            print(traceback_str, file=sys.stderr)
            sys.stderr.flush()
            
            error_result = {
                'success': False,
                'error': error_msg,
                'traceback': traceback_str
            }
            print(json.dumps(error_result))
            sys.stdout.flush()
            sys.exit(1)
    
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
                error_msg = f"Video validation failed: {e}"
                if not quiet:
                    print(f"❌ {error_msg}")
                print(f"ERROR: {error_msg}", file=sys.stderr)
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
        
        print(f"=== DEBUG: Checking API health at {args.api_url} ===", file=sys.stderr)
        
        if not check_api_health(args.api_url, quiet=quiet):
            error_msg = f"API not available at {args.api_url}"
            print(f"ERROR: {error_msg}", file=sys.stderr)
            if not quiet:
                print("\n💡 Make sure the API server is running on server:")
                print("   docker restart mmaudio-api")
                print("   # or: python main.py")
            return 1
        
        print(f"=== DEBUG: API health check passed ===", file=sys.stderr)
        
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
            output_format=args.output_format,
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
                
                print(f"=== DEBUG: Starting PTSL import ===", file=sys.stderr)
                print(f"Audio file: {output_file}", file=sys.stderr)
                
                try:
                    success = import_audio_to_pro_tools(
                        audio_path=output_file,
                        location="SessionStart"
                    )
                    
                    if success:
                        print(f"=== DEBUG: PTSL import SUCCESS ===", file=sys.stderr)
                        if not quiet:
                            print(f"✅ Audio imported to Pro Tools timeline!")
                    else:
                        print(f"WARNING: PTSL import returned False", file=sys.stderr)
                        if not quiet:
                            print(f"⚠️  PTSL import failed - audio file saved but not imported")
                except Exception as e:
                    print(f"ERROR: PTSL import exception: {e}", file=sys.stderr)
                    import traceback
                    traceback.print_exc(file=sys.stderr)
                    if not quiet:
                        print(f"⚠️  PTSL import failed: {e}")
                        print(f"   Audio file saved at: {output_file}")
            
            if quiet:
                # Pro Tools integration: just print the output path
                print(output_file)
            else:
                print(f"\n🎉 Success! Audio generated and saved.")
                print(f"   Output: {output_file}")
            
            print(f"=== DEBUG: Generation completed successfully ===", file=sys.stderr)
            print(f"Output file: {output_file}", file=sys.stderr)
            return 0
        else:
            error_msg = "Audio generation failed - no output file returned"
            print(f"ERROR: {error_msg}", file=sys.stderr)
            if not quiet:
                print(f"\n❌ Audio generation failed.")
            return 1
            
    except KeyboardInterrupt:
        if not quiet:
            print(f"\n\n⚠️  Operation cancelled by user.", file=sys.stderr)
        return 1
    except Exception as e:
        # Print errors to BOTH stdout and stderr for maximum visibility
        # stdout: For C++ plugin to parse
        # stderr: For python_stderr.log debugging
        import traceback
        error_msg = f"ERROR: {e}"
        print(error_msg)  # stdout for C++ parsing
        print(error_msg, file=sys.stderr)  # stderr for logging
        print("\nFull traceback:", file=sys.stderr)
        traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == "__main__":
    exit(main())
