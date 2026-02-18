# Non-Interacting Mode Example

## What is Non-Interacting Mode?

Non-interacting mode provides **built-in first-order physics simulation**:

- ✅ Built-in first-order physics models
- ✅ Ticker thread runs at configured rate (10 Hz default)
- ✅ Each device simulates independently (no coupling)
- ✅ Simple time constants (tau), saturation limits
- ❌ NO external simulation process needed
- ❌ NO device-to-device coupling

## When to Use Non-Interacting Mode

- **Standalone Testing** - No external dependencies required
- **Simple Device Behavior** - First-order convergence sufficient
- **Development** - Quick iteration without FluxGraph
- **Unit Tests** - Device-level behavior validation
- **Demos** - Self-contained demonstrations

## Architecture

```
┌─────────────────────────────┐
│   Anolis Runtime / Test     │
│          Script             │
└──────────────┬──────────────┘
               │ ADPP (stdin/stdout)
               │
┌──────────────▼──────────────┐
│   Provider-Sim              │
│   (Non-Interacting)         │
│                             │
│  ┌────────────────────────┐ │
│  │   LocalEngine          │ │
│  │   - Ticker @ 10 Hz     │ │
│  │   - dt = 0.1s          │ │
│  └────────────────────────┘ │
│               │              │
│     ┌─────────┴─────────┐   │
│     ▼                   ▼   │
│  ┌──────┐          ┌──────┐ │
│  │Temp  │          │Motor │ │
│  │Device│          │Device│ │
│  │      │          │      │ │
│  │ 1st  │          │ 1st  │ │
│  │Order │          │Order │ │
│  │Physics│         │Physics│ │
│  └──────┘          └──────┘ │
└─────────────────────────────┘

Temperature: τ = 6s
Motor Speed: τ = 2s
```

## Physics Models

### Temperature Controller (TempCtl)

**First-Order Lag:**
```
τ = 6 seconds (time constant)
dT/dt = (T_target - T_current) / τ
```

**Closed-Loop Mode:**
- PID controller adjusts heater based on setpoint
- Temperature converges exponentially: `T(t) = T_sp + (T_0 - T_sp) * e^(-t/τ)`

**Open-Loop Mode:**
- Heater power controlled manually
- Temperature responds to heater state

### Motor Controller (MotorCtl)

**First-Order Lag:**
```
τ = 2 seconds (time constant)
speed_target = duty * max_speed
dSpeed/dt = (speed_target - speed_current) / τ
```

**Convergence:**
- Duty 0.5 → 50% of max_speed
- Exponential ramp: `v(t) = v_target * (1 - e^(-t/τ))`

## Files

- **provider.yaml** - Non-interacting mode configuration
- **test_non_interacting.py** - Test demonstrating physics convergence
- **README.md** - This file

## Configuration

```yaml
devices:
  - id: chamber
    type: tempctl
    initial_temp: 25.0
    temp_range: [0, 150]
  
  - id: motor
    type: motorctl
    max_speed: 3000

simulation:
  mode: non_interacting
  tick_rate_hz: 10.0  # 10 Hz ticker
```

## Running the Example

```powershell
# From anolis-provider-sim root
cd examples\02_non_interacting_mode

# Run test (takes ~15 seconds for convergence)
python test_non_interacting.py
```

## Expected Output

```
============================================================
Non-Interacting Mode Example
============================================================

Use Case: Standalone simulation with built-in physics
Benefits: No external dependencies, simple first-order dynamics

Using provider: d:\repos_feast\anolis-provider-sim\build\Release\anolis-provider-sim.exe

[1] Testing temperature convergence...
  Setting mode=closed, setpoint=80°C

  t=0.0s: temp=25.0°C
  t=1.5s: temp=36.5°C
  t=3.0s: temp=46.1°C
  t=4.5s: temp=54.2°C
  t=6.0s: temp=60.8°C
  t=7.5s: temp=66.3°C
  t=9.0s: temp=70.7°C
  t=10.5s: temp=74.1°C
  t=12.0s: temp=76.8°C
  t=13.5s: temp=78.9°C
✓ Temperature converging to setpoint

[2] Testing motor speed ramp...
  Setting motor duty=0.5 (50%)

  t=0s: speed=0 RPM
  t=1s: speed=948 RPM
  t=2s: speed=1327 RPM
  t=3s: speed=1454 RPM
  t=4s: speed=1488 RPM
  t=5s: speed=1497 RPM
✓ Motor speed ramping up

✓ Non-interacting mode physics working!
```

## Key Behaviors

### Ticker Thread
- Runs at `tick_rate_hz` (default 10 Hz)
- Automatically started on provider initialization
- Each tick updates all device physics with `dt`

### Temperature Convergence
- **Setpoint Change** → Exponential convergence
- **Time Constant** (τ = 6s) → ~5τ = 30s to 99% convergence
- **Overshoot** → Minimal (first-order system)

### Motor Speed Ramp
- **Duty Change** → Speed ramps smoothly
- **Time Constant** (τ = 2s) → ~5τ = 10s to steady state
- **Saturation** → Limited by `max_speed`

### Independence
- Temperature changes **DO NOT** affect motor
- Motor speed changes **DO NOT** affect temperature
- Each device has isolated simulation

## Comparison with Other Modes

| Feature | Inert | Non-Interacting | Sim |
|---------|-------|-----------------|-----|
| Physics | ❌ None | ✅ Built-in | ✅ External |
| Ticker | ❌ No | ✅ Yes | ✅ Yes |
| State Changes | ❌ No | ✅ Yes | ✅ Yes |
| Device Coupling | N/A | ❌ No | ✅ Yes |
| Dependencies | None | None | FluxGraph |
| Complexity | Trivial | Simple | Advanced |
| Use Case | Protocol Tests | Simple Physics | Complex Physics |

## Limitations

### No Device Coupling
- Cannot model heat transfer between devices
- Motor vibration doesn't affect temperature sensors
- For coupling, use [sim mode](../03_sim_mode/)

### Simple Models
- First-order only (no oscillations, no PID tuning)
- Linear time-invariant systems
- For complex dynamics, use FluxGraph

### No External Disturbances
- No noise, drift, or external inputs
- For realistic sensor behavior, use FluxGraph

## Next Steps

- **Add device coupling**: See [03_sim_mode](../03_sim_mode/)
- **Complex physics**: Use FluxGraph signal graphs
- **Full integration**: See [06_full_stack_sim](../06_full_stack_sim/)

## Troubleshooting

**Problem:** Temperature not converging  
**Solution:** Wait longer (~30s for full convergence with τ=6s)

**Problem:** Motor speed not ramping  
**Solution:** Check `max_speed` in config, verify duty value 0.0-1.0

**Problem:** Ticker not running  
**Solution:** Mode must be `non_interacting`, not `inert`
