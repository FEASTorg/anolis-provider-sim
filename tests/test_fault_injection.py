#!/usr/bin/env python3
"""
Fault Injection Tests for anolis-provider-sim.

Tests the chaos_control device's fault injection API:
- inject_device_unavailable: Device appears unavailable for duration
- inject_signal_fault: Signal reports FAULT quality for duration
- inject_call_latency: Adds artificial latency to function calls
- inject_call_failure: Makes functions fail probabilistically
- clear_faults: Removes all active fault injections

These tests validate fault handling, error recovery, and resilience testing capabilities.
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
    from protocol_pb2 import Request, Response, Value, ValueType, SignalValue
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print(
        "Run: ./scripts/generate_python_proto.sh (Linux/macOS) or pwsh ./scripts/generate_python_proto.ps1 (Windows)",
        file=sys.stderr,
    )
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

        # Wait for provider to be ready with Hello handshake
        self._wait_for_ready()

    def _wait_for_ready(self):
        """Send Hello request to ensure provider is ready."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.hello.protocol_version = "v1"
        req.hello.client_name = "fault-injection-test"
        req.hello.client_version = "0.0.1"
        resp = self.send_request(req)
        if resp.status.code != 1:
            raise RuntimeError(f"Hello handshake failed: {resp.status.message}")

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

    def describe_device(self, device_id):
        """Call DescribeDevice."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.describe_device.device_id = device_id
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
        """Close the provider process."""
        self.proc.stdin.close()
        self.proc.wait(timeout=1)


def make_string_value(s):
    """Create a string Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_STRING
    v.string_value = s
    return v


