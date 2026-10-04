#!/usr/bin/env python3
"""Offline feature extraction for the IMU activity-recognition project.

For every trial (one continuous recording of one activity) it computes the
mean, standard deviation and peak-to-peak value of:

    accX, accY, accZ, accMag      (accMag  = sqrt(accX^2 + accY^2 + accZ^2))
    gyroX, gyroY, gyroZ, gyroMag  (gyroMag = sqrt(gyroX^2 + gyroY^2 + gyroZ^2))

Accepted input formats
----------------------
1. ``data/raw/trials_raw.csv``  (columns: activity, trial, time_ms, accX..gyroZ)
2. A raw serial-logger file     (columns: time_ms, label, accX..gyroZ[, detected])
   -> trials are split automatically wherever ``time_ms`` drops (new recording).

Usage
-----
    python scripts/extract_features.py
    python scripts/extract_features.py -i data/raw/trials_raw.csv -o data/processed/features_summary.csv
"""
import argparse
from pathlib import Path

import numpy as np
import pandas as pd

SIGNALS = ["accX", "accY", "accZ", "gyroX", "gyroY", "gyroZ"]
FEATURE_SIGNALS = ["accX", "accY", "accZ", "accMag", "gyroX", "gyroY", "gyroZ", "gyroMag"]


def load(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    if "activity" not in df.columns:
        if "label" not in df.columns:
            raise ValueError("Input needs an 'activity' or 'label' column.")
        df = df.rename(columns={"label": "activity"})
    for col in ["time_ms"] + SIGNALS:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna(subset=["time_ms"] + SIGNALS).reset_index(drop=True)
    if "trial" not in df.columns:
        # a new trial starts whenever the timestamp resets
        df["trial"] = (df["time_ms"].diff() < 0).cumsum() + 1
    return df


def extract(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df["accMag"] = np.sqrt(df.accX**2 + df.accY**2 + df.accZ**2)
    df["gyroMag"] = np.sqrt(df.gyroX**2 + df.gyroY**2 + df.gyroZ**2)

    rows = []
    for (activity, trial), g in df.groupby(["activity", "trial"], sort=False):
        row = {"activity": activity, "trial": int(trial), "n_samples": len(g)}
        for sig in FEATURE_SIGNALS:
            row[f"{sig}_mean"] = g[sig].mean()
            row[f"{sig}_std"] = g[sig].std()            # sample std (ddof=1)
            row[f"{sig}_ptp"] = g[sig].max() - g[sig].min()
        rows.append(row)
    return pd.DataFrame(rows)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-i", "--input", type=Path, default=Path("data/raw/trials_raw.csv"))
    p.add_argument("-o", "--output", type=Path, default=Path("data/processed/features_summary.csv"))
    args = p.parse_args()

    summary = extract(load(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    summary.to_csv(args.output, index=False)
    print(f"Saved {len(summary)} trials -> {args.output}\n")

    key = ["accY_std", "accZ_std", "gyroY_ptp", "accMag_std"]
    print("Average of the key features per activity:")
    print(summary.groupby("activity", sort=False)[key].mean().round(3).to_string())


if __name__ == "__main__":
    main()
