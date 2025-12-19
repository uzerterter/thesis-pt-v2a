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
from pathlib import Path

# Add api module to path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from api.sound_search_client import (
    search_sounds,
    download_sound,
    search_and_download,
    cleanup_session,
    check_api_health,
)


def action_health_check(args):
    """Check if Sound Search API is available"""
    print("Checking Sound Search API health...")
    
    if check_api_health(quiet=False):
        print(json.dumps({
            "status": "success",
            "message": "Sound Search API is available"
        }))
        return 0
    else:
        print(json.dumps({
            "status": "error",
            "message": "Sound Search API is not available"
        }), file=sys.stderr)
        return 1


def action_search(args):
    """Search for sounds using video and/or text"""
    if not args.video and not args.text:
        print(json.dumps({
            "status": "error",
            "message": "Must provide either --video or --text"
        }), file=sys.stderr)
        return 1
    
    # Perform search and download
    results = search_and_download(
        video_path=args.video,
        text_query=args.text,
        limit=args.limit,
        text_weight=args.text_weight,
        num_frames=args.num_frames,
        session_id=args.session_id,
        quiet=args.quiet,
        verbose=args.verbose,
    )
    
    if not results:
        print(json.dumps({
            "status": "error",
            "message": "Search failed or no results found"
        }), file=sys.stderr)
        return 1
    
    # Format output for plugin
    output = {
        "status": "success",
        "count": len(results),
        "session_id": args.session_id,
        "results": []
    }
    
    for sound in results:
        output["results"].append({
            "id": sound["id"],
            "description": sound["description"],
            "category": sound["category"],
            "similarity": sound["similarity"],
            "local_path": sound["local_path"],
            "filename": Path(sound["local_path"]).name
        })
    
    print(json.dumps(output, indent=2))
    return 0


def action_download(args):
    """Download a specific sound by ID"""
    if not args.sound_id:
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
        print(json.dumps({
            "status": "error",
            "message": f"Failed to download sound {args.sound_id}"
        }), file=sys.stderr)
        return 1
    
    print(json.dumps({
        "status": "success",
        "sound_id": args.sound_id,
        "local_path": local_path,
        "filename": Path(local_path).name
    }))
    return 0


def action_cleanup(args):
    """Clean up temporary files for a session"""
    if not args.session_id:
        print(json.dumps({
            "status": "error",
            "message": "Must provide --session-id"
        }), file=sys.stderr)
        return 1
    
    success = cleanup_session(args.session_id, quiet=args.quiet)
    
    if success:
        print(json.dumps({
            "status": "success",
            "message": f"Session {args.session_id} cleaned up"
        }))
        return 0
    else:
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
