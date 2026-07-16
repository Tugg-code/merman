"""Windows Tkinter test console for the Arduino fish finder stabilizer POC."""

from __future__ import annotations

import csv
from datetime import datetime
from pathlib import Path
import queue
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from config import APP_TITLE, DEFAULT_BAUD
from plot_panel import PlotPanel
from serial_interface import SerialInterface, available_ports
from telemetry import Telemetry


class StabilizerApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.minsize(1100, 700)
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.serial = SerialInterface()
        self.latest: Telemetry | None = None
        self.logging = False
        self.log_file = None
        self.log_writer: csv.DictWriter | None = None

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self.status_var = tk.StringVar(value="Disconnected")
        self.log_var = tk.StringVar(value="Logging off")
        self.values = {key: tk.StringVar(value="--") for key in (
            "yaw", "yaw_rate", "joy_x", "joy_y", "servo", "target", "error", "mode", "limit", "active_tuning"
        )}
        self.tuning = {
            "kp": tk.DoubleVar(value=1.2),
            "deadband": tk.DoubleVar(value=2.0),
            "min_angle": tk.IntVar(value=20),
            "max_angle": tk.IntVar(value=160),
            "manual_speed": tk.DoubleVar(value=2.0),
            "max_error": tk.DoubleVar(value=45.0),
            "gyro_trim": tk.DoubleVar(value=0.0),
        }
        self._build_ui()
        self.refresh_ports()
        self.after(50, self.poll_serial)

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=0)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(0, weight=1)
        sidebar = ttk.Frame(self, padding=10)
        sidebar.grid(row=0, column=0, sticky="nsw")
        plot_frame = ttk.Frame(self, padding=(0, 10, 10, 10))
        plot_frame.grid(row=0, column=1, sticky="nsew")
        plot_frame.rowconfigure(0, weight=1)
        plot_frame.columnconfigure(0, weight=1)

        conn = ttk.LabelFrame(sidebar, text="Serial connection", padding=8)
        conn.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(conn, text="COM port").grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=13, state="readonly")
        self.port_combo.grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(conn, text="Refresh", command=self.refresh_ports).grid(row=0, column=2)
        ttk.Label(conn, text="Baud rate").grid(row=1, column=0, sticky="w", pady=(5, 0))
        ttk.Entry(conn, textvariable=self.baud_var, width=15).grid(row=1, column=1, sticky="w", pady=(5, 0))
        self.connect_button = ttk.Button(conn, text="Connect", command=self.toggle_connection)
        self.connect_button.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(7, 0))
        ttk.Label(conn, textvariable=self.status_var, wraplength=235).grid(row=3, column=0, columnspan=3, sticky="w", pady=(5, 0))
        conn.columnconfigure(1, weight=1)

        telemetry = ttk.LabelFrame(sidebar, text="Live telemetry", padding=8)
        telemetry.pack(fill=tk.X, pady=(0, 8))
        labels = [
            ("Yaw angle", "yaw"), ("Yaw rate", "yaw_rate"), ("Joystick X", "joy_x"),
            ("Joystick Y", "joy_y"), ("Servo command", "servo"), ("Target / fixed", "target"),
            ("Error angle", "error"), ("Mode", "mode"), ("Limit status", "limit"),
            ("Controller Kp / deadband / speed", "active_tuning"),
        ]
        for row, (label, key) in enumerate(labels):
            ttk.Label(telemetry, text=label + ":").grid(row=row, column=0, sticky="w")
            ttk.Label(telemetry, textvariable=self.values[key], width=14).grid(row=row, column=1, sticky="e")

        controls = ttk.LabelFrame(sidebar, text="Controls", padding=8)
        controls.pack(fill=tk.X, pady=(0, 8))
        buttons = [
            ("FIX TARGET", "FIX"), ("MANUAL MODE", "MANUAL"), ("DISENGAGE", "DISENGAGE"),
            ("RECENTER", "RECENTER"), ("ZERO GYRO", "ZERO"), ("CENTER SERVO", "CENTER"),
        ]
        for index, (label, command) in enumerate(buttons):
            ttk.Button(controls, text=label, command=lambda c=command: self.send(c)).grid(
                row=index // 2, column=index % 2, sticky="ew", padx=2, pady=2
            )
        controls.columnconfigure((0, 1), weight=1)

        tuning = ttk.LabelFrame(sidebar, text="Tuning (applies on release)", padding=8)
        tuning.pack(fill=tk.X, pady=(0, 8))
        self._add_tuning(tuning, "Servo minimum", "min_angle", 0, 90, 1, "SET LIMITS")
        self._add_tuning(tuning, "Servo maximum", "max_angle", 90, 180, 1, "SET LIMITS")
        self._add_tuning(tuning, "Manual speed", "manual_speed", 0.1, 60, 0.5, "SET MANUAL_SPEED")
        self._add_tuning(tuning, "Proportional Kp", "kp", 0, 4, 0.1, "SET KP")
        self._add_tuning(tuning, "Deadband (deg)", "deadband", 0, 15, 0.5, "SET DEADBAND")
        self._add_tuning(tuning, "Max error (deg)", "max_error", 5, 180, 1, "SET MAX_ERROR")
        self._add_tuning(tuning, "Gyro trim (dps)", "gyro_trim", -5, 5, 0.05, "SET GYRO_TRIM")

        logging = ttk.LabelFrame(sidebar, text="Data logging", padding=8)
        logging.pack(fill=tk.X)
        ttk.Button(logging, text="Start / Stop Logging", command=self.toggle_logging).pack(fill=tk.X)
        ttk.Label(logging, textvariable=self.log_var).pack(anchor="w", pady=(4, 0))

        self.plot = PlotPanel(plot_frame)
        self.plot.grid(row=0, column=0, sticky="nsew")

    def _add_tuning(self, parent: ttk.LabelFrame, label: str, key: str, low: float,
                    high: float, resolution: float, command: str) -> None:
        row = parent.grid_size()[1]
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w")
        text = ttk.Label(parent, width=7)
        text.grid(row=row, column=2, sticky="e")
        scale = tk.Scale(parent, variable=self.tuning[key], from_=low, to=high,
                         resolution=resolution, orient=tk.HORIZONTAL, showvalue=False,
                         length=135, command=lambda value, target=text: target.config(text=value))
        scale.grid(row=row, column=1, sticky="ew")
        text.config(text=str(self.tuning[key].get()))
        scale.bind("<ButtonRelease-1>", lambda _event, cmd=command: self.apply_tuning(cmd))
        parent.columnconfigure(1, weight=1)

    def refresh_ports(self) -> None:
        ports = available_ports()
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def toggle_connection(self) -> None:
        if self.serial.connected:
            self.serial.disconnect()
            self.status_var.set("Disconnected")
            self.connect_button.config(text="Connect")
            return
        try:
            baud = int(self.baud_var.get())
            self.serial.connect(self.port_var.get(), baud)
            self.status_var.set(f"Connected: {self.port_var.get()} at {baud}")
            self.connect_button.config(text="Disconnect")
        except Exception as exc:
            self.status_var.set(f"Connection failed: {exc}")

    def send(self, command: str) -> None:
        try:
            self.serial.send_command(command)
        except RuntimeError as exc:
            messagebox.showwarning(APP_TITLE, str(exc))

    def apply_tuning(self, command: str) -> None:
        if command == "SET LIMITS":
            minimum, maximum = self.tuning["min_angle"].get(), self.tuning["max_angle"].get()
            if minimum >= maximum:
                self.status_var.set("Servo minimum must be below maximum.")
                return
            self.send(f"SET LIMITS {minimum} {maximum}")
        elif command == "SET MANUAL_SPEED":
            self.send(f"SET MANUAL_SPEED {self.tuning['manual_speed'].get():.2f}")
        elif command == "SET KP":
            self.send(f"SET KP {self.tuning['kp'].get():.2f}")
        elif command == "SET DEADBAND":
            self.send(f"SET DEADBAND {self.tuning['deadband'].get():.2f}")
        elif command == "SET MAX_ERROR":
            self.send(f"SET MAX_ERROR {self.tuning['max_error'].get():.1f}")
        elif command == "SET GYRO_TRIM":
            self.send(f"SET GYRO_TRIM {self.tuning['gyro_trim'].get():.2f}")

    def poll_serial(self) -> None:
        changed = False
        while True:
            try:
                sample = self.serial.telemetry_queue.get_nowait()
            except queue.Empty:
                break
            self.update_telemetry(sample)
            changed = True
        while True:
            try:
                error = self.serial.error_queue.get_nowait()
            except queue.Empty:
                break
            self.status_var.set(error)
            self.serial.disconnect()
            self.connect_button.config(text="Connect")
        if changed:
            self.plot.redraw()
        self.after(50, self.poll_serial)

    def update_telemetry(self, sample: Telemetry) -> None:
        self.latest = sample
        formatted = {
            "yaw": f"{sample.yaw:.2f} deg", "yaw_rate": f"{sample.yaw_rate:.2f} deg/s",
            "joy_x": str(sample.joy_x), "joy_y": str(sample.joy_y), "servo": f"{sample.servo:.1f} deg",
            "target": f"{sample.target:.2f} deg", "error": f"{sample.error:.2f} deg",
            "mode": sample.mode, "limit": "AT LIMIT" if sample.limit else "OK",
            "active_tuning": f"{sample.kp:.2f} / {sample.deadband:.2f} / {sample.manual_speed:.1f}",
        }
        for key, value in formatted.items():
            self.values[key].set(value)
        self.plot.append(sample)
        if self.logging and self.log_writer and self.log_file:
            self.log_writer.writerow(sample.csv_row())
            self.log_file.flush()

    def toggle_logging(self) -> None:
        if self.logging:
            self.stop_logging()
            return
        default_name = f"fishfinder_telemetry_{datetime.now():%Y%m%d_%H%M%S}.csv"
        file_name = filedialog.asksaveasfilename(defaultextension=".csv", initialfile=default_name,
                                                 filetypes=[("CSV files", "*.csv")])
        if not file_name:
            return
        self.log_file = open(Path(file_name), "w", newline="", encoding="utf-8")
        fields = list(Telemetry(0).csv_row().keys())
        self.log_writer = csv.DictWriter(self.log_file, fieldnames=fields)
        self.log_writer.writeheader()
        self.logging = True
        self.log_var.set(f"Logging: {Path(file_name).name}")

    def stop_logging(self) -> None:
        self.logging = False
        if self.log_file:
            self.log_file.close()
        self.log_file = None
        self.log_writer = None
        self.log_var.set("Logging off")

    def on_close(self) -> None:
        self.stop_logging()
        self.serial.disconnect()
        self.destroy()


if __name__ == "__main__":
    StabilizerApp().mainloop()
