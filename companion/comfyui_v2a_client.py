import json, time, requests, pathlib
import os
import hashlib
import pickle
import numpy as np
import torch
import av  # PyAV for efficient video processing
from pathlib import Path
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

try:
    import av
except ImportError:
    print("❌ PyAV nicht installiert. Installiere mit: pip install av")
    exit(1)

HOST = "http://localhost:8188"  # ComfyUI SSH tunnel endpoint

# Create a session with retry strategy for better ngrok compatibility
session = requests.Session()
retry_strategy = Retry(
    total=3,
    backoff_factor=1,
    status_forcelist=[429, 500, 502, 503, 504],
)
adapter = HTTPAdapter(max_retries=retry_strategy)
session.mount("http://", adapter)
session.mount("https://", adapter)

# Video file path - using the test video from your test data
VIDEO_PATH = r"C:\Users\Ludenbold\Desktop\Master_Thesis\Implementation\model-tests\test-data\MMAudio_examples\noSound\sora_beach.mp4"

# Video Cache Configuration
VIDEO_CACHE = {}
CACHE_DIR = Path("./video_cache")

def get_video_cache_key(video_path, duration, target_fps=25.0):
    """Erstellt einen eindeutigen Cache Key für Video + Parameter"""
    video_stat = Path(video_path).stat()
    cache_data = f"{video_path}_{video_stat.st_mtime}_{video_stat.st_size}_{duration}_{target_fps}"
    return hashlib.md5(cache_data.encode()).hexdigest()

def load_video_optimized(video_path, duration=8.0, target_fps=25.0):
    """
    Optimiertes Video Loading - nur die benötigten Frames
    Gibt ComfyUI IMAGE Format zurück: (frames, height, width, channels)
    """
    print(f"🎬 Loading video optimized: {Path(video_path).name}")
    print(f"   Duration: {duration}s @ {target_fps} FPS")
    
    frames = []
    max_frames = int(duration * target_fps)
    frame_step = 1
    
    with av.open(video_path) as container:
        stream = container.streams.video[0]
        orig_fps = float(stream.average_rate)
        
        # Calculate frame step to get target fps
        frame_step = max(1, int(orig_fps / target_fps))
        
        print(f"   Original FPS: {orig_fps:.1f}, Frame step: {frame_step}")
        
        frame_count = 0
        for packet in container.demux(stream):
            for frame in packet.decode():
                if frame_count % frame_step == 0:
                    if len(frames) >= max_frames:
                        break
                    
                    # Convert to RGB numpy array
                    frame_np = frame.to_ndarray(format='rgb24')
                    frames.append(frame_np)
                
                frame_count += 1
                
                if len(frames) >= max_frames:
                    break
            
            if len(frames) >= max_frames:
                break
    
    if not frames:
        raise ValueError(f"No frames could be loaded from {video_path}")
    
    # Convert to ComfyUI format: (frames, height, width, channels)
    video_tensor = torch.from_numpy(np.stack(frames)).float() / 255.0
    
    print(f"   ✅ Loaded {len(frames)} frames, shape: {video_tensor.shape}")
    return video_tensor

def get_video_duration(video_path):
    """Ermittelt die Duration eines Videos in Sekunden"""
    try:
        with av.open(video_path) as container:
            stream = container.streams.video[0]
            duration = float(stream.duration * stream.time_base)
            return duration
    except Exception as e:
        print(f"⚠️  Konnte Video-Duration nicht ermitteln: {e}")
        print("   Verwende Standard-Duration von 8.0s")
        return 8.0

def load_video_cached(video_path, duration=8.0, target_fps=25.0):
    """Lädt Video mit Cache - Memory + Disk Cache"""
    cache_key = get_video_cache_key(video_path, duration, target_fps)
    
    # 1. Memory Cache Check
    if cache_key in VIDEO_CACHE:
        print(f"⚡ Video from memory cache: {Path(video_path).name}")
        return VIDEO_CACHE[cache_key]
    
    # 2. Disk Cache Check
    cache_file = CACHE_DIR / f"{cache_key}.pkl"
    if cache_file.exists():
        print(f"📁 Video from disk cache: {cache_file.name}")
        try:
            with open(cache_file, 'rb') as f:
                video_tensor = pickle.load(f)
            VIDEO_CACHE[cache_key] = video_tensor
            return video_tensor
        except Exception as e:
            print(f"   ⚠️  Cache file corrupted, reloading: {e}")
    
    # 3. Load and Cache
    video_tensor = load_video_optimized(video_path, duration, target_fps)
    
    # Cache to memory and disk
    VIDEO_CACHE[cache_key] = video_tensor
    CACHE_DIR.mkdir(exist_ok=True)
    try:
        with open(cache_file, 'wb') as f:
            pickle.dump(video_tensor, f)
        print(f"💾 Video cached to: {cache_file.name}")
    except Exception as e:
        print(f"   ⚠️  Could not cache to disk: {e}")
    
    return video_tensor

