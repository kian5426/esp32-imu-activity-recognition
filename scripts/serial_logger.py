#!/usr/bin/env python3
"""Serial logger + accuracy evaluator for the ESP32 / MPU6050 activity detector.

Works with the firmware in ``firmware/activity_detector`` which streams
    time_ms,label,accX,accY,accZ,gyroX,gyroY,gyroZ,detected
where ``label`` is the ground truth you typed and ``detected`` is the
real-time rule-based prediction computed on the ESP32.

Usage
-----
    pip install -r requirements.txt
    python scripts/serial_logger.py --port COM6            # Windows
    python scripts/serial_logger.py --port /dev/ttyUSB0    # Linux / macOS

1. Close the Arduino IDE Serial Monitor (only one program can own the port).
2. Type a label (Walking, Running, Up-stairs, Down-stairs) and press Enter.
3. Touch the D4 wire to GND to start / stop a recording.
4. Press Ctrl+C when done. Two files are written to --out-dir:
     recorded_data.csv      every raw row (incl. "detected")
     accuracy_summary.csv   accuracy per activity and overall
"""
import argparse
import threading
import time
from pathlib import Path

import pandas as pd
import serial

COLUMNS = ["time_ms", "label", "accX", "accY", "accZ", "gyroX", "gyroY", "gyroZ", "detected"]
NUMERIC = COLUMNS[:1] + COLUMNS[2:8]

stop_event = threading.Event()
raw_rows: list[dict] = []


def parse_row(line: str):
    """Return a dict for a valid 9-field data row, otherwise None."""
    parts = line.split(",")
    if len(parts) != len(COLUMNS):
        return None
    try:
        row = {"label": parts[1], "detected": parts[8]}
        for name, value in zip(NUMERIC, parts[:1] + parts[2:8]):
            row[name] = float(value)
    except ValueError:                       # header row or malformed line
        return None
    return {c: row[c] for c in COLUMNS}


def reader(ser: serial.Serial) -> None:
    while not stop_event.is_set():
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
        except serial.SerialException as exc:
            print(f"Read error: {exc}")
            break
        if not line:
            continue
        print(line)
        row = parse_row(line)
        if row:
            raw_rows.append(row)


def compute_accuracy(df: pd.DataFrame) -> pd.DataFrame:
    """Accuracy per label and overall; warm-up rows ('...') are ignored."""
    valid = df[df["detected"] != "..."].copy()
    if valid.empty:
        return pd.DataFrame()
    valid["correct"] = valid["label"].str.lower() == valid["detected"].str.lower()

    rows = [
        {
            "label": label,
            "total_windows": len(g),
            "correct": int(g["correct"].sum()),
            "accuracy_percent": round(100.0 * g["correct"].mean(), 2),
        }
        for label, g in valid.groupby("label")
    ]
    rows.append(
        {
            "label": "OVERALL",
            "total_windows": len(valid),
            "correct": int(valid["correct"].sum()),
            "accuracy_percent": round(100.0 * valid["correct"].mean(), 2),
        }
    )
    return pd.DataFrame(rows)


def save_outputs(out_dir: Path) -> None:
    if not raw_rows:
        print("No data collected, nothing to save.")
        return
    out_dir.mkdir(parents=True, exist_ok=True)

    df = pd.DataFrame(raw_rows, columns=COLUMNS)
    raw_path = out_dir / "recorded_data.csv"
    df.to_csv(raw_path, index=False)
    print(f"\nSaved raw data to {raw_path}")

    summary = compute_accuracy(df)
    if summary.empty:
        print("No detection rows found (all were '...'); accuracy cannot be computed.")
        return
    summary_path = out_dir / "accuracy_summary.csv"
    summary.to_csv(summary_path, index=False)
    print(f"Saved accuracy summary to {summary_path}\n")
    print(summary.to_string(index=False))


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="COM6", help="serial port, e.g. COM6 or /dev/ttyUSB0")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--out-dir", type=Path, default=Path("data/raw"))
    args = p.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(2)  # the ESP32 resets when the port is opened

    print(f"Connected to {args.port}.")
    print("Type a label (Walking, Running, Up-stairs, Down-stairs) and press Enter.")
    print("Touch the D4 wire to GND to start/stop recording. Ctrl+C to stop and save.\n")

    threading.Thread(target=reader, args=(ser,), daemon=True).start()
    try:
        while True:
            label = input().strip()
            if label:
                ser.write((label + "\n").encode("utf-8"))
                print(f">> Sent label: {label}")
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        stop_event.set()
        time.sleep(0.5)
        ser.close()
        save_outputs(args.out_dir)


if __name__ == "__main__":
    main()
