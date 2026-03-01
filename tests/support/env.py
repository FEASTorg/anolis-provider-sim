"""Environment and path helpers for provider-sim test scripts."""

from __future__ import annotations

import os
import socket
from pathlib import Path


def repo_root() -> Path:
    """Return repository root path."""
    return Path(__file__).resolve().parents[2]


def resolve_build_dir(root: Path | None = None) -> Path:
    """Resolve build directory used for generated protocol_pb2 bindings."""
    root = root or repo_root()
    raw = os.environ.get("ANOLIS_PROVIDER_SIM_BUILD_DIR")
    if raw:
        candidate = Path(raw)
        if not candidate.is_absolute():
            candidate = root / candidate
        return candidate.resolve()
    return (root / "build").resolve()


def add_build_dir_to_sys_path(build_dir: Path) -> None:
    """Add build directory to sys.path for protocol_pb2 imports."""
    import sys

    build_dir_str = str(build_dir)
    if build_dir_str not in sys.path:
        sys.path.insert(0, build_dir_str)


def resolve_provider_executable(root: Path | None = None) -> Path:
    """Resolve provider executable path from env var or common build locations."""
    root = root or repo_root()

    env_path = os.environ.get("ANOLIS_PROVIDER_SIM_EXE")
    if env_path:
        candidate = Path(env_path)
        if not candidate.is_absolute():
            candidate = root / candidate
        if candidate.exists():
            return candidate.resolve()
        raise FileNotFoundError(f"ANOLIS_PROVIDER_SIM_EXE points to missing file: {candidate}")

    candidates = [
        root / "build" / "Release" / "anolis-provider-sim.exe",
        root / "build" / "Debug" / "anolis-provider-sim.exe",
        root / "build" / "anolis-provider-sim",
        root / "build-tsan" / "anolis-provider-sim",
        root / "build" / "dev-release" / "anolis-provider-sim",
        root / "build" / "dev-debug" / "anolis-provider-sim",
        root / "build" / "dev-windows-release" / "Release" / "anolis-provider-sim.exe",
        root / "build" / "dev-windows-debug" / "Debug" / "anolis-provider-sim.exe",
        root / "build" / "ci-linux-release" / "anolis-provider-sim",
        root / "build" / "ci-linux-release-strict" / "anolis-provider-sim",
        root / "build" / "ci-windows-release" / "Release" / "anolis-provider-sim.exe",
        root / "build" / "ci-windows-release-strict" / "Release" / "anolis-provider-sim.exe",
        root / "build" / "ci-linux-release-fluxgraph" / "anolis-provider-sim",
        root / "build" / "ci-linux-release-fluxgraph-strict" / "anolis-provider-sim",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    candidate_text = "\n".join(f"  - {path}" for path in candidates)
    raise FileNotFoundError("Could not find anolis-provider-sim executable. Checked:\n" + candidate_text)


def resolve_fluxgraph_server(root: Path | None = None) -> Path:
    """Resolve FluxGraph server executable path."""
    root = root or repo_root()

    env_path = os.environ.get("FLUXGRAPH_SERVER_EXE")
    if env_path:
        candidate = Path(env_path)
        if not candidate.is_absolute():
            candidate = root / candidate
        if candidate.exists():
            return candidate.resolve()
        raise FileNotFoundError(f"FLUXGRAPH_SERVER_EXE points to missing file: {candidate}")

    fluxgraph_root = root.parent / "fluxgraph"
    candidates = [
        fluxgraph_root / "build-release-server" / "server" / "Release" / "fluxgraph-server.exe",
        fluxgraph_root / "build-server" / "server" / "Release" / "fluxgraph-server.exe",
        fluxgraph_root / "build" / "server" / "Release" / "fluxgraph-server.exe",
        fluxgraph_root / "build-release-server" / "server" / "fluxgraph-server",
        fluxgraph_root / "build-server" / "server" / "fluxgraph-server",
        fluxgraph_root / "build" / "server" / "fluxgraph-server",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    candidate_text = "\n".join(f"  - {path}" for path in candidates)
    raise FileNotFoundError("Could not find fluxgraph-server executable. Checked:\n" + candidate_text)


def resolve_config_path(path: str | Path, root: Path | None = None) -> Path:
    """Resolve config path to an absolute file path."""
    root = root or repo_root()
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = root / candidate
    return candidate.resolve()


def find_free_port(start: int = 50051, max_tries: int = 10) -> int:
    """Find a free localhost TCP port in a bounded range."""
    for port in range(start, start + max_tries):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.bind(("127.0.0.1", port))
                return port
            except OSError:
                continue
    raise RuntimeError(f"No free ports in range [{start}, {start + max_tries - 1}]")
