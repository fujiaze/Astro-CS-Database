#!/usr/bin/env python3
# lib/phase2/tools/satellite_gate_metrics.py — V15 卫星线生产门指标（V15）
#
# 真实生产判定：per-pixel 栈交给生产 rejection_cli（同一 p2_reject_stack_ex
# kernel，plan=auto nominal=20 → linear_fit），不写 Python 镜像判定。
# 马赛克 bias：对比 auto（含 trail）与 clean（无 trail）两组 stage2 输出。
import glob
import json
import os
import random
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from astropy.io import fits

ROOT = Path(__file__).resolve().parents[3]
SAT = ROOT / "run" / "temp" / "satgate"
CLI = os.environ.get(
    "ASTROCS_REJECTION_CLI",
    str(ROOT / "lib" / "phase2" / "build" / "rejection_cli.exe"),
)
PLAN = {"request": "auto", "nominal": 20, "profile": "wbpp_2_9_1",
        "underdetermined_n": 2}
FRAMES = [SAT / f"frame{f:02d}.hips" for f in range(20)]
TRAIL_FRAME = 10
SEED = 20260814


def load_tile(hips, ipix):
    fs = glob.glob(str(hips / "signal" / "Norder7" / "**" /
                       f"Npix{ipix}.fits"), recursive=True)
    if not fs:
        return None
    with fits.open(fs[0]) as h:
        return h[0].data.astype(np.float64)


def kernel_reasons(vals):
    """生产 rejection kernel（rejection_cli --plan），返回 reasons 数组。"""
    fd, pf = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    with open(pf, "w", encoding="utf-8") as f:
        json.dump(PLAN, f)
    try:
        txt = "\n".join(repr(float(v)) for v in vals)
        r = subprocess.run([CLI, "--plan", pf, "--reasons"], input=txt,
                           capture_output=True, text=True, timeout=60,
                           encoding="utf-8", errors="replace")
    finally:
        os.unlink(pf)
    if r.returncode != 0:
        raise RuntimeError("rejection_cli failed: " + r.stderr[:200])
    lines = r.stdout.strip().splitlines()
    return np.array([int(c) for c in lines[1].split()], dtype=int)


def pixel_stack(ipix, col, row, frames):
    """20 帧在某 (col,row) 的 signal 栈（数组下标 [row][col]）。"""
    vals = []
    for hips in frames:
        d = load_tile(hips, ipix)
        if d is None:
            vals.append(float("nan"))
        else:
            vals.append(float(d[row, col]))
    return np.array(vals)


