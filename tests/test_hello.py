#!/usr/bin/env python3
"""
Simple smoke test for anolis-provider-sim.
Validates Hello handshake over stdio+uint32_le framing.
"""

import struct
import subprocess
import sys
import os
from pathlib import Path

# Add build directory to path for protocol_pb2 import
script_dir = Path(__file__).parent
repo_root = script_dir.parent
build_dir_env = os.environ.get("ANOLIS_PROVIDER_SIM_BUILD_DIR")
build_dir = Path(build_dir_env) if build_dir_env else (repo_root / "build")
if not build_dir.is_absolute():
    build_dir = repo_root / build_dir
sys.path.insert(0, str(build_dir))

# Try to import protobuf generated code
try:
    from protocol_pb2 import Request, Response
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print("Run one of these from repo root:", file=sys.stderr)
    print(
        "  ./scripts/generate_python_proto.sh  (Linux/macOS)",
        file=sys.stderr,
    )
    print(
        "  pwsh ./scripts/generate_python_proto.ps1  (Windows)",
        file=sys.stderr,
    )
    sys.exit(1)


def find_executable():
    """Find the provider executable in common build locations."""
    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        env_candidate = Path(env_path)
        if env_candidate.exists():
            return str(env_candidate)
        print(
            f"ERROR: ANOLIS_PROVIDER_SIM_EXE points to missing file: {env_candidate}",
            file=sys.stderr,
        )
        sys.exit(1)

    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),  # Windows MSVC
        Path("build/anolis-provider-sim"),  # Linux/macOS
        Path("build/Debug/anolis-provider-sim.exe"),  # Windows Debug
        Path("build-tsan/anolis-provider-sim"),  # Linux TSAN
    ]

    for path in candidates:
        if path.exists():
            return str(path)

    print("ERROR: Could not find anolis-provider-sim executable", file=sys.stderr)
    print("Expected one of:", file=sys.stderr)
    for c in candidates:
        print(f"  {c}", file=sys.stderr)
    sys.exit(1)


def main():
    exe_path = find_executable()
    print(f"Testing: {exe_path}", file=sys.stderr)

    # Use provider-sim.yaml config
    config_path = repo_root / "config" / "provider-sim.yaml"
    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        sys.exit(1)

    proc = subprocess.Popen(
        [exe_path, "--config", str(config_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
    )

    # Build Hello request
    req = Request(request_id=1)
    req.hello.protocol_version = "v1"
    req.hello.client_name = "smoke-test"
    req.hello.client_version = "0.0.1"

    # Serialize and frame
    payload = req.SerializeToString()
    frame = struct.pack("<I", len(payload)) + payload

    # Send request
    proc.stdin.write(frame)
    proc.stdin.flush()

    # Read response
    hdr = proc.stdout.read(4)
    if len(hdr) != 4:
        print("ERROR: Failed to read response header", file=sys.stderr)
        sys.exit(1)

    (length,) = struct.unpack("<I", hdr)
    data = proc.stdout.read(length)
    if len(data) != length:
        print(f"ERROR: Expected {length} bytes, got {len(data)}", file=sys.stderr)
        sys.exit(1)

    # Parse response
    resp = Response()
    resp.ParseFromString(data)

    # Validate
    print(f"Response: {resp}", file=sys.stderr)

    assert resp.request_id == 1, f"request_id mismatch: {resp.request_id}"
    assert resp.status.code == 1, f"status code not OK: {resp.status.code}"
    assert resp.hello.protocol_version == "v1", (
        f"protocol version mismatch: {resp.hello.protocol_version}"
    )
    assert resp.hello.provider_name == "anolis-provider-sim", (
        f"provider name mismatch: {resp.hello.provider_name}"
    )

    print("\nOK: Hello handshake successful", file=sys.stderr)

    # Clean shutdown
    proc.stdin.close()
    proc.wait(timeout=1)


if __name__ == "__main__":
    main()
