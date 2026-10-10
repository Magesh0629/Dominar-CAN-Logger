#!/usr/bin/env python3
"""
make_sample_log.py - generate a FAKE bike log in the exact format the ESP32 writes.

Lets you test analyze_log.py / decode_log.py with no bike attached:
    python make_sample_log.py ../sample_data/sample_canlog.csv

Fake bus contents (IDs are made up, NOT real Dominar IDs):
    0x100  every 20 ms   RPM (bytes 0-1 little-endian, raw = rpm * 4), rolling counter byte 7
    0x200  every 50 ms   speed km/h (byte 0), gear (byte 1)
    0x300  every 100 ms  bit0 front brake, bits1-2 turn signal
    0x7E0  every 500 ms  constant payload (noise)
    0x18DAF110 every 200 ms  constant payload, EXTENDED id (flag bit 31 set like the library does)
Markers: r @5s (throttle blip), b @10s (front brake), t @15s (turn signal on)
"""
import random
import sys

random.seed(1)
out = sys.argv[1] if len(sys.argv) > 1 else "sample_canlog.csv"
T0, DURATION = 1000, 20000
rows, counter = [], 0


def rpm_at(t):
    return 6000 if 5000 <= t < 7000 else 1500 + random.randint(-15, 15)


def add(t, cid, data):
    rows.append((t, cid, data))


for t in range(0, DURATION, 20):
    rpm = rpm_at(t)
    raw = int(rpm * 4)
    counter = (counter + 1) & 0xF
    add(T0 + t, "100", [raw & 0xFF, raw >> 8, 0, 0, 0, 0, 0, counter])
for t in range(0, DURATION, 50):
    speed = 0 if t < 8000 else 40
    add(T0 + t, "200", [speed, 1 if speed else 0, 0, 0, 0, 0, 0, 0])
for t in range(0, DURATION, 100):
    brake = 1 if 10000 <= t < 12000 else 0
    turn = 1 if t >= 15000 else 0
    add(T0 + t, "300", [brake | (turn << 1), 0, 0, 0, 0, 0, 0, 0])
for t in range(0, DURATION, 500):
    add(T0 + t, "7E0", [0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0])
for t in range(0, DURATION, 200):
    add(T0 + t, "%X" % (0x18DAF110 | 0x80000000), [1, 2, 3, 4, 5, 6, 7, 8])

rows.sort(key=lambda r: r[0])
events = [(T0 + 5000, "r"), (T0 + 10000, "b"), (T0 + 15000, "t")]

with open(out, "w") as f:
    f.write("timestamp_ms,id,dlc,d0,d1,d2,d3,d4,d5,d6,d7\n")
    merged = [(t, "F", cid, d) for t, cid, d in rows] + [(t, "E", c, None) for t, c in events]
    merged.sort(key=lambda r: r[0])
    for t, kind, a, d in merged:
        if kind == "E":
            f.write(f"{t},EVENT,{ord(a)},,,,,,,,\n")
        else:
            cells = ["%X" % b for b in d] + [""] * (8 - len(d))
            f.write(f"{t},{a},{len(d)}," + ",".join(cells) + "\n")
print(f"wrote {out} ({len(rows)} frames, {len(events)} markers)")
