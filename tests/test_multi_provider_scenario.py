#!/usr/bin/env python3
"""
Multi-provider FluxGraph integration scenario.

Validates:
1) Two provider-sim instances run concurrently against one FluxGraph server.
2) Distinct provider identities coexist without session invalidation.
3) Cross-provider thermal coupling is observable through ADPP reads.
"""

from __future__ import annotations

import argparse
import queue
import sys
import threading
import time

from support.assertions import assert_ok
from support.env import (
    find_free_port,
    repo_root,
    resolve_config_path,
    resolve_fluxgraph_server,
    resolve_provider_executable,
)
from support.framed_client import (
    AdppClient,
    make_bool_value,
    make_double_value,
    make_int64_value,
    make_string_value,
)
from support.process import ManagedTextProcess
from support.proto_bootstrap import load_protocol_module

FN_SET_MODE = 1
FN_SET_SETPOINT = 2
FN_SET_RELAY = 3

INIT_DELAY_SEC = 1.0
HOTEND_WARMUP_SEC = 30.0
BASELINE_WINDOW_SEC = 20.0
CHAMBER_WARMUP_SEC = 120.0
COUPLED_WINDOW_SEC = 20.0
SAMPLE_PERIOD_SEC = 0.2


def _read_signal(client: AdppClient, device_id: str, signal_id: str) -> float:
    resp = client.read_signals(device_id, [signal_id])
    assert_ok(resp, f"read_signals {device_id}/{signal_id}")
    values = resp.read_signals.values
    if len(values) != 1:
        raise RuntimeError(f"Expected one value for {device_id}/{signal_id}, got {len(values)}")

    value = values[0].value
    vtype = client.protocol.ValueType
    if value.type == vtype.VALUE_TYPE_DOUBLE:
        return float(value.double_value)
    if value.type == vtype.VALUE_TYPE_INT64:
        return float(value.int64_value)
    if value.type == vtype.VALUE_TYPE_BOOL:
        return 1.0 if value.bool_value else 0.0
    raise RuntimeError(f"Unsupported value type for {device_id}/{signal_id}: {value.type}")


def average_material_window(
    extruder_client: AdppClient,
    chamber_client: AdppClient,
    duration_sec: float,
    sample_period_sec: float,
) -> tuple[float, int]:
    samples: list[float] = []
    chamber_reads = 0
    deadline = time.time() + duration_sec

    while time.time() < deadline:
        samples.append(_read_signal(extruder_client, "tempctl1", "tc2_temp"))
        _read_signal(chamber_client, "tempctl0", "tc1_temp")
        chamber_reads += 1
        time.sleep(sample_period_sec)

    if not samples:
        raise RuntimeError("No samples collected for material temperature window")

    return (sum(samples) / len(samples), chamber_reads)


def _wait_ready_parallel(
    chamber: AdppClient,
    extruder: AdppClient,
    timeout_sec: float = 30.0,
) -> None:
    errors: queue.Queue[tuple[str, Exception]] = queue.Queue()

    def run(name: str, client: AdppClient) -> None:
        try:
            resp = client.wait_ready(max_wait_ms_hint=5000)
            assert_ok(resp, f"{name} wait_ready")
        except Exception as exc:
            errors.put((name, exc))

    chamber_t = threading.Thread(target=run, args=("chamber-provider", chamber))
    extruder_t = threading.Thread(target=run, args=("extruder-provider", extruder))

    chamber_t.start()
    extruder_t.start()

    chamber_t.join(timeout=timeout_sec)
    extruder_t.join(timeout=timeout_sec)

    if chamber_t.is_alive() or extruder_t.is_alive():
        raise RuntimeError(f"wait_ready timed out after {timeout_sec:.1f}s")

    if not errors.empty():
        name, exc = errors.get()
        raise RuntimeError(f"{name} wait_ready failed: {exc}")


