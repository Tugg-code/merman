"""Telemetry parsing and in-memory history helpers."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any
import json
import time


@dataclass
class Telemetry:
    timestamp: float
    yaw: float = 0.0
    yaw_rate: float = 0.0
    servo: float = 90.0
    target: float = 0.0
    error: float = 0.0
    mode: str = "UNKNOWN"
    limit: bool = False
    mpu_ok: bool = False
    kp: float = 0.0
    deadband: float = 0.0
    manual_speed: float = 0.0
    gyro_trim: float = 0.0

    @classmethod
    def from_json_line(cls, line: str) -> "Telemetry":
        """Parse one JSON telemetry line emitted by the Arduino."""
        data: dict[str, Any] = json.loads(line)
        return cls(
            timestamp=time.time(),
            yaw=float(data.get("yaw", 0.0)),
            yaw_rate=float(data.get("yaw_rate", 0.0)),
            servo=float(data.get("servo", 90.0)),
            target=float(data.get("target", 0.0)),
            error=float(data.get("error", 0.0)),
            mode=str(data.get("mode", "UNKNOWN")),
            limit=bool(data.get("limit", False)),
            mpu_ok=bool(data.get("mpu_ok", False)),
            kp=float(data.get("kp", 0.0)),
            deadband=float(data.get("deadband", 0.0)),
            manual_speed=float(data.get("manual_speed", 0.0)),
            gyro_trim=float(data.get("gyro_trim", 0.0)),
        )

    def csv_row(self) -> dict[str, object]:
        return {
            "timestamp": self.timestamp,
            "yaw": self.yaw,
            "yaw_rate": self.yaw_rate,
            "servo": self.servo,
            "target": self.target,
            "error": self.error,
            "mode": self.mode,
            "limit": self.limit,
            "mpu_ok": self.mpu_ok,
            "kp": self.kp,
            "deadband": self.deadband,
            "manual_speed": self.manual_speed,
            "gyro_trim": self.gyro_trim,
        }
