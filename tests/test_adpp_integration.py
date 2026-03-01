#!/usr/bin/env python3
"""
ADPP integration tests for anolis-provider-sim.

Covers:
- ListDevices, DescribeDevice, ReadSignals, CallFunction RPCs
- tempctl closed-loop convergence
- motorctl duty/speed behavior
- relay control and precondition enforcement
"""

from __future__ import annotations

import argparse
import sys
import time

from support.assertions import (
    assert_ok,
    list_devices_entries,
    require_signal,
    status_text,
)
from support.env import repo_root, resolve_config_path, resolve_provider_executable
from support.framed_client import (
    AdppClient,
    make_bool_value,
    make_double_value,
    make_int64_value,
    make_string_value,
)
from support.proto_bootstrap import load_protocol_module


def test_list_devices(client: AdppClient) -> bool:
    """Verify ListDevices returns expected default config devices."""
    print("\n=== Test 1: ListDevices ===")
    resp = client.list_devices(include_health=False)
    assert_ok(resp, "list_devices")

    devices = list_devices_entries(resp)
    assert len(devices) == 5, f"Expected 5 devices, got {len(devices)}"

    device_ids = [entry.device_id for entry in devices]
    expected = [
        "tempctl0",
        "motorctl0",
        "relayio0",
        "analogsensor0",
        "chaos_control",
    ]
    for device_id in expected:
        assert device_id in device_ids, f"Missing device: {device_id}"

    print(f"OK: Found {len(devices)} devices: {device_ids}")
    return True


def test_describe_tempctl(client: AdppClient) -> bool:
    """Verify tempctl0 capabilities."""
    print("\n=== Test 2: DescribeDevice (tempctl0) ===")
    resp = client.describe_device("tempctl0")
    assert_ok(resp, "describe_device tempctl0")

    caps = resp.describe_device.capabilities
    signal_ids = [entry.signal_id for entry in caps.signals]
    function_ids = [entry.function_id for entry in caps.functions]

    assert "tc1_temp" in signal_ids
    assert "tc2_temp" in signal_ids
    assert "relay1_state" in signal_ids
    assert "relay2_state" in signal_ids
    assert "control_mode" in signal_ids
    assert "setpoint" in signal_ids

    assert 1 in function_ids  # set_mode
    assert 2 in function_ids  # set_setpoint
    assert 3 in function_ids  # set_relay

    print(f"  Signals ({len(signal_ids)}): {signal_ids}")
    print(f"  Functions ({len(function_ids)}): {function_ids}")
    print("OK: tempctl0 capabilities validated")
    return True


def test_temp_convergence(client: AdppClient, protocol) -> bool:
    """Verify temperature increases in closed-loop mode toward setpoint."""
    print("\n=== Test 3: Temperature Convergence ===")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "closed")},
    )
    assert_ok(resp, "set_mode closed")

    resp = client.call_function(
        "tempctl0",
        2,
        {"value": make_double_value(protocol, 80.0)},
    )
    assert_ok(resp, "set_setpoint 80")

    temps: list[tuple[float, float]] = []
    for idx in range(10):
        resp = client.read_signals("tempctl0", ["tc1_temp", "tc2_temp"])
        assert_ok(resp, f"read_signals temperature sample {idx + 1}")

        tc1 = float(require_signal(resp, "tc1_temp").value.double_value)
        tc2 = float(require_signal(resp, "tc2_temp").value.double_value)
        temps.append((tc1, tc2))

        print(f"  Read {idx + 1}: TC1={tc1:.1f} C, TC2={tc2:.1f} C")
        time.sleep(1.5)

    assert temps[-1][0] > temps[0][0], "TC1 temperature did not increase"
    assert temps[-1][1] > temps[0][1], "TC2 temperature did not increase"

    print(f"OK: Temperatures increased: TC1 {temps[0][0]:.1f} -> {temps[-1][0]:.1f} C")
    return True


