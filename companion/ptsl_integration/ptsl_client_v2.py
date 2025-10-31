"""
PTSL Integration v2 - Using py-ptsl library
============================================

This module replaces our custom ptsl_client.py with the professional py-ptsl library.

Advantages over custom implementation:
- Professional, tested, maintained library
- Better API (Python-native instead of JSON)
- Type safety with .pyi files
- Comprehensive error handling
- All PTSL commands already implemented

Installation:
    pip install -e ../external/py-ptsl

Usage:
    >>> from ptsl_client_v2 import import_audio_to_pro_tools
    >>> success = import_audio_to_pro_tools("C:/audio/generated.flac")
"""

import sys
from pathlib import Path
from typing import Optional

try:
    import soundfile as sf
    SOUNDFILE_AVAILABLE = True
except ImportError:
    SOUNDFILE_AVAILABLE = False
    print("Warning: soundfile not available - FLAC conversion disabled", file=sys.stderr)

try:
    from ptsl import open_engine
    import ptsl.PTSL_pb2 as pt
    from ptsl.ops import Import
    PTSL_AVAILABLE = True
except ImportError:
    PTSL_AVAILABLE = False
    print("Error: py-ptsl not installed!", file=sys.stderr)
    print("Install with: pip install -e external/py-ptsl", file=sys.stderr)


def import_audio_to_pro_tools(
    audio_path: str,
    company_name: str = "Master Thesis",
    app_name: str = "PT V2A Plugin",
    host: str = "localhost",
    port: int = 31416
) -> bool:
    """
    Import audio file to Pro Tools using py-ptsl library.
    
    This is a drop-in replacement for the old import_audio_to_pro_tools function
    but uses the py-ptsl library instead of our custom implementation.
    
    Features:
        - Automatic FLAC → WAV conversion (PTSL requires WAV)
        - Automatic connection management
        - Better error handling
        - Professional, tested codebase
    
    Args:
        audio_path (str): Path to audio file (WAV or FLAC)
        company_name (str): Your company name (for PTSL logs)
        app_name (str): Application name (for PTSL logs)
        host (str): PTSL server hostname (default: localhost)
        port (int): PTSL server port (default: 31416)
    
    Returns:
        bool: True if import succeeded, False otherwise
        
    Example:
        >>> # Simple usage
        >>> success = import_audio_to_pro_tools("C:/audio/generated.flac")
        
        >>> # With custom names
        >>> success = import_audio_to_pro_tools(
        >>>     "C:/audio/file.wav",
        >>>     company_name="My Studio",
        >>>     app_name="Audio Tool"
        >>> )
        
    Note:
        py-ptsl Engine expects 'address' parameter in format "host:port"
        rather than separate host and port arguments.
    """
    if not PTSL_AVAILABLE:
        print("ERROR: py-ptsl library not installed", file=sys.stderr)
        return False
    
    # Convert FLAC to WAV if needed (PTSL limitation)
    audio_path = Path(audio_path)
    actual_path = audio_path
    
    if audio_path.suffix.lower() == '.flac':
        if not SOUNDFILE_AVAILABLE:
            print("ERROR: soundfile required for FLAC conversion", file=sys.stderr)
            print("Install with: pip install soundfile", file=sys.stderr)
            return False
        
        print(f"Converting FLAC to WAV (PTSL requirement)...")
        try:
            # Read FLAC
            data, samplerate = sf.read(str(audio_path))
            
            # Write WAV (24-bit PCM)
            wav_path = audio_path.with_suffix('.wav')
            sf.write(str(wav_path), data, samplerate, subtype='PCM_24')
            
            actual_path = wav_path
            print(f"Converted to: {wav_path}")
            
        except Exception as e:
            print(f"ERROR: FLAC conversion failed: {e}", file=sys.stderr)
            return False
    
    # Convert to absolute path (PTSL requires absolute paths)
    actual_path = actual_path.absolute()
    
    # Check file exists
    if not actual_path.exists():
        print(f"ERROR: Audio file not found: {actual_path}", file=sys.stderr)
        return False
    
    # Import using py-ptsl
    try:
        # py-ptsl expects address in "host:port" format
        address = f"{host}:{port}"
        print(f"Connecting to Pro Tools at {address}...")
        
        with open_engine(
            company_name=company_name,
            application_name=app_name,
            address=address
        ) as engine:
            
            print(f"Importing audio to Pro Tools...")
            print(f"  File: {actual_path}")
            
            # Import audio to new track at session start
            # Note: Using forward slashes even on Windows (PTSL requirement)
            file_path = str(actual_path).replace('\\', '/')
            
            # Build import manually because engine.import_audio() doesn't set session_path
            # For audio-only import, session_path must be empty string (not None)
            location_data = pt.SpotLocationData(
                location_type=pt.Start,
                location_options=pt.TimeCode,
                location_value="00:00:00:00"
            )
            audio_data = pt.AudioData(
                file_list=[file_path],
                audio_destination=pt.MD_NewTrack,
                audio_location=pt.ML_Spot,
                location_data=location_data
            )
            
            # CRITICAL: session_path="" for audio import (not None!)
            import_op = Import(
                session_path="",          # Empty string required for audio-only import
                import_type=pt.Audio,     # Audio file import (not Session)
                audio_data=audio_data
            )
            engine.client.run(import_op)
            
            print("✅ Audio successfully imported to Pro Tools!")
            return True
            
    except Exception as e:
        print(f"ERROR: Import failed: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return False


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Import audio to Pro Tools using py-ptsl")
    parser.add_argument("--audio", required=True, help="Audio file path")
    parser.add_argument("--host", default="localhost", help="PTSL host")
    parser.add_argument("--port", type=int, default=31416, help="PTSL port")
    
    args = parser.parse_args()
    
    success = import_audio_to_pro_tools(args.audio, host=args.host, port=args.port)
    sys.exit(0 if success else 1)
