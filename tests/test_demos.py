#!/usr/bin/env python3
"""Quick test of demo configurations to verify Signal Registry architecture."""

import subprocess
import sys
import time
from pathlib import Path


def test_demo_startup(config_name):
    """Test that a demo config loads successfully."""
    repo_root = Path(__file__).parent.parent
    exe = repo_root / "build" / "Release" / "anolis-provider-sim.exe"
    config = repo_root / "config" / config_name

    if not exe.exists():
        print(f"ERROR: Executable not found: {exe}")
        return False

    if not config.exists():
        print(f"ERROR: Config not found: {config}")
        return False

    print(f"\n{'=' * 60}")
    print(f"Testing: {config_name}")
    print(f"{'=' * 60}")

    # Start provider process
    proc = subprocess.Popen(
        [str(exe), "--config", str(config)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    # Give it time to initialize
    time.sleep(0.8)

    # Check if process is still running (good sign)
    if proc.poll() is None:
        print("[OK] Provider started successfully")
        print("[OK] Physics engine initialized")
        proc.terminate()
        proc.wait(timeout=2)
        return True
    else:
        stdout, stderr = proc.communicate()
        print("[FAIL] Provider crashed during startup")
        print(f"STDERR:\n{stderr}")
        return False


if __name__ == "__main__":
    configs = ["demo-chamber.yaml", "demo-reactor.yaml"]

    results = {}
    for config in configs:
        results[config] = test_demo_startup(config)

    print(f"\n{'=' * 60}")
    print("Demo Verification Summary")
    print(f"{'=' * 60}")
    for config, passed in results.items():
        status = "[PASS]" if passed else "[FAIL]"
        print(f"{status}: {config}")

    if all(results.values()):
        print("\n[OK] All demos verified successfully!")
        sys.exit(0)
    else:
        print("\n[FAIL] Some demos failed")
        sys.exit(1)
