# Sim Mode Example (FluxGraph)

`sim` mode delegates simulation to FluxGraph.

- Provider sends actuator values to FluxGraph each tick
- Provider reads back sensor values and simulation-issued commands
- Supports multi-device and multi-provider coupling through shared graph state

## Config

- Provider config: `examples/sim_mode/provider.yaml`
- FluxGraph graph: `examples/sim_mode/physics.yaml`

## Build and Run

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

By default the script auto-selects a free local port and starts `fluxgraph-server`
through shared helpers in `tests/support/`. Optional flags:

```bash
python examples/sim_mode/test_sim.py --duration 20 --port 50071
```

## Graph Schema Note

The file `physics.yaml` is a FluxGraph graph config (top-level `models`, `edges`, `rules`).
Provider-sim does not define this schema; see FluxGraph docs (`docs/schema-yaml.md` in the FluxGraph repo).

## Related

- [Inert mode](../inert_mode/)
- [Non-interacting mode](../non_interacting_mode/)
