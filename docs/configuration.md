# Provider Configuration

This document defines the `anolis-provider-sim` YAML configuration used at process startup.

## Command-Line Usage

```bash
./anolis-provider-sim --config /path/to/provider.yaml
```

`--config` is required.

## Provider Config Schema

```yaml
provider:              # Optional
  name: chamber-provider

startup_policy: strict # Optional: strict | degraded (default: strict)

devices:               # Recommended (can be empty)
  - id: tempctl0
    type: tempctl

simulation:            # Required
  mode: non_interacting
  tick_rate_hz: 10.0
```

### `provider` (optional)

If present, `provider.name` is required.

Rules:

- Pattern: `^[A-Za-z0-9_.-]{1,64}$`
- Used as provider identity in FluxGraph registration for `mode=sim`

### `devices`

Each entry uses:

- `id` (string): device instance id
- `type` (string): `tempctl`, `motorctl`, `relayio`, `analogsensor`
- Additional keys are type-specific options

Supported type options:

- `tempctl`: `initial_temp`, `temp_range`
- `motorctl`: `max_speed`
- `relayio`: none currently
- `analogsensor`: none currently

`chaos_control` is always added automatically and must not be configured in `devices`.

### `startup_policy` (optional)

Controls startup behavior when one or more configured devices fail to initialize.

- `strict` (default): abort startup on first initialization failure
- `degraded`: continue startup with successfully initialized devices only

### `simulation` (required)

`simulation.mode` is required and must be one of:

- `inert`
- `non_interacting`
- `sim`

Mode matrix:

| Mode | `tick_rate_hz` | `physics_config` | `ambient_temp_c` / `ambient_signal_path` |
| --- | --- | --- | --- |
| `inert` | forbidden | forbidden | forbidden |
| `non_interacting` | required | forbidden | forbidden |
| `sim` | required | required | optional (`ambient_signal_path` requires `ambient_temp_c`) |

Additional notes:

- `tick_rate_hz` range: `[0.1, 1000.0]`
- `physics_config` is resolved relative to the provider config file directory
- `sim` mode requires a FluxGraph-enabled build (`ENABLE_FLUXGRAPH=ON`)
- Removed keys (hard errors): `noise_enabled`, `update_rate_hz`

## Example Configs In-Repo

Default provider config:

```yaml
# config/provider-sim.yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 25.0
  - id: motorctl0
    type: motorctl
    max_speed: 3000.0
  - id: relayio0
    type: relayio
  - id: analogsensor0
    type: analogsensor

simulation:
  mode: non_interacting
  tick_rate_hz: 10.0
```

Minimal inert config:

```yaml
# config/minimal.yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 20.0

simulation:
  mode: inert
```

FluxGraph sim config:

```yaml
# config/provider-chamber.yaml
provider:
  name: chamber-provider

devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 22.0

simulation:
  mode: sim
  tick_rate_hz: 10.0
  physics_config: multi-provider-extrusion.yaml
  ambient_temp_c: 22.0
```

## FluxGraph Physics Config Boundary (`mode=sim`)

`physics_config` points to a FluxGraph graph file (for example `config/multi-provider-extrusion.yaml` or `examples/sim_mode/physics.yaml`).

Provider-sim does not define or validate the FluxGraph graph schema. It only
passes the file path/content through to FluxGraph and consumes resulting signal
updates/commands.

Use these files for concrete examples in this repo:

- [examples/sim_mode/physics.yaml](../examples/sim_mode/physics.yaml)
- [config/multi-provider-extrusion.yaml](../config/multi-provider-extrusion.yaml)

Use FluxGraph docs as the authoritative schema source:

- `https://github.com/FEASTorg/fluxgraph/blob/main/docs/schema-yaml.md`

## Runtime Integration

The runtime passes the provider config path through provider process args:

```yaml
providers:
  - id: sim0
    command: /path/to/anolis-provider-sim
    args: ["--config", "/path/to/provider.yaml"]
```

The runtime does not parse provider YAML content.

## Logging Controls

Logging is configured via environment variable (not YAML):

- `ANOLIS_PROVIDER_SIM_LOG_LEVEL=debug|info|warn|error|none`

Behavior:

- Default level: `info`
- Parsing is case-insensitive
- Invalid value: one warning is emitted and level falls back to `info`
- Logs are written to `stderr` only

`stdout` is reserved for ADPP framed protocol traffic and must not be used for
diagnostics.

## Startup Behavior

Startup behavior is policy-controlled:

- Config/schema errors are always fail-fast:
  - invalid YAML/schema
  - duplicate `devices[].id`
  - invalid simulation key matrix
- Device initialization failures:
  - `startup_policy=strict`: fail-fast
  - `startup_policy=degraded`: startup continues with successful devices only

## Validation Commands

```bash
# Local run
bash ./scripts/run_local.sh --preset dev-release -- --config config/provider-sim.yaml

# Provider baseline tests
bash ./scripts/test.sh --preset dev-release --suite all
```
