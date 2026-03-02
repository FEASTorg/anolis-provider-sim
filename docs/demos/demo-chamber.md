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
bash ./scripts/build.sh --preset ci-linux-release-fluxgraph -- -DFLUXGRAPH_DIR=../fluxgraph
bash ./scripts/test.sh --preset ci-linux-release-fluxgraph --suite fluxgraph
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
python tests\test_fluxgraph_integration.py
```

## Primary Validation Script

- `tests/test_fluxgraph_integration.py`
