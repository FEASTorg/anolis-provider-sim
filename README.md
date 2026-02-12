# anolis-provider-sim

Simulation device provider for anolis runtime, implementing the Anolis Device Provider Protocol (ADPP) v1.

## Overview

Provider-sim provides a **dry-run machine** with 5 simulated devices covering a variety of signal types and control patterns. This enables comprehensive validation of the anolis runtime before integrating real hardware.

### Device Roster

| Device ID       | Type                 | Signals                                                                | Functions                               |
| --------------- | -------------------- | ---------------------------------------------------------------------- | --------------------------------------- |
| `tempctl0`      | Temperature Control  | `tc1_temp`, `tc2_temp`, `relay1_state`, `relay2_state`, `control_mode`, `setpoint` | `set_mode`, `set_setpoint`, `set_relay` |
| `motorctl0`     | Motor Control        | `motor1_speed`, `motor2_speed`, `motor1_duty`, `motor2_duty`           | `set_motor_duty`                         |
| `relayio0`      | Relay/IO Module      | `relay_ch1_state`, `relay_ch2_state`, `gpio_input_1`, `gpio_input_2` | `set_relay_ch1`, `set_relay_ch2`        |
| `analogsensor0` | Analog Sensor Module | `voltage_ch1`, `voltage_ch2`, `sensor_quality`                         | `calibrate_channel`, `inject_noise`      |
| `sim_control`   | Fault Injection      | _(none)_                                                               | See [Fault Injection API](#fault-injection-api) |

Physical basis documentation for each device is available in [docs/](docs/).

## Configuration

Provider-sim supports device configuration via YAML files. This allows operators to customize device topology without code changes.

### Basic Usage

```bash
# Run with configuration file (required)
./anolis-provider-sim --config config/provider-sim.yaml
```

### Configuration Files

Provider-sim includes several example configurations:

- **`config/provider-sim.yaml`** - Default configuration matching hardcoded devices
- **`config/multi-tempctl.yaml`** - Multiple temperature controllers (demonstrates same-type devices)
- **`config/minimal.yaml`** - Single device for lightweight testing

### Configuration Schema

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for complete schema documentation and hardware provider guidance.

### Quick Example

```yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 25.0

  - id: tempctl1
    type: tempctl
    initial_temp: 30.0

simulation:
  noise_enabled: true
  update_rate_hz: 10
```

## Fault Injection API

Provider-sim includes a special control device (`sim_control`) with functions for injecting deterministic failures into the simulation. This enables testing of fault handling, recovery workflows, and edge cases.

### Functions

#### `inject_device_unavailable`

Makes a device appear unavailable for a specified duration.

**Parameters:**

- `device_id` (string): Target device ID
- `duration_ms` (int64): Unavailability duration in milliseconds

**Behavior:**

- `ListDevices` omits the unavailable device while fault is active
- `DescribeDevice` returns `NOT_FOUND` (unknown/unavailable target)
- `ReadSignals` for explicit signal requests returns `NOT_FOUND`
- `CallFunction` returns `INVALID_ARGUMENT` with injected fault message
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
- `function_id` (string): Target function ID (numeric ID as string, e.g., "1" for first function)
- `failure_rate` (double): Failure probability (0.0 = never fail, 1.0 = always fail)

**Behavior:**

- Function returns `INVALID_ARGUMENT` at specified rate
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

# Inject 50% failure rate on set_setpoint (function_id=2)
requests.post(f"{BASE_URL}/v0/call/sim0/sim_control/inject_call_failure", json={
    "args": {
        "device_id": "tempctl0",
        "function_id": "2",
        "failure_rate": 0.5
    }
})

# Clear all faults
requests.post(f"{BASE_URL}/v0/call/sim0/sim_control/clear_faults", json={"args": {}})
```

## Building

```bash
# Recommended wrappers
./scripts/build.sh
# PowerShell: .\scripts\build.ps1
```

### Windows (MSVC + vcpkg)

```powershell
# Install dependencies
vcpkg install protobuf yaml-cpp

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
# Requires --config argument
./build/Release/anolis-provider-sim --config config/provider-sim.yaml
```

Provider uses stdio+uint32_le transport for ADPP v1 communication with anolis-runtime.

## Testing

### Test Scripts

Provider-sim includes comprehensive test coverage:

```bash
# Recommended wrappers
./scripts/test.sh --suite all
# PowerShell: .\scripts\test.ps1 -Suite all
```

**Basic Protocol Tests:**

```bash
python tests/test_hello.py              # ADPP Hello handshake validation
python tests/test_adpp_integration.py   # Full ADPP protocol compliance
python tests/test_multi_instance.py     # Multiple provider instances
```

**Fault Injection Tests:**

```bash
# Run all fault injection tests
python tests/test_fault_injection.py --test all

# Run individual fault injection tests
python tests/test_fault_injection.py --test device_unavailable
python tests/test_fault_injection.py --test signal_fault
python tests/test_fault_injection.py --test call_latency
python tests/test_fault_injection.py --test call_failure
python tests/test_fault_injection.py --test clear_faults
python tests/test_fault_injection.py --test multiple_devices
```

**Integration Tests:**

```bash
# From anolis repo root (requires anolis-runtime)
python scripts/run_scenarios.py
```

All tests use the stdio+uint32_le transport and validate correct ADPP v1 behavior.

## Architecture

Provider-sim implements ADPP v1 using stdio+uint32_le transport. Key components:

- **device_manager**: Routes ADPP calls to device implementations
- **Device implementations**: Simulate realistic device behaviors with state machines
- **Fault injection**: Global state tracking for injected faults
- **Transport**: stdio with uint32_le message framing (ADPP v1 standard)

Physical device documentation and operational context available in [docs/](docs/).

## Safe Initialization in Provider-Sim

Provider-sim demonstrates compliance with the **Anolis Provider Safe Initialization Contract**. All devices initialize in safe, inactive states on startup, ensuring physical safety and predictable behavior.

### Device Safe Defaults

| Device            | Safe State                           | Rationale                                          |
| ----------------- | ------------------------------------ | -------------------------------------------------- |
| **tempctl0**      | Relays OFF, Open-loop mode           | No heating actuation; monitoring only              |
| **motorctl0**     | Duty cycle = 0 (both motors)         | No motion; PWM outputs disabled                    |
| **relayio0**      | Both relays OPEN (de-energized)      | Fail-safe state; external equipment unaffected     |
| **analogsensor0** | No output control (read-only device) | Sensor readings available; no actuation capability |
| **sim_control**   | No faults injected on startup        | Clean slate for fault injection testing            |

### Implementation Details

Device state structures use C++ member initializers to guarantee safe defaults:

**tempctl0** (`src/devices/tempctl_device.cpp`):

```cpp
struct State {
    double tc1_c = 25.0;           // Ambient temperature
    double tc2_c = 25.0;
    bool relay1 = false;           // ✅ Safe: OFF
    bool relay2 = false;           // ✅ Safe: OFF
    std::string mode = "open";     // ✅ Safe: No control action
    double setpoint_c = 60.0;      // Not active in open-loop
};
```

**motorctl0** (`src/devices/motorctl_device.cpp`):

```cpp
struct State {
    double duty1 = 0.0;  // ✅ Safe: No PWM output
    double duty2 = 0.0;  // ✅ Safe: No PWM output
    double speed1 = 0.0; // Stationary
    double speed2 = 0.0;
};
```

**relayio0** (`src/devices/relayio_device.cpp`):

```cpp
struct State {
    bool relay_ch1 = false;  // ✅ Safe: Open/de-energized
    bool relay_ch2 = false;  // ✅ Safe: Open/de-energized
    bool gpio_input_1 = false;
    bool gpio_input_2 = false;
};
```

### Contract Compliance Verification

Provider-sim meets all safe initialization requirements:

- ✅ **No Actuation**: Relays open, motors stopped, no outputs active
- ✅ **No Heating/Cooling**: Temperature controller in open-loop (monitoring only)
- ✅ **No Motion**: Motor duty cycles = 0
- ✅ **No State Assumptions**: Static initializers ensure safe state regardless of prior execution
- ✅ **Hardware Verification**: N/A (simulation); real providers must query hardware state

### Testing Safe Initialization

Validate safe defaults:

```bash
# Start provider-sim
./anolis-provider-sim

# In separate terminal, use anolis runtime
# - Runtime starts in IDLE mode (control operations blocked)
# - Devices discovered with capabilities advertised
# - Query device signals to verify safe states:
curl http://localhost:8080/v0/state/sim0/tempctl0
# Expected: relay1_state=false, relay2_state=false, control_mode="open"

curl http://localhost:8080/v0/state/sim0/motorctl0
# Expected: motor1_duty=0.0, motor2_duty=0.0, motor1_speed=0.0, motor2_speed=0.0

curl http://localhost:8080/v0/state/sim0/relayio0
# Expected: relay_ch1_state=false, relay_ch2_state=false
```

### For Hardware Provider Authors

When building providers for real hardware, use this as a reference:

1. **Read hardware state first**: Query current actuator positions/states
2. **Command safe state explicitly**: Don't assume power-on-reset is safe
3. **Log verification**: Record confirmation that safe state was achieved
4. **Use hardware disable lines**: Prefer hardware interlocks over software-only disable

Example pattern for hardware providers:

```cpp
void initialize_device() {
    // 1. Query current hardware state
    HardwareState current = read_hardware_state();

    // 2. Log current state for diagnostics
    LOG_INFO("Device startup state: " << current.to_string());

    // 3. Command safe state explicitly
    command_relay_open();
    command_motor_disable();
    command_heater_off();

    // 4. Verify safe state achieved
    HardwareState verified = read_hardware_state();
    if (!verified.is_safe()) {
        LOG_ERROR("Failed to achieve safe state: " << verified.to_string());
        // Provider should exit or mark device unavailable
    }

    LOG_INFO("Device initialized in safe state");
}
```

See [Anolis Provider Safe Initialization Contract](https://github.com/FEASTorg/anolis/blob/main/docs/providers.md#safe-initialization-contract) for complete requirements.
