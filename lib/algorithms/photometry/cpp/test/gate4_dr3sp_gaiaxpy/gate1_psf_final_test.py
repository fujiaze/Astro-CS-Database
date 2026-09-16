# -*- coding: utf-8 -*-
"""
Phase A (Phase1 Final Closure V3): PSF 坐标契约最终测试

统一坐标契约: 像素索引即中心坐标 (pixel[k] center = k),
合成图像 I(i) = M(i - truth), truth 直接为数组坐标。

硬门 (控制包 02):
  G1 noiseless phase scan  max 质心误差 <= 0.005 px
  G2 seed invariance       spread <= 0.002 px
  G3 Photutils 噪声场      median <= 0.01 px, p95 <= 0.05 px
  G4 椭率同定义            |e_fit - e_truth| median <= 0.005
  G5 fitRadius 5/6/7/8/12  收敛一致
  G6 FP32/FP64             结果一致
"""

import ctypes
import os

import numpy as np

ROOT = r"F:\Astro dev\Astro CS Normalization Database"


def add_dll_dirs():
    for d in (ROOT + r"\lib\star_detector", ROOT + r"\lib\dynamic_psf",
              r"C:\msys64\mingw64\bin", r"C:\msys64\mingw64\lib"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


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


class DPSFFitParams(ctypes.Structure):
    _fields_ = [("fitRadius", ctypes.c_int),
                ("maxIter", ctypes.c_int),
                ("tolerance", ctypes.c_double)]


class DPSFFitResult(ctypes.Structure):
    _fields_ = [("status", ctypes.c_int),
                ("B", ctypes.c_double),
                ("A", ctypes.c_double),
                ("cx", ctypes.c_double),
                ("cy", ctypes.c_double),
                ("sx", ctypes.c_double),
                ("sy", ctypes.c_double),
                ("theta", ctypes.c_double),
                ("fwhm_x", ctypes.c_double),
                ("fwhm_y", ctypes.c_double),
                ("mad", ctypes.c_double),
                ("flux", ctypes.c_double),
                ("eccentricity", ctypes.c_double)]


def synth(cx, cy, n=256, fwhm=3.0, A=1500.0, B=100.0, ell=0.0, theta=0.0):
    """统一契约合成 Moffat4: I(i) = B + A/(1+Q)^4, Q 用 (i - truth)."""
    sx = fwhm / 1.230310 * (1 + ell)
    sy = fwhm / 1.230310 * (1 - ell)
    yy, xx = np.mgrid[0:n, 0:n]
    dx = xx - cx
    dy = yy - cy
    ct, st = np.cos(theta), np.sin(theta)
    u = (dx * ct + dy * st) / sx
    v = (-dx * st + dy * ct) / sy
    q = 0.5 * (u * u + v * v)
    return (B + A / (1 + q) ** 4).astype(np.float32)


def detect(img):
    sdet = ctypes.CDLL(ROOT + r"\lib\star_detector\star_detector.dll")
    sp = SDetParams(structureLayers=5, hotPixelFilterRadius=1,
                    iterativeClipSigma=9.0, iterativeMaxRounds=5,
                    medianFilterDetail=1, maxStars=2000, fitRadius=0,
                    fwhmClipSigma=3.0, maxAxisRatio=2.0)
    sdet.sdet_create.restype = ctypes.c_void_p
    h = sdet.sdet_create(ctypes.byref(sp))
    hh, w = img.shape
    arr = np.ascontiguousarray(np.clip(img, 0, 65535).astype(np.uint16))
    ptr = arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint16))
    x = ctypes.POINTER(ctypes.c_double)(); y = ctypes.POINTER(ctypes.c_double)()
    flux = ctypes.POINTER(ctypes.c_float)(); sat = ctypes.POINTER(ctypes.c_int)()
    mag = ctypes.POINTER(ctypes.c_float)(); has_sat = ctypes.POINTER(ctypes.c_int)()
    count = ctypes.c_int(0)
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
    sdet.sdet_detect_ex(h, ptr, w, hh, ctypes.byref(x), ctypes.byref(y),
                        ctypes.byref(flux), ctypes.byref(sat), ctypes.byref(mag),
                        ctypes.byref(has_sat), ctypes.byref(count), None, 0, None)
    n = count.value
    if n == 0:
        return np.zeros(0), np.zeros(0), np.zeros(0)
    xs = np.ctypeslib.as_array(x, shape=(n,)).copy()
    ys = np.ctypeslib.as_array(y, shape=(n,)).copy()
    fs = np.ctypeslib.as_array(flux, shape=(n,)).copy()
    return xs, ys, fs


