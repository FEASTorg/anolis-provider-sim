#!/usr/bin/env python3
"""
Fault injection tests for anolis-provider-sim chaos_control API.

Covers:
- inject_device_unavailable
- inject_signal_fault
- inject_call_latency
- inject_call_failure
- clear_faults
- fault isolation across devices
"""

from __future__ import annotations

import argparse
import sys
import time

from support.assertions import assert_ok, require_signal
from support.env import repo_root, resolve_config_path, resolve_provider_executable
from support.framed_client import (
    AdppClient,
    make_double_value,
    make_int64_value,
    make_string_value,
)
from support.proto_bootstrap import load_protocol_module


def test_device_unavailable(client: AdppClient, protocol) -> bool:
    """Validate inject_device_unavailable behavior and auto-recovery."""
    print("\n=== Test: Device Unavailable Injection ===")

    resp = client.describe_device("tempctl0")
    assert_ok(resp, "describe tempctl0 initial")
    initial_signal_count = len(resp.describe_device.capabilities.signals)

    resp = client.call_function(
        "chaos_control",
        1,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "duration_ms": make_int64_value(protocol, 500),
        },
    )
    assert_ok(resp, "inject_device_unavailable")

    resp = client.describe_device("tempctl0")
    assert resp.status.code != 1, "Device should return error when unavailable"

    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert resp.status.code != 1, "ReadSignals should fail on unavailable device"

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert resp.status.code != 1, "CallFunction should fail on unavailable device"

    time.sleep(0.6)

    resp = client.describe_device("tempctl0")
    assert_ok(resp, "describe tempctl0 after expiration")
    assert len(resp.describe_device.capabilities.signals) == initial_signal_count

    print("OK: Device unavailable injection working correctly")
    return True


def test_signal_fault(client: AdppClient, protocol) -> bool:
    """Validate inject_signal_fault quality override and recovery."""
    print("\n=== Test: Signal Fault Injection ===")

    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert_ok(resp, "read initial tc1_temp")
    signal = require_signal(resp, "tc1_temp")
    initial_value = signal.value.double_value

    resp = client.call_function(
        "chaos_control",
        2,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "signal_id": make_string_value(protocol, "tc1_temp"),
            "duration_ms": make_int64_value(protocol, 300),
        },
    )
    assert_ok(resp, "inject_signal_fault")

    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert_ok(resp, "read faulted tc1_temp")
    signal = require_signal(resp, "tc1_temp")
    expected_fault = protocol.SignalValue.Quality.QUALITY_FAULT
    assert signal.quality == expected_fault, (
        f"Expected FAULT quality, got {protocol.SignalValue.Quality.Name(signal.quality)}"
    )

    print(
        f"  Faulted value={signal.value.double_value:.1f} C, initial={initial_value:.1f} C, quality={protocol.SignalValue.Quality.Name(signal.quality)}"
    )

    time.sleep(0.4)

    resp = client.read_signals("tempctl0", ["tc1_temp"])
    assert_ok(resp, "read recovered tc1_temp")
    recovered = require_signal(resp, "tc1_temp")
    assert recovered.quality != expected_fault, (
        "Quality should recover after expiration"
    )

    print(
        f"OK: Signal quality recovered to {protocol.SignalValue.Quality.Name(recovered.quality)}"
    )
    return True


def test_call_latency(client: AdppClient, protocol) -> bool:
    """Validate injected call latency and clear_faults reset."""
    print("\n=== Test: Call Latency Injection ===")

    start = time.time()
    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert_ok(resp, "baseline set_mode")
    baseline_ms = (time.time() - start) * 1000

    resp = client.call_function(
        "chaos_control",
        3,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "latency_ms": make_int64_value(protocol, 200),
        },
    )
    assert_ok(resp, "inject_call_latency")

    start = time.time()
    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "closed")},
    )
    assert_ok(resp, "set_mode with injected latency")
    injected_ms = (time.time() - start) * 1000

    added_latency = injected_ms - baseline_ms
    assert added_latency >= 180, (
        f"Expected ~200ms added latency, got {added_latency:.1f}ms"
    )

    resp = client.call_function("chaos_control", 5, {})
    assert_ok(resp, "clear_faults")

    start = time.time()
    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert_ok(resp, "set_mode after clear_faults")
    cleared_ms = (time.time() - start) * 1000
    assert cleared_ms < baseline_ms + 50, "Latency should be removed after clear_faults"

    print(
        f"OK: baseline={baseline_ms:.1f}ms injected={injected_ms:.1f}ms cleared={cleared_ms:.1f}ms"
    )
    return True


def test_call_failure(client: AdppClient, protocol) -> bool:
    """Validate per-function probabilistic failure injection."""
    print("\n=== Test: Call Failure Injection ===")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert_ok(resp, "baseline set_mode")

    resp = client.call_function(
        "chaos_control",
        4,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "function_id": make_string_value(protocol, "1"),
            "failure_rate": make_double_value(protocol, 1.0),
        },
    )
    assert_ok(resp, "inject_call_failure 100%")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "closed")},
    )
    assert resp.status.code != 1, "Function should fail with 100% failure rate"

    resp = client.call_function(
        "chaos_control",
        4,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "function_id": make_string_value(protocol, "1"),
            "failure_rate": make_double_value(protocol, 0.0),
        },
    )
    assert_ok(resp, "inject_call_failure 0%")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "open")},
    )
    assert_ok(resp, "set_mode after 0% failure")

    resp = client.call_function("chaos_control", 5, {})
    assert_ok(resp, "clear_faults")

    resp = client.call_function(
        "tempctl0",
        1,
        {"mode": make_string_value(protocol, "closed")},
    )
    assert_ok(resp, "set_mode after clear_faults")

    print("OK: Call failure injection working correctly")
    return True


