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
devices: # Required: List of devices to instantiate
  - ...

simulation: # Optional: Simulation-specific parameters
  ...
```

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

**Note:** `sim_control` device is always present and does not need to be configured.

### Simulation Parameters

Optional section for simulation-wide settings:

```yaml
simulation:
  noise_enabled: true # Enable/disable sensor noise
  update_rate_hz: 10 # Physics update rate
```

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
./build/anolis-provider-sim --config config/provider-sim.yaml

# Verify devices via runtime
curl http://localhost:8080/v0/devices

# Run full test suite
./scripts/test.sh --suite all
```

## Future: ADPP Configure Message

This configuration system (Phase 16) provides a foundation for a future ADPP protocol extension:

**Current (Phase 16)**: Config file approach

- Provider reads its own config
- No runtime validation of expected devices

**Future (Phase 17+)**: Optional ADPP Configure message

- Runtime sends expected device list to provider
- Provider reports per-device success/failure
- Enables runtime validation and hot-reload

Hardware providers should implement the config file pattern now, with awareness that Configure message support may be added later. The patterns are compatible.
