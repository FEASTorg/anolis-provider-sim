# anolis-provider-sim — Linux Setup

Complete guide to build and validate the **Anolis Sim Provider** from scratch.

---

## Prerequisites

- Ubuntu 20.04+ / Debian 11+ / Fedora 36+ / Arch (or equivalent)
- GCC ≥ 9 or Clang ≥ 10
- CMake ≥ 3.20
- Git
- Python 3.8+

**Note**: You'll install vcpkg as part of this guide.

---

## Step-by-Step Setup

### 1. Install System Dependencies

**Ubuntu/Debian**:

```bash
sudo apt update
sudo apt install build-essential cmake git curl zip unzip tar python3 python3-pip
```

**Fedora**:

```bash
sudo dnf install gcc-c++ cmake git curl zip unzip tar python3 python3-pip
```

**Arch**:

```bash
sudo pacman -S base-devel cmake git curl zip unzip tar python python-pip
```

### 2. Install vcpkg

Clone vcpkg and bootstrap it:

```bash
git clone https://github.com/microsoft/vcpkg.git ~/tools/vcpkg
cd ~/tools/vcpkg
./bootstrap-vcpkg.sh
```

Set `VCPKG_ROOT` environment variable permanently:

```bash
echo 'export VCPKG_ROOT="$HOME/tools/vcpkg"' >> ~/.bashrc
source ~/.bashrc
```

**For zsh**: Add to `~/.zshrc` instead of `~/.bashrc`  
**For fish**: Add to `~/.config/fish/config.fish` as `set -gx VCPKG_ROOT "$HOME/tools/vcpkg"`

**Verify**:

```bash
echo $VCPKG_ROOT
# Should show: /home/yourusername/tools/vcpkg
```

### 3. Install Protobuf

```bash
cd $VCPKG_ROOT
./vcpkg install protobuf:x64-linux
```

This takes 5-10 minutes. Verify installation:

```bash
./vcpkg list protobuf
# Should show: protobuf:x64-linux  6.33.4  ...
```

### 4. Clone and Build Project

Navigate to where you want the project:

```bash
cd ~/repos  # Or your preferred location
git clone https://github.com/FEASTorg/anolis-provider-sim.git
cd anolis-provider-sim
git submodule update --init --recursive  # Get anolis spec
```

Create build directory and configure:

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

**Expected output**: `-- Configuring done`, `-- Generating done`, `-- Build files have been written to: ...`

Build the project:

```bash
cmake --build . --config Release
```

**Expected output**: `anolis-provider-sim` executable created

**Build artifact**: `build/anolis-provider-sim`

### 5. Quick Verification

Test the executable runs:

```bash
./anolis-provider-sim
```

**Expected stderr**: `anolis-provider-sim: starting (transport=stdio+uint32_le)`

Press `Ctrl+C` to exit or close stdin. You should see: `anolis-provider-sim: EOF on stdin; exiting cleanly`

✅ **Checkpoint**: Executable built and runs.

---

## Validation

Validate the provider with a protocol handshake test.

### 6. Generate Python Protobuf Bindings

From the project root:

```bash
cd ~/repos/anolis-provider-sim  # Adjust to your path
export PATH="$VCPKG_ROOT/installed/x64-linux/tools/protobuf:$PATH"
protoc --python_out=build --proto_path=external/anolis/spec/device-provider external/anolis/spec/device-provider/protocol.proto
```

**Verify**: Check that `protocol_pb2.py` exists in the build directory.

```bash
ls build/protocol_pb2.py
# Should show: build/protocol_pb2.py
```

### 7. Install Python Protobuf Library

```bash
python3 -m pip install --user protobuf
```

**Expected output**: `Successfully installed protobuf-6.33.5` (or newer)

### 8. Run Smoke Test

```bash
python3 scripts/test_hello.py
```

**Expected output**:

```text
Testing: build/anolis-provider-sim
anolis-provider-sim: starting (transport=stdio+uint32_le)
Response: request_id: 1
status {
  code: CODE_OK
  message: "ok"
}
hello {
  protocol_version: "v0"
  provider_name: "anolis-provider-sim"
  provider_version: "0.0.1"
  ...
}

✓ Hello handshake successful
anolis-provider-sim: EOF on stdin; exiting cleanly
```

