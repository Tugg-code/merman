"""Matplotlib plot widget embedded in Tkinter."""

from __future__ import annotations

from collections import deque
import tkinter as tk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

from config import PLOT_HISTORY_SECONDS, PLOT_MAX_POINTS
from telemetry import Telemetry


class PlotPanel(tk.Frame):
    def __init__(self, master: tk.Misc) -> None:
        super().__init__(master)
        self.times: deque[float] = deque(maxlen=PLOT_MAX_POINTS)
        self.yaws: deque[float] = deque(maxlen=PLOT_MAX_POINTS)
        self.servos: deque[float] = deque(maxlen=PLOT_MAX_POINTS)
        self.errors: deque[float] = deque(maxlen=PLOT_MAX_POINTS)
        self.start_time: float | None = None

        figure = Figure(figsize=(7.0, 5.5), dpi=100, tight_layout=True)
        self.axes = figure.subplots(3, 1, sharex=True)
        self.axes[0].set_ylabel("Yaw (deg)")
        self.axes[1].set_ylabel("Servo (deg)")
        self.axes[2].set_ylabel("Error (deg)")
        self.axes[2].set_xlabel("Time (s)")
        for axis in self.axes:
            axis.grid(True, alpha=0.3)
        self.yaw_line, = self.axes[0].plot([], [], color="#1f77b4")
        self.servo_line, = self.axes[1].plot([], [], color="#2ca02c")
        self.error_line, = self.axes[2].plot([], [], color="#d62728")
        self.canvas = FigureCanvasTkAgg(figure, master=self)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def append(self, sample: Telemetry) -> None:
        if self.start_time is None:
            self.start_time = sample.timestamp
        self.times.append(sample.timestamp - self.start_time)
        self.yaws.append(sample.yaw)
        self.servos.append(sample.servo)
        self.errors.append(sample.error)

    def redraw(self) -> None:
        if not self.times:
            return
        x = list(self.times)
        self.yaw_line.set_data(x, self.yaws)
        self.servo_line.set_data(x, self.servos)
        self.error_line.set_data(x, self.errors)
        left = max(0.0, x[-1] - PLOT_HISTORY_SECONDS)
        right = max(PLOT_HISTORY_SECONDS, x[-1])
        for axis in self.axes:
            axis.set_xlim(left, right)
            axis.relim()
            axis.autoscale_view(scalex=False, scaley=True)
        self.canvas.draw_idle()

