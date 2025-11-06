"""
Pro Tools timeline operations via PTSL

Provides:
- Timeline selection reading (In/Out points)
- Timecode conversion utilities
"""

from typing import Dict


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
            raise ValueError(f"Invalid timecode format: {timecode} (expected HH:MM:SS:FF)")
        
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
        
        # Store result before closing connection to avoid hanging in context manager
        result = None
        
        # Connect to Pro Tools via PTSL
        with open_engine(
            company_name=company_name,
            application_name=app_name
        ) as engine:
            # Get timeline selection in TimeCode format
            # Returns: Tuple[str, str] = (in_time, out_time)
            in_time, out_time = engine.get_timeline_selection(pt.TimeCode)
            
            # Get frame rate for accurate timecode conversion
            # Pro Tools common frame rates: 23.976, 24, 25, 29.97, 30
            # Default to 30 fps if not available
            fps = 30.0  # Default to 30 fps (most common)
            
            # Convert to seconds for duration calculation
            in_seconds = timecode_to_seconds(in_time, fps)
            out_seconds = timecode_to_seconds(out_time, fps)
            duration_seconds = out_seconds - in_seconds
            
            # Build result while connection is open
            result = {
                'success': True,
                'in_time': in_time,
                'out_time': out_time,
                'in_seconds': in_seconds,
                'out_seconds': out_seconds,
                'duration_seconds': duration_seconds,
                'fps': fps,
                'error': None
            }
        
        # Connection closed, return immediately
        return result
            
    except ImportError as e:
        return {
            'success': False,
            'in_time': None,
            'out_time': None,
            'in_seconds': None,
            'out_seconds': None,
            'duration_seconds': None,
            'fps': None,
            'error': f'py-ptsl not available: {e}'
        }
    except Exception as e:
        return {
            'success': False,
            'in_time': None,
            'out_time': None,
            'in_seconds': None,
            'out_seconds': None,
            'duration_seconds': None,
            'fps': None,
            'error': f'PTSL error: {str(e)}'
        }