def fit(img, cx, cy, fit_radius=8, fp64=False):
    dpsf = ctypes.CDLL(ROOT + r"\lib\dynamic_psf\dynamic_psf.dll")
    fp = DPSFFitParams(fitRadius=fit_radius, maxIter=200, tolerance=1e-8)
    hh, w = img.shape
    cx_arr = np.array([cx], dtype=np.float64)
    cy_arr = np.array([cy], dtype=np.float64)
    cxp = cx_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
    cyp = cy_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
    res_ptr = ctypes.POINTER(DPSFFitResult)()
    if fp64:
        imgd = np.ascontiguousarray(img.astype(np.float64))
        ptrf = imgd.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
        dpsf.dpsf_fit_batch_d.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.c_int,
                                          ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                                          ctypes.POINTER(ctypes.c_double),
                                          ctypes.c_int, ctypes.POINTER(DPSFFitParams),
                                          ctypes.POINTER(ctypes.POINTER(DPSFFitResult))]
        dpsf.dpsf_fit_batch_d.restype = ctypes.c_int
        dpsf.dpsf_fit_batch_d(ptrf, w, hh, cxp, cyp, 1,
                              ctypes.byref(fp), ctypes.byref(res_ptr))
    else:
        imgf = np.ascontiguousarray(img.astype(np.float32))
        ptrf = imgf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        dpsf.dpsf_fit_batch_f.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_int,
                                          ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                                          ctypes.POINTER(ctypes.c_double),
                                          ctypes.c_int, ctypes.POINTER(DPSFFitParams),
                                          ctypes.POINTER(ctypes.POINTER(DPSFFitResult))]
        dpsf.dpsf_fit_batch_f.restype = ctypes.c_int
        dpsf.dpsf_fit_batch_f(ptrf, w, hh, cxp, cyp, 1,
                              ctypes.byref(fp), ctypes.byref(res_ptr))
    r = res_ptr.contents if res_ptr else None
    if r is None or (r.status != 0 and r.status != 3):
        return np.nan, np.nan, np.nan, np.nan
    if not (np.isfinite(r.cx) and np.isfinite(r.cy)):
        return np.nan, np.nan, np.nan, np.nan
    return r.cx, r.cy, r.sx, r.sy


def measure_single(img, truth_x, truth_y, fit_radius=8, fp64=False):
    """生产路径: sdet 检测 -> DPSF 拟合, 返回 DPSF 输出 (统一契约)."""
    xs, ys, _ = detect(img)
    if len(xs) == 0:
        return None
    fx, fy, sx, sy = fit(img, xs[0], ys[0], fit_radius, fp64)
    return fx, fy, sx, sy


