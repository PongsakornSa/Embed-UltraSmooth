"""Train the "will someone turn the AC on soon?" model and export it to C.

Input : CSV files written by serial_logger.py (one LOG row per minute from the gateway)
Output: gateway/include/ac_model.h  (weights for the on-device MLP in gateway/src/ai.cpp)

Label for a row t (only rows where the AC is OFF):
    y = 1  if a manual "AC on" press happens within the next HORIZON_MIN minutes
    y = 0  otherwise

Model: MLP 9 -> 8 (ReLU) -> 1 (sigmoid), trained with numpy (no extra libraries),
so every step can be explained in Q&A.

Usage:
    python ai/train_model.py ai/data/*.csv            # train on real logs
    python ai/train_model.py --synthetic              # bootstrap model from simulated data
    python ai/train_model.py --synthetic ai/data/*.csv  # mix both
"""
import argparse
import csv
import glob
import math
import os
import random

import numpy as np

COLUMNS = ["tag", "epoch", "in_temp", "in_rh", "co2", "occupied", "lux", "out_temp",
           "hour_sin", "hour_cos", "slope", "out_pm25", "ac_on", "fan", "window", "presses", "ai_prob"]
FEATURES = ["in_temp", "in_rh", "co2", "occupied", "lux", "out_temp", "hour_sin", "hour_cos", "slope"]
HORIZON_MIN = 10
HIDDEN = 8
THRESHOLD = 0.70  # must match AI_ON_THRESHOLD in gateway/include/config.h

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER_OUT = os.path.join(ROOT, "gateway", "include", "ac_model.h")


# ----------------------------------------------------------------------------- data
def load_logs(paths):
    """Return a list of runs; each run is a list of dict rows in time order."""
    runs = []
    for path in paths:
        rows = []
        with open(path, newline="", encoding="utf-8") as f:
            for rec in csv.reader(f):
                if not rec or rec[0] != "LOG" or len(rec) != len(COLUMNS):
                    continue
                try:
                    row = {k: float(v) for k, v in zip(COLUMNS[1:], rec[1:])}
                except ValueError:
                    continue
                if any(math.isnan(row[k]) for k in FEATURES):
                    continue
                rows.append(row)
        if rows:
            runs.append(rows)
            print(f"loaded {len(rows):5d} rows from {path}")
    return runs


def synthetic_runs(days=21, seed=7):
    """Simulated classroom: weekday classes 08-16, room heats up with people and sun.
    People press the AC button when it feels hot (comfort threshold varies per press)."""
    rng = random.Random(seed)
    runs = []
    for day in range(days):
        weekday = day % 7 < 5
        rows = []
        temp = 27.0 + rng.uniform(-1, 1)
        co2 = 450.0
        out_base = 30 + rng.uniform(-2, 3)
        ac_on = False
        comfort = rng.uniform(28.0, 29.5)  # how hot before someone presses today
        for minute in range(6 * 60, 20 * 60):
            hour = minute / 60
            occupied = weekday and (8 <= hour < 12 or 13 <= hour < 16) and rng.random() > 0.03
            out_temp = out_base + 3 * math.sin(math.pi * (hour - 9) / 12) + rng.gauss(0, 0.2)
            lux = max(0, 600 * math.sin(math.pi * (hour - 6) / 13)) + (300 if occupied else 0) + rng.gauss(0, 20)
            prev = temp
            if ac_on:
                temp += (24.5 - temp) * 0.06
            else:
                temp += (out_temp - temp) * 0.01 + (0.025 if occupied else 0) + lux / 600 * 0.006
            temp += rng.gauss(0, 0.03)
            co2 += (12 if occupied else 0) - (co2 - 420) * (0.03 if ac_on else 0.015) + rng.gauss(0, 3)
            press = 0
            if not ac_on and occupied and temp + rng.gauss(0, 0.3) > comfort and rng.random() < 0.25:
                press, ac_on = 1, True
            if ac_on and not occupied and rng.random() < 0.1:
                ac_on = False  # last person leaves and switches off
            rows.append(dict(
                epoch=day * 86400 + minute * 60, in_temp=temp, in_rh=60 + rng.gauss(0, 2), co2=co2,
                occupied=1.0 if occupied else 0.0, lux=lux, out_temp=out_temp,
                hour_sin=math.sin(2 * math.pi * hour / 24), hour_cos=math.cos(2 * math.pi * hour / 24),
                slope=(temp - prev) * 10, out_pm25=30, ac_on=1.0 if (ac_on and not press) else 0.0,
                fan=0, window=0, presses=press, ai_prob=0))
        # slope = change over the last 10 minutes (same as the gateway)
        for i, r in enumerate(rows):
            j = max(0, i - 10)
            r["slope"] = (r["in_temp"] - rows[j]["in_temp"]) * 10 / max(1, i - j)
        runs.append(rows)
    return runs


