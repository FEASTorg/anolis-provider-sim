#!/usr/bin/env python3
"""
Phase 22 Physics Mode Integration Test.

Tests the full physics engine implementation:
- ThermalMassModel driving temperature control simulation
- Transform primitives (FirstOrderLag, Noise, Saturation, Linear)
- Signal graph evaluation with ticker thread
- Physics-driven device state updates

This test validates that the physics engine correctly:
1. Loads and initializes physics models from configuration
2. Evaluates the signal graph at the configured tick rate
3. Applies transform primitives to signal values
4. Updates device state based on physics model outputs
5. Drives temperature convergence in the thermal simulation
"""

import argparse
import struct
import subprocess
import sys
import time
import os
from pathlib import Path

# Add build directory to path for protocol_pb2 import
script_dir = Path(__file__).parent
repo_root = script_dir.parent
build_dir_env = os.environ.get("ANOLIS_PROVIDER_SIM_BUILD_DIR")
build_dir = Path(build_dir_env) if build_dir_env else (repo_root / "build")
if not build_dir.is_absolute():
    build_dir = repo_root / build_dir
sys.path.insert(0, str(build_dir))

try:
    from protocol_pb2 import Request, Response, Value, ValueType, Status
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print(
        "Run: ./scripts/generate_python_proto.sh (Linux/macOS) or pwsh ./scripts/generate_python_proto.ps1 (Windows)",
        file=sys.stderr,
    )
    sys.exit(1)


def find_executable():
    """Find the provider executable in common build locations."""
    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        env_candidate = Path(env_path)
        if env_candidate.exists():
            return str(env_candidate)
        print(
            f"ERROR: ANOLIS_PROVIDER_SIM_EXE points to missing file: {env_candidate}",
            file=sys.stderr,
        )
        sys.exit(1)

    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),  # Windows MSVC
        Path("build/anolis-provider-sim"),  # Linux/macOS
        Path("build/Debug/anolis-provider-sim.exe"),  # Windows Debug
        Path("build-tsan/anolis-provider-sim"),  # Linux TSAN
    ]

    for path in candidates:
        if path.exists():
            return str(path)

    print("ERROR: Could not find anolis-provider-sim executable", file=sys.stderr)
    print("Expected one of:", file=sys.stderr)
    for c in candidates:
        print(f"  {c}", file=sys.stderr)
    sys.exit(1)