---

## Success Criteria

✅ System dependencies installed  
✅ vcpkg installed and `VCPKG_ROOT` set  
✅ protobuf:x64-linux installed  
✅ Project builds without errors  
✅ Executable runs and shows startup message  
✅ Python bindings generated (`build/protocol_pb2.py` exists)  
✅ Python protobuf library installed  
✅ Smoke test passes with `✓ Hello handshake successful`

**If all checkmarks pass**: Setup is complete! 🎉

---

## Troubleshooting

### CMake can't find Protobuf

**Error**: `Could NOT find Protobuf (missing: Protobuf_LIBRARIES Protobuf_INCLUDE_DIR)`

**Fix**:

1. Verify vcpkg protobuf: `cd $VCPKG_ROOT; ./vcpkg list protobuf`
2. Check toolchain file exists: `test -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" && echo OK`
3. Ensure you passed `-DCMAKE_TOOLCHAIN_FILE` to cmake
4. Clean and rebuild: `rm -rf build; mkdir build; cd build; cmake .. -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`

### Compiler not found

**Error**: `CMake Error: CMAKE_CXX_COMPILER not found`

**Fix**: Install build tools (see Step 1 system dependencies)

### Missing curl, zip, or unzip

**Error**: vcpkg bootstrap or install fails

**Fix**: Install missing tools:

- Ubuntu/Debian: `sudo apt install curl zip unzip tar`
- Fedora: `sudo dnf install curl zip unzip tar`

### protoc not found

**Error**: `bash: protoc: command not found`

**Fix**: Make sure you set `PATH` before running protoc:

```bash
export PATH="$VCPKG_ROOT/installed/x64-linux/tools/protobuf:$PATH"
```

### Python can't import protocol_pb2

**Error**: `ModuleNotFoundError: No module named 'protocol_pb2'`

**Fix**:

1. Verify `protocol_pb2.py` exists in project root: `ls protocol_pb2.py`
2. Run the smoke test from the project root, not from subdirectories

### Python can't import google.protobuf

**Error**: `ModuleNotFoundError: No module named 'google'`

**Fix**: Install Python protobuf: `python3 -m pip install --user protobuf`

### Permission denied when running executable

**Error**: `bash: ./anolis-provider-sim: Permission denied`

**Fix**: Make executable: `chmod +x build/anolis-provider-sim`

---

## What You Built

- **Transport layer**: stdio with length-prefixed protobuf framing
- **ADPP handlers**: Full protocol implementation (Hello, ListDevices, DescribeDevice, ReadSignals, Call, GetHealth)
- **Simulated devices**: Temperature controller and motor controller with physics
- **Test infrastructure**: Automated validation scripts

---

## Next Steps

**Setup Complete!** You've built and validated the provider.

### Additional Testing

For comprehensive device testing, see test scripts in `scripts/`:

- `test_hello.py` - Protocol handshake validation
- `test_phase2.py` - Device simulation and ADPP handlers

**Run full Phase 2 validation** (all 6 tests):

```bash
python3 scripts/test_phase2.py --test all
```

**Expected**: All tests pass with `🎉 All Phase 2 tests passed!`

Individual tests can be run separately:

```bash
python3 scripts/test_phase2.py --test list_devices
python3 scripts/test_phase2.py --test describe_tempctl
python3 scripts/test_phase2.py --test temp_convergence
python3 scripts/test_phase2.py --test motor_control
python3 scripts/test_phase2.py --test relay_control
python3 scripts/test_phase2.py --test precondition_check
```

### Resources

- **Architecture**: `working/planning.md` - Design principles and roadmap
- **Protocol spec**: `external/anolis/spec/device-provider/` - ADPP reference
- **Development notes**: `working/` - Phase completions and validation guides

✓ Unknown requests → `CODE_UNIMPLEMENTED`  
✓ Clean EOF handling  
✓ No stdout logging (stderr only)

**Next**: Phase 2 adds simulated devices + full ADPP handlers.
