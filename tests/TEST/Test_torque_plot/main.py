"""
Live plot for XL330 CSV stream.

Reads CSV lines from the Arduino/OpenRB USB serial output and plots:
- position / time
- estimated torque / time
- voltage / time
"""

import csv
import time
from collections import deque

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- User settings ----------------------------------------------------------

PORT = "/dev/cu.usbmodem1401"   # adapte si besoin
BAUDRATE = 115200
WINDOW_S = 20.0
MAX_POINTS = 5000

# --- Buffers ----------------------------------------------------------------

t_buf = deque(maxlen=MAX_POINTS)
pos_buf = deque(maxlen=MAX_POINTS)
torque_buf = deque(maxlen=MAX_POINTS)
volt_buf = deque(maxlen=MAX_POINTS)

# --- Serial -----------------------------------------------------------------

ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)
time.sleep(2.0)

# --- Figure -----------------------------------------------------------------

fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
ax_pos, ax_torque, ax_volt = axes

line_pos, = ax_pos.plot([], [], label="Position [deg]")
line_torque, = ax_torque.plot([], [], label="Estimated torque [N.m]")
line_volt, = ax_volt.plot([], [], label="Voltage [V]")

ax_pos.set_ylabel("Position [deg]")
ax_torque.set_ylabel("Torque est. [N.m]")
ax_volt.set_ylabel("Voltage [V]")
ax_volt.set_xlabel("Time [s]")

ax_pos.grid(True)
ax_torque.grid(True)
ax_volt.grid(True)

ax_pos.legend(loc="upper left")
ax_torque.legend(loc="upper left")
ax_volt.legend(loc="upper left")

# --- Parsing ----------------------------------------------------------------

def parse_line(line: str):
    if not line:
        return None

    line = line.strip()

    if not line:
        return None

    if line.startswith("#"):
        return None

    if line.startswith("t_s"):
        return None

    try:
        parts = next(csv.reader([line]))
        if len(parts) != 12:
            return None

        return {
            "t_s": float(parts[0]),
            "position_deg": float(parts[2]),
            "torque_est_Nm": float(parts[7]),
            "voltage_V": float(parts[9]),
        }
    except (ValueError, IndexError):
        return None

# --- Update function --------------------------------------------------------

def update(_frame):
    while ser.in_waiting:
        raw = ser.readline().decode("utf-8", errors="ignore").strip()
        row = parse_line(raw)
        if row is None:
            continue

        t_buf.append(row["t_s"])
        pos_buf.append(row["position_deg"])
        torque_buf.append(row["torque_est_Nm"])
        volt_buf.append(row["voltage_V"])

    if not t_buf:
        return line_pos, line_torque, line_volt

    t_min = max(t_buf[-1] - WINDOW_S, 0.0)
    xs = list(t_buf)

    line_pos.set_data(xs, list(pos_buf))
    line_torque.set_data(xs, list(torque_buf))
    line_volt.set_data(xs, list(volt_buf))

    for ax, ys in (
        (ax_pos, pos_buf),
        (ax_torque, torque_buf),
        (ax_volt, volt_buf),
    ):
        ax.set_xlim(t_min, t_buf[-1] + 0.1)

        y_list = list(ys)
        y_min = min(y_list)
        y_max = max(y_list)

        if abs(y_max - y_min) < 1e-9:
            pad = 1.0
        else:
            pad = 0.1 * (y_max - y_min)

        ax.set_ylim(y_min - pad, y_max + pad)

    return line_pos, line_torque, line_volt

# --- Animation --------------------------------------------------------------

ani = FuncAnimation(
    fig,
    update,
    interval=50,
    blit=False,
    cache_frame_data=False,
)

plt.tight_layout()
plt.show()

ser.close()