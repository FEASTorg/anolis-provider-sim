#!/usr/bin/env python3
"""
Multi-provider integration scenario.

Validates:
1) Two provider-sim instances run concurrently against one FluxGraph server.
2) Distinct provider identities can coexist without session invalidation.
3) Cross-provider thermal coupling is observable through ADPP signal reads.
"""

import argparse
import atexit
import os
import queue
import signal
import socket
import struct
import subprocess
import sys
import threading
import time
import traceback
from pathlib import Path
from typing import Dict, List, Optional, Tuple

script_dir = Path(__file__).parent
repo_root = script_dir.parent

build_dir_env = os.environ.get("ANOLIS_PROVIDER_SIM_BUILD_DIR")
build_dir = Path(build_dir_env) if build_dir_env else (repo_root / "build")
if not build_dir.is_absolute():
    build_dir = repo_root / build_dir
sys.path.insert(0, str(build_dir))

try:
    from protocol_pb2 import Request, Response, Value, ValueType
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print(
        "Run: ./scripts/generate_proto_python.sh (Linux/macOS) or pwsh ./scripts/generate_proto_python.ps1 (Windows)",
        file=sys.stderr,
    )
    sys.exit(1)


OK_CODE = 1
FN_SET_MODE = 1
FN_SET_SETPOINT = 2
FN_SET_RELAY = 3

INIT_DELAY_SEC = 1.0
HOTEND_WARMUP_SEC = 30.0
BASELINE_WINDOW_SEC = 20.0
CHAMBER_WARMUP_SEC = 120.0
COUPLED_WINDOW_SEC = 20.0
SAMPLE_PERIOD_SEC = 0.2


def make_string_value(value: str) -> Value:
    out = Value()
    out.type = ValueType.VALUE_TYPE_STRING
    out.string_value = value
    return out


def make_double_value(value: float) -> Value:
    out = Value()
    out.type = ValueType.VALUE_TYPE_DOUBLE
    out.double_value = value
    return out


def make_int64_value(value: int) -> Value:
    out = Value()
    out.type = ValueType.VALUE_TYPE_INT64
    out.int64_value = value
    return out


def make_bool_value(value: bool) -> Value:
    out = Value()
    out.type = ValueType.VALUE_TYPE_BOOL
    out.bool_value = value
    return out


def ensure_ok(resp: Response, context: str) -> None:
    if resp.status.code != OK_CODE:
        raise RuntimeError(
            f"{context} failed: code={resp.status.code} message='{resp.status.message}'"
        )


def find_provider_executable() -> Path:
    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        candidate = Path(env_path)
        if candidate.exists():
            return candidate
        raise FileNotFoundError(
            f"ANOLIS_PROVIDER_SIM_EXE points to missing file: {candidate}"
        )

    candidates = [
        repo_root / "build/Release/anolis-provider-sim.exe",
        repo_root / "build/Debug/anolis-provider-sim.exe",
        repo_root / "build/anolis-provider-sim",
        repo_root / "build-tsan/anolis-provider-sim",
    ]
    for path in candidates:
        if path.exists():
            return path
    raise FileNotFoundError("Could not find anolis-provider-sim executable")


def find_fluxgraph_server() -> Path:
    env_path = os.environ.get("FLUXGRAPH_SERVER_EXE")
    if env_path:
        candidate = Path(env_path)
        if candidate.exists():
            return candidate
        raise FileNotFoundError(
            f"FLUXGRAPH_SERVER_EXE points to missing file: {candidate}"
        )

    fluxgraph_root = repo_root.parent / "fluxgraph"
    candidates = [
        fluxgraph_root / "build-release-server/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build-server/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build-release-server/server/fluxgraph-server",
        fluxgraph_root / "build-server/server/fluxgraph-server",
        fluxgraph_root / "build/server/fluxgraph-server",
    ]
    for path in candidates:
        if path.exists():
            return path
    raise FileNotFoundError(
        "Could not find fluxgraph-server executable (set FLUXGRAPH_SERVER_EXE to override)"
    )


def find_free_port(start: int = 50051, max_tries: int = 10) -> int:
    for port in range(start, start + max_tries):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.bind(("127.0.0.1", port))
                return port
            except OSError:
                continue
    raise RuntimeError(f"No free ports in range [{start}, {start + max_tries - 1}]")


def wait_for_server(port: int, timeout_sec: float) -> bool:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(0.2)
            try:
                sock.connect(("127.0.0.1", port))
                return True
            except OSError:
                time.sleep(0.1)
    return False


class ManagedProcess:
    def __init__(self, name: str, proc: subprocess.Popen):
        self.name = name
        self.proc = proc

    def terminate(self, timeout_sec: float = 2.0) -> None:
        if self.proc.poll() is not None:
            return
        self.proc.terminate()
        try:
            self.proc.wait(timeout=timeout_sec)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=timeout_sec)


processes: List[ManagedProcess] = []


