# Inert Mode Example

`inert` mode is the no-simulation path:

- No ticker thread
- No automatic physics updates
- Device state only changes through explicit calls

## Config

- Provider config: `examples/inert_mode/provider.yaml`

## Run

Linux/macOS:

```bash
bash ./scripts/build.sh --preset dev-release
python examples/inert_mode/test_inert.py
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR="build/dev-windows-release"
python .\examples\inert_mode\test_inert.py
```

The script uses shared helpers from `tests/support/` for protocol framing and
process management; only inert-mode scenario logic lives in this file.

## Related

- [Non-interacting mode](../non_interacting_mode/)
- [Sim mode](../sim_mode/)
