# Multi-Provider Scenario

This scenario runs two `anolis-provider-sim` instances against one FluxGraph server and validates cross-provider coupling.

## What It Validates

- Distinct provider identities (`provider.name`) on one FluxGraph server.
- Shared graph load (`config/multi-provider-extrusion.yaml`) across both providers.
- Observable thermal coupling: chamber warmup raises extruder material output temperature.

## Config Files

- `config/provider-chamber.yaml`
- `config/provider-extruder.yaml`
- `config/multi-provider-extrusion.yaml`

Both provider configs point to the same `physics_config` file.

## Build Prerequisites

Provider-sim (FluxGraph enabled):

```bash
cmake --preset ci-linux-release-fluxgraph -DFLUXGRAPH_DIR=../fluxgraph
cmake --build --preset ci-linux-release-fluxgraph --parallel
```

Windows:

```powershell
cmake --preset dev-windows-release-fluxgraph -DFLUXGRAPH_DIR=..\fluxgraph
cmake --build --preset dev-windows-release-fluxgraph --parallel
```

FluxGraph server must also be built in `../fluxgraph`.

## Run the Scenario

Direct test entrypoint:

```bash
python tests/test_multi_provider_scenario.py
```

Windows:

```powershell
python tests\test_multi_provider_scenario.py
```

Or run through CTest label:

```bash
ctest --preset ci-linux-release-fluxgraph -L fluxgraph
```

```powershell
ctest --preset dev-windows-release-fluxgraph -L fluxgraph
```

## Assertions

- Hotend reaches about `230C` (within tolerance).
- Chamber warmup window reaches at least `40C`.
- Coupling delta passes threshold:
  - `avg_coupled - avg_baseline >= 8.0C`

## Environment Overrides (optional)

- `ANOLIS_PROVIDER_SIM_EXE`
- `ANOLIS_PROVIDER_SIM_BUILD_DIR`
- `FLUXGRAPH_SERVER_EXE`
