# Provider Configuration

This document describes the provider configuration system for anolis-provider-sim and provides guidance for hardware provider implementations.

## Overview

Provider-sim uses YAML configuration files to define which devices to instantiate at startup. This enables operators to customize device topology for different test scenarios or deployment sites without modifying code.

## Command-Line Usage

```bash
# With configuration file (required)
./anolis-provider-sim --config /path/to/config.yaml
```

## Configuration Schema

### Top-Level Structure

```yaml
provider: # Optional: Provider identity metadata
  name: chamber-provider

devices: # Required: List of devices to instantiate
  - ...

simulation: # Required: Simulation-specific parameters
  ...
```

### Provider Identity (Optional)

`provider.name` is optional, but when present it is validated and used by remote simulation registration (FluxGraph mode).

Rules:

- Pattern: `^[A-Za-z0-9_.-]{1,64}$`
- If `provider` block is present, `provider.name` is required.

### Device Specification

Each device in the `devices` list requires:

```yaml
- id: <string>        # Required: Unique device identifier (used by behavior tree)
  type: <string>      # Required: Device type (tempctl, motorctl, relayio, analogsensor)
  <parameters>        # Optional: Device-specific configuration
```

### Supported Device Types

| Type           | Description            | Optional Parameters          |
| -------------- | ---------------------- | ---------------------------- |
| `tempctl`      | Temperature controller | `initial_temp`, `temp_range` |
| `motorctl`     | Motor controller       | `max_speed`                  |
| `relayio`      | Relay/GPIO module      | (none currently)             |
| `analogsensor` | Analog sensor module   | (none currently)             |

**Note:** `chaos_control` device is always present and does not need to be configured.

### Simulation Parameters

Configures simulation behavior:

```yaml
simulation:
  mode: sim # Required: inert | non_interacting | sim
  tick_rate_hz: 10.0 # Required: Update rate (0.1-1000 Hz)
  physics_config: physics.yaml # Required when mode=sim
  ambient_temp_c: 22.0 # Optional: constant ambient input for sim mode
  ambient_signal_path: environment/ambient_temp # Optional: override ambient path
```

**Modes:**

- **`inert`** - Devices return static values, no automatic updates
- **`non_interacting`** - Each device has internal physics, no cross-device flow
- **`sim`** - External simulation engine with signal routing

**Path resolution:** `physics_config` paths are relative to provider config directory.

When `ambient_temp_c` is set in `sim` mode, provider-sim injects that constant at
`ambient_signal_path` (default `environment/ambient_temp`) each tick.

**Build Options:**

- `ENABLE_FLUXGRAPH=OFF` (default): Standalone build, `sim` mode disabled (only `inert` and `non_interacting` available)
- `ENABLE_FLUXGRAPH=ON`: Full support for all modes, including `sim`

## Physics Configuration

When `simulation.mode = sim`, a separate physics config file defines models, signal routing, and rules.

### Structure

```yaml
physics:
  models: # Physics models (thermal, mechanical, etc.)
    - ...
  signal_graph: # Signal routing with transforms
    - ...
  rules: # Automated actions based on conditions
    - ...
```

### Models

```yaml
models:
  - id: chamber_thermal # Unique model ID
    type: thermal_mass # Model type
    params:
      thermal_mass: 5000.0 # Model-specific parameters
      heat_transfer_coeff: 15.0
      initial_temp: 25.0
```

**Available models:**

- **`thermal_mass`** - Lumped-capacitance thermal model
  - Params: `thermal_mass` (J/K), `heat_transfer_coeff` (W/K), `initial_temp` (°C)

### Signal Graph

Defines signal routing with optional transforms:

```yaml
signal_graph:
  - source: device_id/signal_id # Source path
    target: model_id/input_name # Target path
    transform: # Optional transform
      type: linear
      scale: 100.0
      offset: 0.0
```

**Path format:** `device_id/signal_id` or `model_id/signal_name`

**Transform types:**

