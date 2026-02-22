#!/usr/bin/env python3
"""
Test multi-instance device behavior for Task 21.1.
Verifies that multiple devices of the same type maintain independent state.
"""

import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path

# Add build directory to path for protocol_pb2 import
script_dir = Path(__file__).parent
repo_root = script_dir.parent
build_dir_env = os.environ.get("ANOLIS_PROVIDER_SIM_BUILD_DIR")
build_dir = Path(build_dir_env) if build_dir_env else (repo_root / "build")
if not build_dir.is_absolute():
    build_dir = repo_root / build_dir
sys.path.insert(0, str(build_dir))

try:
    from protocol_pb2 import Request, Response, Value, ValueType
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print(
        "Run: ./scripts/generate_python_proto.sh (Linux/macOS) or pwsh ./scripts/generate_python_proto.ps1 (Windows)",
        file=sys.stderr,
    )
    sys.exit(1)


def make_string_value(s):
    v = Value()
    v.type = ValueType.VALUE_TYPE_STRING
    v.string_value = s
    return v


def make_bool_value(b):
    v = Value()
    v.type = ValueType.VALUE_TYPE_BOOL
    v.bool_value = b
    return v


class ProviderClient:
    """Simple synchronous client for ADPP stdio transport."""

    def __init__(self, exe_path, config_path=None):
        cmd = [str(exe_path)]
        if config_path:
            cmd.extend(["--config", str(config_path)])

        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,  # Pass through to see debug output
        )
        self.request_id = 0

    def send_request(self, req):
        """Send a request and return the response."""
        payload = req.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

        hdr = self.proc.stdout.read(4)
        if len(hdr) != 4:
            raise RuntimeError("Failed to read response header")

        (length,) = struct.unpack("<I", hdr)
        data = self.proc.stdout.read(length)
        if len(data) != length:
            raise RuntimeError(f"Expected {length} bytes, got {len(data)}")

        resp = Response()
        resp.ParseFromString(data)
        return resp

    def list_devices(self):
        """Call ListDevices."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.list_devices.include_health = False
        return self.send_request(req)

    def call_function(self, device_id, function_id, args=None):
        """Call a device function."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.call.device_id = device_id
        req.call.function_id = function_id
        if args:
            for k, v in args.items():
                req.call.args[k].CopyFrom(v)
        return self.send_request(req)

    def read_signals(self, device_id, signal_ids=None):
        """Call ReadSignals."""
        self.request_id += 1
        req = Request(request_id=self.request_id)
        req.read_signals.device_id = device_id
        if signal_ids:
            req.read_signals.signal_ids.extend(signal_ids)
        return self.send_request(req)

    def close(self):
        """Close the provider process."""
        self.proc.stdin.close()
        self.proc.wait(timeout=2)


def find_executable():
    """Find provider executable in common build locations."""
    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        candidate = Path(env_path)
        if candidate.exists():
            return candidate
        raise FileNotFoundError(
            f"ANOLIS_PROVIDER_SIM_EXE points to missing file: {candidate}"
        )

    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),  # Windows Release
        Path("build/Debug/anolis-provider-sim.exe"),  # Windows Debug
        Path("build/anolis-provider-sim"),  # Linux/macOS
        Path("build-tsan/anolis-provider-sim"),  # Linux TSAN
    ]
    for path in candidates:
        if path.exists():
            return path

    raise FileNotFoundError("Could not find anolis-provider-sim executable")


