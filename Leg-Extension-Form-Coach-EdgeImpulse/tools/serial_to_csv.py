"""
serial_to_csv.py - OPTIONAL helper for data collection

Listens to the Wio Terminal running data_collection.ino and saves
every take printed between

    --- BEGIN <label>.<session>.<n>.csv ---
    ...
    --- END ---

into its own CSV file, ready to upload in Edge Impulse Studio
(Data acquisition -> Upload data). It only replaces copy/pasting
from the Serial Monitor; nothing is sent to the internet.

Close the Arduino Serial Monitor first (only one program can use
the port at a time).

Usage:
    pip install pyserial
    python serial_to_csv.py --port /dev/ttyACM0        # Linux
    python serial_to_csv.py --port COM5                # Windows
    python serial_to_csv.py --port /dev/ttyACM0 --out ../dataset
"""

import argparse
import os
import re

import serial  # pyserial

BEGIN_RE = re.compile(r"^--- BEGIN (\S+\.csv) ---$")
END_LINE = "--- END ---"


def unique_path(folder, name):
    """Never overwrite: add -1, -2, ... before .csv if the file exists."""
    path = os.path.join(folder, name)
    base, ext = os.path.splitext(path)
    n = 1
    while os.path.exists(path):
        path = f"{base}-{n}{ext}"
        n += 1
    return path


def main():
    parser = argparse.ArgumentParser(description="Save Wio Terminal takes as CSV files")
    parser.add_argument("--port", required=True, help="serial port, e.g. /dev/ttyACM0 or COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out", default="dataset", help="folder for the CSV files")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    print(f"Listening on {args.port}... (Ctrl+C to stop)")

    current = None  # open file while a take is being received
    with serial.Serial(args.port, args.baud, timeout=1) as port:
        try:
            while True:
                line = port.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                match = BEGIN_RE.match(line)
                if match:
                    path = unique_path(args.out, match.group(1))
                    current = open(path, "w", newline="")
                    rows = 0
                    print(f"Recording {path} ...")
                elif line == END_LINE and current:
                    current.close()
                    print(f"  saved {rows - 1} samples")  # minus the header line
                    current = None
                elif current:
                    current.write(line + "\n")
                    rows += 1
                else:
                    print(f"[wio] {line}")  # normal messages from the board
        except KeyboardInterrupt:
            print("\nStopped.")
        finally:
            if current:
                current.close()


if __name__ == "__main__":
    main()
