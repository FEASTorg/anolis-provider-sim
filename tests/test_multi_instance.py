#!/usr/bin/env python3
"""Validate multi-instance device state independence."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from support.assertions import assert_ok, list_devices_entries, require_signal
from support.env import repo_root, resolve_config_path, resolve_provider_executable
from support.framed_client import AdppClient, make_string_value
from support.proto_bootstrap import load_protocol_module


def test_multi_instance_independence(
    protocol, exe_path: Path, config_path: Path
) -> bool:
    """Verify tempctl0 and tempctl1 maintain independent control state."""
    print("=== Test: Multi-Instance Device Independence ===")
    print(f"Provider: {exe_path}")
    print(f"Config: {config_path}\n")

    client = AdppClient(protocol, exe_path, config_path)
    try:
        hello_resp = client.hello(
            client_name="multi-instance-test",
            client_version="0.0.1",
        )
        assert_ok(hello_resp, "hello")

        print("Test 1: List devices")
        resp = client.list_devices()
        assert_ok(resp, "list_devices")
        device_ids = [entry.device_id for entry in list_devices_entries(resp)]
        print(f"  Found devices: {device_ids}")

        assert "tempctl0" in device_ids, "Missing tempctl0"
        assert "tempctl1" in device_ids, "Missing tempctl1"
        print("  [PASS] Both tempctl0 and tempctl1 found\n")

        print("Test 2: Read initial modes")
        resp0 = client.read_signals("tempctl0", ["control_mode"])
        resp1 = client.read_signals("tempctl1", ["control_mode"])
        assert_ok(resp0, "read tempctl0 control_mode")
        assert_ok(resp1, "read tempctl1 control_mode")

        mode0_init = require_signal(resp0, "control_mode").value.string_value
        mode1_init = require_signal(resp1, "control_mode").value.string_value
        print(f"  tempctl0 initial mode: {mode0_init}")
        print(f"  tempctl1 initial mode: {mode1_init}\n")

        print("Test 3: Verify configured initial temperatures")
        temp0_resp = client.read_signals("tempctl0", ["tc1_temp"])
        temp1_resp = client.read_signals("tempctl1", ["tc1_temp"])
        assert_ok(temp0_resp, "read tempctl0 tc1_temp")
        assert_ok(temp1_resp, "read tempctl1 tc1_temp")

        temp0 = float(require_signal(temp0_resp, "tc1_temp").value.double_value)
        temp1 = float(require_signal(temp1_resp, "tc1_temp").value.double_value)
        print(f"  tempctl0 tc1: {temp0:.2f} C (config 25.0 C)")
        print(f"  tempctl1 tc1: {temp1:.2f} C (config 30.0 C)")

        assert abs(temp0 - 25.0) < 0.1, f"tempctl0 temperature {temp0} != 25.0"
        assert abs(temp1 - 30.0) < 0.1, f"tempctl1 temperature {temp1} != 30.0"
        print("  [PASS] Initial temperatures match config\n")

        print("Test 4: Set independent modes")
        resp = client.call_function(
            "tempctl0",
            1,
            {"mode": make_string_value(protocol, "closed")},
        )
        assert_ok(resp, "set tempctl0 mode closed")

        resp = client.call_function(
            "tempctl1",
            1,
            {"mode": make_string_value(protocol, "open")},
        )
        assert_ok(resp, "set tempctl1 mode open")
        print("  [PASS] Mode change calls succeeded\n")

        print("Test 5: Verify state independence")
        resp0 = client.read_signals("tempctl0", ["control_mode"])
        resp1 = client.read_signals("tempctl1", ["control_mode"])
        assert_ok(resp0, "read tempctl0 mode after set")
        assert_ok(resp1, "read tempctl1 mode after set")

        mode0 = require_signal(resp0, "control_mode").value.string_value
        mode1 = require_signal(resp1, "control_mode").value.string_value
        print(f"  tempctl0 mode: {mode0}")
        print(f"  tempctl1 mode: {mode1}")

        assert mode0 == "closed", f"Expected tempctl0 mode='closed', got '{mode0}'"
        assert mode1 == "open", f"Expected tempctl1 mode='open', got '{mode1}'"

        print("  [PASS] States are independent")
        print("\n=== All tests PASSED ===")
        return True

    except Exception as exc:
        print(f"\n[FAIL] Test FAILED: {exc}", file=sys.stderr)
        print("Provider stderr tail:", file=sys.stderr)
        print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return False

    finally:
        client.close()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Test multi-instance behavior for anolis-provider-sim"
    )
    parser.add_argument(
        "--exe",
        help="Path to anolis-provider-sim executable (optional; auto-detect if omitted)",
    )
    parser.add_argument(
        "--config",
        default="config/multi-tempctl.yaml",
        help="Path to provider config (default: config/multi-tempctl.yaml)",
    )
    args = parser.parse_args()

    protocol, _ = load_protocol_module()
    root = repo_root()

    if args.exe:
        exe_path = Path(args.exe)
        if not exe_path.is_absolute():
            exe_path = (root / exe_path).resolve()
    else:
        exe_path = resolve_provider_executable(root)

    config_path = resolve_config_path(args.config, root)

    if not exe_path.exists():
        print(f"ERROR: Provider executable not found: {exe_path}", file=sys.stderr)
        return 1
    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    success = test_multi_instance_independence(protocol, exe_path, config_path)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
