"""Protocol protobuf import bootstrap for test scripts."""

from __future__ import annotations

import importlib
from types import ModuleType


def load_protocol_module() -> ModuleType:
    """Load protocol_pb2 module from the installed anolis-protocol package."""
    return importlib.import_module("protocol_pb2")
