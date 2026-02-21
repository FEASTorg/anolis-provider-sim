#!/usr/bin/env python3
"""
Sim Mode Example: FluxGraph external simulation.

Demonstrates:
- FluxGraph server integration
- External physics computation
- Device coupling through simulation
- Signal transforms (noise, scaling)
- Safety rules from simulation
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


def find_fluxgraph_server():
    """Find FluxGraph server executable"""
    candidates = [
        Path("../../../fluxgraph/build-server/server/Release/fluxgraph-server.exe"),
        Path(
            "../../../fluxgraph/build-release-server/server/Release/fluxgraph-server.exe"
        ),
        Path("../../../fluxgraph/build/server/Release/fluxgraph-server.exe"),
        Path("../../../fluxgraph/build-server/server/fluxgraph-server"),
        Path("../../../fluxgraph/build/server/fluxgraph-server"),
    ]
    for path in candidates:
        if path.exists():
            return str(path.resolve())
    return None


def find_provider():
    """Find provider executable"""
    candidates = [
        Path(__file__).parent.parent.parent
        / "build"
        / "Release"
        / "anolis-provider-sim.exe",
        Path(__file__).parent.parent.parent / "build" / "anolis-provider-sim.exe",
    ]
    for path in candidates:
        if path.exists():
            return str(path)
    return None


def run_sim_example():
    # Start FluxGraph server
    print("[0] Starting FluxGraph server...")
    server_exe = find_fluxgraph_server()
    if not server_exe:
        print("[FAIL] FluxGraph server not found. Build it with:")
        print("  cd d:\\repos_feast\\fluxgraph")
        print("  .\\scripts\\build.ps1 -Server -Release")
        print("\nAlternatively, start FluxGraph server manually:")
        print("  fluxgraph-server.exe --port 50051")
        print("\nThen run this test again.")
        return False

    server = subprocess.Popen(
        [server_exe, "--port", "50051"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )

    try:
        time.sleep(2.0)  # Let server start and bind port
        print("[OK] FluxGraph server running on localhost:50051")

        # Start provider (connects to FluxGraph)
        print("\n[1] Starting provider-sim with FluxGraph integration...")
        provider_exe = find_provider()
        if not provider_exe:
            print("[FAIL] Provider executable not found")
            print("Solution: Build with: .\\scripts\\build.ps1 -Release")
            return False

        provider = subprocess.Popen(
            [
                provider_exe,
                "--config",
                "provider.yaml",
                "--sim-server",
                "localhost:50051",
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        time.sleep(1.0)  # Let provider connect

        # Check if provider is still running
        if provider.poll() is not None:
            stderr_output = provider.stderr.read().decode()
            print("[FAIL] Provider terminated unexpectedly:")
            print(stderr_output)
            return False

        print("[OK] Provider connected to FluxGraph")

        # Test: Temperature convergence with external physics
        print("\n[2] Testing FluxGraph thermal simulation...")
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

        # Monitor temperature (FluxGraph computes physics)
        temps = []
        for i in range(10):
            time.sleep(1.5)
            req = Request(request_id=10 + i)
            req.read_signals.device_id = "chamber"
            req.read_signals.signal_ids.extend(["tc1_temp", "tc2_temp", "relay1_state"])
            resp = send_recv(provider, req)

            if resp.status.code != 1:
                print(f"[FAIL] ReadSignals failed: {resp.status.message}")
                return False

            if len(resp.read_signals.values) < 3:
                print(f"[FAIL] Expected 3 signals, got {len(resp.read_signals.values)}")
                return False

            # Extract values by signal_id
            values = {v.signal_id: v for v in resp.read_signals.values}

            tc1 = values["tc1_temp"].value.double_value
            tc2 = values["tc2_temp"].value.double_value
            relay = values["relay1_state"].value.bool_value

            temps.append(tc1)
            print(
                f"  t={i * 1.5:.1f}s: TC1={tc1:.1f}degC, TC2={tc2:.1f}degC, relay={'ON' if relay else 'OFF'}"
            )

        # Verify convergence
        if temps[-1] <= temps[0]:
            print("[FAIL] Temperature did not increase (FluxGraph physics issue)")
            print(f"  Initial: {temps[0]:.1f}degC, Final: {temps[-1]:.1f}degC")
            return False

        if temps[-1] < 60.0:
            print(f"[FAIL] Should reach >60degC, got {temps[-1]:.1f}degC")
            return False

        print("[OK] FluxGraph thermal physics working!")

        print("\n[OK] Sim mode with FluxGraph integration successful!")
        return True

    except Exception as e:
        print(f"\n[FAIL] Exception: {e}")
        import traceback

        traceback.print_exc()
        return False
    finally:
        # Cleanup
        try:
            provider.stdin.close()
            provider.terminate()
            provider.wait(timeout=2)
        except Exception:
            pass

        try:
            server.terminate()
            server.wait(timeout=2)
        except Exception:
            pass


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


if __name__ == "__main__":
    print("=" * 60)
    print("Sim Mode Example (FluxGraph Integration)")
    print("=" * 60)
    print("\nUse Case: Advanced physics with external simulation")
    print("Benefits: Device coupling, complex models, safety rules\n")

    success = run_sim_example()
    sys.exit(0 if success else 1)
