import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# --- CONFIGURATION ---
COM_PORT = 'COM4'   # Change this to your Arduino's COM port (e.g., 'COM4')
BAUD_RATE = 115200
MAX_POINTS = 200    # Number of samples shown across the screen

# Connect to Arduino
try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
except Exception as e:
    print(f"Could not open {COM_PORT}. Close Arduino Serial Monitor first!\nError: {e}")
    exit()

# Data buffer
data = deque([0.0] * MAX_POINTS, maxlen=MAX_POINTS)

# Setup Oscilloscope Window
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(10, 5))
fig.canvas.manager.set_window_title('Arduino 0-5V Oscilloscope')

line, = ax.plot(range(MAX_POINTS), data, color='#00FF66', linewidth=2)
ax.set_ylim(-0.2, 5.2)
ax.set_xlim(0, MAX_POINTS - 1)
ax.set_ylabel('Voltage (V)', fontsize=12)
ax.set_xlabel('Samples', fontsize=12)
ax.set_title('Live Voltage Waveform (0 - 5V)', fontsize=14, color='cyan')
ax.grid(True, linestyle='--', alpha=0.4)

# Live voltage text readout in top-right corner
voltage_text = ax.text(0.82, 0.92, '0.00 V', transform=ax.transAxes,
                       fontsize=16, color='yellow', fontweight='bold',
                       bbox=dict(facecolor='black', edgecolor='cyan', alpha=0.8))

def update(frame):
    # Read all waiting lines from serial buffer to prevent lag
    while ser.in_waiting:
        try:
            raw_line = ser.readline().decode('utf-8').strip()
            if raw_line:
                val = float(raw_line)
                data.append(val)
                voltage_text.set_text(f"{val:.2f} V")
        except ValueError:
            pass  # Ignore partial/corrupted lines on startup

    line.set_ydata(data)
    return line, voltage_text

# Animate at ~30 FPS
ani = animation.FuncAnimation(fig, update, interval=30, blit=True, cache_frame_data=False)
plt.tight_layout()
plt.show()

ser.close()
