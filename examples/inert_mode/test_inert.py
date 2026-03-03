#!/usr/bin/env python3
"""Inert-mode scenario demo backed by shared tests/support harness."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = ROOT / "tests"
if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from support.assertions import assert_ok, list_devices_entries, require_signal  # noqa: E402
from support.env import repo_root, resolve_config_path, resolve_provider_executable  # noqa: E402
from support.framed_client import AdppClient, make_string_value  # noqa: E402
from support.proto_bootstrap import load_protocol_module  # noqa: E402


def _read_temp_pair(client: AdppClient) -> tuple[float, float]:
    resp = client.read_signals("tempctl0", ["tc1_temp", "tc2_temp"])
    assert_ok(resp, "read_signals tempctl0")
    tc1 = float(require_signal(resp, "tc1_temp").value.double_value)
    tc2 = float(require_signal(resp, "tc2_temp").value.double_value)
    return tc1, tc2


def run_inert_example() -> int:
    protocol, build_dir = load_protocol_module()
    root = repo_root()
    provider_exe = resolve_provider_executable(root)
    config_path = resolve_config_path("examples/inert_mode/provider.yaml", root)

    print("=" * 60)
    print("Inert Mode Example")
    print("=" * 60)
    print(f"Provider: {provider_exe}")
    print(f"Build dir: {build_dir}")
    print(f"Config: {config_path}")

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    client = AdppClient(protocol, provider_exe, config_path)
    try:
        hello = client.hello(client_name="inert-example", client_version="1.0.0")
        assert_ok(hello, "hello")

        listed = client.list_devices(include_health=False)
        assert_ok(listed, "list_devices")
        device_ids = [entry.device_id for entry in list_devices_entries(listed)]
        print(f"Devices: {device_ids}")

        required = {"tempctl0", "motorctl0", "relayio0", "analogsensor0", "chaos_control"}
        missing = sorted(required.difference(device_ids))
        if missing:
            raise RuntimeError(f"Missing expected devices: {missing}")

        before = _read_temp_pair(client)
        print(f"Initial temperatures: tc1={before[0]:.1f} C tc2={before[1]:.1f} C")

        call_resp = client.call_function(
            "tempctl0",
            1,
            {"mode": make_string_value(protocol, "closed")},
        )
        assert_ok(call_resp, "call set_mode closed")

        after = _read_temp_pair(client)
        print(f"After set_mode call: tc1={after[0]:.1f} C tc2={after[1]:.1f} C")

        if after != before:
            raise RuntimeError(
                "In inert mode, temperatures should not advance without ticker/physics "
                f"(before={before}, after={after})"
            )

        print("[PASS] Inert mode scenario completed")
        return 0
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        print("Provider stderr tail:", file=sys.stderr)
        print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(run_inert_example())
