# Device: analogsensor0 (Analog Sensor Module)

## Physical Concept

**Form Factor:** Data acquisition module mounted in instrumentation backplane

**Core Components:**

- Microcontroller with ADC sampling and calibration logic
- Analog-to-digital converter (2 channels, 0-10V range)
- Input conditioning (filtering, protection)
- Communication interface for device protocol
- Non-volatile storage for calibration data

## Operational Pattern

**Primary Function:** Voltage measurement with quality assessment

**Quality Management:**
Controller tracks measurement quality based on noise and drift analysis:

- **GOOD:** Low noise, stable readings
- **NOISY:** Elevated noise (sensor degradation or interference)
- **FAULT:** High noise or out-of-range (disconnected sensor or wiring fault)

**Typical Control Flow:**

1. Host polls voltage signals and quality indicator
2. If quality degrades, host responds to degradation
3. Host calls `calibrate_channel()` to restore accuracy (requires GOOD quality)
4. Controller updates calibration coefficients
5. For testing: Host calls `inject_noise()` to simulate degradation

## Protocol Interface

**Signals (read by host):**

- `voltage_ch1`, `voltage_ch2` (double) - Voltage readings (0-10V)
- `sensor_quality` (string) - Quality state: "GOOD", "NOISY", or "FAULT"

**Functions (called by host):**

- `calibrate_channel(channel)` - Execute calibration (requires GOOD quality)
- `inject_noise(enabled)` - Testing: simulate signal degradation

**Preconditions:** Calibration blocked when quality is not GOOD

## Simulation Purpose

Validates floating-point signals, quality state machines, preconditions based on signal state, and gradual degradation.

Represents: **Continuous measurement with quality assessment**
