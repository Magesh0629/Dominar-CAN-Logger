"""
Step 3: Analyze canlog.csv to find which IDs/bytes correspond to which
real-world signal (RPM, speed, brake, etc.)

Usage:
    pip install pandas
    python 04_analyze_log.py canlog.csv

Then use the interactive functions below (or import into a notebook).
"""

import sys
import pandas as pd

EVENT_LABELS = {
    ord('r'): "throttle/RPM blip",
    ord('b'): "front brake",
    ord('c'): "rear brake",
    ord('t'): "turn signal on",
    ord('T'): "turn signal off",
    ord('g'): "gear change",
    ord('h'): "headlight/high beam",
    ord('s'): "speed change",
}


def load_log(path):
    df = pd.read_csv(path)
    return df


def summarize(df):
    frames = df[df['id'] != 'EVENT']
    events = df[df['id'] == 'EVENT']

    print(f"Total rows: {len(df)}")
    print(f"CAN frames: {len(frames)}")
    print(f"Event markers: {len(events)}")
    print()
    print("Frames per CAN ID (most frequent = periodic status messages,")
    print("likely candidates for RPM/speed/etc):")
    print(frames['id'].value_counts())
    print()
    if len(events):
        print("Event markers found:")
        for _, row in events.iterrows():
            code = int(row['dlc'])
            label = EVENT_LABELS.get(code, chr(code))
            print(f"  t={row['timestamp_ms']}ms  marker='{chr(code)}' ({label})")


def show_id_around_events(df, can_id_hex, window_ms=1500):
    """
    Show frames for a given CAN ID within window_ms before/after each
    event marker, so you can see which byte(s) changed when you performed
    the test action.
    """
    frames = df[df['id'].str.upper() == can_id_hex.upper()]
    events = df[df['id'] == 'EVENT']

    for _, ev in events.iterrows():
        t = ev['timestamp_ms']
        code = int(ev['dlc'])
        label = EVENT_LABELS.get(code, chr(code))
        print(f"\n--- Event '{chr(code)}' ({label}) at t={t}ms ---")
        window = frames[(frames['timestamp_ms'] >= t - window_ms) &
                         (frames['timestamp_ms'] <= t + window_ms)]
        cols = ['timestamp_ms', 'd0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7']
        print(window[cols].to_string(index=False))


def find_changing_ids(df, min_change_fraction=0.05):
    """
    Flags CAN IDs whose data bytes actually change over time (vs IDs that
    are constant/noise) — a quick way to shortlist candidates.
    """
    frames = df[df['id'] != 'EVENT']
    print("ID        byte-change activity (higher = more likely a live signal)")
    for can_id, group in frames.groupby('id'):
        byte_cols = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7']
        changes = 0
        total = 0
        for col in byte_cols:
            vals = group[col].dropna()
            if len(vals) < 2:
                continue
            changes += (vals != vals.shift()).sum()
            total += len(vals)
        if total > 0:
            frac = changes / total
            if frac > min_change_fraction:
                print(f"  {can_id:8s}  change fraction: {frac:.2f}  (n={total})")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "canlog.csv"
    df = load_log(path)
    summarize(df)
    print()
    find_changing_ids(df)
    print()
    print("Next: call show_id_around_events(df, '1A0') for any promising ID")
    print("to see exactly which byte moved when you triggered each event.")
