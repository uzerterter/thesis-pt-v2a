"""
Configuration constants and helpers for the MMAudio API client.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict

# Paths
_BASE_DIR = Path(__file__).resolve().parent
_CONFIG_PATH = _BASE_DIR / "config.json"

# API Configuration defaults
DEFAULT_API_URL = "http://localhost:8000"

# Config defaults (can be overridden via companion/api/config.json)
CONFIG_DEFAULTS: Dict[str, Any] = {
  "use_cloudflared": False,
  "api_url_direct": DEFAULT_API_URL,
  "api_url_cloudflared": "",
  "cf_access_client_id": "",
  "cf_access_client_secret": "",
}

_config_cache: Dict[str, Any] | None = None


def _load_config() -> Dict[str, Any]:
    """Load the user config file (with sensible defaults)."""
    global _config_cache
    if _config_cache is not None:
        return _config_cache

    config = CONFIG_DEFAULTS.copy()
    if _CONFIG_PATH.exists():
        try:
            file_data = json.loads(_CONFIG_PATH.read_text())
            if isinstance(file_data, dict):
                config.update(file_data)
        except json.JSONDecodeError:
            # Keep defaults if the config is invalid.
            pass
    _config_cache = config
    return _config_cache


def reload_config() -> None:
    """Clear the cached config (primarily for tests)."""
    global _config_cache
    _config_cache = None


def get_config() -> Dict[str, Any]:
    """Return a copy of the loaded config."""
    return _load_config().copy()


def use_cloudflared() -> bool:
    """True when the user wants to route requests through Cloudflare Tunnel."""
    cfg = _load_config()
    return bool(cfg.get("use_cloudflared") and cfg.get("api_url_cloudflared"))


def get_api_url() -> str:
    """Return the currently active API URL (direct or Cloudflare)."""
    cfg = _load_config()
    if use_cloudflared():
        return cfg.get("api_url_cloudflared") or DEFAULT_API_URL
    return cfg.get("api_url_direct") or DEFAULT_API_URL


def get_cf_headers() -> Dict[str, str]:
    """Return Cloudflare Access headers (only when enabled)."""
    if not use_cloudflared():
        return {}
    cfg = _load_config()
    client_id = cfg.get("cf_access_client_id")
    client_secret = cfg.get("cf_access_client_secret")
    if client_id and client_secret:
        return {
            "CF-Access-Client-Id": client_id,
            "CF-Access-Client-Secret": client_secret,
        }
    return {}


# Supported video formats for Pro Tools integration
SUPPORTED_VIDEO_FORMATS = {
    ".mp4",
    ".mov",
    ".avi",
    ".mkv",
    ".webm",
    ".flv",
    ".wmv",
    ".m4v",
}

# Default generation parameters
DEFAULT_NEGATIVE_PROMPT = "voices, music, melody, singing, speech"
DEFAULT_SEED = 42
DEFAULT_NUM_STEPS = 25
DEFAULT_CFG_STRENGTH = 4.5
DEFAULT_MODEL = "large_44k_v2"

# Output configuration
DEFAULT_OUTPUT_FORMAT = "wav"  # "wav" or "flac"
DEFAULT_TIMEOUT = 300  # seconds
