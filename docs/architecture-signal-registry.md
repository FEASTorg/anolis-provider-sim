# Signal Registry Architecture

## Overview

The Signal Registry pattern provides clean separation between physics simulation and device protocol implementation. Physics engine operates on abstract signal paths, devices expose protocol-specific interfaces.

## Key Components

### ISignalSource Interface

```cpp
class ISignalSource {
    virtual std::optional<double> read_signal(const std::string& path) = 0;
    virtual void write_signal(const std::string& path, double value) = 0;
};
```

**Contract:** Path format is `device_id/signal_id`. Returns `nullopt` if signal unavailable.

### SignalRegistry

Thread-safe cache implementing `ISignalSource`. Coordinates between physics ticker and device request handlers.

**Key members:**

- `signal_cache_` - Cached signal values (`map<string, double>`)
- `physics_driven_signals_` - Signals managed by physics (`set<string>`)
- `device_reader_` - Callback to read actual device state (for actuators)
- `registry_mutex_` - Protects all operations

### SimPhysics

Physics engine depends only on `ISignalSource*`. Zero knowledge of ADPP protocol.

**Signal flow:**

1. Read actuator states via `signal_source_->read_signal()`
2. Evaluate signal graph (transforms, routing)
3. Update physics models
4. Write sensor values via `signal_source_->write_signal()`

### Device Integration

Devices check registry before returning internal state:

```cpp
// In read_signals():
std::string path = device_id + "/" + signal_id;
if (g_signal_registry && g_signal_registry->is_physics_driven(path)) {
    auto val = g_signal_registry->read_signal(path);
    if (val) return *val;
}
return internal_state[signal_id];
```

## Thread Safety

**Lock policy:**

- Physics ticker: Acquire registry mutex → read actuators → compute → write sensors → release
- Device handlers: Acquire registry mutex → read values → release
- No lock held during physics model evaluation (only during registry I/O)

**Deadlock prevention:** No nested lock acquisition. Physics and device mutexes are separate.

## Signal Routing

In `mode=sim`, routing is declarative in the FluxGraph graph file referenced by
`simulation.physics_config` (provider config only carries the path).

```yaml
edges:
  - source: device_a/actuator # Read from device
    target: model_b/input # Write to model
    transform: { type: linear, scale: 100.0 }

  - source: model_b/output # Read from model
    target: device_c/sensor # Write to device (registry cache)
    transform: { type: first_order_lag, tau_s: 2.0 }
```

FluxGraph processes this schema; provider-sim does not define or validate it.
See FluxGraph docs for authoritative schema details (`docs/schema-yaml.md` in
the FluxGraph repository).

Device reads from the registry return physics-computed values when the signal
is marked physics-driven.

## Extension Points

**Adding graph transforms/models/rules:** update the FluxGraph graph and
FluxGraph runtime as needed.

**Adding devices:** include registry checks in device `read_signals()`
implementations.

## Benefits

- **Protocol independence** - Physics has zero ADPP dependencies
- **Reusability** - Same physics engine works with any protocol via `ISignalSource`
- **Declarative config** - Full signal flow visible in YAML
- **Testability** - Mock `ISignalSource` for unit tests
- **Debuggability** - Registry can log all signal transactions

## Design Trade-offs

**Cost:** Additional indirection layer, mutex overhead on signal access

**Benefit:** Clean architecture boundaries, long-term maintainability, future-proof for signal recording/replay features
