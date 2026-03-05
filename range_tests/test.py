import serial
import csv
import time
from datetime import datetime

# Disclaimer: Some AI was used for this testfile 

# ---------------- CONFIG ----------------
PORT = "/dev/ttyACM0"
BAUD = 115200
DURATION = 10

distance = input("Enter measurement distance (meters): ")

RAW_FILE = f"radio_raw_{distance}.csv"
SUMMARY_FILE = "radio_summary.csv"

ser = serial.Serial(PORT, BAUD, timeout=1)

start_time = time.time()

received_packets = 0
packet_numbers = []
rssi_values = []
payload_bytes = 64  # 32-bit counter
mode_value = "FLRC"

# ---------------- RAW CSV ----------------
with open(RAW_FILE, "w", newline="") as raw_file:

    raw_writer = csv.writer(raw_file)
    raw_writer.writerow([
        "timestamp",
        "packet_counter",
        "rssi",
        "distance"
    ])

    while time.time() - start_time < DURATION:

        line = ser.readline().decode(errors="ignore").strip()

        if not line.startswith("R,"):
            continue

        try:
            _, counter, rssi = line.split(",")
            counter = int(counter)
            rssi = int(rssi)
        except:
            continue

        now = datetime.now()

        raw_writer.writerow([
            now,
            counter,
            rssi,
            distance
        ])

        received_packets += 1
        packet_numbers.append(counter)
        rssi_values.append(rssi)

        print(f"Pkt: {counter}  RSSI: {rssi}", end="\r", flush=True)

print("\nMeasurement finished")

if received_packets == 0:
    print("No packets received")
    exit()

# expected_packets = int(packet_numbers[-1]) - int(packet_numbers[0]) + 1

expected_packets = DURATION * (1000 / 6.3)

packet_loss = 100 * (1 - (received_packets / expected_packets))

print(received_packets)
print(expected_packets)



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

print("\n" + "=" * 45)
print("            RADIO LINK SUMMARY")
print("=" * 45)
print(f"Distance:            {distance} m")
print(f"Received packets:    {received_packets}")
print(f"Packet loss:         {packet_loss:.2f} %")
print(f"Mean RSSI:           {rssi_avg:.2f} dBm")
print(f"RSSI min / max:      {rssi_min} / {rssi_max} dBm")
print(f"Effective rate:      {data_rate:.2f} bit/s")
print("=" * 45)