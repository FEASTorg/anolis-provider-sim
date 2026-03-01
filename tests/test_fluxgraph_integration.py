#!/usr/bin/env python3
"""
FluxGraph integration test for anolis-provider-sim.

Validates:
- FluxGraph server startup and connectivity
- Provider hello + wait_ready flow in sim mode
- Basic control action causes observable simulated temperature progression
"""

from __future__ import annotations

import argparse
import sys
import time

from support.assertions import assert_ok
from support.env import (
    find_free_port,
    repo_root,
    resolve_config_path,
    resolve_fluxgraph_server,
    resolve_provider_executable,
)
from support.framed_client import AdppClient, make_double_value, make_string_value
from support.process import ManagedTextProcess
from support.proto_bootstrap import load_protocol_module


def _read_temp(client: AdppClient, signal_id: str = "tc1_temp") -> float:
    resp = client.read_signals("tempctl0", [signal_id])
    assert_ok(resp, f"read_signals tempctl0/{signal_id}")
    if not resp.read_signals.values:
        raise RuntimeError(f"No values returned for signal {signal_id}")
    return float(resp.read_signals.values[0].value.double_value)


def run_fluxgraph_integration(duration: int, port: int) -> int:
    protocol, _ = load_protocol_module()
    root = repo_root()

    provider_exe = resolve_provider_executable(root)
    server_exe = resolve_fluxgraph_server(root)
    config_path = resolve_config_path("config/provider-chamber.yaml", root)

    if not config_path.exists():
        print(f"ERROR: Test config not found: {config_path}", file=sys.stderr)
        return 1

    print("=" * 60)
    print("FluxGraph Integration Test")
    print("=" * 60)
    print(f"Provider: {provider_exe}")
    print(f"FluxGraph server: {server_exe}")
    print(f"Config: {config_path}")
    print(f"Port: {port}")

    server = ManagedTextProcess.start(
        "fluxgraph-server",
        [str(server_exe), "--port", str(port), "--dt", "0.1"],
    )

    client: AdppClient | None = None
    try:
        if not server.wait_for_port("127.0.0.1", port, timeout=8.0):
            raise RuntimeError(
                f"FluxGraph server did not listen on 127.0.0.1:{port} within timeout\n{server.output_tail(120)}"
            )

        client = AdppClient(
            protocol,
            provider_exe,
            config_path,
            sim_server=f"localhost:{port}",
        )

        hello_resp = client.hello(
            client_name="fluxgraph-integration-test",
            client_version="1.0.0",
        )
        assert_ok(hello_resp, "hello")

        ready_resp = client.wait_ready(max_wait_ms_hint=5000)
        assert_ok(ready_resp, "wait_ready")

        initial_temp = _read_temp(client)
        print(f"Initial chamber temperature: {initial_temp:.2f} C")

        # Drive closed-loop heating and confirm measurable state progression.
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

        deadline = time.time() + max(duration, 5)
        max_temp = initial_temp
        samples = 0
        while time.time() < deadline:
            current = _read_temp(client)
            max_temp = max(max_temp, current)
            samples += 1
            if current >= initial_temp + 1.0:
                print(f"Observed temperature rise to {current:.2f} C after {samples} samples")
                break
            time.sleep(0.5)
        else:
            raise RuntimeError(
                "Simulation progression assertion failed: chamber temperature did not rise by >= 1.0 C "
                f"within {max(duration, 5)}s (initial={initial_temp:.2f} C, max={max_temp:.2f} C)"
            )

        print("\n" + "=" * 60)
        print("[PASS] FluxGraph integration test PASSED")
        print("=" * 60)
        return 0

    except Exception as exc:
        print(f"\nERROR: {exc}", file=sys.stderr)
        print(server.output_tail(120), file=sys.stderr)
        if client is not None:
            print("[provider] stderr tail:", file=sys.stderr)
            print(client.output_tail(120) or "(empty)", file=sys.stderr)
        return 1

    finally:
        if client is not None:
            client.close()
        server.close(timeout=5.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="FluxGraph integration test")
    parser.add_argument(
        "-d",
        "--duration",
        type=int,
        default=15,
        help="Max seconds to wait for measurable state progression (default: 15)",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=int,
        default=0,
        help="FluxGraph server port (default: auto-detect free port)",
    )
    args = parser.parse_args()

    port = args.port if args.port > 0 else find_free_port(50061, 10)

    try:
        return run_fluxgraph_integration(args.duration, port)
    except KeyboardInterrupt:
        print("Interrupted by user", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
