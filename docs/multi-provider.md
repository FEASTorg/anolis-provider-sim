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
bash ./scripts/build.sh --preset ci-linux-release-fluxgraph -- -DFLUXGRAPH_DIR=../fluxgraph
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
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
bash ./scripts/test.sh --preset ci-linux-release-fluxgraph --suite fluxgraph
```

```powershell
.\scripts\test.ps1 -Preset dev-windows-release-fluxgraph -Suite fluxgraph
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
