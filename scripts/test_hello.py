#!/usr/bin/env python3
"""
Smoke test for anolis-provider-sim Phase 1.
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
build_dir = repo_root / "build"
sys.path.insert(0, str(build_dir))

# Try to import protobuf generated code
try:
    from protocol_pb2 import Request, Response
except ImportError:
    print("ERROR: protocol_pb2 module not found.", file=sys.stderr)
    print("Run this first from repo root:", file=sys.stderr)
    print(
        "  protoc --python_out=build --proto_path=external/anolis/spec/device-provider external/anolis/spec/device-provider/protocol.proto",
        file=sys.stderr,
    )
    sys.exit(1)


def find_executable():
    """Find the provider executable in common build locations."""
    candidates = [
        Path("build/Release/anolis-provider-sim.exe"),  # Windows MSVC
        Path("build/anolis-provider-sim"),  # Linux/macOS
        Path("build/Debug/anolis-provider-sim.exe"),  # Windows Debug
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

    proc = subprocess.Popen(
        [exe_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
    )

    # Build Hello request
    req = Request(request_id=1)
    req.hello.protocol_version = "v0"
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
    assert (
        resp.hello.protocol_version == "v0"
    ), f"protocol version mismatch: {resp.hello.protocol_version}"
    assert (
        resp.hello.provider_name == "anolis-provider-sim"
    ), f"provider name mismatch: {resp.hello.provider_name}"

    print("\nOK: Hello handshake successful", file=sys.stderr)

    # Clean shutdown
    proc.stdin.close()
    proc.wait(timeout=1)


if __name__ == "__main__":
    main()