def main():
    add_dll_dirs()
    results = {}

    # G1: noiseless phase scan
    errs = []
    for frac in np.arange(0.0, 1.0, 0.05):
        cx = 100.0 + frac
        cy = 120.0 + (frac * 7 % 1.0)
        img = synth(cx, cy)
        m = measure_single(img, cx, cy)
        if m is None:
            errs.append(999.0)
            continue
        errs.append(np.hypot(m[0] - cx, m[1] - cy))
    errs = np.array(errs)
    g1 = float(errs.max()) <= 0.005
    results["G1_phase_scan"] = {"max_err": float(errs.max()),
                                "mean_err": float(errs.mean()), "pass": bool(g1)}
    print(f"G1 phase scan: max={errs.max():.6f}px pass={g1}")

    # G2: seed invariance (不同初始收敛到同一真值)
    import random
    random.seed(42)
    spreads = []
    for _ in range(20):
        cx = 150.0 + random.uniform(0, 1)
        cy = 150.0 + random.uniform(0, 1)
        img = synth(cx, cy)
        xs, ys, _ = detect(img)
        if len(xs) == 0:
            continue
        outs = []
        for dx in (-0.75, -0.5, -0.25, 0.0, 0.25, 0.5, 0.75):
            for dy in (-0.5, 0.0, 0.5):
                fx, fy, _, _ = fit(img, xs[0] + dx, ys[0] + dy)
                if np.isfinite(fx) and np.isfinite(fy):
                    outs.append((fx, fy))
        if len(outs) < 3:
            continue
        spread = np.max(np.hypot(np.array([o[0] for o in outs]) - outs[0][0],
                                 np.array([o[1] for o in outs]) - outs[0][1]))
        spreads.append(spread)
    spreads = np.array(spreads)
    g2 = float(spreads.max()) <= 0.002
    results["G2_seed_invariance"] = {"max_spread": float(spreads.max()), "pass": bool(g2)}
    print(f"G2 seed invariance: max_spread={spreads.max():.6f}px pass={g2}")

    # G3: 噪声场 + Photutils
    from photutils.background import MMMBackground
    from photutils.detection import DAOStarFinder
    from astropy.modeling import models, fitting
    rng = np.random.default_rng(7)
    img = np.full((512, 512), 120.0, dtype=np.float32)
    truths = []
    for _ in range(60):
        cx = rng.uniform(30, 482)
        cy = rng.uniform(30, 482)
        A = rng.uniform(800, 4000)
        img += synth(cx, cy, n=512, A=A, B=120.0) - 120.0
        truths.append((cx, cy))
    img += rng.normal(0, 8.0, img.shape).astype(np.float32)
    xs, ys, fs = detect(img)
    e_all = []
    for k in range(len(xs)):
        fx, fy, _, _ = fit(img, xs[k], ys[k])
        e_all.append((fx, fy))
    e_all = np.array(e_all)
    # 匹配 truth
    errs = []
    for tx, ty in truths:
        d = np.hypot(e_all[:, 0] - tx, e_all[:, 1] - ty)
        if d.size and d.min() < 2.0:
            errs.append(d.min())
    errs = np.array(errs)
    g3a = float(np.median(errs)) <= 0.01 and float(np.percentile(errs, 95)) <= 0.05
    results["G3_astrocs_noisy"] = {"n": int(len(errs)),
                                   "median": float(np.median(errs)),
                                   "p95": float(np.percentile(errs, 95)),
                                   "pass": bool(g3a)}
    print(f"G3 AstroCS 噪声场: n={len(errs)} median={np.median(errs):.4f} "
          f"p95={np.percentile(errs, 95):.4f} pass={g3a}")
    # Photutils Oracle
    bkg = MMMBackground()
    bg = bkg(img)
    _, _, std = __import__("astropy.stats").stats.sigma_clipped_stats(img - bg, sigma=3.0)
    daofind = DAOStarFinder(fwhm=3.0, threshold=5.0 * std)
    srcs = daofind(img - bg)
    perr = []
    if srcs is not None:
        fitter = fitting.LevMarLSQFitter()
        for s in srcs:
            xi, yi = int(s["xcentroid"]), int(s["ycentroid"])
            if xi - 12 < 0 or yi - 12 < 0 or xi + 12 >= 512 or yi + 12 >= 512:
                continue
            patch = img[yi - 12:yi + 13, xi - 12:xi + 13].astype(float)
            yy, xx = np.mgrid[0:25, 0:25]
            g = models.Moffat2D(amplitude=patch.max() - bg, x_0=12, y_0=12,
                                gamma=1.3, alpha=4.0)
            g.alpha.fixed = True  # 固定 beta=4, 与 AstroCS 模型一致
            try:
                gf = fitter(g, xx, yy, patch - bg, maxiter=200)
                px = gf.x_0.value + xi - 12
                py = gf.y_0.value + yi - 12
                d = np.min(np.hypot(px - np.array([t[0] for t in truths]),
                                    py - np.array([t[1] for t in truths])))
                if d < 2.0:
                    perr.append(d)
            except Exception:  # noqa: BLE001
                continue
    perr = np.array(perr)
    g3b = float(np.percentile(perr, 95)) <= 0.05
    results["G3_photutils_oracle"] = {"n": int(len(perr)),
                                      "p95": float(np.percentile(perr, 95)),
                                      "pass": bool(g3b)}
    print(f"G3 Photutils: n={len(perr)} p95={np.percentile(perr, 95):.4f} pass={g3b}")

    # G4: 椭率同定义 (无噪声椭圆星)
    e_diffs = []
    for ell in (0.0, 0.1, 0.2, 0.3):
        for th in (0.0, 0.5, 1.2):
            img = synth(140.0, 140.0, ell=ell, theta=th)
            m = measure_single(img, 140.0, 140.0)
            if m is None:
                continue
            sx, sy = m[2], m[3]
            e_fit = np.sqrt(1.0 - (min(abs(sx), abs(sy)) / max(abs(sx), abs(sy))) ** 2)
            e_truth = np.sqrt(1.0 - ((1 - ell) / (1 + ell)) ** 2)
            e_diffs.append(abs(e_fit - e_truth))
    e_diffs = np.array(e_diffs)
    g4 = float(np.median(e_diffs)) <= 0.005
    results["G4_ellipse_definition"] = {"median_diff": float(np.median(e_diffs)),
                                        "max_diff": float(e_diffs.max()), "pass": bool(g4)}
    print(f"G4 椭率: median_diff={np.median(e_diffs):.5f} pass={g4}")

    # G5: fitRadius 扫描
    fr_errs = []
    for fr in (5, 6, 7, 8, 12):
        img = synth(130.5, 130.5)
        xs, ys, _ = detect(img)
        fx, fy, _, _ = fit(img, xs[0], ys[0], fit_radius=fr)
        fr_errs.append(np.hypot(fx - 130.5, fy - 130.5))
    fr_errs = np.array(fr_errs)
    g5 = float(fr_errs.max()) <= 0.005
    results["G5_fit_radius"] = {"errs": fr_errs.tolist(), "pass": bool(g5)}
    print(f"G5 fitRadius 5/6/7/8/12: {np.round(fr_errs, 6)} pass={g5}")

    # G6: FP32/FP64 一致
    img = synth(150.25, 150.75)
    xs, ys, _ = detect(img)
    f32 = fit(img, xs[0], ys[0], fp64=False)
    f64 = fit(img, xs[0], ys[0], fp64=True)
    g6 = np.hypot(f32[0] - f64[0], f32[1] - f64[1]) <= 0.002
    results["G6_fp32_fp64"] = {"dx": float(f32[0] - f64[0]),
                               "dy": float(f32[1] - f64[1]), "pass": bool(g6)}
    print(f"G6 FP32/FP64: d=({f32[0]-f64[0]:.6f},{f32[1]-f64[1]:.6f}) pass={g6}")

    import json
    out = ROOT + r"\run\temp\reorder_test_v2\gate1_psf_result.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print(f"DONE -> {out}")


if __name__ == "__main__":
    main()