def cleanup_all() -> None:
    for managed in reversed(processes):
        try:
            managed.terminate()
        except Exception:
            pass


atexit.register(cleanup_all)


def _handle_sigint(_signum, _frame) -> None:
    cleanup_all()
    raise KeyboardInterrupt


signal.signal(signal.SIGINT, _handle_sigint)


class ProviderClient:
    def __init__(
        self,
        name: str,
        exe_path: Path,
        config_path: Path,
        sim_server: str,
    ):
        self.name = name
        self.request_id = 0
        cmd = [str(exe_path), "--config", str(config_path), "--sim-server", sim_server]
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,
        )
        processes.append(ManagedProcess(name, self.proc))
        time.sleep(0.4)
        if self.proc.poll() is not None:
            raise RuntimeError(f"{name} exited early with code {self.proc.returncode}")

    def _send(self, req: Request) -> Response:
        if self.proc.poll() is not None:
            raise RuntimeError(
                f"{self.name} process exited with code {self.proc.returncode}"
            )
        payload = req.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        assert self.proc.stdin is not None
        assert self.proc.stdout is not None
        
        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

        hdr = self.proc.stdout.read(4)
        if len(hdr) != 4:
            raise RuntimeError(f"{self.name}: failed to read response header")
        (length,) = struct.unpack("<I", hdr)
        body = self.proc.stdout.read(length)
        if len(body) != length:
            raise RuntimeError(
                f"{self.name}: expected {length} response bytes, got {len(body)}"
            )
        resp = Response()
        resp.ParseFromString(body)
        return resp

    def hello(self) -> None:
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.hello.protocol_version = "v1"
        req.hello.client_name = "multi-provider-scenario"
        req.hello.client_version = "1.0.0"
        resp = self._send(req)
        ensure_ok(resp, f"{self.name} hello")

    def wait_ready(self, max_wait_ms_hint: int = 5000) -> None:
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.wait_ready.max_wait_ms_hint = max_wait_ms_hint
        resp = self._send(req)
        ensure_ok(resp, f"{self.name} wait_ready")

    def call_function(
        self, device_id: str, function_id: int, args: Optional[Dict[str, Value]] = None
    ) -> None:
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.call.device_id = device_id
        req.call.function_id = function_id
        if args:
            for key, value in args.items():
                req.call.args[key].CopyFrom(value)
        resp = self._send(req)
        ensure_ok(resp, f"{self.name} call {device_id}:{function_id}")

    def read_signal(self, device_id: str, signal_id: str) -> float:
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.read_signals.device_id = device_id
        req.read_signals.signal_ids.append(signal_id)
        resp = self._send(req)
        ensure_ok(resp, f"{self.name} read {device_id}/{signal_id}")
        values = resp.read_signals.values
        if len(values) != 1:
            raise RuntimeError(
                f"{self.name} expected one signal in read response, got {len(values)}"
            )
        value = values[0].value
        if value.type == ValueType.VALUE_TYPE_DOUBLE:
            return value.double_value
        if value.type == ValueType.VALUE_TYPE_INT64:
            return float(value.int64_value)
        if value.type == ValueType.VALUE_TYPE_BOOL:
            return 1.0 if value.bool_value else 0.0
        raise RuntimeError(
            f"{self.name} unsupported value type for {device_id}/{signal_id}: {value.type}"
        )


def average_material_window(
    extruder_client: ProviderClient,
    chamber_client: ProviderClient,
    duration_sec: float,
    sample_period_sec: float,
) -> Tuple[float, int]:
    samples: List[float] = []
    chamber_reads = 0
    deadline = time.time() + duration_sec
    while time.time() < deadline:
        samples.append(extruder_client.read_signal("tempctl1", "tc2_temp"))
        chamber_client.read_signal("tempctl0", "tc1_temp")
        chamber_reads += 1
        time.sleep(sample_period_sec)
    if not samples:
        raise RuntimeError("No samples collected for material temperature window")
    return sum(samples) / len(samples), chamber_reads


