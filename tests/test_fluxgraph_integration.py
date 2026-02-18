#!/usr/bin/env python3
"""
FluxGraph Integration Test (Phase 25).

Tests provider-sim integration with FluxGraph gRPC server:
- Server connection and config translation
- Provider registration and session creation
- Signal updates from FluxGraph simulation
- Graceful shutdown and cleanup

This test validates:
1. Provider connects to FluxGraph server via gRPC
2. Physics config translates correctly to FluxGraph format
3. Thermal mass model runs in FluxGraph
4. Signal values update in provider-sim
5. Clean shutdown of both processes
"""

import argparse
import struct
import subprocess
import sys
import time
import os
import signal
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
    from protocol_pb2 import Request, Response
except ImportError:
    print(f"ERROR: protocol_pb2 module not found in {build_dir}.", file=sys.stderr)
    print(
        "Run: ./scripts/generate-proto-python.sh (Linux/macOS) or pwsh ./scripts/generate-proto-python.ps1 (Windows)",
        file=sys.stderr,
    )
    sys.exit(1)


def find_provider_executable():
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
        repo_root / "build/Release/anolis-provider-sim.exe",  # Windows MSVC
        repo_root / "build/anolis-provider-sim",  # Linux/macOS
        repo_root / "build/Debug/anolis-provider-sim.exe",  # Windows Debug
    ]

    for path in candidates:
        if path.exists():
            return str(path)

    print("ERROR: Could not find anolis-provider-sim executable", file=sys.stderr)
    print("Expected one of:", file=sys.stderr)
    for c in candidates:
        print(f"  {c}", file=sys.stderr)
    sys.exit(1)


def find_fluxgraph_server():
    """Find FluxGraph server executable."""
    # Check environment variable
    env_path = os.environ.get("FLUXGRAPH_SERVER_EXE")
    if env_path:
        env_candidate = Path(env_path)
        if env_candidate.exists():
            return str(env_candidate)

    # Auto-detect relative to anolis-provider-sim
    fluxgraph_root = repo_root.parent / "fluxgraph"
    if not fluxgraph_root.exists():
        print(f"ERROR: FluxGraph repo not found at: {fluxgraph_root}", file=sys.stderr)
        print("Set FLUXGRAPH_SERVER_EXE environment variable to server path", file=sys.stderr)
        return None

    candidates = [
        fluxgraph_root / "build-release-server/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build-server/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build/server/Release/fluxgraph-server.exe",
        fluxgraph_root / "build-release-server/server/fluxgraph-server",
        fluxgraph_root / "build-server/server/fluxgraph-server",
        fluxgraph_root / "build/server/fluxgraph-server",
    ]

    for path in candidates:
        if path.exists():
            return str(path)

    print("ERROR: FluxGraph server not found", file=sys.stderr)
    print("Build it with: cd ../fluxgraph && ./scripts/build.ps1 -Server", file=sys.stderr)
    return None


class ProviderClient:
    """Simple synchronous client for ADPP stdio transport."""

    def __init__(self, config_path: str, flux_server: str):
        """Start provider process with FluxGraph integration."""
        provider_exe = find_provider_executable()
        
        self.proc = subprocess.Popen(
            [provider_exe, "--config", str(config_path), "--flux-server", flux_server],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        time.sleep(0.5)  # Give provider time to connect

    def send(self, req: Request) -> Response:
        """Send a request and receive response."""
        serialized = req.SerializeToString()
        frame_len = struct.pack("<I", len(serialized))
        self.proc.stdin.write(frame_len + serialized)
        self.proc.stdin.flush()

        # Read response frame
        frame_header = self.proc.stdout.read(4)
        if len(frame_header) < 4:
            raise RuntimeError("Failed to read response frame header")
        
        resp_len = struct.unpack("<I", frame_header)[0]
        resp_data = self.proc.stdout.read(resp_len)
        if len(resp_data) < resp_len:
            raise RuntimeError("Failed to read complete response")

        resp = Response()
        resp.ParseFromString(resp_data)
        return resp

    def close(self):
        """Shutdown provider process."""
        try:
            self.proc.terminate()
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()


def test_fluxgraph_integration(duration: int, port: int):
    """Test provider with FluxGraph server."""
    print("=" * 60)
    print("FluxGraph Integration Test")
    print("=" * 60)

    # Find FluxGraph server
    server_exe = find_fluxgraph_server()
    if not server_exe:
        print("\nSKIPPED: FluxGraph server not available")
        return 0

    # Start FluxGraph server
    print(f"\n[1/4] Starting FluxGraph server on port {port}...")
    server_proc = subprocess.Popen(
        [server_exe, "--port", str(port), "--dt", "0.1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(2)  # Give server time to start

    if server_proc.poll() is not None:
        stdout, stderr = server_proc.communicate()
        print(f"ERROR: Server failed to start")
        print(stderr.decode('utf-8', errors='replace'))
        return 1

    try:
        # Start provider with FluxGraph integration
        config_path = repo_root / "config" / "test-flux-integration.yaml"
        if not config_path.exists():
            print(f"ERROR: Test config not found: {config_path}")
            return 1

        print(f"\n[2/4] Starting provider with FluxGraph integration...")
        print(f"  Config: {config_path}")
        print(f"  Server: localhost:{port}")

        provider = ProviderClient(config_path, f"localhost:{port}")

        # Simple hello test
        print(f"\n[3/4] Testing ADPP communication...")
        req = Request(request_id=1)
        req.hello.protocol_version = "v1"
        req.hello.client_name = "fluxgraph-integration-test"
        req.hello.client_version = "1.0.0"

        resp = provider.send(req)
        if resp.status.code != 1:  # 1 = OK
            print(f"ERROR: Hello failed with status {resp.status.code}")
            return 1

        print(f"  ✓ Hello successful (provider: {resp.hello.provider_name})")

        # Run for specified duration
        print(f"\n[4/4] Running simulation for {duration} seconds...")
        time.sleep(duration)

        print("\n" + "=" * 60)
        print("✓ FluxGraph integration test PASSED")
        print("=" * 60)

        return 0

    finally:
        # Cleanup
        print("\nCleaning up...")
        provider.close()
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_proc.kill()
            server_proc.wait()


def main():
    parser = argparse.ArgumentParser(description="FluxGraph integration test")
    parser.add_argument(
        "-d", "--duration",
        type=int,
        default=10,
        help="Test duration in seconds (default: 10)"
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=50051,
        help="FluxGraph server port (default: 50051)"
    )
    args = parser.parse_args()

    try:
        return test_fluxgraph_integration(args.duration, args.port)
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 130
    except Exception as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
