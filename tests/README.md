# Provider-Sim Python Test Harness

## Purpose
This directory contains CTest-invoked Python integration scripts for `anolis-provider-sim`.

The shared harness under `tests/support/` centralizes process lifecycle, ADPP framed transport, environment resolution, and common assertions so each test script can focus on scenario logic.

## Shared Modules
- `support/env.py`: executable/config/build-dir/port resolution helpers.
- `support/proto_bootstrap.py`: canonical `protocol_pb2` import bootstrap and remediation messaging.
- `support/process.py`: process lifecycle and output-tail capture utilities.
- `support/framed_client.py`: ADPP stdio framed client and value builders.
- `support/assertions.py`: status/signal assertion helpers.

## Test Authoring Rules
1. Keep script entrypoints and CLI stable unless a breaking change is intentional.
2. Always issue `Hello` before protocol assertions.
3. For `simulation.mode=sim`, issue `wait_ready()` before state/control assertions.
4. Include actionable failure context (provider/server stderr tail) for startup and assertion failures.
5. Avoid fixed sleeps when polling can express readiness/progression behavior.

## Running Tests
Preferred (via CTest wrappers):
- Linux/macOS: `bash ./scripts/test.sh --preset dev-release --suite all`
- Windows: `pwsh ./scripts/test.ps1 -Preset dev-windows-release -Suite all`

Targeted suites:
- `smoke`: hello handshake baseline
- `config`: config parser + startup policy validation (`tests/test_config_startup.py`)
- `adpp`: protocol surface integration checks
- `multi`: multi-instance behavior
- `fault`: chaos fault-injection behavior (including invalid-input validation)
- `fluxgraph`: FluxGraph integration scenarios (FluxGraph-enabled builds only)

Direct script invocation requires generated Python protobuf bindings (`protocol_pb2.py`) in the chosen build directory.

Generate bindings:
- Linux/macOS: `bash ./scripts/generate_proto_python.sh <build-dir>`
- Windows: `pwsh ./scripts/generate_proto_python.ps1 -OutputDir <build-dir>`

Environment overrides:
- `ANOLIS_PROVIDER_SIM_EXE`: explicit provider executable path
- `ANOLIS_PROVIDER_SIM_BUILD_DIR`: build directory containing `protocol_pb2.py`
- `FLUXGRAPH_SERVER_EXE`: explicit FluxGraph server executable path
- `FLUXGRAPH_DIR`: explicit FluxGraph repository root used for server auto-discovery

## Troubleshooting
1. If a test hangs, run the same suite with verbose CTest output (`-VV`) and inspect stderr tails emitted by the harness.
2. If `protocol_pb2` import fails, verify generation target path and `ANOLIS_PROVIDER_SIM_BUILD_DIR`.
3. If FluxGraph tests fail early, validate server binary path and port availability before rerunning.