def test_motor_control(client: AdppClient, protocol) -> bool:
    """Verify motor duty call updates motor speed dynamics."""
    print("\n=== Test 4: Motor Control ===")

    resp = client.call_function(
        "motorctl0",
        10,
        {
            "motor_index": make_int64_value(protocol, 1),
            "duty": make_double_value(protocol, 0.5),
        },
    )
    assert_ok(resp, "set_motor_duty motor1")

    speeds: list[float] = []
    for idx in range(6):
        resp = client.read_signals("motorctl0", ["motor1_speed"])
        assert_ok(resp, f"read motor speed sample {idx + 1}")
        speed = float(require_signal(resp, "motor1_speed").value.double_value)
        speeds.append(speed)

        print(f"  Read {idx + 1}: Motor1={speed:.0f} RPM")
        time.sleep(1.0)

    assert speeds[-1] > speeds[0], "Motor speed did not increase"
    assert speeds[-1] > 1000, f"Motor speed too low: {speeds[-1]:.0f} RPM"

    print(f"OK: Motor speed ramping up: {speeds[0]:.0f} -> {speeds[-1]:.0f} RPM")
    return True


def test_relay_control(client: AdppClient, protocol) -> bool:
    """Verify relay control in open-loop mode."""
    print("\n=== Test 5: Relay Control (Open Loop) ===")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert_ok(resp, "set_mode open")

    resp = client.call_function(
        "tempctl0",
        3,
        {
            "relay_index": make_int64_value(protocol, 1),
            "state": make_bool_value(protocol, True),
        },
    )
    assert_ok(resp, "set_relay relay1 on")

    resp = client.call_function(
        "tempctl0",
        3,
        {
            "relay_index": make_int64_value(protocol, 2),
            "state": make_bool_value(protocol, False),
        },
    )
    assert_ok(resp, "set_relay relay2 off")

    resp = client.read_signals("tempctl0", ["relay1_state", "relay2_state"])
    assert_ok(resp, "read relay states")

    relay1 = require_signal(resp, "relay1_state").value.bool_value
    relay2 = require_signal(resp, "relay2_state").value.bool_value

    assert relay1 is True, "Relay 1 state mismatch"
    assert relay2 is False, "Relay 2 state mismatch"

    print(f"OK: Relay states correct: relay1={relay1}, relay2={relay2}")
    return True


def test_precondition_check(client: AdppClient, protocol) -> bool:
    """Verify relay control is blocked in closed-loop mode."""
    print("\n=== Test 6: Precondition Enforcement ===")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "closed")},
    )
    assert_ok(resp, "set_mode closed")

    resp = client.call_function(
        "tempctl0",
        3,
        {
            "relay_index": make_int64_value(protocol, 1),
            "state": make_bool_value(protocol, True),
        },
    )

    assert resp.status.code == 12, f"Expected CODE_FAILED_PRECONDITION (12), got {status_text(resp)}"
    print(f"OK: set_relay blocked as expected: {status_text(resp)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="ADPP integration tests for anolis-provider-sim")
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

    protocol, _ = load_protocol_module()
    root = repo_root()
    exe_path = resolve_provider_executable(root)
    config_path = resolve_config_path("config/provider-sim.yaml", root)

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    client = AdppClient(protocol, exe_path, config_path)
    try:
        hello_resp = client.hello(
            client_name="adpp-integration-test",
            client_version="0.0.1",
        )
        assert_ok(hello_resp, "hello")

        tests = {
            "list_devices": lambda c: test_list_devices(c),
            "describe_tempctl": lambda c: test_describe_tempctl(c),
            "temp_convergence": lambda c: test_temp_convergence(c, protocol),
            "motor_control": lambda c: test_motor_control(c, protocol),
            "relay_control": lambda c: test_relay_control(c, protocol),
            "precondition_check": lambda c: test_precondition_check(c, protocol),
        }

        if args.test == "all":
            print("Running all ADPP integration tests...")
            results: list[tuple[str, bool]] = []

            for name, test_fn in tests.items():
                try:
                    test_fn(client)
                    results.append((name, True))
                except Exception as exc:
                    print(f"FAIL: {name} FAILED: {exc}", file=sys.stderr)
                    results.append((name, False))

            print("\n" + "=" * 50)
            print("Test Summary:")
            for name, passed in results:
                print(f"  {'PASS' if passed else 'FAIL'}: {name}")

            all_passed = all(passed for _, passed in results)
            if all_passed:
                print("\nAll ADPP integration tests passed!")
                return 0

            print("\nSome tests failed", file=sys.stderr)
            print("Provider stderr tail:", file=sys.stderr)
            print(client.output_tail(120) or "(empty)", file=sys.stderr)
            return 1

        tests[args.test](client)
        print(f"\nOK: Test '{args.test}' passed!")
        return 0

    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        print("Provider stderr tail:", file=sys.stderr)
        print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1

    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
