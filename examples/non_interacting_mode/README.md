# Non-Interacting Mode Example

`non_interacting` mode runs built-in local first-order dynamics with no external simulator.

- Ticker thread runs at `simulation.tick_rate_hz`
- Device models evolve independently
- No cross-device coupling

## Config

- Provider config: `examples/non_interacting_mode/provider.yaml`

## Run

Linux/macOS:

```bash
bash ./scripts/build.sh --preset dev-release
python examples/non_interacting_mode/test_non_interacting.py
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR="build/dev-windows-release"
python .\examples\non_interacting_mode\test_non_interacting.py
```

The script uses shared helpers from `tests/support/` for protocol framing and
process management; only non-interacting scenario logic lives in this file.

## Related

- [Inert mode](../inert_mode/)
- [Sim mode](../sim_mode/)
