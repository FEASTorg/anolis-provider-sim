#!/usr/bin/env python3
"""Non-interacting-mode scenario demo backed by shared tests/support harness."""

from __future__ import annotations

import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = ROOT / "tests"
if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from support.assertions import assert_ok, require_signal  # noqa: E402
from support.env import repo_root, resolve_config_path, resolve_provider_executable  # noqa: E402
from support.framed_client import (  # noqa: E402
    AdppClient,
    make_double_value,
    make_int64_value,
    make_string_value,
)
from support.proto_bootstrap import load_protocol_module  # noqa: E402


def _read_temp(client: AdppClient) -> float:
    resp = client.read_signals("chamber", ["tc1_temp"])
    assert_ok(resp, "read chamber tc1_temp")
    return float(require_signal(resp, "tc1_temp").value.double_value)


def _read_speed(client: AdppClient) -> float:
    resp = client.read_signals("motor", ["motor1_speed"])
    assert_ok(resp, "read motor1_speed")
    return float(require_signal(resp, "motor1_speed").value.double_value)


def run_non_interacting_example() -> int:
    protocol, build_dir = load_protocol_module()
    root = repo_root()
    provider_exe = resolve_provider_executable(root)
    config_path = resolve_config_path("examples/non_interacting_mode/provider.yaml", root)

    print("=" * 60)
    print("Non-Interacting Mode Example")
    print("=" * 60)
    print(f"Provider: {provider_exe}")
    print(f"Build dir: {build_dir}")
    print(f"Config: {config_path}")

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    client = AdppClient(protocol, provider_exe, config_path)
    try:
        hello = client.hello(client_name="non-interacting-example", client_version="1.0.0")
        assert_ok(hello, "hello")

        resp = client.call_function(
            "chamber",
            1,
            {"mode": make_string_value(protocol, "closed")},
        )
        assert_ok(resp, "set_mode closed")

        resp = client.call_function(
            "chamber",
            2,
            {"value": make_double_value(protocol, 80.0)},
        )
        assert_ok(resp, "set_setpoint 80")

        temps: list[float] = []
        for idx in range(8):
            time.sleep(1.0)
            temp = _read_temp(client)
            temps.append(temp)
            print(f"temp sample {idx + 1}: {temp:.1f} C")

        if temps[-1] <= temps[0]:
            raise RuntimeError(f"Temperature did not rise (first={temps[0]:.1f}, last={temps[-1]:.1f})")

        resp = client.call_function(
            "motor",
            10,
            {
                "motor_index": make_int64_value(protocol, 1),
                "duty": make_double_value(protocol, 0.5),
            },
        )
        assert_ok(resp, "set_motor_duty")

        speeds: list[float] = []
        for idx in range(5):
            time.sleep(1.0)
            speed = _read_speed(client)
            speeds.append(speed)
            print(f"motor sample {idx + 1}: {speed:.0f} RPM")

        if speeds[-1] <= speeds[0]:
            raise RuntimeError(f"Motor speed did not rise (first={speeds[0]:.0f}, last={speeds[-1]:.0f})")

        print("[PASS] Non-interacting mode scenario completed")
        return 0
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        print("Provider stderr tail:", file=sys.stderr)
        print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(run_non_interacting_example())