def make_dataset(runs):
    X, y = [], []
    for rows in runs:
        presses = [r["presses"] for r in rows]
        for t, r in enumerate(rows):
            if r["ac_on"] >= 0.5:
                continue  # AC already on: nothing to predict
            future = presses[t + 1: t + 1 + HORIZON_MIN]
            if len(future) < HORIZON_MIN:
                continue
            X.append([r[k] for k in FEATURES])
            y.append(1.0 if sum(future) > 0 else 0.0)
    return np.array(X, dtype=np.float32), np.array(y, dtype=np.float32)


# ----------------------------------------------------------------------------- model
def sigmoid(z):
    return 1 / (1 + np.exp(-np.clip(z, -30, 30)))


def train(X, y, epochs=400, lr=0.01, seed=0):
    rng = np.random.default_rng(seed)
    n, d = X.shape
    W1 = rng.normal(0, math.sqrt(2 / d), (HIDDEN, d)).astype(np.float32)
    b1 = np.zeros(HIDDEN, np.float32)
    W2 = rng.normal(0, math.sqrt(1 / HIDDEN), HIDDEN).astype(np.float32)
    b2 = np.float32(0)
    pos_w = (len(y) - y.sum()) / max(1, y.sum())  # rare positives get more weight
    w = np.where(y > 0.5, pos_w, 1.0).astype(np.float32)
    params = [W1, b1, W2, b2]
    m = [np.zeros_like(p) for p in params]
    v = [np.zeros_like(p) for p in params]
    for ep in range(1, epochs + 1):
        idx = rng.permutation(n)
        for start in range(0, n, 256):
            bi = idx[start:start + 256]
            xb, yb, wb = X[bi], y[bi], w[bi]
            h_pre = xb @ W1.T + b1
            h = np.maximum(h_pre, 0)
            p = sigmoid(h @ W2 + b2)
            g_out = (p - yb) * wb / len(bi)           # dLoss/dlogit for weighted BCE
            gW2 = h.T @ g_out
            gb2 = g_out.sum()
            g_h = np.outer(g_out, W2) * (h_pre > 0)
            gW1 = g_h.T @ xb
            gb1 = g_h.sum(0)
            grads = [gW1, gb1, gW2, gb2]
            for i, (prm, grd) in enumerate(zip(params, grads)):  # Adam
                m[i] = 0.9 * m[i] + 0.1 * grd
                v[i] = 0.999 * v[i] + 0.001 * grd * grd
                mh = m[i] / (1 - 0.9 ** ep)
                vh = v[i] / (1 - 0.999 ** ep)
                params[i] = prm - lr * mh / (np.sqrt(vh) + 1e-8)
            W1, b1, W2, b2 = params
        if ep % 100 == 0:
            p = predict(X, W1, b1, W2, b2)
            loss = -np.mean(w * (y * np.log(p + 1e-7) + (1 - y) * np.log(1 - p + 1e-7)))
            print(f"epoch {ep:4d} weighted loss {loss:.4f}")
    return W1, b1, W2, float(b2)


def predict(X, W1, b1, W2, b2):
    return sigmoid(np.maximum(X @ W1.T + b1, 0) @ W2 + b2)


