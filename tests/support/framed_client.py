"""ADPP framed-stdio client for provider integration tests."""

from __future__ import annotations

import struct
import subprocess
from pathlib import Path
from types import ModuleType
from typing import Any

from .process import LineCapture, terminate_process


class AdppClient:
    """Simple synchronous ADPP client over stdio+uint32_le framing."""

    def __init__(
        self,
        protocol: ModuleType,
        exe_path: Path,
        config_path: Path,
        *,
        sim_server: str | None = None,
    ):
        self.protocol = protocol
        self.exe_path = exe_path
        self.config_path = config_path
        self._next_request_id = 1

        cmd = [str(exe_path), "--config", str(config_path)]
        if sim_server:
            cmd.extend(["--sim-server", sim_server])

        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
        )

        self.stderr_capture = LineCapture(self.process.stderr)
        self.stderr_capture.start()

    def is_running(self) -> bool:
        return self.process.poll() is None

    def _request_id(self) -> int:
        request_id = self._next_request_id
        self._next_request_id += 1
        return request_id

    def _read_exact(self, length: int) -> bytes:
        stream = self.process.stdout
        if stream is None:
            raise RuntimeError("Provider stdout stream unavailable")

        out = bytearray()
        while len(out) < length:
            chunk = stream.read(length - len(out))
            if not chunk:
                raise RuntimeError(
                    f"Provider stream closed while reading {length} bytes; got {len(out)} bytes\n"
                    f"{self.output_tail(100)}"
                )
            out.extend(chunk)
        return bytes(out)

    def send_request(self, request: Any) -> Any:
        if not self.is_running():
            raise RuntimeError(
                f"Provider process exited before request send (code={self.process.poll()})\n"
                f"{self.output_tail(100)}"
            )

        payload = request.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload

        stdin = self.process.stdin
        if stdin is None:
            raise RuntimeError("Provider stdin stream unavailable")

        stdin.write(frame)
        stdin.flush()

        header = self._read_exact(4)
        (resp_len,) = struct.unpack("<I", header)
        body = self._read_exact(resp_len)

        response = self.protocol.Response()
        response.ParseFromString(body)
        return response

    def hello(
        self,
        *,
        client_name: str = "provider-sim-test",
        client_version: str = "0.0.1",
        protocol_version: str = "v1",
    ) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.hello.protocol_version = protocol_version
        request.hello.client_name = client_name
        request.hello.client_version = client_version
        return self.send_request(request)

    def wait_ready(self, max_wait_ms_hint: int = 5000) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.wait_ready.max_wait_ms_hint = max_wait_ms_hint
        return self.send_request(request)

    def list_devices(self, include_health: bool = False) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.list_devices.include_health = include_health
        return self.send_request(request)

    def describe_device(self, device_id: str) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.describe_device.device_id = device_id
        return self.send_request(request)

    def read_signals(self, device_id: str, signal_ids: list[str] | None = None) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.read_signals.device_id = device_id
        if signal_ids:
            request.read_signals.signal_ids.extend(signal_ids)
        return self.send_request(request)

    def call_function(
        self, device_id: str, function_id: int, args: dict[str, Any] | None = None
    ) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.call.device_id = device_id
        request.call.function_id = function_id
        if args:
            for key, value in args.items():
                request.call.args[key].CopyFrom(value)
        return self.send_request(request)

    def get_health(self) -> Any:
        request = self.protocol.Request(request_id=self._request_id())
        request.get_health.SetInParent()
        return self.send_request(request)

    def output_tail(self, lines: int = 80) -> str:
        return self.stderr_capture.tail(lines)

    def close(self, timeout: float = 2.0) -> None:
        stdin = self.process.stdin
        if stdin is not None and not stdin.closed:
            try:
                stdin.close()
            except OSError:
                pass

        if self.process.poll() is None:
            try:
                self.process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                terminate_process(self.process, timeout=timeout)

        self.stderr_capture.stop()


def make_string_value(protocol: ModuleType, value: str) -> Any:
    out = protocol.Value()
    out.type = protocol.ValueType.VALUE_TYPE_STRING
    out.string_value = value
    return out


def make_double_value(protocol: ModuleType, value: float) -> Any:
    out = protocol.Value()
    out.type = protocol.ValueType.VALUE_TYPE_DOUBLE
    out.double_value = value
    return out


def make_int64_value(protocol: ModuleType, value: int) -> Any:
    out = protocol.Value()
    out.type = protocol.ValueType.VALUE_TYPE_INT64
    out.int64_value = value
    return out


def make_bool_value(protocol: ModuleType, value: bool) -> Any:
    out = protocol.Value()
    out.type = protocol.ValueType.VALUE_TYPE_BOOL
    out.bool_value = value
    return out
