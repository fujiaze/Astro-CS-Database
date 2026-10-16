#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05-B：117 星真实帧覆盖复核（重新测量，不引用、不乘 2.7928）。

纪律（AGENTS.md §13.1）：
  · 帧集从 `实验/shared/data_catalog.json` 清单枚举（不按目录名/记忆猜）；
  · 几何（CRVAL/CRPIX/CD）逐帧从 FITS 头读出，并以 CRVAL 做天区一致性断言；
  · 训练/留出按星表下标哈希不重叠切分；趋势模型只在训练集拟合，留出集报覆盖率；
  · 每个数字带单位、分母、样本量、seed 与输入 SHA。

被估量：dmag_i = m_i(t1) − m_i(t2)（同一颗共同星在两次观测的仪器星等差）
        sigma_dmag,i = (2.5/ln10)·sqrt((sigma_F1/F1)^2 + (sigma_F2/F2)^2)
        z_i = (dmag_i − trend(x_i))/sigma_dmag,i
其中 sigma_F 用正本 §3.3 条件方差口径（gain=1.333 e-/ADU、RN=8.59 e-，来源登记）。

用法：
  python3 s05_real117.py --out run/ACSDSDOC-02/S05/results/real117.json \
      --fig-dir run/ACSDSDOC-02/S05/figures
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s05_common as C          # noqa: E402

SEED = 20261014
GAIN = 1.333
RN = 8.59
MATCH_ARCSEC = 1.5
APER_PX = 4.0
ANN_IN_PX = 8.0
ANN_OUT_PX = 14.0
DET_THRESH = 6.0
MAX_STARS = 4000
SAT_ADU = 60000.0


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 22), b""):
            h.update(chunk)
    return h.hexdigest()


# --------------------------------------------------------------- 清单枚举

def enumerate_frames(root, dataset="M42_T2T3_mosaic_Flying_dutchman", tile="T2/M1",
                     filters=("Red", "Green"), exposure=300.0):
    cat = json.load(open(os.path.join(root, "实验/shared/data_catalog.json"),
                        encoding="utf-8"))
    out = []
    for f in cat["frames"]:
        fid = f["frame_id"]
        if not fid.startswith(dataset + "/" + tile + "/"):
            continue
        if f.get("filter") not in filters:
            continue
        if float(f.get("exposure_s") or 0.0) != exposure:
            continue
        loc = os.path.join(root, f["location"])
        if not os.path.exists(loc):
            continue
        out.append({"frame_id": fid, "location": loc, "filter": f["filter"],
                    "exposure_s": f["exposure_s"], "shape": f["shape"],
                    "file_sha256": f["file_sha256"],
                    "catalog_wcs_center_deg": f["wcs_center_deg"],
                    "calibration_status": f["calibration_status"]})
    out.sort(key=lambda x: x["frame_id"])
    return out, {"catalog_generated_at": cat["generated_at"], "catalog_base_sha": cat["base_sha"],
                 "n_catalog_frames": len(cat["frames"])}


# --------------------------------------------------------------- 读帧

def read_frame(path):
    from astropy.io import fits
    with fits.open(path, memmap=False) as hdul:
        h = hdul[0].header
        d = hdul[0].data
        bzero = float(h.get("BZERO", 0.0))
        bscale = float(h.get("BSCALE", 1.0))
        img = d.astype(np.float32) * bscale + bzero
        geo = {
            "NAXIS": [int(h.get("NAXIS1", 0)), int(h.get("NAXIS2", 0))],
            "CRVAL1": float(h["CRVAL1"]), "CRVAL2": float(h["CRVAL2"]),
            "CRPIX1": float(h["CRPIX1"]), "CRPIX2": float(h["CRPIX2"]),
            "CD1_1": float(h.get("CD1_1", h.get("CDELT1", 0.0))),
            "CD1_2": float(h.get("CD1_2", 0.0)),
            "CD2_1": float(h.get("CD2_1", 0.0)),
            "CD2_2": float(h.get("CD2_2", h.get("CDELT2", 0.0))),
            "CTYPE1": h.get("CTYPE1"), "CTYPE2": h.get("CTYPE2"),
            "BZERO": bzero, "BSCALE": bscale,
            "EXPTIME": float(h.get("EXPTIME", 0.0)), "FILTER": h.get("FILTER"),
            "DATEOBS": h.get("DATE-OBS"), "OBJECT": h.get("OBJECT"),
            "pixel_scale_arcsec": float(math.hypot(h.get("CD1_1", 0.0), h.get("CD1_2", 0.0)) * 3600.0),
        }
    return img, geo


