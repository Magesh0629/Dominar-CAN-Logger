#!/usr/bin/env python3
"""
analyze_log.py - find which CAN IDs / bytes correspond to real bike signals.

Usage:
    python analyze_log.py canlog_001.csv                 # summary + changing IDs
    python analyze_log.py canlog_001.csv --events        # what changed at each marker
    python analyze_log.py canlog_001.csv --id 1A0        # raw bytes around each marker
    python analyze_log.py canlog_001.csv --events --window 2000

Log format (written by the ESP32 firmware):
    timestamp_ms,id,dlc,d0..d7        (id and data bytes are hex)
    marker rows:  timestamp_ms,EVENT,<ascii code of the key you typed>,,,,,,,,
"""

import argparse
from collections import Counter

import pandas as pd

BYTE_COLS = [f"d{i}" for i in range(8)]

# Default marker meanings (match the convention in the firmware header)
EVENT_LABELS = {
    "r": "throttle/RPM blip", "b": "front brake", "c": "rear brake",
    "t": "turn signal on", "T": "turn signal off", "g": "gear change",
    "h": "high beam on", "H": "high beam off", "s": "speed change",
    "e": "engine start", "n": "neutral / clutch", "x": "kill switch",
    "*": "button press",
}


def load_log(path):
    """Load CSV as strings (avoids pandas guessing 'A0' vs '100' types)."""
    df = pd.read_csv(path, dtype=str, keep_default_na=False)
    df["timestamp_ms"] = df["timestamp_ms"].astype(int)
    df["id"] = df["id"].str.strip().str.upper()

    events = df[df["id"] == "EVENT"].copy()
    events["marker"] = events["dlc"].astype(int).map(chr)

    frames = df[df["id"] != "EVENT"].copy()
    frames["dlc"] = frames["dlc"].astype(int)
    # The MCP2515 library sets bit 31 for extended (29-bit) IDs - strip it.
    raw = frames["id"].apply(lambda s: int(s, 16))
    frames["extended"] = (raw & 0x80000000) != 0
    frames["id"] = (raw & 0x1FFFFFFF).apply(lambda v: f"{v:X}")
    for c in BYTE_COLS:
        frames[c] = frames[c].apply(lambda s: int(s, 16) if s != "" else -1)
    return frames.reset_index(drop=True), events.reset_index(drop=True)


def label(marker):
    return EVENT_LABELS.get(marker, "custom marker")


def summarize(frames, events):
    if frames.empty:
        print("No CAN frames in this log (bus silent? wrong file?).")
        return
    dur = (frames.timestamp_ms.max() - frames.timestamp_ms.min()) / 1000
    print(f"Frames: {len(frames)}   Markers: {len(events)}   Duration: {dur:.1f}s")
    if frames["extended"].any():
        print("Note: some frames use 29-bit extended IDs "
              "(use ID | 0x80000000 in the DBC for those messages).")
    print("\nID       count    rate(Hz)  (periodic status messages = best candidates)")
    for cid, n in frames["id"].value_counts().items():
        print(f"  {cid:>8s} {n:7d} {n / dur if dur else 0:9.1f}")
    if len(events):
        print("\nMarkers:")
        for _, e in events.iterrows():
            print(f"  t={e.timestamp_ms:>8d} ms  '{e.marker}'  {label(e.marker)}")


def changing_ids(frames, min_fraction=0.0):
    """Rank IDs/bytes by how much their value actually changes over time."""
    rows = []
    for cid, g in frames.groupby("id"):
        for col in BYTE_COLS:
            v = g[col]
            v = v[v >= 0].reset_index(drop=True)
            if len(v) < 3 or v.nunique() < 2:
                continue  # constant bytes are not signals
            frac = (v.iloc[1:].values != v.iloc[:-1].values).sum() / (len(v) - 1)
            if frac >= min_fraction:
                rows.append((cid, col, frac, v.nunique()))
    print("\nBytes that change over time (ID, byte, change-rate, distinct values):")
    print("(change-rate ~1.00 with a small repeating value set = probably a rolling counter)")
    if not rows:
        print("  none found")
    for cid, col, frac, uniq in sorted(rows, key=lambda r: -r[2])[:40]:
        print(f"  {cid:>8s}  {col}  {frac:5.2f}  {uniq:3d} values")


def event_diffs(frames, events, window):
    """
    For each marker, compare each ID/byte just before vs just after.
    A byte is reported only if its median moved by more than 2x its own
    normal spread - this hides rolling counters and noisy bytes, and keeps
    real step changes (brake 0->1, turn signal 0->2, RPM jump).
    """
    if events.empty:
        print("\nNo markers in log - type marker keys while logging (see README).")
        return
    for _, e in events.iterrows():
        t = e.timestamp_ms
        before = frames[(frames.timestamp_ms >= t - window) & (frames.timestamp_ms < t)]
        after = frames[(frames.timestamp_ms >= t) & (frames.timestamp_ms <= t + window)]
        print(f"\n=== marker '{e.marker}' ({label(e.marker)}) at t={t} ms "
              f"(+/-{window} ms) ===")
        hits = []
        for cid in sorted(set(before["id"]) & set(after["id"])):
            b, a = before[before["id"] == cid], after[after["id"] == cid]
            for col in BYTE_COLS:
                vb, va = b[col][b[col] >= 0], a[col][a[col] >= 0]
                if len(vb) < 2 or len(va) < 2:
                    continue
                mb, ma = vb.median(), va.median()
                spread = max(vb.std(), va.std())
                shift = abs(ma - mb)
                if shift > 0 and shift > 2 * spread:
                    hits.append((shift / (spread + 1), cid, col, mb, ma))
        for _, cid, col, mb, ma in sorted(hits, reverse=True)[:15]:
            print(f"  ID {cid:>6s} {col}: 0x{int(round(mb)):02X} -> 0x{int(round(ma)):02X}")
        if not hits:
            print("  no clear step change found in this window")


def show_id(frames, events, cid, window):
    cid = cid.upper().lstrip("0X") or "0"
    sub = frames[frames["id"] == cid]
    if sub.empty:
        print(f"ID {cid} not found in log.")
        return
    if events.empty:
        print(sub[["timestamp_ms"] + BYTE_COLS].to_string(index=False))
        return
    for _, e in events.iterrows():
        t = e.timestamp_ms
        w = sub[(sub.timestamp_ms >= t - window) & (sub.timestamp_ms <= t + window)]
        print(f"\n--- ID {cid} around marker '{e.marker}' ({label(e.marker)}) t={t} ms ---")
        show = w[["timestamp_ms"] + BYTE_COLS].copy()
        for c in BYTE_COLS:
            show[c] = show[c].apply(lambda v: "--" if v < 0 else f"{v:02X}")
        print(show.to_string(index=False))


def main():
    p = argparse.ArgumentParser(description="Analyze a Dominar CAN log")
    p.add_argument("csv")
    p.add_argument("--events", action="store_true", help="show byte changes at each marker")
    p.add_argument("--id", help="show raw bytes of this hex ID around each marker")
    p.add_argument("--window", type=int, default=1500, help="ms before/after marker")
    a = p.parse_args()

    frames, events = load_log(a.csv)
    if a.id:
        show_id(frames, events, a.id, a.window)
        return
    summarize(frames, events)
    if a.events:
        event_diffs(frames, events, a.window)
    else:
        changing_ids(frames)
        print("\nNext: run again with --events to see what changed at each marker.")


if __name__ == "__main__":
    main()
