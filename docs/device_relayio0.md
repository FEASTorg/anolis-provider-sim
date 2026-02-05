# Device: relayio0 (Relay/IO Module)

## Physical Concept

**Form Factor:** Basic I/O module mounted in instrumentation backplane

**Core Components:**

- Microcontroller for I/O scanning and communication
- Relay output drivers (2 channels) - switches for external loads
- Digital input buffers (2 channels) - isolated monitoring inputs
- Communication interface for device protocol
- No persistent storage (stateless)

## Operational Pattern

**Primary Function:** General-purpose switching and digital signal monitoring

**Typical Control Flow:**

1. Host polls signals to read relay states and GPIO input levels
2. Host calls relay functions to control external loads
3. Relay state changes immediately, reflected in next signal poll
4. GPIO inputs monitor external events (switches, sensors, contacts)
5. Host detects GPIO changes via periodic polling

**Behavior:** Simple stateless control - no internal state machine or preconditions

## Protocol Interface

**Signals (read by host):**

- `relay_ch1_state`, `relay_ch2_state` (bool) - Relay states
- `gpio_input_1`, `gpio_input_2` (bool) - Digital input levels

**Functions (called by host):**

- `set_relay_ch1(enabled)` - Control relay 1
- `set_relay_ch2(enabled)` - Control relay 2

**Preconditions:** None (always callable)

## Simulation Purpose

Validates simple stateless control, boolean signal handling, and immediate command execution.

Represents: **Basic digital I/O** (simplest device pattern)
