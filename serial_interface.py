"""Threaded serial connection for newline-delimited Arduino telemetry."""

from __future__ import annotations

import queue
import threading
from typing import Optional

import serial
from serial.tools import list_ports

from config import SERIAL_TIMEOUT_SECONDS, TELEMETRY_QUEUE_SIZE
from telemetry import Telemetry


def available_ports() -> list[str]:
    """Return COM port device names suitable for the connection selector."""
    return [port.device for port in list_ports.comports()]


class SerialInterface:
    def __init__(self) -> None:
        self._serial: Optional[serial.Serial] = None
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self.telemetry_queue: queue.Queue[Telemetry] = queue.Queue(TELEMETRY_QUEUE_SIZE)
        self.error_queue: queue.Queue[str] = queue.Queue()
        self.status_queue: queue.Queue[str] = queue.Queue()
        self._malformed_count = 0

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    def connect(self, port: str, baudrate: int) -> None:
        self.disconnect()
        self._malformed_count = 0
        self._serial = serial.Serial(port, baudrate, timeout=SERIAL_TIMEOUT_SECONDS)
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def disconnect(self) -> None:
        self._stop_event.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=0.5)
        self._thread = None
        if self._serial:
            try:
                self._serial.close()
            except serial.SerialException:
                pass
        self._serial = None

    def send_command(self, command: str) -> None:
        if not self.connected or self._serial is None:
            raise RuntimeError("Not connected to an Arduino.")
        try:
            self._serial.write((command.strip() + "\n").encode("ascii"))
        except (serial.SerialException, OSError) as exc:
            raise RuntimeError(f"Could not send command: {exc}") from exc

    def _read_loop(self) -> None:
        while not self._stop_event.is_set() and self.connected:
            try:
                assert self._serial is not None
                raw = self._serial.readline().decode("utf-8", errors="replace").strip()
                if not raw:
                    continue
                telemetry = Telemetry.from_json_line(raw)
                try:
                    self.telemetry_queue.put_nowait(telemetry)
                except queue.Full:
                    try:
                        self.telemetry_queue.get_nowait()
                    except queue.Empty:
                        pass
                    self.telemetry_queue.put_nowait(telemetry)
            except (serial.SerialException, OSError) as exc:
                self.error_queue.put(f"Serial connection lost: {exc}")
                break
            except ValueError:
                # Startup banners from smoke/sweep sketches are useful clues
                # when the GUI connects but no telemetry appears.
                self._malformed_count += 1
                if self._malformed_count <= 3:
                    preview = raw[:100]
                    self.status_queue.put(
                        "Received non-telemetry serial text. "
                        f"Is the full stabilizer sketch uploaded? Text: {preview}"
                    )
                continue
