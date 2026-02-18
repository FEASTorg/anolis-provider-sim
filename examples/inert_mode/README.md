# Inert Mode Example

## What is Inert Mode?

Inert mode is the simplest simulation mode with **no physics computation**:

- ❌ NO physics simulation
- ❌ NO ticker thread
- ✅ Sensors return fixed default values
- ✅ Function calls accepted but ignored
- ✅ Fast, deterministic, zero dependencies

## When to Use Inert Mode

- **ADPP Protocol Testing** - Verify message framing, serialization
- **Provider Startup/Shutdown Testing** - Lifecycle validation
- **Function Signature Validation** - Check function IDs and parameters
- **Quick Sanity Checks** - Fast provider smoke tests
- **CI/CD Pipelines** - Lightweight integration tests

## Architecture

```
┌─────────────────────────────┐
│   Anolis Runtime / Test     │
│          Script             │
└──────────────┬──────────────┘
               │ ADPP (stdin/stdout)
               │
┌──────────────▼──────────────┐
│   Provider-Sim (Inert)      │
│                             │
│  ┌────────────────────────┐ │
│  │   NullEngine           │ │
│  │   - tick() → false     │ │
│  │   - No state changes   │ │
│  └────────────────────────┘ │
│                             │
│  Devices return defaults:   │
│  - tempctl: 23.0°C         │
│  - motorctl: 0 RPM         │
│  - relay: OFF              │
└─────────────────────────────┘
```

## Files

- **provider.yaml** - Inert mode configuration
- **test_inert.py** - Python test demonstrating inert behavior
- **README.md** - This file

## Configuration

```yaml
devices:
  - id: tempctl0
    type: tempctl
    initial_temp: 25.0
  
  - id: motorctl0
    type: motorctl
    max_speed: 3000

simulation:
  mode: inert  # No physics
```

## Running the Example

```powershell
# From anolis-provider-sim root
cd examples\01_inert_mode

# Run test
python test_inert.py
```

## Expected Output

```
============================================================
Inert Mode Example
============================================================

Use Case: ADPP protocol testing without physics simulation
Benefits: Fast, deterministic, no dependencies

[1] Listing devices...
✓ Found devices: ['tempctl0', 'motorctl0', 'relayio0', 'analogsensor0', 'sim_control']
[2] Reading signals...
  tc1_temp = 23.0
  tc2_temp = 23.0
[3] Calling set_mode function...
✓ Function call accepted
[4] Verifying no state change...
✓ No state change (as expected)

✓ Inert mode working correctly!
```

## Key Behaviors

### Device Discovery
- All configured devices appear in ListDevices
- Health status available
- Metadata correct

### Signal Reading
- **Sensors** return hardcoded defaults (defined in device implementation)
- **Actuators** return last-set value (or default if never set)
- Values never change over time (no ticker)

### Function Calls
- All functions return SUCCESS
- **NO state changes occur** - functions are no-ops
- Useful for testing function signature validity

### Performance
- Extremely fast (no computation)
- Deterministic (no time-based behavior)
- Suitable for high-frequency testing

## Comparison with Other Modes

| Feature | Inert | Non-Interacting | Sim |
|---------|-------|-----------------|-----|
| Physics | ❌ None | ✅ Built-in | ✅ External |
| Ticker | ❌ No | ✅ Yes | ✅ Yes |
| State Changes | ❌ No | ✅ Yes | ✅ Yes |
| Dependencies | None | None | FluxGraph |
| Speed | Fastest | Fast | Moderate |
| Use Case | Protocol Tests | Simple Physics | Complex Physics |

## Next Steps

- **Add physics**: See [02_non_interacting_mode](../02_non_interacting_mode/)
- **External simulation**: See [03_sim_mode](../03_sim_mode/)
- **Full integration**: See [05_full_stack_inert](../05_full_stack_inert/)

## Troubleshooting

**Problem:** `Protocol buffer not found`
**Solution:** Ensure protocol_pb2.py is generated: `.\scripts\build.ps1`

**Problem:** `Provider not found`
**Solution:** Build first: `.\scripts\build.ps1 -Release`

**Problem:** `Python import error`
**Solution:** Install protobuf: `pip install protobuf`
