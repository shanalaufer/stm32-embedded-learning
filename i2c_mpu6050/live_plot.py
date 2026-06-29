#!/usr/bin/env python3
"""Live plot of STM32 MPU6050 IMU data streamed over USB serial.

Expected line format (six space-separated name=int fields):
    ax=1768 ay=16356 az=780 gx=-319 gy=332 gz=-186

Usage:
    python live_plot.py                      # use defaults below
    python live_plot.py /dev/tty.usbmodem103 115200
"""

import sys
from collections import deque

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- Configuration -----------------------------------------------------------
PORT = "/dev/tty.usbmodem103"
BAUD = 115200
WINDOW = 200          # number of samples kept on screen
FIELDS = ("ax", "ay", "az", "gx", "gy", "gz")

# Allow overriding port/baud from the command line.
if len(sys.argv) >= 2:
    PORT = sys.argv[1]
if len(sys.argv) >= 3:
    BAUD = int(sys.argv[2])


def parse_line(line):
    """Parse one line into a dict of the six int values.

    Returns None for malformed/partial lines instead of raising, so the
    plotting loop can simply skip them.
    """
    parts = line.split()
    if len(parts) != len(FIELDS):
        return None
    values = {}
    for part in parts:
        name, sep, raw = part.partition("=")
        if sep != "=" or name not in FIELDS:
            return None
        try:
            values[name] = int(raw)
        except ValueError:
            return None
    # Ensure all six expected fields are present (no duplicates/missing).
    if len(values) != len(FIELDS):
        return None
    return values


# --- Rolling buffers ----------------------------------------------------------
# A shared sample counter for the x-axis and one deque per field.
samples = deque(maxlen=WINDOW)
data = {name: deque(maxlen=WINDOW) for name in FIELDS}
sample_count = 0

# --- Open the serial port -----------------------------------------------------
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
except serial.SerialException as exc:
    sys.exit(f"Could not open {PORT} at {BAUD} baud: {exc}\n"
             "Tip: run  ls /dev/tty.usbmodem*  to find the right port.")

# --- Set up the figure --------------------------------------------------------
fig, (ax_accel, ax_gyro) = plt.subplots(2, 1, sharex=True, figsize=(10, 7))
fig.suptitle(f"MPU6050 live data — {PORT} @ {BAUD}")

accel_lines = {name: ax_accel.plot([], [], label=name)[0]
               for name in ("ax", "ay", "az")}
gyro_lines = {name: ax_gyro.plot([], [], label=name)[0]
              for name in ("gx", "gy", "gz")}

ax_accel.set_ylabel("accel (raw)")
ax_accel.legend(loc="upper left")
ax_accel.grid(True, alpha=0.3)

ax_gyro.set_ylabel("gyro (raw)")
ax_gyro.set_xlabel("sample")
ax_gyro.legend(loc="upper left")
ax_gyro.grid(True, alpha=0.3)


def read_available():
    """Drain whatever lines are waiting on the port into the buffers.

    Reads up to one screen-full per call so the plot stays responsive even
    if data arrives faster than the animation interval.
    """
    global sample_count
    for _ in range(WINDOW):
        if ser.in_waiting == 0:
            break
        try:
            raw = ser.readline().decode("ascii", errors="ignore").strip()
        except serial.SerialException:
            break
        if not raw:
            continue
        values = parse_line(raw)
        if values is None:
            continue  # skip malformed/partial lines
        sample_count += 1
        samples.append(sample_count)
        for name in FIELDS:
            data[name].append(values[name])


def update(_frame):
    read_available()

    xs = list(samples)
    for name, line in accel_lines.items():
        line.set_data(xs, list(data[name]))
    for name, line in gyro_lines.items():
        line.set_data(xs, list(data[name]))

    if xs:
        # Scroll the x-axis to show the most recent WINDOW samples.
        ax_gyro.set_xlim(xs[0], max(xs[-1], xs[0] + 1))
        ax_accel.relim()
        ax_accel.autoscale_view(scalex=False, scaley=True)
        ax_gyro.relim()
        ax_gyro.autoscale_view(scalex=False, scaley=True)

    return (*accel_lines.values(), *gyro_lines.values())


# blit=False so axis-limit (scrolling/autoscale) changes are redrawn.
ani = FuncAnimation(fig, update, interval=50, blit=False, cache_frame_data=False)

try:
    plt.show()
finally:
    ser.close()
