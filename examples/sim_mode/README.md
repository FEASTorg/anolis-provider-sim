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
bash ./scripts/run_local.sh --preset ci-linux-release-fluxgraph -- --config examples/sim_mode/provider.yaml --sim-server localhost:50051
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
.\scripts\run_local.ps1 -Preset dev-windows-release-fluxgraph -- --config examples/sim_mode/provider.yaml --sim-server localhost:50051
```

## Graph Schema Note

The file `physics.yaml` is a FluxGraph graph config (top-level `models`, `edges`, `rules`).
Provider-sim does not define this schema; see FluxGraph docs (`docs/schema-yaml.md` in the FluxGraph repo).

## Related

- [Inert mode](../inert_mode/)
- [Non-interacting mode](../non_interacting_mode/)
