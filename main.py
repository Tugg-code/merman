"""Tkinter test console for the fish finder stabilizer proof of concept."""

from __future__ import annotations

import csv
from datetime import datetime
import logging
from pathlib import Path
import queue
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from config import APP_TITLE, DEFAULT_BAUD
from plot_panel import PlotPanel
from serial_interface import SerialInterface, available_ports
from telemetry import Telemetry

LOG_PATH = Path.home() / "Desktop" / "Merman Stabilizer Test Build" / "gui_error.log"
LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
logging.basicConfig(
    filename=LOG_PATH,
    level=logging.ERROR,
    format="%(asctime)s %(levelname)s %(message)s",
)


class StabilizerApp(tk.Tk):
    """One application with a simple tester view and a developer view."""

    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.minsize(1100, 700)
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.report_callback_exception = self._handle_callback_exception

        self.serial = SerialInterface()
        self.latest: Telemetry | None = None
        self.connected_at: float | None = None
        self.no_telemetry_warning_shown = False
        self.logging = False
        self.log_file = None
        self.log_writer: csv.DictWriter | None = None

        self.view_mode = tk.StringVar(value="simple")
        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self.status_var = tk.StringVar(value="Disconnected")
        self.log_var = tk.StringVar(value="Logging off")
        self.simple_summary_var = tk.StringVar(value="Connect to controller to begin.")
        self.connect_buttons: list[ttk.Button] = []

        self.values = {
            key: tk.StringVar(value="--")
            for key in (
                "yaw",
                "yaw_rate",
                "mpu_ok",
                "mpu_addr",
                "servo",
                "target",
                "error",
                "mode",
                "limit",
                "active_tuning",
            )
        }
        self.tuning = {
            "kp": tk.DoubleVar(value=0.7),
            "deadband": tk.DoubleVar(value=0.5),
            "min_angle": tk.IntVar(value=20),
            "max_angle": tk.IntVar(value=160),
            "manual_speed": tk.DoubleVar(value=25.0),
            "max_error": tk.DoubleVar(value=45.0),
            "gyro_trim": tk.DoubleVar(value=0.0),
        }

        self._build_ui()
        self.refresh_ports()
        self.show_view()
        self.after(50, self.poll_serial)

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        top = ttk.Frame(self, padding=(10, 10, 10, 0))
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(1, weight=1)

        ttk.Label(top, text="View").grid(row=0, column=0, sticky="w")
        mode = ttk.Frame(top)
        mode.grid(row=0, column=1, sticky="w", padx=(8, 0))
        ttk.Radiobutton(mode, text="Simple tester", variable=self.view_mode, value="simple",
                        command=self.show_view).pack(side=tk.LEFT)
        ttk.Radiobutton(mode, text="Dev / tuning", variable=self.view_mode, value="dev",
                        command=self.show_view).pack(side=tk.LEFT, padx=(12, 0))

        self.view_container = ttk.Frame(self, padding=10)
        self.view_container.grid(row=1, column=0, sticky="nsew")
        self.view_container.columnconfigure(0, weight=1)
        self.view_container.rowconfigure(0, weight=1)

        self.simple_view = ttk.Frame(self.view_container)
        self.dev_view = ttk.Frame(self.view_container)
        self.simple_view.grid(row=0, column=0, sticky="nsew")
        self.dev_view.grid(row=0, column=0, sticky="nsew")

        self._build_simple_view()
        self._build_dev_view()

    def _build_connection_panel(self, parent: ttk.Frame | ttk.LabelFrame) -> ttk.LabelFrame:
        conn = ttk.LabelFrame(parent, text="Serial connection", padding=8)
        ttk.Label(conn, text="COM port").grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=13, state="readonly")
        self.port_combo.grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(conn, text="Refresh", command=self.refresh_ports).grid(row=0, column=2)
        ttk.Label(conn, text="Baud rate").grid(row=1, column=0, sticky="w", pady=(5, 0))
        ttk.Entry(conn, textvariable=self.baud_var, width=15).grid(row=1, column=1, sticky="w", pady=(5, 0))
        connect_button = ttk.Button(conn, text="Connect", command=self.toggle_connection)
        connect_button.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(7, 0))
        self.connect_buttons.append(connect_button)
        ttk.Label(conn, textvariable=self.status_var, wraplength=290).grid(
            row=3, column=0, columnspan=3, sticky="w", pady=(5, 0)
        )
        conn.columnconfigure(1, weight=1)
        return conn

    def _build_simple_view(self) -> None:
        self.simple_view.columnconfigure(0, weight=1)
        self.simple_view.rowconfigure(1, weight=1)

        header = ttk.Frame(self.simple_view)
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(1, weight=1)
        self._build_connection_panel(header).grid(row=0, column=0, sticky="nw")

        state = ttk.LabelFrame(header, text="Status", padding=12)
        state.grid(row=0, column=1, sticky="nsew", padx=(10, 0))
        state.columnconfigure(0, weight=1)
        ttk.Label(state, textvariable=self.simple_summary_var, font=("Segoe UI", 16),
                  wraplength=650).grid(row=0, column=0, sticky="w")
        ttk.Label(state, textvariable=self.values["mode"], font=("Segoe UI", 28, "bold")).grid(
            row=1, column=0, sticky="w", pady=(12, 0)
        )

        controls = ttk.LabelFrame(self.simple_view, text="Tester controls", padding=18)
        controls.grid(row=1, column=0, sticky="nsew", pady=(10, 0))
        controls.columnconfigure((0, 1, 2), weight=1)
        controls.rowconfigure((0, 1, 2), weight=1)

        left = ttk.Button(controls, text="< LEFT")
        left.grid(row=0, column=0, sticky="nsew", padx=8, pady=8)
        left.bind("<ButtonPress-1>", lambda _event: self.send("JOG LEFT"))
        left.bind("<ButtonRelease-1>", lambda _event: self.send("JOG STOP"))
        left.bind("<Leave>", lambda _event: self.send("JOG STOP"))

        right = ttk.Button(controls, text="RIGHT >")
        right.grid(row=0, column=2, sticky="nsew", padx=8, pady=8)
        right.bind("<ButtonPress-1>", lambda _event: self.send("JOG RIGHT"))
        right.bind("<ButtonRelease-1>", lambda _event: self.send("JOG STOP"))
        right.bind("<Leave>", lambda _event: self.send("JOG STOP"))

        ttk.Button(controls, text="LOCK TARGET", command=lambda: self.send("FIX")).grid(
            row=1, column=0, sticky="nsew", padx=8, pady=8
        )
        ttk.Button(controls, text="UNLOCK", command=lambda: self.send("DISENGAGE")).grid(
            row=1, column=1, sticky="nsew", padx=8, pady=8
        )
        ttk.Button(controls, text="RESET SERVO", command=lambda: self.send("CENTER")).grid(
            row=1, column=2, sticky="nsew", padx=8, pady=8
        )
        ttk.Button(controls, text="ZERO GYRO", command=lambda: self.send("ZERO")).grid(
            row=2, column=0, sticky="nsew", padx=8, pady=8
        )
        ttk.Button(controls, text="MANUAL / READY", command=lambda: self.send("MANUAL")).grid(
            row=2, column=1, sticky="nsew", padx=8, pady=8
        )
        ttk.Button(controls, text="RECENTER", command=lambda: self.send("RECENTER")).grid(
            row=2, column=2, sticky="nsew", padx=8, pady=8
        )

        note = ttk.Label(
            self.simple_view,
            text="Hold LEFT or RIGHT to move. Release to stop. LOCK TARGET stores the current direction.",
            anchor="center",
        )
        note.grid(row=2, column=0, sticky="ew", pady=(8, 0))

    def _build_dev_view(self) -> None:
        self.dev_view.columnconfigure(0, weight=0)
        self.dev_view.columnconfigure(1, weight=1)
        self.dev_view.rowconfigure(0, weight=1)

        sidebar = ttk.Frame(self.dev_view)
        sidebar.grid(row=0, column=0, sticky="nsw")
        plot_frame = ttk.Frame(self.dev_view, padding=(10, 0, 0, 0))
        plot_frame.grid(row=0, column=1, sticky="nsew")
        plot_frame.rowconfigure(0, weight=1)
        plot_frame.columnconfigure(0, weight=1)

        self._build_connection_panel(sidebar).pack(fill=tk.X, pady=(0, 8))

        telemetry = ttk.LabelFrame(sidebar, text="Live telemetry", padding=8)
        telemetry.pack(fill=tk.X, pady=(0, 8))
        labels = [
            ("Yaw angle", "yaw"),
            ("Yaw rate", "yaw_rate"),
            ("MPU6050", "mpu_ok"),
            ("MPU address", "mpu_addr"),
            ("Servo command", "servo"),
            ("Target / fixed", "target"),
            ("Error angle", "error"),
            ("Mode", "mode"),
            ("Limit status", "limit"),
            ("Controller Kp / deadband / speed", "active_tuning"),
        ]
        for row, (label, key) in enumerate(labels):
            ttk.Label(telemetry, text=label + ":").grid(row=row, column=0, sticky="w")
            ttk.Label(telemetry, textvariable=self.values[key], width=16).grid(row=row, column=1, sticky="e")

        controls = ttk.LabelFrame(sidebar, text="Controls", padding=8)
        controls.pack(fill=tk.X, pady=(0, 8))
        buttons = [
            ("FIX TARGET", "FIX"),
            ("MANUAL MODE", "MANUAL"),
            ("DISENGAGE", "DISENGAGE"),
            ("RECENTER", "RECENTER"),
            ("ZERO GYRO", "ZERO"),
            ("CENTER SERVO", "CENTER"),
            ("NUDGE LEFT", "NUDGE LEFT"),
            ("NUDGE RIGHT", "NUDGE RIGHT"),
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

    def show_view(self) -> None:
        if self.view_mode.get() == "dev":
            self.dev_view.tkraise()
        else:
            self.simple_view.tkraise()

    def _add_tuning(
        self,
        parent: ttk.LabelFrame,
        label: str,
        key: str,
        low: float,
        high: float,
        resolution: float,
        command: str,
    ) -> None:
        row = parent.grid_size()[1]
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w")
        text = ttk.Label(parent, width=7)
        text.grid(row=row, column=2, sticky="e")
        scale = tk.Scale(
            parent,
            variable=self.tuning[key],
            from_=low,
            to=high,
            resolution=resolution,
            orient=tk.HORIZONTAL,
            showvalue=False,
            length=135,
            command=lambda value, target=text: target.config(text=value),
        )
        scale.grid(row=row, column=1, sticky="ew")
        text.config(text=str(self.tuning[key].get()))
        scale.bind("<ButtonRelease-1>", lambda _event, cmd=command: self.apply_tuning(cmd))
        parent.columnconfigure(1, weight=1)

    def refresh_ports(self) -> None:
        ports = available_ports()
        for combo in self._port_combos():
            combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def _port_combos(self) -> list[ttk.Combobox]:
        return [child for child in self._walk_widgets(self) if isinstance(child, ttk.Combobox)]

    def _walk_widgets(self, widget: tk.Misc) -> list[tk.Misc]:
        children: list[tk.Misc] = []
        for child in widget.winfo_children():
            children.append(child)
            children.extend(self._walk_widgets(child))
        return children

    def toggle_connection(self) -> None:
        if self.serial.connected:
            self.serial.disconnect()
            self.status_var.set("Disconnected")
            self.latest = None
            self.connected_at = None
            self.no_telemetry_warning_shown = False
            self._set_connect_buttons("Connect")
            return
        try:
            baud = int(self.baud_var.get())
            self.latest = None
            self.serial.connect(self.port_var.get(), baud)
            self.status_var.set(f"Connected: {self.port_var.get()} at {baud}")
            self.connected_at = time.monotonic()
            self.no_telemetry_warning_shown = False
            self._set_connect_buttons("Disconnect")
        except Exception as exc:
            self.status_var.set(f"Connection failed: {exc}")

    def _set_connect_buttons(self, text: str) -> None:
        for button in self.connect_buttons:
            button.config(text=text)

    def send(self, command: str) -> None:
        try:
            self.serial.send_command(command)
        except Exception as exc:
            self.status_var.set(str(exc))
            logging.exception("Command failed: %s", command)

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
            self.connected_at = None
            self._set_connect_buttons("Connect")
        while True:
            try:
                status = self.serial.status_queue.get_nowait()
            except queue.Empty:
                break
            self.status_var.set(status)
        if (
            self.serial.connected
            and self.latest is None
            and self.connected_at is not None
            and not self.no_telemetry_warning_shown
            and time.monotonic() - self.connected_at > 3.0
        ):
            self.no_telemetry_warning_shown = True
            self.status_var.set(
                "Connected, but no JSON telemetry yet. Upload the full ESP32 stabilizer sketch, "
                "close Arduino Serial Monitor, then press RESET on the ESP32."
            )
        if changed:
            self.plot.redraw()
        self.after(50, self.poll_serial)

    def update_telemetry(self, sample: Telemetry) -> None:
        self.latest = sample
        formatted = {
            "yaw": f"{sample.yaw:.2f} deg",
            "yaw_rate": f"{sample.yaw_rate:.2f} deg/s",
            "mpu_ok": "OK" if sample.mpu_ok else "NOT FOUND",
            "mpu_addr": f"0x{sample.mpu_addr:02X}" if sample.mpu_addr else "--",
            "servo": f"{sample.servo:.1f} deg",
            "target": f"{sample.target:.2f} deg",
            "error": f"{sample.error:.2f} deg",
            "mode": sample.mode,
            "limit": "AT LIMIT" if sample.limit else "OK",
            "active_tuning": f"{sample.kp:.2f} / {sample.deadband:.2f} / {sample.manual_speed:.1f}",
        }
        for key, value in formatted.items():
            self.values[key].set(value)

        self.simple_summary_var.set(
            f"Servo {sample.servo:.1f} deg   |   Yaw {sample.yaw:.1f} deg   |   "
            f"MPU: {'OK' if sample.mpu_ok else 'NOT FOUND'}"
            f"{f' 0x{sample.mpu_addr:02X}' if sample.mpu_addr else ''}   |   "
            f"Limit: {'YES' if sample.limit else 'OK'}"
        )

        self.plot.append(sample)
        if self.logging and self.log_writer and self.log_file:
            self.log_writer.writerow(sample.csv_row())
            self.log_file.flush()

    def toggle_logging(self) -> None:
        if self.logging:
            self.stop_logging()
            return
        default_name = f"fishfinder_telemetry_{datetime.now():%Y%m%d_%H%M%S}.csv"
        file_name = filedialog.asksaveasfilename(
            defaultextension=".csv",
            initialfile=default_name,
            filetypes=[("CSV files", "*.csv")],
        )
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
        self.send_stop_if_connected()
        self.stop_logging()
        self.serial.disconnect()
        self.destroy()

    def send_stop_if_connected(self) -> None:
        if self.serial.connected:
            try:
                self.serial.send_command("JOG STOP")
            except Exception:
                pass

    def _handle_callback_exception(self, exc_type: type[BaseException], exc: BaseException, tb: object) -> None:
        logging.exception("Unhandled GUI callback exception", exc_info=(exc_type, exc, tb))
        self.status_var.set(f"GUI error logged to {LOG_PATH}")


if __name__ == "__main__":
    StabilizerApp().mainloop()