def main():
    mingw = os.environ.get("ASTROCS_MINGW_BIN", r"C:\msys64\mingw64\bin")
    aio = os.environ.get("ASTROCS_AIO_DIR",
                         str(ROOT / "lib" / "astro_image_io"))
    if mingw not in os.environ.get("PATH", ""):
        os.environ["PATH"] = mingw + ";" + aio + ";" + \
            os.environ.get("PATH", "")
    mask = json.load(open(SAT / "trail_mask.json", encoding="utf-8"))
    tiles = {}
    for t, px in mask["tiles"][str(TRAIL_FRAME)].items():
        tiles[int(t)] = np.array(px)
    print("trail tiles:", {k: len(v) for k, v in tiles.items()})

    # ---- 1. trail rejection recall（真实 kernel）----
    n_rejected = 0
    n_trail = 0
    for ip, px in tiles.items():
        for c, r in px:
            stack = pixel_stack(ip, int(c), int(r), FRAMES)
            if not np.isfinite(stack).all():
                continue  # 非全有限位置不计（support 结构一致）
            reasons = kernel_reasons(stack)
            n_trail += 1
            if reasons[TRAIL_FRAME] != 0:
                n_rejected += 1
    recall = n_rejected / max(1, n_trail)
    print(f"[sat] trail pixels={n_trail} rejected={n_rejected} "
          f"recall={recall:.4f}")

    # ---- 2. clean false reject（真实 kernel，非 trail 背景像素）----
    rng = np.random.default_rng(SEED)
    # 取一个 trail tile 的有限像素作样本
    ip0 = next(iter(tiles))
    d0 = load_tile(FRAMES[0], ip0)
    sup = load_tile(SAT / f"frame00.hips", ip0)
    finite = np.isfinite(d0) & (sup > 0)
    clean_cands = np.argwhere(finite)
    trail_set = {tuple(p) for p in tiles[ip0]}
    clean_cands = [p for p in clean_cands if tuple(p) not in trail_set]
    rng.shuffle(clean_cands)
    n_clean = min(1500, len(clean_cands))
    clean_rej = 0
    clean_sample_rej = 0
    clean_sample_total = 0
    for r_, c_ in clean_cands[:n_clean]:
        stack = pixel_stack(ip0, int(c_), int(r_), FRAMES)
        if not np.isfinite(stack).all():
            continue
        reasons = kernel_reasons(stack)
        clean_sample_total += len(stack)
        clean_sample_rej += int(np.sum(reasons != 0))
        # 干净背景像素：任何样本被拒都算 false reject（该样本非 trail）
        if np.any(reasons != 0):
            clean_rej += 1
    print(f"[sat] clean background pixels={n_clean} "
          f"with_any_rejection={clean_rej} "
          f"rate={clean_rej / max(1, n_clean):.4f}")
    print(f"[sat] clean sample-level false reject="
          f"{clean_sample_rej}/{clean_sample_total} "
          f"({clean_sample_rej / max(1, clean_sample_total):.4f})")

    # ---- 3. 马赛克 bias（auto vs clean 输出对比）----
    def load_mosaic(path):
        out = {}
        for f in glob.glob(str(path / "signal" / "Norder7" / "**" /
                                "Npix*.fits"), recursive=True):
            ip = int(Path(f).stem[4:])
            with fits.open(f) as h:
                out[ip] = h[0].data.astype(np.float64)
        return out

    m_auto = load_mosaic(SAT / "mosaic_auto.hips")
    m_clean = load_mosaic(SAT / "mosaic_clean.hips")
    ip_common = sorted(set(m_auto) & set(m_clean))
    all_auto = np.concatenate([m_auto[ip].ravel() for ip in ip_common])
    all_clean = np.concatenate([m_clean[ip].ravel() for ip in ip_common])
    fin = np.isfinite(all_auto) & np.isfinite(all_clean)
    bg = fin & (all_clean > 0.002) & (all_clean < 0.02)  # 背景带
    star = fin & (all_clean > 0.05)                      # 亮星
    bg_diff = all_auto[bg] - all_clean[bg]
    star_rel = (all_auto[star] - all_clean[star]) / np.maximum(
        all_clean[star], 1e-9)
    print(f"[sat] mosaic bg pixels={int(bg.sum())} "
          f"bias_median={np.median(bg_diff):.2e} "
          f"bias_p95={np.percentile(np.abs(bg_diff), 95):.2e}")
    print(f"[sat] mosaic star pixels={int(star.sum())} "
          f"rel_bias_median={np.median(star_rel):.2e} "
          f"rel_bias_p95={np.percentile(np.abs(star_rel), 95):.2e}")

    # ---- 4. diagnostics（production path）----
    diag = json.load(open(SAT / "mosaic_auto.hips" / "diagnostics.json",
                          encoding="utf-8"))
    print("[sat] diagnostics:", json.dumps({
        k: diag.get(k) for k in (
            "reject_method", "reject_profile", "reject_underdetermined_n",
            "rejection_resolved_methods", "underdetermined_pixels",
            "fallback_pixels", "rejected_samples", "integrated_pixels",
            "pixels_depth_1", "pixels_depth_ge_2")}, indent=1))

    metrics = {
        "trail_rejection_recall": recall,
        "trail_pixels": n_trail,
        "trail_rejected": n_rejected,
        "clean_false_reject_rate": clean_rej / max(1, n_clean),
        "clean_sample_false_reject_rate":
            clean_sample_rej / max(1, clean_sample_total),
        "clean_pixels_sampled": n_clean,
        "background_bias_median": float(np.median(bg_diff)),
        "background_bias_p95": float(np.percentile(np.abs(bg_diff), 95)),
        "star_flux_rel_bias_median": float(np.median(star_rel)),
        "star_flux_rel_bias_p95": float(
            np.percentile(np.abs(star_rel), 95)),
        "diagnostics": diag,
    }
    with open(SAT / "satellite_metrics.json", "w", encoding="utf-8") as f:
        json.dump(metrics, f, ensure_ascii=False, indent=2)
    print("metrics written: run/temp/satgate/satellite_metrics.json")


if __name__ == "__main__":
    main()
