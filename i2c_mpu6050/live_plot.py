import serial
import matplotlib.pyplot as plt
from collections import deque

PORT = '/dev/tty.usbmodem1103'
BAUD = 115200
WINDOW = 200

ser = serial.Serial(PORT, BAUD, timeout=1)

angle = deque([0] * WINDOW, maxlen=WINDOW)
ax = deque([0] * WINDOW, maxlen=WINDOW)
ay = deque([0] * WINDOW, maxlen=WINDOW)
az = deque([0] * WINDOW, maxlen=WINDOW)
gx = deque([0] * WINDOW, maxlen=WINDOW)
gy = deque([0] * WINDOW, maxlen=WINDOW)
gz = deque([0] * WINDOW, maxlen=WINDOW)

plt.ion()
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)

l_angle, = ax1.plot(angle, color='black', linewidth=2)
ax1.set_ylabel('fused angle (deg)')
ax1.set_ylim(-180, 180)
ax1.grid(True, alpha=0.3)

l_ax, = ax2.plot(ax, label='ax')
l_ay, = ax2.plot(ay, label='ay')
l_az, = ax2.plot(az, label='az')
ax2.set_ylabel('accel (counts)')
ax2.set_ylim(-20000, 20000)
ax2.legend(loc='upper right')
ax2.grid(True, alpha=0.3)

l_gx, = ax3.plot(gx, label='gx')
l_gy, = ax3.plot(gy, label='gy')
l_gz, = ax3.plot(gz, label='gz')
ax3.set_ylabel('gyro (counts)')
ax3.set_ylim(-5000, 5000)
ax3.legend(loc='upper right')
ax3.grid(True, alpha=0.3)

fig.tight_layout()

n = 0
while True:
    line = ser.readline().decode('ascii', errors='ignore').strip()
    parts = line.split(',')
    if len(parts) != 7:
        continue
    try:
        vals = [int(p) for p in parts]
    except ValueError:
        continue

    angle.append(vals[0] / 100.0)
    ax.append(vals[1])
    ay.append(vals[2])
    az.append(vals[3])
    gx.append(vals[4])
    gy.append(vals[5])
    gz.append(vals[6])

    n += 1
    if n % 5 == 0:
        l_angle.set_ydata(angle)
        l_ax.set_ydata(ax)
        l_ay.set_ydata(ay)
        l_az.set_ydata(az)
        l_gx.set_ydata(gx)
        l_gy.set_ydata(gy)
        l_gz.set_ydata(gz)
        fig.canvas.draw_idle()
        fig.canvas.flush_events()