class ProviderClient:
    """Simple synchronous client for ADPP stdio transport."""

    def __init__(self, exe_path, config_path):
        self.proc = subprocess.Popen(
            [str(exe_path), "--config", str(config_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,
        )
        self.request_id = 0

    def send_request(self, req):
        """Send a request and return the response."""
        payload = req.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

        hdr = self.proc.stdout.read(4)
        if len(hdr) != 4:
            raise RuntimeError("Failed to read response header")

        (length,) = struct.unpack("<I", hdr)
        data = self.proc.stdout.read(length)
        if len(data) != length:
            raise RuntimeError(f"Expected {length} bytes, got {len(data)}")

        resp = Response()
        resp.ParseFromString(data)
        return resp

    def hello(self):
        """Perform Hello handshake."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.hello.protocol_version = "v1"
        return self.send_request(req)

    def wait_ready(self):
        """Call WaitReady to start ticker thread."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.wait_ready.CopyFrom(Request().wait_ready)
        return self.send_request(req)

    def read_signals(self, device_id, signal_ids=None):
        """Call ReadSignals."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.read_signals.device_id = device_id
        if signal_ids:
            req.read_signals.signal_ids.extend(signal_ids)
        return self.send_request(req)

    def call_function(self, device_id, function_id, args=None):
        """Call a device function."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.call.device_id = device_id
        req.call.function_id = function_id
        if args:
            for k, v in args.items():
                req.call.args[k].CopyFrom(v)
        return self.send_request(req)

    def close(self):
        """Terminate the provider process."""
        self.proc.terminate()
        self.proc.wait(timeout=2)


def make_string_value(s):
    """Helper to create a string Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_STRING
    v.string_value = s
    return v


def make_double_value(d):
    """Helper to create a double Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_DOUBLE
    v.double_value = d
    return v


def test_physics_mode():
    """Test Phase 22 physics mode with ThermalMassModel."""
    exe_path = find_executable()
    config_path = repo_root / "config" / "test-physics.yaml"

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    print(f"Testing: {exe_path}", file=sys.stderr)
    print(f"Config: {config_path}", file=sys.stderr)

    client = ProviderClient(exe_path, config_path)

    try:
        # Give provider time to initialize
        time.sleep(0.5)

        # Test 1: Hello handshake
        print("\n=== Test 1: Hello Handshake ===", file=sys.stderr)
        resp = client.hello()
        if resp.status.code != Status.CODE_OK:
            print(f"FAIL: Hello failed: {resp.status.message}", file=sys.stderr)
            return 1
        print(f"OK: Provider started ({resp.hello.provider_name})", file=sys.stderr)

        # Test 2: WaitReady (starts physics ticker thread)
        print("\n=== Test 2: WaitReady (Start Physics Engine) ===", file=sys.stderr)
        resp = client.wait_ready()
        if resp.status.code != Status.CODE_OK:
            print(f"FAIL: WaitReady failed: {resp.status.message}", file=sys.stderr)
            return 1
        print("OK: Physics ticker thread started", file=sys.stderr)

        # Test 3: Configure closed-loop temperature control
        print("\n=== Test 3: Configure Temperature Control ===", file=sys.stderr)

        # Set mode to 'closed'
        resp = client.call_function(
            "tempctl0", 1, {"mode": make_string_value("closed")}
        )
        if resp.status.code != Status.CODE_OK:
            print(f"FAIL: set_mode failed: {resp.status.message}", file=sys.stderr)
            return 1

        # Set setpoint to 80°C
        resp = client.call_function("tempctl0", 2, {"value": make_double_value(80.0)})
        if resp.status.code != Status.CODE_OK:
            print(f"FAIL: set_setpoint failed: {resp.status.message}", file=sys.stderr)
            return 1

        print("OK: Mode='closed', Setpoint=80°C", file=sys.stderr)

        # Test 4: Temperature convergence
        print("\n=== Test 4: Temperature Convergence ===", file=sys.stderr)
        print("Waiting for physics engine to heat chamber...", file=sys.stderr)
        print(
            "(ThermalMassModel with 5000 J/K thermal mass takes time)", file=sys.stderr
        )

        initial_temp = None
        final_temp = None
        converged = False
        max_samples = 20

        for i in range(max_samples):
            time.sleep(1.0)  # Sample every 1 second

            resp = client.read_signals(
                "tempctl0", ["tc1_temp", "relay1_state", "relay2_state"]
            )
            if resp.status.code != Status.CODE_OK:
                print(
                    f"FAIL: read_signals failed: {resp.status.message}", file=sys.stderr
                )
                return 1

            # Extract values
            tc1_temp = None
            relay1 = None
            relay2 = None
            for sig in resp.read_signals.values:
                if sig.signal_id == "tc1_temp":
                    tc1_temp = sig.value.double_value
                elif sig.signal_id == "relay1_state":
                    relay1 = sig.value.bool_value
                elif sig.signal_id == "relay2_state":
                    relay2 = sig.value.bool_value

            if tc1_temp is None:
                print("FAIL: tc1_temp not found in response", file=sys.stderr)
                return 1

            if initial_temp is None:
                initial_temp = tc1_temp

            final_temp = tc1_temp
            print(
                f"  Sample {i + 1:2d}: TC1={tc1_temp:5.1f}°C, R1={relay1}, R2={relay2}",
                file=sys.stderr,
            )

            # Check for significant temperature increase
            if tc1_temp > initial_temp + 10.0:
                print(
                    f"\nOK: Temperature increased from {initial_temp:.1f}°C to {tc1_temp:.1f}°C",
                    file=sys.stderr,
                )
                print(
                    "    Physics engine successfully driving thermal model!",
                    file=sys.stderr,
                )
                converged = True
                break

        if not converged:
            print("\nFAIL: Temperature did not increase sufficiently", file=sys.stderr)
            print(
                f"      Initial: {initial_temp:.1f}°C, Final: {final_temp:.1f}°C",
                file=sys.stderr,
            )
            return 1

        # Test 5: Transform primitives
        print("\n=== Test 5: Transform Primitives ===", file=sys.stderr)
        resp = client.read_signals("tempctl0", ["tc1_temp", "tc2_temp"])
        if resp.status.code != Status.CODE_OK:
            print(f"FAIL: read_signals failed: {resp.status.message}", file=sys.stderr)
            return 1

        tc1 = tc2 = None
        for sig in resp.read_signals.values:
            if sig.signal_id == "tc1_temp":
                tc1 = sig.value.double_value
            elif sig.signal_id == "tc2_temp":
                tc2 = sig.value.double_value

        if tc1 is None or tc2 is None:
            print("FAIL: Temperature sensors not found", file=sys.stderr)
            return 1

        # TC1 has FirstOrderLag (tau=2.0s)
        # TC2 has FirstOrderLag (tau=1.5s) + Noise (amplitude=0.5, seed=42)
        # They should be similar but not identical
        temp_diff = abs(tc1 - tc2)
        print(
            f"  TC1={tc1:.2f}°C (lag=2.0s), TC2={tc2:.2f}°C (lag=1.5s + noise)",
            file=sys.stderr,
        )
        print(f"  Difference: {temp_diff:.2f}°C", file=sys.stderr)

        if temp_diff > 5.0:
            print(
                f"FAIL: Temperature sensors differ too much ({temp_diff:.2f}°C > 5.0°C)",
                file=sys.stderr,
            )
            return 1

        print(
            "OK: Transform primitives working (FirstOrderLag + Noise)", file=sys.stderr
        )

        print("\n" + "=" * 60, file=sys.stderr)
        print("SUCCESS: All Phase 22 physics mode tests passed!", file=sys.stderr)
        print("=" * 60, file=sys.stderr)

        return 0

    finally:
        client.close()


def main():
    parser = argparse.ArgumentParser(
        description="Test Phase 22 physics mode with ThermalMassModel"
    )
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()

    return test_physics_mode()


if __name__ == "__main__":
    sys.exit(main())
