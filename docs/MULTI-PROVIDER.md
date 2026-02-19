# Multi-Provider

This document covers running two `anolis-provider-sim` instances against one FluxGraph server to validate cross-provider coupling.

## What this Validates

- Distinct provider identities (`provider.name`) registering to the same server.
- Shared physics graph loaded idempotently via deterministic `config_hash`.
- Observable coupling: chamber heating raises extruder material output temperature.

## Architecture

- `fluxgraph-server`: owns global signal/model graph and tick coordination.
- `provider-chamber` (`tempctl0`): writes chamber actuators, reads chamber sensors.
- `provider-extruder` (`tempctl1`, `motorctl0`): writes extruder actuators, reads extruder sensors.
- Shared physics config: `config/multi-provider-extrusion.yaml`.

Both providers point to the same physics file. Provider client sends `config_hash` on `LoadConfig`, so the second identical load becomes no-op and does not invalidate existing sessions.

## Required Config Fields

Each provider config must include:

```yaml
provider:
  name: chamber-provider  # [A-Za-z0-9_.-], length 1-64
```

If `provider` block is present, `provider.name` is required and validated.

## Run Scenario

Prerequisites:

1. Build FluxGraph server.
2. Build `anolis-provider-sim` with FluxGraph enabled.
3. Generate Python protobuf bindings (`protocol_pb2.py`).

Run:

```bash
python scripts/test_multi_provider_scenario.py
```

Windows wrapper:

```powershell
pwsh .\scripts\test_multi_provider.ps1
```

Optional environment overrides:

- `ANOLIS_PROVIDER_SIM_EXE`
- `FLUXGRAPH_SERVER_EXE`
- `ANOLIS_PROVIDER_SIM_BUILD_DIR`

## Assertions

- Hotend reaches `230C ± 10C`.
- Chamber warm phase reaches at least `40C`.
- Material output delta assertion:
  - `avg_coupled - avg_baseline >= 8.0C`

## Known Limitations (TODO)

- No engine-level accumulation semantics for multiple writers to one target.
- Ambient baseline is provider-configured via `simulation.ambient_temp_c` (default path `environment/ambient_temp`).
