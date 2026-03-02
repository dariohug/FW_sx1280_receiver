import serial
import csv
import re
import time
from datetime import datetime

# ---------------- CONFIG ----------------
PORT = "/dev/ttyACM0"
BAUD = 115200
DURATION = 30   # 5 minutes

distance = input("Enter measurement distance (meters): ")

RAW_FILE = f"radio_raw_{distance}.csv"
SUMMARY_FILE = "radio_summary.csv"

pattern = re.compile(
    r"Mode:\s*(\w+).*RX\s*\((\d+)\s*bytes\):\s*((?:[0-9A-F]{2}\s+)+).*RSSI:\s*(-?\d+)"
)

ser = serial.Serial(PORT, BAUD, timeout=1)

start_time = time.time()

received_packets = 0
packet_numbers = []
rssi_values = []
payload_bytes = 0
mode_value = "UNKNOWN"

# ---------------- RAW CSV ----------------
with open(RAW_FILE, "w", newline="") as raw_file:

    raw_writer = csv.writer(raw_file)
    raw_writer.writerow([
        "timestamp",
        "mode",
        "packet_counter",
        "rssi",
        "distance"
    ])

    while time.time() - start_time < DURATION:

        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            continue

        match = pattern.search(line)
        if not match:
            continue

        mode_value = match.group(1)
        payload_bytes = int(match.group(2))
        payload = match.group(3).strip()
        rssi = int(match.group(4))

        bytes_list = payload.split()

        if len(bytes_list) < 2:
            continue

        # little-endian 16-bit counter
        packet_counter = int(bytes_list[0], 16) | (int(bytes_list[1], 16) << 8)

        now = datetime.now()

        raw_writer.writerow([
            now,
            mode_value,
            packet_counter,
            rssi,
            distance
        ])

        received_packets += 1
        packet_numbers.append(packet_counter)
        rssi_values.append(rssi)

        print(f"Packet: {packet_counter}   RSSI: {rssi}      ", end="\r", flush=True)

print("Measurement finished")

if received_packets == 0:
    print("No packets received")
    exit()

# ---------------- HANDLE COUNTER OVERFLOW ----------------
first = packet_numbers[0]
last = packet_numbers[-1]

if last >= first:
    expected_packets = last - first + 1
else:
    # overflow case
    expected_packets = (65536 - first) + last + 1

packet_loss = 100 * (1 - (received_packets / expected_packets))

# ---------------- RSSI stats ----------------
rssi_min = min(rssi_values)
rssi_max = max(rssi_values)
rssi_avg = sum(rssi_values) / len(rssi_values)

# ---------------- DATA RATE ----------------
data_rate = (received_packets * payload_bytes * 8) / DURATION

now = datetime.now()

# ---------------- SUMMARY CSV ----------------
with open(SUMMARY_FILE, "a", newline="") as summary_file:

    writer = csv.writer(summary_file)

    if summary_file.tell() == 0:
        writer.writerow([
            "mode",
            "date",
            "time",
            "distance",
            "rssi_min",
            "rssi_max",
            "rssi_avg",
            "packet_loss_percent",
            "data_rate_bit_s",
            "received_packets"
        ])

    writer.writerow([
        mode_value,
        now.date(),
        now.time(),
        distance,
        rssi_min,
        rssi_max,
        round(rssi_avg, 2),
        round(packet_loss, 2),
        round(data_rate, 2),
        received_packets
    ])

print("Summary written")