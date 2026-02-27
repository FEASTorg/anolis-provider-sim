"""Common assertion helpers for provider-sim test scripts."""

from __future__ import annotations

from typing import Any

OK_CODE = 1


def status_text(resp: Any) -> str:
    return f"code={resp.status.code} message='{resp.status.message}'"


def assert_status(resp: Any, expected_code: int, context: str) -> None:
    actual = resp.status.code
    if actual != expected_code:
        raise AssertionError(
            f"{context}: expected status {expected_code}, got {status_text(resp)}"
        )


def assert_ok(resp: Any, context: str) -> None:
    assert_status(resp, OK_CODE, context)


def list_devices_entries(resp: Any):
    if hasattr(resp, "list_devices"):
        return resp.list_devices.devices
    if hasattr(resp, "list_devices_result"):
        return resp.list_devices_result.devices
    raise AttributeError("Response has no list_devices payload")


def read_signal_entries(resp: Any):
    if hasattr(resp, "read_signals"):
        return resp.read_signals.values
    if hasattr(resp, "read_signals_result"):
        return resp.read_signals_result.values
    raise AttributeError("Response has no read_signals payload")


def require_signal(resp: Any, signal_id: str):
    for entry in read_signal_entries(resp):
        if entry.signal_id == signal_id:
            return entry
    raise AssertionError(f"Signal '{signal_id}' not found in read response")