def test_multi_instance_independence(exe_path, config_path):
    """Test that tempctl0 and tempctl1 have independent state."""

    print("=== Test: Multi-Instance Device Independence ===")
    print(f"Provider: {exe_path}")
    print(f"Config: {config_path}\n")

    client = ProviderClient(exe_path, config_path)

    try:
        # Test 1: Verify both devices are listed
        print("Test 1: List devices...")
        resp = client.list_devices()

        # Check if response is successful - use 'list_devices' or 'list_devices_result'
        devices_list = None
        if hasattr(resp, "list_devices_result"):
            devices_list = resp.list_devices_result.devices
        elif hasattr(resp, "list_devices"):
            devices_list = resp.list_devices.devices

        if not devices_list:
            print(f"  Response: {resp}")
            raise AssertionError("ListDevices returned no devices")

        device_ids = [d.device_id for d in devices_list]
        print(f"  Found devices: {device_ids}")

        assert "tempctl0" in device_ids, "Missing tempctl0"
        assert "tempctl1" in device_ids, "Missing tempctl1"
        print("  [PASS] Both tempctl0 and tempctl1 found\n")

        # Test 2: Read initial state
        print("Test 2: Read initial state...")
        resp0_init = client.read_signals("tempctl0", ["control_mode"])
        values0_init = None
        if hasattr(resp0_init, "read_signals_result"):
            values0_init = resp0_init.read_signals_result.values
        elif hasattr(resp0_init, "read_signals"):
            values0_init = resp0_init.read_signals.values

        resp1_init = client.read_signals("tempctl1", ["control_mode"])
        values1_init = None
        if hasattr(resp1_init, "read_signals_result"):
            values1_init = resp1_init.read_signals_result.values
        elif hasattr(resp1_init, "read_signals"):
            values1_init = resp1_init.read_signals.values

        print(
            f"  tempctl0 initial mode: {values0_init[0].value.string_value if values0_init else 'N/A'}"
        )
        print(
            f"  tempctl1 initial mode: {values1_init[0].value.string_value if values1_init else 'N/A'}"
        )
        print()

        # Test 2.5: Read initial temperatures (verify config application)
        print("Test 2.5: Read initial temperatures (from config)...")
        resp0_temp = client.read_signals("tempctl0", ["tc1_temp"])
        values0_temp = None
        if hasattr(resp0_temp, "read_signals_result"):
            values0_temp = resp0_temp.read_signals_result.values
        elif hasattr(resp0_temp, "read_signals"):
            values0_temp = resp0_temp.read_signals.values

        resp1_temp = client.read_signals("tempctl1", ["tc1_temp"])
        values1_temp = None
        if hasattr(resp1_temp, "read_signals_result"):
            values1_temp = resp1_temp.read_signals_result.values
        elif hasattr(resp1_temp, "read_signals"):
            values1_temp = resp1_temp.read_signals.values

        temp0 = float(values0_temp[0].value.double_value) if values0_temp else 0.0
        temp1 = float(values1_temp[0].value.double_value) if values1_temp else 0.0
        print(f"  tempctl0 tc1: {temp0} degC (config: 25.0 degC)")
        print(f"  tempctl1 tc1: {temp1} degC (config: 30.0 degC)")

        # Verify temperatures match config (allow small tolerance)
        assert abs(temp0 - 25.0) < 0.1, f"tempctl0 temperature {temp0} != 25.0"
        assert abs(temp1 - 30.0) < 0.1, f"tempctl1 temperature {temp1} != 30.0"
        print("  [PASS] Initial temperatures match config!")
        print()

        # Test 3: Set different modes on each device
        print("Test 3: Set modes independently...")

        print("  Setting tempctl0 mode = closed")
        resp = client.call_function(
            "tempctl0",
            1,  # set_mode function_id
            {"mode": make_string_value("closed")},
        )
        # Check for either 'call_result' or 'call' field
        if not (hasattr(resp, "call_result") or hasattr(resp, "call")):
            raise AssertionError(f"Call failed: {resp}")

        print("  Setting tempctl1 mode = open")
        resp = client.call_function(
            "tempctl1",
            1,  # set_mode function_id
            {"mode": make_string_value("open")},
        )
        if not (hasattr(resp, "call_result") or hasattr(resp, "call")):
            raise AssertionError(f"Call failed: {resp}")
        print("  [PASS] Functions called successfully\n")

        # Test 4: Verify states are independent
        print("Test 4: Verify independent state...")

        resp0 = client.read_signals("tempctl0", ["control_mode"])
        # Check for either field name variant
        values0 = None
        if hasattr(resp0, "read_signals_result"):
            values0 = resp0.read_signals_result.values
        elif hasattr(resp0, "read_signals"):
            values0 = resp0.read_signals.values

        if not values0:
            raise AssertionError(f"ReadSignals failed: {resp0}")
        mode0 = values0[0].value.string_value

        resp1 = client.read_signals("tempctl1", ["control_mode"])
        values1 = None
        if hasattr(resp1, "read_signals_result"):
            values1 = resp1.read_signals_result.values
        elif hasattr(resp1, "read_signals"):
            values1 = resp1.read_signals.values

        if not values1:
            raise AssertionError(f"ReadSignals failed: {resp1}")
        mode1 = values1[0].value.string_value

        print(f"  tempctl0 mode: {mode0}")
        print(f"  tempctl1 mode: {mode1}")

        assert mode0 == "closed", f"Expected tempctl0 mode='closed', got '{mode0}'"
        assert mode1 == "open", f"Expected tempctl1 mode='open', got '{mode1}'"

        print("  [PASS] States are independent!\n")

        print("=== All tests PASSED ===")
        return True

    except Exception as e:
        print(f"\n[FAIL] Test FAILED: {e}")
        return False
    finally:
        client.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Test multi-instance behavior for anolis-provider-sim"
    )
    parser.add_argument(
        "--exe",
        help="Path to anolis-provider-sim executable (optional; auto-detect if omitted)",
    )
    parser.add_argument(
        "--config",
        default="config/multi-tempctl.yaml",
        help="Path to provider config (default: config/multi-tempctl.yaml)",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).parent.parent

    if args.exe:
        exe_path = Path(args.exe)
    else:
        try:
            exe_path = find_executable()
        except FileNotFoundError as e:
            print(f"ERROR: {e}")
            sys.exit(1)

    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = repo_root / config_path

    if not exe_path.exists():
        print(f"ERROR: Provider executable not found: {exe_path}")
        sys.exit(1)

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}")
        sys.exit(1)

    success = test_multi_instance_independence(exe_path, config_path)
    sys.exit(0 if success else 1)