| Type              | Parameters                                     | Description            |
| ----------------- | ---------------------------------------------- | ---------------------- |
| `first_order_lag` | `tau_s` (>0), `initial_value` (opt)            | Low-pass filter        |
| `noise`           | `amplitude` (>0), `seed` (int)                 | Gaussian noise         |
| `saturation`      | `min`, `max`                                   | Clamp to range         |
| `linear`          | `scale`, `offset` (opt), `clamp_min/max` (opt) | Affine transform       |
| `deadband`        | `threshold` (≥0)                               | Suppress small changes |
| `rate_limiter`    | `max_rate_per_sec` (>0)                        | Limit rate of change   |
| `delay`           | `delay_sec` (≥0), `buffer_size` (opt)          | Time delay             |
| `moving_average`  | `window_size` (int, >0)                        | Sliding average        |

### Rules

Automated actions triggered by signal conditions:

```yaml
rules:
  - id: overheat_shutdown
    condition: "chamber_thermal/temperature > 85.0"
    on_error: log_and_continue
    actions:
      - device: tempctl0
        function: set_relay
        args:
          relay_id: 1
          enabled: false
```

**Condition syntax:** `device_id/signal_id comparator threshold`  
**Comparators:** `<`, `>`, `<=`, `>=`, `==`, `!=`

**Action execution:** Calls device function with YAML args converted to protobuf types.

### Complete Example

```yaml
# provider-config.yaml
devices:
  - id: chamber
    type: tempctl
    initial_temp: 22.0

simulation:
  mode: sim
  tick_rate_hz: 10.0
  physics_config: chamber-physics.yaml

# chamber-physics.yaml
physics:
  models:
    - id: chamber_air
      type: thermal_mass
      params:
        thermal_mass: 8000.0
        heat_transfer_coeff: 20.0
        initial_temp: 22.0

  signal_graph:
    - source: chamber/relay1_state
      target: chamber_air/heating_power
      transform: { type: linear, scale: 750.0 }

    - source: chamber_air/temperature
      target: chamber/tc1_temp
      transform: { type: first_order_lag, tau_s: 3.0 }

  rules:
    - id: overheat_protection
      condition: "chamber_air/temperature > 140.0"
      on_error: log_and_continue
      actions:
        - device: chamber
          function: set_relay
          args: { relay_id: 1, enabled: false }
```

See [demo-chamber.md](demos/demo-chamber.md) and [demo-reactor.md](demos/demo-reactor.md) for complete examples.

## Example Configurations

### Default Configuration

Matches the hardcoded device list for backward compatibility testing:

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
  noise_enabled: true
  update_rate_hz: 10
```

### Multi-Device Configuration

Demonstrates multiple instances of the same device type:

```yaml
# config/multi-tempctl.yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 25.0
    temp_range: [10.0, 50.0]

  - id: tempctl1
    type: tempctl
    initial_temp: 30.0
    temp_range: [15.0, 45.0]

  - id: motorctl0
    type: motorctl
    max_speed: 3000.0

simulation:
  noise_enabled: false
```

### Minimal Configuration

Single device for lightweight testing:

```yaml
# config/minimal.yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 20.0

simulation:
  noise_enabled: false
  update_rate_hz: 5
```

## Runtime Integration

Provider configuration is specified in the anolis runtime configuration file:

```yaml
# anolis-runtime.yaml
providers:
  - id: sim0
    command: /path/to/anolis-provider-sim
    args: ["--config", "/path/to/provider-sim.yaml"]
    timeout_ms: 5000
```

The runtime:

1. Spawns the provider with the `--config` argument
2. Waits for Hello handshake
3. Calls `ListDevices` to discover available devices
4. Registers devices in the global registry

**The runtime never sees or parses the provider config file.** It only passes the path through command-line arguments.

## Device Factory Pattern

Provider-sim uses a factory pattern to instantiate devices from configuration:

```cpp
// Simplified flow
ProviderConfig config = load_config(config_path);

for (const auto& spec : config.devices) {
    auto device = DeviceFactory::create(spec.type, spec.id, spec.config);
    device_registry.push_back(device);
}

