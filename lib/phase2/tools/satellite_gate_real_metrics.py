#!/usr/bin/env python3
# lib/phase2/tools/satellite_gate_real_metrics.py — V16 真实 16-exposure
# 卫星线门 V2 指标（NON_PRODUCTION_TOOL_ONLY）
#
# 队列：NGC1727 T2 H-alpha 1200s × 16 真实独立 exposure（Phase1 per-exposure
# HiPS, nside 65536, order 7）。第 8 帧受控注入大圆卫星线。
# 四组生产输出：truth(none) / clean(auto) / trail(auto) / trail_none(none)。
# 修复 V15 指标 bug：support 一律读 support/（不再用 signal 代替）。
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
SAT = ROOT / "run" / "temp" / "satgate" / "e2e" / "real16"
CLI = os.environ.get(
    "ASTROCS_REJECTION_CLI",
    str(ROOT / "lib" / "phase2" / "build" / "rejection_cli.exe"),
)
PLAN = {"request": "auto", "nominal": 16, "profile": "wbpp_2_9_1",
        "underdetermined_n": 2,
        "normalization": "astrocs_median_center_v1"}
FRAMES = [SAT / f"frame{n:02d}.hips" for n in range(16)]
TRAIL_FRAME = 8
SEED = 20260814


def load_tile(hips, ipix, product="signal"):
    fs = glob.glob(str(hips / product / "Norder7" / "**" /
                       f"Npix{ipix}.fits"), recursive=True)
    if not fs:
        return None
    with fits.open(fs[0]) as h:
        return h[0].data.astype(np.float64)


def kernel_reasons(vals):
    mingw = os.environ.get("ASTROCS_MINGW_BIN", r"C:\msys64\mingw64\bin")
    if mingw not in os.environ.get("PATH", ""):
        os.environ["PATH"] = mingw + ";" + \
            os.environ.get("ASTROCS_AIO_DIR",
                           str(ROOT / "lib" / "astro_image_io")) + ";" + \
            os.environ.get("PATH", "")
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


def pixel_stack(ipix, col, row):
    vals = []
    for hips in FRAMES:
        d = load_tile(hips, ipix)
        vals.append(float(d[row, col]) if d is not None else float("nan"))
    return np.array(vals)


def load_mosaic(path):
    out = {}
    for f in glob.glob(str(path / "signal" / "Norder7" / "**" /
                            "Npix*.fits"), recursive=True):
        ip = int(Path(f).stem[4:])
        with fits.open(f) as h:
            out[ip] = h[0].data.astype(np.float64)
    return out


