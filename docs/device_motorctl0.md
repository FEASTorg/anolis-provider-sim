# Device: motorctl0 (Motor Control Card)

## Physical Concept

**Form Factor:** Industrial controller module mounted in instrumentation backplane

**Core Components:**

- Microcontroller with PWM generation
- Dual H-bridge motor drivers for bidirectional control (two DC motors)
- Current sensing for speed estimation  
- Communication interface for device protocol
- No position feedback or persistent storage

## Operational Pattern

**Primary Function:** Variable speed control for two DC motors

**Typical Control Flow:**

1. Host polls speed signals to monitor motor RPM
2. Host calls `set_motor_duty()` to control motor speed via PWM
3. Motor speed ramps toward target based on duty cycle
4. Controller estimates speed from back-EMF or current draw
5. No position tracking - open-loop speed control only

**Motion Execution:** Simple duty cycle control, motor responds with first-order lag

## Protocol Interface

**Signals (read by host):**

- `motor1_speed`, `motor2_speed` (double) - Estimated speed in RPM
- `motor1_duty`, `motor2_duty` (double) - Current PWM duty cycle (0-1)

**Functions (called by host):**

- `set_motor_duty(motor_index, duty)` - Set PWM duty for motor 1 or 2

**Preconditions:** None (always callable, no state machine)

## Simulation Purpose

Validates simple actuator control, dual-channel independence, and first-order dynamics.

Represents: **Open-loop motor speed control** (simplest motion pattern)
