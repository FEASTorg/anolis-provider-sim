# Demo: Chamber FluxGraph Integration

This demo uses one provider instance (`tempctl0`) with external FluxGraph simulation.

## Config

- Provider config: `config/provider-chamber.yaml`
- FluxGraph graph: `config/multi-provider-extrusion.yaml`
- Graph schema authority: FluxGraph (`docs/schema-yaml.md` in the FluxGraph repo)

## What to Observe

- `wait_ready` + physics ticker startup in `mode=sim`
- Chamber temperature (`tempctl0/tc1_temp`) rises after closed-loop heating is enabled
- Ambient injection via `simulation.ambient_temp_c`

## Run

Linux/macOS:

```bash
cmake --preset ci-linux-release-fluxgraph -DFLUXGRAPH_DIR=../fluxgraph
cmake --build --preset ci-linux-release-fluxgraph --parallel
ctest --preset ci-linux-release-fluxgraph -L fluxgraph
```

Windows:

```powershell
cmake --preset dev-windows-release-fluxgraph -DFLUXGRAPH_DIR=..\fluxgraph
cmake --build --preset dev-windows-release-fluxgraph --parallel
python tests\test_fluxgraph_integration.py
```

## Primary Validation Script

- `tests/test_fluxgraph_integration.py`
