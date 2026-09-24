# -*- coding: utf-8 -*-
"""
Gate 2 (Phase1 Full Freeze v2): PSF/STAR_MEASURE 外部 Oracle 对比

对比: ACSD (star_detector + dynamic_psf DLL) vs Photutils vs 解析真值
指标: 质心 / 通量 / FWHM / 椭率 / 背景

门的两类地位 (M3b-F-01 整改; **不得混用**):
  [验收门 / ACCEPTANCE] 按**生产路径**定义 —— 初值来自 sdet 检测坐标
    (astrocs_production_path), 真值 + 0.5 仅用于把 truth 换算到 DPSF 的
    像素中心约定后逐星配对, 不改变判据对象。
      centroid_p95_le_0.3px_production  — SCI-P1-STAR-001 §1 冻结绝对验收项
        |Δc| <= 0.3 px @SNR>=20 (FROZEN; 阈值不在本脚本内重定义)。
      fwhm_median_le_1pct / ell_median_le_0.005 / flux_median_le_1pct
        也一律取 astrocs_production_path 的统计量。
  [旁证门 / NOT ACCEPTANCE] 仅证明"拟合器本征精度", 不得用于宣称验收:
      centroid_p95_le_0.01px_converged — 以 floor(检测坐标) 作整数初始
        (非生产配置) 的收敛对照, 命名与注释显式标注"非验收"。

用法:
  py -3.12 gate2_psf_oracle.py --out <dir> [--n-stars 120] [--seed 42]
"""

import argparse
import ctypes
import json
import os
import sys

import numpy as np

ROOT = r"F:\Astro dev\Astro CS Normalization Database"


def add_dll_dirs():
    os.add_dll_directory(os.path.join(ROOT, "lib", "star_detector"))
    os.add_dll_directory(os.path.join(ROOT, "lib", "dynamic_psf"))
    os.add_dll_directory(r"C:\msys64\mingw64\bin")
    os.add_dll_directory(r"C:\msys64\mingw64\lib")


def build_synthetic_image(n_stars, width, height, seed, fwhm_px=3.0,
                          flux_range=(500, 50000), bg=120.0, noise=8.0):
    """合成 Moffat4 (beta=4) 星场 (与 dynamic_psf 模型一致, 解析真值).
    模型: I = B + A/(1+Q)^4, Q = 0.5·(u²+v²)/sx² (u/v 为旋转坐标)
    FWHM = 1.230310·sx; flux = 2π·A·sx·sy/3"""
    rng = np.random.default_rng(seed)
    img = np.full((height, width), bg, dtype=np.float32)
    truths = []
    xx = np.arange(width)[None, :]
    yy = np.arange(height)[:, None]
    for i in range(n_stars):
        x = rng.uniform(20, width - 20)
        y = rng.uniform(20, height - 20)
        flux = rng.uniform(*flux_range)
        fwhm = fwhm_px * rng.uniform(0.8, 1.2)
        ell = rng.uniform(0.0, 0.35)
        theta = rng.uniform(0, np.pi)
        sx = fwhm / 1.230310
        sx = sx * (1 + ell)
        sy = fwhm / 1.230310 * (1 - ell)
        A = flux * 3.0 / (2 * np.pi * sx * sy)
        ct = np.cos(theta)
        st = np.sin(theta)
        dx = xx - x
        dy = yy - y
        u = (dx * ct + dy * st) / sx
        v = (-dx * st + dy * ct) / sy
        q = 0.5 * (u ** 2 + v ** 2)
        star = A / (1.0 + q) ** 4
        img += star.astype(np.float32)
        truths.append({"x": x, "y": y, "flux": flux, "fwhm": fwhm,
                       "ell": ell, "theta": theta, "sx": sx, "sy": sy})
    img += rng.normal(0, noise, img.shape).astype(np.float32)
    return img, truths


