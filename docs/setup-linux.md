# anolis-provider-sim Linux Setup

Build and validate `anolis-provider-sim` on Linux using repository presets.

## Prerequisites

- Ubuntu 22.04+ / Debian 12+ (or equivalent modern distro)
- GCC/Clang with C++17 support
- CMake >= 3.20
- Ninja
- Python 3.12+
- Git

## 1) Install system dependencies

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git curl zip unzip tar python3 python3-pip
```

## 2) Install and configure vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/tools/vcpkg"
"$HOME/tools/vcpkg/bootstrap-vcpkg.sh"
```

Set `VCPKG_ROOT` in your shell profile:

```bash
echo 'export VCPKG_ROOT="$HOME/tools/vcpkg"' >> ~/.bashrc
source ~/.bashrc
```

Verify:

```bash
echo "$VCPKG_ROOT"
```

## 3) Clone repository

```bash
git clone https://github.com/FEASTorg/anolis-provider-sim.git
cd anolis-provider-sim
git submodule update --init --recursive
```

## 4) Configure and build

```bash
cmake --preset dev-release
cmake --build --preset dev-release --parallel
```

This configures into `build/dev-release` and builds `anolis-provider-sim` there.

## 5) Run locally (requires `--config`)

```bash
./build/dev-release/anolis-provider-sim --config config/provider-sim.yaml
```

Notes:

- `--config` is required.
- Provider transport is stdio framed ADPP, so it waits for client frames on stdin.

## 6) Validate with tests

Smoke test:

```bash
ctest --preset dev-release -L smoke
```

Full provider baseline suite:

```bash
ctest --preset dev-release
```

## Optional: FluxGraph-enabled build (`sim` mode)

```bash
cmake --preset ci-linux-release-fluxgraph -DFLUXGRAPH_DIR=../fluxgraph
cmake --build --preset ci-linux-release-fluxgraph --parallel
ctest --preset ci-linux-release-fluxgraph -L fluxgraph
```

## Troubleshooting

`Could NOT find Protobuf` or other package errors:

- Confirm `VCPKG_ROOT` is set.
- Re-run configure via `rm -rf build/dev-release && cmake --preset dev-release`.

`mode=sim requires FluxGraph support`:

- Build with a FluxGraph-enabled preset (`*-fluxgraph`).

TSAN build launches but fails to resolve libraries:

- For `-DENABLE_TSAN=ON` builds, export runtime environment first:

```bash
BUILD_DIR=build/dev-release
VCPKG_TRIPLET=$(awk -F= '/^VCPKG_TARGET_TRIPLET:STRING=/{print $2}' "$BUILD_DIR/CMakeCache.txt")
export LD_LIBRARY_PATH="$BUILD_DIR/vcpkg_installed/${VCPKG_TRIPLET:-x64-linux}/lib:${LD_LIBRARY_PATH:-}"
export TSAN_OPTIONS="second_deadlock_stack=1 detect_deadlocks=1 history_size=7"
```

`FATAL: --config argument is required`:

- Start provider with `./build/dev-release/anolis-provider-sim --config <path>`.
