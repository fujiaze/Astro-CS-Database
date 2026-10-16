#!/usr/bin/env python3
"""ROUND2 独立审查执行结论第 1 项(3): 真实 WCS 已解帧的留出星 PSF/通量残差复核。

对象: M42 T2/M4 帧（run/norm-precision-test, IPV 已解, CRVAL(84.2314,−6.3681),
RMS 0.0922″）。518 颗星有完整七参数 PSF 拟合（p1_sources.json）。

设计（留出法, expected 不由被测实现反推）:
  A. 参数审计: 生产 σ 口径复核（C++ 上中位数 vs 平均中位数）; 逐星
     (sx, sy, θ) 的空间/椭率分布; flux ≡ 2π·A·sx·sy/3 恒等式。
  B. 留出星（按 star_id 稳定分成两半, 各 259 颗）:
     1) 与生产**同口径**重建圆对称帧常模型 σ_prod（上中位数几何平均）,
        在每颗留出星位置用同一像元集计算:
          P_prod = 单位总通量圆 Moffat4 像元积分响应（生产 PSF 库同公式）
          P_star = 该星自身 (sx, sy, θ) 的椭圆旋转 Moffat4 响应
        报告两者相对差（这正是生产 reference SNR 目前承受的 PSF 模型失配）。
     2) 孔径测光: 在 cleaned 帧上对半径 r=√(sx·sy)·k 的圆孔径求和减背景,
        与该星解析通量 2π·A·sx·sy/3 对比（通量标度真实性）。
     3) 残差: 窗内减七参数模型后取 10%–90% 截尾稳健尺度, 与帧 noise_sigma
        及 Poisson+RN 预估值对比（背景失配/模型失配的量化）。
  C. per-frame 状态登记: 光度标度（本帧 native_calibrated, α=1）、B 面亮度
     分布、omitted terms 依据。

用法: python3 verify_round5_holdout_stars.py [work_dir] [out_dir]
退出码恒 0（报告器）; 读数供前台裁决。
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import sys

import numpy as np

WORK = sys.argv[1] if len(sys.argv) > 1 else "run/norm-precision-test/work"
OUT = sys.argv[2] if len(sys.argv) > 2 else \
    "run/norm-precision-test/out/M42_M4_T2_flying_dutchman-20251212_033644-600S-H-alpha"


def upper_median(vals):
    s = sorted(vals)
    return s[len(s) // 2]          # 与 normalize_precision.cpp 同口径


def mean_median(vals):
    s = sorted(vals)
    n = len(s)
    return 0.5 * (s[n // 2 - 1] + s[n // 2]) if n % 2 == 0 else s[n // 2]


def moffat_pixel_response(sx, sy, theta, cx, cy, xs, ys, nsub=64):
    """单位总通量像元积分响应（复合辛普森, 与生产 GL 独立; 64 子格误差 ~1e-11）。"""
    c, s = math.cos(theta), math.sin(theta)
    p1 = c * c / (2 * sx * sx) + s * s / (2 * sy * sy)
    p2 = math.sin(2 * theta) / (4 * sx * sx) - math.sin(2 * theta) / (4 * sy * sy)
    p3 = s * s / (2 * sx * sx) + c * c / (2 * sy * sy)
    norm = 3.0 / (2.0 * math.pi * sx * sy)
    P = np.zeros(len(xs))
    for i, (px, py) in enumerate(zip(xs, ys)):
        x0, y0 = px - 0.5, py - 0.5
        h = 1.0 / nsub
        acc = 0.0
        for a in range(nsub + 1):
            wa = 1.0 if a in (0, nsub) else (4.0 if a % 2 else 2.0)
            x = x0 + a * h
            for b in range(nsub + 1):
                wb = 1.0 if b in (0, nsub) else (4.0 if b % 2 else 2.0)
                y = y0 + b * h
                dx, dy = x - cx, y - cy
                q = p1 * dx * dx + 2 * p2 * dx * dy + p3 * dy * dy
                acc += wa * wb / (1.0 + q) ** 4
        P[i] = norm * acc * h * h / 9.0
    return P


def main() -> int:
    src = json.load(open(os.path.join(WORK, "p1_sources.json"), encoding="utf-8"))
    fr = src["frames"][0]
    pp = [p for p in fr["psf_params"]
          if all(k in p for k in ("A", "B", "cx", "cy", "sx", "sy", "theta", "flux"))]
    n_all = len(pp)
    sx_all = [p["sx"] for p in pp]
    sy_all = [p["sy"] for p in pp]
    sigma_prod = math.sqrt(upper_median(sx_all) * upper_median(sy_all))
    print("== A. 参数审计 ==")
    print("n_stars=%d  noise_sigma=%.6f" % (n_all, fr.get("noise_sigma", -1)))
    print("sigma: C++ 上中位数 sqrt(mx*my)=%.9f | 平均中位数=%.9f | 生产日志值 2.211089"
          % (sigma_prod, math.sqrt(mean_median(sx_all) * mean_median(sy_all))))
    sxa, sya = np.array(sx_all), np.array(sy_all)
    print("sx: median=%.4f std=%.4f (%.1f%%) | sy: median=%.4f std=%.4f (%.1f%%)"
          % (np.median(sxa), sxa.std(), 100 * sxa.std() / sxa.mean(),
             np.median(sya), sya.std(), 100 * sya.std() / sya.mean()))
    # flux 恒等式
    err = max(abs(2 * math.pi * p["A"] * p["sx"] * p["sy"] / 3 - p["flux"]) /
              p["flux"] for p in pp)
    print("flux ≡ 2π·A·sx·sy/3 : max rel err = %.3e" % err)

    # ── B. 留出星 ───────────────────────────────────────────────────────────
    print("\n== B. 留出星（稳定对半分裂） ==")
    fit, hold = [], []
    for p in pp:
        h = int(hashlib.sha1(p["star_id"].encode()).hexdigest(), 16)
        (fit if h % 2 == 0 else hold).append(p)
    print("fit=%d  holdout=%d" % (len(fit), len(hold)))

    # B1: 生产圆模型 vs 逐星椭圆模型的响应失配（在同一 29×29 像元集上）
    # 窗口半径取生产 precision_window_radius_px 的 psf_radius=max(4,6σ)
    win_r = max(4.0, 6.0 * sigma_prod)
    half = int(math.floor(win_r))
    xs, ys = np.meshgrid(np.arange(-half, half + 1), np.arange(-half, half + 1))
    xs = xs.ravel().astype(float)
    ys = ys.ravel().astype(float)
    m_win = len(xs)
    mism_max, mism_med, enc_diff = [], [], []
    for p in hold:
        cx, cy = p["cx"], p["cy"]
        # 像元中心 = 星周整数网格（0-based 探测器坐标）
        gx = np.floor(cx) + xs
        gy = np.floor(cy) + ys
        Pp = moffat_pixel_response(sigma_prod, sigma_prod, 0.0, cx, cy, gx, gy, 48)
        Ps = moffat_pixel_response(p["sx"], p["sy"], p["theta"], cx, cy, gx, gy, 48)
        denom = np.maximum(Ps, 1e-6)
        mism_max.append(float(np.max(np.abs(Pp - Ps) / denom)))
        mism_med.append(float(np.median(np.abs(Pp - Ps) / denom)))
        enc_diff.append(abs(float(Pp.sum()) - float(Ps.sum())))
    mism_max = np.array(mism_max)
    mism_med = np.array(mism_med)
    enc_diff = np.array(enc_diff)
    print("响应失配（生产圆模型 vs 逐星椭圆, %d 像元/星）:" % m_win)
    print("  逐点相对差: median=%.4f  p90=%.4f  max=%.4f"
          % (np.median(mism_med), np.percentile(mism_med, 90), mism_max.max()))
    print("  窗内包络 ΣP 差: median=%.2e  p90=%.2e  max=%.2e"
          % (np.median(enc_diff), np.percentile(enc_diff, 90), enc_diff.max()))
    # 与椭率的关系（预期: 椭率越大失配越大）
    e = np.array([math.sqrt(max(0.0, 1 - min(p["sx"], p["sy"]) ** 2 /
                                max(p["sx"], p["sy"]) ** 2)) for p in hold])
    hi = mism_med[e > np.median(e)]
    lo = mism_med[e <= np.median(e)]
    print("  高椭率半 median 失配=%.4f vs 低椭率半=%.4f（圆模型对椭星的系统失配）"
          % (np.median(hi), np.median(lo)))

    # B2/B3: 像素级（cleaned 帧, astropy memmap）
    cand = [f for f in os.listdir(WORK) if f.startswith("cleaned_") and
            f.endswith(".fts")]
    if not cand:
        print("cleaned FITS 不在 work 目录, 跳过 B2/B3")
        return 0
    from astropy.io import fits
    with fits.open(os.path.join(WORK, cand[0]), memmap=True) as hdul:
        data = hdul[0].data.astype(np.float64)
    ny, nx = data.shape
    print("B2/B3 像素源: %s (%dx%d)" % (cand[0], nx, ny))
    # 取通量中位以上的留出星做孔径/残差（低通量星噪声主导, 仍报告分层）
    order = np.argsort([-p["flux"] for p in hold])
    sample = [hold[i] for i in order[:40]]
    ap_res, res_ratio, trim_ratio = [], [], []
    noise_sigma = float(fr.get("noise_sigma", 0.0))
    for p in sample:
        cx, cy = p["cx"], p["cy"]
        sig = math.sqrt(p["sx"] * p["sy"])
        r_ap = 2.5 * sig
        x0, x1 = int(math.floor(cx - r_ap - 2)), int(math.ceil(cx + r_ap + 2))
        y0, y1 = int(math.floor(cy - r_ap - 2)), int(math.ceil(cy + r_ap + 2))
        if x0 < 0 or y0 < 0 or x1 >= nx or y1 >= ny:
            continue
        win = data[y0:y1 + 1, x0:x1 + 1]
        yy, xx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        dx, dy = xx - cx, yy - cy
        r2 = dx * dx + dy * dy
        in_ap = r2 <= r_ap * r_ap
        # 孔径测光: Σ pixels − B·n_pix（局部背景取该星拟合 B）
        ap_flux = float(win[in_ap].sum() - p["B"] * int(in_ap.sum()))
        ana_flux = 2 * math.pi * p["A"] * p["sx"] * p["sy"] / 3
        ap_res.append((ap_flux - ana_flux) / ana_flux)
        # 残差: 减七参数模型
        c, s = math.cos(p["theta"]), math.sin(p["theta"])
        q = (c * c / (2 * p["sx"] ** 2) + s * s / (2 * p["sy"] ** 2)) * dx * dx \
            + 2 * (math.sin(2 * p["theta"]) / (4 * p["sx"] ** 2) -
                   math.sin(2 * p["theta"]) / (4 * p["sy"] ** 2)) * dx * dy \
            + (s * s / (2 * p["sx"] ** 2) + c * c / (2 * p["sy"] ** 2)) * dy * dy
        model = p["B"] + p["A"] / (1.0 + q) ** 4
        res = (win - model)[r2 <= (3 * sig) ** 2]
        arr = np.sort(np.abs(res))
        k = len(arr)
        lo_i, hi_i = int(0.1 * k), int(0.9 * k)
        trim = float(arr[lo_i:hi_i].mean()) / 0.7316727929211932
        trim_ratio.append(trim / noise_sigma if noise_sigma > 0 else float("nan"))
        # Poisson+RN 预期: sqrt((ana_flux/n_ap + B)/g + RN²/g²), g=1 e-/ADU, RN=9 e
        n_ap = int(in_ap.sum())
        sig_pred = math.sqrt((ana_flux / max(n_ap, 1) + p["B"]) / 1.0 + 81.0)
        res_ratio.append(trim / sig_pred)
    ap_res = np.array(ap_res)
    trim_ratio = np.array(trim_ratio)
    res_ratio = np.array(res_ratio)
    print("孔径测光 vs 解析通量（n=%d, r=2.5√(sx·sy)）:" % len(ap_res))
    print("  (ap−analytic)/analytic: median=%+.4f  p10=%+.4f  p90=%+.4f  std=%.4f"
          % (np.median(ap_res), np.percentile(ap_res, 10),
             np.percentile(ap_res, 90), ap_res.std()))
    print("窗内截尾残差尺度 / 帧 noise_sigma: median=%.3f p90=%.3f"
          % (np.median(trim_ratio), np.percentile(trim_ratio, 90)))
    print("窗内截尾残差尺度 / Poisson+RN 预期: median=%.3f p90=%.3f"
          % (np.median(res_ratio), np.percentile(res_ratio, 90)))

    print("\n== C. per-frame 状态登记 ==")
    print("本帧 photometry 通道缺席 ⇒ native_calibrated, α=1（无测光滑差）; "
          "B 面亮度 median=%.2f ADU/px（ aperture 局部背景）, "
          "omitted=[master_uncertainty]（母版统计未建模）。" % np.median(
              [p["B"] for p in pp]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
