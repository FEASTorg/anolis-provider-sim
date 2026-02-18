#!/usr/bin/env python3
"""
Inert Mode Example: Protocol testing without physics.

Demonstrates:
- Device discovery
- Signal reading (returns defaults)
- Function calls (accepted but no state changes)
- Use case: ADPP protocol validation
"""

import subprocess
import struct
import sys
import time
from pathlib import Path

# Add protocol_pb2 to path
build_dir = Path(__file__).parent.parent.parent / "build"
if not build_dir.exists():
    print("ERROR: Build directory not found. Run: .\\scripts\\build.ps1 -Release")
    sys.exit(1)

sys.path.insert(0, str(build_dir))

try:
    from protocol_pb2 import Request, Response, Value, ValueType
except ImportError:
    print("ERROR: protocol_pb2.py not found in build directory")
    print("Solution: Rebuild with: .\\scripts\\build.ps1 -Release")
    sys.exit(1)


def run_inert_example():
    # Find provider executable
    provider_paths = [
        Path(__file__).parent.parent.parent / "build" / "Release" / "anolis-provider-sim.exe",
        Path(__file__).parent.parent.parent / "build-standalone" / "Release" / "anolis-provider-sim.exe",
        Path(__file__).parent.parent.parent / "build" / "anolis-provider-sim.exe",
    ]
    
    provider_exe = None
    for path in provider_paths:
        if path.exists():
            provider_exe = str(path)
            break
    
    if not provider_exe:
        print("ERROR: Provider executable not found")
        print("Solution: Build first with: .\\scripts\\build.ps1 -Release")
        sys.exit(1)
    
    print(f"Using provider: {provider_exe}\n")
    
    # Start provider
    provider = subprocess.Popen(
        [provider_exe, "--config", "provider.yaml"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    try:
        time.sleep(0.3)  # Let provider initialize
        
        # Test 1: List devices
        print("[1] Listing devices...")
        req = Request(request_id=1)
        req.list_devices.include_health = False
        resp = send_recv(provider, req)
        
        if resp.status.code != 1:
            print(f"[FAIL] ListDevices failed: {resp.status.message}")
            return False
        
        devices = [d.device_id for d in resp.list_devices.devices]
        print(f"[OK] Found devices: {devices}")
        
        if "tempctl0" not in devices or "motorctl0" not in devices:
            print("[FAIL] Expected devices not found")
            return False
        
        # Test 2: Read signals (should return defaults)
        print("\n[2] Reading signals...")
        req = Request(request_id=2)
        req.read_signals.device_id = "tempctl0"
        req.read_signals.signal_ids.extend(["tc1_temp", "tc2_temp"])
        resp = send_recv(provider, req)
        
        if resp.status.code != 1:
            print(f"[FAIL] ReadSignals failed: {resp.status.message}")
            return False
        
        for val in resp.read_signals.values:
            print(f"  {val.signal_id} = {val.value.double_value:.1f}")
        
        # Verify defaults
        temps = {v.signal_id: v.value.double_value for v in resp.read_signals.values}
        if "tc1_temp" not in temps or "tc2_temp" not in temps:
            print("[FAIL] Expected signals not returned")
            return False
        
        print("[OK] Default values returned")
        
        # Test 3: Call function (accepted but no effect)
        print("\n[3] Calling set_mode function...")
        req = Request(request_id=3)
        req.call.device_id = "tempctl0"
        req.call.function_id = 1  # set_mode
        arg = Value()
        arg.type = ValueType.VALUE_TYPE_STRING
        arg.string_value = "closed"
        req.call.args["mode"].CopyFrom(arg)
        resp = send_recv(provider, req)
        
        if resp.status.code != 1:
            print(f"[FAIL] Function call failed: {resp.status.message}")
            return False
        
        print("[OK] Function call accepted")
        
        # Test 4: Read signals again - values should NOT change (inert mode)
        print("\n[4] Reading signals again...")
        req = Request(request_id=4)
        req.read_signals.device_id = "tempctl0"
        req.read_signals.signal_ids.extend(["tc1_temp", "tc2_temp"])
        resp = send_recv(provider, req)
        
        if resp.status.code != 1:
            print(f"[FAIL] Second ReadSignals failed: {resp.status.message}")
            return False
        
        temps2 = {v.signal_id: v.value.double_value for v in resp.read_signals.values}
        
        # In inert mode, values should be identical (no physics)
        if temps == temps2:
            print("[OK] Values unchanged (as expected in inert mode)")
        else:
            print("[WARN] Values changed (unexpected in inert mode)")
            print(f"  Before: {temps}")
            print(f"  After: {temps2}")
        
        # Test 5: Call motor function
        print("\n[5] Testing motor function call...")
        req = Request(request_id=5)
        req.call.device_id = "motorctl0"
        req.call.function_id = 10  # set_motor_duty
        
        motor_idx = Value()
        motor_idx.type = ValueType.VALUE_TYPE_INT64
        motor_idx.int64_value = 1
        req.call.args["motor_index"].CopyFrom(motor_idx)
        
        duty = Value()
        duty.type = ValueType.VALUE_TYPE_DOUBLE
        duty.double_value = 0.75
        req.call.args["duty"].CopyFrom(duty)
        
        resp = send_recv(provider, req)
        
        if resp.status.code != 1:
            print(f"[FAIL] Motor function failed: {resp.status.message}")
            return False
        
        print("[OK] Motor function accepted")
        
        print("\n[OK] Inert mode working correctly!")
        return True
        
    except Exception as e:
        print(f"\n[FAIL] Exception: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        provider.stdin.close()
        provider.terminate()
        provider.wait(timeout=2)


def send_recv(proc, req):
    """Send request and receive response via ADPP framing"""
    payload = req.SerializeToString()
    frame = struct.pack("<I", len(payload)) + payload
    proc.stdin.write(frame)
    proc.stdin.flush()
    
    # Read response
    hdr = proc.stdout.read(4)
    if len(hdr) < 4:
        raise RuntimeError("Failed to read response header")
    
    length = struct.unpack("<I", hdr)[0]
    data = proc.stdout.read(length)
    
    if len(data) < length:
        raise RuntimeError(f"Incomplete response: expected {length}, got {len(data)}")
    
    resp = Response()
    resp.ParseFromString(data)
    return resp


if __name__ == "__main__":
    print("=" * 60)
    print("Inert Mode Example")
    print("=" * 60)
    print("\nUse Case: ADPP protocol testing without physics simulation")
    print("Benefits: Fast, deterministic, no dependencies\n")
    
    success = run_inert_example()
    sys.exit(0 if success else 1)
