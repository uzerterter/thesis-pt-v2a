"""
Pro Tools video file operations via PTSL

Provides:
- Video file detection from Pro Tools session
"""

from typing import Dict


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
            # Get video files from session
            video_files = engine.get_file_location(filters=[pt.Video_Files])
            
            if not video_files:
                return {
                    'success': False,
                    'video_path': None,
                    'video_files': [],
                    'error': 'No video files found in Pro Tools session'
                }
            
            # Return first video file (most common case)
            return {
                'success': True,
                'video_path': video_files[0],
                'video_files': video_files,
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
