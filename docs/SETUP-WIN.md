# anolis-provider-sim — Windows Setup

Complete guide to build and validate the **Anolis Sim Provider** from scratch.

---

## Prerequisites

- Windows 10/11
- Visual Studio 2022 (Build Tools or IDE) with C++ support
- CMake ≥ 3.20
- Git
- Python 3.8+

**Note**: You'll install vcpkg as part of this guide.

---

## Step-by-Step Setup

### 1. Install vcpkg

Clone vcpkg and bootstrap it:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
```

Set `VCPKG_ROOT` environment variable permanently:

```powershell
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\tools\vcpkg', 'User')
```

**Restart your terminal** for the environment variable to take effect.

**Verify**:

```powershell
echo $env:VCPKG_ROOT
# Should show: C:\tools\vcpkg
```

**Alternative**: Set via System Properties (`Win+R` → `sysdm.cpl` → Advanced → Environment Variables → New User Variable).

### 2. Install Protobuf

```powershell
cd $env:VCPKG_ROOT
.\vcpkg install protobuf:x64-windows
```

This takes 5-10 minutes. Verify installation:

```powershell
.\vcpkg list protobuf
# Should show: protobuf:x64-windows  6.33.4  ...
```

### 3. Clone and Build Project

Navigate to where you want the project:

```powershell
cd D:\repos  # Or your preferred location
git clone https://github.com/FEASTorg/anolis-provider-sim.git
cd anolis-provider-sim
git submodule update --init --recursive  # Get anolis spec
```

Create build directory and configure:

```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

**Expected output**: `-- Configuring done`, `-- Generating done`, `-- Build files have been written to: ...`

Build the project:

```powershell
cmake --build . --config Release
```

**Expected output**: `anolis-provider-sim.vcxproj -> ...\Release\anolis-provider-sim.exe`

**Build artifact**: `build\Release\anolis-provider-sim.exe`

### 4. Quick Verification

Test the executable runs:

```powershell
.\Release\anolis-provider-sim.exe
```

**Expected stderr**: `anolis-provider-sim: starting (transport=stdio+uint32_le)`

Press `Ctrl+C` to exit or close stdin. You should see: `anolis-provider-sim: EOF on stdin; exiting cleanly`

✅ **Checkpoint**: Executable built and runs.

---

## Validation

Validate the provider with a protocol handshake test.

### 5. Generate Python Protobuf Bindings

From the project root:

```powershell
cd D:\repos\anolis-provider-sim  # Adjust to your path
$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\tools\protobuf;$env:PATH"
protoc --python_out=build --proto_path=external/anolis/spec/device-provider external/anolis/spec/device-provider/protocol.proto
```

**Verify**: Check that `protocol_pb2.py` exists in the build directory.

```powershell
Test-Path build\protocol_pb2.py
# Should show: True
```

### 6. Install Python Protobuf Library

```powershell
python -m pip install protobuf
```

**Expected output**: `Successfully installed protobuf-6.33.5` (or newer)

### 7. Run Smoke Test

```powershell
python tests\test_hello.py
```

**Expected output**:

```text
Testing: build\Release\anolis-provider-sim.exe
anolis-provider-sim: starting (transport=stdio+uint32_le)
Response: request_id: 1
status {
  code: CODE_OK
  message: "ok"
}
hello {
  protocol_version: "v1"
  provider_name: "anolis-provider-sim"
  provider_version: "0.0.3"
  ...
}

✓ Hello handshake successful
anolis-provider-sim: EOF on stdin; exiting cleanly
```

---

## Success Criteria

✅ vcpkg installed and `VCPKG_ROOT` set  
✅ protobuf:x64-windows installed  
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

1. Verify vcpkg protobuf: `cd $env:VCPKG_ROOT; .\vcpkg list protobuf`
2. Check toolchain file exists: `Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"`
3. Ensure you passed `-DCMAKE_TOOLCHAIN_FILE` to cmake
4. Clean and rebuild: `cd build; rm -r -Force *; cmake .. -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"`

### Wrong protobuf architecture

**Error**: Architecture mismatch errors

**Fix**: Ensure you installed `protobuf:x64-windows` not `protobuf:x86-windows`

### protoc not found

**Error**: `protoc : The term 'protoc' is not recognized...`

**Fix**: Make sure you set `$env:PATH` before running protoc:

```powershell
$env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\tools\protobuf;$env:PATH"
```

### Python can't import protocol_pb2

**Error**: `ModuleNotFoundError: No module named 'protocol_pb2'`

**Fix**:

1. Verify `protocol_pb2.py` exists in build directory: `Test-Path build\protocol_pb2.py`
2. Run the smoke test from the project root, not from subdirectories

### Python can't import google.protobuf

**Error**: `ModuleNotFoundError: No module named 'google'`

**Fix**: Install Python protobuf: `python -m pip install protobuf`

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

For comprehensive device testing, see test scripts in `tests\`:

- `tests\test_hello.py` - Protocol handshake validation
- `tests\test_adpp_integration.py` - Full ADPP protocol compliance
- `tests\test_multi_instance.py` - Multiple provider instances
- `tests\test_fault_injection.py` - Fault injection test suite

Or use the wrapper script:

```powershell
.\scripts\test.ps1 -Suite all
```

**Run ADPP integration tests** (6 tests):

```powershell
python tests\test_adpp_integration.py --test all
```

**Expected**: All tests pass with `All ADPP integration tests passed!`

Individual tests can be run separately:

```powershell
python tests\test_adpp_integration.py --test list_devices
python tests\test_adpp_integration.py --test describe_tempctl
python tests\test_adpp_integration.py --test temp_convergence
python tests\test_adpp_integration.py --test motor_control
python tests\test_adpp_integration.py --test relay_control
python tests\test_adpp_integration.py --test precondition_check
```

---

## Common Issues

### Windows Macro Conflicts

If you see errors like:

```text
error C2039: 'GetTickCount': is not a member of 'google::protobuf::util::TimeUtil'
error C2589: '(': illegal token on right side of '::'
```

Windows headers define macros (`min`, `max`, `GetTickCount`, etc.) that conflict with C++ code. Solution:

1. Add these defines **before** any Windows headers:

   ```cpp
   #ifdef _WIN32
   #define NOMINMAX
   #define WIN32_LEAN_AND_MEAN
   #endif
   ```

2. Use parentheses to prevent macro expansion:

   ```cpp
   (std::max)(a, b);  // Not std::max(a, b)
   (TimeUtil::GetCurrentTime)();  // Not GetCurrentTime()
   ```

### vcpkg Not Finding Packages

Ensure `VCPKG_ROOT` is set and you're using the toolchain file:

```powershell
echo $env:VCPKG_ROOT  # Should show path
cmake .. -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

### Resources

- **Architecture**: `working/planning.md` - Design principles and roadmap
- **Protocol spec**: `external/anolis/spec/device-provider/` - ADPP reference
- **Development notes**: `working/` - Phase completions and validation guides
- **Contributing**: See `external/anolis/docs/CONTRIBUTING.md` for CI/build pitfalls
