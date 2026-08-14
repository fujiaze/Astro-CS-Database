#!/usr/bin/env python3
# lib/phase2/tools/controlled_rejection_metrics.py — V17 受控 clean truth 指标
#
# 用途（NON_PRODUCTION_TOOL_ONLY）：
#   在"零 outlier"合成 20 帧（known sky + 8 PSF 星 + faint extended
#   structure + 独立 Gaussian 噪声）上测真实 rejection 质量：
#     - true sample FPR / pixel any-rejection FPR（背景区；production kernel）
#     - Siril 1.4.3 frozen harness 同源 case 对照（同一批 stack 的
#       LinearFit decision；避免 AstroCS 自身过拒而不自知）
#     - production stage2 马赛克：none vs auto 的 star aperture flux /
#       PSF FWHM / faint structure transfer / background noise efficiency
#     - 注入离群 recall：thin satellite / cosmic / hot streak（production
#       kernel 逐像素 stack）
#   stage2 输出与 kernel 测量使用同一 production config 语义：
#     method=auto profile=wbpp_2_9_1 normalization=astrocs_median_center_v1
#     underdetermined_n=2（20 样本 → WBPP 2.9.1 路由 LinearFit）。
#
# 已知近似：kernel 级测量直接用原始 stack（UPM 校准对零梯度合成数据≈恒等，
# stage2 内部校准同样近恒等）；文档中如实标注。
import json
import math
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from astropy.io import fits

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "run" / "temp" / "v17_control_truth"
STAGE2 = os.environ.get(
    "ASTROCS_STAGE2",
    str(ROOT / "lib" / "phase2" / "build" / "astrocs-stage2.exe"))
CLI = os.environ.get(
    "ASTROCS_REJECTION_CLI",
    str(ROOT / "lib" / "phase2" / "build" / "rejection_cli.exe"))
HARNESS = os.environ.get(
    "ASTROCS_SIRIL_LINEARFIT_HARNESS",
    str(ROOT / "run" / "temp" / "p2_v4_evidence" / "siril_harness" /
        "siril_linearfit_oracle.exe"))
PLAN = {"request": "auto", "nominal": 20, "profile": "wbpp_2_9_1",
        "underdetermined_n": 2,
        "normalization": "astrocs_median_center_v1"}
SEED = 20260814
N_FRAMES = 20
FRAMES = [OUT / f"frame{n:02d}.hips" for n in range(N_FRAMES)]
STAR_EXCL_RADIUS = 12.0
STRUCTURE_EXCL_RADIUS_FACTOR = 2.5
BORDER = 16


def ensure_path():
    mingw = os.environ.get("ASTROCS_MINGW_BIN", r"C:\msys64\mingw64\bin")
    aio = os.environ.get("ASTROCS_AIO_DIR",
                         str(ROOT / "lib" / "astro_image_io"))
    os.environ["PATH"] = mingw + ";" + aio + ";" + \
        os.environ.get("PATH", "")


def kernel_reasons(vals):
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
        raise RuntimeError("rejection_cli failed: " + r.stderr[:300])
    lines = r.stdout.strip().splitlines()
    return np.array([int(c) for c in lines[1].split()], dtype=int)


