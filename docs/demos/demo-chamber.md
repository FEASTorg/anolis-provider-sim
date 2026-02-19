# Demo: Thermal Chamber

Environmental chamber simulation demonstrating basic physics integration.

## Configuration

**Provider config:** `config/demo-chamber.yaml`  
**Physics config:** `config/demo-chamber-physics.yaml`

## Devices

- `chamber` (tempctl) - Temperature controller with 2 relay outputs, 2 thermocouple inputs

## Physics Model

**chamber_air** (thermal_mass):

- Thermal mass: 8000 J/K (medium chamber)
- Heat transfer: 20 W/K (insulated walls)
- Initial temp: 22°C

## Signal Flow

1. Relay states → heating power (750W per relay via linear transform)
2. Chamber air temperature → TC1 (3.0s lag), TC2 (1.8s lag + noise)

## Safety Rule

**overheat_protection:** If chamber_air temperature exceeds 140°C, disable both relays.

## Running

```bash
cd anolis-provider-sim
.\build\Release\anolis-provider-sim.exe --config config\demo-chamber.yaml
```

## Expected Behavior

- Enable relay1 → temperature rises (~0.09°C/sec initially)
- Both TC readings follow chamber temperature with different lag times
- TC2 shows ±0.3°C measurement noise
- Rule triggers automatic shutdown at 140°C threshold

## Validation

```bash
python tests\test_demos.py
```

Verifies provider starts and physics engine initializes without errors.
