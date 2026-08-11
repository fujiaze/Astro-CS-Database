# lib/phase2/tools/memory_acr_compare.py — R9 machine-readable compare
#
# 比较（同一 overlap 输入、Sigma 算法）：
#   memory: large(24GB) vs medium(2GB) vs tiny(8MB, micro-chunk)
#   acr:    CPU Sigma vs CUDA Sigma
# 输出：signal/support 逐像素 max abs diff + mismatch count + rejected/
# fallback/zero/valid 统计（machine-readable JSON）。
import glob
import json
import os
import sys

import numpy as np
from astropy.io import fits

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.join(ROOT, "run", "phase2", "compare_v3")
RUNS = {
    "large": "run/phase2/t4_overlap_sigma.mosaic.hips",
    "medium": "run/phase2/t4_overlap_sigma_medium.mosaic.hips",
    "tiny": "run/phase2/t4_overlap_sigma_tiny.mosaic.hips",
    "cpu_sigma": "run/phase2/t4_overlap_sigma_cpu.mosaic.hips",
}
STATS = {
    "large": "run/temp/stage2_overlap_sigma_v3b.log",
    "medium": "run/temp/stage2_overlap_sigma_medium.log",
    "tiny": "run/temp/stage2_overlap_sigma_tiny.log",
    "cpu_sigma": "run/temp/stage2_overlap_sigma_cpu.log",
}


def load_hips(path, prod):
    base = os.path.join(ROOT, path, prod, "Norder7")
    out = {}
    for f in glob.glob(os.path.join(base, "Dir*", "Npix*.fits")):
        name = os.path.basename(f)
        out[name] = fits.getdata(f)
    return out


def stats_from_log(log):
    s = {}
    with open(os.path.join(ROOT, log), encoding="utf-8", errors="replace") as f:
        text = f.read()
    import re
    m = re.search(r"pixels=(\d+) rejected=(\d+) fallback=(\d+)"
                  r" \[reject_px=(\d+) fb_px=(\d+) zero_px=(\d+)\]", text)
    if m:
        s = {"pixels": int(m.group(1)), "rejected": int(m.group(2)),
             "fallback": int(m.group(3)), "reject_px": int(m.group(4)),
             "fb_px": int(m.group(5)), "zero_px": int(m.group(6))}
    return s


def compare(a, b, tol_ulp=2):
    names = sorted(set(a) & set(b))
    max_diff = 0.0
    mismatch = 0
    total = 0
    for n in names:
        x = a[n]
        y = b[n]
        m = np.isfinite(x) & np.isfinite(y)
        d = np.abs(x[m] - y[m])
        total += int(m.sum())
        if len(d):
            max_diff = max(max_diff, float(np.max(d)))
            # ULP tolerance：2 ULP
            xa = x[m].astype(np.float32)
            ya = y[m].astype(np.float32)
            ulp = np.abs(xa.view(np.int32) - ya.view(np.int32))
            mismatch += int(np.sum(ulp > tol_ulp))
    return {"compared_pixels": total, "max_abs_diff": max_diff,
            "mismatch_gt_2ulp": mismatch}


def main():
    os.makedirs(OUT, exist_ok=True)
    report = {}
    sig = {k: load_hips(v, "signal") for k, v in RUNS.items()}
    sup = {k: load_hips(v, "support") for k, v in RUNS.items()}
    # memory invariance：large vs medium vs tiny
    report["memory"] = {
        "large_vs_medium": {"signal": compare(sig["large"], sig["medium"]),
                            "support": compare(sup["large"], sup["medium"])},
        "large_vs_tiny": {"signal": compare(sig["large"], sig["tiny"]),
                          "support": compare(sup["large"], sup["tiny"])},
        "medium_vs_tiny": {"signal": compare(sig["medium"], sig["tiny"]),
                           "support": compare(sup["medium"], sup["tiny"])},
    }
    # ACR：CPU Sigma vs CUDA Sigma
    report["acr"] = {
        "cpu_sigma_vs_cuda_sigma": {
            "signal": compare(sig["cpu_sigma"], sig["large"]),
            "support": compare(sup["cpu_sigma"], sup["large"])},
    }
    report["stats"] = {k: stats_from_log(v) for k, v in STATS.items()}
    with open(os.path.join(OUT, "compare_report.json"), "w",
              encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    for k, v in report["memory"].items():
        print(f"[memory {k}] signal {v['signal']}")
        print(f"[memory {k}] support {v['support']}")
    print(f"[acr] cpu_vs_cuda {report['acr']['cpu_sigma_vs_cuda_sigma']}")
    print(f"[stats] {report['stats']}")
    # 门限：memory 与 ACR 均要求 mismatch(>2ulp)=0，support exact
    ok = True
    for k, v in report["memory"].items():
        for p in ("signal", "support"):
            if v[p]["mismatch_gt_2ulp"] != 0:
                ok = False
    a = report["acr"]["cpu_sigma_vs_cuda_sigma"]
    if a["signal"]["mismatch_gt_2ulp"] != 0 or a["support"]["mismatch_gt_2ulp"] != 0:
        ok = False
    # stats 一致性
    st = list(report["stats"].values())
    if any(s != st[0] for s in st[1:]):
        ok = False
    print("MEMORY_ACR_COMPARE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
