# anolis-provider-sim Scripts and Testing

## Overview

This directory contains build, test, and utility scripts for anolis-provider-sim.
All scripts have Windows (`.ps1`) and Linux/macOS (`.sh`) variants for cross-platform consistency.

---

## Build Scripts

### Windows

```powershell
.\scripts\build.ps1 [options]

Options:
  -Clean                  Remove build directory before configuring
  -Config <type>          Release (default), Debug, or RelWithDebInfo
```

**Examples:**

```powershell
# Release build (default)
.\scripts\build.ps1

# Clean debug build
.\scripts\build.ps1 -Clean -Config Debug

# Clean release build
.\scripts\build.ps1 -Clean
```

### Linux/macOS

```bash
./scripts/build.sh [options]

Options:
  --clean                 Remove build directory before configuring
  --config <type>         Release (default), Debug, or RelWithDebInfo
  --tsan                  Build with ThreadSanitizer
```

**Examples:**

```bash
# Release build
./scripts/build.sh

# Clean debug build
./scripts/build.sh --clean --config Debug

# ThreadSanitizer build for race condition detection
./scripts/build.sh --tsan
```

---

## Test Scripts

### Windows

```powershell
.\scripts\test.ps1 [options]

Options:
  -Suite <name>          Test suite: all|smoke|adpp|multi|fault (default: all)
  -BuildDir <path>       Build directory (default: build)
  -Python <command>      Python command (default: python)
```

**Examples:**

```powershell
# Run all tests
.\scripts\test.ps1

# Run smoke test only
.\scripts\test.ps1 -Suite smoke

# Run ADPP integration tests
.\scripts\test.ps1 -Suite adpp

# Run multi-instance tests
.\scripts\test.ps1 -Suite multi

# Run fault injection tests
.\scripts\test.ps1 -Suite fault
```

### Linux/macOS

```bash
./scripts/test.sh [options]

Options:
  --suite <name>         Test suite: all|smoke|adpp|multi|fault
  --build-dir <path>     Build directory
  --tsan                 Use build-tsan directory
  --python <command>     Python command
```

**Examples:**

```bash
# Run all tests
./scripts/test.sh

# Run smoke test only
./scripts/test.sh --suite smoke

# Run tests with ThreadSanitizer build
./scripts/test.sh --tsan
```

---

## Run Local Scripts

Start the provider locally for manual testing or integration with Anolis Runtime.

### Windows

```powershell
.\scripts\run_local.ps1 [options] [-- <provider-args>]

Options:
  -BuildDir <path>       Build directory (default: build)
  -Config <type>         Release (default), Debug, or RelWithDebInfo
  -- <args>              Arguments to pass to provider executable
```

**Examples:**

```powershell
# Run with minimal config (no physics)
.\scripts\run_local.ps1 -- --config config/minimal.yaml

# Run with physics mode
.\scripts\run_local.ps1 -- --config config/test-physics.yaml

# Run with FluxGraph integration
.\scripts\run_local.ps1 -- --config config/test-flux-integration.yaml --flux-server localhost:50051
```

### Linux/macOS

```bash
./scripts/run_local.sh [options]

Options:
  --build-dir <path>     Build directory (auto-detects build-tsan if present)
```

**Note:** Linux/macOS version doesn't yet support passing arguments - edit script or run executable directly.

---

## Utility Scripts

### Generate Python Protobuf Bindings

Required before running Python tests.

**Windows:**

```powershell
.\scripts\generate-proto-python.ps1
```

**Linux/macOS:**

```bash
./scripts/generate-proto-python.sh
```

Generates `build/protocol_pb2.py` from the ADPP protocol specification.

---

## Test Suite Details

### Smoke Test (`tests/test_hello.py`)

- Validates basic Hello handshake over stdio+uint32_le framing
- Confirms provider starts and responds correctly
- **Runtime: <1 second**

### ADPP Integration Tests (`tests/test_adpp_integration.py`)

- Full ADPP protocol validation
- Device enumeration, capabilities, state reading, command execution
- Tests all device types (tempctl, motorctl, relayio, analogsensor)
- **Runtime: ~5 seconds**

