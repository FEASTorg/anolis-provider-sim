# Demo: Multi-Provider Coupling

This demo runs two provider instances against one FluxGraph server and validates cross-provider thermal coupling.

## Config

- Chamber provider: `config/provider-chamber.yaml`
- Extruder provider: `config/provider-extruder.yaml`
- Shared FluxGraph graph: `config/multi-provider-extrusion.yaml`
- Graph schema authority: FluxGraph (`docs/schema-yaml.md` in the FluxGraph repo)

## What to Observe

- Both providers register and run concurrently.
- Chamber warmup increases extruder material output temperature.
- Coupling assertion passes (`avg_coupled - avg_baseline >= 8.0C`).

## Run

Linux/macOS:

```bash
cmake --preset ci-linux-release-fluxgraph -DFLUXGRAPH_DIR=../fluxgraph
cmake --build --preset ci-linux-release-fluxgraph --parallel
python tests/test_multi_provider_scenario.py
```

Windows:

```powershell
cmake --preset dev-windows-release-fluxgraph -DFLUXGRAPH_DIR=..\fluxgraph
cmake --build --preset dev-windows-release-fluxgraph --parallel
python tests\test_multi_provider_scenario.py
```

## Primary Validation Script

- `tests/test_multi_provider_scenario.py`
