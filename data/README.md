# Data

All recordings come from one subject wearing an MPU6050 strapped to the waist/chest, sampled at ~20 Hz
(50 ms period). Accelerations are in m/s², angular rates in rad/s (Adafruit MPU6050 library units).

| File | Rows | Description |
|------|-----:|-------------|
| `raw/trials_raw.csv` | 3,467 | Training recordings: 4 activities × 5 trials (20–30 s each). Input of `scripts/extract_features.py`. |
| `raw/accuracy_run.csv` | 2,587 | Real-time evaluation run streamed by the firmware, including its prediction. |
| `processed/features_summary.csv` | 20 | Per-trial mean / std / peak-to-peak of acc and gyro (axes and magnitude). |
| `results/accuracy_summary.csv` | 5 | Per-activity and overall real-time accuracy (1949 evaluated windows). |

## Column reference

**`raw/trials_raw.csv`**: `activity` (walking, running, up-stairs, down-stairs), `trial` (1–5),
`time_ms` (ms since the recording started), `accX/accY/accZ`, `gyroX/gyroY/gyroZ`.

**`raw/accuracy_run.csv`**: `time_ms, label` (ground truth), `accX…gyroZ`, `detected` (firmware prediction,
`...` while the first 40-sample window fills), `status` (`correct`, `incorrect` or `warmup`).
The 638 `warmup` rows are excluded from the accuracy numbers (2,587 − 638 = 1,949 windows).

**`processed/features_summary.csv`**: `activity, trial, n_samples` plus
`{accX, accY, accZ, accMag, gyroX, gyroY, gyroZ, gyroMag}_{mean, std, ptp}`.
`std` is the sample standard deviation (ddof = 1); the firmware uses the population standard deviation
over 40 samples, a negligible difference for thresholding.

## Data preparation notes

The original recordings lived in spreadsheets exported from the Serial Monitor. They were converted to
plain CSV with these changes:

- **Labels:** the label column in the spreadsheet was corrupted by spreadsheet auto-fill
  (e.g. `down-stairs11`, `down-stairs12`, … inside a single trial, and the typo `dowm-stairs13`).
  The activity is therefore taken from the block each trial was recorded in, and trials are numbered 1–5.
- **Stray sample:** one isolated sample (`time_ms = 370`) between up-stairs trials 4 and 5 was dropped.
- **Verification:** `scripts/extract_features.py` reproduces the original feature spreadsheet to within
  1e-13, and `scripts/serial_logger.py` reproduces the original accuracy summary from `accuracy_run.csv`.