def astrocs_measure(img, fit_radius=8, floor_init=False):
    """调用 star_detector + dynamic_psf DLL 复现 orchestrator PSF 阶段."""
    sdet = ctypes.CDLL(os.path.join(ROOT, "lib", "star_detector", "star_detector.dll"))
    dpsf = ctypes.CDLL(os.path.join(ROOT, "lib", "dynamic_psf", "dynamic_psf.dll"))
    h, w = img.shape
    params = ctypes.c_int(5)  # structureLayers
    # SDetParams: 7 ints + 4 floats
    class SDetParams(ctypes.Structure):
        _fields_ = [("structureLayers", ctypes.c_int),
                    ("hotPixelFilterRadius", ctypes.c_int),
                    ("iterativeClipSigma", ctypes.c_float),
                    ("iterativeMaxRounds", ctypes.c_int),
                    ("medianFilterDetail", ctypes.c_int),
                    ("maxStars", ctypes.c_int),
                    ("fitRadius", ctypes.c_int),
                    ("fwhmClipSigma", ctypes.c_float),
                    ("maxAxisRatio", ctypes.c_float)]
    sp = SDetParams(structureLayers=5, hotPixelFilterRadius=1,
                    iterativeClipSigma=9.0, iterativeMaxRounds=5,
                    medianFilterDetail=1, maxStars=2000, fitRadius=0,
                    fwhmClipSigma=3.0, maxAxisRatio=2.0)
    sdet.sdet_create.restype = ctypes.c_void_p
    handle = sdet.sdet_create(ctypes.byref(sp))
    x = ctypes.POINTER(ctypes.c_double)()
    y = ctypes.POINTER(ctypes.c_double)()
    flux = ctypes.POINTER(ctypes.c_float)()
    sat = ctypes.POINTER(ctypes.c_int)()
    mag = ctypes.POINTER(ctypes.c_float)()
    has_sat = ctypes.POINTER(ctypes.c_int)()
    count = ctypes.c_int(0)
    img_u16 = np.clip(img, 0, 65535).astype(np.uint16)
    arr = np.ascontiguousarray(img_u16)
    ptr = arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint16))
    sdet.sdet_detect_ex.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint16),
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),
                                    ctypes.POINTER(ctypes.c_int),
                                    ctypes.POINTER(ctypes.c_char_p), ctypes.c_int,
                                    ctypes.POINTER(ctypes.POINTER(ctypes.POINTER(ctypes.c_float)))]
    sdet.sdet_detect_ex.restype = ctypes.c_int
    ret = sdet.sdet_detect_ex(handle, ptr, w, h,
                              ctypes.byref(x), ctypes.byref(y), ctypes.byref(flux),
                              ctypes.byref(sat), ctypes.byref(mag), ctypes.byref(has_sat),
                              ctypes.byref(count), None, 0, None)
    n = count.value
    xs = np.ctypeslib.as_array(x, shape=(n,)).copy()
    ys = np.ctypeslib.as_array(y, shape=(n,)).copy()
    fluxes = np.ctypeslib.as_array(flux, shape=(n,)).copy()
    if floor_init:
        # 收敛性对照: 以整数像素为初始 (LM 收敛良好), 用于证明拟合器本身精度
        xs = np.floor(xs)
        ys = np.floor(ys)
    sdet.sdet_free_detect_ex.argtypes = [ctypes.POINTER(ctypes.c_double),
                                         ctypes.POINTER(ctypes.c_double),
                                         ctypes.POINTER(ctypes.c_float),
                                         ctypes.POINTER(ctypes.c_int),
                                         ctypes.POINTER(ctypes.c_float),
                                         ctypes.POINTER(ctypes.c_int),
                                         ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                         ctypes.c_int]
    sdet.sdet_destroy.argtypes = [ctypes.c_void_p]
    sdet.sdet_free_detect_ex(x, y, flux, sat, mag, has_sat, None, 0)
    sdet.sdet_destroy(handle)

    # PSF 拟合
    class DPSFFitParams(ctypes.Structure):
        _fields_ = [("fitRadius", ctypes.c_int), ("maxIter", ctypes.c_int),
                    ("tolerance", ctypes.c_double)]
    fp = DPSFFitParams(fitRadius=fit_radius, maxIter=100, tolerance=1e-6)
    results = ctypes.POINTER(ctypes.c_double)()
    img_f = np.ascontiguousarray(img)
    ptrf = img_f.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    det = np.column_stack([xs, ys, fluxes, np.zeros(n), np.zeros(n), np.zeros(n)])
    det_c = np.ascontiguousarray(det.astype(np.float64))
    detp = det_c.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
    dpsf.dpsf_fit_batch_f32.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_int,
                                        ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                                        ctypes.c_int, ctypes.POINTER(DPSFFitParams),
                                        ctypes.POINTER(ctypes.c_double),
                                        ctypes.POINTER(ctypes.c_int)]
    n_valid = ctypes.c_int(0)
    out = (ctypes.c_double * (n * 9))()
    ret2 = dpsf.dpsf_fit_batch_f32(ptrf, w, h, detp, n, ctypes.byref(fp), out,
                                   ctypes.byref(n_valid))
    out_arr = np.ctypeslib.as_array(out).reshape(n, 9).copy()
    return out_arr, n_valid.value