def report(name, y, p, thr):
    pred = p >= thr
    tp = int(np.sum(pred & (y > 0.5)))
    fp = int(np.sum(pred & (y < 0.5)))
    fn = int(np.sum(~pred & (y > 0.5)))
    tn = int(np.sum(~pred & (y < 0.5)))
    prec = tp / max(1, tp + fp)
    rec = tp / max(1, tp + fn)
    f1 = 2 * prec * rec / max(1e-9, prec + rec)
    acc = (tp + tn) / max(1, len(y))
    print(f"[{name}] thr={thr:.2f} acc={acc:.3f} precision={prec:.3f} recall={rec:.3f} F1={f1:.3f} "
          f"(TP={tp} FP={fp} FN={fn} TN={tn})")


# ----------------------------------------------------------------------------- export
def c_array(values):
    return ", ".join(f"{v:.6f}f" for v in values)


def export_header(mean, std, W1, b1, W2, b2, info):
    lines = [
        "// ac_model.h - GENERATED by ai/train_model.py. Do not edit by hand.",
        f"// {info}",
        f"// Features: {', '.join(FEATURES)}",
        "#pragma once",
        "#define AC_MODEL_TRAINED 1",
        f"#define AC_N_FEATURES {len(FEATURES)}",
        f"#define AC_N_HIDDEN {HIDDEN}",
        f"static const float AC_FEAT_MEAN[AC_N_FEATURES] = {{{c_array(mean)}}};",
        f"static const float AC_FEAT_STD[AC_N_FEATURES] = {{{c_array(std)}}};",
        "static const float AC_W1[AC_N_HIDDEN][AC_N_FEATURES] = {",
        *[f"  {{{c_array(row)}}}," for row in W1],
        "};",
        f"static const float AC_B1[AC_N_HIDDEN] = {{{c_array(b1)}}};",
        f"static const float AC_W2[AC_N_HIDDEN] = {{{c_array(W2)}}};",
        f"static const float AC_B2 = {b2:.6f}f;",
        "",
    ]
    with open(HEADER_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"wrote {HEADER_OUT}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="*", help="LOG csv files from serial_logger.py")
    ap.add_argument("--synthetic", action="store_true", help="add simulated classroom data")
    ap.add_argument("--epochs", type=int, default=400)
    args = ap.parse_args()

    paths = [p for pat in args.csv for p in glob.glob(pat)]
    runs = load_logs(paths)
    if args.synthetic:
        runs += synthetic_runs()
        print(f"added {len(runs)} runs incl. synthetic")
    if not runs:
        ap.error("no data: pass CSV files and/or --synthetic")

    # Time-ordered split (no future leakage):
    #  many runs (days)  -> last 25 % of the days are the test set
    #  few long runs     -> first 75 % of each run trains, last 25 % tests
    runs.sort(key=lambda r: r[0]["epoch"])
    if len(runs) >= 4:
        cut = int(len(runs) * 0.75)
        train_runs, test_runs = runs[:cut], runs[cut:]
    else:
        train_runs = [rows[:int(len(rows) * 0.75)] for rows in runs]
        test_runs = [rows[int(len(rows) * 0.75):] for rows in runs]
    Xtr, ytr = make_dataset(train_runs)
    Xte, yte = make_dataset(test_runs)
    print(f"train {len(ytr)} rows ({int(ytr.sum())} positive), test {len(yte)} rows ({int(yte.sum())} positive)")

    mean = Xtr.mean(0)
    std = Xtr.std(0) + 1e-6
    W1, b1, W2, b2 = train((Xtr - mean) / std, ytr, epochs=args.epochs)

    for name, X, y in [("train", Xtr, ytr), ("test", Xte, yte)]:
        p = predict((X - mean) / std, W1, b1, W2, b2)
        for thr in (0.5, THRESHOLD):
            report(name, y, p, thr)

    src = ("synthetic + " if args.synthetic else "") + f"{len(paths)} real log file(s)"
    export_header(mean, std, W1, b1, W2, b2, f"trained on {src}, {len(ytr)} rows, horizon {HORIZON_MIN} min")


if __name__ == "__main__":
    main()