// Later, in ListDevices handler
for (const auto& device : device_registry) {
    response.add_device(device->get_info());
}
```

This pattern ensures:

- **Graceful degradation**: Failed device initialization doesn't crash the provider
- **Flexible topology**: Same device type can be instantiated multiple times with different IDs
- **Clean separation**: Device implementations don't need config-parsing logic

## Hardware Provider Guidance

This configuration system is designed to support hardware providers. Example hardware configuration:

```yaml
# hypothetical hw-provider.yaml
devices:
  - id: chamber_temp
    type: tempctl
    i2c_bus: /dev/i2c-1
    i2c_address: 0x48
    calibration_offset: -2.5

  - id: ambient_temp
    type: tempctl
    i2c_bus: /dev/i2c-1
    i2c_address: 0x49
    calibration_offset: 0.0

connection:
  retry_attempts: 3
  timeout_ms: 1000
```

### Key Principles for Hardware Providers

1. **Provider owns the schema**: Each provider defines its own YAML structure
   - Runtime doesn't validate or inspect provider config content
   - Hardware-specific parameters (I2C addresses, COM ports, etc.) stay in provider

2. **ListDevices is truth**: Provider reports only successfully connected devices
   - If config specifies 5 devices but only 3 connect, report 3
   - Missing hardware is normal - graceful degradation prevents provider crashes

3. **Device IDs and types are universal**: Runtime needs these for binding
   - `id`: Used by behavior tree to reference devices (`GetDevice("chamber_temp")`)
   - `type`: Used by runtime for capability discovery

4. **Everything else is provider-specific**: Connection details, calibration, retry logic
   - I2C addresses, COM ports, baud rates
   - Physical topology, cable assignments
   - Hardware initialization sequences

### Hardware Provider Implementation Pattern

```cpp
// Load configuration
ProviderConfig config = load_config(config_path);

// Attempt to initialize each device
for (const auto& spec : config.devices) {
    try {
        auto device = DeviceFactory::create(spec.type, spec.id, spec.config);

        // Attempt hardware connection
        if (device->connect()) {
            device_registry.push_back(device);
            LOG_INFO("Device {} connected", spec.id);
        } else {
            LOG_ERROR("Device {} not found, skipping", spec.id);
            // Continue with other devices
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize {}: {}", spec.id, e.what());
    }
}

// Runtime queries devices
std::vector<Device> list_devices() {
    std::vector<Device> out;
    for (const auto& dev : device_registry) {
        out.push_back(dev->get_info());
    }
    return out;  // Only returns successfully connected devices
}
```

## Error Handling

### Configuration Load Failures

If configuration file cannot be loaded:

- Provider logs error and exits with non-zero code
- Runtime detects provider failure and may restart per supervision policy
- Operator sees error in logs

### Device Initialization Failures

If a device fails to initialize:

- Provider logs error and continues with remaining devices
- Failed device is not added to registry
- `ListDevices` excludes failed devices
- Behavior tree sees subset of configured devices

### Invalid Configuration

If configuration contains errors:

- **Unknown device types** cause immediate startup failure with error message
- **Missing required fields** (id, type) cause immediate startup failure
- **Invalid parameter values** (e.g., negative temperatures, invalid ranges) cause immediate startup failure
- **Invalid YAML syntax** causes configuration load failure

The provider follows a **fail-fast** approach: any configuration error prevents startup to ensure correctness.

## Configuration Requirement

Provider-sim requires a configuration file:

- **`--config` argument is required**: Must specify device configuration
- **No default/hardcoded device list**: All devices must be explicitly configured

This ensures explicit, verifiable device configuration for all deployments.

## Testing

Validate configuration changes:

```bash
# Test configuration loads correctly
./build/dev-release/anolis-provider-sim --config config/provider-sim.yaml

# Verify devices via runtime
curl http://localhost:8080/v0/devices

# Run full test suite
bash ./scripts/test.sh --preset dev-release --suite all

# Run FluxGraph integration suite (requires FluxGraph-enabled build)
bash ./scripts/test.sh --preset ci-linux-release-fluxgraph --suite fluxgraph
```
