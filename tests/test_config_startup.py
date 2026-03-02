#!/usr/bin/env python3
"""Config integrity and startup-policy tests for provider-sim."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import textwrap
import time
from pathlib import Path

from support.assertions import assert_ok, list_devices_entries
from support.env import repo_root, resolve_provider_executable
from support.framed_client import AdppClient
from support.proto_bootstrap import load_protocol_module


def write_config(tmp_dir: Path, name: str, body: str) -> Path:
    path = tmp_dir / name
    path.write_text(textwrap.dedent(body).strip() + "\n", encoding="utf-8")
    return path


def expect_startup_failure(
    provider_exe: Path,
    config_path: Path,
    expected_tokens: list[str],
    timeout_sec: float = 8.0,
) -> None:
    proc = subprocess.run(
        [str(provider_exe), "--config", str(config_path)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_sec,
        check=False,
    )

    if proc.returncode == 0:
        raise AssertionError(f"Expected startup failure, got exit=0 for config {config_path}\nstderr:\n{proc.stderr}")

    stderr = proc.stderr or ""
    for token in expected_tokens:
        if token not in stderr:
            raise AssertionError(f"Missing expected stderr token '{token}' for config {config_path}\nstderr:\n{stderr}")


def test_duplicate_device_id(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "duplicate-id.yaml",
            """
            devices:
              - id: tempctl0
                type: tempctl
              - id: tempctl0
                type: motorctl
            simulation:
              mode: non_interacting
              tick_rate_hz: 10.0
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["Duplicate device id: 'tempctl0'"],
        )


def test_unknown_simulation_key(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "unknown-sim-key.yaml",
            """
            devices:
              - id: tempctl0
                type: tempctl
            simulation:
              mode: non_interacting
              tick_rate_hz: 10.0
              bogus_key: 123
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["Unknown simulation key: 'bogus_key'"],
        )


def test_reserved_chaos_control_id(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "reserved-chaos-id.yaml",
            """
            devices:
              - id: chaos_control
                type: tempctl
            simulation:
              mode: inert
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["devices[].id 'chaos_control' is reserved"],
        )


def test_deprecated_simulation_key(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "deprecated-sim-key.yaml",
            """
            devices:
              - id: tempctl0
                type: tempctl
            simulation:
              mode: non_interacting
              tick_rate_hz: 10.0
              update_rate_hz: 20.0
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["simulation.update_rate_hz is no longer supported"],
        )


def test_ambient_path_requires_temp(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "ambient-missing-temp.yaml",
            """
            provider:
              name: chamber-provider
            devices:
              - id: tempctl0
                type: tempctl
            simulation:
              mode: sim
              tick_rate_hz: 10.0
              physics_config: physics.yaml
              ambient_signal_path: environment/ambient_temp
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["simulation.ambient_signal_path requires simulation.ambient_temp_c"],
        )


def test_strict_startup_abort(provider_exe: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "strict-startup.yaml",
            """
            startup_policy: strict
            devices:
              - id: tempctl0
                type: tempctl
              - id: bad0
                type: does_not_exist
            simulation:
              mode: non_interacting
              tick_rate_hz: 10.0
            """,
        )
        expect_startup_failure(
            provider_exe,
            cfg,
            ["startup_policy=strict", "failed to initialize 'bad0'"],
        )


def test_degraded_startup_continue(provider_exe: Path) -> None:
    protocol, _ = load_protocol_module()
    with tempfile.TemporaryDirectory(prefix="provider-sim-cfg-") as td:
        cfg = write_config(
            Path(td),
            "degraded-startup.yaml",
            """
            startup_policy: degraded
            devices:
              - id: tempctl0
                type: tempctl
              - id: bad0
                type: does_not_exist
            simulation:
              mode: non_interacting
              tick_rate_hz: 10.0
            """,
        )

        client = AdppClient(protocol, provider_exe, cfg)
        try:
            hello = client.hello(client_name="ws2-config-test", client_version="0.0.1")
            assert_ok(hello, "hello degraded startup")

            resp = client.list_devices(include_health=False)
            assert_ok(resp, "list_devices degraded startup")
            ids = [entry.device_id for entry in list_devices_entries(resp)]

            assert "tempctl0" in ids, f"Expected tempctl0 in inventory, got {ids}"
            assert "chaos_control" in ids, f"Expected chaos_control in inventory, got {ids}"
            assert "bad0" not in ids, f"Failed device should not be listed, got {ids}"

            time.sleep(0.1)
            stderr_tail = client.output_tail(120)
            if "degraded init failure: device_id=bad0" not in stderr_tail:
                raise AssertionError(f"Expected degraded startup failure log for bad0\nstderr tail:\n{stderr_tail}")
        finally:
            client.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Config/startup policy tests for anolis-provider-sim")
    parser.add_argument(
        "--test",
        default="all",
        choices=[
            "all",
            "duplicate_device_id",
            "reserved_chaos_control_id",
            "unknown_simulation_key",
            "deprecated_simulation_key",
            "ambient_signal_requires_temp",
            "strict_startup_abort",
            "degraded_startup_continue",
        ],
        help="Test to run",
    )
    args = parser.parse_args()

    root = repo_root()
    provider_exe = resolve_provider_executable(root)

    tests = {
        "duplicate_device_id": lambda: test_duplicate_device_id(provider_exe),
        "reserved_chaos_control_id": lambda: test_reserved_chaos_control_id(provider_exe),
        "unknown_simulation_key": lambda: test_unknown_simulation_key(provider_exe),
        "deprecated_simulation_key": lambda: test_deprecated_simulation_key(provider_exe),
        "ambient_signal_requires_temp": lambda: test_ambient_path_requires_temp(provider_exe),
        "strict_startup_abort": lambda: test_strict_startup_abort(provider_exe),
        "degraded_startup_continue": lambda: test_degraded_startup_continue(provider_exe),
    }

    try:
        if args.test == "all":
            failures: list[str] = []
            for name, test_fn in tests.items():
                try:
                    test_fn()
                    print(f"PASS: {name}")
                except Exception as exc:
                    failures.append(name)
                    print(f"FAIL: {name}: {exc}", file=sys.stderr)

            if failures:
                print(f"Config/startup tests failed: {', '.join(failures)}", file=sys.stderr)
                return 1

            print("All config/startup tests passed")
            return 0

        tests[args.test]()
        print(f"PASS: {args.test}")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
