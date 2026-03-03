# Scripts Reference

This directory provides cross-platform wrappers for build, run, and test flows.

## Preset Discovery

```bash
cmake --list-presets
ctest --list-presets
```

## Build

Linux/macOS:

```bash
bash ./scripts/build.sh --preset dev-release
```

Windows:

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
```

Common options:

- `--clean` / `-Clean`
- `--jobs <N>` / `-Jobs <N>`
- `-- <extra cmake configure args>`

FluxGraph-enabled examples:

```bash
bash ./scripts/build.sh --preset ci-linux-release-fluxgraph -- -DFLUXGRAPH_DIR=../fluxgraph
```

```powershell
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
```

## Run Locally

Linux/macOS:

```bash
bash ./scripts/run_local.sh --preset dev-release -- --config config/provider-sim.yaml
```

Windows:

```powershell
.\scripts\run_local.ps1 -Preset dev-windows-release -- --config config/provider-sim.yaml
```

`--config` is required.

`sim` mode example:

```bash
bash ./scripts/run_local.sh --preset ci-linux-release-fluxgraph -- --config config/provider-chamber.yaml --sim-server localhost:50051
```

```powershell
.\scripts\run_local.ps1 -Preset dev-windows-release-fluxgraph -- --config config/provider-chamber.yaml --sim-server localhost:50051
```

## Test

Linux/macOS:

```bash
bash ./scripts/test.sh --preset dev-release --suite all
```

Windows:

```powershell
.\scripts\test.ps1 -Preset dev-windows-release -Suite all
```

Suites:

- `all` (maps to CTest label `provider`)
- `smoke`
- `adpp`
- `multi`
- `fault`
- `fluxgraph`

Verbose output:

- Linux/macOS: `-v` or `--verbose`
- Windows: `-VerboseOutput`

FluxGraph tests:

```bash
bash ./scripts/test.sh --preset ci-linux-release-fluxgraph --suite fluxgraph
```

```powershell
# Build FluxGraph server first (required for fluxgraph suite):
#   cd ..\fluxgraph
#   .\scripts\build.ps1 -Preset dev-windows-server
#   cd ..\anolis-provider-sim
.\scripts\test.ps1 -Preset dev-windows-release-fluxgraph -Suite fluxgraph
```

## Python Protobuf Generation (Direct Script Usage)

If you invoke Python tests directly (outside CTest), generate `protocol_pb2.py` in the target build directory.

Linux/macOS:

```bash
bash ./scripts/generate_proto_python.sh ./build/dev-release
```

Windows:

```powershell
.\scripts\generate_proto_python.ps1 -OutputDir .\build\dev-windows-release
```

## Practical Workflows

Quick local validation:

```bash
bash ./scripts/build.sh --preset dev-release
bash ./scripts/test.sh --preset dev-release --suite smoke
```

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
.\scripts\test.ps1 -Preset dev-windows-release -Suite smoke
```

Full local provider suite:

```bash
bash ./scripts/test.sh --preset dev-release --suite all
```

```powershell
.\scripts\test.ps1 -Preset dev-windows-release -Suite all
```

## Troubleshooting

`FATAL: --config argument is required`:

- Pass config path after `--` in `run_local.*`.

`mode=sim requires FluxGraph support`:

- Rebuild with a FluxGraph-enabled preset.

Windows preset/toolchain mismatch:

- Use `dev-windows-*` presets on Windows.
