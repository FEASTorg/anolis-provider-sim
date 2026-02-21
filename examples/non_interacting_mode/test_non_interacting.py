#!/usr/bin/env python3
"""
Non-Interacting Mode Example: Built-in first-order physics.

Demonstrates:
- Temperature convergence (heater -> setpoint)
- Motor speed ramp (duty -> target RPM)
- Independent device physics
- No external dependencies
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


def run_non_interacting_example():
    # Find provider executable
    provider_paths = [
        Path(__file__).parent.parent.parent
        / "build"
        / "Release"
        / "anolis-provider-sim.exe",
        Path(__file__).parent.parent.parent
        / "build-standalone"
        / "Release"
        / "anolis-provider-sim.exe",
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
        stderr=subprocess.PIPE,
    )

    try:
        time.sleep(0.5)  # Let provider start and ticker begin

        # Test 1: Temperature convergence
        print("[1] Testing temperature convergence...")
        print("  Setting mode=closed, setpoint=80degC\n")

        # Set closed-loop mode
        req = Request(request_id=1)
        req.call.device_id = "chamber"
        req.call.function_id = 1  # set_mode
        mode_arg = make_string("closed")
        req.call.args["mode"].CopyFrom(mode_arg)
        resp = send_recv(provider, req)

        if resp.status.code != 1:
            print(f"[FAIL] set_mode failed: {resp.status.message}")
            return False

        # Set setpoint
        req = Request(request_id=2)
        req.call.device_id = "chamber"
        req.call.function_id = 2  # set_setpoint
        sp_arg = make_double(80.0)
        req.call.args["value"].CopyFrom(sp_arg)
        resp = send_recv(provider, req)

        if resp.status.code != 1:
            print(f"[FAIL] set_setpoint failed: {resp.status.message}")
            return False

        # Read temperature over time
        temps = []
        for i in range(10):
            time.sleep(1.5)
            req = Request(request_id=10 + i)
            req.read_signals.device_id = "chamber"
            req.read_signals.signal_ids.append("tc1_temp")
            resp = send_recv(provider, req)

            if resp.status.code != 1:
                print(f"[FAIL] ReadSignals failed: {resp.status.message}")
                return False

            if not resp.read_signals.values:
                print("[FAIL] No signal values returned")
                return False

            temp = resp.read_signals.values[0].value.double_value
            temps.append(temp)
            print(f"  t={i * 1.5:.1f}s: temp={temp:.1f}degC")

        # Verify convergence
        if temps[-1] <= temps[0]:
            print(
                f"[FAIL] Temperature did not increase: {temps[0]:.1f} -> {temps[-1]:.1f}"
            )
            return False

        if temps[-1] < 60.0:
            print(f"[FAIL] Temperature should reach >60degC, got {temps[-1]:.1f}degC")
            return False

        print("[OK] Temperature converging to setpoint")

        # Test 2: Motor speed ramp
        print("\n[2] Testing motor speed ramp...")
        print("  Setting motor duty=0.5 (50%)\n")

        # Set motor duty
        req = Request(request_id=20)
        req.call.device_id = "motor"
        req.call.function_id = 10  # set_motor_duty

        motor_idx = make_int64(1)
        req.call.args["motor_index"].CopyFrom(motor_idx)

        duty = make_double(0.5)
        req.call.args["duty"].CopyFrom(duty)

        resp = send_recv(provider, req)

        if resp.status.code != 1:
            print(f"[FAIL] set_motor_duty failed: {resp.status.message}")
            return False

        # Read speed over time
        speeds = []
        for i in range(6):
            time.sleep(1.0)
            req = Request(request_id=30 + i)
            req.read_signals.device_id = "motor"
            req.read_signals.signal_ids.append("motor1_speed")
            resp = send_recv(provider, req)

            if resp.status.code != 1:
                print(f"[FAIL] ReadSignals failed: {resp.status.message}")
                return False

            if not resp.read_signals.values:
                print("[FAIL] No signal values returned")
                return False

            speed = resp.read_signals.values[0].value.double_value
            speeds.append(speed)
            print(f"  t={i}s: speed={speed:.0f} RPM")

        # Verify ramp
        if speeds[-1] <= speeds[0]:
            print(f"[FAIL] Speed did not increase: {speeds[0]:.0f} -> {speeds[-1]:.0f}")
            return False

        if speeds[-1] < 1000:
            print(f"[FAIL] Speed should reach >1000 RPM, got {speeds[-1]:.0f}")
            return False

        print("[OK] Motor speed ramping up")

        print("\n[OK] Non-interacting mode physics working!")
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


def make_string(s):
    v = Value()
    v.type = ValueType.VALUE_TYPE_STRING
    v.string_value = s
    return v


def make_double(d):
    v = Value()
    v.type = ValueType.VALUE_TYPE_DOUBLE
    v.double_value = d
    return v


def make_int64(i):
    v = Value()
    v.type = ValueType.VALUE_TYPE_INT64
    v.int64_value = i
    return v


if __name__ == "__main__":
    print("=" * 60)
    print("Non-Interacting Mode Example")
    print("=" * 60)
    print("\nUse Case: Standalone simulation with built-in physics")
    print("Benefits: No external dependencies, simple first-order dynamics\n")

    success = run_non_interacting_example()
    sys.exit(0 if success else 1)
