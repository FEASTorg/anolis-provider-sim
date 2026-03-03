# Provider-Sim Examples

This directory contains scenario-focused examples for each simulation mode.
Example scripts intentionally reuse the shared harness in `tests/support/` so
transport/process/protobuf handling stays centralized in one place.

## Example Directories

- `inert_mode/`: `mode=inert` (no ticker, no automatic simulation updates)
- `non_interacting_mode/`: `mode=non_interacting` (built-in local first-order dynamics)
- `sim_mode/`: `mode=sim` (external FluxGraph integration)

## Quick Start

Build provider-sim first.

Linux/macOS:

```bash
bash ./scripts/build.sh --preset dev-release
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
```

Run inert and non-interacting scenarios:

Linux/macOS:

```bash
python examples/inert_mode/test_inert.py
python examples/non_interacting_mode/test_non_interacting.py
```

Windows:

```powershell
python .\examples\inert_mode\test_inert.py
python .\examples\non_interacting_mode\test_non_interacting.py
```

If generated Python protobuf bindings are in a preset-specific build directory,
set `ANOLIS_PROVIDER_SIM_BUILD_DIR` before running examples.
Example (Windows):

```powershell
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR="build/dev-windows-release"
```

`sim_mode` also requires FluxGraph server availability.

## FluxGraph (`sim_mode`)

Linux/macOS:

```bash
bash ./scripts/build.sh --preset ci-linux-release-fluxgraph -- -DFLUXGRAPH_DIR=../fluxgraph
python examples/sim_mode/test_sim.py
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR="build/dev-windows-release-fluxgraph"
python .\examples\sim_mode\test_sim.py
```

## Notes

- Example Python scripts in each folder are scenario demos.
- CI-grade integration validation lives under `tests/`.