def make_int64_value(i):
    """Create an int64 Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_INT64
    v.int64_value = i
    return v


def make_double_value(d):
    """Create a double Value."""
    v = Value()
    v.type = ValueType.VALUE_TYPE_DOUBLE
    v.double_value = d
    return v


def find_executable():
    """Find the provider executable in common build locations."""
    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        env_candidate = Path(env_path)
        if env_candidate.exists():
            return env_candidate
        print(
            f"ERROR: ANOLIS_PROVIDER_SIM_EXE points to missing file: {env_candidate}",
            file=sys.stderr,
        )
        sys.exit(1)

    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),
        Path("build/anolis-provider-sim"),
        Path("build/Debug/anolis-provider-sim.exe"),
        Path("build-tsan/anolis-provider-sim"),
    ]

    for path in candidates:
        if path.exists():
            return path

    print("ERROR: Could not find anolis-provider-sim executable", file=sys.stderr)
    print("Expected one of:", file=sys.stderr)
    for c in candidates:
        print(f"  {c}", file=sys.stderr)
    sys.exit(1)


# ============================================================================
# Fault Injection Test Functions
# ============================================================================


def test_device_unavailable(client):
    """Test inject_device_unavailable fault injection."""
    print("\n=== Test: Device Unavailable Injection ===")

    # Verify tempctl0 is initially available
    resp = client.describe_device("tempctl0")
    assert resp.status.code == 1, "Device should be available initially"
    initial_signal_count = len(resp.describe_device.capabilities.signals)
    print(f"  Initial state: Device available with {initial_signal_count} signals")

    # Inject device unavailable fault (500ms duration)
    print("  Injecting device_unavailable fault (500ms)...")
    resp = client.call_function(
        "chaos_control",
        1,  # inject_device_unavailable
        {
            "device_id": make_string_value("tempctl0"),
            "duration_ms": make_int64_value(500),
        },
    )
    assert resp.status.code == 1, f"Inject failed: {resp.status.message}"

    # Verify device now appears unavailable (returns error)
    resp = client.describe_device("tempctl0")
    # Device unavailable returns error (CODE_INVALID_ARGUMENT=11 or CODE_INTERNAL=7)
    assert resp.status.code != 1, "Device should return error when unavailable"
    print(f"  [OK] Device now unavailable (returns error: {resp.status.message})")

    # Verify ReadSignals returns error
    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert resp.status.code != 1, "ReadSignals should fail on unavailable device"
    print(f"  [OK] ReadSignals fails: {resp.status.message}")

    # Verify CallFunction fails
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    assert resp.status.code != 1, "CallFunction should fail on unavailable device"
    print(f"  [OK] CallFunction fails: {resp.status.message}")

    # Wait for fault to expire
    print("  Waiting for fault to expire...")
    time.sleep(0.6)  # 600ms > 500ms

    # Verify device is available again
    resp = client.describe_device("tempctl0")
    assert resp.status.code == 1, "Device should be available after expiration"
    assert len(resp.describe_device.capabilities.signals) == initial_signal_count, (
        "Device should have original signal count"
    )
    print("  [OK] Device auto-recovered after expiration")

    print("OK: Device unavailable injection working correctly\n")
    return True


def test_signal_fault(client):
    """Test inject_signal_fault fault injection."""
    print("\n=== Test: Signal Fault Injection ===")

    # Read initial signal value and quality
    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert resp.status.code == 1
    assert len(resp.read_signals.values) == 1
    signal = resp.read_signals.values[0]
    initial_value = signal.value.double_value
    initial_quality = signal.quality
    print(
        f"  Initial: tc1_temp={initial_value:.1f}C, quality={SignalValue.Quality.Name(initial_quality)}"
    )

    # Inject signal fault (300ms duration)
    print("  Injecting signal fault on tc1_temp (300ms)...")
    resp = client.call_function(
        "chaos_control",
        2,  # inject_signal_fault
        {
            "device_id": make_string_value("tempctl0"),
            "signal_id": make_string_value("tc1_temp"),
            "duration_ms": make_int64_value(300),
        },
    )
    assert resp.status.code == 1, f"Inject failed: {resp.status.message}"

    # Verify signal quality is now FAULT
    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert resp.status.code == 1
    signal = resp.read_signals.values[0]
    assert signal.quality == SignalValue.Quality.QUALITY_FAULT, (
        f"Expected FAULT quality, got {SignalValue.Quality.Name(signal.quality)}"
    )
    faulted_value = signal.value.double_value
    print(
        f"  [OK] Quality is now {SignalValue.Quality.Name(signal.quality)}, value={faulted_value:.1f}C"
    )

    # Wait for fault to expire
    print("  Waiting for fault to expire...")
    time.sleep(0.4)  # 400ms > 300ms

    # Verify quality restored
    resp = client.read_signals("tempctl0", ["tc1_temp"])
    signal = resp.read_signals.values[0]
    recovered_quality = signal.quality
    print(f"  [OK] Quality restored to {SignalValue.Quality.Name(recovered_quality)}")
    assert recovered_quality != SignalValue.Quality.QUALITY_FAULT, (
        "Quality should not be FAULT after expiration"
    )

    print("OK: Signal fault injection working correctly\n")
    return True


def test_call_latency(client):
    """Test inject_call_latency fault injection."""
    print("\n=== Test: Call Latency Injection ===")

    # Measure baseline call latency
    start = time.time()
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    baseline_ms = (time.time() - start) * 1000
    assert resp.status.code == 1
    print(f"  Baseline call latency: {baseline_ms:.1f}ms")

    # Inject 200ms latency
    print("  Injecting 200ms call latency...")
    resp = client.call_function(
        "chaos_control",
        3,  # inject_call_latency
        {
            "device_id": make_string_value("tempctl0"),
            "latency_ms": make_int64_value(200),
        },
    )
    assert resp.status.code == 1

    # Measure latency with injection
    start = time.time()
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("closed")})
    injected_ms = (time.time() - start) * 1000
    assert resp.status.code == 1
    print(f"  Call latency with injection: {injected_ms:.1f}ms")

    # Verify latency was added (allow some tolerance for timing variance)
    added_latency = injected_ms - baseline_ms
    assert added_latency >= 180, (
        f"Expected ~200ms added latency, got {added_latency:.1f}ms"
    )
    print(f"  [OK] Added latency: {added_latency:.1f}ms (expected ~200ms)")

    # Clear fault and verify latency removed
    resp = client.call_function("chaos_control", 5, {})  # clear_faults
    assert resp.status.code == 1

    start = time.time()
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    cleared_ms = (time.time() - start) * 1000
    print(f"  [OK] Latency after clear: {cleared_ms:.1f}ms (baseline restored)")
    assert cleared_ms < baseline_ms + 50, "Latency should be removed after clear"

    print("OK: Call latency injection working correctly\n")
    return True


def test_call_failure(client):
    """Test inject_call_failure fault injection."""
    print("\n=== Test: Call Failure Injection ===")

    # Verify function works normally
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    assert resp.status.code == 1, "Function should succeed initially"
    print("  Initial state: Function succeeds")

    # Inject 100% failure rate (use function_id as string: "1" = set_mode)
    print("  Injecting 100% call failure rate on set_mode (function_id='1')...")
    resp = client.call_function(
        "chaos_control",
        4,  # inject_call_failure
        {
            "device_id": make_string_value("tempctl0"),
            "function_id": make_string_value("1"),
            "failure_rate": make_double_value(1.0),
        },
    )
    assert resp.status.code == 1

    # Verify function now fails
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("closed")})
    assert resp.status.code != 1, "Function should fail with 100% failure rate"
    print(f"  [OK] Function fails: {resp.status.message}")

    # Test 0% failure rate (should succeed)
    print("  Changing to 0% failure rate...")
    resp = client.call_function(
        "chaos_control",
        4,
        {
            "device_id": make_string_value("tempctl0"),
            "function_id": make_string_value("1"),
            "failure_rate": make_double_value(0.0),
        },
    )
    assert resp.status.code == 1

    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    assert resp.status.code == 1, "Function should succeed with 0% failure rate"
    print("  [OK] Function succeeds with 0% failure rate")

    # Clear fault
    resp = client.call_function("chaos_control", 5, {})  # clear_faults
    assert resp.status.code == 1

    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("closed")})
    assert resp.status.code == 1, "Function should succeed after clear"
    print("  [OK] Function succeeds after clear_faults")

    print("OK: Call failure injection working correctly\n")
    return True


def test_clear_faults(client):
    """Test clear_faults clears all active faults."""
    print("\n=== Test: Clear All Faults ===")

    # Inject multiple faults
    print("  Injecting multiple faults...")

    # Device unavailable
    resp = client.call_function(
        "chaos_control",
        1,
        {
            "device_id": make_string_value("tempctl0"),
            "duration_ms": make_int64_value(10000),  # Long duration
        },
    )
    assert resp.status.code == 1

    # Call latency on motorctl0
    resp = client.call_function(
        "chaos_control",
        3,
        {
            "device_id": make_string_value("motorctl0"),
            "latency_ms": make_int64_value(500),
        },
    )
    assert resp.status.code == 1

    # Verify faults are active
    resp = client.describe_device("tempctl0")
    assert resp.status.code != 1, "Device should return error when unavailable"
    print("  [OK] Faults confirmed active")

    # Clear all faults
    print("  Calling clear_faults...")
    resp = client.call_function("chaos_control", 5, {})  # clear_faults
    assert resp.status.code == 1

    # Verify all faults cleared
    resp = client.describe_device("tempctl0")
    assert len(resp.describe_device.capabilities.signals) > 0, (
        "Device should be available"
    )
    print("  [OK] Device unavailable fault cleared")

    # Test motorctl0 latency cleared (baseline timing)
    start = time.time()
    resp = client.call_function(
        "motorctl0",
        1,
        {"motor_index": make_int64_value(1), "duty": make_double_value(0.5)},
    )
    latency_ms = (time.time() - start) * 1000
    assert latency_ms < 100, f"Latency should be cleared, got {latency_ms:.1f}ms"
    print(f"  [OK] Call latency cleared (latency: {latency_ms:.1f}ms)")

    print("OK: Clear faults working correctly\n")
    return True


def test_multiple_devices(client):
    """Test fault isolation between multiple devices."""
    print("\n=== Test: Fault Isolation Between Devices ===")

    # Inject fault on tempctl0
    print("  Injecting fault on tempctl0...")
    resp = client.call_function(
        "chaos_control",
        1,
        {
            "device_id": make_string_value("tempctl0"),
            "duration_ms": make_int64_value(500),
        },
    )
    assert resp.status.code == 1

    # Verify tempctl0 unavailable (returns error)
    resp = client.describe_device("tempctl0")
    assert resp.status.code != 1, "tempctl0 should return error when unavailable"
    print("  [OK] tempctl0 unavailable")

    # Verify motorctl0 still available
    resp = client.describe_device("motorctl0")
    assert resp.status.code == 1, "motorctl0 should be available"
    assert len(resp.describe_device.capabilities.signals) > 0, (
        "motorctl0 should have signals"
    )
    print("  [OK] motorctl0 still available (fault isolated)")

    # Verify relayio0 still available
    resp = client.describe_device("relayio0")
    assert resp.status.code == 1, "relayio0 should be available"
    assert len(resp.describe_device.capabilities.signals) > 0, (
        "relayio0 should have signals"
    )
    print("  [OK] relayio0 still available (fault isolated)")

    # Clear and verify
    resp = client.call_function("chaos_control", 5, {})
    resp = client.describe_device("tempctl0")
    assert resp.status.code == 1, "tempctl0 should be available after clear"
    assert len(resp.describe_device.capabilities.signals) > 0, (
        "tempctl0 should have signals after clear"
    )
    print("  [OK] All devices available after clear")

    print("OK: Fault isolation working correctly\n")
    return True


# ============================================================================
# Main Test Runner
# ============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="Fault injection tests for anolis-provider-sim"
    )
    parser.add_argument(
        "--test",
        default="all",
        choices=[
            "all",
            "device_unavailable",
            "signal_fault",
            "call_latency",
            "call_failure",
            "clear_faults",
            "multiple_devices",
        ],
        help="Test to run",
    )
    args = parser.parse_args()

    exe_path = find_executable()
    print(f"Testing: {exe_path}")

    # Use provider-sim.yaml config
    config_path = repo_root / "config" / "provider-sim.yaml"
    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        sys.exit(1)

    client = ProviderClient(exe_path, config_path)

    try:
        tests = {
            "device_unavailable": test_device_unavailable,
            "signal_fault": test_signal_fault,
            "call_latency": test_call_latency,
            "call_failure": test_call_failure,
            "clear_faults": test_clear_faults,
            "multiple_devices": test_multiple_devices,
        }

        if args.test == "all":
            print("Running all fault injection tests...\n")
            results = []
            for name, test_func in tests.items():
                # Create fresh client for each test to avoid connection issues
                test_client = ProviderClient(exe_path, config_path)
                try:
                    test_func(test_client)
                    results.append((name, True))
                except Exception as e:
                    print(f"FAIL: {name} - {e}\n", file=sys.stderr)
                    results.append((name, False))
                finally:
                    try:
                        test_client.close()
                    except Exception:
                        pass  # Ignore close errors

            print("\n" + "=" * 50)
            print("Test Summary:")
            for name, passed in results:
                status = "PASS" if passed else "FAIL"
                print(f"  {status}: {name}")

            all_passed = all(r[1] for r in results)
            if all_passed:
                print("\nAll fault injection tests passed!")
                return 0
            else:
                print("\nSome tests failed")
                return 1
        else:
            tests[args.test](client)
            print(f"\nOK: Test '{args.test}' passed!")
            return 0

    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
