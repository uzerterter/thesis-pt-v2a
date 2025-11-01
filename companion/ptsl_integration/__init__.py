"""
PTSL Integration Package
Handles Pro Tools Scripting Library (PTSL) communication via gRPC

Now using py-ptsl library for maintained PTSL integration.
Legacy custom implementation preserved in ptsl_client_v1_LEGACY.py for reference.
"""

# Import from py-ptsl based implementation (now the main version)
from .ptsl_client import import_audio_to_pro_tools

__all__ = ['import_audio_to_pro_tools']
