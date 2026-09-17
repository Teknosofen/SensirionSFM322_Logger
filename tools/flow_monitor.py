"""
flow_monitor.py — live plot + CSV recorder for FlowSensorTest.

Reads lines of the form '#<seq>\t<flow>\n' from the ESP32 serial port,
plots flow vs. time, and optionally records timestamped samples to a
CSV file.

Usage:
    python flow_monitor.py [--port COM7] [--baud 115200] [--window 30]

Serial port is selected inside the plot window. If --port is passed,
the tool auto-connects on startup.

Keys / buttons in the plot window:
    r / [Record] : start/stop CSV recording (creates a new file each time)
    c / [Clear]  : clear the on-screen buffer
    q           : quit
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import re
import sys
import threading
import time
import tkinter as tk
from collections import deque
from pathlib import Path
from tkinter import ttk

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports
from matplotlib.widgets import Button, TextBox

LINE_RE = re.compile(r"^#(\d+)\t(-?\d+(?:\.\d+)?)\s*$")


class SerialReader(threading.Thread):
    """Background thread pulling lines from the serial port."""

    def __init__(self, port: str, baud: int):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._write_lock = threading.Lock()
        self._buffer: list[tuple[float, int, float]] = []
        self._ser: serial.Serial | None = None
        self.error: str | None = None

    def stop(self) -> None:
        self._stop.set()

    def drain(self) -> list[tuple[float, int, float]]:
        with self._lock:
            out = self._buffer
            self._buffer = []
            return out

    def send(self, cmd: str) -> bool:
        with self._write_lock:
            ser = self._ser
            if ser is None:
                return False
            try:
                ser.write((cmd + "\n").encode("ascii"))
                ser.flush()
                return True
            except Exception as exc:
                self.error = f"write: {type(exc).__name__}: {exc}"
                return False

    def run(self) -> None:
        try:
            with serial.Serial(self.port, self.baud, timeout=1) as ser:
                ser.reset_input_buffer()
                with self._write_lock:
                    self._ser = ser
                while not self._stop.is_set():
                    line = ser.readline().decode("ascii", errors="replace").strip()
                    if not line:
                        continue
                    m = LINE_RE.match(line)
                    if not m:
                        continue
                    seq  = int(m.group(1))
                    flow = float(m.group(2))
                    ts   = time.time()
                    with self._lock:
                        self._buffer.append((ts, seq, flow))
        except Exception as exc:  # pragma: no cover - runtime feedback
            self.error = f"{type(exc).__name__}: {exc}"
        finally:
            with self._write_lock:
                self._ser = None


class Recorder:
    """CSV writer that opens a new timestamped file each session."""

    def __init__(self, out_dir: Path):
        self.out_dir = out_dir
        self._file = None
        self._writer = None
        self.path: Path | None = None

    @property
    def active(self) -> bool:
        return self._file is not None

    def start(self) -> Path:
        self.out_dir.mkdir(parents=True, exist_ok=True)
        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.path = self.out_dir / f"flow_{stamp}.csv"
        self._file = self.path.open("w", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        self._writer.writerow(["iso_time", "epoch_s", "seq", "flow_slm"])
        return self.path

    def write(self, samples: list[tuple[float, int, float]]) -> None:
        if not self._writer:
            return
        for ts, seq, flow in samples:
            iso = dt.datetime.fromtimestamp(ts).isoformat(timespec="milliseconds")
            self._writer.writerow([iso, f"{ts:.3f}", seq, f"{flow:.3f}"])
        self._file.flush()

    def stop(self) -> Path | None:
        if self._file:
            self._file.close()
        path, self.path = self.path, None
        self._file = None
        self._writer = None
        return path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Live plot + CSV recorder for FlowSensorTest")
    p.add_argument("--port", help="Serial port, e.g. COM7 or /dev/ttyUSB0. If omitted, a picker is shown.")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--window", type=float, default=30.0,
                   help="Rolling window shown on the plot, in seconds (default 30)")
    p.add_argument("--outdir", default="recordings", help="Where CSV files are written")
    return p.parse_args()


def list_ports() -> list[tuple[str, str]]:
    return [(p.device, p.description) for p in serial.tools.list_ports.comports()]


def pick_port_dialog(default_baud: int) -> tuple[str, int] | None:
    """Modal Tk dialog. Returns (port, baud) or None if the user cancels."""
    ports = list_ports()

    root = tk.Tk()
    root.title("FlowSensor - select serial port")
    root.resizable(False, False)

    choice: dict[str, object] = {"port": None, "baud": default_baud}

    frame = ttk.Frame(root, padding=12)
    frame.grid()

    ttk.Label(frame, text="Serial port:").grid(row=0, column=0, sticky="w")
    port_var = tk.StringVar()
    port_labels = [f"{dev}  —  {desc}" for dev, desc in ports] or ["(no serial ports found)"]
    port_combo = ttk.Combobox(frame, values=port_labels, textvariable=port_var,
                              width=45, state="readonly")
    if ports:
        port_combo.current(0)
    port_combo.grid(row=0, column=1, padx=(6, 0), pady=(0, 4))

    ttk.Label(frame, text="Baud:").grid(row=1, column=0, sticky="w")
    baud_var = tk.StringVar(value=str(default_baud))
    baud_combo = ttk.Combobox(frame, values=["9600", "38400", "57600", "115200", "230400", "460800", "921600"],
                              textvariable=baud_var, width=10)
    baud_combo.grid(row=1, column=1, sticky="w", padx=(6, 0))

    def do_rescan():
        nonlocal ports
        ports = list_ports()
        labels = [f"{dev}  —  {desc}" for dev, desc in ports] or ["(no serial ports found)"]
        port_combo["values"] = labels
        if ports:
            port_combo.current(0)

    def do_ok(_event=None):
        if not ports:
            return
        idx = port_combo.current()
        if idx < 0:
            idx = 0
        choice["port"] = ports[idx][0]
        try:
            choice["baud"] = int(baud_var.get())
        except ValueError:
            choice["baud"] = default_baud
        root.destroy()

    def do_cancel(_event=None):
        choice["port"] = None
        root.destroy()

    btns = ttk.Frame(frame)
    btns.grid(row=2, column=0, columnspan=2, pady=(10, 0), sticky="e")
    ttk.Button(btns, text="Rescan", command=do_rescan).grid(row=0, column=0, padx=(0, 6))
    ttk.Button(btns, text="Cancel", command=do_cancel).grid(row=0, column=1, padx=(0, 6))
    ttk.Button(btns, text="Connect", command=do_ok).grid(row=0, column=2)

    root.bind("<Return>", do_ok)
    root.bind("<Escape>", do_cancel)
    root.protocol("WM_DELETE_WINDOW", do_cancel)
    port_combo.focus_set()
    root.mainloop()

    if not choice["port"]:
        return None
    return str(choice["port"]), int(choice["baud"])


def main() -> int:
    args = parse_args()

    if args.port:
        port, baud = args.port, args.baud
    else:
        pick = pick_port_dialog(args.baud)
        if pick is None:
            print("No port selected, exiting.")
            return 0
        port, baud = pick

    print(f"Connecting to {port} @ {baud} baud")
    reader = SerialReader(port, baud)
    reader.start()

    recorder = Recorder(Path(args.outdir))

    max_points = max(200, int(args.window * 50))  # ~50 Hz worst-case
    times: deque[float] = deque(maxlen=max_points)
    flows: deque[float] = deque(maxlen=max_points)

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.canvas.manager.set_window_title(f"FlowSensor @ {port}")
    fig.suptitle("IaT Team flow sensor interface", fontsize=14, fontweight="bold")
    line, = ax.plot([], [], lw=1.5, color="tab:cyan")
    ax.set_xlabel("time [s]")
    ax.set_ylabel("flow [slm Air]")
    ax.grid(True, alpha=0.3)
    status = ax.text(0.01, 0.98, "", transform=ax.transAxes,
                     va="top", ha="left", fontsize=9, family="monospace")

    plt.subplots_adjust(bottom=0.22)
    ax_rec   = fig.add_axes([0.10, 0.06, 0.15, 0.07])
    ax_clr   = fig.add_axes([0.27, 0.06, 0.15, 0.07])
    ax_rate_tb = fig.add_axes([0.62, 0.06, 0.10, 0.07])   # custom Hz text box
    btn_rec  = Button(ax_rec, "Record")
    btn_clr  = Button(ax_clr, "Clear")

    rate_state: dict[str, int] = {"hz": 20}    # firmware default = 50 ms = 20 Hz

    def send_rate(hz: int):
        hz = max(1, min(500, int(hz)))
        ms = max(1, round(1000 / hz))
        rate_state["hz"] = hz
        ok = reader.send(f"R{ms}")
        print(f"[rate] R{ms}  ({hz} Hz)  sent={ok}")

    rate_tb = TextBox(ax_rate_tb, "Hz ", initial=str(rate_state["hz"]))

    def on_rate_submit(text: str):
        try:
            hz = int(float(text))
        except ValueError:
            rate_tb.set_val(str(rate_state["hz"]))
            return
        send_rate(hz)

    rate_tb.on_submit(on_rate_submit)

    def toggle_record(_event=None):
        if recorder.active:
            path = recorder.stop()
            btn_rec.label.set_text("Record")
            btn_rec.color = "0.85"
            print(f"[rec] stopped: {path}")
        else:
            path = recorder.start()
            btn_rec.label.set_text("Stop")
            btn_rec.color = "tab:red"
            print(f"[rec] started: {path}")
        fig.canvas.draw_idle()

    def clear_buffer(_event=None):
        times.clear()
        flows.clear()

    def on_key(event):
        if event.key == "r":
            toggle_record()
        elif event.key == "c":
            clear_buffer()
        elif event.key == "q":
            plt.close(fig)

    btn_rec.on_clicked(toggle_record)
    btn_clr.on_clicked(clear_buffer)
    fig.canvas.mpl_connect("key_press_event", on_key)

    t0: float | None = None
    last_draw = 0.0

    try:
        plt.show(block=False)
        while plt.fignum_exists(fig.number):
            samples = reader.drain()
            if reader.error:
                status.set_text(f"SERIAL ERROR: {reader.error}")
                fig.canvas.draw_idle()
                plt.pause(0.5)
                break

            if samples:
                if recorder.active:
                    recorder.write(samples)
                if t0 is None:
                    t0 = samples[0][0]
                for ts, _seq, flow in samples:
                    times.append(ts - t0)
                    flows.append(flow)

            now = time.time()
            if now - last_draw >= 0.05 and times:
                line.set_data(times, flows)
                tmax = times[-1]
                ax.set_xlim(max(0.0, tmax - args.window), max(args.window, tmax))
                fmin, fmax = min(flows), max(flows)
                pad = max(0.05, 0.1 * (fmax - fmin))
                ax.set_ylim(fmin - pad, fmax + pad)
                rec_txt = f"REC -> {recorder.path.name}" if recorder.active else "idle"
                status.set_text(f"{rec_txt}   samples={len(times)}   last={flows[-1]:.3f}")
                fig.canvas.draw_idle()
                last_draw = now

            plt.pause(0.02)
    except KeyboardInterrupt:
        pass
    finally:
        reader.stop()
        if recorder.active:
            print(f"[rec] stopped: {recorder.stop()}")
        plt.close("all")

    return 0


if __name__ == "__main__":
    sys.exit(main())