def run_scenario(port: int) -> int:
    protocol, _ = load_protocol_module()
    root = repo_root()

    provider_exe = resolve_provider_executable(root)
    server_exe = resolve_fluxgraph_server(root)
    chamber_cfg = resolve_config_path("config/provider-chamber.yaml", root)
    extruder_cfg = resolve_config_path("config/provider-extruder.yaml", root)

    for path in [chamber_cfg, extruder_cfg]:
        if not path.exists():
            raise FileNotFoundError(f"Missing config: {path}")

    server = ManagedTextProcess.start(
        "fluxgraph-server",
        [str(server_exe), "--port", str(port), "--dt", "0.1"],
    )

    chamber: AdppClient | None = None
    extruder: AdppClient | None = None

    try:
        if not server.wait_for_port("127.0.0.1", port, timeout=8.0):
            raise RuntimeError(
                f"FluxGraph server failed to accept connections on port {port}\n{server.output_tail(120)}"
            )

        sim_server = f"localhost:{port}"
        chamber = AdppClient(protocol, provider_exe, chamber_cfg, sim_server=sim_server)
        extruder = AdppClient(protocol, provider_exe, extruder_cfg, sim_server=sim_server)

        assert_ok(
            chamber.hello(client_name="multi-provider-scenario", client_version="1.0.0"),
            "chamber hello",
        )
        assert_ok(
            extruder.hello(client_name="multi-provider-scenario", client_version="1.0.0"),
            "extruder hello",
        )

        print("[Multi-Provider Scenario] Starting both providers...")
        _wait_ready_parallel(chamber, extruder)
        time.sleep(INIT_DELAY_SEC)

        # Extruder setup: closed-loop hotend at 230C.
        assert_ok(
            extruder.call_function(
                "tempctl1",
                FN_SET_MODE,
                {"mode": make_string_value(protocol, "closed")},
            ),
            "extruder set_mode closed",
        )
        assert_ok(
            extruder.call_function(
                "tempctl1",
                FN_SET_SETPOINT,
                {"value": make_double_value(protocol, 230.0)},
            ),
            "extruder set_setpoint 230",
        )

        # Chamber baseline: keep heater inactive.
        assert_ok(
            chamber.call_function(
                "tempctl0",
                FN_SET_MODE,
                {"mode": make_string_value(protocol, "open")},
            ),
            "chamber set_mode open",
        )
        assert_ok(
            chamber.call_function(
                "tempctl0",
                FN_SET_RELAY,
                {
                    "relay_index": make_int64_value(protocol, 1),
                    "state": make_bool_value(protocol, False),
                },
            ),
            "chamber relay1 off",
        )
        assert_ok(
            chamber.call_function(
                "tempctl0",
                FN_SET_RELAY,
                {
                    "relay_index": make_int64_value(protocol, 2),
                    "state": make_bool_value(protocol, False),
                },
            ),
            "chamber relay2 off",
        )

        print(f"[Multi-Provider Scenario] Warmup: hotend to 230C for {HOTEND_WARMUP_SEC:.0f}s")
        time.sleep(HOTEND_WARMUP_SEC)

        hotend_temp = _read_signal(extruder, "tempctl1", "tc1_temp")
        if abs(hotend_temp - 230.0) > 10.0:
            raise RuntimeError(f"Hotend failed warmup: tc1_temp={hotend_temp:.2f}C (expected within 10C of 230C)")

        baseline_avg, baseline_chamber_reads = average_material_window(
            extruder,
            chamber,
            BASELINE_WINDOW_SEC,
            SAMPLE_PERIOD_SEC,
        )
        print(
            f"[Multi-Provider Scenario] Baseline material avg={baseline_avg:.2f}C "
            f"(chamber reads={baseline_chamber_reads})"
        )

        # Chamber warmup: closed-loop to 50C.
        assert_ok(
            chamber.call_function(
                "tempctl0",
                FN_SET_MODE,
                {"mode": make_string_value(protocol, "closed")},
            ),
            "chamber set_mode closed",
        )
        assert_ok(
            chamber.call_function(
                "tempctl0",
                FN_SET_SETPOINT,
                {"value": make_double_value(protocol, 50.0)},
            ),
            "chamber set_setpoint 50",
        )

        print(f"[Multi-Provider Scenario] Warmup: chamber to 50C for {CHAMBER_WARMUP_SEC:.0f}s")
        time.sleep(CHAMBER_WARMUP_SEC)

        chamber_temp = _read_signal(chamber, "tempctl0", "tc1_temp")
        if chamber_temp < 40.0:
            raise RuntimeError(f"Chamber did not warm as expected: tc1_temp={chamber_temp:.2f}C (expected >= 40C)")

        coupled_avg, coupled_chamber_reads = average_material_window(
            extruder,
            chamber,
            COUPLED_WINDOW_SEC,
            SAMPLE_PERIOD_SEC,
        )
        print(
            f"[Multi-Provider Scenario] Coupled material avg={coupled_avg:.2f}C (chamber reads={coupled_chamber_reads})"
        )

        delta = coupled_avg - baseline_avg
        print(f"[Multi-Provider Scenario] Coupling delta={delta:.2f}C")
        if delta < 8.0:
            raise RuntimeError(f"Cross-provider coupling assertion failed: delta={delta:.2f}C (< 8.0C)")

        print("[Multi-Provider Scenario] PASS: multi-provider coupling validated")
        return 0

    except Exception as exc:
        print(f"[Multi-Provider Scenario] ERROR: {exc}", file=sys.stderr)
        print(server.output_tail(120), file=sys.stderr)
        if chamber is not None:
            print("[chamber-provider] stderr tail:", file=sys.stderr)
            print(chamber.output_tail(120) or "(empty)", file=sys.stderr)
        if extruder is not None:
            print("[extruder-provider] stderr tail:", file=sys.stderr)
            print(extruder.output_tail(120) or "(empty)", file=sys.stderr)
        return 1

    finally:
        if chamber is not None:
            chamber.close()
        if extruder is not None:
            extruder.close()
        server.close(timeout=5.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run multi-provider FluxGraph scenario")
    parser.add_argument(
        "--port",
        type=int,
        default=0,
        help="FluxGraph server port (default: auto-detect from 50051+)",
    )
    args = parser.parse_args()

    port = args.port if args.port > 0 else find_free_port(50051, 10)

    try:
        return run_scenario(port)
    except KeyboardInterrupt:
        print("[Multi-Provider Scenario] Interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
