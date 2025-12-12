#!/usr/bin/env python3
"""
Sequential API VRAM Stress Test

Tests VRAM usage during inference for MMAudio and HunyuanVideo-Foley APIs.
Monitors VRAM continuously during request processing.

Usage:
    python3 test_concurrent_vram.py
"""

import asyncio
import aiohttp
import time
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, List
import sys


# Configuration
MMAUDIO_URL = "https://mmaudio.linwig.de"
HYVF_URL = "https://hyvf.linwig.de"
MONITORING_INTERVAL = 3.0  # seconds between VRAM checks during inference

# Test videos - using short clips from customMicroFoleyTestSet
TEST_VIDEOS = {
    "mmaudio": "../model-tests/data/customMicroFoleyTestSet/noSound/test_door.mp4",
    "hyvf": "../model-tests/data/customMicroFoleyTestSet/noSound/test_footstepsPavement.mp4",
}


@dataclass
class VRAMSnapshot:
    timestamp: float
    allocated_mb: float
    reserved_mb: float
    api_name: str
    
    def __str__(self):
        return f"{self.api_name}: {self.allocated_mb:.0f} MB allocated, {self.reserved_mb:.0f} MB reserved"


@dataclass
class TestResult:
    api_name: str
    video_name: str
    duration: float
    success: bool
    vram_snapshots: List[VRAMSnapshot] = field(default_factory=list)
    error: Optional[str] = None
    
    @property
    def vram_before(self) -> Optional[float]:
        return self.vram_snapshots[0].allocated_mb if self.vram_snapshots else None
    
    @property
    def vram_after(self) -> Optional[float]:
        return self.vram_snapshots[-1].allocated_mb if self.vram_snapshots else None
    
    @property
    def vram_peak(self) -> Optional[float]:
        return max(s.allocated_mb for s in self.vram_snapshots) if self.vram_snapshots else None
    
    @property
    def vram_delta(self) -> Optional[float]:
        if self.vram_before and self.vram_after:
            return self.vram_after - self.vram_before
        return None


async def get_vram_usage(session: aiohttp.ClientSession, api_url: str) -> VRAMSnapshot:
    """Poll /cache/stats endpoint for VRAM usage"""
    try:
        async with session.get(f"{api_url}/cache/stats", timeout=5) as resp:
            if resp.status != 200:
                print(f"⚠️  Failed to get memory stats from {api_url}: HTTP {resp.status}")
                return VRAMSnapshot(time.time(), 0.0, 0.0, api_url.split(':')[-1])
            
            stats = await resp.json()
            torch_view = stats["gpu_memory"]["torch_process_view"]
            
            return VRAMSnapshot(
                timestamp=time.time(),
                allocated_mb=torch_view["this_process_allocated_mb"],
                reserved_mb=torch_view["this_process_reserved_mb"],
                api_name=api_url.split(':')[-1]  # Extract port number
            )
    except Exception as e:
        print(f"⚠️  Error polling VRAM from {api_url}: {e}")
        return VRAMSnapshot(time.time(), 0.0, 0.0, api_url.split(':')[-1])


async def monitor_vram_continuously(session: aiohttp.ClientSession, 
                                   api_url: str, 
                                   snapshots: List[VRAMSnapshot],
                                   stop_event: asyncio.Event):
    """Background task to continuously monitor VRAM during inference"""
    while not stop_event.is_set():
        snapshot = await get_vram_usage(session, api_url)
        snapshots.append(snapshot)
        print(f"   📊 {snapshot}")
        
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=MONITORING_INTERVAL)
        except asyncio.TimeoutError:
            continue


async def send_request(session: aiohttp.ClientSession, 
                      api_url: str, 
                      video_path: str, 
                      params: dict) -> TestResult:
    """Send single API request with continuous VRAM monitoring"""
    
    api_name = "MMAudio" if "8000" in api_url else "HunyuanVideo-Foley"
    video_name = Path(video_path).name
    
    print(f"\n🚀 Starting request: {api_name} with {video_name}")
    print(f"   Parameters: {params}")
    
    # Initialize result
    snapshots = []
    start_time = time.time()
    
    # Get initial VRAM
    initial_snapshot = await get_vram_usage(session, api_url)
    snapshots.append(initial_snapshot)
    print(f"   📊 Initial: {initial_snapshot}")
    
    # Start background VRAM monitoring
    stop_event = asyncio.Event()
    monitor_task = asyncio.create_task(
        monitor_vram_continuously(session, api_url, snapshots, stop_event)
    )
    
    # Prepare request
    data = aiohttp.FormData()
    
    # Add video file
    with open(video_path, 'rb') as f:
        data.add_field('video', f, 
                      filename=Path(video_path).name, 
                      content_type='video/mp4')
        
        # Add parameters
        for key, value in params.items():
            data.add_field(key, str(value))
        
        # Send generation request
        try:
            print(f"   ⏳ Sending request...")
            async with session.post(f"{api_url}/generate", data=data, timeout=600) as resp:
                audio_data = await resp.read()
                success = resp.status == 200
                error = None if success else f"HTTP {resp.status}: {await resp.text()}"
                
                if success:
                    print(f"   ✅ Request completed ({len(audio_data)} bytes)")
                else:
                    print(f"   ❌ Request failed: {error}")
        except Exception as e:
            success = False
            error = str(e)
            print(f"   ❌ Exception: {error}")
    
    # Stop monitoring
    stop_event.set()
    await monitor_task
    
    # Get final VRAM
    final_snapshot = await get_vram_usage(session, api_url)
    snapshots.append(final_snapshot)
    print(f"   📊 Final: {final_snapshot}")
    
    duration = time.time() - start_time
    print(f"   ⏱️  Duration: {duration:.1f}s")
    
    return TestResult(
        api_name=api_name,
        video_name=video_name,
        duration=duration,
        success=success,
        vram_snapshots=snapshots,
        error=error
    )


