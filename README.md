# anolis-provider-sim

Simulation device provider for anolis runtime, implementing the Anolis Device Provider Protocol (ADPP) v0.

## Overview

Provider-sim provides a **dry-run machine** with 5 simulated devices covering a variety of signal types and control patterns. This enables comprehensive validation of the anolis runtime before integrating real hardware.

### Device Roster

| Device ID        | Type                 | Signals                                                                     | Functions                                   |
| ---------------- | -------------------- | --------------------------------------------------------------------------- | ------------------------------------------- |
| `tempctl0`       | Temperature Control  | `mode`, `setpoint`, `temp_pv`, `heater_state`                              | `set_mode`, `set_setpoint`, `set_relay`     |
| `motorctl0`      | Motor Control        | `motor1_speed`, `motor2_speed`, `motor1_current`, `motor2_current`         | `set_motor1_speed`, `set_motor2_speed`      |
| `relayio0`       | Relay/IO Module      | `relay_ch1_state`, `relay_ch2_state`, `gpio_input_1`, `gpio_input_2`      | `set_relay_ch1`, `set_relay_ch2`            |
| `analogsensor0`  | Analog Sensor Module | `voltage_ch1`, `voltage_ch2`, `sensor_quality`                             | `calibrate_channel`, `inject_noise`         |
| `sim_control`    | Fault Injection      | _(none)_                                                                   | See [Fault Injection API](#fault-injection) |

Physical basis documentation for each device is available in [docs/](docs/).

## Fault Injection API

Provider-sim includes a special control device (`sim_control`) with functions for injecting deterministic failures into the simulation. This enables testing of fault handling, recovery workflows, and edge cases.

### Functions

#### `inject_device_unavailable`

Makes a device appear unavailable for a specified duration.

**Parameters:**
- `device_id` (string): Target device ID
- `duration_ms` (int64): Unavailability duration in milliseconds

**Behavior:**
- DescribeDevice returns empty capabilities
- ReadSignals returns empty signal list
- CallFunction returns `INTERNAL` error
- Automatically clears after duration expires

#### `inject_signal_fault`

Forces a signal to report `FAULT` quality for a specified duration.

**Parameters:**
- `device_id` (string): Target device ID
- `signal_id` (string): Target signal ID
- `duration_ms` (int64): Fault duration in milliseconds

**Behavior:**
- Signal quality becomes `FAULT`
- Signal value freezes at current value
- Automatically clears after duration expires

#### `inject_call_latency`

Adds artificial latency to all function calls on a device.

**Parameters:**
- `device_id` (string): Target device ID
- `latency_ms` (int64): Added latency in milliseconds

**Behavior:**
- All CallFunction requests delayed by specified amount
- Useful for testing timeout handling and responsiveness under load

#### `inject_call_failure`

Causes a specific function to fail probabilistically.

**Parameters:**
- `device_id` (string): Target device ID
- `function_id` (string): Target function ID
- `failure_rate` (double): Failure probability (0.0 = never fail, 1.0 = always fail)

**Behavior:**
- Function returns `INTERNAL` error at specified rate
- Uses uniform random distribution for probabilistic failures

#### `clear_faults`

Clears all active fault injections.

**Parameters:** _(none)_

**Behavior:**
- Removes all device unavailability faults
- Removes all signal faults
- Removes all latency injections
- Removes all failure rate injections

### Usage Example

```python
import requests

BASE_URL = "http://localhost:8080"

# Inject device unavailable for 5 seconds
requests.post(f"{BASE_URL}/v0/call/sim0/sim_control/inject_device_unavailable", json={
    "args": {
        "device_id": "tempctl0",
        "duration_ms": 5000
    }
})

# Inject 50% failure rate on set_setpoint
requests.post(f"{BASE_URL}/v0/call/sim0/sim_control/inject_call_failure", json={
    "args": {
        "device_id": "tempctl0",
        "function_id": "set_setpoint",
        "failure_rate": 0.5
    }
})

# Clear all faults
requests.post(f"{BASE_URL}/v0/call/sim0/sim_control/clear_faults", json={"args": {}})
```

## Building

### Windows (MSVC + vcpkg)

```powershell
# Install dependencies
vcpkg install protobuf grpc

# Build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux

See [docs/SETUP-LINUX.md](docs/SETUP-LINUX.md)

## Running

```bash
./build/Release/anolis-provider-sim
```

Provider listens on `localhost:50051` for ADPP connections from anolis-runtime.

## Testing

Run the validation scenario suite:

```bash
# From anolis repo root
python scripts/run_scenarios.py
```

See [../anolis/scenarios/](../anolis/scenarios/) for scenario implementations.

## Architecture

Provider-sim implements ADPP v0 using gRPC. Key components:

- **device_manager**: Routes ADPP calls to device implementations
- **Device implementations**: Simulate realistic device behaviors with state machines
- **Fault injection**: Global state tracking for injected faults
- **Transport**: gRPC server with ADPP service handlers

Physical device documentation and operational context available in [docs/](docs/).