def siril_mask(vals, siglow=5.0, sighigh=3.5):
    txt = "\n".join(repr(float(v)) for v in vals)
    r = subprocess.run([HARNESS, repr(siglow), repr(sighigh)], input=txt,
                       capture_output=True, text=True, timeout=60,
                       encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise RuntimeError("siril harness failed: " + r.stderr[:300])
    return np.array([int(c) for c in r.stdout.splitlines()[0].split()],
                    dtype=int)


def pixel_stack(frames, row, col):
    vals = []
    for hips in frames:
        p = hips / "signal" / "Norder7" / "Dir0" / "Npix0.fits"
        with fits.open(p) as h:
            vals.append(float(h[0].data[row, col]))
    return np.array(vals)


def build_masks(truth_json, truth_img):
    """背景 / 星点 / extended structure 掩码（互斥）。"""
    h, w = truth_img.shape
    yy, xx = np.mgrid[0:h, 0:w]
    bg = np.ones((h, w), dtype=bool)
    star = np.zeros((h, w), dtype=bool)
    struct = np.zeros((h, w), dtype=bool)
    for s in truth_json["stars"]:
        r2 = (xx - s["x"]) ** 2 + (yy - s["y"]) ** 2
        r = max(STAR_EXCL_RADIUS, 2.6 * s["fwhm"])
        star |= r2 <= r * r
    for st in truth_json["structure"]:
        r2 = (xx - st["x"]) ** 2 + (yy - st["y"]) ** 2
        rad = STRUCTURE_EXCL_RADIUS_FACTOR * st["sigma"]
        struct |= r2 <= rad * rad
    bg &= ~star & ~struct
    bg[:BORDER, :] = bg[-BORDER:, :] = False
    bg[:, :BORDER] = bg[:, -BORDER:] = False
    bg &= np.isfinite(truth_img)
    return bg, star, struct


def run_stage2(cfg_name, method, frames, out_name):
    """运行生产 astrocs-stage2；已存在且较新时跳过。"""
    cfg_path = OUT / f"stage2_{cfg_name}.json"
    out_hips = OUT / out_name
    if out_hips.exists():
        return out_hips
    inputs = [f"run/temp/v17_control_truth/{f.relative_to(OUT).as_posix()}"
              for f in frames]
    cfg = {"version": 1, "inputs": {"hips": inputs, "target_order": "auto"},
           "model": {"control_grid_per_tile": 8, "patch_radius_pixels": 2,
                     "min_samples": 5, "snr_search_radius_deg": 0.05,
                     "robust_loss": "huber",
                     "snr_weight_mode": "snr2_normalized",
                     "huber_delta": 1.345, "smoothing": 0.0,
                     "zero_anchor_weight": 0.001, "sigma_floor": 0.001,
                     "support_power": 1.0},
           "integration": {"precision": "fp32", "memory_limit_mb": 4096,
                           "rejection": {"method": method,
                                         "profile": "wbpp_2_9_1",
                                         "normalization":
                                             "astrocs_median_center_v1",
                                         "underdetermined_n": 2},
                           "weight_mode": "auto", "acr_route": "cpu"},
           "output": {"hips": f"run/temp/v17_control_truth/{out_name}"},
           "diagnostics": {"enabled": True}}
    cfg_path.write_text(json.dumps(cfg, indent=2), encoding="utf-8")
    r = subprocess.run([STAGE2, str(cfg_path)], capture_output=True, text=True,
                       timeout=1200, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise RuntimeError(f"stage2 {cfg_name} failed rc={r.returncode}: "
                           + (r.stdout + r.stderr)[-1000:])
    return out_hips


def load_mosaic(path):
    fs = sorted((path / "signal" / "Norder7" / "Dir0").glob("Npix*.fits"))
    with fits.open(fs[0]) as h:
        return h[0].data.astype(np.float64)


def psf_fwhm(img, cx, cy):
    """矩估计 FWHM（背景扣除后强度²加权协方差；确定性、无需拟合）。"""
    r = 10
    slx = slice(max(0, int(cx) - r), min(img.shape[1], int(cx) + r + 1))
    sly = slice(max(0, int(cy) - r), min(img.shape[0], int(cy) + r + 1))
    patch = img[sly, slx]
    if patch.shape[0] < 5 or patch.shape[1] < 5:
        return float("nan")
    bgv = np.median(np.concatenate([patch[0, :], patch[-1, :],
                                    patch[:, 0], patch[:, -1]]))
    w = np.clip(patch - bgv, 0.0, None) ** 2
    s = w.sum()
    if s <= 0:
        return float("nan")
    yy, xx = np.mgrid[0:patch.shape[0], 0:patch.shape[1]]
    mx = float((xx * w).sum() / s)
    my = float((yy * w).sum() / s)
    vx = float(((xx - mx) ** 2 * w).sum() / s)
    vy = float(((yy - my) ** 2 * w).sum() / s)
    return 2.3548 * math.sqrt(max(0.25, (vx + vy) / 2.0))


def main():
    ensure_path()
    truth_json = json.load(open(OUT / "truth.json", encoding="utf-8"))
    truth_img = np.asarray(truth_json["truth"], dtype=np.float64)
    bg, star, struct = build_masks(truth_json, truth_img)
    rng = np.random.default_rng(SEED)

    # ---- 1. true sample FPR / pixel any-rejection（背景，零 outlier）----
    bg_px = np.argwhere(bg)
    rng.shuffle(bg_px)
    bg_px = bg_px[:1500]
    n_sample = 0
    n_rej_sample = 0
    n_pix_any = 0
    siril_agree = 0
    siril_n = 0
    astro_rej_rates = []
    for row, col in bg_px:
        stack = pixel_stack(FRAMES, int(row), int(col))
        if not np.isfinite(stack).all():
            continue
        reasons = kernel_reasons(stack)
        rej = reasons != 0
        n_sample += len(stack)
        n_rej_sample += int(np.sum(rej))
        if np.any(rej):
            n_pix_any += 1
        astro_rej_rates.append(float(np.mean(rej)))
    # Siril 同源对照：取前 400 个背景 stack
    siril_rej = 0
    astro_rej_same = 0
    for row, col in bg_px[:400]:
        stack = pixel_stack(FRAMES, int(row), int(col))
        if not np.isfinite(stack).all():
            continue
        m_siril = siril_mask(stack)
        reasons = kernel_reasons(stack)
        m_astro_acc = ((reasons == 0) | (reasons == 3)).astype(int)
        m_siril_acc = m_siril.astype(int)
        agree = int(np.sum(m_siril_acc == m_astro_acc))
        siril_agree += agree
        siril_n += len(stack)
        siril_rej += int(np.sum(m_siril == 0))
        astro_rej_same += int(np.sum(m_astro_acc == 0))

    sample_fpr = n_rej_sample / max(1, n_sample)
    pixel_fpr = n_pix_any / max(1, len(bg_px))
    siril_agreement = siril_agree / max(1, siril_n)
    siril_same_case_rej_rate = siril_rej / max(1, siril_n)
    astro_same_case_rej_rate = astro_rej_same / max(1, siril_n)

    # ---- 2. production stage2 mosaics: none / auto / satellite ----
    m_none = load_mosaic(run_stage2("truth", "none", FRAMES,
                                    "mosaic_truth.hips"))
    m_auto = load_mosaic(run_stage2("auto", "auto", FRAMES,
                                    "mosaic_auto.hips"))

    # star aperture flux bias（r = 2.4*FWHM）
    star_bias = []
    for s in truth_json["stars"]:
        yy, xx = np.mgrid[0:512, 0:512]
        r2 = (xx - s["x"]) ** 2 + (yy - s["y"]) ** 2
        aper = r2 <= (2.4 * s["fwhm"]) ** 2
        fa = float(np.sum(m_auto[aper]))
        fn = float(np.sum(m_none[aper]))
        if fn > 1e-12:
            star_bias.append((fa - fn) / fn)
    star_flux_rel_bias_median = float(np.median(star_bias)) if star_bias else None

    # PSF FWHM：取最亮 3 颗星
    bright = sorted(truth_json["stars"], key=lambda s: -s["amp"])[:3]
    fwhm_none, fwhm_auto = [], []
    for s in bright:
        fn = psf_fwhm(m_none, s["x"], s["y"])
        fa = psf_fwhm(m_auto, s["x"], s["y"])
        if np.isfinite(fn) and np.isfinite(fa):
            fwhm_none.append(fn)
            fwhm_auto.append(fa)
    psf_fwhm_bias = (np.mean(fwhm_auto) / max(1e-12, np.mean(fwhm_none)) - 1.0) \
        if fwhm_none else None

    # faint extended structure transfer / bg noise efficiency
    struct_mean_none = float(np.mean(m_none[struct]))
    struct_mean_auto = float(np.mean(m_auto[struct]))
    struct_rel_bias = (struct_mean_auto - struct_mean_none) / max(
        1e-12, abs(struct_mean_none))
    bg_std_none = float(np.std(m_none[bg]))
    bg_std_auto = float(np.std(m_auto[bg]))
    bg_noise_eff = bg_std_auto / max(1e-12, bg_std_none)
    bg_mean_abs_bias = float(np.mean(np.abs(m_auto[bg] - m_none[bg])))
    bg_mean_bias = float(np.mean(m_auto[bg] - m_none[bg]))

    # ---- 3. 注入离群 recall（satellite / cosmic / streak）----
    def inject_and_recall(kind, frame_idx, add_fn, seed):
        frames_v = list(FRAMES)
        variant = OUT / "variants" / kind
        marker = variant / ".injected.json"
        if marker.exists():
            injected = [tuple(p) for p in
                        json.load(open(marker, encoding="utf-8"))]
        else:
            variant.mkdir(parents=True, exist_ok=True)
            shutil.copytree(FRAMES[frame_idx],
                            variant / f"frame{frame_idx:02d}.hips")
            vpath = variant / f"frame{frame_idx:02d}.hips"
            sig_path = vpath / "signal" / "Norder7" / "Dir0" / "Npix0.fits"
            with fits.open(sig_path) as h:
                sig = h[0].data.astype(np.float64).copy()
            rng2 = np.random.default_rng(seed)
            injected = add_fn(sig, rng2)
            fits.PrimaryHDU(sig.astype(np.float32)).writeto(
                sig_path, overwrite=True)
            marker.write_text(json.dumps(
                [[int(r), int(c)] for r, c in injected]), encoding="utf-8")
        vpath = variant / f"frame{frame_idx:02d}.hips"
        frames_v[frame_idx] = vpath
        total = 0
        hit = 0
        for row, col in injected:
            stack = pixel_stack(frames_v, int(row), int(col))
            if not np.isfinite(stack).all():
                continue
            reasons = kernel_reasons(stack)
            total += 1
            if reasons[frame_idx] != 0:
                hit += 1
        return hit / max(1, total), total, frames_v

    # thin satellite：对角细线（~1.5px）
    def add_satellite(sig, rng2):
        pts = []
        for t in np.linspace(0.0, 1.0, 700):
            x = 60 + t * 380
            y = 60 + t * 380
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if abs(dx) + abs(dy) <= 1:
                        xi, yi = int(round(x + dx)), int(round(y + dy))
                        if 0 <= xi < 512 and 0 <= yi < 512:
                            sig[yi, xi] += 0.05
                            pts.append((yi, xi))
        return sorted(set(pts))

    # cosmic：24 个 2×2 紧凑斑点
    def add_cosmic(sig, rng2):
        pts = []
        for _ in range(24):
            x0 = int(rng2.integers(40, 460))
            y0 = int(rng2.integers(40, 460))
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                xi, yi = x0 + dx, y0 + dy
                if 0 <= xi < 512 and 0 <= yi < 512:
                    sig[yi, xi] += 0.08
                    pts.append((yi, xi))
        return sorted(set(pts))

    # hot streak：宽 6px 横带
    def add_streak(sig, rng2):
        pts = []
        for y in range(150, 157):
            for x in range(50, 462):
                sig[y, x] += 0.03
                pts.append((y, x))
        return pts

    sat_recall, sat_n, sat_frames = inject_and_recall(
        "satellite", 10, add_satellite, 11)
    cosmic_recall, cosmic_n, _ = inject_and_recall(
        "cosmic", 11, add_cosmic, 12)
    streak_recall, streak_n, _ = inject_and_recall(
        "streak", 12, add_streak, 13)

    # satellite stage2 马赛克（truth/clean+rej/trail+rej 三基线）
    m_trail = load_mosaic(run_stage2("satellite_auto", "auto", sat_frames,
                                     "mosaic_satellite_auto.hips"))
    trail_pts = np.array(sorted(add_satellite(
        np.zeros((512, 512)), np.random.default_rng(0))))
    band = np.zeros((512, 512), dtype=bool)
    for row, col in trail_pts:
        band[row, col] = True
    band_diff = float(np.mean(m_trail[band] - m_auto[band]))
    band_abs = float(np.mean(np.abs(m_trail[band] - m_auto[band])))

    metrics = {
        "controlled_clean_truth": {
            "n_frames": N_FRAMES,
            "noise_sigma": 0.002,
            "sample_fpr": sample_fpr,
            "pixel_any_rejection_fpr": pixel_fpr,
            "background_pixels_sampled": len(bg_px),
            "samples_evaluated": n_sample,
            "samples_rejected": n_rej_sample,
            "background_pixel_reject_rate_p95": float(np.percentile(
                astro_rej_rates, 95)) if astro_rej_rates else None,
            "siril_harness_same_case_agreement": siril_agreement,
            "siril_harness_samples": siril_n,
            "siril_same_case_sample_rej_rate": siril_same_case_rej_rate,
            "astrocs_same_case_sample_rej_rate": astro_same_case_rej_rate,
            "note": ("零 outlier 合成真值；kernel 级测量用原始 stack"
                     "（UPM 零梯度≈恒等）；auto=wbpp_2_9_1→LinearFit"
                     " median_center")
        },
        "science_preservation": {
            "star_flux_rel_bias_median": star_flux_rel_bias_median,
            "psf_fwhm_rel_bias_median": psf_fwhm_bias,
            "faint_structure_rel_bias": struct_rel_bias,
            "faint_structure_none_mean": struct_mean_none,
            "faint_structure_auto_mean": struct_mean_auto,
            "background_noise_std_none": bg_std_none,
            "background_noise_std_auto": bg_std_auto,
            "background_noise_efficiency": bg_noise_eff,
            "background_mean_abs_bias": bg_mean_abs_bias,
            "background_mean_bias": bg_mean_bias
        },
        "injected_outlier_recall": {
            "thin_satellite_recall": sat_recall,
            "thin_satellite_pixels": sat_n,
            "compact_cosmic_recall": cosmic_recall,
            "compact_cosmic_pixels": cosmic_n,
            "hot_streak_recall": streak_recall,
            "hot_streak_pixels": streak_n,
            "satellite_mosaic_band_mean_diff": band_diff,
            "satellite_mosaic_band_mean_abs_diff": band_abs
        },
        "plan": PLAN,
    }
    (OUT / "controlled_rejection_metrics.json").write_text(
        json.dumps(metrics, indent=2), encoding="utf-8")
    print(json.dumps(metrics, indent=2, ensure_ascii=False))
    print("metrics written: run/temp/v17_control_truth/"
          "controlled_rejection_metrics.json")


if __name__ == "__main__":
    main()
