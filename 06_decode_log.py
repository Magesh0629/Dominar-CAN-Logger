"""
Step 4: Decode the raw CAN log using your DBC file, to validate that
your signal definitions actually make sense.

Usage:
    pip install cantools pandas
    python 06_decode_log.py canlog.csv dominar.dbc
"""

import sys
import pandas as pd
import cantools


def main():
    if len(sys.argv) < 3:
        print("Usage: python 06_decode_log.py <canlog.csv> <dominar.dbc>")
        sys.exit(1)

    log_path, dbc_path = sys.argv[1], sys.argv[2]

    db = cantools.database.load_file(dbc_path)
    df = pd.read_csv(log_path)

    decoded_count = 0
    unknown_ids = set()

    for _, row in df.iterrows():
        if row['id'] == 'EVENT':
            continue

        try:
            can_id = int(str(row['id']), 16)
            dlc = int(row['dlc'])
            data_bytes = bytes(
                int(row[f'd{i}'], 16) for i in range(dlc)
                if not pd.isna(row[f'd{i}'])
            )
            message = db.get_message_by_frame_id(can_id)
            decoded = db.decode_message(can_id, data_bytes)
            print(f"t={row['timestamp_ms']}ms  {message.name}: {decoded}")
            decoded_count += 1
        except KeyError:
            unknown_ids.add(row['id'])
        except Exception as e:
            print(f"Could not decode row at t={row['timestamp_ms']}: {e}")

    print()
    print(f"Decoded {decoded_count} frames successfully.")
    if unknown_ids:
        print(f"IDs not yet in your DBC ({len(unknown_ids)}): {sorted(unknown_ids)}")


if __name__ == "__main__":
    main()
