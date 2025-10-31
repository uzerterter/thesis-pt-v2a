"""
End-to-End Test: Audio Generation + PTSL Import
Tests the complete workflow from video to Pro Tools timeline
"""

import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from ptsl_integration.ptsl_client import PTSLClient

def test_full_workflow():
    """Test complete workflow: Generate audio and import to Pro Tools"""
    
    print("="*60)
    print("PTSL Integration Test")
    print("="*60)
    
    # Step 1: Connect to Pro Tools
    print("\n1. Connecting to Pro Tools PTSL...")
    client = PTSLClient()
    
    if not client.connect():
        print("❌ Failed to connect to Pro Tools")
        print("\nMake sure:")
        print("  - Pro Tools is running")
        print("  - PTSL is enabled (Setup > Preferences > MIDI)")
        print("  - A session is open")
        return False
    
    print("✅ Connected successfully!")
    
    # Step 2: Test with a dummy audio file (for now)
    print("\n2. Testing audio import...")
    
    # Use a test audio file from temp directory if available
    test_audio = Path("C:/Users/LUDENB~1/AppData/Local/Temp/pt_v2a_outputs").glob("*.flac")
    test_audio_file = next(test_audio, None)
    
    if not test_audio_file:
        print("⚠️  No test audio file found in temp directory")
        print("   Run the plugin to generate audio first, or provide a test file")
        client.disconnect()
        return False
    
    print(f"   Using: {test_audio_file}")
    
    result = client.import_audio_to_timeline(
        audio_file_path=str(test_audio_file),
        location="SessionStart",
        destination="NewTrack"
    )
    
    if result:
        print(f"✅ {result}")
    else:
        print("❌ Import failed")
        client.disconnect()
        return False
    
    # Step 3: Cleanup
    print("\n3. Disconnecting...")
    client.disconnect()
    
    print("\n" + "="*60)
    print("✅ All tests passed!")
    print("="*60)
    return True


if __name__ == "__main__":
    success = test_full_workflow()
    sys.exit(0 if success else 1)