def convert_tensor_for_api(video_tensor):
    """Konvertiert Video Tensor für ComfyUI API"""
    # ComfyUI erwartet das Tensor als nested list
    return video_tensor.numpy().tolist()

def get_user_inputs():
    """Sammelt Benutzereingaben für Prompt, Negative Prompt und Seed"""
    print("\n=== MMAudio Interactive Parameters ===")
    print("Geben Sie die Parameter für die Audiogenerierung ein:\n")

    prompt = input("🎵 Prompt (Default: \"\"): ").strip()
    negative_prompt = input("❌ Negative Prompt (Default: \"voices, music\"): ").strip()
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
    print(f"   ⏱️  Duration: Wird automatisch aus Video ermittelt\n")
    
    return prompt, negative_prompt, seed

print(f"🌐 Connecting to ComfyUI via SSH tunnel: {HOST}")

# Get user inputs first
prompt, negative_prompt, seed = get_user_inputs()

# Test connection first
print("🔗 Testing connection...")
try:
    test_response = session.get(f"{HOST}/system_stats", timeout=30)
    test_response.raise_for_status()
    print("✅ Connection successful!")
except requests.exceptions.RequestException as e:
    print(f"❌ Connection failed: {e}")
    exit(1)

# Check if video file exists
if not os.path.exists(VIDEO_PATH):
    print(f"❌ Video file not found: {VIDEO_PATH}")
    exit(1)

print(f"📹 Video file found: {os.path.basename(VIDEO_PATH)} ({os.path.getsize(VIDEO_PATH) / 1024 / 1024:.1f} MB)")

# ========== OPTIMIZED VIDEO PREPROCESSING ==========
# Instead of uploading and using VHS_LoadVideo, we preprocess the video
# and inject it directly into the MMAudioSampler

print(f"\n🚀 Starting optimized video preprocessing...")

# Ermittle Video Duration automatisch
duration = get_video_duration(VIDEO_PATH)
print(f"📏 Detected video duration: {duration:.2f}s")

start_preprocess_time = time.time()

# Load and preprocess video optimally
video_tensor = load_video_cached(VIDEO_PATH, duration=duration)
preprocess_time = time.time() - start_preprocess_time
print(f"✅ Video preprocessing completed in {preprocess_time:.2f}s")

# 2) Load your workflow JSON
print("📋 Loading workflow...")
workflow_path = "comfyUI_workflows/mmaudio_test-API(fp16).json"
if not os.path.exists(workflow_path):
    print(f"❌ Workflow file not found: {workflow_path}")
    exit(1)

wf = json.load(open(workflow_path, "r", encoding="utf-8"))
print("✅ Workflow loaded successfully!")

# Remove VHS_LoadVideo and VHS_VideoInfo nodes - we're bypassing them!
nodes_to_remove = []
for node_id, node_data in wf.items():
    if node_data.get("class_type") in ["VHS_LoadVideo", "VHS_LoadVideoFFmpeg", "VHS_VideoInfo"]:
        nodes_to_remove.append(node_id)
        print(f"🗑️  Removing {node_data.get('class_type')} node {node_id}")

for node_id in nodes_to_remove:
    del wf[node_id]

# Update MMAudioSampler parameters - Node 92
if "92" in wf and wf["92"]["class_type"] == "MMAudioSampler":
    # Inject our preprocessed video directly
    wf["92"]["inputs"]["prompt"] = prompt
    wf["92"]["inputs"]["negative_prompt"] = negative_prompt
    wf["92"]["inputs"]["seed"] = seed
    wf["92"]["inputs"]["duration"] = duration
    
    # Convert video tensor to format ComfyUI API can handle
    print("📦 Converting video tensor for API...")
    wf["92"]["inputs"]["images"] = convert_tensor_for_api(video_tensor)
    
    print(f"🎵 Updated MMAudioSampler with optimized parameters:")
    print(f"   Prompt: '{prompt}'")
    print(f"   Negative Prompt: '{negative_prompt}'")
    print(f"   Seed: {seed}")
    print(f"   Duration: {duration}s")
    print(f"   Video tensor shape: {video_tensor.shape}")
