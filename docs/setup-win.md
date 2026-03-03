# anolis-provider-sim Windows Setup

Build and validate `anolis-provider-sim` on Windows using Visual Studio presets and wrapper scripts.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 (Desktop development with C++)
- CMake >= 3.20
- Git
- Python 3.12+

## 1) Install and configure vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
C:\tools\vcpkg\bootstrap-vcpkg.bat
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\tools\vcpkg', 'User')
```

Open a new PowerShell session and verify:

```powershell
echo $env:VCPKG_ROOT
```

## 2) Clone repository

```powershell
git clone https://github.com/FEASTorg/anolis-provider-sim.git
cd anolis-provider-sim
git submodule update --init --recursive
```

## 3) Configure and build

```powershell
.\scripts\build.ps1 -Preset dev-windows-release
```

This configures into `build\dev-windows-release` and builds `anolis-provider-sim.exe` there.

## 4) Run locally (requires `--config`)

```powershell
.\scripts\run_local.ps1 -Preset dev-windows-release -- --config config/provider-sim.yaml
```

Notes:

- `--config` is required.
- Provider transport is stdio framed ADPP, so it waits for client frames on stdin.

## 5) Validate with tests

Smoke test:

```powershell
.\scripts\test.ps1 -Preset dev-windows-release -Suite smoke
```

Full provider baseline suite:

```powershell
.\scripts\test.ps1 -Preset dev-windows-release -Suite all
```

## Optional: FluxGraph-enabled build (`sim` mode)

```powershell
# From sibling FluxGraph repo, build server preset first:
#   cd ..\fluxgraph
#   .\scripts\build.ps1 -Preset dev-windows-server
#   cd ..\anolis-provider-sim
.\scripts\build.ps1 -Preset dev-windows-release-fluxgraph -- -DFLUXGRAPH_DIR=..\fluxgraph
.\scripts\test.ps1 -Preset dev-windows-release-fluxgraph -Suite fluxgraph
```

## Troubleshooting

`mode=sim requires FluxGraph support`:

- Build with a FluxGraph-enabled preset (`*-fluxgraph`).

`FATAL: --config argument is required`:

- Start provider with `-- --config <path>` when using `scripts/run_local.ps1`.

Preset/toolchain mismatch on Windows:

- Use `dev-windows-*` presets (not `dev-*` Ninja presets) for local Windows builds.
