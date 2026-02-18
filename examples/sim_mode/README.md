# Sim Mode Example (FluxGraph Integration)

## What is Sim Mode?

Sim mode connects to an **external FluxGraph simulation server** for advanced physics:

- ✅ External simulation process (FluxGraph server)
- ✅ Provider sends actuators, receives sensors via gRPC
- ✅ Supports device coupling and complex physics models
- ✅ Declarative YAML configs (signal graphs, models, rules)
- ✅ FluxGraph's model library (thermal, mechanical, electrical, etc.)

## When to Use Sim Mode

- **Multi-Device Coupling** - Heat transfer, vibration, electrical coupling
- **Complex Physics Models** - CFD, FEA, nonlinear dynamics
- **Safety Rules in Simulation** - Overheat protection, interlocks
- **FluxGraph Model Library** - Reuse pre-built models
- **High-Fidelity Simulation** - Accurate physical behavior

## Architecture

```
┌────────────────────┐      gRPC       ┌────────────────────┐
│  Provider-Sim      │◄────────────────►│  FluxGraph Server  │
│                    │   Tick(Act,dt)   │                    │
│  ┌──────────────┐  │   Sensors        │  ┌──────────────┐  │
│  │ TempCtl      │  │                  │  │ Thermal Mass │  │
│  │ MotorCtl     │  │                  │  │ Heat Transfer│  │
│  │ RelayIO      │  │                  │  │ Signal Graph │  │
│  └──────────────┘  │                  │  └──────────────┘  │
│                    │                  │                    │
│  RemoteEngine      │                  │  Physics Models    │
│  + FluxGraphAdapter│                  │  + Rules Engine    │
└────────────────────┘                  └────────────────────┘
```

**Provider Config:** Device topology, tick rate, server address  
**Physics Config:** Models, signal routing, transforms, safety rules

## Physics Configuration

### Signal Graph
Routes device signals through physics models:
```yaml
signal_graph:
  # Actuator → Model Input
  - source: chamber/relay1_state
    target: chamber_thermal/heat_input
    transform:
      type: linear
      scale: 1000.0  # relay ON = 1000W
  
  # Model Output → Sensor
  - source: chamber_thermal/temperature
    target: chamber/tc1_temp
```

### Transforms
Modify signal values during routing:
- **Linear:** `output = input * scale + offset`
- **Noise:** Add Gaussian noise
- **Clamp:** Limit range
- **LUT:** Lookup table interpolation

### Safety Rules
Simulation-enforced safety interlocks:
```yaml
rules:
  - id: overheat_protection
    condition: "chamber_thermal/temperature > 140.0"
    actions:
      - device: chamber
        function: set_relay
        args:
          relay_id: 1
          enabled: false
```

## Files

- **provider.yaml** - Provider configuration (sim mode)
- **physics.yaml** - FluxGraph simulation configuration
- **test_sim.py** - Test with FluxGraph integration
- **README.md** - This file

## Provider Configuration

```yaml
devices:
  - id: chamber
    type: tempctl
    initial_temp: 25.0

simulation:
  mode: sim
  tick_rate_hz: 10.0
  physics_config: physics.yaml  # FluxGraph config
  # Server address passed via --sim-server CLI flag
```

## Physics Configuration

```yaml
# physics.yaml - FluxGraph simulation config

models:
  - id: chamber_thermal
    type: thermal_mass
    params:
      thermal_mass: 5000.0       # J/K (heat capacity)
      heat_transfer_coeff: 15.0  # W/K (to ambient)
      ambient_temp: 25.0
      initial_temp: 25.0

signal_graph:
  # Chamber heater input
  - source: chamber/relay1_state
    target: chamber_thermal/heat_input
    transform:
      type: linear
      scale: 1000.0  # relay ON = 1000W
  
  # Chamber temperature output
  - source: chamber_thermal/temperature
    target: chamber/tc1_temp
  
  # Second thermocouple with noise
  - source: chamber_thermal/temperature
    target: chamber/tc2_temp
    transform:
      type: noise
      amplitude: 0.5  # ±0.5°C measurement noise

# Safety rule: overheat shutdown
rules:
  - id: overheat_protection
    condition: "chamber_thermal/temperature > 140.0"
    actions:
      - device: chamber
        function: set_relay
        args:
          relay_id: 1
          enabled: false
```

## Running the Example

### Prerequisites

1. **Build FluxGraph Server:**
   ```powershell
   cd d:\repos_feast\fluxgraph
   .\scripts\build.ps1 -Server -Release
   ```

2. **Build Provider-Sim (with FluxGraph support):**
   ```powershell
   cd d:\repos_feast\anolis-provider-sim
   .\scripts\build.ps1 -Release
   # ENABLE_FLUXGRAPH=ON by default
   ```

