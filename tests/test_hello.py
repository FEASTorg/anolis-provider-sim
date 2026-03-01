#!/usr/bin/env python3
"""Simple smoke test for anolis-provider-sim Hello handshake."""

from __future__ import annotations

import sys

from support.assertions import assert_ok
from support.env import repo_root, resolve_config_path, resolve_provider_executable
from support.framed_client import AdppClient
from support.proto_bootstrap import load_protocol_module


def main() -> int:
    protocol, _ = load_protocol_module()

    root = repo_root()
    exe_path = resolve_provider_executable(root)
    config_path = resolve_config_path("config/provider-sim.yaml", root)

    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        return 1

    print(f"Testing: {exe_path}", file=sys.stderr)
    client = AdppClient(protocol, exe_path, config_path)

    try:
        resp = client.hello(
            client_name="smoke-test",
            client_version="0.0.1",
            protocol_version="v1",
        )
        assert_ok(resp, "hello")

        assert resp.request_id == 1, f"request_id mismatch: {resp.request_id}"
        assert resp.hello.protocol_version == "v1", f"protocol version mismatch: {resp.hello.protocol_version}"
        assert resp.hello.provider_name == "anolis-provider-sim", f"provider name mismatch: {resp.hello.provider_name}"

        print("OK: Hello handshake successful", file=sys.stderr)
        return 0

    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        tail = client.output_tail(120)
        if tail:
            print("Provider stderr tail:", file=sys.stderr)
            print(tail, file=sys.stderr)
        return 1

    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
