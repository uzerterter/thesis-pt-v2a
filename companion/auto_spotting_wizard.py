#!/usr/bin/env python3
"""
Auto Spotting Wizard - Wizard of Oz Prototype

This script simulates automatic audio event detection and marker placement
for user study purposes. It performs fake progress updates over 12-15 seconds,
then creates memory locations at hardcoded positions in the Pro Tools timeline.

Workflow:
    1. Print progress updates to stdout (C++ reads these for UI updates)
    2. Sleep between updates to simulate processing time
    3. Connect to Pro Tools via PTSL
    4. Create memory locations at predefined positions
    5. Print success message

Usage:
    python auto_spotting_wizard.py

Requirements:
    - Pro Tools running with session open
    - py-ptsl installed (pip install py-ptsl)
    - PTSL server running in Pro Tools

Configuration:
    Edit MEMORY_LOCATIONS list below with positions extracted from test session
    (Use extract_memory_locations.py to get these values)
"""

import sys
from pathlib import Path
from ptsl import open_engine
import ptsl.PTSL_pb2 as pt

# Add companion modules to path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from cli.error_handler import safe_action_wrapper

# ============================================================================
# CONFIGURATION: Edit this list with your test session memory locations
# ============================================================================
# Format: (name, start_time, end_time, color_index, location, track_name)
# Use extract_memory_locations.py to get these values from your test session
# NOTE: Positions must be unique - Pro Tools doesn't allow multiple markers at same position
MEMORY_LOCATIONS = [
    ("DIA", "0", "0", 7, "MLC_MainRuler", ""),
    ("Foley", "0", "0", 7, "MLC_MainRuler", ""),
    ("BG", "0", "0", 7, "MLC_MainRuler", ""),
    ("FX", "0", "0", 7, "MLC_MainRuler", ""),
    ("MX", "0", "0", 7, "MLC_MainRuler", ""),
    ("Master", "0", "0", 7, "MLC_MainRuler", ""),
    ("FFOA", "4774000", "4774000", 16, "MLC_MainRuler", ""),
    ("Location 85", "6882000", "6882000", 13, "MLC_MainRuler", ""),
    ("Location 86", "6636000", "6636000", 13, "MLC_MainRuler", ""),
    ("Credits", "16286000", "16286000", 16, "MLC_MainRuler", ""),
    ("LFOA", "19680000", "19680000", 16, "MLC_MainRuler", ""),
]

# Timing configuration
TOTAL_DURATION = 13  # Total fake progress duration in seconds
PROGRESS_STEPS = 4   # Number of progress updates

def main():
    """Execute auto spotting workflow - returns dict for JSON serialization"""
    
    # Validate configuration first (before any PTSL operations)
    if not MEMORY_LOCATIONS:
        return {
            "success": False,
            "error": "No memory locations configured. Edit MEMORY_LOCATIONS list in script."
        }
    
    # Note: Fake delays (3s+4s+4s=11s) are handled by C++ plugin timer
    # This script only creates the memory locations as fast as possible
    print("Creating memory locations...", flush=True)
    
    # CRITICAL: Store all results BEFORE closing PTSL connection
    # Accessing engine methods after context closes causes Pro Tools crashes
    session_name = None
    created_count = 0
    failed_markers = []
    
    try:
        # Connect to Pro Tools - will raise if PTSL server not running
        print("Connecting to Pro Tools...", flush=True)
        with open_engine(
            company_name="Master Thesis",
            application_name="Memory Location Extractor"
        ) as engine:
            # Get session name - will raise if no session open
            try:
                session_name = engine.session_name()
                print(f"Connected to Pro Tools session: {session_name}", flush=True)
            except Exception as e:
                # Session name failed - likely no session open
                return {
                    "success": False,
                    "error": f"Could not get session name. Is a session open in Pro Tools? ({e})"
                }
            
            # Create each memory location
            for i, (name, start_time, end_time, color_index, location_str, track_name) in enumerate(MEMORY_LOCATIONS):
                print(f"Creating marker {i+1}/{len(MEMORY_LOCATIONS)}: {name}", flush=True)
                
                try:
                    # Convert location string to enum value
                    location_enum = getattr(pt, location_str, pt.MLC_MainRuler)
                    
                    # Create memory location via PTSL
                    # NOTE: memory_number must be unique - using i+1 as sequential IDs
                    engine.create_memory_location(
                        memory_number=i + 1,  # Unique ID for each marker (1, 2, 3, ...)
                        name=name,
                        start_time=start_time,
                        end_time=end_time,
                        time_properties=pt.TP_Marker,  # Marker type (not Selection)
                        reference=pt.MLR_Absolute,     # Absolute timeline position
                        general_properties=pt.MemoryLocationProperties(
                            zoom_settings=False,
                            pre_post_roll_times=False,
                            track_visibility=False,
                            track_heights=False,
                            group_enables=False,
                            window_configuration=False
                        ),
                        comments="Auto-generated by Auto Spotting wizard",
                        color_index=color_index,
                        location=location_enum,  # Main ruler, Track, or Named ruler
                        track_name=track_name if track_name else None  # Track name if location is MLC_Track
                    )
                    created_count += 1
                except Exception as e:
                    failed_markers.append({"name": name, "error": str(e)})
                    # Continue with next marker instead of aborting
                    continue
        
        # PTSL connection closed - now safe to build result with stored values
        result = {
            "success": created_count > 0,
            "created": created_count,
            "total": len(MEMORY_LOCATIONS),
            "session": session_name
        }
        
        if failed_markers:
            result["warnings"] = [f"Failed to create '{m['name']}': {m['error']}" for m in failed_markers]
        
        print(f"SUCCESS: Created {created_count}/{len(MEMORY_LOCATIONS)} memory locations", flush=True)
        
        return result
        
    except ConnectionRefusedError:
        return {
            "success": False,
            "error": "Cannot connect to Pro Tools. Is Pro Tools running and PTSL server enabled?"
        }
    except TimeoutError:
        return {
            "success": False,
            "error": "Connection to Pro Tools timed out. Check PTSL server status."
        }
    except ImportError as e:
        return {
            "success": False,
            "error": f"py-ptsl library not available: {e}"
        }

if __name__ == "__main__":
    sys.exit(safe_action_wrapper(main))