def test_clear_faults(client: AdppClient, protocol) -> bool:
    """Validate clear_faults removes all active injections."""
    print("\n=== Test: Clear All Faults ===")

    resp = client.call_function(
        "chaos_control",
        1,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "duration_ms": make_int64_value(protocol, 10000),
        },
    )
    assert_ok(resp, "inject_device_unavailable long")

    resp = client.call_function(
        "chaos_control",
        3,
        {
            "device_id": make_string_value(protocol, "motorctl0"),
            "latency_ms": make_int64_value(protocol, 500),
        },
    )
    assert_ok(resp, "inject_call_latency motorctl0")

    resp = client.describe_device("tempctl0")
    assert resp.status.code != 1, "tempctl0 should be unavailable"

    resp = client.call_function("chaos_control", 5, {})
    assert_ok(resp, "clear_faults")

    resp = client.describe_device("tempctl0")
    assert_ok(resp, "describe tempctl0 after clear")

    start = time.time()
    resp = client.call_function(
        "motorctl0",
        10,
        {
            "motor_index": make_int64_value(protocol, 1),
            "duty": make_double_value(protocol, 0.5),
        },
    )
    assert_ok(resp, "motorctl call after clear")
    latency_ms = (time.time() - start) * 1000
    assert latency_ms < 100, f"Latency should be cleared, got {latency_ms:.1f}ms"

    print("OK: clear_faults removed all injections")
    return True


def test_multiple_devices(client: AdppClient, protocol) -> bool:
    """Validate injected fault isolation across devices."""
    print("\n=== Test: Fault Isolation Between Devices ===")

    resp = client.call_function(
        "chaos_control",
        1,
        {
            "device_id": make_string_value(protocol, "tempctl0"),
            "duration_ms": make_int64_value(protocol, 500),
        },
    )
    assert_ok(resp, "inject tempctl0 unavailable")

    resp = client.describe_device("tempctl0")
    assert resp.status.code != 1, "tempctl0 should be unavailable"

    resp = client.describe_device("motorctl0")
    assert_ok(resp, "describe motorctl0 while tempctl0 faulted")

    resp = client.describe_device("relayio0")
    assert_ok(resp, "describe relayio0 while tempctl0 faulted")

    resp = client.call_function("chaos_control", 5, {})
    assert_ok(resp, "clear_faults")

    resp = client.describe_device("tempctl0")
    assert_ok(resp, "describe tempctl0 after clear")

    print("OK: Fault isolation working correctly")
    return True


def _new_client(protocol, exe_path, config_path) -> AdppClient:
    client = AdppClient(protocol, exe_path, config_path)
    hello_resp = client.hello(
        client_name="fault-injection-test",
        client_version="0.0.1",
    )
    assert_ok(hello_resp, "hello")
    return client


def main() -> int:
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

    protocol, _ = load_protocol_module()
    root = repo_root()
    exe_path = resolve_provider_executable(root)
    config_path = resolve_config_path("config/provider-sim.yaml", root)

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    tests = {
        "device_unavailable": lambda c: test_device_unavailable(c, protocol),
        "signal_fault": lambda c: test_signal_fault(c, protocol),
        "call_latency": lambda c: test_call_latency(c, protocol),
        "call_failure": lambda c: test_call_failure(c, protocol),
        "clear_faults": lambda c: test_clear_faults(c, protocol),
        "multiple_devices": lambda c: test_multiple_devices(c, protocol),
    }

    if args.test == "all":
        print("Running all fault injection tests...")
        results: list[tuple[str, bool]] = []

        for name, test_fn in tests.items():
            client = None
            try:
                client = _new_client(protocol, exe_path, config_path)
                test_fn(client)
                results.append((name, True))
            except Exception as exc:
                print(f"FAIL: {name} - {exc}", file=sys.stderr)
                if client is not None:
                    print("Provider stderr tail:", file=sys.stderr)
                    print(client.output_tail(120) or "(empty)", file=sys.stderr)
                results.append((name, False))
            finally:
                if client is not None:
                    client.close()

        print("\n" + "=" * 50)
        print("Test Summary:")
        for name, passed in results:
            print(f"  {'PASS' if passed else 'FAIL'}: {name}")

        return 0 if all(passed for _, passed in results) else 1

    client = None
    try:
        client = _new_client(protocol, exe_path, config_path)
        tests[args.test](client)
        print(f"\nOK: Test '{args.test}' passed!")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        if client is not None:
            print("Provider stderr tail:", file=sys.stderr)
            print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1
    finally:
        if client is not None:
            client.close()


if __name__ == "__main__":
    sys.exit(main())