else:
    print("❌ Could not find MMAudioSampler node in workflow")
    exit(1)

# 3) Kick off the job - ComfyUI expects workflow wrapped in "prompt" key
print("🚀 Submitting workflow to ComfyUI...")
# ComfyUI API requires the workflow to be wrapped in a "prompt" object
prompt_data = {
    "prompt": wf,
    "client_id": "python_client"
}
pr = session.post(f"{HOST}/prompt", json=prompt_data, timeout=30)
pr.raise_for_status()
prompt_id = pr.json()["prompt_id"]
print(f"✅ Workflow submitted! Job ID: {prompt_id}")

# 4) Poll for completion
print("⏳ Processing... (this may take several minutes)")
start_time = time.time()
while True:
    hr = session.get(f"{HOST}/history/{prompt_id}")
    hr.raise_for_status()
    hist = hr.json()
    
    # ComfyUI returns a complex status object, not just a string
    entry = hist.get(prompt_id, {})
    status_obj = entry.get("status", {})
    
    # Check if it's the new format with status_str and completed fields
    if isinstance(status_obj, dict):
        status_str = status_obj.get("status_str", "unknown")
        completed = status_obj.get("completed", False)
        
        elapsed = time.time() - start_time
        print(f"   Status: {status_str} (elapsed: {elapsed:.1f}s)", end='\r')
        
        if completed and status_str == "success":
            print(f"\n✅ Processing completed successfully! (took {elapsed:.1f}s)")
            break
        elif status_str == "error":
            print(f"\n❌ Processing failed after {elapsed:.1f}s")
            print(f"Error details: {status_obj}")
            raise RuntimeError(hist)
    else:
        # Fallback for old format (simple string)
        elapsed = time.time() - start_time
        print(f"   Status: {status_obj if status_obj else 'processing'} (elapsed: {elapsed:.1f}s)", end='\r')
        
        if status_obj == "completed":
            print(f"\n✅ Processing completed! (took {elapsed:.1f}s)")
            break
        elif status_obj == "error":
            print(f"\n❌ Processing failed after {elapsed:.1f}s")
            raise RuntimeError(hist)
    
    time.sleep(2)  # Check every 2 seconds

# 5) Find the saved FLAC from SaveAudio node (107)
entry = hist[prompt_id]
outputs = entry.get("outputs", {})

# Look specifically for SaveAudio node (107) output
save_audio_node = outputs.get("107", {})
files = save_audio_node.get("gltf", [])  # SaveAudio might use different key

# If not found, try common output keys
if not files:
    files = save_audio_node.get("files", [])
if not files:
    files = save_audio_node.get("audio", [])

# Debug: print the actual output structure
print(f"🔍 Debug - Available outputs: {list(outputs.keys())}")
if "107" in outputs:
    print(f"🔍 Debug - SaveAudio node keys: {list(save_audio_node.keys())}")
    print(f"🔍 Debug - SaveAudio content: {save_audio_node}")

# Fallback: search all nodes for any file output
file_info = None
if files and len(files) > 0:
    file_info = files[0]
else:
    # Search all nodes for file outputs
    for node_id, node_output in outputs.items():
        for key, value in node_output.items():
            if isinstance(value, list) and len(value) > 0 and isinstance(value[0], dict):
                if "filename" in value[0]:
                    file_info = value[0]
                    print(f"🔍 Found file in node {node_id}, key {key}: {file_info}")
                    break
        if file_info:
            break

if not file_info:
    print("❌ No output file found in history")
    print(f"Available outputs structure: {outputs}")
    raise RuntimeError("No output file in history")

# 6) Download it
print("📥 Downloading generated audio...")
fn = file_info["filename"]; sf = file_info.get("subfolder","")
audio = session.get(f"{HOST}/view", params={"filename": fn, "subfolder": sf, "type": "output"}, timeout=60)
audio.raise_for_status()

output_path = f"comfyUI_outputs/output_{prompt_id}.flac"
pathlib.Path(output_path).write_bytes(audio.content)
print(f"✅ Audio saved as: {output_path}")
print(f"📊 File size: {len(audio.content) / 1024:.1f} KB")
