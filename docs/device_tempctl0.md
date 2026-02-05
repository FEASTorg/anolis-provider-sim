# Device: tempctl0 (Temperature Control Card)

## Physical Concept

**Form Factor:** Industrial controller module mounted in instrumentation backplane

**Core Components:**

- Microcontroller with PID control loop execution
- Thermocouple acquisition (2 channels) - digitized temperature sensors
- Relay output drivers (2 channels) - switches for heater/cooler control
- Communication interface for device protocol
- Non-volatile storage for setpoint/configuration

## Operational Pattern

**Primary Function:** Temperature regulation with manual or automatic control

**Operating Modes:**

- **Open Loop:** Host directly controls relays, temperatures monitored
- **Closed Loop:** Onboard controller maintains setpoint, host monitors progress

**Typical Control Flow:**

1. Host polls temperature signals periodically
2. Manual mode: Host calls relay functions to control heating/cooling
3. Automatic mode: Host sets mode and setpoint, controller handles relay actuation
4. Controller maintains setpoint autonomously until mode change or new setpoint

## Protocol Interface

**Signals (read by host):**

- `tc1_temp`, `tc2_temp` (double) - Temperature readings
- `relay1_state`, `relay2_state` (bool) - Relay states
- `control_mode` (string) - Current mode
- `setpoint` (double) - Target temperature

**Functions (called by host):**

- `set_mode(mode)` - Switch control mode
- `set_setpoint(value)` - Set target temperature
- `set_relay(relay_index, state)` - Manual relay control (open mode only)

**Precondition:** Manual relay control blocked when in closed-loop mode

## Simulation Purpose

Validates multi-signal coordination, mode-based preconditions, and stateful control patterns.

Represents: **Sensor → Controller → Actuator feedback loop**
