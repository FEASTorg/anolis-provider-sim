"""Process lifecycle and output capture helpers for tests."""

from __future__ import annotations

import subprocess
import threading
import time
from typing import Any, Sequence


class LineCapture:
    """Capture text stream lines in a background thread."""

    def __init__(self, stream: Any | None):
        self._stream = stream
        self._lines: list[str] = []
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        if self._stream is None:
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self) -> None:
        assert self._stream is not None
        try:
            while not self._stop_event.is_set():
                line = self._stream.readline()
                if isinstance(line, bytes):
                    if line == b"":
                        break
                    cleaned = line.decode("utf-8", errors="replace").rstrip("\r\n")
                else:
                    if line == "":
                        break
                    cleaned = line.rstrip("\r\n")

                with self._lock:
                    self._lines.append(cleaned)
        except Exception as exc:  # best-effort diagnostics only
            with self._lock:
                self._lines.append(f"[capture-error] {exc}")

    def tail(self, lines: int = 80) -> str:
        with self._lock:
            slice_lines = self._lines[-lines:] if lines > 0 else self._lines
            return "\n".join(slice_lines)

    def stop(self, timeout: float = 1.0) -> None:
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=timeout)


def terminate_process(proc: subprocess.Popen, timeout: float = 5.0) -> None:
    """Terminate process with graceful-then-force fallback."""
    if proc.poll() is not None:
        return

    proc.terminate()
    try:
        proc.wait(timeout=timeout)
        return
    except subprocess.TimeoutExpired:
        pass

    proc.kill()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        pass


class ManagedTextProcess:
    """Text-process wrapper with stdout/stderr tail capture."""

    def __init__(self, name: str, process: subprocess.Popen):
        self.name = name
        self.process = process
        self.stdout = LineCapture(process.stdout)
        self.stderr = LineCapture(process.stderr)
        self.stdout.start()
        self.stderr.start()

    @classmethod
    def start(
        cls,
        name: str,
        cmd: Sequence[str],
        *,
        cwd: str | None = None,
        env: dict[str, str] | None = None,
    ) -> "ManagedTextProcess":
        process = subprocess.Popen(
            list(cmd),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            cwd=cwd,
            env=env,
        )
        return cls(name, process)

    def is_running(self) -> bool:
        return self.process.poll() is None

    def poll(self) -> int | None:
        return self.process.poll()

    def assert_running(self, context: str) -> None:
        if self.is_running():
            return
        raise RuntimeError(
            f"{self.name} exited early during {context} with code {self.poll()}\n"
            f"{self.output_tail(80)}"
        )

    def output_tail(self, lines: int = 80) -> str:
        stdout_tail = self.stdout.tail(lines)
        stderr_tail = self.stderr.tail(lines)
        return (
            f"[{self.name}] stdout tail:\n{stdout_tail or '(empty)'}\n"
            f"[{self.name}] stderr tail:\n{stderr_tail or '(empty)'}"
        )

    def wait_for_port(
        self,
        host: str,
        port: int,
        timeout: float = 5.0,
        interval: float = 0.1,
    ) -> bool:
        import socket

        deadline = time.time() + timeout
        while time.time() < deadline:
            self.assert_running(f"wait_for_port {host}:{port}")
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(0.2)
                try:
                    sock.connect((host, port))
                    return True
                except OSError:
                    time.sleep(interval)
        return False

    def close(self, timeout: float = 5.0) -> None:
        terminate_process(self.process, timeout=timeout)
        self.stdout.stop()
        self.stderr.stop()
