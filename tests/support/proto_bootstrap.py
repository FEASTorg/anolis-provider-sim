"""Protocol protobuf import bootstrap for test scripts."""

from __future__ import annotations

from pathlib import Path
from types import ModuleType

from .env import add_build_dir_to_sys_path, repo_root, resolve_build_dir


def _import_hint() -> str:
    return (
        "Run one of these from repo root:\n"
        "  bash ./scripts/generate_proto_python.sh <build-dir>  (Linux/macOS)\n"
        "  pwsh ./scripts/generate_proto_python.ps1 -OutputDir <build-dir>  (Windows)"
    )


def load_protocol_module() -> tuple[ModuleType, Path]:
    """Load protocol_pb2 module from configured build directory."""
    root = repo_root()
    build_dir = resolve_build_dir(root)
    add_build_dir_to_sys_path(build_dir)

    try:
        import protocol_pb2 as protocol  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            f"protocol_pb2 module not found in {build_dir}.\n{_import_hint()}"
        ) from exc

    return protocol, build_dir