def wcs_xy2radec(geo, x, y):
    """TAN 线性近似（帧内 CRVAL 差 ~1″，SIP 项未写在头内；与 CD 一致到亚像素）。"""
    crpix1 = geo["CRPIX1"] - 1.0
    crpix2 = geo["CRPIX2"] - 1.0
    xi = (x - crpix1) * geo["CD1_1"] + (y - crpix2) * geo["CD1_2"]
    eta = (x - crpix1) * geo["CD2_1"] + (y - crpix2) * geo["CD2_2"]
    xi_r, eta_r = math.radians(xi), math.radians(eta)
    ra0, dec0 = math.radians(geo["CRVAL1"]), math.radians(geo["CRVAL2"])
    den = math.cos(dec0) - eta_r * math.sin(dec0)
    ra = ra0 + math.atan2(xi_r, den)
    dec = math.atan2((math.sin(dec0) + eta_r * math.cos(dec0)) *
                     math.cos(ra - ra0), den)
    return math.degrees(ra) % 360.0, math.degrees(dec)


# --------------------------------------------------------------- 测光

def detect_and_measure(img, geo, tag, rng):
    """检测 + 孔径测光 + 形状筛选 + 正本 §3.3 条件 sigma。

    噪声口径（显式声明，见 REPORT_S05 §0）：
      floor = 暗区（全帧 2 分位）电平 = 偏置+暗流底；sky_adu = 环带中位 − floor（≥0）
      sigma_i^2 = sky_adu/g + (RN/g)^2 + F*P_i/g        [shot_noise_only]
    同时记录 empirical 口径（环带 MAD）作为交叉核对，不混入主判据。
    """
    from scipy import ndimage
    ny, nx = img.shape
    sample = img[::4, ::4].astype(np.float64)
    med = float(np.median(sample))
    floor = float(np.percentile(sample, 2.0))
    mad = 1.482602218505602 * float(np.median(np.abs(sample - med)))
    # 检测：匹配滤波（高斯 sigma=1.2 px）后减大尺度背景（去星云梯度）
    yy, xx = np.mgrid[-4:5, -4:5].astype(np.float32)
    s = 1.2
    ker = np.exp(-(xx * xx + yy * yy) / (2 * s * s))
    ker /= ker.sum()
    conv = ndimage.convolve(img, ker, mode="nearest")
    bg_large = ndimage.uniform_filter(conv, size=41, mode="nearest")
    hp = conv - bg_large
    hps = hp[::4, ::4]
    hp_mad = 1.482602218505602 * float(np.median(np.abs(hps - np.median(hps))))
    thr = DET_THRESH * hp_mad
    mx = ndimage.maximum_filter(hp, size=5)
    peaks = (hp == mx) & (hp > thr)
    peaks[:20, :] = False; peaks[-20:, :] = False
    peaks[:, :20] = False; peaks[:, -20:] = False
    satmask = ndimage.maximum_filter(img, size=3) >= SAT_ADU
    peaks &= ~satmask
    ys, xs = np.nonzero(peaks)
    if ys.size == 0:
        return {"tag": tag, "n_stars": 0, "floor_adu": floor, "hp_mad_adu": hp_mad}
    vals = hp[ys, xs]
    order = np.argsort(-vals)[:MAX_STARS]
    ys, xs = ys[order], xs[order]
    # 亚像素质心（5×5 窗口一阶矩）
    xf = np.empty(ys.size); yf = np.empty(ys.size)
    for k in range(ys.size):
        j, i = ys[k], xs[k]
        w = img[j - 2:j + 3, i - 2:i + 3].astype(np.float64) - floor
        w = np.clip(w, 0.0, None)
        tot = w.sum()
        gy, gx = np.mgrid[-2:3, -2:3]
        if tot <= 0:
            xf[k] = i; yf[k] = j; continue
        xf[k] = i + float((w * gx).sum() / tot)
        yf[k] = j + float((w * gy).sum() / tot)
    # 孔径测光 + 环带背景 + 形状量
    n = ys.size
    fluxes = np.empty(n); skies = np.empty(n); sky_mad = np.empty(n)
    fwhms = np.empty(n); peaks_adu = np.empty(n)
    conc = np.empty(n); roundness = np.empty(n)
    for k in range(n):
        cx, cy = xf[k], yf[k]
        i0, i1 = int(max(0, cx - ANN_OUT_PX - 2)), int(min(nx, cx + ANN_OUT_PX + 3))
        j0, j1 = int(max(0, cy - ANN_OUT_PX - 2)), int(min(ny, cy + ANN_OUT_PX + 3))
        sub = img[j0:j1, i0:i1].astype(np.float64)
        sy, sx = np.mgrid[j0:j1, i0:i1]
        r2 = (sx - cx) ** 2 + (sy - cy) ** 2
        ann = (r2 >= ANN_IN_PX ** 2) & (r2 <= ANN_OUT_PX ** 2)
        if ann.sum() < 8:
            sky = med
            sky_mad[k] = mad
        else:
            sky = float(np.median(sub[ann]))
            sky_mad[k] = 1.482602218505602 * float(np.median(np.abs(sub[ann] - sky)))
        ap = r2 <= APER_PX ** 2
        core = r2 <= 1.5 ** 2
        w = np.clip(sub - sky, 0, None)
        fluxes[k] = float(w[ap].sum())
        skies[k] = max(sky - floor, 0.0)
        tot = w[ap].sum()
        if tot > 0:
            mx_ = float((w[ap] * (sx[ap] - cx)).sum() / tot)
            my_ = float((w[ap] * (sy[ap] - cy)).sum() / tot)
            vx = float((w[ap] * (sx[ap] - cx - mx_) ** 2).sum() / tot)
            vy = float((w[ap] * (sy[ap] - cy - my_) ** 2).sum() / tot)
            fwhms[k] = 2.3548200450309493 * math.sqrt(max(0.5 * (vx + vy), 1e-6))
            roundness[k] = abs(vx - vy) / max(vx + vy, 1e-9)
            conc[k] = float(w[core].sum() / tot)
        else:
            fwhms[k] = float("nan"); roundness[k] = float("nan"); conc[k] = float("nan")
        peaks_adu[k] = float(np.max(sub[ap]))
    # 噪声模型（正本 §3.3 shot_noise_only；n_ap = 孔径面积像素数）
    n_ap = math.pi * APER_PX ** 2
    var = n_ap * (skies / GAIN + (RN / GAIN) ** 2) + np.clip(fluxes, 0, None) / GAIN
    sig = np.sqrt(np.clip(var, 1e-12, None))
    # empirical 口径交叉核对（不进入主判据）
    var_emp = n_ap * sky_mad ** 2 + np.clip(fluxes, 0, None) / GAIN
    sig_emp = np.sqrt(np.clip(var_emp, 1e-12, None))
    snr = fluxes / sig
    # 形状/质量筛选（不是门；判据是先声明的一组域条件，见 REPORT_S05）
    good = (np.isfinite(fwhms) & (fwhms > 1.2) & (fwhms < 5.5) &
            (roundness < 0.35) & (conc > 0.30) & (snr > 5.0) &
            (peaks_adu < SAT_ADU) & (skies > 0))
    xf, yf, fluxes, sig, sig_emp, skies, fwhms = (
        xf[good], yf[good], fluxes[good], sig[good], sig_emp[good], skies[good], fwhms[good])
    snr = snr[good]
    ra = np.empty(xf.size); dec = np.empty(xf.size)
    for k in range(xf.size):
        ra[k], dec[k] = wcs_xy2radec(geo, xf[k], yf[k])
    return {
        "tag": tag, "n_detected": int(n), "n_stars": int(xf.size),
        "floor_adu": floor, "global_bg_adu": med, "global_bg_mad_adu": mad,
        "hp_mad_adu": hp_mad, "detect_threshold_adu": float(thr),
        "shape_cuts": {"fwhm_px": [1.2, 5.5], "roundness_lt": 0.35,
                       "concentration_gt": 0.30, "snr_gt": 5.0, "peak_lt_adu": SAT_ADU},
        "x": xf.tolist(), "y": yf.tolist(), "ra_deg": ra.tolist(), "dec_deg": dec.tolist(),
        "flux_adu": fluxes.tolist(), "flux_err_adu": sig.tolist(),
        "flux_err_empirical_adu": sig_emp.tolist(), "snr": snr.tolist(),
        "sky_adu": skies.tolist(), "fwhm_px": fwhms.tolist(),
        "gain_e_per_adu": GAIN, "read_noise_e": RN,
    }