def get_default_params(api_url: str) -> dict:
    """Get default parameters based on API"""
    if ":8000" in api_url:  # MMAudio
        return {
            "prompt": "door opening and closing",
            "negative_prompt": "voices, music",
            "seed": 42,
            "model_name": "large_44k_v2",
            "num_steps": 25,
            "cfg_strength": 4.5,
            "output_format": "wav",
            "full_precision": "false"
        }
    else:  # HunyuanVideo-Foley (8001)
        return {
            "prompt": "footsteps on pavement",
            "negative_prompt": "voices, music",
            "seed": 0,
            "model_size": "xxl",  # Test with XXL to check VRAM with both models
            "num_steps": 50,
            "cfg_strength": 4.5,
            "output_format": "wav",
            "full_precision": "false"
        }


async def run_concurrent_test():
    """Run concurrent tests (MMAudio and HunyuanVideo-Foley simultaneously)"""
    
    print("=" * 80)
    print("🧪 Concurrent API VRAM Stress Test")
    print("=" * 80)
    print(f"\n📝 Test Configuration:")
    print(f"   - VRAM monitoring interval: {MONITORING_INTERVAL}s")
    print(f"   - MMAudio API: {MMAUDIO_URL}")
    print(f"   - HunyuanVideo-Foley API: {HYVF_URL}")
    print(f"   - Test mode: CONCURRENT (both APIs at the same time)")
    
    # Check if test videos exist
    for api_name, video_path in TEST_VIDEOS.items():
        full_path = Path(__file__).parent / video_path
        if not full_path.exists():
            print(f"\n❌ Error: Test video not found: {full_path}")
            print(f"   Please ensure the video exists or update TEST_VIDEOS in the script")
            return
        print(f"   - {api_name}: {video_path}")
    
    async with aiohttp.ClientSession() as session:
        print("\n" + "=" * 80)
        print("🚀 STARTING CONCURRENT REQUESTS")
        print("=" * 80)
        print("\n⚡ Sending requests to BOTH APIs simultaneously...\n")
        
        # Prepare both requests
        mmaudio_video = str(Path(__file__).parent / TEST_VIDEOS["mmaudio"])
        mmaudio_params = get_default_params(MMAUDIO_URL)
        
        hyvf_video = str(Path(__file__).parent / TEST_VIDEOS["hyvf"])
        hyvf_params = get_default_params(HYVF_URL)
        
        # Execute BOTH requests concurrently
        results = await asyncio.gather(
            send_request(session, MMAUDIO_URL, mmaudio_video, mmaudio_params),
            send_request(session, HYVF_URL, hyvf_video, hyvf_params),
            return_exceptions=True
        )
        
        # Filter out exceptions
        results = [r for r in results if isinstance(r, TestResult)]
    
    # Print summary
    print("\n" + "=" * 80)
    print("📊 TEST SUMMARY")
    print("=" * 80)
    
    for r in results:
        status = "✅ SUCCESS" if r.success else "❌ FAILED"
        print(f"\n{status}: {r.api_name}")
        print(f"   Video: {r.video_name}")
        print(f"   Duration: {r.duration:.1f}s")
        print(f"   VRAM Before: {r.vram_before:.0f} MB" if r.vram_before else "   VRAM Before: N/A")
        print(f"   VRAM Peak: {r.vram_peak:.0f} MB" if r.vram_peak else "   VRAM Peak: N/A")
        print(f"   VRAM After: {r.vram_after:.0f} MB" if r.vram_after else "   VRAM After: N/A")
        print(f"   VRAM Delta: {r.vram_delta:+.0f} MB" if r.vram_delta else "   VRAM Delta: N/A")
        print(f"   Snapshots: {len(r.vram_snapshots)} measurements")
        
        if r.error:
            print(f"   Error: {r.error}")
    
    print("\n" + "=" * 80)
    print("🏁 Test completed")
    print("=" * 80)


if __name__ == "__main__":
    try:
        asyncio.run(run_concurrent_test())
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        sys.exit(130)
