#!/usr/bin/env python3
"""
BBC Sound Search API Client for Pro Tools Plugin

This script is called by the AAX plugin (C++) to perform semantic sound search.
It uses the sound_search_client module to interact with the Sound Search API.

Actions:
    search - Search for sounds using video and/or text
    download - Download a specific sound by ID
    cleanup - Clean up temporary files for a session

Examples:
    # Text search
    python sound_search_api_client.py --action search --text "footsteps" --limit 5
    
    # Video search
    python sound_search_api_client.py --action search --video /path/to/video.mp4 --limit 5
    
    # Hybrid search (video + text)
    python sound_search_api_client.py --action search --video /path/to/video.mp4 --text "door closing" --limit 5
    
    # Download sound
    python sound_search_api_client.py --action download --sound-id 5362 --output /tmp/sound.wav
    
    # Cleanup session
    python sound_search_api_client.py --action cleanup --session-id abc123
"""

import argparse
import json
import sys
import os
import tempfile
import datetime
from pathlib import Path

# Add api module to path
sys.path.insert(0, str(Path(__file__).resolve().parent))

# Global log file path
LOG_FILE = os.path.join(tempfile.gettempdir(), "sound_search_debug.log")

def log_debug(msg):
    """Write debug message to log file ONLY (not to stderr to avoid mixing with JSON output)"""
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            f.write(f"[{timestamp}] {msg}\n")
            f.flush()
    except Exception:
        pass

from api.sound_search_client import (
    search_sounds,
    download_sound,
    search_and_download,
    cleanup_session,
    check_api_health,
)


def action_health_check(args):
    """Check if Sound Search API is available"""
    log_debug("Action: health_check")
    print("Checking Sound Search API health...")
    
    if check_api_health(quiet=False):
        log_debug("Sound Search API is available")
        print(json.dumps({
            "status": "success",
            "message": "Sound Search API is available"
        }))
        return 0
    else:
        log_debug("ERROR: Sound Search API is not available")
        print(json.dumps({
            "status": "error",
            "message": "Sound Search API is not available"
        }), file=sys.stderr)
        return 1


def action_search(args):
    """Search for sounds using video and/or text"""
    log_debug("Action: search")
    log_debug(f"Video: {args.video}, Text: {args.text}, Limit: {args.limit}")
    
    if not args.video and not args.text:
        log_debug("ERROR: Must provide either --video or --text")
        print(json.dumps({
            "status": "error",
            "message": "Must provide either --video or --text"
        }), file=sys.stderr)
        return 1
    
    # Adjust text_weight based on input type
    effective_text_weight = args.text_weight
    if not args.video and args.text:
        # Text-only search: 100% text weight
        effective_text_weight = 1.0
        log_debug("Text-only search - using text_weight=1.0 (100% text)")
    elif args.video and not args.text:
        # Video-only search: 0% text weight (100% video)
        effective_text_weight = 0.0
        log_debug("Video-only search - using text_weight=0.0 (100% video)")
    else:
        # Hybrid search: use configured weight
        log_debug(f"Hybrid search (video + text) - using text_weight={effective_text_weight}")
    
    # Perform search and download
    log_debug(f"Starting search with text_weight={effective_text_weight}, num_frames={args.num_frames}")
    
    try:
        log_debug("Calling search_and_download()...")
        log_debug(f"  video_path: {args.video}")
        log_debug(f"  text_query: {args.text}")
        log_debug(f"  limit: {args.limit}")
        
        results = search_and_download(
            video_path=args.video,
            text_query=args.text,
            limit=args.limit,
            text_weight=effective_text_weight,
            num_frames=args.num_frames,
            session_id=args.session_id,
            quiet=args.quiet,  # Use CLI argument (True when called from plugin)
            verbose=args.verbose,  # Use CLI argument (False when called from plugin)
        )
        log_debug(f"search_and_download() returned: {type(results)} with {len(results) if results else 0} items")
    except Exception as e:
        log_debug(f"EXCEPTION in search_and_download(): {str(e)}")
        import traceback
        log_debug(traceback.format_exc())
        print(json.dumps({
            "status": "error",
            "message": f"Exception during search: {str(e)}"
        }), file=sys.stderr)
        sys.stderr.flush()
        return 1
    
    if not results:
        log_debug("ERROR: Search failed or no results found (results is None or empty)")
        print(json.dumps({
            "status": "error",
            "message": "Search failed or no results found"
        }), file=sys.stderr)
        sys.stderr.flush()
        return 1
    
    log_debug(f"Search successful: {len(results)} results found")
    
    # Format output for plugin
    try:
        log_debug("Formatting JSON output...")
        output = {
            "status": "success",
            "count": len(results),
            "session_id": args.session_id,
            "results": []
        }
        
        for i, sound in enumerate(results, 1):
            log_debug(f"Formatting sound {i}/{len(results)}: ID={sound.get('id')}")
            output["results"].append({
                "id": sound["id"],
                "description": sound["description"],
                "category": sound["category"],
                "similarity": sound["similarity"],
                "local_path": sound["local_path"],
                "filename": Path(sound["local_path"]).name
            })
        
        log_debug(f"Formatting output with {len(output['results'])} results")
        json_output = json.dumps(output, indent=2)
        log_debug(f"JSON output length: {len(json_output)} chars")
        log_debug(f"First 200 chars of JSON: {json_output[:200]}")
        
        # CRITICAL DEBUG: Write full JSON to log file for inspection
        log_debug("=== FULL JSON OUTPUT START ===")
        log_debug(json_output)
        log_debug("=== FULL JSON OUTPUT END ===")
        
        # Determine output file path
        if args.output_json:
            output_file = args.output_json
        else:
            # Write to temp dir with session ID for easy polling
            import tempfile
            if args.session_id:
                output_file = os.path.join(tempfile.gettempdir(), f"sound_search_{args.session_id}.json")
            else:
                output_file = os.path.join(tempfile.gettempdir(), "sound_search_results.json")
        
        # Write results to file (avoids stdout/stderr blocking issues on Windows)
        try:
            log_debug(f"Writing results to file: {output_file}")
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(json_output)
            log_debug(f"✓ Results written to: {output_file}")
            
            # Print file path to stdout for plugin to read
            print(output_file)
            sys.stdout.flush()
            
        except Exception as e:
            log_debug(f"✗ File write failed: {e}")
            return 1
        
        log_debug("=== Output operation completed successfully ===")
        return 0
    except Exception as e:
        log_debug(f"EXCEPTION during JSON formatting: {str(e)}")
        import traceback
        log_debug(traceback.format_exc())
        print(json.dumps({
            "status": "error",
            "message": f"JSON formatting error: {str(e)}"
        }), file=sys.stderr)
        sys.stderr.flush()
        return 1


