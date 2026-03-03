#!/usr/bin/env python3
"""Sim-mode scenario demo backed by shared tests/support harness."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = ROOT / "tests"
if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from support.assertions import assert_ok, require_signal  # noqa: E402
from support.env import (  # noqa: E402
    find_free_port,
    repo_root,
    resolve_config_path,
    resolve_fluxgraph_server,
    resolve_fluxgraph_provider_executable,
)
from support.framed_client import AdppClient, make_double_value, make_string_value  # noqa: E402
from support.process import ManagedTextProcess  # noqa: E402
from support.proto_bootstrap import load_protocol_module  # noqa: E402


def _read_temp(client: AdppClient) -> float:
    resp = client.read_signals("chamber", ["tc1_temp"])
    assert_ok(resp, "read chamber tc1_temp")
    return float(require_signal(resp, "tc1_temp").value.double_value)


def run_sim_example(duration_sec: int, port: int) -> int:
    protocol, build_dir = load_protocol_module()
    root = repo_root()
    provider_exe = resolve_fluxgraph_provider_executable(root)
    fluxgraph_server = resolve_fluxgraph_server(root)
    config_path = resolve_config_path("examples/sim_mode/provider.yaml", root)

    print("=" * 60)
    print("Sim Mode Example")
    print("=" * 60)
    print(f"Provider: {provider_exe}")
    print(f"Build dir: {build_dir}")
    print(f"FluxGraph server: {fluxgraph_server}")
    print(f"Config: {config_path}")
    print(f"Port: {port}")

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    server = ManagedTextProcess.start(
        "fluxgraph-server",
        [str(fluxgraph_server), "--port", str(port), "--dt", "0.1"],
    )
    client: AdppClient | None = None
    try:
        if not server.wait_for_port("127.0.0.1", port, timeout=8.0):
            raise RuntimeError(
                "FluxGraph server did not become ready in time\n"
                + server.output_tail(120)
            )

        client = AdppClient(
            protocol,
            provider_exe,
            config_path,
            sim_server=f"localhost:{port}",
        )

        hello = client.hello(client_name="sim-example", client_version="1.0.0")
        assert_ok(hello, "hello")

        ready = client.wait_ready(max_wait_ms_hint=5000)
        assert_ok(ready, "wait_ready")

        baseline = _read_temp(client)
        print(f"Initial chamber temperature: {baseline:.2f} C")

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

        deadline = time.time() + max(duration_sec, 5)
        observed_max = baseline
        samples = 0
        while time.time() < deadline:
            current = _read_temp(client)
            observed_max = max(observed_max, current)
            samples += 1
            print(f"temp sample {samples}: {current:.2f} C")
            if current >= baseline + 1.0:
                break
            time.sleep(0.5)
        else:
            raise RuntimeError(
                "Simulation progression assertion failed: expected >=1.0 C rise "
                f"(initial={baseline:.2f}, max={observed_max:.2f})"
            )

        print("[PASS] Sim mode scenario completed")
        return 0
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        print(server.output_tail(120), file=sys.stderr)
        if client is not None:
            print("Provider stderr tail:", file=sys.stderr)
            print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1
    finally:
        if client is not None:
            client.close()
        server.close(timeout=5.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Sim mode example scenario")
    parser.add_argument(
        "--duration",
        type=int,
        default=15,
        help="Max seconds to wait for measurable simulation progression (default: 15)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=0,
        help="FluxGraph server port (default: auto-select free port)",
    )
    args = parser.parse_args()

    port = args.port if args.port > 0 else find_free_port(50061, 10)
    return run_sim_example(args.duration, port)


if __name__ == "__main__":
    sys.exit(main())
