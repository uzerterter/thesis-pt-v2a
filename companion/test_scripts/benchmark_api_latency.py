#!/usr/bin/env python3
"""Benchmark CLI-like API calls via localhost vs Cloudflare endpoints.
Usage:
cd ludwig-thesis/thesis-pt-v2a
python companion/test_scripts/benchmark_api_latency.py --service mmaudio --video path/to/test.mp4 --runs 3
python companion/test_scripts/benchmark_api_latency.py --service hunyuan --video path/to/test.mp4 --runs 3
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import requests

sys.path.append(str(Path(__file__).resolve().parents[1]))  # companion/
from api.config import get_config, get_cf_headers, get_api_url, use_cloudflared  # noqa: E402

SERVICE_PAYLOADS = {
    "mmaudio": {
        "data": {
            "prompt": "benchmark prompt",
            "negative_prompt": "voices, music",
            "seed": 42,
            "model_name": "large_44k_v2",
            "num_steps": 25,
            "cfg_strength": 4.5,
            "output_format": "wav",
            "full_precision": False,
            "duration": 10.0,
        }
    },
    "hunyuan": {
        "data": {
            "prompt": "benchmark prompt",
            "negative_prompt": "voices, music",
            "seed": 42,
            "model_size": "xxl",
            "num_steps": 50,
            "cfg_strength": 4.5,
            "output_format": "wav",
            "full_precision": False,
            "duration": 10.0,
        }
    },
}


class Result:
    def __init__(self, endpoint: str, elapsed: float, status: int, upload_bytes: int, download_bytes: int, error: Optional[str] = None) -> None:
        self.endpoint = endpoint
        self.elapsed = elapsed
        self.status = status
        self.upload_bytes = upload_bytes
        self.download_bytes = download_bytes
        self.error = error


def build_form_data(data: Dict[str, object]) -> Dict[str, str]:
    form = {}
    for key, value in data.items():
        if isinstance(value, bool):
            form[key] = json.dumps(value)
        else:
            form[key] = str(value)
    return form


def send_request(service: str, url: str, video_path: Path, headers: Dict[str, str]) -> Result:
    payload = SERVICE_PAYLOADS[service]["data"].copy()
    files = {"video": (video_path.name, video_path.read_bytes(), "video/mp4")}
    upload_bytes = video_path.stat().st_size

    start = time.time()
    try:
        response = requests.post(f"{url}/generate", headers=headers, data=build_form_data(payload), files=files, timeout=900)
        elapsed = time.time() - start
        download_bytes = len(response.content)
        response.raise_for_status()
        return Result(url, elapsed, response.status_code, upload_bytes, download_bytes)
    except requests.RequestException as exc:
        elapsed = time.time() - start
        status = getattr(exc.response, "status_code", -1)
        download_bytes = len(getattr(exc.response, "content", b""))
        return Result(url, elapsed, status, upload_bytes, download_bytes, str(exc))


def summarize(results: List[Result]) -> None:
    grouped: Dict[str, List[Result]] = {}
    for result in results:
        grouped.setdefault(result.endpoint, []).append(result)

    for endpoint, rows in grouped.items():
        successes = [r.elapsed for r in rows if r.error is None]
        print(f"\nEndpoint: {endpoint}")
        print(f"  Runs: {len(rows)}, Successes: {len(successes)}")
        if successes:
            print(
                "  Avg: {:.2f}s | Median: {:.2f}s | Min: {:.2f}s | Max: {:.2f}s".format(
                    statistics.mean(successes), statistics.median(successes), min(successes), max(successes)
                )
            )
        for r in rows:
            status = f"status={r.status}" if r.error is None else f"error={r.error}"
            print(
                "    - {:.2f}s | upload={:.2f} MB | download={:.2f} MB | {}".format(
                    r.elapsed,
                    r.upload_bytes / 1e6,
                    r.download_bytes / 1e6,
                    status,
                )
            )


def get_endpoints(service: str) -> List[Tuple[str, Dict[str, str]]]:
    cfg = get_config()
    original_flag = use_cloudflared()

    # Local (whatever config currently uses)
    local_url = get_api_url(service)
    headers_local = {}

    # Cloudflare – temporarily force flag on by creating a copy of config
    cfg["use_cloudflared"] = True
    os.environ["FORCE_CF_MODE"] = "1"
    cloud_url = get_api_url(service)
    headers_cf = get_cf_headers()
    os.environ.pop("FORCE_CF_MODE", None)

    # Restore flag
    if not original_flag:
        cfg["use_cloudflared"] = False

    return [(local_url, headers_local), (cloud_url, headers_cf)]


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark localhost vs Cloudflare API latency")
    parser.add_argument("--service", choices=SERVICE_PAYLOADS.keys(), required=True)
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()

    if not args.video.exists():
        print(f"Video file not found: {args.video}")
        return 1

    results: List[Result] = []
    for url, headers in get_endpoints(args.service):
        for _ in range(args.runs):
            results.append(send_request(args.service, url, args.video, headers))

    summarize(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
