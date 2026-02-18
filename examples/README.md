# Provider-Sim Examples

This directory contains working examples demonstrating each simulation mode and integration pattern.

## Examples

### Provider-Sim Standalone Examples

- **[inert_mode](inert_mode/)** - Protocol testing without physics simulation  
- **[non_interacting_mode](non_interacting_mode/)** - Built-in first-order physics  
- **[sim_mode](sim_mode/)** - FluxGraph external simulation integration

### Full Stack Integration Examples

_(Coming soon - see validation plan)_

## Quick Start

Each example directory contains:
- **README.md** - Overview and usage instructions
- **Configuration files** - YAML configs for provider and/or physics
- **Test script** - Python or shell script to run the example
- **Expected output** - What success looks like

## Running Examples

```powershell
# Navigate to example directory
cd examples/inert_mode

# Follow the README instructions
python test_inert.py
```

## Prerequisites

- **anolis-provider-sim** built: `.\scripts\build.ps1 -Release`
- **Python 3.8+** with protobuf package: `pip install protobuf`
- **FluxGraph** (for sim mode examples): Built with server support

## Troubleshooting

If examples fail:
1. Verify builds are up to date: `.\scripts\build.ps1 -Release`
2. Check that protocol_pb2.py exists in `build/` directory
3. Ensure Python can find protobuf module: `pip list | grep protobuf`
4. For sim mode, verify FluxGraph server is accessible

## Documentation

- [Mode Selection Guide](../docs/MODE_SELECTION.md) - Which mode to use when
- [Configuration Reference](../docs/CONFIGURATION.md) - YAML configuration syntax
- [Phase 26 Architecture](../working/archive/phase_26_completion.md) - Implementation details
