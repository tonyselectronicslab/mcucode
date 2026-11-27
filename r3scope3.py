import serial
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# --- CONFIGURATION ---
COM_PORT = 'COM4'
BAUD_RATE = 1000000        # Matches Serial.begin(1000000)
NUM_SAMPLES = 800
V_REF = 5.0
SYNC_HEADER = b'\xff\x00\xaa\x55'
SYNC_FOOTER = b'\x55\xaa'
PACKET_SIZE = 4 + 4 + NUM_SAMPLES + 2  # Header(4) + Duration(4) + Data(800) + Footer(2)

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.2)
except Exception as e:
    print(f"Could not open {COM_PORT}. Close Arduino Serial Monitor first!\nError: {e}")
    exit()

plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(11, 5.5))
fig.canvas.manager.set_window_title('Arduino 120kS/s Digital Oscilloscope (Press Q to Quit)')

x_ms = np.linspace(0, 6.5, NUM_SAMPLES)
y_volts = np.zeros(NUM_SAMPLES)

line, = ax.plot(x_ms, y_volts, color='#00FF66', linewidth=2)
ax.set_ylim(-0.3, 5.4)
ax.set_xlim(0, 6.5)
ax.set_ylabel('Voltage (V)', fontsize=12)
ax.set_xlabel('Time (ms)', fontsize=12)
ax.set_title('120kS/s Digital Oscilloscope (Press Q or Esc to exit)', fontsize=13, color='cyan')
ax.grid(True, linestyle='--', alpha=0.4)

info_text = ax.text(0.02, 0.92, 'Waiting for trigger...', transform=ax.transAxes,
                    fontsize=10.5, color='yellow', fontweight='bold',
                    bbox=dict(facecolor='black', edgecolor='cyan', alpha=0.85))

def on_key(event):
    if event.key in ['q', 'Q', 'escape']:
        plt.close(fig)

fig.canvas.mpl_connect('key_press_event', on_key)

def measure_signal(volts, sample_rate_hz):
    v_min = np.min(volts)
    v_max = np.max(volts)
    v_pp = v_max - v_min

    if v_pp < 0.3:
        return 0.0, 0.0, v_min, v_max

    mid_level = (v_max + v_min) / 2.0
    hysteresis = v_pp * 0.15
    duty_cycle = (np.sum(volts > mid_level) / len(volts)) * 100.0

    rising_edges = []
    armed = bool(volts[0] < (mid_level - hysteresis))
    for i in range(1, len(volts)):
        if not armed and volts[i] < (mid_level - hysteresis):
            armed = True
        elif armed and volts[i] >= (mid_level + hysteresis):
            rising_edges.append(i)
            armed = False

    if len(rising_edges) >= 2:
        avg_period_samples = (rising_edges[-1] - rising_edges[0]) / (len(rising_edges) - 1)
        freq_hz = sample_rate_hz / avg_period_samples
    else:
        freq_hz = 0.0

    return freq_hz, duty_cycle, v_min, v_max

rx_buffer = bytearray()

def update(frame):
    global rx_buffer

    waiting = ser.in_waiting
    if waiting > 0:
        rx_buffer.extend(ser.read(waiting))

    if len(rx_buffer) > PACKET_SIZE * 4:
        rx_buffer = rx_buffer[-(PACKET_SIZE * 3):]

    # Search backward for a complete, verified packet (Header + exact length + Footer)
    search_end = len(rx_buffer)
    while True:
        idx = rx_buffer.rfind(SYNC_HEADER, 0, search_end)
        if idx == -1:
            break
        if len(rx_buffer) >= idx + PACKET_SIZE:
            if rx_buffer[idx + PACKET_SIZE - 2 : idx + PACKET_SIZE] == SYNC_FOOTER:
                packet = rx_buffer[idx + 4 : idx + PACKET_SIZE - 2]
                rx_buffer = rx_buffer[idx + PACKET_SIZE:]

                duration_us = struct.unpack('<I', packet[:4])[0]
                raw_adc = np.frombuffer(packet[4:], dtype=np.uint8)

                if 1000 < duration_us < 100000:
                    volts = (raw_adc * V_REF) / 255.0
                    total_ms = duration_us / 1000.0
                    sample_rate_hz = (NUM_SAMPLES / duration_us) * 1_000_000.0
                    sample_rate_khz = sample_rate_hz / 1000.0

                    time_axis = np.linspace(0, total_ms, NUM_SAMPLES)
                    line.set_data(time_axis, volts)
                    ax.set_xlim(0, total_ms)

                    freq_hz, duty, v_min, v_max = measure_signal(volts, sample_rate_hz)

                    if freq_hz >= 1000:
                        freq_str = f"{freq_hz / 1000.0:.2f} kHz"
                    elif freq_hz > 0:
                        freq_str = f"{freq_hz:.1f} Hz"
                    else:
                        freq_str = "DC / No Wave"

                    info_text.set_text(
                        f"Freq: {freq_str}  |  Duty: {duty:.1f}%  |  Min: {v_min:.2f}V  |  Max: {v_max:.2f}V  |  Rate: {sample_rate_khz:.1f} kS/s"
                    )
                break
        search_end = idx

    return line, info_text

ani = animation.FuncAnimation(fig, update, interval=30, blit=False, cache_frame_data=False)
plt.tight_layout()
plt.show()

ser.close()
print("COM4 closed cleanly.")