def main():
    mask = json.load(open(SAT / "trail_mask.json", encoding="utf-8"))
    tiles = {int(k): np.array(v) for k, v in mask["tiles"].items()}
    print("trail tiles:", {k: len(v) for k, v in tiles.items()})

    # ---- 1. trail rejection recall（真实 kernel，16 帧栈）----
    n_rejected = n_trail = 0
    for ip, px in tiles.items():
        for c, r in px:
            stack = pixel_stack(ip, int(c), int(r))
            if not np.isfinite(stack).all():
                continue
            reasons = kernel_reasons(stack)
            n_trail += 1
            if reasons[TRAIL_FRAME] != 0:
                n_rejected += 1
    recall = n_rejected / max(1, n_trail)
    print(f"[real16] trail pixels={n_trail} rejected={n_rejected} "
          f"recall={recall:.4f}")

    # ---- 2. clean false reject（真实 kernel；support 从 support/ 读取）----
    rng = np.random.default_rng(SEED)
    ip0 = next(iter(tiles))
    d0 = load_tile(FRAMES[0], ip0)
    sup0 = load_tile(FRAMES[0], ip0, "support")   # V16 修复：真正 support
    finite = np.isfinite(d0) & (sup0 > 0)
    trail_set = {tuple(p) for p in tiles[ip0]}
    cands = [p for p in np.argwhere(finite) if tuple(p) not in trail_set]
    rng.shuffle(cands)
    n_clean = min(1200, len(cands))
    clean_rej = clean_samples = clean_total = 0
    for r_, c_ in cands[:n_clean]:
        stack = pixel_stack(ip0, int(c_), int(r_))
        if not np.isfinite(stack).all():
            continue
        reasons = kernel_reasons(stack)
        clean_total += len(stack)
        clean_samples += int(np.sum(reasons != 0))
        if np.any(reasons != 0):
            clean_rej += 1
    print(f"[real16] clean pixels={n_clean} any_reject={clean_rej} "
          f"({clean_rej/max(1,n_clean):.4f}) "
          f"observed_sample_rejection={clean_samples}/{clean_total} "
          f"({clean_samples/max(1,clean_total):.4f})")

    # ---- 3. 马赛克对照（truth / clean / trail / trail_none）----
    mosaics = {n: load_mosaic(SAT / f"mosaic_{n}.hips")
               for n in ("truth", "clean", "trail", "trail_none")}
    ips = sorted(set(mosaics["truth"]) & set(mosaics["clean"]) &
                 set(mosaics["trail"]) & set(mosaics["trail_none"]))
    def vec(name):
        return np.concatenate([mosaics[name][ip].ravel() for ip in ips])
    T = vec("truth"); C = vec("clean"); R = vec("trail"); RN = vec("trail_none")
    fin = np.isfinite(T) & np.isfinite(C) & np.isfinite(R) & np.isfinite(RN)
    Tf, Cf, Rf, RNf = T[fin], C[fin], R[fin], RN[fin]

    # injection 确认：truth vs trail_none（无拒绝基线）——只在 trail mask
    # 像素上比较（整幅 median 被海量未注入像素稀释）。
    inj_diffs = []
    for ip, px in tiles.items():
        if ip not in mosaics["truth"] or ip not in mosaics["trail_none"]:
            continue
        a = mosaics["trail_none"][ip]
        b = mosaics["truth"][ip]
        for c, r in px:
            if np.isfinite(a[r, c]) and np.isfinite(b[r, c]):
                inj_diffs.append(a[r, c] - b[r, c])
    inj_diffs = np.array(inj_diffs)
    print(f"[real16] injection@mask: px={len(inj_diffs)} "
          f"median={np.median(inj_diffs):.4f} "
          f"p95_abs={np.percentile(np.abs(inj_diffs),95):.4f}")

    # rejection 损伤：truth vs clean
    bg = (Tf > 0.002) & (Tf < 0.05)      # 背景带
    star_thr = float(np.percentile(Tf, 99.9))
    star = Tf > max(star_thr, 0.01)  # 亮星（百分位）
    bg_bias = Cf[bg] - Tf[bg]
    star_rel = (Cf[star] - Tf[star]) / np.maximum(Tf[star], 1e-9)
    print(f"[real16] clean_vs_truth: bg px={int(bg.sum())} "
          f"bias_med={np.median(bg_bias):.3e} "
          f"|bias|_p95={np.percentile(np.abs(bg_bias),95):.3e} "
          f"bg_std_ratio={np.std(Cf[bg])/max(np.std(Tf[bg]),1e-12):.4f}")
    print(f"[real16] clean_vs_truth: star px={int(star.sum())} "
          f"rel_bias_med={np.median(star_rel):.4f}")

    # trail 抑制：trail vs truth（trail 帧被拒后输出应与 truth 接近）
    tr_bias = Rf - Tf
    print(f"[real16] trail_vs_truth: bg bias_med={np.median(tr_bias[bg]):.3e} "
          f"|bias|_p95={np.percentile(np.abs(tr_bias[bg]),95):.3e}")

    # ---- 4. diagnostics ----
    diag = json.load(open(SAT / "mosaic_trail.hips" / "diagnostics.json",
                          encoding="utf-8"))
    metrics = {
        "real_frames": 16,
        "dataset": "NGC1727 T2 H-alpha 1200s",
        "trail_frame": TRAIL_FRAME,
        "trail_rejection_recall": recall,
        "trail_pixels": n_trail,
        "trail_rejected": n_rejected,
        "clean_pixels_sampled": n_clean,
        "observed_pixel_any_rejection_rate": clean_rej / max(1, n_clean),
        "observed_sample_rejection_rate": clean_samples / max(1, clean_total),
        "injection_mask_median": float(np.median(inj_diffs)),
        "injection_mask_p95_abs": float(np.percentile(np.abs(inj_diffs), 95)),
        "clean_vs_truth_bg_bias_median": float(np.median(bg_bias)),
        "clean_vs_truth_bg_bias_p95": float(
            np.percentile(np.abs(bg_bias), 95)),
        "clean_vs_truth_bg_std_ratio": float(
            np.std(Cf[bg]) / max(np.std(Tf[bg]), 1e-12)),
        "clean_vs_truth_star_rel_bias_median": float(np.median(star_rel)),
        "trail_vs_truth_bg_bias_median": float(np.median(tr_bias[bg])),
        "trail_vs_truth_bg_bias_p95": float(
            np.percentile(np.abs(tr_bias[bg]), 95)),
        "diagnostics": diag,
    }
    (SAT / "satellite_v2_metrics.json").write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2), encoding="utf-8")
    print("written: run/temp/satgate/e2e/real16/satellite_v2_metrics.json")


if __name__ == "__main__":
    main()