### Multi-Instance Test (`tests/test_multi_instance.py`)

- Multiple provider instances running concurrently
- Validates process isolation and independent state
- **Runtime: ~5 seconds**

### Fault Injection Tests (`tests/test_fault_injection.py`)

- Tests error handling and fault injection API
- Validates device failure modes and recovery
- **Runtime: ~5 seconds**

### FluxGraph Integration Test (`tests/test_fluxgraph_integration.py`)

- Phase 25 FluxGraph gRPC integration (NEW)
- Requires FluxGraph server to be built
- Validates server connection, config translation, and simulation
- **Runtime: ~10-30 seconds**

---

## FluxGraph Integration Testing

### Prerequisites

1. **Build FluxGraph server:**

   ```powershell
   cd ..\fluxgraph
   .\scripts\build.ps1 -Server
   ```

2. **Build anolis-provider-sim:**

   ```powershell
   cd ..\anolis-provider-sim
   .\scripts\build.ps1
   ```

3. **Generate Python bindings:**
   ```powershell
   .\scripts\generate-proto-python.ps1
   ```

### Run Integration Test

```powershell
python tests/test_fluxgraph_integration.py -d 10  # 10 second test
python tests/test_fluxgraph_integration.py -d 30  # 30 second test
```

### Manual Testing

1. **Start FluxGraph server:**

   ```powershell
   cd ..\fluxgraph
   .\scripts\run-server.ps1 -Port 50051
   ```

2. **In another terminal, start provider:**

   ```powershell
   cd ..\anolis-provider-sim
   .\scripts\run_local.ps1 -- --config config/test-flux-integration.yaml --flux-server localhost:50051
   ```

3. **Provider will connect and register with FluxGraph server**

---

## Common Workflows

### Quick Validation After Code Changes

```powershell
# Rebuild and run smoke test
.\scripts\build.ps1
.\scripts\test.ps1 -Suite smoke
```

### Full Validation Before Commit

```powershell
# Clean build and run all tests
.\scripts\build.ps1 -Clean
.\scripts\test.ps1 -Suite all
```

### Phase 25 FluxGraph Development

```powershell
# Terminal 1: Build and start FluxGraph server
cd ..\fluxgraph
.\scripts\build.ps1 -Server -Clean
.\scripts\run-server.ps1

# Terminal 2: Build provider-sim and run integration test
cd ..\anolis-provider-sim
.\scripts\build.ps1 -Clean
.\scripts\generate-proto-python.ps1
python tests/test_fluxgraph_integration.py -d 30
```

### Debug Build Investigation

```powershell
# Build debug version
.\scripts\build.ps1 -Config Debug

# Run with debugger
.\build\Debug\anolis-provider-sim.exe --config config/minimal.yaml
```

---

## Troubleshooting

### Tests Fail to Find Executable

**Problem:** `ERROR: Could not find anolis-provider-sim executable`

**Solution:**

```powershell
# Ensure build completed successfully
.\scripts\build.ps1

# Or set environment variable
$env:ANOLIS_PROVIDER_SIM_EXE = "D:\path\to\anolis-provider-sim.exe"
.\scripts\test.ps1
```

### Python Import Errors

**Problem:** `ERROR: protocol_pb2 module not found in build`

**Solution:**

```powershell
# Generate Python protobuf bindings
.\scripts\generate-proto-python.ps1
```

### FluxGraph Server Not Found

**Problem:** `ERROR: FluxGraph server not found`

**Solution:**

```powershell
# Build FluxGraph server
cd ..\fluxgraph
.\scripts\build.ps1 -Server

# Or set environment variable
$env:FLUXGRAPH_SERVER_EXE = "D:\path\to\fluxgraph-server.exe"
python tests/test_fluxgraph_integration.py
```

---

## Script Naming Conventions

All scripts follow consistent naming:

- `build.ps1` / `build.sh` - Build the project
- `test.ps1` / `test.sh` - Run test suites
- `run_local.ps1` / `run_local.sh` - Start provider locally
- `generate-proto-python.ps1` / `.sh` - Generate Python bindings

Tests are in `tests/` directory, not in `scripts/`.
