#!/usr/bin/env python3
"""
ADPP Integration Tests for anolis-provider-sim.

Tests core ADPP v1 protocol operations and device simulation behaviors:
- ListDevices, DescribeDevice, ReadSignals, CallFunction RPCs
- Device state machines (tempctl PID control, motorctl speed control)
- Physics simulation convergence
- Precondition enforcement and error handling
"""

import argparse
import struct
import subprocess
import sys
import time
from pathlib import Path

# Add build directory to path for protocol_pb2 import
script_dir = Path(__file__).parent
repo_root = script_dir.parent
build_dir = repo_root / "build"
sys.path.insert(0, str(build_dir))

try:
    from protocol_pb2 import Request, Response, Value, ValueType
except ImportError:
    print("ERROR: protocol_pb2 module not found in build/", file=sys.stderr)
    print(
        "Run: protoc --python_out=build --proto_path=external/anolis/spec/device-provider external/anolis/spec/device-provider/protocol.proto",
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

    def list_devices(self):
        """Call ListDevices."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.list_devices.include_health = False
        return self.send_request(req)

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


def find_executable():
    """Find the provider executable."""
    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),  # Windows
        Path("build/anolis-provider-sim"),  # Linux
    ]
    for path in candidates:
        if path.exists():
            return path
    raise FileNotFoundError("Could not find anolis-provider-sim executable")


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


def make_int64_value(i):
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


def test_list_devices(client):
    """Test 1: Verify ListDevices returns expected devices from config."""
    print("\n=== Test 1: ListDevices ===")
    resp = client.list_devices()

    assert resp.status.code == 1, f"Expected CODE_OK, got {resp.status.code}"

    # provider-sim.yaml has 4 devices + sim_control = 5 total
    assert len(resp.list_devices.devices) == 5, (
        f"Expected 5 devices, got {len(resp.list_devices.devices)}"
    )

    device_ids = [d.device_id for d in resp.list_devices.devices]

    # Verify all expected devices are present
    expected_devices = [
        "tempctl0",
        "motorctl0",
        "relayio0",
        "analogsensor0",
        "sim_control",
    ]
    for device_id in expected_devices:
        assert device_id in device_ids, f"Missing {device_id}"

    print(f"OK: Found {len(resp.list_devices.devices)} devices: {device_ids}")
    return True


def test_describe_tempctl(client):
    """Test 2: Verify tempctl0 capabilities."""
    print("\n=== Test 2: DescribeDevice (tempctl0) ===")
    resp = client.describe_device("tempctl0")

    assert resp.status.code == 1, f"Expected CODE_OK, got {resp.status.code}"

    caps = resp.describe_device.capabilities
    signal_ids = [s.signal_id for s in caps.signals]
    function_ids = [f.function_id for f in caps.functions]

    print(f"  Signals ({len(signal_ids)}): {signal_ids}")
    print(f"  Functions ({len(function_ids)}): {function_ids}")

    # Verify expected signals
    assert "tc1_temp" in signal_ids
    assert "tc2_temp" in signal_ids
    assert "relay1_state" in signal_ids
    assert "relay2_state" in signal_ids
    assert "control_mode" in signal_ids
    assert "setpoint" in signal_ids

    # Verify expected functions
    assert 1 in function_ids  # set_mode
    assert 2 in function_ids  # set_setpoint
    assert 3 in function_ids  # set_relay

    print("OK: All expected signals and functions present")
    return True


def test_temp_convergence(client):
    """Test 3: Temperature convergence in closed-loop mode."""
    print("\n=== Test 3: Temperature Convergence ===")

    # Set mode to closed
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("closed")})
    assert resp.status.code == 1, "Failed to set mode to closed"
    print("  Mode set to 'closed'")

    # Set setpoint to 80C
    resp = client.call_function("tempctl0", 2, {"value": make_double_value(80.0)})
    assert resp.status.code == 1, "Failed to set setpoint"
    print("  Setpoint set to 80degC")

    # Read temperatures over time
    print("  Reading temperatures...")
    temps = []
    for i in range(10):
        resp = client.read_signals("tempctl0", ["tc1_temp", "tc2_temp"])
        assert resp.status.code == 1

        tc1 = next(
            (
                v.value.double_value
                for v in resp.read_signals.values
                if v.signal_id == "tc1_temp"
            ),
            None,
        )
        tc2 = next(
            (
                v.value.double_value
                for v in resp.read_signals.values
                if v.signal_id == "tc2_temp"
            ),
            None,
        )

        temps.append((tc1, tc2))
        print(f"    Read {i + 1}: TC1={tc1:.1f}degC, TC2={tc2:.1f}degC")
        time.sleep(1.5)

    # Verify convergence: last temp should be higher than first
    assert temps[-1][0] > temps[0][0], "TC1 temperature did not increase"
    assert temps[-1][1] > temps[0][1], "TC2 temperature did not increase"

    print(f"OK: Temperatures converging: {temps[0][0]:.1f} -> {temps[-1][0]:.1f}degC")
    return True


def test_motor_control(client):
    """Test 4: Motor duty/speed control."""
    print("\n=== Test 4: Motor Control ===")

    # Set motor 1 duty to 50%
    resp = client.call_function(
        "motorctl0",
        10,
        {"motor_index": make_int64_value(1), "duty": make_double_value(0.5)},
    )
    assert resp.status.code == 1, "Failed to set motor duty"
    print("  Motor 1 duty set to 0.5 (50%)")

    # Read speeds over time
    print("  Reading motor speeds...")
    speeds = []
    for i in range(6):
        resp = client.read_signals("motorctl0", ["motor1_speed"])
        assert resp.status.code == 1

        speed = next(
            (
                v.value.double_value
                for v in resp.read_signals.values
                if v.signal_id == "motor1_speed"
            ),
            0.0,
        )
        speeds.append(speed)
        print(f"    Read {i + 1}: Motor1={speed:.0f} RPM")
        time.sleep(1.0)

    # Verify speed increases
    assert speeds[-1] > speeds[0], "Motor speed did not increase"
    assert speeds[-1] > 1000, (
        f"Motor speed too low: {speeds[-1]:.0f} RPM (expected ~1600 RPM)"
    )

    print(f"OK: Motor speed ramping up: {speeds[0]:.0f} -> {speeds[-1]:.0f} RPM")
    return True


def test_relay_control(client):
    """Test 5: Relay control in open-loop mode."""
    print("\n=== Test 5: Relay Control (Open Loop) ===")

    # Ensure open mode
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("open")})
    assert resp.status.code == 1
    print("  Mode set to 'open'")

    # Set relay states
    resp = client.call_function(
        "tempctl0",
        3,
        {"relay_index": make_int64_value(1), "state": make_bool_value(True)},
    )
    assert resp.status.code == 1
    print("  Relay 1 set to ON")

    resp = client.call_function(
        "tempctl0",
        3,
        {"relay_index": make_int64_value(2), "state": make_bool_value(False)},
    )
    assert resp.status.code == 1
    print("  Relay 2 set to OFF")

    # Read relay states
    resp = client.read_signals("tempctl0", ["relay1_state", "relay2_state"])
    assert resp.status.code == 1

    relay1 = next(
        (
            v.value.bool_value
            for v in resp.read_signals.values
            if v.signal_id == "relay1_state"
        ),
        None,
    )
    relay2 = next(
        (
            v.value.bool_value
            for v in resp.read_signals.values
            if v.signal_id == "relay2_state"
        ),
        None,
    )

    assert relay1, "Relay 1 state mismatch"
    assert not relay2, "Relay 2 state mismatch"

    print(f"OK: Relay states correct: R1={relay1}, R2={relay2}")
    return True


def test_precondition_check(client):
    """Test 6: Precondition enforcement (relay control blocked in closed mode)."""
    print("\n=== Test 6: Precondition Enforcement ===")

    # Set mode to closed
    resp = client.call_function("tempctl0", 1, {"mode": make_string_value("closed")})
    assert resp.status.code == 1
    print("  Mode set to 'closed'")

    # Try to set relay (should fail)
    resp = client.call_function(
        "tempctl0",
        3,
        {"relay_index": make_int64_value(1), "state": make_bool_value(True)},
    )

    # Expect FAILED_PRECONDITION (code = 12)
    assert resp.status.code == 12, (
        f"Expected CODE_FAILED_PRECONDITION (12), got {resp.status.code}"
    )
    print(f"  OK: set_relay blocked: {resp.status.message}")

    return True


def main():
    parser = argparse.ArgumentParser(
        description="ADPP integration tests for anolis-provider-sim"
    )
    parser.add_argument(
        "--test",
        default="all",
        choices=[
            "all",
            "list_devices",
            "describe_tempctl",
            "temp_convergence",
            "motor_control",
            "relay_control",
            "precondition_check",
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
            "list_devices": test_list_devices,
            "describe_tempctl": test_describe_tempctl,
            "temp_convergence": test_temp_convergence,
            "motor_control": test_motor_control,
            "relay_control": test_relay_control,
            "precondition_check": test_precondition_check,
        }

        if args.test == "all":
            print("Running all ADPP integration tests...")
            results = []
            for name, test_func in tests.items():
                try:
                    test_func(client)
                    results.append((name, True))
                except Exception as e:
                    print(f"FAIL: {name} FAILED: {e}", file=sys.stderr)
                    results.append((name, False))

            print("\n" + "=" * 50)
            print("Test Summary:")
            for name, passed in results:
                status = "PASS" if passed else "FAIL"
                print(f"  {status}: {name}")

            all_passed = all(r[1] for r in results)
            if all_passed:
                print("\nAll ADPP integration tests passed!")
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
