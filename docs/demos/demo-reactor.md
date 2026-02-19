# Demo: Multi-Zone Reactor

Complex thermal system demonstrating multi-instance routing and thermal coupling.

## Configuration

**Provider config:** `config/demo-reactor.yaml`  
**Physics config:** `config/demo-reactor-physics.yaml`

## Devices

- `reactor_core` (tempctl) - Core temperature control (initial: 80°C)
- `reactor_jacket` (tempctl) - Cooling jacket control (initial: 40°C)
- `coolant_pump` (motorctl) - Circulation pump (max: 5000 RPM)

## Physics Models

**core_thermal** (thermal_mass):

- Thermal mass: 12000 J/K (reaction mixture)
- Heat transfer: 35 W/K
- Initial: 80°C

**jacket_thermal** (thermal_mass):

- Thermal mass: 4000 J/K (cooling water)
- Heat transfer: 25 W/K
- Initial: 40°C

## Signal Flow

1. Core relays → core heating (1200W per relay)
2. Jacket relay → jacket cooling (-800W)
3. Core temp → jacket heating (bidirectional thermal coupling)
4. Pump speed → jacket cooling (-0.15 W/RPM)
5. Model temps → device thermocouples (independent lag times)

## Safety Rules

**core_emergency_shutdown:** Core temp > 280°C → disable core heaters  
**auto_cooling_enable:** Core temp > 150°C → enable jacket chiller  
**pump_speed_boost:** Jacket temp > 60°C → set pump to 4000 RPM

## Running

```bash
cd anolis-provider-sim
.\build\Release\anolis-provider-sim.exe --config config\demo-reactor.yaml
```

## Expected Behavior

- Independent control of 3 separate devices
- Core heating affects jacket temperature through thermal coupling
- Pump speed influences cooling efficiency
- Multiple rules coordinate protection actions
- Each tempctl has independent TC lag times

## Multi-Instance Validation

Demonstrates that devices of same type (2x tempctl) have independent signal routing:

- Changing `reactor_core` relay doesn't directly affect `reactor_jacket` signals
- Each device receives physics-computed values for its specific instance
- Signal graph edges target specific device IDs, not device types

## Validation

```bash
python tests\test_demos.py
```
