# lib/phase2/tools/rejection_matrix.py — G6 Rejection synthetic matrix
#
# 控制包 Phase2_Validation §6：
#   样本深度 N = 2,3,4,5,8,10,15,25,50,100,500
#   污染：none/single_high/single_low/symmetric/20%/50%/satellite/cloud
#   统计：TP rejection / FP rejection / final bias / final variance /
#         runtime / fallback count
# Oracle 工具（NON_PRODUCTION_TOOL_ONLY）。
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

CLI = os.environ.get(
    "ASTROCS_REJECTION_CLI",
    str(Path(__file__).resolve().parents[1] / "build" / "rejection_cli.exe"),
)
TIMEOUT_S = 120
SEED = 20260810
TRUTH = 10.0
N_DEPTHS = [2, 3, 4, 5, 8, 10, 15, 25, 50, 100, 500]
CONTAM = [
    "none", "single_high", "single_low", "symmetric",
    "20pct", "50pct", "satellite", "cloud",
]
METHODS = [
    ("none", 0), ("sigma", 1), ("winsorized", 2), ("averaged_sigma", 3),
    ("linear_fit", 4), ("esd", 5), ("rcr", 6), ("percentile", 7),
    ("median_sigma", 8), ("minmax", 9),
]
REPS = 3


def gen_stack(n, kind, rng):
    vals = rng.normal(TRUTH, 1.0, n)
    outlier_mask = np.zeros(n, dtype=bool)
    if kind == "single_high":
        outlier_mask[0] = True
        vals[0] = TRUTH + 6.0
    elif kind == "single_low":
        outlier_mask[0] = True
        vals[0] = TRUTH - 6.0
    elif kind == "symmetric":
        outlier_mask[0] = True
        outlier_mask[1] = True
        vals[0] = TRUTH + 6.0
        vals[1] = TRUTH - 6.0
    elif kind == "20pct":
        k = max(1, int(n * 0.2))
        idx = rng.choice(n, size=k, replace=False)
        outlier_mask[idx] = True
        vals[idx] += rng.choice([-1, 1], size=k) * 6.0
    elif kind == "50pct":
        k = max(1, int(n * 0.5))
        idx = rng.choice(n, size=k, replace=False)
        outlier_mask[idx] = True
        vals[idx] += rng.choice([-1, 1], size=k) * 6.0
    elif kind == "satellite":
        outlier_mask[0:3] = True
        vals[0:3] += 5.0  # 连续正离群（卫星轨迹式）
    elif kind == "cloud":
        m = max(1, int(n * 0.3))
        vals[0:m] += 2.0  # 低频偏置（云/局部 offset）
    return vals, outlier_mask


def run_cli(vals, method, lo=-4.0, hi=3.0, max_iter=8, min_samples=3):
    txt = "\n".join(repr(float(v)) for v in vals)
    r = subprocess.run(
        [CLI, str(method), str(lo), str(hi), str(max_iter), str(min_samples)],
        input=txt, capture_output=True, text=True, timeout=TIMEOUT_S)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    lines = r.stdout.strip().splitlines()
    mask = np.array([int(c) for c in lines[0].split()], dtype=bool)
    meta = {}
    for tok in lines[1].split():
        if "=" in tok:
            k, v = tok.split("=")
            meta[k] = v
    return mask, meta


def main():
    rows = []
    total_t0 = time.time()
    for mname, mcode in METHODS:
        for n in N_DEPTHS:
            for kind in CONTAM:
                tp = fp = bias = var = fallback = 0
                accepted_all = []
                t0 = time.time()
                for rep in range(REPS):
                    rng = np.random.default_rng(SEED * 1000 + n * 100 +
                                                METHODS.index((mname, mcode)) *
                                                100 + len(CONTAM) * 10 + rep)
                    vals, omask = gen_stack(n, kind, rng)
                    mask, meta = run_cli(vals, mcode)
                    # 有效样本 = 调用方 min_samples 检查前所有有限值
                    valid = np.isfinite(vals)
                    acc = mask & valid
                    if meta.get("status") == "1":
                        fallback += 1
                    rej = valid & ~mask
                    # TP：离群被拒（离群数 > 0 时）
                    if omask.any():
                        tp += int(np.sum(rej & omask))
                        tp_den = int(np.sum(omask))
                    else:
                        tp_den = 0
                    fp += int(np.sum(rej & ~omask))
                    kept = vals[acc]
                    if len(kept):
                        bias += float(np.mean(kept) - TRUTH)
                        var += float(np.var(kept))
                        accepted_all.extend(kept.tolist())
                    else:
                        var += 0.0
                runtime_ms = (time.time() - t0) * 1000.0
                n_out = REPS * max(1, tp_den) if tp_den else 0
                rows.append({
                    "method": mname, "n": n, "contamination": kind,
                    "tp_rate": round(tp / n_out, 4) if n_out else None,
                    "fp_rate": round(fp / (REPS * n), 4),
                    "final_bias": round(bias / REPS, 4),
                    "final_variance": round(var / REPS, 4),
                    "runtime_ms": round(runtime_ms, 1),
                    "fallback_count": fallback,
                })
    total_s = time.time() - total_t0
    out_dir = r"F:\Astro dev\Astro CS Normalization Database\run\phase2\matrix"
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "rejection_matrix.csv"), "w",
              encoding="utf-8") as f:
        f.write("method,n,contamination,tp_rate,fp_rate,final_bias,"
                "final_variance,runtime_ms,fallback_count\n")
        for r in rows:
            f.write("{method},{n},{contamination},{tp_rate},{fp_rate},"
                    "{final_bias},{final_variance},{runtime_ms},"
                    "{fallback_count}\n".format(**r))
    summary = {
        "total_rows": len(rows),
        "total_runtime_s": round(total_s, 1),
        "methods": [m for m, _ in METHODS],
        "depths": N_DEPTHS,
        "contaminations": CONTAM,
        "reps": REPS,
        "seed": SEED,
    }
    with open(os.path.join(out_dir, "rejection_matrix_summary.json"), "w",
              encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    print(f"matrix done: {len(rows)} rows in {total_s:.1f}s -> {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