def run_scenario(port: int) -> int:
    provider_exe = find_provider_executable()
    server_exe = find_fluxgraph_server()

    chamber_cfg = repo_root / "config" / "provider-chamber.yaml"
    extruder_cfg = repo_root / "config" / "provider-extruder.yaml"
    if not chamber_cfg.exists():
        raise FileNotFoundError(f"Missing config: {chamber_cfg}")
    if not extruder_cfg.exists():
        raise FileNotFoundError(f"Missing config: {extruder_cfg}")

    server_proc = subprocess.Popen(
        [str(server_exe), "--port", str(port), "--dt", "0.1"],
        stdin=subprocess.DEVNULL,
        stdout=sys.stderr,
        stderr=sys.stderr,
    )
    processes.append(ManagedProcess("fluxgraph-server", server_proc))
    if not wait_for_server(port, timeout_sec=5.0):
        raise RuntimeError("FluxGraph server failed to accept connections within 5s")

    sim_server = f"localhost:{port}"
    chamber = ProviderClient("chamber-provider", provider_exe, chamber_cfg, sim_server)
    chamber.hello()

    extruder = ProviderClient(
        "extruder-provider", provider_exe, extruder_cfg, sim_server
    )
    extruder.hello()

    # Call wait_ready() on both providers IN PARALLEL to minimize
    # startup delay. Sequential calls cause ~400ms delta which leads to one
    # provider ticking alone initially. Parallel calls reduce delta to <50ms.
    print("[Multi-Provider Scenario] Starting both providers...")

    wait_ready_errors = queue.Queue()

    def _wait_ready(name: str, client: ProviderClient) -> None:
        try:
            client.wait_ready()
        except Exception as exc:
            wait_ready_errors.put((name, exc, traceback.format_exc()))

    chamber_thread = threading.Thread(
        target=_wait_ready, args=("chamber-provider", chamber)
    )
    extruder_thread = threading.Thread(
        target=_wait_ready, args=("extruder-provider", extruder)
    )

    chamber_thread.start()
    extruder_thread.start()

    chamber_thread.join(timeout=30.0)
    extruder_thread.join(timeout=30.0)

    if chamber_thread.is_alive() or extruder_thread.is_alive():
        raise RuntimeError("wait_ready() timed out after 30 seconds")

    if not wait_ready_errors.empty():
        name, exc, tb = wait_ready_errors.get()
        raise RuntimeError(f"{name} wait_ready() failed: {exc}\n{tb}")

    time.sleep(INIT_DELAY_SEC)

    # Extruder setup: closed-loop hotend at 230C.
    extruder.call_function(
        "tempctl1", FN_SET_MODE, {"mode": make_string_value("closed")}
    )
    extruder.call_function(
        "tempctl1", FN_SET_SETPOINT, {"value": make_double_value(230.0)}
    )

    # Chamber baseline: keep heater inactive.
    chamber.call_function("tempctl0", FN_SET_MODE, {"mode": make_string_value("open")})
    chamber.call_function(
        "tempctl0",
        FN_SET_RELAY,
        {"relay_index": make_int64_value(1), "state": make_bool_value(False)},
    )
    chamber.call_function(
        "tempctl0",
        FN_SET_RELAY,
        {"relay_index": make_int64_value(2), "state": make_bool_value(False)},
    )

    print(
        f"[Multi-Provider Scenario] Warmup: hotend to 230C for {HOTEND_WARMUP_SEC:.0f}s "
        f"(server localhost:{port})"
    )
    time.sleep(HOTEND_WARMUP_SEC)

    hotend_temp = extruder.read_signal("tempctl1", "tc1_temp")
    if abs(hotend_temp - 230.0) > 10.0:
        raise RuntimeError(
            f"Hotend failed warmup: tc1_temp={hotend_temp:.2f}C (expected within 10C of 230C)"
        )

    baseline_avg, baseline_chamber_reads = average_material_window(
        extruder, chamber, BASELINE_WINDOW_SEC, SAMPLE_PERIOD_SEC
    )
    print(
        f"[Multi-Provider Scenario] Baseline material avg={baseline_avg:.2f}C "
        f"(chamber reads={baseline_chamber_reads})"
    )

    # Chamber warmup: closed-loop to 50C.
    chamber.call_function(
        "tempctl0", FN_SET_MODE, {"mode": make_string_value("closed")}
    )
    chamber.call_function(
        "tempctl0", FN_SET_SETPOINT, {"value": make_double_value(50.0)}
    )

    print(f"[Multi-Provider Scenario] Warmup: chamber to 50C for {CHAMBER_WARMUP_SEC:.0f}s")
    time.sleep(CHAMBER_WARMUP_SEC)

    chamber_temp = chamber.read_signal("tempctl0", "tc1_temp")
    if chamber_temp < 40.0:
        raise RuntimeError(
            f"Chamber did not warm as expected: tc1_temp={chamber_temp:.2f}C (expected >= 40C)"
        )

    coupled_avg, coupled_chamber_reads = average_material_window(
        extruder, chamber, COUPLED_WINDOW_SEC, SAMPLE_PERIOD_SEC
    )
    print(
        f"[Multi-Provider Scenario] Coupled material avg={coupled_avg:.2f}C "
        f"(chamber reads={coupled_chamber_reads})"
    )

    delta = coupled_avg - baseline_avg
    print(f"[Multi-Provider Scenario] Coupling delta={delta:.2f}C")
    if delta < 8.0:
        raise RuntimeError(
            f"Cross-provider coupling assertion failed: delta={delta:.2f}C (< 8.0C)"
        )

    print("[Multi-Provider Scenario] PASS: multi-provider coupling validated")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Multi-Provider FluxGraph scenario"
    )
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
    except Exception as exc:
        print(f"[Multi-Provider Scenario] ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