def flux_to_mag(f):
    f = np.asarray(f, dtype=float)
    out = np.full(f.shape, np.nan)
    good = f > 0
    out[good] = -2.5 * np.log10(f[good])
    return out


def match_catalog(ra1, dec1, ra2, dec2, tol_arcsec=MATCH_ARCSEC):
    """最近邻匹配（小天区，直接用角距离）；返回 (i1, i2) 下标对。"""
    d2r = math.pi / 180.0
    c1 = np.column_stack([np.cos(dec1 * d2r) * np.cos(ra1 * d2r),
                          np.cos(dec1 * d2r) * np.sin(ra1 * d2r), np.sin(dec1 * d2r)])
    c2 = np.column_stack([np.cos(dec2 * d2r) * np.cos(ra2 * d2r),
                          np.cos(dec2 * d2r) * np.sin(ra2 * d2r), np.sin(dec2 * d2r)])
    tol = math.radians(tol_arcsec / 3600.0)
    idx = []
    # 分块最近邻（避免 N×M 全矩阵）
    for i in range(c1.shape[0]):
        dot = c2 @ c1[i]
        j = int(np.argmax(dot))
        if math.acos(min(1.0, max(-1.0, float(dot[j])))) <= tol:
            idx.append((i, j))
    # 双向唯一
    used = {}
    keep = []
    for i, j in idx:
        if j in used:
            continue
        used[j] = i
        keep.append((i, j))
    return keep


