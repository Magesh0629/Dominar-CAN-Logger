#!/usr/bin/env python3
"""
decode_log.py - decode a raw CAN log with your DBC to validate the signals.

Usage:
    python decode_log.py canlog_001.csv dbc/dominar.dbc
    python decode_log.py canlog_001.csv dbc/dominar.dbc -o decoded_001.csv
    python decode_log.py canlog_001.csv dbc/dominar.dbc --quiet   # summary only

The output CSV has one row per decoded signal: timestamp_ms, message, signal, value
(plot it in Excel / pandas / PlotJuggler and compare against the dash).
"""

import argparse
import sys

import cantools
import pandas as pd

BYTE_COLS = [f"d{i}" for i in range(8)]


def main():
    p = argparse.ArgumentParser(description="Decode a Dominar CAN log with a DBC")
    p.add_argument("csv")
    p.add_argument("dbc")
    p.add_argument("-o", "--out", help="write decoded signals to this CSV")
    p.add_argument("--quiet", action="store_true", help="don't print every frame")
    a = p.parse_args()

    db = cantools.database.load_file(a.dbc)
    if not db.messages:
        print("Your DBC has no messages yet - fill it in first (see docs/dbc_format.md).")
        sys.exit(1)

    df = pd.read_csv(a.csv, dtype=str, keep_default_na=False)
    df["id"] = df["id"].str.strip().str.upper()
    df = df[df["id"] != "EVENT"]

    rows, decoded, unknown, failed = [], 0, {}, 0
    for r in df.itertuples(index=False):
        raw_id = int(r.id, 16)
        frame_id = raw_id & 0x1FFFFFFF
        dlc = int(r.dlc)
        data = bytes(int(getattr(r, c), 16) for c in BYTE_COLS[:dlc])
        try:
            msg = db.get_message_by_frame_id(frame_id)
        except KeyError:
            key = f"{frame_id:X}"
            unknown[key] = unknown.get(key, 0) + 1
            continue
        try:
            sigs = msg.decode(data, allow_truncated=True)
        except Exception as e:  # bad length / scaling problem in the DBC
            failed += 1
            if failed <= 5:
                print(f"decode error t={r.timestamp_ms} {msg.name}: {e}")
            continue
        decoded += 1
        if not a.quiet:
            print(f"t={r.timestamp_ms:>8s} ms  {msg.name}: {sigs}")
        for name, val in sigs.items():
            rows.append((int(r.timestamp_ms), msg.name, name, val))

    print(f"\nDecoded frames: {decoded}   decode errors: {failed}")
    if unknown:
        top = sorted(unknown.items(), key=lambda kv: -kv[1])[:15]
        print("IDs not in your DBC yet (id: frames): "
              + ", ".join(f"{k}: {v}" for k, v in top))

    if a.out:
        pd.DataFrame(rows, columns=["timestamp_ms", "message", "signal", "value"]).to_csv(
            a.out, index=False)
        print(f"Wrote {len(rows)} signal samples to {a.out}")


if __name__ == "__main__":
    main()
