import serial
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# --- CONFIGURATION ---
COM_PORT = 'COM4'      # Your Arduino COM port
BAUD_RATE = 1000000    # 1 Mbps
NUM_SAMPLES = 400
V_REF = 5.0            # Match your Arduino vRef

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.5)
except Exception as e:
    print(f"Could not open {COM_PORT}. Close Arduino Serial Monitor first!\nError: {e}")
    exit()

# Setup Oscilloscope Window
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(10, 5))
fig.canvas.manager.set_window_title('Arduino 50kS/s Digital Oscilloscope (Press Q to Quit)')

x_ms = np.linspace(0, 8.0, NUM_SAMPLES)
y_volts = np.zeros(NUM_SAMPLES)

line, = ax.plot(x_ms, y_volts, color='#00FF66', linewidth=2)
ax.set_ylim(-0.3, 5.3)
ax.set_xlim(0, 8.0)
ax.set_ylabel('Voltage (V)', fontsize=12)
ax.set_xlabel('Time (ms)', fontsize=12)
ax.set_title('Triggered Waveform Capture (Press Q or Esc to exit)', fontsize=13, color='cyan')
ax.grid(True, linestyle='--', alpha=0.4)

info_text = ax.text(0.02, 0.92, 'Waiting for trigger...', transform=ax.transAxes,
                    fontsize=11, color='yellow', fontweight='bold',
                    bbox=dict(facecolor='black', edgecolor='cyan', alpha=0.8))

# Allow pressing 'q' or 'Escape' to cleanly close window and free COM4
def on_key(event):
    if event.key in ['q', 'Q', 'escape']:
        plt.close(fig)

fig.canvas.mpl_connect('key_press_event', on_key)

def update(frame):
    # Flush old backlog so we always plot the newest frame
    if ser.in_waiting > 1800:
        ser.reset_input_buffer()

    # Look for 2-byte sync header (0xAA 0x55)
    while ser.in_waiting >= 2:
        if ser.read(1) == b'\xaa' and ser.read(1) == b'\x55':
            # Read 4-byte duration + 800 bytes of sample data (400 * 2 bytes)
            payload = ser.read(4 + NUM_SAMPLES * 2)
            if len(payload) == 4 + NUM_SAMPLES * 2:
                duration_us = struct.unpack('<I', payload[:4])[0]
                raw_adc = np.frombuffer(payload[4:], dtype='<u2')
                
                volts = (raw_adc * V_REF) / 1024.0
                total_ms = duration_us / 1000.0
                sample_rate_khz = (NUM_SAMPLES / duration_us) * 1000.0
                
                time_axis = np.linspace(0, total_ms, NUM_SAMPLES)
                line.set_data(time_axis, volts)
                ax.set_xlim(0, total_ms)
                
                v_min, v_max = np.min(volts), np.max(volts)
                info_text.set_text(
                    f"Rate: {sample_rate_khz:.1f} kS/s | Min: {v_min:.2f}V | Max: {v_max:.2f}V"
                )
            break

    return line, info_text

ani = animation.FuncAnimation(fig, update, interval=30, blit=True, cache_frame_data=False)
plt.tight_layout()
plt.show()

ser.close()
print("COM4 closed cleanly.")