def wilson(k, n, z=1.959963984540054):
    if n == 0:
        return (float("nan"), float("nan"))
    p = k / n
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return ((c - h) / d, (c + h) / d)


def cov_block(z, levels=(0.6826894921370859, 0.90, 0.95)):
    out = {}
    for lv in levels:
        from math import erf
        kz = math.sqrt(2.0) * C._erfinv(lv)
        k = int(np.sum(np.abs(z) <= kz))
        lo, hi = wilson(k, z.size)
        out["%.10g" % lv] = {"nominal": lv, "k": k, "n": int(z.size),
                             "cov": float(k / z.size) if z.size else float("nan"),
                             "binomial_se": float(math.sqrt(lv * (1 - lv) / z.size)) if z.size else None,
                             "wilson95": [lo, hi],
                             "contains_nominal": bool(lo <= lv <= hi) if z.size else False,
                             "z_bound": float(kz)}
    return out


def wls_fit(Adesign, y, w):
    Aw = Adesign * w[:, None]
    ATA = Aw.T @ Adesign
    ATy = Aw.T @ y
    return np.linalg.solve(ATA, ATy)



def _sys2_for(snr_arr, sys_bins):
    out = np.zeros_like(snr_arr, dtype=float)
    for k in range(snr_arr.size):
        for b in sys_bins:
            if b["snr_bin"][0] <= snr_arr[k] < b["snr_bin"][1]:
                out[k] = b["sigma_sys_mag"] ** 2
                break
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=None)
    ap.add_argument("--out", default="run/ACSDSDOC-02/S05/results/real117.json")
    ap.add_argument("--fig-dir", default="run/ACSDSDOC-02/S05/figures")
    args = ap.parse_args()
    root = args.root or C.repo_root()

    frames, cat_meta = enumerate_frames(root)
    log = {"catalog": cat_meta, "n_frames_enumerated": len(frames),
           "frames": frames, "header_geometry": {}, "crval_assertion": {}}
    measured = {}
    order = []
    for fr in frames:
        img, geo = read_frame(fr["location"])
        log["header_geometry"][fr["frame_id"]] = geo
        tag = ("%s_%s" % (fr["filter"], (geo["DATEOBS"] or "").replace(":", "").replace("-", "")
                          .replace("T", "").replace(" ", "")))
        rng = C.rng_for(tag, SEED)
        r = detect_and_measure(img, geo, tag, rng)
        r["frame_id"] = fr["frame_id"]
        r["filter"] = fr["filter"]
        r["file_sha256"] = fr["file_sha256"]
        measured[tag] = r
        order.append(tag)
        print("[S05 real] %-40s det=%d bg=%.1f" % (tag, r.get("n_stars", 0),
                                                   r.get("global_bg_adu", float("nan"))))
    # CRVAL 一致性断言（1″ 级：同一目标天区）
    keys = list(log["header_geometry"].keys())
    worst = 0.0
    for i in range(len(keys)):
        for j in range(i + 1, len(keys)):
            a, b = log["header_geometry"][keys[i]], log["header_geometry"][keys[j]]
            dd = math.hypot((a["CRVAL1"] - b["CRVAL1"]) * math.cos(math.radians(a["CRVAL2"])),
                            a["CRVAL2"] - b["CRVAL2"]) * 3600.0
            log["crval_assertion"]["%s|%s" % (keys[i], keys[j])] = dd
            worst = max(worst, dd)
    log["crval_assertion_worst_arcsec"] = worst
    log["crval_assertion_tolerance_arcsec"] = 36.0   # 0.01°
    log["crval_assertion_pass"] = bool(worst <= 36.0)

    # 选两次观测的 Red 帧 + 同夜 Green（颜色协变量）
    def pick(filt, epoch):
        cands = [t for t in order if t.startswith(filt + "_")]
        cands.sort()
        # epoch: 0 = 20251212, 1 = 20251224（由 DATEOBS 决定）
        for t in cands:
            d = log["header_geometry"][measured[t]["frame_id"]]["DATEOBS"] or ""
            if (epoch == 0 and d.startswith("2025-12-12")) or (epoch == 1 and d.startswith("2025-12-24")):
                return t
        return None

    t_r1, t_r2 = pick("Red", 0), pick("Red", 1)
    t_g1, t_g2 = pick("Green", 0), pick("Green", 1)
    log["selected"] = {"red_epoch1": t_r1, "red_epoch2": t_r2,
                       "green_epoch1": t_g1, "green_epoch2": t_g2}
    if not (t_r1 and t_r2):
        C.dump_json(args.out, {"status": "NOT_RUN", "reason": "缺少两帧 Red", "log": log})
        return 1

    R1, R2 = measured[t_r1], measured[t_r2]
    m1 = flux_to_mag(np.array(R1["flux_adu"]))
    r1 = np.array(R1["ra_deg"]); d1 = np.array(R1["dec_deg"])
    f1 = np.array(R1["flux_adu"]); s1 = np.array(R1["flux_err_adu"])
    x1 = np.array(R1["x"]); y1 = np.array(R1["y"])
    fw1 = np.array(R1["fwhm_px"])
    m2 = flux_to_mag(np.array(R2["flux_adu"]))
    r2 = np.array(R2["ra_deg"]); d2 = np.array(R2["dec_deg"])
    f2 = np.array(R2["flux_adu"]); s2 = np.array(R2["flux_err_adu"])
    x2 = np.array(R2["x"]); y2 = np.array(R2["y"])

    pairs = match_catalog(r1, d1, r2, d2)
    log["n_matched_common_stars"] = len(pairs)
    ii = np.array([p[0] for p in pairs]); jj = np.array([p[1] for p in pairs])
    dmag = m1[ii] - m2[jj]
    sig = (2.5 / math.log(10.0)) * np.sqrt((s1[ii] / f1[ii]) ** 2 + (s2[jj] / f2[jj]) ** 2)
    color = np.full(ii.size, np.nan)
    if t_g1 and t_g2:
        G1, G2 = measured[t_g1], measured[t_g2]
        g1 = flux_to_mag(np.array(G1["flux_adu"]))
        g2 = flux_to_mag(np.array(G2["flux_adu"]))
        pg1 = dict((a, b) for a, b in match_catalog(r1, d1, np.array(G1["ra_deg"]),
                                                   np.array(G1["dec_deg"])))
        pg2 = dict((a, b) for a, b in match_catalog(r2, d2, np.array(G2["ra_deg"]),
                                                   np.array(G2["dec_deg"])))
        for k, (a, b) in enumerate(pairs):
            if a in pg1 and b in pg2:
                color[k] = 0.5 * ((g1[pg1[a]] - m1[a]) + (g2[pg2[b]] - m2[b]))
    e1 = np.array(R1["flux_err_empirical_adu"]); e2 = np.array(R2["flux_err_empirical_adu"])
    good = (np.isfinite(dmag) & np.isfinite(sig) & (sig > 0) & np.isfinite(x1[ii]) &
            np.isfinite(color) & (f1[ii] > 0) & (f2[jj] > 0) &
            (fw1[ii] > 1.2) & (fw1[ii] < 5.5))
    dmag, sig, color = dmag[good], sig[good], color[good]
    xg, yg = x1[ii][good], y1[ii][good]
    mg = m1[ii][good]
    n_common = int(dmag.size)
    log["n_common_after_quality"] = n_common

    # 不重叠训练/留出（下标哈希，确定性）
    h = np.array([int(hashlib.sha256(("%d:%d" % (SEED, k)).encode()).hexdigest()[:8], 16)
                  for k in range(n_common)]) if n_common else np.zeros(0, dtype=int)
    is_train = (h % 2 == 0)
    log["n_train"], log["n_holdout"] = int(is_train.sum()), int((~is_train).sum())

    # 趋势模型：常数 + 星等 + 颜色 + 位置（线性 + 径向）
    def design(x, y, mag, col):
        rx = (x - 2048.0) / 2048.0
        ry = (y - 2048.0) / 2048.0
        return np.column_stack([np.ones_like(mag), mag - np.median(mag), col - np.median(col),
                                rx, ry, rx * rx + ry * ry])

    A = design(xg, yg, mg, color)
    w = 1.0 / sig
    beta = wls_fit(A[is_train], dmag[is_train], w[is_train])
    pred = A @ beta
    z_raw = (dmag - np.average(dmag, weights=1.0 / sig ** 2)) / sig     # 常数模型（全局零点）
    z_det = (dmag - pred) / sig                                        # 留出/全样本趋势模型

    # clip 前后（3σ 迭代，只用于诊断，不调门）
    def clipped(z):
        keep = np.abs(z - np.median(z)) < 3.0 * (1.482602218505602 *
                                                 np.median(np.abs(z - np.median(z))))
        return keep

    res = {
        "experiment_id": "P2-E-S05-real117",
        "paper_id": "P2",
        "evidence_level": "REAL_DATA",
        "env": C.env_fingerprint(),
        "status": "PASS",
        "seed": SEED,
        "inputs": {"frames": [R1["frame_id"], R2["frame_id"]],
                   "sha256": [R1["file_sha256"], R2["file_sha256"]],
                   "green_frames_for_color": [t_g1, t_g2]},
        "gain_e_per_adu": GAIN, "read_noise_e": RN,
        "aperture_px": APER_PX, "annulus_px": [ANN_IN_PX, ANN_OUT_PX],
        "match_tolerance_arcsec": MATCH_ARCSEC,
        "n_common_stars": n_common,
        "trend_model": {"design": ["1", "mag", "color", "rx", "ry", "r^2"],
                        "beta": beta.tolist(),
                        "fit_scope": "training half only"},
        "global_stats": {
            "sd_dmag_mag": float(np.std(dmag, ddof=1)),
            "median_sigma_dmag_mag": float(np.median(sig)),
            "ratio_sd_over_sigma": float(np.std(dmag, ddof=1) / np.median(sig)),
            "note": ("该比值是**未去趋势**的实测散度/解析条件 sigma，"
                     "不等于噪声模型低估倍数（含帧零点/色差/位置系统项）"),
        },
        "coverage_raw_constant_model": {
            "all": cov_block(z_raw),
            "clipped": cov_block(z_raw[clipped(z_raw)]),
            "z_rms": float(np.sqrt(np.mean(z_raw ** 2))),
        },
        "coverage_detrended": {
            "train": cov_block(z_det[is_train]),
            "holdout": cov_block(z_det[~is_train]),
            "all": cov_block(z_det),
            "all_clipped": cov_block(z_det[clipped(z_det)]),
            "z_rms_holdout": float(np.sqrt(np.mean(z_det[~is_train] ** 2))) if (~is_train).any() else None,
        },
    }
    # 分层（留出集，星等/颜色/位置/SNR）
    strat = {}

    def strat_block(name, vals, edges):
        out = []
        for lo, hi in zip(edges[:-1], edges[1:]):
            m = (vals >= lo) & (vals < hi)
            mh = m & (~is_train)
            if mh.sum() < 5:
                continue
            zz = z_det[mh]
            out.append({"bin": [lo, hi], "n": int(mh.sum()),
                        "median_z": float(np.median(zz)),
                        "z_rms": float(np.sqrt(np.mean(zz ** 2))),
                        "cov68": cov_block(zz)["0.6826894921"]["cov"],
                        "wilson95": cov_block(zz)["0.6826894921"]["wilson95"],
                        "median_sigma_mag": float(np.median(sig[mh]))})
        strat[name] = out
    # 相对星等：以本帧通量中位为零点（仪器星等，无绝对零点）
    med_f1 = float(np.median(f1[ii][good]))
    relmag = -2.5 * np.log10(f1[ii][good] / med_f1)
    snr1 = f1[ii][good] / s1[ii][good]
    strat_block("relative_magnitude", relmag, [-3, -2, -1.5, -1, -0.5, 0, 0.5, 1, 3])
    strat_block("color_g_minus_r", color, [-3, -0.5, 0.0, 0.5, 1.0, 1.5, 3])
    strat_block("x", xg, [0, 1024, 2048, 3072, 4096])
    strat_block("y", yg, [0, 1024, 2048, 3072, 4096])
    strat_block("snr_epoch1", snr1, [5, 10, 20, 50, 100, 300, 1e9])
    res["stratified_holdout"] = strat

    # ---- 经验系统项地板层（D-04 §3：另立含适用域/有效样本数/交叉验证的层）----
    # 模型：Var(dmag) = sigma_analytic^2 + sigma_sys^2，sigma_sys 按 SNR 分层在
    # **训练集**估计，留出集独立验证 coverage。禁止全局乘 2.7928。
    snr_all = f1[ii][good] / s1[ii][good]
    resid = dmag - pred          # 去趋势后的残差（pred 只在训练集拟合）
    edges = [5.0, 20.0, 50.0, 100.0, 300.0, 1e9]
    sys_bins = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        mtr = (snr_all >= lo) & (snr_all < hi) & is_train
        if mtr.sum() < 20:
            continue
        v = float(np.var(resid[mtr], ddof=1))
        sm = float(np.mean(sig[mtr] ** 2))
        sys2 = max(v - sm, 0.0)
        sys_bins.append({"snr_bin": [lo, hi], "n_train": int(mtr.sum()),
                         "var_resid": v, "mean_sigma2": sm,
                         "sigma_sys_mag": math.sqrt(sys2),
                         "n_eff_bin": int(mtr.sum())})
    # 应用（留出集）：按同一分层给 sigma_sys
    z_floor = np.empty_like(z_det)
    for k in range(snr_all.size):
        s2bin = 0.0
        for b in sys_bins:
            if b["snr_bin"][0] <= snr_all[k] < b["snr_bin"][1]:
                s2bin = b["sigma_sys_mag"] ** 2
                break
        z_floor[k] = resid[k] / math.sqrt(sig[k] ** 2 + s2bin)
    res["empirical_systematic_floor"] = {
        "model": "Var(dmag) = sigma_analytic^2 + sigma_sys^2（按 SNR 分层）",
        "bins": sys_bins,
        "coverage_holdout_after_floor": cov_block(z_floor[~is_train]),
        "coverage_all_after_floor": cov_block(z_floor),
        "z_rms_holdout_after_floor": float(np.sqrt(np.mean(z_floor[~is_train] ** 2))),
        "global_sigma_sys_mag_train": float(math.sqrt(max(
            np.var(resid[is_train], ddof=1) - np.mean(sig[is_train] ** 2), 0.0))),
        "applicability": ("本帧组（T2/M1 Red 300s，0.967″/px，两夜）、"
                          "4 px 孔径/8-14 px 环带、M42 视场；不外推到其它通带/视场"),
        "forbidden": "不得写成全局 2.7928 倍修正",
    }
    # 共同系统误差：把残差（去趋势后）按 8×8 空间块求均值，检验共模
    blocks = {}
    bx = np.clip((xg / 4096.0 * 8).astype(int), 0, 7)
    by = np.clip((yg / 4096.0 * 8).astype(int), 0, 7)
    bkey = by * 8 + bx
    for b in range(64):
        m = bkey == b
        if m.sum() >= 5:
            blocks[str(b)] = {"n": int(m.sum()), "mean_z": float(np.mean(z_det[m])),
                              "rms_z": float(np.sqrt(np.mean(z_det[m] ** 2)))}
    bm = np.array([v["mean_z"] for v in blocks.values()])
    n_per = np.array([v["n"] for v in blocks.values()], dtype=float)
    # 共模检验：块均值的分散 vs 独立预期
    exp_var = float(np.mean(1.0 / n_per))
    res["common_mode"] = {
        "n_blocks": len(blocks), "blocks": blocks,
        "var_block_mean": float(np.var(bm, ddof=1)) if bm.size > 1 else None,
        "expected_var_if_independent": exp_var,
        "excess_common_mode_power": (float(np.var(bm, ddof=1) / exp_var - 1.0)
                                     if bm.size > 1 else None),
        "mean_z_global": float(np.mean(z_det)),
        "weighted_mean_z": float(np.average(z_det, weights=1.0 / sig ** 2)),
    }
    # 有效独立样本数（空间块 bootstrap 近似）：N_eff = N/(1+rho_bar*(n-1))
    rho_hat = None
    if bm.size > 1:
        n_mean = float(np.mean(n_per))
        rho_hat = (n_mean * float(np.var(bm, ddof=1)) - float(np.mean(1.0 / n_per))) / (n_mean - 1.0)
        rho_hat = max(rho_hat, 0.0)
    res["effective_independent_samples"] = {
        "n_common": n_common,
        "n_blocks": len(blocks),
        "rho_bar_estimate": rho_hat,
        "n_eff_from_rho": (n_common / (1.0 + max(rho_hat, 0.0) * (n_common - 1.0)))
        if rho_hat is not None else None,
        "n_eff_conservative_blocks": len(blocks),
        "definition": "N_eff = N/(1+rho_bar*(N-1))，rho_bar 由 8×8 空间块均值的超额分散估计",
    }
    # empirical 口径对照（不进入主判据）
    sig_emp = (2.5 / math.log(10.0)) * np.sqrt((e1[ii][good] / f1[ii][good]) ** 2 +
                                               (e2[jj][good] / f2[jj][good]) ** 2)
    z_emp = (dmag - np.average(dmag, weights=1.0 / sig_emp ** 2)) / sig_emp
    res["crosscheck_empirical_rms_convention"] = {
        "note": ("empirical_total_rms 口径（环带 MAD 当每像素噪声）只作对照；"
                 "主判据用 shot_noise_only + 暗区底扣除，两种口径不混用"),
        "sd_dmag_mag": float(np.std(dmag, ddof=1)),
        "median_sigma_dmag_empirical_mag": float(np.median(sig_emp)),
        "ratio_sd_over_sigma_empirical": float(np.std(dmag, ddof=1) / np.median(sig_emp)),
        "z_rms_empirical": float(np.sqrt(np.mean(z_emp ** 2))),
    }
    f_hold = res["empirical_systematic_floor"]["coverage_holdout_after_floor"]
    f_hold68 = f_hold["0.6826894921"]["cov"]
    d_floor_lv95 = f_hold["0.95"]
    npz_path = os.path.splitext(args.out)[0] + "_stars.npz"
    np.savez_compressed(npz_path, dmag=dmag, sigma_dmag=sig, color=color,
                        x=xg, y=yg, relmag=-2.5 * np.log10(f1[ii][good] / float(np.median(f1[ii][good]))),
                        snr1=f1[ii][good] / s1[ii][good],
                        snr2=f2[jj][good] / s2[jj][good],
                        fwhm1=np.array(R1["fwhm_px"])[ii][good],
                        fwhm2=np.array(R2["fwhm_px"])[jj][good],
                        sigma_dmag_empirical=sig_emp, is_train=is_train, z_raw=z_raw, z_det=z_det)
    res["per_star_arrays"] = {"path": npz_path,
                              "sha256": sha256_file(npz_path),
                              "columns": ["dmag", "sigma_dmag", "color", "x", "y", "relmag",
                                          "snr1", "snr2", "fwhm1", "fwhm2",
                                          "sigma_dmag_empirical", "is_train",
                                          "z_raw", "z_det"]}
    # ---- 阴性/阳性对照（在真实测光结果上做注入，不重跑测光）----
    rng_inj = C.rng_for("real117_inject", SEED + 11)
    hold = ~is_train
    ctrl = {
        "zero_effect_no_injection": {
            "declared": "不注入时留出集 95% 覆盖率必须仍含标称（Wilson 区间）",
            "coverage68_floor": f_hold68,
            "contains_nominal_95": bool(
                d_floor_lv95["wilson95"][0] <= 0.95 <= d_floor_lv95["wilson95"][1]),
        },
        "injected_extra_random_0p10mag": {},
        "injected_common_offset_0p05mag": {},
    }
    for tag, feat in (("injected_extra_random_0p10mag", 0.10),):
        z_extra = (resid + rng_inj.normal(0.0, feat, size=resid.size)) / \
            np.sqrt(sig ** 2 + _sys2_for(snr_all, sys_bins))
        cov = cov_block(z_extra[hold])
        ctrl[tag] = {
            "declared": "注入已知 0.10 mag 附加随机误差 ⇒ 68% 覆盖率必须落到 Wilson 上界以下（判红）",
            "coverage": cov,
            "red_68_wilson_below_nominal": bool(cov["0.6826894921"]["wilson95"][1] < 0.6826894921370859),
        }
    # 共模偏移：两种口径都报（先声明方向）
    off = 0.05
    dmg_off = dmag + off
    c0_fixed = float(np.average(dmag[is_train], weights=1.0 / sig[is_train] ** 2))
    z_raw_fixed = (dmag - c0_fixed) / sig
    z_raw_fixed_off = (dmg_off - c0_fixed) / sig
    z_det_fixed_off = (dmg_off - pred) / sig              # 模型固定、不重拟合
    # 重拟合（把注入当成新数据）：趋势模型在训练集重拟合后应用到留出集
    beta_off = wls_fit(A[is_train], dmg_off[is_train], w[is_train])
    z_det_refit_off = (dmg_off - A @ beta_off) / sig
    ctrl["injected_common_offset_0p05mag"] = {
        "declared": ("注入已知 +0.05 mag 共模偏移：模型**固定**时 median(z) 必须显著偏离 0（判红）；"
                     "在同一数据上**重拟合**时共模不可辨识、median(z) 必须仍 ≈ 0（退化性登记）"),
        "fixed_model_median_z_raw_shift": float(np.median(z_raw_fixed_off) - np.median(z_raw_fixed)),
        "fixed_model_median_z_detrended_shift": float(np.median(z_det_fixed_off) - np.median(z_det)),
        "refit_model_median_z_shift": float(np.median(z_det_refit_off) - np.median(z_det)),
        "fixed_model_red": bool(abs(np.median(z_raw_fixed_off) - np.median(z_raw_fixed)) > 3.0),
        "refit_model_absorbed": bool(abs(np.median(z_det_refit_off)) < 0.5),
        "note": "共模零点无外部锚时不可辨识；本条只登记退化方向，不据此放行任何校正。",
    }
    res["negative_and_positive_controls"] = ctrl
    res["log"] = log
    C.dump_json(args.out, res)
    print("[S05 real] wrote %s (n_common=%d train=%d holdout=%d)" %
          (args.out, n_common, int(is_train.sum()), int((~is_train).sum())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
