#!/usr/bin/env python3
"""
Rule Engine Integration Test.

Tests the automation rules engine implementation:
- Rule condition evaluation from signal registry
- Rule action execution via device function calls
- Integration with physics ticker callback mechanism

This test validates that the rules engine correctly:
1. Parses rule conditions from configuration
2. Evaluates conditions against live signal values
3. Triggers actions when conditions are met
4. Executes device function calls with correct parameters
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

    def call_function(self, device_id, function_id_or_name, args=None):
        """Call a device function by ID (uint32) or name (string)."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.call.device_id = device_id
        if isinstance(function_id_or_name, int):
            req.call.function_id = function_id_or_name
        else:
            req.call.function_id = 0  # When using name, ID can be 0
            req.call.function_name = function_id_or_name
        if args:
            for k, v in args.items():
                req.call.args[k].CopyFrom(v)
        return self.send_request(req)

    def close(self):
        """Terminate the provider process."""
        self.proc.terminate()
        self.proc.wait(timeout=2)


def make_int_value(i):
    """Helper to create an int64 Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_INT64
    v.int64_value = i
    return v


def make_bool_value(b):
    """Helper to create a bool Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_BOOL
    v.bool_value = b
    return v


def make_double_value(d):
    """Helper to create a double Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_DOUBLE
    v.double_value = d
    return v


def test_rule_trigger():
    """Test rules engine with temperature overheat protection rule."""
    exe_path = find_executable()
    config_path = repo_root / "config" / "test-rule-trigger.yaml"

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

        # Test 3: Turn on heater (relay 1)
        print("\n=== Test 3: Turn On Heater ===", file=sys.stderr)
        resp = client.call_function(
            "chamber",
            3,  # Function ID for set_relay
            {"relay_index": make_int_value(1), "state": make_bool_value(True)},
        )
        if resp.status.code != Status.CODE_OK:
            print(
                f"FAIL: set_relay failed: {resp.status.message}",
                file=sys.stderr,
            )
            return 1
        print("OK: Heater turned on (relay 1 = 1)", file=sys.stderr)

        # Wait for physics tick to propagate the value
        time.sleep(0.2)

        # Verify heater is on
        resp = client.read_signals("chamber", ["relay1_state"])
        if resp.status.code != Status.CODE_OK:
            print(
                f"FAIL: ReadSignals(chamber/relay1_state) failed: {resp.status.message}",
                file=sys.stderr,
            )
            return 1
        
        print(f"DEBUG: read_signals response: {resp}", file=sys.stderr)
        
        heater_state = None
        for sig in resp.read_signals.values:
            if sig.signal_id == "relay1_state":
                print(f"DEBUG: Found relay1_state signal, value type={sig.value.type}, bool_value={sig.value.bool_value}", file=sys.stderr)
                heater_state = sig.value.bool_value
                break
        if heater_state is None:
            print("FAIL: relay1_state not found in response", file=sys.stderr)
            return 1
        if heater_state != 1:
            print(
                f"FAIL: Heater not on after set_relay (relay1_state = {heater_state})",
                file=sys.stderr,
            )
            return 1
        print(f"OK: Heater confirmed on (relay1_state = {heater_state})", file=sys.stderr)

        # Test 4: Read initial temperature
        print("\n=== Test 4: Initial Temperature ===", file=sys.stderr)
        resp = client.read_signals("chamber", ["tc1_temp"])
        if resp.status.code != Status.CODE_OK:
            print(
                f"FAIL: ReadSignals(chamber/tc1_temp) failed: {resp.status.message}",
                file=sys.stderr,
            )
            return 1
        initial_temp = None
        for sig in resp.read_signals.values:
            if sig.signal_id == "tc1_temp":
                initial_temp = sig.value.double_value
                break
        if initial_temp is None:
            print("FAIL: tc1_temp not found in response", file=sys.stderr)
            return 1
        print(f"OK: Initial temperature = {initial_temp:.2f} C", file=sys.stderr)

        # Test 5: Wait for temperature to rise and rule to trigger
        print("\n=== Test 5: Wait for Rule Trigger ===", file=sys.stderr)
        print("Waiting for temperature to exceed 30.0 C threshold...", file=sys.stderr)
        
        max_wait_sec = 10.0
        check_interval_sec = 0.5
        elapsed = 0.0
        rule_triggered = False
        final_temp = initial_temp

        while elapsed < max_wait_sec:
            time.sleep(check_interval_sec)
            elapsed += check_interval_sec

            # Read current temperature
            resp = client.read_signals("chamber", ["tc1_temp"])
            if resp.status.code != Status.CODE_OK:
                print(
                    f"FAIL: ReadSignals(chamber/tc1_temp) failed: {resp.status.message}",
                    file=sys.stderr,
                )
                return 1
            for sig in resp.read_signals.values:
                if sig.signal_id == "tc1_temp":
                    final_temp = sig.value.double_value
                    break

            # Read heater state
            resp = client.read_signals("chamber", ["relay1_state"])
            if resp.status.code != Status.CODE_OK:
                print(
                    f"FAIL: ReadSignals(chamber/relay1_state) failed: {resp.status.message}",
                    file=sys.stderr,
                )
                return 1
            heater_state = None
            for sig in resp.read_signals.values:
                if sig.signal_id == "relay1_state":
                    heater_state = sig.value.bool_value
                    break

            print(
                f"  t={elapsed:.1f}s: temp={final_temp:.2f} C, heater={heater_state}",
                file=sys.stderr,
            )

            # Check if rule has triggered (heater turned off and temp > threshold)
            if not heater_state and final_temp > 30.0:
                rule_triggered = True
                print(
                    f"OK: Rule triggered! Heater shut down at {final_temp:.2f} C",
                    file=sys.stderr,
                )
                break

        if not rule_triggered:
            print(
                f"FAIL: Rule did not trigger within {max_wait_sec}s (final temp={final_temp:.2f} C)",
                file=sys.stderr,
            )
            return 1

        # Test 6: Verify final state
        print("\n=== Test 6: Verify Final State ===", file=sys.stderr)
        resp = client.read_signals("chamber", ["relay1_state"])
        if resp.status.code != Status.CODE_OK:
            print(
                f"FAIL: ReadSignals(chamber/relay1_state) failed: {resp.status.message}",
                file=sys.stderr,
            )
            return 1
        heater_state = None
        for sig in resp.read_signals.values:
            if sig.signal_id == "relay1_state":
                heater_state = sig.value.bool_value
                break

        resp = client.read_signals("chamber", ["tc1_temp"])
        if resp.status.code != Status.CODE_OK:
            print(
                f"FAIL: ReadSignals(chamber/tc1_temp) failed: {resp.status.message}",
                file=sys.stderr,
            )
            return 1
        final_temp = None
        for sig in resp.read_signals.values:
            if sig.signal_id == "tc1_temp":
                final_temp = sig.value.double_value
                break

        if heater_state:
            print(
                f"FAIL: Heater not off after rule trigger (relay1_state = {heater_state})",
                file=sys.stderr,
            )
            return 1

        if final_temp <= 30.0:
            print(
                f"FAIL: Temperature did not exceed threshold (temp = {final_temp:.2f} C)",
                file=sys.stderr,
            )
            return 1

        print(
            f"OK: Final state verified (temp={final_temp:.2f} C, heater=0)",
            file=sys.stderr,
        )

        print("\n=== SUCCESS: All rule trigger tests passed ===", file=sys.stderr)
        return 0

    except Exception as e:
        print(f"FAIL: Exception during test: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1
    finally:
        client.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test rules engine")
    args = parser.parse_args()

    sys.exit(test_rule_trigger())
