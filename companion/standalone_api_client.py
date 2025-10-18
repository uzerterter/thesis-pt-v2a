#!/usr/bin/env python3
"""
Standalone MMAudio API Client

A client script that uses the standalone MMAudio API server for fast video-to-audio generation.
This replaces the ComfyUI-based approach with a direct API call for better performance.

Usage:
    python standalone_api_client.py

Features:
- Interactive parameter input (prompt, negative prompt, seed)
- Automatic video duration detection
- Progress feedback and timing information
- Compatible with existing video test files
"""

import requests
import time
import os
from pathlib import Path
from typing import Optional

# Configuration
DEFAULT_API_URL = "http://localhost:8000"
DEFAULT_VIDEO_PATH = "/mnt/disk1/users/ludwig/ludwig-thesis/model-tests/data/MMAudio_examples/noSound/sora_beach.mp4"

def get_user_inputs():
    """Sammelt Benutzereingaben für Prompt, Negative Prompt und Seed"""
    print("\n=== MMAudio Standalone API Client ===")

    prompt = input("🎵 Prompt (Default: ''): ").strip()
    negative_prompt = input("❌ Negative Prompt (Default: 'voices, music'): ").strip()
    if not negative_prompt:
        negative_prompt = "voices, music"
    
    while True:
        seed_input = input("🎲 Seed (Default: 42): ").strip()
        if not seed_input:
            seed = 42
            break
        try:
            seed = int(seed_input)
            break
        except ValueError:
            print("   ⚠️  Bitte geben Sie eine gültige Zahl ein.")
    
    print(f"\n✅ Parameter gesetzt:")
    print(f"   Prompt: '{prompt}'")
    print(f"   Negative Prompt: '{negative_prompt}'")
    print(f"   Seed: {seed}")
    
    return prompt, negative_prompt, seed

def check_api_health(api_url: str) -> bool:
    """Überprüft ob die API erreichbar ist"""
    try:
        response = requests.get(f"{api_url}/", timeout=10)
        response.raise_for_status()
        return True
    except requests.exceptions.RequestException as e:
        print(f"❌ API nicht erreichbar: {e}")
        return False

def get_available_models(api_url: str) -> Optional[dict]:
    """Holt verfügbare Modelle von der API"""
    try:
        response = requests.get(f"{api_url}/models", timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"⚠️  Konnte Modelle nicht abrufen: {e}")
        return None

def generate_audio(
    api_url: str,
    video_path: str,
    prompt: str,
    negative_prompt: str,
    seed: int,
    model_name: str = "large_44k_v2",
    duration: Optional[float] = None,
    num_steps: int = 25,
    cfg_strength: float = 4.5
) -> Optional[str]:
    """Generiert Audio über die Standalone API"""
    
    if not os.path.exists(video_path):
        print(f"❌ Video file not found: {video_path}")
        return None
    
    print(f"\n🚀 Sending request to API...")
    print(f"   Video: {Path(video_path).name}")
    print(f"   Model: {model_name}")
    print(f"   Duration: {'auto-detect' if duration is None else f'{duration}s'}")
    
    # Prepare request data
    data = {
        "prompt": prompt,
        "negative_prompt": negative_prompt,
        "seed": seed,
        "model_name": model_name,
        "num_steps": num_steps,
        "cfg_strength": cfg_strength
    }
    
    if duration is not None:
        data["duration"] = duration
    
    start_time = time.time()
    
    try:
        with open(video_path, 'rb') as video_file:
            files = {"video": (Path(video_path).name, video_file, "video/mp4")}
            
            print("⏳ Processing... (this may take a minute)")
            response = requests.post(
                f"{api_url}/generate",
                files=files,
                data=data,
                timeout=300  # 5 minutes timeout
            )
            response.raise_for_status()
        
        total_time = time.time() - start_time
        
        # Get metadata from headers
        generation_time = response.headers.get('X-Generation-Time', 'unknown')
        actual_duration = response.headers.get('X-Duration', 'unknown')
        used_seed = response.headers.get('X-Seed', seed)
        
        print(f"\n✅ Audio generated successfully!")
        print(f"   Total time: {total_time:.2f}s")
        print(f"   Generation time: {generation_time}s")
        print(f"   Video duration: {actual_duration}s")
        print(f"   Seed used: {used_seed}")
        
        # Save audio file
        timestamp = int(time.time())
        output_filename = f"generated_audio_{timestamp}_{seed}.flac"
        output_path = Path(f"./standalone-API_outputs/{output_filename}")

        with open(output_path, 'wb') as f:
            f.write(response.content)
        
        print(f"📁 Audio saved as: {output_path.absolute()}")
        print(f"📊 File size: {len(response.content) / 1024:.1f} KB")
        
        return str(output_path)
        
    except requests.exceptions.Timeout:
        print("❌ Request timed out. The server might be overloaded.")
        return None
    except requests.exceptions.RequestException as e:
        print(f"❌ Request failed: {e}")
        if hasattr(e.response, 'text'):
            print(f"   Server response: {e.response.text}")
        return None
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return None

def main():
    print("🌐 MMAudio Standalone API Client")
    print("==================================")
    
    # Get user inputs
    prompt, negative_prompt, seed = get_user_inputs()
    
    # Check API health
    print(f"\n🔗 Checking API connection to {DEFAULT_API_URL}...")
    if not check_api_health(DEFAULT_API_URL):
        print("\n💡 Make sure the API server is running:")
        print("   cd /path/to/standalone-API")
        print("   python main.py")
        return 1
    
    print("✅ API is online!")
    
    # Get available models (optional info)
    models_info = get_available_models(DEFAULT_API_URL)
    if models_info:
        loaded_models = models_info.get("loaded_models", [])
        if loaded_models:
            print(f"📦 Loaded models: {', '.join(loaded_models)}")
        else:
            print("📦 No models loaded yet (will load on first request)")
    
    # Check video file
    if not os.path.exists(DEFAULT_VIDEO_PATH):
        print(f"❌ Video file not found: {DEFAULT_VIDEO_PATH}")
        print("   Please update DEFAULT_VIDEO_PATH in the script")
        return 1
    
    video_size_mb = os.path.getsize(DEFAULT_VIDEO_PATH) / (1024 * 1024)
    print(f"📹 Video file: {Path(DEFAULT_VIDEO_PATH).name} ({video_size_mb:.1f} MB)")
    
    # Generate audio
    output_file = generate_audio(
        api_url=DEFAULT_API_URL,
        video_path=DEFAULT_VIDEO_PATH,
        prompt=prompt,
        negative_prompt=negative_prompt,
        seed=seed
    )
    
    if output_file:
        print(f"\n🎉 Success! Audio generated and saved.")
        print(f"   Output: {output_file}")

    else:
        print(f"\n❌ Audio generation failed.")
        return 1

if __name__ == "__main__":
    exit(main())