def action_download(args):
    """Download a specific sound by ID"""
    log_debug(f"Action: download - sound_id={args.sound_id}")
    
    if not args.sound_id:
        log_debug("ERROR: Must provide --sound-id")
        print(json.dumps({
            "status": "error",
            "message": "Must provide --sound-id"
        }), file=sys.stderr)
        return 1
    
    local_path = download_sound(
        sound_id=args.sound_id,
        output_path=args.output,
        session_id=args.session_id,
        quiet=args.quiet,
    )
    
    if not local_path:
        log_debug(f"ERROR: Failed to download sound {args.sound_id}")
        print(json.dumps({
            "status": "error",
            "message": f"Failed to download sound {args.sound_id}"
        }), file=sys.stderr)
        return 1
    
    log_debug(f"Download successful: {local_path}")
    print(json.dumps({
        "status": "success",
        "sound_id": args.sound_id,
        "local_path": local_path,
        "filename": Path(local_path).name
    }))
    return 0


def action_cleanup(args):
    """Clean up temporary files for a session"""
    log_debug(f"Action: cleanup - session_id={args.session_id}")
    
    if not args.session_id:
        log_debug("ERROR: Must provide --session-id")
        print(json.dumps({
            "status": "error",
            "message": "Must provide --session-id"
        }), file=sys.stderr)
        return 1
    
    success = cleanup_session(args.session_id, quiet=args.quiet)
    
    if success:
        log_debug(f"Session {args.session_id} cleaned up successfully")
        print(json.dumps({
            "status": "success",
            "message": f"Session {args.session_id} cleaned up"
        }))
        return 0
    else:
        log_debug(f"ERROR: Failed to clean up session {args.session_id}")
        print(json.dumps({
            "status": "error",
            "message": f"Failed to clean up session {args.session_id}"
        }), file=sys.stderr)
        return 1


def main():
    parser = argparse.ArgumentParser(
        description="BBC Sound Search API Client for Pro Tools Plugin",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    
    # Action selection
    parser.add_argument(
        '--action',
        type=str,
        choices=['health', 'search', 'download', 'cleanup'],
        default='search',
        help='Action to perform (default: search)'
    )
    
    # Search parameters
    parser.add_argument(
        '--video',
        type=str,
        help='Path to video file for video-based search'
    )
    
    parser.add_argument(
        '--text',
        type=str,
        help='Text query for text-based search'
    )
    
    parser.add_argument(
        '--limit',
        type=int,
        default=5,
        help='Number of results to return (default: 5)'
    )
    
    parser.add_argument(
        '--text-weight',
        type=float,
        default=0.6,
        help='Weight for text vs video (0-1, default: 0.6)'
    )
    
    parser.add_argument(
        '--num-frames',
        type=int,
        default=16,
        help='Number of frames to extract from video (default: 16)'
    )
    
    # Download parameters
    parser.add_argument(
        '--sound-id',
        type=int,
        help='Sound ID to download'
    )
    
    parser.add_argument(
        '--output',
        type=str,
        help='Output path for downloaded file'
    )
    
    # Session management
    parser.add_argument(
        '--session-id',
        type=str,
        help='Session ID for organizing temp files (auto-generated if not provided)'
    )
    
    # Output file path (for plugin integration - avoids stdout issues)
    parser.add_argument(
        '--output-json',
        type=str,
        help='Output JSON file path (default: writes to temp dir)'
    )
    
    # Output control
    parser.add_argument(
        '--quiet',
        action='store_true',
        help='Suppress progress messages'
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Show detailed output'
    )
    
    args = parser.parse_args()
    
    # Log script start
    log_debug(f"Sound Search API Client started - Action: {args.action}")
    log_debug(f"Arguments: {vars(args)}")
    log_debug(f"Log file: {LOG_FILE}")
    
    # Route to appropriate action handler
    if args.action == 'health':
        return action_health_check(args)
    elif args.action == 'search':
        return action_search(args)
    elif args.action == 'download':
        return action_download(args)
    elif args.action == 'cleanup':
        return action_cleanup(args)
    else:
        print(json.dumps({
            "status": "error",
            "message": f"Unknown action: {args.action}"
        }), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