def photutils_measure(img, truths):
    from photutils.detection import DAOStarFinder
    from photutils.background import MMMBackground
    from astropy.stats import sigma_clipped_stats
    from astropy.modeling import models, fitting
    bkg = MMMBackground()
    bg = bkg(img)
    mean, _, std = sigma_clipped_stats(img - bg, sigma=3.0)
    daofind = DAOStarFinder(fwhm=3.0, threshold=5.0 * std)
    sources = daofind(img - bg)
    rows = []
    if sources is None:
        return rows
    fitter = fitting.LevMarLSQFitter()
    for s in sources:
        x, y = s["xcentroid"], s["ycentroid"]
        xi, yi = int(x), int(y)
        if xi - 12 < 0 or yi - 12 < 0 or xi + 12 >= img.shape[1] or yi + 12 >= img.shape[0]:
            continue
        patch = img[yi - 12:yi + 13, xi - 12:xi + 13].astype(float)
        yy, xx = np.mgrid[0:25, 0:25]
        g = models.Moffat2D(amplitude=patch.max() - bg, x_0=12, y_0=12,
                            gamma=1.3, alpha=4.0)
        try:
            gfit = fitter(g, xx, yy, patch - bg, maxiter=200)
            gamma = abs(gfit.gamma.value)
            fwhm_x = 2 * gamma * np.sqrt(2 ** (1 / gfit.alpha.value) - 1)
            fwhm_y = fwhm_x
            amp = gfit.amplitude.value
            ell = 0.0
            flux = np.pi * amp * gamma ** 2 / (gfit.alpha.value - 1)
            rows.append({"x": gfit.x_0.value + xi - 12,
                         "y": gfit.y_0.value + yi - 12,
                         "flux": flux, "fwhm": (fwhm_x + fwhm_y) / 2, "ell": ell})
        except Exception:  # noqa: BLE001
            continue
    return rows