### Run Example

```powershell
# Terminal 1: Start FluxGraph server
cd d:\repos_feast\fluxgraph
.\build-server\server\Release\fluxgraph-server.exe --port 50051

# Terminal 2: Run test
cd d:\repos_feast\anolis-provider-sim\examples\03_sim_mode
python test_sim.py
```

## Expected Output

```
============================================================
Sim Mode Example (FluxGraph Integration)
============================================================

Use Case: Advanced physics with external simulation
Benefits: Device coupling, complex models, safety rules

[0] Starting FluxGraph server...
✓ FluxGraph server running on localhost:50051

[1] Starting provider-sim with FluxGraph integration...
✓ Provider connected to FluxGraph

[2] Testing FluxGraph thermal simulation...
  Setting mode=closed, setpoint=80°C

  t=0.0s: TC1=25.0°C, TC2=25.2°C, relay=ON
  t=1.5s: TC1=35.8°C, TC2=36.1°C, relay=ON
  t=3.0s: TC1=45.2°C, TC2=44.9°C, relay=ON
  t=4.5s: TC1=53.1°C, TC2=53.4°C, relay=ON
  t=6.0s: TC1=59.7°C, TC2=59.3°C, relay=ON
  t=7.5s: TC1=65.1°C, TC2=65.6°C, relay=ON
  t=9.0s: TC1=69.5°C, TC2=69.2°C, relay=ON
  t=10.5s: TC1=73.0°C, TC2=72.8°C, relay=ON
  t=12.0s: TC1=75.8°C, TC2=76.1°C, relay=ON
  t=13.5s: TC1=78.0°C, TC2=77.7°C, relay=ON
✓ FluxGraph thermal physics working!

✓ Sim mode with FluxGraph integration successful!
```

## Key Behaviors

### gRPC Communication
- Provider connects to FluxGraph on startup
- Each tick sends actuator values, receives sensor values
- Timeout handling (default 5 seconds)

### Signal Routing
- Device signals mapped through graph
- Transforms applied during routing
- Clear separation of concerns

### Sensor Noise
- TC2 has ±0.5°C noise transform
- Shows realistic measurement variability
- Demonstrates signal processing pipeline

### Performance
- ~1-2ms tick latency (local gRPC)
- Suitable for 10-100 Hz simulation rates
- Network overhead minimal on localhost

## Comparison with Other Modes

| Feature | Inert | Non-Interacting | Sim |
|---------|-------|-----------------|-----|
| Physics | ❌ None | ✅ Built-in | ✅ External |
| Device Coupling | N/A | ❌ No | ✅ Yes |
| Ticker | ❌ No | ✅ Yes | ✅ Yes |
| Dependencies | None | None | FluxGraph + gRPC |
| Model Complexity | N/A | Simple | Advanced |
| Safety Rules | N/A | N/A | ✅ Yes |
| Use Case | Protocol Tests | Simple Physics | Complex Physics |

## Advanced Features

### Multi-Device Coupling
See [07_multi_device_reactor](../07_multi_device_reactor/) for example with:
- Core heater + jacket cooling + pump
- Thermal coupling between zones
- Coordinated control sequences

### Safety Rules
Physics enforces safety automatically:
- Overheat → auto-shutdown
- Pressure limits → relief valve
- Interlock conditions → disable actuators

### Signal Introspection
FluxGraph server provides introspection:
- List all available signals
- Query signal metadata (units, ranges)
- Debug signal routing

## Troubleshooting

**Problem:** `Failed to connect to FluxGraph server`  
**Solution:** Start server first: `fluxgraph-server.exe --port 50051`

**Problem:** `grpc not found` during build  
**Solution:** Build with FluxGraph support: `.\scripts\build.ps1 -Release` (default ON)

**Problem:** Timeout errors  
**Solution:** Check server is running, increase timeout in config

**Problem:** Signals not updating  
**Solution:** Verify signal graph routes actuators → sensors correctly

## Next Steps

- **Multi-device coupling**: See [07_multi_device_reactor](../07_multi_device_reactor/)
- **Full stack integration**: See [06_full_stack_sim](../06_full_stack_sim/)
- **FluxGraph standalone**: See `fluxgraph/examples/`

## Related Documentation

- [FluxGraph Configuration](https://github.com/FEASTorg/fluxgraph/docs/configuration.md)
- [Signal Graph Syntax](https://github.com/FEASTorg/fluxgraph/docs/signal-graph.md)
- [Safety Rules](https://github.com/FEASTorg/fluxgraph/docs/rules-engine.md)
