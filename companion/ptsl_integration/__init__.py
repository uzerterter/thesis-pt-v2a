"""
PTSL Integration Package
Handles Pro Tools Scripting Library (PTSL) communication via gRPC

Using py-ptsl library for maintained PTSL integration.
"""

# Import from py-ptsl based implementation (now the main version)
from .ptsl_client import import_audio_to_pro_tools

__all__ = ['import_audio_to_pro_tools']