def match_truth(meas, truths, tol_px=2.0, offset=0.0):
    matched = []
    used = set()
    for t in truths:
        best, bd = None, tol_px
        for k, m in enumerate(meas):
            if k in used:
                continue
            d = np.hypot(m["x"] - (t["x"] + offset), m["y"] - (t["y"] + offset))
            if d < bd:
                best, bd = k, d
        if best is not None:
            used.add(best)
            matched.append((t, meas[best]))
    return matched


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--n-stars", type=int, default=120)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()
    add_dll_dirs()
    from photutils.background import MMMBackground  # noqa: F401 (import trigger)

    img, truths = build_synthetic_image(args.n_stars, 1024, 1024, args.seed)
    out_arr, n_valid = astrocs_measure(img, floor_init=False)
    # dpsf_fit_batch_f32 每行 9 列: B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y
    # 失败拟合字段为 NaN (无 status 列)
    astro = []
    for row in out_arr:
        A, cx, cy = row[1], row[2], row[3]
        if np.isnan(cx) or np.isnan(cy):
            continue
        sx, sy, fwx, fwy = row[4], row[5], row[7], row[8]
        fwhm = (fwx + fwy) / 2.0
        ell = abs(sx - sy) / max(sx, sy)
        flux = 2 * np.pi * A * abs(sx) * abs(sy) / 3.0
        # 坐标约定: DPSF 输出为像素中心坐标 (index+0.5), 统一到 0-based 像素索引
        # 坐标约定: star_detector/DPSF 输出为像素中心坐标 (array index + 0.5, FITS 约定)
        astro.append({"x": cx, "y": cy, "flux": flux, "fwhm": fwhm, "ell": ell})
    print(f"[gate2] ACSD 检测拟合: {len(astro)} 星 (valid={n_valid})")

    ph = photutils_measure(img, truths)
    print(f"[gate2] Photutils: {len(ph)} 星")

    results = {}
    for name, meas, off in (("astrocs_production_path", astro, 0.5),
                            ("photutils", ph, 0.0)):
        pairs = match_truth(meas, truths, offset=off)
        dx = np.array([m["x"] - (t["x"] + off) for t, m in pairs])
        dy = np.array([m["y"] - (t["y"] + off) for t, m in pairs])
        df = np.array([abs(m["flux"] - t["flux"]) / t["flux"] for t, m in pairs])
        dfwhm = np.array([abs(m["fwhm"] - t["fwhm"]) / t["fwhm"] for t, m in pairs])
        dell = np.array([abs(m.get("ell", 0) - t["ell"]) for t, m in pairs])
        stats = {
            "n_matched": len(pairs),
            "centroid_median_px": float(np.median(np.hypot(dx, dy))) if len(pairs) else None,
            "centroid_p95_px": float(np.percentile(np.hypot(dx, dy), 95)) if len(pairs) else None,
            "flux_median_rel": float(np.median(df)) if len(pairs) else None,
            "flux_p95_rel": float(np.percentile(df, 95)) if len(pairs) else None,
            "fwhm_median_rel": float(np.median(dfwhm)) if len(pairs) else None,
            "ell_median": float(np.median(dell)) if len(pairs) else None,
        }
        results[name] = stats
        print(f"[gate2] {name}: n={len(pairs)} 质心 median={stats['centroid_median_px']:.4f}px "
              f"p95={stats['centroid_p95_px']:.4f} | FWHM rel median={stats['fwhm_median_rel']:.5f} "
              f"| 通量 rel median={stats['flux_median_rel']:.5f} | 椭率 median={stats['ell_median']:.5f}")

    # 对照: 整数初始化 (证明拟合器收敛后精度)
    out_arr2, _ = astrocs_measure(img, floor_init=True)
    astro2 = []
    for row in out_arr2:
        A, cx, cy = row[1], row[2], row[3]
        if np.isnan(cx) or np.isnan(cy):
            continue
        sx, sy, fwx, fwy = row[4], row[5], row[7], row[8]
        fwhm = (fwx + fwy) / 2.0
        ell = abs(sx - sy) / max(sx, sy)
        flux = 2 * np.pi * A * abs(sx) * abs(sy) / 3.0
        astro2.append({"x": cx, "y": cy, "flux": flux, "fwhm": fwhm, "ell": ell})
    pairs = match_truth(astro2, truths, offset=0.5)
    dx = np.array([m["x"] - (t["x"] + 0.5) for t, m in pairs])
    dy = np.array([m["y"] - (t["y"] + 0.5) for t, m in pairs])
    results["astrocs_converged_init"] = {
        "n_matched": len(pairs),
        "centroid_median_px": float(np.median(np.hypot(dx, dy))) if len(pairs) else None,
        "centroid_p95_px": float(np.percentile(np.hypot(dx, dy), 95)) if len(pairs) else None,
    }
    print(f"[gate2] astrocs_converged_init: n={len(pairs)} 质心 median="
          f"{results['astrocs_converged_init']['centroid_median_px']:.4f}px p95="
          f"{results['astrocs_converged_init']['centroid_p95_px']:.4f}")

    # ── 门判定 (M3b-F-01 整改: 验收门 = 生产路径; converged = 旁证) ─────
    # 验收门全部取 astrocs_production_path (sdet 检测坐标为初值的生产配置);
    # converged-init 只作拟合器本征精度旁证, 名称带 _converged_not_acceptance。
    ac = results["astrocs_converged_init"]
    ac_p = results["astrocs_production_path"]
    gates = {
        # [验收门] SCI-P1-STAR-001 §1 冻结绝对质心门 |Δc| <= 0.3px @SNR>=20
        "centroid_p95_le_0.3px_production": (
            ac_p["centroid_p95_px"] is not None
            and ac_p["centroid_p95_px"] <= 0.3),
        "fwhm_median_le_1pct": ac_p["fwhm_median_rel"] is not None and ac_p["fwhm_median_rel"] <= 0.01,
        "ell_median_le_0.005": ac_p["ell_median"] is not None and ac_p["ell_median"] <= 0.005,
        "flux_median_le_1pct": ac_p["flux_median_rel"] is not None and ac_p["flux_median_rel"] <= 0.01,
        "photutils_oracle_centroid_p95_le_0.05px": results["photutils"]["centroid_p95_px"] is not None
        and results["photutils"]["centroid_p95_px"] <= 0.05,
        # [旁证门 / 非验收] 拟合器本征精度 (整数初始化, 非生产配置)
        "centroid_p95_le_0.01px_converged_not_acceptance": (
            ac["centroid_p95_px"] is not None and ac["centroid_p95_px"] <= 0.01),
    }
    convention_note = ("star_detector/DPSF 坐标 = array index + 0.5 (像素中心, FITS 约定); "
                       "Photutils DAOStarFinder = array index 约定. 对比时分别使用对应真值约定. "
                       "gate 分类 (M3b-F-01): *production* 为验收门; "
                       "*converged_not_acceptance* 为拟合器本征精度旁证门, "
                       "**不构成 PSF-001 验收依据**.")
    blocker = ("BLOCKER (PSF-001, 未闭合): 生产路径 (sdet 系初值) 质心偏差仍是本门失败项. "
               "以整数初值的 converged 对照精确收敛, 说明拟合器本身可达 <=0.01px; "
               "因此差异集中在 sdet 检测坐标与 DPSF 输入约定之间的端到端坐标契约 "
               "(M3b-C-01, 属负责人裁决面, 本脚本不单方面定义). "
               "最小复现: Moffat4 星 truth=(315.045,443.485), sdet=(315.545,443.985), "
               "fit(sdet init)=(315.002,443.002), fit(int init)=(315.547,443.985). "
               "在 M3b-C-01 裁决落地前, centroid_p95_le_0.3px_production 保持红灯, "
               "PSF-001 保持未闭合 (禁止以 converged 旁证门宣告验收)。")
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "gate2_result.json"), "w", encoding="utf-8") as f:
        json.dump({"results": results, "gates": gates, "convention_note": convention_note,
                   "blocker": blocker, "model": "Moffat4(beta=4) 合成真值"},
                  f, ensure_ascii=False, indent=2)
    print(f"[gate2] gates: {gates}")
    print(f"[gate2] DONE -> {args.out}")


if __name__ == "__main__":
    main()
