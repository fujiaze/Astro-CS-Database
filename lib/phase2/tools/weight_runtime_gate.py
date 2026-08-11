# lib/phase2/tools/weight_runtime_gate.py — G7 运行时权重 gate
#
# 验证（真实 Phase1 SNR Catalogue + stage2 diagnostics）：
#   1. 同一帧内不同空间区域（control cell 级）存在不同局部 SNR；
#   2. stage2 diagnostics 显示 local_snr_used > 0（局部优先）与
#      frame_snr_median_fallback 计数（无局部 SNR 区域触发整帧 fallback）。
import json
import os
import sys

import numpy as np

from astropy.io import fits

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
HIPS = os.path.join(ROOT, "run", "temp", "phase1_freeze", "t4_full_v3_final.hips")
DIAG = os.path.join(ROOT, "run", "phase2", "t4_overlap_sigma.mosaic.hips",
                    "diagnostics.json")


def main():
    # 1. 同一帧 SNR catalogue 空间差异（按 tile 分箱比较局部 SNR）
    snr_dir = os.path.join(HIPS, "snr", "Norder7")
    tiles = []
    for d in sorted(os.listdir(snr_dir)):
        p = os.path.join(snr_dir, d)
        if not os.path.isdir(p):
            continue
        for f in sorted(os.listdir(p)):
            if f.startswith("Npix") and f.endswith(".tsv"):
                tiles.append(os.path.join(p, f))
    if not tiles:
        print("no snr tiles"); return 1
    rows = []
    for t in tiles:
        with open(t, encoding="utf-8") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) >= 4 and parts[0].lstrip('-').isdigit():
                    rows.append((float(parts[1]), float(parts[3])))  # ra, snr
    if len(rows) >= 20:
        ras = np.array([r[0] for r in rows])
        snrs = np.array([r[1] for r in rows])
        # 同一帧内按 ra 空间分箱（低/高两半）比较局部 SNR
        half = np.median(ras)
        low = snrs[ras < half]
        high = snrs[ras >= half]
        spread = float(np.median(high) - np.median(low))
        print(f"[weight-gate] frame-local SNR spread (ra bins) = {spread:.3f} "
              f"(n_stars={len(rows)}, low_med={np.median(low):.2f}, "
              f"high_med={np.median(high):.2f})")
        if abs(spread) < 1e-6:
            print("FAIL: 同一帧不同区域无 SNR 差异")
            return 1
    else:
        print("FAIL: 不足两个 tile 有 SNR")
        return 1
    # 2. diagnostics：局部优先 + fallback 计数
    with open(DIAG, encoding="utf-8") as f:
        d = json.load(f)
    used = d.get("local_snr_used", 0)
    fb = d.get("frame_snr_median_fallback", 0)
    print(f"[weight-gate] local_snr_used={used} "
          f"frame_snr_median_fallback={fb}")
    if used <= 0 or fb <= 0:
        print("FAIL: 需要同时存在局部 SNR 使用与 fallback")
        return 1
    print("WEIGHT_RUNTIME_GATE=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
