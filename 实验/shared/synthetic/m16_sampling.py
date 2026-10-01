#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACSD RELEASE-02 / M16-SAMPLING --- 真实信号模板 -> 仿真采样帧 -> 重建。

定位（负责人 9.67 定案 7，逐字）
---------------------------------
> 「7.两帧够了。我不是让你用来叠加，而是用来代表真实信号，在此基础上建立仿真采样帧
>   再重建的这方面合成数据使用用的。」

* M16 真实 drz 帧**代表真实信号**（真实星云结构 + 真实星场 + 真实 PSF/采样）；
* 在其基础上**建立仿真采样帧**（simulated sampling frames）：按可控的**采样几何**
  （指向 / 抖动 / 像素尺度 / 旋转）、**曝光**、**seeing（PSF FWHM）**、**天光水平**、
  **增益/读出噪声**，从真实信号生成多帧观测；
* 这些采样帧交给 ACSD 重建链（normalize -> mosaic -> export）跑，再与**已知真值**
  （信号面）比较，量化重建保真度。

**不是**用来做多帧叠加/排异/接缝的（那些 >=3 帧才测的项目由合成数据覆盖，
不因 M16 只有两帧而受阻 --- 9.67 定案 7）。

物理噪声（9.41 / 9.47 强制）
------------------------------
噪声**全部**由 实验/shared/synthetic/noise_model.py（NM.expose，唯一事实源）
按物理过程重新生成::

    lam_e = t * (src_rate + sky_rate) * m(x,y) + t * D(T) * hot      [e-]
    n_e   = Poisson(lam_e) + Normal(0, sigma_R) + CR                 [e-]
    adu   = round(n_e / g + bias)   ，硬钳位到满阱                      [ADU]

* 源 / 天光 / 暗电流 **各自 Poisson（电子域）**；读出 **Gaussian（电子域）**；
  **增益量化（ADU）**；**平场乘性响应 m(x,y)**；**天空梯度**（可含月光光晕）。
* **严禁纯加性天光**（mode="additive" 仅作显式负例臂保留）。
* 真实帧**只提供结构**（期望面）；真实帧自带的噪声**不作**仿真噪声 --- 见「底图残差」登记。

单位（关键，勿搞反）
--------------------
真实 drz 帧 BUNIT = ELECTRONS/S ==> 像素值是**速率** r [e-/s]（EXPTIME 已除掉）。
本模块的信号模板 = 速率面 [e-/s]；合成时**先乘 exposure_s 得电子数**，再进泊松。
真实帧的 EXPTIME（F657N 9600 s / F673N 14400 s / F502N 16000 s）只用于**解释**底图，
**不**等于仿真曝光时间。

采样几何（精确、可核）
----------------------
每帧一个 TAN WCS（CRVAL 取该帧视场中心的天球坐标，CD = scale * R(theta) @ CD_canvas）。
生成时按 **像素 -> 天球 -> 画布像素** 两步精确映射（astropy），因此落盘 WCS 与生成所用
映射**逐像素一致** --- 重建链拿到的是一等公民 WCS，不存在「生成用 A、头部写 B」的错位。

诚实边界（不得省略）
--------------------
1. **底图残差**：画布 = 真实帧高斯平滑（canvas.smooth_sigma_px）后的期望面。该残差
   **不是**白噪声，且**未**计入合成帧的方差预算 --- 本模块直接**实测**底图残差 sigma 并
   与合成帧噪声 sigma 并列登记（判据：残差 << 合成噪声）。
2. **底图有效 PSF**：平滑会同时展宽真实星点 ==> 画布有效 PSF FWHM = **实测值**（登记）；
   目标 seeing **不得低于**该下限；低于时 sigma_add = 0 并置 seeing_floor_reached=true
   （如实登记，不假装锐化）。
3. **重采样核**：像素尺度 != 画布尺度时用三次样条重采样；插值核会轻微改变 PSF 形状
   （登记 resample_order）。
4. **未建模**：drizzle 相关噪声、CTE、非线性、fringing、溢出/辉散、导星漂移。
5. 真实 drz 的噪声经 32 次曝光 + drizzle 重采样后**相关化**；本模块生成的是**逐像素
   独立**噪声 ==> 噪声功率谱与真实 drz 不同，凡对**相关长度**敏感的判据不得用本合成帧定标。

场景配方字段
------------
  renderer        "m16_sampling"
  band            F657N(Ha) / F673N([S II]) / F502N([O III])
  shape           [ny, nx] 探测器帧尺寸
  canvas          {smooth_sigma_px, subtract_percentile, mask_dir, flux_scale, exclude_spike}
  sampling        {pixel_scale_arcsec, rotation_deg, base_fwhm_px(可 null=实测), resample_order}
  psf             默认 PSF（可被逐帧覆盖）：{model, fwhm_px(探测器像素), beta}
  detector        默认探测器（可被逐帧覆盖）
  flat            乘性平场响应参数（探测器固定图样，逐帧同一张）
  sky             默认天光（可被逐帧覆盖）
  masters         {dark_exposure_s, hot_pixel_fraction, hot_seed}
  frames          [{frame_id, pointing:{y,x,dy,dx}, exposure_s, psf, sky, detector, seed}, ...]

CLI
---
    export TMPDIR=/var/tmp/astrocs
    python3 m16_sampling.py --list-scenes
    python3 m16_sampling.py --scene scenes/m16_sampling_overlap_common.json \
        --out ../../../run/reverse_verify/m16_sampling/overlap_common
    python3 m16_sampling.py --selftest
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]                      # 仓库根（实验/shared/synthetic -> 根）
sys.path.insert(0, str(HERE))
import noise_model as NM          # noqa: E402  物理噪声链（唯一事实源）
import render as R                # noqa: E402  deep_merge / psf_kernel
import m16_mask as MK             # noqa: E402  有效域掩膜

SCENES_DIR = HERE / "scenes"
DEFAULT_MASK_DIR = "run/reverse_verify/m16_scene/masks"
CANVAS_ARCSEC_PER_PX = 0.04       # 真实 drz 板比例（头 D001SCAL=0.04）

BANDS: Dict[str, Dict[str, Any]] = {
    "F657N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits", "line": "H-alpha"},
    "F673N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f673n_v1_drz.fits", "line": "[S II]"},
    "F502N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f502n_v1_drz.fits", "line": "[O III]"},
}


# ---------------------------------------------------------------------------
# 0. 默认配置
# ---------------------------------------------------------------------------
SAMPLING_DEFAULTS: Dict[str, Any] = {
    "renderer": "m16_sampling",
    "scene_id": "m16_sampling",
    "kind": "same_pointing_overlap",
    "band": "F657N",
    "shape": [1024, 1024],
    "seed": 20261010,
    "canvas": {"smooth_sigma_px": 0.8, "subtract_percentile": 5.0,
               "mask_dir": DEFAULT_MASK_DIR, "flux_scale": 1.0, "exclude_spike": False},
    "sampling": {"pixel_scale_arcsec": CANVAS_ARCSEC_PER_PX, "rotation_deg": 0.0,
                 "base_fwhm_px": None, "resample_order": 3},
    "psf": {"model": "moffat4", "fwhm_px": 3.0, "beta": 4.0},
    "detector": {"gain_e_per_adu": 1.5, "read_noise_e": 5.0, "bias_adu": 1000.0,
                 "full_well_e": 120000.0, "dark_current_e_per_s": 0.02,
                 "dark_ref_temp_c": -20.0, "dark_double_temp_c": 6.0},
    "flat": {"prnu_rms": 0.01, "low_order": 0.02, "tilt_x": 1.0, "tilt_y": 0.5,
             "vignette": 0.05, "seed": 1234},
    "sky": {"level_e_per_s": 1.0, "grad_x_e_per_s": 0.0, "grad_y_e_per_s": 0.0,
            "grad_quad_e_per_s": 0.0, "theta_deg": 0.0, "moon_halo_e_per_s": 0.0,
            "moon_center": None, "moon_scale_px": 400.0},
    "temp_c": -20.0,
    "masters": {"dark_exposure_s": 600.0, "hot_pixel_fraction": 0.0, "hot_seed": 4321},
    "artifacts": {"cr_rate_per_frame": 0.0, "cr_mean_charge_e": 1000.0},
    "mode": NM.MODE_PHYSICAL,
    "frames": [],
}


def load_scene(path) -> Dict[str, Any]:
    p = Path(path)
    if not p.is_absolute():
        cand = [Path.cwd() / p, SCENES_DIR / p.name]
        p = next((c for c in cand if c.exists()), cand[0])
    raw = json.loads(p.read_text(encoding="utf-8"))
    return R.deep_merge(copy.deepcopy(SAMPLING_DEFAULTS), raw)


# ---------------------------------------------------------------------------
# 1. 真实信号模板（画布）：真实帧 -> 期望率面 [e-/s]
# ---------------------------------------------------------------------------
def _fill_nearest(arr: np.ndarray, valid: np.ndarray) -> np.ndarray:
    from scipy.ndimage import distance_transform_edt
    if valid.all():
        return arr.copy()
    idx = distance_transform_edt(~valid, return_distances=False, return_indices=True)
    return arr[tuple(idx)]


def canvas_source_path(band: str) -> Path:
    if band not in BANDS:
        raise ValueError("unknown band %r; expected one of %s" % (band, list(BANDS)))
    return ROOT / "testdata" / "HST_M16" / BANDS[band]["file"]


def _smooth_canvas(fill: np.ndarray, sig_k: float
                   ) -> Tuple[np.ndarray, np.ndarray, float]:
    """画布平滑的生产实现：返回 (smooth, resid, resid_frac_theory)。

    * ``smooth``            = 高斯平滑后的场（``sig_k <= 0`` 时不做滤波，原样返回）；
    * ``resid = fill - smooth`` = 被平滑**移除**的量；
    * ``resid_frac_theory`` = Σg² = 1/(4πσ_k²)：高斯核连续极限下平滑后**存活**的噪声
      方差份额（σ_k ≤ 0 即不滤波 ⇒ 份额 1）。

    G08-04 整改 R2：此段此前**内联**在 ``load_canvas`` 里，而自检 V6 只能在函数外用
    自己 import 的 scipy **另算一份**来核对 1/(4πσ²) —— 那是「验 scipy」而非「验本模块」，
    实测删掉生产平滑 / σ 翻倍 / theory 分母 4π→2π 三处缺陷 V6 全部判绿。抽成本函数后
    V6 走的就是 ``load_canvas`` 真正调用的这条实现。
    """
    from scipy.ndimage import gaussian_filter

    if sig_k > 0:
        smooth = gaussian_filter(fill, sigma=sig_k, mode="nearest")
        resid = fill - smooth
        resid_frac_theory = 1.0 / (4.0 * math.pi * sig_k * sig_k)
    else:
        smooth = fill
        resid = np.zeros_like(fill)
        resid_frac_theory = 1.0
    return smooth, resid, resid_frac_theory


def load_canvas(cfg: Dict[str, Any], *, verbose: bool = False
                ) -> Tuple[np.ndarray, np.ndarray, Dict[str, Any]]:
    """真实帧 -> (期望率面 [e-/s], 有效掩膜 bool, meta)。**不重采样**，全帧。"""
    from astropy.io import fits

    ccfg = dict(cfg["canvas"])
    band = str(cfg["band"])
    src = canvas_source_path(band)
    if not src.exists():
        raise FileNotFoundError("M16 frame not found: %s" % src)
    md = Path(ccfg.get("mask_dir") or DEFAULT_MASK_DIR)
    if not md.is_absolute():
        md = ROOT / md
    with fits.open(src, memmap=True) as f:
        hdr = f[0].header
        sci = np.asarray(f[0].data, dtype=np.float32)
    valid = np.ones(sci.shape, dtype=bool)
    mask_meta: Dict[str, Any] = {"used": False}
    try:
        vm, _mask_full = MK.load_valid_mask(band, md)
        valid = np.asarray(vm, dtype=bool)
        mask_meta = {"used": True, "mask_dir": str(md.relative_to(ROOT)), "band": band,
                     "valid_fraction": float(valid.mean())}
        if ccfg.get("exclude_spike"):
            with fits.open(md / ("%s_flags.fits" % band), memmap=True) as f:
                fl = np.asarray(f[0].data)
            n_sp = int(np.count_nonzero(fl & MK.F_SPIKE))
            valid = valid & ((fl & MK.F_SPIKE) == 0)
            mask_meta["exclude_spike"] = True
            mask_meta["spike_pixels_excluded"] = n_sp
    except FileNotFoundError as exc:
        mask_meta = {"used": False, "reason": str(exc)}
        if verbose:
            print("[m16_sampling] WARNING: mask not found (%s) -> all pixels valid" % exc)

    fill = _fill_nearest(sci.astype(np.float64), valid)
    pct = float(ccfg.get("subtract_percentile", 5.0))
    ped = float(np.percentile(fill[valid], pct))
    sig_k = float(ccfg.get("smooth_sigma_px", 0.8))
    smooth, resid, resid_frac_theory = _smooth_canvas(fill, sig_k)
    rate = smooth - ped
    rate = rate * float(ccfg.get("flux_scale", 1.0))
    rate = np.where(valid, rate, 0.0)

    # 底图噪声**实测**（只在平坦天区上量）
    flat_sky = valid & (rate < np.percentile(rate[valid], 40.0))
    #  (a) 含真实小尺度结构的散布（**不是**噪声，登记为对照）
    scatter_incl_structure = float(np.std(fill[flat_sky] - np.median(fill[flat_sky]))) \
        if flat_sky.any() else 0.0
    #  (b) **噪声型**估计：相邻像素差 std/sqrt(2) —— 对 >1 px 尺度的真实结构不敏感
    dmask = flat_sky[:, :-1] & flat_sky[:, 1:]
    raw_sigma_rate = float(np.std(np.diff(fill, axis=1)[dmask]) / math.sqrt(2.0)) \
        if dmask.any() else 0.0
    removed_sigma_rate = float(np.std(resid[flat_sky])) if flat_sky.any() else 0.0
    # 画布 = 平滑后的场 ==> 画布里**存活**的噪声 sigma = sigma_raw * sqrt(sum(g^2))
    #   sum(g^2) = 1/(4*pi*sigma_k^2)（高斯核，连续极限；selftest V6 已核 1% 内）
    resid_sigma_rate = raw_sigma_rate * math.sqrt(resid_frac_theory)

    phot = {"photflam": float(hdr["PHOTFLAM"]), "photplam": float(hdr["PHOTPLAM"]),
            "photzpt": float(hdr.get("PHOTZPT", -21.10))}
    phot["zp_ab"] = (-2.5 * math.log10(phot["photflam"])
                     - 5.0 * math.log10(phot["photplam"]) - 2.408)
    phot["zp_st"] = -2.5 * math.log10(phot["photflam"]) + phot["photzpt"]

    cd = [[float(hdr["CD1_1"]), float(hdr["CD1_2"])], [float(hdr["CD2_1"]), float(hdr["CD2_2"])]]
    scale_arcsec = math.sqrt(abs(cd[0][0] * cd[1][1] - cd[0][1] * cd[1][0])) * 3600.0
    wcs = {"crval1": float(hdr["CRVAL1"]), "crval2": float(hdr["CRVAL2"]),
           "crpix1": float(hdr["CRPIX1"]), "crpix2": float(hdr["CRPIX2"]),
           "cd": cd, "ctype1": str(hdr.get("CTYPE1", "RA---TAN")),
           "ctype2": str(hdr.get("CTYPE2", "DEC--TAN")),
           "scale_arcsec_per_px": scale_arcsec}
    meta: Dict[str, Any] = {
        "band": band, "line": BANDS[band]["line"], "path": str(src.relative_to(ROOT)),
        "shape": list(sci.shape), "bunit": hdr.get("BUNIT"),
        "real_exptime_s": float(hdr.get("EXPTIME", 0.0)),
        "ndrizim": int(hdr.get("NDRIZIM", 0)),
        "filters": {k: hdr.get(k) for k in ("TELESCOP", "INSTRUME", "DETECTOR", "FILTER",
                                            "APERTURE", "TARGNAME", "PROPOSID", "CAL_VER")
                    if k in hdr},
        "photometric": phot, "wcs": wcs, "mask": mask_meta,
        "pedestal_e_per_s": ped, "subtract_percentile": pct, "smooth_sigma_px": sig_k,
        "residual_base_noise_variance_fraction_theory": resid_frac_theory,
        "residual_base_noise_sigma_e_per_s_measured": resid_sigma_rate,
        "removed_by_smoothing_sigma_e_per_s_measured": removed_sigma_rate,
        "raw_pixel_sigma_e_per_s_measured": raw_sigma_rate,
        "flat_sky_scatter_incl_structure_e_per_s": scatter_incl_structure,
        "raw_sigma_estimator": "std(diff_along_x)/sqrt(2) on the low-40% rate region "
                               "(structure-insensitive noise estimator; drz noise is "
                               "correlated at ~1 px so this is an estimate, not exact)",
        "residual_note": "resid = raw * sqrt(1/(4*pi*sigma_k^2))：画布**存活**的噪声 sigma；"
                         "它是真实帧噪声经平滑后的残差，**未**计入合成帧噪声预算",
        "flat_sky_pixel_fraction": float(flat_sky.mean()),
        "units_note": "真实帧 BUNIT=ELECTRONS/S ==> 画布是速率面 [e-/s]；合成时乘 exposure_s",
        "rate_stats_e_per_s": {
            "median": float(np.median(rate[valid])), "mean": float(rate[valid].mean()),
            "p99": float(np.percentile(rate[valid], 99.0)),
            "p99_99": float(np.percentile(rate[valid], 99.99)),
            "max": float(rate[valid].max())},
    }
    if verbose:
        print("[m16_sampling] canvas %s %s valid=%.5f pedestal=%.5f e-/s "
              "resid_sigma=%.3e e-/s (raw %.3e)" % (
                  band, sci.shape, valid.mean(), ped, resid_sigma_rate, raw_sigma_rate))
    return rate, valid, meta


def canvas_wcs(meta: Dict[str, Any]):
    """由 meta 里的 TAN 参数构造 astropy WCS（无 SIP；与真实头逐字一致）。"""
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.crval = [meta["wcs"]["crval1"], meta["wcs"]["crval2"]]
    w.wcs.crpix = [meta["wcs"]["crpix1"], meta["wcs"]["crpix2"]]
    w.wcs.cd = np.array(meta["wcs"]["cd"], dtype=float)
    w.wcs.ctype = [meta["wcs"]["ctype1"], meta["wcs"]["ctype2"]]
    return w


def measure_base_psf(rate: np.ndarray, valid: np.ndarray, *, n_max: int = 300,
                     box: int = 15, step: int = 2, fwhm_accept_max: float = 8.0
                     ) -> Dict[str, Any]:
    """实测画布有效 PSF FWHM [画布像素]：亮且孤立峰的二阶矩中位。

    画布已被高斯平滑 ==> 测到的是**有效** PSF（含平滑），正是采样时「额外模糊」要减掉的量。
    峰检测在 step 倍降采样网格上做（快），二阶矩在**全分辨率**窗口上做（准）。
    """
    from scipy.ndimage import maximum_filter
    ny, nx = rate.shape
    sub = rate[::step, ::step]
    vsub = valid[::step, ::step]
    if not vsub.any():
        return {"fwhm_px": 2.0, "n_stars": 0, "method": "fallback_default"}
    thr = float(np.percentile(sub[vsub], 99.95))
    mx = maximum_filter(np.where(vsub, sub, -np.inf), size=7)
    peaks = (sub >= mx) & vsub & (sub > thr)
    ys, xs = np.nonzero(peaks)
    if len(ys) == 0:
        return {"fwhm_px": 2.0, "n_stars": 0, "method": "fallback_default"}
    order = np.argsort(sub[ys, xs])[::-1][:n_max]
    h = box // 2
    fwhms: List[float] = []
    yy, xx = np.mgrid[-h:h + 1, -h:h + 1]
    for k in order:
        y0, x0 = int(ys[k]) * step, int(xs[k]) * step
        if y0 - h < 0 or x0 - h < 0 or y0 + h >= ny or x0 + h >= nx:
            continue
        win = rate[y0 - h:y0 + h + 1, x0 - h:x0 + h + 1].astype(float)
        if win.shape != (box, box):
            continue
        w = win - float(np.median(win))
        w = np.where(w > 0, w, 0.0)
        s = w.sum()
        if s <= 0:
            continue
        cy = float((w * yy).sum() / s)
        cx = float((w * xx).sum() / s)
        vy = float((w * (yy - cy) ** 2).sum() / s)
        vx = float((w * (xx - cx) ** 2).sum() / s)
        sig = math.sqrt(max(0.5 * (vx + vy), 1e-9))
        f = 2.354820045 * sig
        if f <= fwhm_accept_max:                    # 排除星云结/双星等非点源
            fwhms.append(f)
    if len(fwhms) < 8:
        return {"fwhm_px": 2.0, "n_stars": len(fwhms), "method": "fallback_default"}
    return {"fwhm_px": float(np.median(fwhms)), "n_stars": len(fwhms),
            "method": "full_res_second_moment_median",
            "fwhm_p16": float(np.percentile(fwhms, 16)),
            "fwhm_p84": float(np.percentile(fwhms, 84)),
            "note": "含 canvas.smooth_sigma_px 的**有效** PSF（非 HST 原生 PSF）"}


# ---------------------------------------------------------------------------
# 2. 采样几何：探测器网格 -> 天球 -> 画布像素（精确两步映射）
# ---------------------------------------------------------------------------
def _rot(deg: float) -> np.ndarray:
    th = math.radians(float(deg))
    return np.array([[math.cos(th), -math.sin(th)], [math.sin(th), math.cos(th)]])


# --- TAN 投影的**解析**正反变换（WCS Paper II 式；避免迭代反解的精度损失）---------
def tan_deproject(xi_deg: np.ndarray, eta_deg: np.ndarray, crval1: float, crval2: float):
    """中间世界坐标 (xi, eta) [deg] -> 天球 (ra, dec) [deg]，TAN 投影。"""
    xi = np.radians(xi_deg)
    eta = np.radians(eta_deg)
    ra0 = math.radians(crval1)
    dec0 = math.radians(crval2)
    rho = np.hypot(xi, eta)
    c = np.arctan(rho)
    sinc = np.sin(c)
    cosc = np.cos(c)
    with np.errstate(invalid="ignore", divide="ignore"):
        dec = np.arcsin(np.clip(cosc * math.sin(dec0)
                                + eta * sinc * math.cos(dec0) / np.where(rho == 0, 1.0, rho),
                                -1.0, 1.0))
        ra = ra0 + np.arctan2(xi * sinc,
                              rho * math.cos(dec0) * cosc
                              - eta * math.sin(dec0) * sinc)
    dec = np.where(rho == 0, dec0, dec)
    ra = np.where(rho == 0, ra0, ra)
    return np.degrees(ra), np.degrees(dec)


def tan_project(ra_deg: np.ndarray, dec_deg: np.ndarray, crval1: float, crval2: float):
    """天球 (ra, dec) [deg] -> 中间世界坐标 (xi, eta) [deg]，TAN 投影。"""
    ra = np.radians(ra_deg)
    dec = np.radians(dec_deg)
    ra0 = math.radians(crval1)
    dec0 = math.radians(crval2)
    dl = ra - ra0
    cosc = math.sin(dec0) * np.sin(dec) + math.cos(dec0) * np.cos(dec) * np.cos(dl)
    xi = np.cos(dec) * np.sin(dl) / cosc
    eta = (math.cos(dec0) * np.sin(dec)
           - math.sin(dec0) * np.cos(dec) * np.cos(dl)) / cosc
    return np.degrees(xi), np.degrees(eta)


def _wcs_parts(meta_or_hdr: Dict[str, Any], from_meta: bool):
    if from_meta:
        w = meta_or_hdr["wcs"]
        cd = np.array(w["cd"], dtype=float)
        return (float(w["crval1"]), float(w["crval2"]), float(w["crpix1"]), float(w["crpix2"]),
                cd, np.linalg.inv(cd))
    h = meta_or_hdr
    cd = np.array([[h["CD1_1"], h["CD1_2"]], [h["CD2_1"], h["CD2_2"]]], dtype=float)
    return (float(h["CRVAL1"]), float(h["CRVAL2"]), float(h["CRPIX1"]), float(h["CRPIX2"]),
            cd, np.linalg.inv(cd))


def frame_wcs_from_center(canvas_meta: Dict[str, Any], shape: Tuple[int, int], s: float,
                          rotation_deg: float, center_yx: Tuple[float, float]):
    """由**视场中心**的画布像素坐标构造帧 WCS（与生成映射精确一致）。"""
    from astropy.wcs import WCS
    ny, nx = shape
    wc = canvas_wcs(canvas_meta)
    sky = wc.all_pix2world([[center_yx[1], center_yx[0]]], 0)[0]
    cd_c = np.array(canvas_meta["wcs"]["cd"], dtype=float)
    cd_d = s * (_rot(rotation_deg) @ cd_c)
    w = WCS(naxis=2)
    w.wcs.crval = [float(sky[0]), float(sky[1])]
    w.wcs.crpix = [nx / 2.0 + 0.5, ny / 2.0 + 0.5]
    w.wcs.cd = cd_d
    w.wcs.ctype = [canvas_meta["wcs"]["ctype1"], canvas_meta["wcs"]["ctype2"]]
    hdr = {"CRVAL1": float(sky[0]), "CRVAL2": float(sky[1]),
           "CRPIX1": nx / 2.0 + 0.5, "CRPIX2": ny / 2.0 + 0.5,
           "CD1_1": float(cd_d[0, 0]), "CD1_2": float(cd_d[0, 1]),
           "CD2_1": float(cd_d[1, 0]), "CD2_2": float(cd_d[1, 1]),
           "CTYPE1": canvas_meta["wcs"]["ctype1"], "CTYPE2": canvas_meta["wcs"]["ctype2"]}
    return w, hdr


def pointing_to_center(pointing: Dict[str, Any], shape: Tuple[int, int], s: float,
                       rotation_deg: float) -> Tuple[float, float]:
    """(y, x, dy, dx) 指向 -> 该帧**视场中心**在画布上的像素坐标（0-based index）。

    语义（精确）：**探测器像素 index (0,0)** 落在画布 index (y+dy, x+dx)（旋转/尺度已计）。
    帧中心像素的 index = ((ny-1)/2, (nx-1)/2)（= FITS CRPIX ny/2+0.5 换算到 0-based）。
    """
    ny, nx = shape
    off = s * (_rot(rotation_deg) @ np.array([(nx - 1.0) / 2.0, (ny - 1.0) / 2.0]))
    cy = float(pointing["y"]) + float(pointing.get("dy", 0.0)) + off[1]
    cx = float(pointing["x"]) + float(pointing.get("dx", 0.0)) + off[0]
    return cy, cx


def detector_to_canvas(frame_hdr: Dict[str, Any], canvas_meta: Dict[str, Any],
                       shape: Tuple[int, int]) -> np.ndarray:
    """探测器像素网格 -> 画布像素坐标 [2, ny, nx]（0-based index 空间）。

    **解析**两步映射：frame pixel -> (xi,eta) -> (ra,dec) -> (xi',eta') -> canvas pixel。
    两侧都是 TAN + 线性 CD，故该映射无迭代、无容差（机器精度）。
    """
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    c1, c2, px, py, cd_d, _inv_d = _wcs_parts(frame_hdr, from_meta=False)
    k1, k2, qx, qy, cd_c, inv_c = _wcs_parts(canvas_meta, from_meta=True)
    xi = cd_d[0, 0] * (xx - (px - 1.0)) + cd_d[0, 1] * (yy - (py - 1.0))
    eta = cd_d[1, 0] * (xx - (px - 1.0)) + cd_d[1, 1] * (yy - (py - 1.0))
    ra, dec = tan_deproject(xi, eta, c1, c2)
    xi2, eta2 = tan_project(ra, dec, k1, k2)
    cx = inv_c[0, 0] * xi2 + inv_c[0, 1] * eta2 + (qx - 1.0)
    cy = inv_c[1, 0] * xi2 + inv_c[1, 1] * eta2 + (qy - 1.0)
    return np.stack([cy, cx]).astype(np.float64)


def _bbox(coords: np.ndarray, margin: int, shape: Tuple[int, int]) -> Tuple[int, int, int, int]:
    y0 = max(0, int(math.floor(float(coords[0].min()))) - margin)
    y1 = min(shape[0], int(math.ceil(float(coords[0].max()))) + margin + 1)
    x0 = max(0, int(math.floor(float(coords[1].min()))) - margin)
    x1 = min(shape[1], int(math.ceil(float(coords[1].max()))) + margin + 1)
    return y0, y1, x0, x1


def sample_canvas(canvas_rate: np.ndarray, canvas_valid: np.ndarray, coords: np.ndarray,
                  *, sigma_extra_canvas_px: float, order: int
                  ) -> Tuple[np.ndarray, np.ndarray, Dict[str, Any]]:
    """按画布坐标 coords [2, ny, nx] 采样 -> (信号率 [e-/s], 有效 bool, meta)。

    额外模糊在**画布网格上**施加（避免先重采样再模糊引入混叠）。
    """
    from scipy.ndimage import gaussian_filter, map_coordinates
    margin = int(math.ceil(4.0 * max(sigma_extra_canvas_px, 0.0))) + 4
    y0, y1, x0, x1 = _bbox(coords, margin, canvas_rate.shape)
    if y1 - y0 < 2 or x1 - x0 < 2:
        raise ValueError("sampling window falls outside the canvas")
    crop = canvas_rate[y0:y1, x0:x1].astype(np.float64)
    cvalid = canvas_valid[y0:y1, x0:x1]
    if sigma_extra_canvas_px > 1e-6:
        crop = gaussian_filter(crop, sigma=float(sigma_extra_canvas_px), mode="nearest")
    cy = coords[0] - y0
    cx = coords[1] - x0
    inb = (cy >= 0) & (cy <= crop.shape[0] - 1) & (cx >= 0) & (cx <= crop.shape[1] - 1)
    yc = np.clip(cy, 0, crop.shape[0] - 1)
    xc = np.clip(cx, 0, crop.shape[1] - 1)
    sig = map_coordinates(crop, [yc, xc], order=int(order), mode="nearest").astype(np.float64)
    val = map_coordinates(cvalid.astype(np.float64), [yc, xc], order=1,
                          mode="constant", cval=0.0)
    valid = inb & (val >= 0.999)
    sig = np.where(valid, sig, 0.0)
    meta = {"crop": [int(y0), int(x0), int(y1 - y0), int(x1 - x0)],
            "sigma_extra_canvas_px": float(sigma_extra_canvas_px),
            "resample_order": int(order), "valid_fraction": float(valid.mean()),
            "outside_canvas_pixels": int((~inb).sum())}
    return sig, valid, meta


# ---------------------------------------------------------------------------
# 3. 渲染一帧仿真采样帧
# ---------------------------------------------------------------------------
def render_sampling_frame(scene: Dict[str, Any], canvas: Dict[str, Any], *,
                          frame_index: int, verbose: bool = False):
    """渲染一帧 -> (NM.Frame, truth dict, valid mask, signal rate [e-/s])。"""
    frames = scene.get("frames") or []
    if frame_index >= len(frames):
        raise IndexError("frame_index %d out of range (%d frames)" % (frame_index, len(frames)))
    fcfg = frames[frame_index]
    shape = tuple(int(v) for v in scene["shape"])
    ny, nx = shape
    seed = int(fcfg.get("seed", int(scene["seed"]) + frame_index))
    rng = np.random.default_rng(seed)

    canvas_rate: np.ndarray = canvas["rate"]
    canvas_valid: np.ndarray = canvas["valid"]
    cmeta: Dict[str, Any] = canvas["meta"]

    sampling = scene["sampling"]
    psf_cfg = R.deep_merge(dict(scene["psf"]), fcfg.get("psf", {}) or {})
    sky_cfg = R.deep_merge(dict(scene["sky"]), fcfg.get("sky", {}) or {})
    det_cfg = R.deep_merge(dict(scene["detector"]), fcfg.get("detector", {}) or {})
    t = float(fcfg["exposure_s"])
    if t <= 0:
        raise ValueError("exposure_s must be > 0")

    pix_scale = float(fcfg.get("sampling_pixel_scale_arcsec",
                               sampling["pixel_scale_arcsec"]))
    rot_deg = float(fcfg.get("sampling_rotation_deg", sampling.get("rotation_deg", 0.0)))
    s = pix_scale / float(cmeta["wcs"]["scale_arcsec_per_px"])
    base_fwhm_canvas = float(canvas["base_psf"]["fwhm_px"])

    pointing = dict(fcfg.get("pointing", {}))
    pointing.setdefault("y", 0)
    pointing.setdefault("x", 0)
    center_yx = pointing_to_center(pointing, shape, s, rot_deg)
    fwcs, whdr = frame_wcs_from_center(cmeta, shape, s, rot_deg, center_yx)

    fwhm_det = float(psf_cfg["fwhm_px"])
    # 1 探测器像素 = s 个画布像素（s = 探测器 arcsec/px ÷ 画布 arcsec/px）
    #   => 同一角度在画布网格上的像素数 = 在探测器网格上的像素数 × s
    fwhm_target_canvas = fwhm_det * s
    disc = fwhm_target_canvas ** 2 - base_fwhm_canvas ** 2
    sigma_extra = math.sqrt(max(disc, 0.0)) / 2.354820045 if disc > 0 else 0.0
    seeing_floor = bool(disc <= 0)

    coords = detector_to_canvas(whdr, cmeta, shape)
    sig_rate, valid, samp_meta = sample_canvas(
        canvas_rate, canvas_valid, coords,
        sigma_extra_canvas_px=sigma_extra, order=int(sampling.get("resample_order", 3)))
    # ── 面亮度 -> 探测器像素计数（**通量守恒**）─────────────────────────────
    # 画布像素值 = I[sb] * A_canvas；探测器像素覆盖 s^2 个画布像素
    #   => 探测器像素率 = I * A_det = 画布值 * s^2（s<1 时同样成立）
    # 插值给出的是画布像素**值**（面积平均意义），故须乘 s^2 才等于该探测器像素收到的率。
    sig_rate = sig_rate * (s ** 2)

    sky = NM.sky_surface_e_per_s(shape, **{k: v for k, v in sky_cfg.items()})
    sky = np.where(valid, sky, 0.0)

    det = NM.Detector(**det_cfg)
    flat_cfg = dict(scene["flat"])
    flat_seed = int(flat_cfg.pop("seed", 1234))
    flat = NM.flat_response(shape, np.random.default_rng(flat_seed), **flat_cfg)
    masters = scene.get("masters", {}) or {}
    hot = NM.hot_pixel_map(shape, np.random.default_rng(int(masters.get("hot_seed", 4321))),
                           float(masters.get("hot_pixel_fraction", 0.0)),
                           float(masters.get("hot_pixel_dark_gain", 50.0)))
    art = scene.get("artifacts", {}) or {}
    temp_c = float(fcfg.get("temp_c", scene.get("temp_c", det.dark_ref_temp_c)))

    frame = NM.expose(src_e_per_s=sig_rate, sky_e_per_s=sky, det=det, exptime_s=t, rng=rng,
                      temp_c=temp_c, flat=flat, hot_map=hot,
                      cr_rate_per_frame=float(art.get("cr_rate_per_frame", 0.0)),
                      cr_mean_charge_e=float(art.get("cr_mean_charge_e", 1000.0)),
                      mode=str(scene.get("mode", NM.MODE_PHYSICAL)))
    frame.adu = np.where(valid, frame.adu, 0.0)

    blank = valid & (sig_rate < np.percentile(sig_rate[valid], 30.0)) if valid.any() else valid
    synth_sigma_adu = float(NM.robust_sigma_adu(frame.adu, blank)) if blank.any() else 0.0
    resid_rate = float(cmeta["residual_base_noise_sigma_e_per_s_measured"])
    resid_adu_in_frame = resid_rate * t / det.gain_e_per_adu

    truth: Dict[str, Any] = {
        "scene_id": scene["scene_id"], "kind": scene["kind"], "renderer": "m16_sampling",
        "frame_id": str(fcfg.get("frame_id", "%s_f%02d" % (scene["scene_id"], frame_index))),
        "frame_index": frame_index, "seed": seed, "shape": list(shape),
        "band": scene["band"], "line": cmeta["line"],
        "exposure_s": t, "temp_c": temp_c, "mode": str(scene.get("mode", NM.MODE_PHYSICAL)),
        "sampling": {"pixel_scale_arcsec": pix_scale,
                     "canvas_scale_arcsec": cmeta["wcs"]["scale_arcsec_per_px"],
                     "scale_factor": s, "rotation_deg": rot_deg,
                     "resample_order": int(sampling.get("resample_order", 3))},
        "pointing": {"y": float(pointing["y"]), "x": float(pointing["x"]),
                     "dy": float(pointing.get("dy", 0.0)), "dx": float(pointing.get("dx", 0.0)),
                     "center_canvas_yx": [float(center_yx[0]), float(center_yx[1])],
                     "dither_label": fcfg.get("dither_label")},
        "psf": {"requested": psf_cfg, "fwhm_px_detector": fwhm_det,
                "fwhm_canvas_px_effective": base_fwhm_canvas,
                "sigma_extra_canvas_px": sigma_extra, "seeing_floor_reached": seeing_floor,
                "note": "画布有效 PSF = 实测（含平滑）；额外模糊按 FWHM 平方差加性合成"},
        "wcs": dict(whdr, scale_arcsec_per_px=pix_scale,
                    generation_map="pixel -> all_pix2world(frame WCS) -> "
                                   "all_world2pix(canvas WCS) -> cubic sample", exact=True),
        "detector": det.as_dict(),
        "flat": {"params": scene["flat"], "seed": flat_seed, "min": float(flat.min()),
                 "max": float(flat.max()), "median": float(np.median(flat))},
        "sky": {"params": sky_cfg,
                "level_e_per_s_mean": float(sky[valid].mean()) if valid.any() else 0.0,
                "max_e_per_s": float(sky.max()), "poisson": True,
                "note": "天光逐像素进泊松；纯加性天光是显式非物理负例臂（noise_model mode=additive）"},
        "canvas": {k: cmeta[k] for k in ("band", "path", "shape", "bunit", "real_exptime_s",
                                         "ndrizim", "pedestal_e_per_s", "smooth_sigma_px",
                                         "residual_base_noise_sigma_e_per_s_measured",
                                         "residual_base_noise_variance_fraction_theory")},
        "sampling_meta": samp_meta,
        "flux_conservation": {
            "rule": "detector_pixel_rate = canvas_rate * s^2 (s = detector arcsec/px ÷ "
                    "canvas arcsec/px); canvas value = surface brightness x canvas pixel area",
            "s_squared": float(s ** 2),
            "note": "点源总通量在重采样前后守恒（画布 F -> 探测器 F）"},
        "noise_budget": {
            "synthetic_sigma_adu_measured": synth_sigma_adu,
            "base_residual_sigma_e_per_s": resid_rate,
            "base_residual_e_in_frame": float(resid_rate * t),
            "base_residual_adu_in_frame": float(resid_adu_in_frame),
            "base_residual_variance_share": float((resid_adu_in_frame / synth_sigma_adu) ** 2)
            if synth_sigma_adu > 0 else None,
            "note": "底图残差**未**计入合成噪声；此处如实并列登记。份额应 << 1"},
        "valid": {"fraction": float(valid.mean()), "n_invalid": int((~valid).sum()),
                  "policy": "invalid -> ADU 0（与真实 drz 零填充一致）；truth 文件 MASK 登记"},
        "adu_stats": {"min": float(frame.adu.min()),
                      "median": float(np.median(frame.adu[valid])) if valid.any() else 0.0,
                      "max": float(frame.adu.max()),
                      "saturated_pixels": int(np.count_nonzero(frame.adu >= det.saturation_adu))
                      if det.saturate else 0},
        "signal_rate_stats_e_per_s": {
            "median": float(np.median(sig_rate[valid])) if valid.any() else 0.0,
            "p99": float(np.percentile(sig_rate[valid], 99.0)) if valid.any() else 0.0,
            "max": float(sig_rate.max())},
        "provenance": frame.provenance,
    }
    if verbose:
        print("[m16_sampling] %-22s band=%-5s t=%6.1fs see=%.2fpx(extra %.2f canvas px) "
              "sky=%.3f e-/s med=%7.0f ADU valid=%.4f sat=%d" % (
                  truth["frame_id"], scene["band"], t, fwhm_det, sigma_extra,
                  truth["sky"]["level_e_per_s_mean"], truth["adu_stats"]["median"],
                  truth["valid"]["fraction"], truth["adu_stats"]["saturated_pixels"]))
    return frame, truth, valid, sig_rate


# ---------------------------------------------------------------------------
# 4. 落盘：SCI 帧（只主 HDU，喂重建链）+ 真值文件 + meta
# ---------------------------------------------------------------------------
def _ra_hms(ra_deg: float) -> str:
    h = (float(ra_deg) % 360.0) / 15.0
    hh = int(h)
    mm = int((h - hh) * 60.0)
    ss = ((h - hh) * 60.0 - mm) * 60.0
    return "%02d %02d %06.3f" % (hh, mm, ss)


def _dec_dms(dec_deg: float) -> str:
    sign = "-" if float(dec_deg) < 0 else "+"
    d = abs(float(dec_deg))
    dd = int(d)
    mm = int((d - dd) * 60.0)
    ss = ((d - dd) * 60.0 - mm) * 60.0
    return "%s%02d %02d %05.2f" % (sign, dd, mm, ss)


def write_sampling_frame(outdir: Path, truth: Dict[str, Any], frame: NM.Frame,
                         valid: np.ndarray, sig_rate: np.ndarray) -> Dict[str, str]:
    from astropy.io import fits
    fdir = outdir / "frames"
    fdir.mkdir(parents=True, exist_ok=True)
    fid = truth["frame_id"]
    w = truth["wcs"]
    hdr = fits.Header()
    hdr["CRVAL1"] = (w["CRVAL1"], "[deg] ICRS")
    hdr["CRVAL2"] = (w["CRVAL2"], "[deg] ICRS")
    hdr["CRPIX1"] = (w["CRPIX1"], "reference pixel (1-based)")
    hdr["CRPIX2"] = (w["CRPIX2"], "reference pixel (1-based)")
    hdr["CD1_1"] = w["CD1_1"]
    hdr["CD1_2"] = w["CD1_2"]
    hdr["CD2_1"] = w["CD2_1"]
    hdr["CD2_2"] = w["CD2_2"]
    hdr["CTYPE1"] = (w["CTYPE1"], "TAN projection")
    hdr["CTYPE2"] = (w["CTYPE2"], "TAN projection")
    hdr["RADESYS"] = ("ICRS", "celestial reference system")
    # ── 指向/板尺度关键字：ACSD normalize 的 wcs.init_source=header_pointing 消费 ──
    # （OBJCTRA = 六进制**小时**；OBJCTDEC = 六进制度；s0 = 206.265*XPIXSZ/FOCALLEN）
    hdr["OBJCTRA"] = (_ra_hms(w["CRVAL1"]), "[h m s] field-centre RA (ICRS)")
    hdr["OBJCTDEC"] = (_dec_dms(w["CRVAL2"]), "[d m s] field-centre Dec (ICRS)")
    hdr["FOCALLEN"] = (1000.0, "[mm] synthetic focal length (with XPIXSZ gives plate scale)")
    hdr["XPIXSZ"] = (float(truth["sampling"]["pixel_scale_arcsec"]) * 1000.0 / 206.265,
                     "[um] synthetic pixel size; s0=206.265*XPIXSZ/FOCALLEN [arcsec/px]")
    hdr["BUNIT"] = ("ADU", "synthetic sampling frame unit")
    hdr["EXPTIME"] = (float(truth["exposure_s"]), "[s] synthetic exposure")
    hdr["GAIN"] = (float(truth["detector"]["gain_e_per_adu"]), "[e-/ADU]")
    hdr["RDNOISE"] = (float(truth["detector"]["read_noise_e"]), "[e-]")
    hdr["FILTER"] = (str(truth["band"]), "HST WFC3/UVIS filter of the real signal template")
    hdr["SCENEID"] = (str(truth["scene_id"]), "sampling scene recipe id")
    hdr["KIND"] = (str(truth["kind"]), "coverage-matrix cell kind")
    hdr["FRAMEID"] = (fid, "frame id")
    hdr["SEED"] = (int(truth["seed"]), "RNG seed")
    hdr["RENDERER"] = ("m16_sampling", "M16 real-signal sampling-frame renderer")
    hdr["PIXSCALE"] = (float(truth["sampling"]["pixel_scale_arcsec"]), "[arcsec/px] detector")
    hdr["PSFFWHM"] = (float(truth["psf"]["fwhm_px_detector"]), "[pix] target seeing FWHM")
    hdr["ROT_DEG"] = (float(truth["sampling"]["rotation_deg"]), "[deg] detector rotation")
    hdr["P_Y"] = (float(truth["pointing"]["y"]), "[canvas px] pointing origin y")
    hdr["P_X"] = (float(truth["pointing"]["x"]), "[canvas px] pointing origin x")
    hdr["P_DY"] = (float(truth["pointing"]["dy"]), "[canvas px] sub-pixel dither y")
    hdr["P_DX"] = (float(truth["pointing"]["dx"]), "[canvas px] sub-pixel dither x")
    hdr.add_history("ACSD M16-SAMPLING: real M16 signal template -> sampling frame")
    hdr.add_history("noise chain (GAP_AUDIT 9.41/9.47):")
    hdr.add_history("lam_e = t*(src+sky)*flat + t*D(T)*hot")
    hdr.add_history("n_e = Poisson(lam_e) + N(0,RN); adu = round(n_e/gain + bias)")
    hdr.add_history("invalid -> 0; NO additive-sky shortcut")
    hdr.add_history("template: %s" % truth["canvas"]["path"])

    fp = fdir / ("%s.fits" % fid)
    fits.PrimaryHDU(data=np.asarray(frame.adu, dtype=np.float32),
                    header=hdr).writeto(fp, overwrite=True)
    tp = fdir / ("%s.truth.fits" % fid)
    th = fits.Header()
    th["FRAMEID"] = (fid, "frame id")
    th["BUNIT1"] = ("ELECTRONS/S", "HDU1 TRUTHRATE")
    th["BUNIT2"] = ("ELECTRONS", "HDU2 TRUTHE")
    th.add_history("truth for the sampling frame; NOT read by the reconstruction chain")
    fits.HDUList([
        fits.PrimaryHDU(data=np.asarray(sig_rate, dtype=np.float32), header=th),
        fits.ImageHDU(data=np.asarray(frame.truth_e, dtype=np.float32), name="TRUTHE"),
        fits.ImageHDU(data=np.asarray(valid.astype(np.uint8)), name="MASK"),
    ]).writeto(tp, overwrite=True)
    mp = fdir / ("%s.meta.json" % fid)
    with open(mp, "w", encoding="utf-8") as fh:
        json.dump(truth, fh, indent=1, ensure_ascii=False, default=float)
    return {"fits": str(fp), "truth": str(tp), "meta": str(mp)}


def write_masters(outdir: Path, scene: Dict[str, Any], shape: Tuple[int, int],
                  canvas_meta: Dict[str, Any]) -> Dict[str, Any]:
    """合成校准母版（与帧生成所用探测器模型**严格一致**，供 normalize 真标定）。

    SCI-CAL-001 5:
        master_bias  零曝光本底（ADU，与 light 同标度）
        master_dark  已减 bias 的暗电流母版（ADU），EXPTIME = t_dark（K=t_light/t_dark 由此推）
        master_flat  已归一平场（median=1.0）

    注意：expose 的物理链把 flat 乘在 src+sky 上、**不**乘 dark（显式简化，见 provenance）；
    标定链按 cal=(raw-bias-K*dark)/flat 会把 dark 也除掉 --- 该差异在暗电流量级下已登记。
    """
    from astropy.io import fits
    mdir = outdir / "masters"
    mdir.mkdir(parents=True, exist_ok=True)
    det = NM.Detector(**dict(scene["detector"]))
    masters = scene.get("masters", {}) or {}
    t_dark = float(masters.get("dark_exposure_s", 600.0))
    temp_c = float(scene.get("temp_c", det.dark_ref_temp_c))
    dark_rate = det.dark_current_at(temp_c)
    hot = NM.hot_pixel_map(shape, np.random.default_rng(int(masters.get("hot_seed", 4321))),
                           float(masters.get("hot_pixel_fraction", 0.0)),
                           float(masters.get("hot_pixel_dark_gain", 50.0)))
    dark_map = np.full(shape, dark_rate, dtype=float)
    if hot is not None:
        dark_map = dark_map * hot
    bias_adu = np.full(shape, float(det.bias_adu), dtype=np.float32)
    dark_adu = (dark_map * t_dark / det.gain_e_per_adu).astype(np.float32)
    flat_cfg = dict(scene["flat"])
    flat_seed = int(flat_cfg.pop("seed", 1234))
    flat = NM.flat_response(shape, np.random.default_rng(flat_seed), **flat_cfg).astype(np.float32)

    def _w(path: Path, data, exptime: float, kind: str) -> str:
        h = fits.Header()
        h["BUNIT"] = ("ADU", "master frame unit (ADU)")
        h["EXPTIME"] = (float(exptime), "[s]")
        h["MASTER"] = (kind, "bias|dark|flat")
        h["GAIN"] = (float(det.gain_e_per_adu), "[e-/ADU]")
        h["TEMPC"] = (temp_c, "[C]")
        h["SCENEID"] = (str(scene["scene_id"]), "sampling scene recipe id")
        h.add_history("ACSD RELEASE-02 M16-SAMPLING synthetic master; consistent with the")
        h.add_history("same detector model used to render the sampling frames")
        fits.PrimaryHDU(data=data, header=h).writeto(path, overwrite=True)
        return str(path)

    out = {"master_bias": _w(mdir / "master_bias.fits", bias_adu, 0.0, "bias"),
           "master_dark": _w(mdir / "master_dark.fits", dark_adu, t_dark, "dark"),
           "master_flat": _w(mdir / "master_flat.fits", flat, 1.0, "flat")}
    out["_meta"] = {
        "bias_adu": float(det.bias_adu),
        "dark_exposure_s": t_dark, "dark_current_e_per_s_at_temp": float(dark_rate),
        "dark_adu_median": float(np.median(dark_adu)),
        "flat_median": float(np.median(flat)), "flat_seed": flat_seed,
        "flat_params": scene["flat"],
        "hot_pixel_fraction": float(masters.get("hot_pixel_fraction", 0.0)),
        "hot_seed": int(masters.get("hot_seed", 4321)),
        "note": "dark_optimization=false（master_dark 已减 bias）；K 由 FITS EXPTIME 推 "
                "K=t_light/t_dark（module_adapters B2-A13/BIAS-001）",
        "canvas_path": canvas_meta["path"]}
    return out


def write_canvas_truth(outdir: Path, canvas: Dict[str, Any],
                       frames_meta: List[Dict[str, Any]]) -> Dict[str, Any]:
    """落盘真值画布的**最小覆盖裁剪**（供重建保真度比较），含其 WCS。"""
    from astropy.io import fits
    cmeta = canvas["meta"]
    y0 = min(int(f["sampling_meta"]["crop"][0]) for f in frames_meta)
    x0 = min(int(f["sampling_meta"]["crop"][1]) for f in frames_meta)
    y1 = max(int(f["sampling_meta"]["crop"][0]) + int(f["sampling_meta"]["crop"][2])
             for f in frames_meta)
    x1 = max(int(f["sampling_meta"]["crop"][1]) + int(f["sampling_meta"]["crop"][3])
             for f in frames_meta)
    crop = canvas["rate"][y0:y1, x0:x1]
    vcrop = canvas["valid"][y0:y1, x0:x1]
    h = fits.Header()
    w = cmeta["wcs"]
    h["CRVAL1"] = w["crval1"]
    h["CRVAL2"] = w["crval2"]
    h["CRPIX1"] = w["crpix1"] - x0
    h["CRPIX2"] = w["crpix2"] - y0
    h["CD1_1"] = w["cd"][0][0]
    h["CD1_2"] = w["cd"][0][1]
    h["CD2_1"] = w["cd"][1][0]
    h["CD2_2"] = w["cd"][1][1]
    h["CTYPE1"] = w["ctype1"]
    h["CTYPE2"] = w["ctype2"]
    h["BUNIT"] = ("ELECTRONS/S", "truth signal rate (real template, denoised)")
    h["BAND"] = (cmeta["band"], "HST WFC3/UVIS filter")
    h["CROP_Y0"] = (y0, "crop origin in full-frame coords")
    h["CROP_X0"] = (x0, "crop origin in full-frame coords")
    h.add_history("truth canvas = real M16 drz denoised to the expectation rate surface;")
    h.add_history("NO synthetic noise added (compare against the reconstructed mosaic)")
    fp = outdir / "truth_canvas.fits"
    fits.HDUList([fits.PrimaryHDU(data=crop.astype(np.float32), header=h),
                  fits.ImageHDU(data=vcrop.astype(np.uint8),
                                name="MASK")]).writeto(fp, overwrite=True)
    return {"truth_canvas": str(fp), "crop": [y0, x0, int(y1 - y0), int(x1 - x0)],
            "shape": [int(y1 - y0), int(x1 - x0)]}


# ---------------------------------------------------------------------------
# 5. 数据集入口
# ---------------------------------------------------------------------------
def render_sampling_dataset(scene_path, outdir, *, seed: Optional[int] = None,
                            verbose: bool = True) -> Dict[str, Any]:
    scene = load_scene(scene_path)
    if seed is not None:
        scene["seed"] = int(seed)
    shape = tuple(int(v) for v in scene["shape"])
    out = Path(outdir)
    if not out.is_absolute():
        out = ROOT / out
    out.mkdir(parents=True, exist_ok=True)

    t0 = time.time()
    rate, valid, cmeta = load_canvas(scene, verbose=verbose)
    base = scene["sampling"].get("base_fwhm_px")
    if base is None:
        base_psf = measure_base_psf(rate, valid)
    else:
        base_psf = {"fwhm_px": float(base), "n_stars": 0, "method": "config_override"}
    canvas = {"rate": rate, "valid": valid, "meta": cmeta, "base_psf": base_psf}

    man: Dict[str, Any] = {
        "scene_path": str(scene_path), "scene_id": scene["scene_id"], "kind": scene["kind"],
        "renderer": "m16_sampling", "band": scene["band"], "shape": list(shape),
        "base_seed": int(scene["seed"]), "outdir": str(out),
        "canvas": cmeta, "base_psf": base_psf, "sampling": scene["sampling"],
        "sky_default": scene["sky"], "detector_default": scene["detector"],
        "masters": write_masters(out, scene, shape, cmeta), "frames": []}
    frames_meta: List[Dict[str, Any]] = []
    for k in range(len(scene.get("frames") or [])):
        f, truth, v, sig = render_sampling_frame(scene, canvas, frame_index=k, verbose=verbose)
        paths = write_sampling_frame(out, truth, f, v, sig)
        frames_meta.append(truth)
        rec = {"status": "OK", "frame_id": truth["frame_id"], "frame_index": k,
               "seed": truth["seed"], "exposure_s": truth["exposure_s"],
               "band": truth["band"], "psf_fwhm_px": truth["psf"]["fwhm_px_detector"],
               "pixel_scale_arcsec": truth["sampling"]["pixel_scale_arcsec"],
               "pointing": truth["pointing"], "sky_level_e_per_s": truth["sky"]["level_e_per_s_mean"],
               "valid_fraction": truth["valid"]["fraction"],
               "adu_stats": truth["adu_stats"],
               "noise_budget": truth["noise_budget"]}
        rec.update(paths)
        man["frames"].append(rec)
    man["truth"] = write_canvas_truth(out, canvas, frames_meta)
    man["elapsed_s"] = time.time() - t0
    with open(out / "dataset.json", "w", encoding="utf-8") as fh:
        json.dump(man, fh, indent=1, ensure_ascii=False, default=float)
    if verbose:
        print("[m16_sampling] wrote %d frame(s) -> %s (%.1fs)" % (
            len(man["frames"]), out, man["elapsed_s"]))
    return man


def list_scenes() -> List[str]:
    return sorted(p.name for p in SCENES_DIR.glob("m16_sampling_*.json"))


# ---------------------------------------------------------------------------
# 6. 自检（能红能绿）
# ---------------------------------------------------------------------------
def _fake_canvas(shape=(96, 96), *, sky_rate=0.5):
    """合成"真实信号模板"画布（不读真实帧），用于端到端采样自检。"""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    rate = (0.3
            + 2.0 * np.exp(-(((yy - 30.0) ** 2 + (xx - 30.0) ** 2) / (2 * 6.0 ** 2)))
            + 5.0 * np.exp(-(((yy - 70.0) ** 2 + (xx - 62.0) ** 2) / (2 * 1.6 ** 2))))
    valid = np.ones(shape, dtype=bool)
    meta = {"band": "SYNTH", "line": "selftest", "path": "selftest://fake",
            "shape": list(shape), "bunit": "ELECTRONS/S", "real_exptime_s": 100.0,
            "ndrizim": 1, "pedestal_e_per_s": 0.0, "smooth_sigma_px": 0.0,
            "residual_base_noise_sigma_e_per_s_measured": 0.0,
            "residual_base_noise_variance_fraction_theory": 1.0,
            "wcs": {"crval1": 274.7, "crval2": -13.84, "crpix1": 48.5, "crpix2": 48.5,
                    "cd": [[-9.1016831619991e-06, -6.3731918728008e-06],
                           [-6.3730803948928e-06, 9.10186123591136e-06]],
                    "ctype1": "RA---TAN", "ctype2": "DEC--TAN",
                    "scale_arcsec_per_px": 0.04}}
    return {"rate": rate, "valid": valid, "meta": meta,
            "base_psf": {"fwhm_px": 2.0, "n_stars": 0, "method": "selftest_fixed"}}


def _selftest_render_end_to_end() -> Dict[str, Any]:
    """端到端：走 render_sampling_frame 全链（含 WCS/采样/噪声）。"""
    canvas = _fake_canvas()
    base = copy.deepcopy(SAMPLING_DEFAULTS)
    base["scene_id"] = "selftest_render"
    base["shape"] = [96, 96]
    base["canvas"] = {"smooth_sigma_px": 0.0}
    base["sampling"] = {"pixel_scale_arcsec": 0.04, "rotation_deg": 0.0,
                        "base_fwhm_px": 2.0, "resample_order": 3}
    base["psf"] = {"model": "moffat4", "fwhm_px": 2.0, "beta": 4.0}
    base["detector"] = {"gain_e_per_adu": 1.5, "read_noise_e": 5.0, "bias_adu": 1000.0,
                        "full_well_e": 120000.0, "dark_current_e_per_s": 0.0}
    base["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "vignette": 0.0, "seed": 1}
    base["sky"] = {"level_e_per_s": 0.5}
    base["masters"] = {"dark_exposure_s": 600.0, "hot_pixel_fraction": 0.0}
    base["frames"] = [
        {"frame_id": "lo", "exposure_s": 300.0, "seed": 101,
         "pointing": {"y": 0, "x": 0, "dy": 0.0, "dx": 0.0},
         "sky": {"level_e_per_s": 0.5}},
        {"frame_id": "hi", "exposure_s": 300.0, "seed": 101,
         "pointing": {"y": 0, "x": 0, "dy": 0.0, "dx": 0.0},
         "sky": {"level_e_per_s": 8.0}},
        {"frame_id": "shift", "exposure_s": 300.0, "seed": 101,
         "pointing": {"y": 0, "x": 0, "dy": 0.5, "dx": 0.25},
         "sky": {"level_e_per_s": 0.5}},
    ]
    outs = []
    for k in range(3):
        f, truth, valid, sig = render_sampling_frame(base, canvas, frame_index=k)
        outs.append((f, truth, valid, sig))
    sig_lo = outs[0][3]
    sig_hi = outs[1][3]
    # 天光 Poisson 通道：同种子下 hi 的 sigma 必须显著大于 lo
    s_lo = NM.robust_sigma_adu(outs[0][0].adu, outs[0][2])
    s_hi = NM.robust_sigma_adu(outs[1][0].adu, outs[1][2])
    # 抖动：用**锐星质心**测亚像素位移（相位相关只能给整数，不够）
    def _centroid(a, y0, x0, h=6):
        win = a[y0 - h:y0 + h + 1, x0 - h:x0 + h + 1].astype(float)
        bkg = float(np.median(win))
        w = np.clip(win - bkg, 0.0, None)
        yy, xx = np.mgrid[-h:h + 1, -h:h + 1]
        s = w.sum()
        return (float((w * yy).sum() / s) + y0, float((w * xx).sum() / s) + x0)
    c_a = _centroid(sig_lo, 70, 62)
    c_b = _centroid(outs[2][3], 70, 62)
    dy = c_b[0] - c_a[0]
    dx = c_b[1] - c_a[1]
    # 纯加性负例臂：同一成品帧 + 常数 => sigma 逐位不变
    a0 = outs[0][0].adu
    rel = abs(NM.robust_sigma_adu(a0 + 300.0, outs[0][2])
              - NM.robust_sigma_adu(a0, outs[0][2])) / NM.robust_sigma_adu(a0, outs[0][2])
    # V8 采样几何 × seeing 单位：0.1"/px（s=2.5）时目标 seeing 必须真的出现在**探测器**帧上
    #    专用画布：单个高斯星 + 常数本底（无其它结构，二阶矩才干净）
    ny8, nx8 = 128, 128
    yy8c, xx8c = np.mgrid[0:ny8, 0:nx8]
    sig_star_canvas = 1.0
    rate8 = 1.0 + 10.0 * np.exp(-(((yy8c - 64.0) ** 2 + (xx8c - 64.0) ** 2)
                                  / (2 * sig_star_canvas ** 2)))
    cmeta8 = dict(canvas["meta"])
    cmeta8["wcs"] = dict(canvas["meta"]["wcs"], crpix1=64.5, crpix2=64.5)
    canvas8 = {"rate": rate8, "valid": np.ones((ny8, nx8), dtype=bool), "meta": cmeta8,
               "base_psf": {"fwhm_px": 2.354820045 * sig_star_canvas, "n_stars": 1,
                            "method": "selftest_analytic"}}
    base8 = copy.deepcopy(base)
    base8["shape"] = [48, 48]
    base8["sampling"] = {"pixel_scale_arcsec": 0.1, "rotation_deg": 0.0,
                         "base_fwhm_px": None, "resample_order": 3}
    base8["frames"] = [{"frame_id": "see32", "exposure_s": 1.0, "seed": 7,
                        "pointing": {"y": 0, "x": 0, "dy": 0.0, "dx": 0.0},
                        "psf": {"fwhm_px": 3.2}, "sky": {"level_e_per_s": 0.0}}]
    f8, t8, v8, sig8 = render_sampling_frame(base8, canvas8, frame_index=0)
    s8 = t8["sampling"]["scale_factor"]
    h = 12
    yc, xc = 24, 24                     # 星在画布 (64,64) -> 探测器 (64/2.5, 64/2.5) = (25.6, 25.6)
    win = sig8[yc - h:yc + h + 1, xc - h:xc + h + 1].astype(float)
    yy8, xx8 = np.mgrid[-h:h + 1, -h:h + 1]
    w8 = np.clip(win - np.median(win), 0, None)
    ssum = w8.sum()
    cy8 = float((w8 * yy8).sum() / ssum)
    cx8 = float((w8 * xx8).sum() / ssum)
    sig_m = math.sqrt(max(0.5 * (float((w8 * (yy8 - cy8) ** 2).sum() / ssum)
                                 + float((w8 * (xx8 - cx8) ** 2).sum() / ssum)), 1e-12))
    fwhm_meas = 2.354820045 * sig_m
    return {"sigma_lo_adu": float(s_lo), "sigma_hi_adu": float(s_hi),
            "scale_factor_s": float(s8),
            "seeing_target_detector_px": 3.2, "seeing_measured_detector_px": float(fwhm_meas),
            "seeing_rel_err": float(abs(fwhm_meas - 3.2) / 3.2),
            "sigma_extra_canvas_px": float(t8["psf"]["sigma_extra_canvas_px"]),
            "seeing_floor_reached": bool(t8["psf"]["seeing_floor_reached"]),
            "sigma_ratio_high_over_low": float(s_hi / s_lo),
            "dither_shift_px": [dy, dx],
            "dither_expected_px": [-0.5, -0.25],
            "dither_shift_px_err": float(math.hypot(dy + 0.5, dx + 0.25)),
            "additive_arm_sigma_rel_change": float(rel),
            "sky_level_lo": outs[0][1]["sky"]["level_e_per_s_mean"],
            "sky_level_hi": outs[1][1]["sky"]["level_e_per_s_mean"],
            "noise_budget_share": outs[0][1]["noise_budget"]["base_residual_variance_share"]}


def _selftest_real_canvas() -> Dict[str, Any]:
    """走**真实 FITS 加载路径** load_canvas（读 testdata 里的真实 M16 drz 板）。

    G08-04 整改 T5：本自检此前只用 ``_fake_canvas``（96×96 合成画布），
    ``load_canvas`` 从不被调用 —— 实测把 ``load_canvas`` 打成永远 raise，
    ``--selftest`` 仍全绿 ⇒ 对真实模板恒真。判据必须覆盖真实路径，否则
    「真实 M16 模板可用」这一前提从未被验证过。

    **模板缺失时记红，不静默跳过**（不设 waiver 开关）：真实模板
    ``testdata/HST_M16/hlsp_..._<band>_drz.fits`` 被 .gitignore 排除
    （.gitignore:167），干净 clone 上必然缺失 ⇒ 此时本判据红是**正确**的
    「前提不成立」读数，由运行者补齐数据，而不是把门悄悄放过。
    """
    out: Dict[str, Any] = {
        "name": "V10_real_canvas_load",
        "production_call": "m16_sampling.load_canvas (真实 FITS)",
        "template_available": False,
    }
    band = "F657N"
    try:
        src = canvas_source_path(band)
    except ValueError as exc:                       # 负例：未知 band 必须被拒
        out["unknown_band_rejected"] = True
        out["unknown_band_error"] = str(exc)
        src = None
    if src is None or not src.exists():
        out["verdict"] = False
        out["reason"] = (
            "真实 M16 模板缺失：%s —— load_canvas 的真实路径无法被验证。"
            "按 AGENTS §8「不以 waiver 静默覆盖红灯」，此处记红；"
            "请补齐 testdata/HST_M16/ 后重跑，不要用开关跳过。"
            % (str(src) if src is not None else "band 未知"))
        out["verdict_reason"] = "模板缺失 = 真实加载路径未被覆盖"
        return out
    out["template_available"] = True
    out["template_path"] = str(src)
    out["template_bytes"] = int(src.stat().st_size)
    # 负例：未知 band 必须抛错 ⇒ 证明 band 表真的被查、不是桩函数
    try:
        canvas_source_path("__NOT_A_BAND__")
        out["unknown_band_rejected"] = False
    except ValueError as exc:
        out["unknown_band_rejected"] = True
        out["unknown_band_error"] = str(exc)
    # 真实场景配方（取第一个可用 scene；没有则退回默认配置）→ 一并覆盖 load_scene
    scenes = sorted(SCENES_DIR.glob("m16_sampling_*.json"))
    cfg = load_scene(scenes[0].name) if scenes else copy.deepcopy(SAMPLING_DEFAULTS)
    cfg["band"] = band
    out["scene_used"] = scenes[0].name if scenes else None
    rate, valid, cmeta = load_canvas(cfg, verbose=False)
    phot = cmeta["photometric"]
    wcs = cmeta["wcs"]
    st = cmeta["rate_stats_e_per_s"]
    checks = {
        # 形状与掩膜
        "shape_matches_fits": tuple(rate.shape) == tuple(cmeta["shape"]),
        "valid_is_bool_mask": valid.dtype == bool and valid.shape == rate.shape,
        "rate_all_finite": bool(np.all(np.isfinite(rate))),
        # 一手定标：PHOTFLAM/PHOTPLAM 必须真的从真实头读出且为正
        "photflam_positive_finite": bool(np.isfinite(phot["photflam"]) and phot["photflam"] > 0.0),
        "photplam_positive_finite": bool(np.isfinite(phot["photplam"]) and phot["photplam"] > 0.0),
        # WCS：板比例必须是真实的 0.04 arcsec/px（合成画布可以随便填，必须能区分）
        "wcs_scale_matches_board": abs(wcs["scale_arcsec_per_px"] - CANVAS_ARCSEC_PER_PX) < 1.0e-3,
        "wcs_ctype_tan": wcs["ctype1"].startswith("RA---TAN") and wcs["ctype2"].startswith("DEC--TAN"),
        # 真实星云模板必须有真实结构：亮尾远超中位（合成常数画布过不了这一关）
        "has_real_nebula_structure": bool(st["p99_99"] > 10.0 * max(st["median"], 1e-12)),
        "median_rate_positive": bool(st["median"] > 0.0),
        # 底图噪声：真实 drz 的存活残差必须为正且被如实登记
        "residual_noise_measured_positive":
            bool(cmeta["residual_base_noise_sigma_e_per_s_measured"] > 0.0),
        "residual_fraction_theory_positive":
            bool(cmeta["residual_base_noise_variance_fraction_theory"] > 0.0),
        # provenance 溯源：路径必须回指真实文件。cmeta["path"] 是**相对 ROOT** 的
        # 相对路径，故必须相对 ROOT 解析（不能依赖进程 CWD —— run_selftests.sh 会 cd）。
        "provenance_points_to_real_fits":
            Path(cmeta["path"]).name == src.name and (ROOT / cmeta["path"]).exists(),
    }
    # --- 独立读头交叉核对（不经 load_canvas） ---
    # 只查「是否为正」太弱：把 PHOTFLAM 写死成任意正数也能过。这里绕过 load_canvas，
    # 直接用 astropy 再读一次同一个 FITS 的头，与 cmeta **逐位**比对 —— 这样
    # 「读错关键字 / 写死常数 / 矩阵转置」都会立刻判红，且不需要任何硬编码期望值。
    try:
        from astropy.io import fits as _fits
        with _fits.open(src, memmap=True) as _h:
            _hdr = _h[0].header
            _shape = tuple(np.asarray(_h[0].data).shape)
            _cd_max = max(abs(float(wcs["cd"][i][j]) - float(_hdr["CD%d_%d" % (i + 1, j + 1)]))
                          for i in range(2) for j in range(2))
            checks["hdr_photflam_bit_exact"] = bool(phot["photflam"] == float(_hdr["PHOTFLAM"]))
            checks["hdr_photplam_bit_exact"] = bool(phot["photplam"] == float(_hdr["PHOTPLAM"]))
            checks["hdr_exptime_bit_exact"] = bool(cmeta["real_exptime_s"] == float(_hdr["EXPTIME"]))
            checks["hdr_ndrizim_exact"] = bool(int(cmeta["ndrizim"]) == int(_hdr["NDRIZIM"]))
            checks["hdr_crpix_bit_exact"] = bool(
                wcs["crpix1"] == float(_hdr["CRPIX1"]) and wcs["crpix2"] == float(_hdr["CRPIX2"]))
            checks["hdr_cd_matrix_bit_exact"] = bool(_cd_max == 0.0)
            checks["data_shape_matches_direct_read"] = bool(tuple(rate.shape) == _shape)
            checks["bunit_is_electrons_per_s"] = bool(
                str(cmeta.get("bunit", "")).upper().startswith("ELECTRONS/S"))
        out["independent_header_readback"] = {
            "source": "astropy 直接重读同一 FITS（不经 load_canvas）",
            "photflam_direct": float(_hdr["PHOTFLAM"]),
            "exptime_direct_s": float(_hdr["EXPTIME"]),
            "data_shape_direct": list(_shape),
        }
    except Exception as exc:                       # 读不了真实头 => 判红，不放过
        checks["independent_header_readback_ok"] = False
        out["independent_header_readback"] = {"error": str(exc)}
    out["checks"] = checks
    out["readings"] = {
        "shape": list(rate.shape),
        "valid_fraction": float(valid.mean()),
        "photflam": phot["photflam"], "photplam": phot["photplam"],
        "zp_ab_mag": phot["zp_ab"], "zp_st_mag": phot["zp_st"],
        "scale_arcsec_per_px": wcs["scale_arcsec_per_px"],
        "rate_stats_e_per_s": st,
        "pedestal_e_per_s": cmeta["pedestal_e_per_s"],
        "smooth_sigma_px": cmeta["smooth_sigma_px"],
        # **请求值**（来自传给 load_canvas 的 cfg）：V6 用它核对 load_canvas 实际用的
        # σ_k —— 若生产把 σ 翻倍/改写，cmeta 里的 σ 与请求值就不一致（V10 的判词不依赖
        # 这一项，它只把读数交给 V6 判）。
        "requested_smooth_sigma_px": float(cfg["canvas"].get("smooth_sigma_px", 0.8)),
        "residual_fraction_theory": cmeta["residual_base_noise_variance_fraction_theory"],
        "residual_sigma_meas_e_per_s": cmeta["residual_base_noise_sigma_e_per_s_measured"],
        "raw_sigma_meas_e_per_s": cmeta["raw_pixel_sigma_e_per_s_measured"],
        "removed_sigma_meas_e_per_s": cmeta["removed_by_smoothing_sigma_e_per_s_measured"],
        "mask": cmeta["mask"],
        "real_exptime_s": cmeta["real_exptime_s"], "ndrizim": cmeta["ndrizim"],
        "bunit": cmeta["bunit"],
    }
    out["failed_checks"] = [k for k, v in checks.items() if not v]
    out["verdict"] = bool(all(checks.values()))
    out["criterion"] = ("真实 M16 FITS 经 load_canvas 加载后：形状/掩膜/有限性、PHOTFLAM>0、"
                        "板比例=0.04 arcsec/px、TAN CTYPE、p99.99 > 10×median（真实星云结构）、"
                        "残差噪声>0、provenance 回指真实文件；且未知 band 被拒")
    return out


def _selftest_canvas_smoothing_budget(v10: Dict[str, Any]) -> Dict[str, Any]:
    """V6 —— 画布「存活底图残差预算」的**生产实现**核对（走 ``_smooth_canvas``）。

    G08-04 整改 R2：本条此前是**结构恒真门** —— 它在 ``selftest()`` 里自己
    ``from scipy.ndimage import gaussian_filter``、拿白噪声自测
    ``var(smoothed)/var(white) ≈ 1/(4πσ²)``，**完全不碰** ``load_canvas`` 的生产平滑
    （旧版生产平滑在 ``load_canvas`` 内联）。复核实测注入三处生产缺陷
    （删掉生产平滑 / σ 翻倍 / ``resid_frac_theory`` 分母 ``4π``→``2π``）**3/3 全绿**。
    现在拆成三臂，期望值全部由**测试端**现场算出，不取自生产返回值：

    (a) **白噪声探针走生产实现**：用 ``_smooth_canvas``（生产平滑本体）滤波白噪声，
        要求存活方差份额 = ``1/(4πσ_req²)``（5% 内），且生产声明的 ``resid_frac_theory``
        与测试端闭式**逐位**一致。⇒ 删掉生产平滑（份额变 1.0）、theory 分母写错
        （声明值翻倍）都判红。
    (b) **结构守恒**：同一生产实现作用在解析高斯斑块上，峰值与积分保持不变（±1%）。
    (c) **真实模板交叉核对**：复用 V10 已经读进来的 ``cmeta``（**不重复读 268MB FITS**），
        要求 ``load_canvas`` **实际用的** σ 等于**请求的** σ（⇒ 生产侧把 σ 翻倍判红）、
        真实帧上的 theory 分母与测试端闭式一致、且「被移除的 σ」实测 > 0
        （⇒ 生产平滑根本没跑判红）。

    真实模板缺失时本条**记红**（与 V10 同因，不静默跳过、不设 waiver）。
    """
    out: Dict[str, Any] = {"name": "V6_canvas_smoothing_budget",
                           "production_call": "m16_sampling._smooth_canvas (load_canvas 的平滑段本体)",
                           "readings": {}, "checks": {}, "verdict": False}
    sig_req = float(v10.get("readings", {}).get("requested_smooth_sigma_px", 0.8))
    out["readings"]["requested_sigma_px"] = sig_req
    if not v10.get("template_available", False):
        out["checks"]["real_template_available"] = False
        out["verdict_reason"] = (
            "真实模板缺失（与 V10_real_canvas_load 同因）：存活残差预算的真实路径"
            "无法被核对 ⇒ 记红，不跳过。")
        return out
    out["checks"]["real_template_available"] = True
    # --- (a) 白噪声探针走生产实现 ---
    white = np.random.default_rng(23).normal(0.0, 1.0, size=(512, 512))
    sm, resid_w, frac_prod = _smooth_canvas(white, sig_req)
    frac_meas = float(np.var(sm) / np.var(white))
    frac_test = 1.0 / (4.0 * math.pi * sig_req * sig_req)     # 测试端闭式（期望）
    rel_dev = float(abs(frac_meas - frac_test) / frac_test)
    frac_prod_dev = float(abs(frac_prod - frac_test) / frac_test)
    resid_var_meas = float(np.var(resid_w))
    out["readings"].update({
        "surviving_fraction_measured": frac_meas,
        "surviving_fraction_declared_by_production": float(frac_prod),
        "surviving_fraction_test_closed_form": frac_test,
        "rel_dev_meas_vs_test": rel_dev,
        "rel_dev_prod_declared_vs_test": frac_prod_dev,
        "removed_fraction_measured": float(resid_var_meas / np.var(white)),
    })
    out["checks"]["white_probe_surviving_fraction_matches_test_closed_form"] = bool(rel_dev < 0.05)
    out["checks"]["production_declared_fraction_matches_test_closed_form"] = bool(frac_prod_dev < 1e-12)
    # --- (b) 结构守恒（同一生产实现） ---
    yy, xx = np.mgrid[0:512, 0:512]
    blob = np.exp(-(((yy - 256.0) ** 2 + (xx - 256.0) ** 2) / (2 * 30.0 ** 2)))
    blobs, _rb, _fp = _smooth_canvas(blob, sig_req)
    peak_ratio = float(blobs.max() / blob.max())
    flux_ratio = float(blobs.sum() / blob.sum())
    out["readings"].update({"blob_peak_ratio": peak_ratio, "blob_flux_ratio": flux_ratio})
    out["checks"]["structure_conserved_peak"] = bool(abs(peak_ratio - 1.0) < 0.01)
    out["checks"]["structure_conserved_flux"] = bool(abs(flux_ratio - 1.0) < 0.01)
    # --- (c) 真实模板交叉核对 ---
    r = v10["readings"]
    sig_used = float(r["smooth_sigma_px"])
    frac_real = float(r["residual_fraction_theory"])
    frac_real_test = 1.0 / (4.0 * math.pi * sig_req * sig_req)
    removed = float(r["removed_sigma_meas_e_per_s"])
    resid_sig = float(r["residual_sigma_meas_e_per_s"])
    raw_sig = float(r["raw_sigma_meas_e_per_s"])
    out["readings"].update({
        "real_sigma_used_px": sig_used, "real_sigma_requested_px": sig_req,
        "real_fraction_declared": frac_real,
        "real_fraction_test_closed_form": frac_real_test,
        "real_removed_sigma_e_per_s": removed,
        "real_residual_sigma_e_per_s": resid_sig,
        "real_raw_sigma_e_per_s": raw_sig,
    })
    out["checks"]["real_sigma_used_equals_requested"] = bool(sig_used == sig_req)
    out["checks"]["real_fraction_matches_test_closed_form"] = bool(
        abs(frac_real - frac_real_test) <= 1e-12 * frac_real_test)
    out["checks"]["real_smoothing_removed_noise_measured_positive"] = bool(removed > 0.0)
    out["checks"]["real_surviving_noise_below_raw"] = bool(0.0 < resid_sig < raw_sig)
    out["failed_checks"] = [k for k, v in out["checks"].items() if not v]
    out["verdict"] = bool(all(out["checks"].values()))
    out["verdict_reason"] = (
        "白噪声存活份额 %.6g vs 测试端闭式 %.6g（rel_dev %.3g）；生产声明份额 rel_dev %.3g；"
        "真实帧 σ 用/请求 %g/%g px，移除 σ 实测 %.6g e-/s，存活 σ/原始 σ = %.6g"
        % (frac_meas, frac_test, rel_dev, frac_prod_dev, sig_used, sig_req,
           removed, resid_sig / raw_sig if raw_sig else float("nan")))
    return out


def selftest(verbose: bool = True) -> Dict[str, Any]:
    """噪声物理 + 采样几何 + 底图残差 + 负例（纯加性）+ **真实 M16 模板加载**自检。

    G08-04 整改 T5：V1–V9 用 ``_fake_canvas`` 合成画布跑（快、几何可控），
    但它们**不覆盖** ``load_canvas`` 的真实 FITS 路径；V10 补该覆盖。
    真实模板缺失时 V10 记红（不静默跳过，见 ``_selftest_real_canvas`` docstring）。

    G08-04 整改 R2：V6 此前在 ``selftest`` 内**另算一份** scipy 滤波来核对
    1/(4πσ²)（零生产耦合的恒真门）；现改为走生产实现 ``_smooth_canvas``，并复用
    V10 已载入的真实模板读数做「实际 σ = 请求 σ」「被移除 σ 实测 > 0」的交叉核对。
    """
    res: Dict[str, Any] = {"criteria": {}, "verdicts": {}}
    shape = (128, 128)
    det = NM.Detector(gain_e_per_adu=1.5, read_noise_e=5.0, bias_adu=1000.0,
                      dark_current_e_per_s=0.0)
    # V1 天光 Poisson 单调：B 升 => sigma 升（同源同种子）
    rows = []
    for B in (0.5, 5.0, 50.0):
        f = NM.expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, B), det=det,
                      exptime_s=100.0, rng=np.random.default_rng(7))
        rows.append({"sky_e_per_s": B, "sigma_adu": float(NM.robust_sigma_adu(f.adu))})
    res["criteria"]["sky_series"] = rows
    res["verdicts"]["V1_sky_poisson_monotone"] = bool(
        all(rows[i + 1]["sigma_adu"] > rows[i]["sigma_adu"] for i in range(len(rows) - 1)))
    # V2 纯加性负例：配对同种子 + 常数 => sigma 逐位不变（判据必须归零）
    a = NM.expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, 10.0), det=det,
                  exptime_s=100.0, rng=np.random.default_rng(3)).adu
    b = a + 500.0
    rel = abs(NM.robust_sigma_adu(b) - NM.robust_sigma_adu(a)) / NM.robust_sigma_adu(a)
    res["criteria"]["additive_sigma_rel_change"] = float(rel)
    res["verdicts"]["V2_additive_negative_control_zero"] = bool(rel < 1e-12)
    # V3 平场乘性：用**无噪声臂**（mean_only）测乘性 —— adu-bias 必须逐像素 ∝ m
    m = NM.flat_response(shape, np.random.default_rng(5), prnu_rms=0.0, low_order=0.2,
                         tilt_x=1.0, tilt_y=0.0, vignette=0.0)
    det_exact = NM.Detector(gain_e_per_adu=1.5, read_noise_e=0.0, bias_adu=1000.0,
                            full_well_e=1e12, dark_current_e_per_s=0.0,
                            quantize=False, saturate=False)
    f2 = NM.expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, 10.0), det=det_exact,
                   exptime_s=100.0, rng=np.random.default_rng(11), flat=m,
                   mode=NM.MODE_MEAN_ONLY)
    r = (f2.adu - det_exact.bias_adu) / m
    res["criteria"]["flat_demodulation_rel_spread"] = float(np.std(r) / np.mean(r))
    res["criteria"]["flat_profile_pk_pct"] = float(np.max(np.abs(m / np.median(m) - 1.0)))
    res["verdicts"]["V3_flat_is_multiplicative"] = bool(np.std(r) / np.mean(r) < 1e-12)
    # V3b 反例：**加性**天光在同样的去平场操作下**不**被解调（判据必须能红）
    f2b = NM.expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, 10.0), det=det_exact,
                    exptime_s=100.0, rng=np.random.default_rng(11), mode=NM.MODE_MEAN_ONLY)
    rb = (f2b.adu + 200.0 - det_exact.bias_adu) / m   # 加性臂：去平场后仍带 200 ADU 的常数
    res["criteria"]["additive_demodulation_rel_spread"] = float(np.std(rb) / np.mean(rb))
    res["verdicts"]["V3b_additive_fails_demodulation"] = bool(
        np.std(rb) / np.mean(rb) > 1e-3)
    # V3c 带噪声/量化的真实臂（如实登记量级，不作判据）
    f2c = NM.expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, 10.0), det=det,
                    exptime_s=100.0, rng=np.random.default_rng(11), flat=m,
                    mode=NM.MODE_MEAN_ONLY)
    rc = (f2c.adu - det.bias_adu) / m
    res["criteria"]["flat_demodulation_rel_spread_noisy_quantized"] = float(
        np.std(rc) / np.mean(rc))
    # V4 采样几何（**共切点**画布：切点落在帧中心）=> 采样映射必须是**精确**仿射
    cmeta = {"wcs": {"crval1": 274.7, "crval2": -13.84, "crpix1": 231.5, "crpix2": 131.5,
                     "cd": [[-9.1016831619991e-06, -6.3731918728008e-06],
                            [-6.3730803948928e-06, 9.10186123591136e-06]],
                     "ctype1": "RA---TAN", "ctype2": "DEC--TAN",
                     "scale_arcsec_per_px": 0.04}}
    cmeta_far = {"wcs": dict(cmeta["wcs"], crpix1=4001.0, crpix2=4201.0)}
    pt = {"y": 100.0, "x": 200.0, "dy": 0.0, "dx": 0.0}
    c0 = pointing_to_center(pt, (64, 64), 1.0, 0.0)
    _fw0, h0 = frame_wcs_from_center(cmeta, (64, 64), 1.0, 0.0, c0)
    coords = detector_to_canvas(h0, cmeta, (64, 64))
    dy = coords[0, 1, 0] - coords[0, 0, 0]
    dx = coords[1, 0, 1] - coords[1, 0, 0]
    offy = coords[0, 0, 0] - 100.0
    offx = coords[1, 0, 0] - 200.0
    res["criteria"]["identity_map"] = {"dy_per_row": float(dy), "dx_per_col": float(dx),
                                       "off_y": float(offy), "off_x": float(offx)}
    res["criteria"]["identity_map"]["note"] = (
        "容差 1e-4 px：astropy all_pix2world 与本模块解析 TAN 的浮点往返差 ~1e-10 deg；"
        "两者互为独立实现，此处一致性本身即交叉验证")
    res["verdicts"]["V4_identity_sampling_geometry"] = bool(
        abs(dy - 1.0) < 1e-4 and abs(dx - 1.0) < 1e-4
        and abs(offy) < 1e-4 and abs(offx) < 1e-4)
    # V4b 非共切点（= 真实 M16 全帧头：切点距该帧 ~4000 px）：偏差**小但非零**（TAN 差动畸变）
    _fwf, hf = frame_wcs_from_center(cmeta_far, (64, 64), 1.0, 0.0, c0)
    cof = detector_to_canvas(hf, cmeta_far, (64, 64))
    res["criteria"]["far_tangent_deviation"] = {
        "dy_per_row_minus_1": float(cof[0, 1, 0] - cof[0, 0, 0] - 1.0),
        "off_y": float(cof[0, 0, 0] - 100.0), "off_x": float(cof[1, 0, 0] - 200.0),
        "lever_arm_px": 4000.0,
        "note": "非共切点 TAN 差动畸变（~theta^2 * lever arm）；生成与落盘 WCS 逐像素一致，"
                "该偏差是**真实几何**而非实现误差"}
    res["verdicts"]["V4b_far_tangent_deviation_small"] = bool(
        abs(res["criteria"]["far_tangent_deviation"]["off_y"]) < 0.05
        and abs(res["criteria"]["far_tangent_deviation"]["off_x"]) < 0.05)
    # V5 像素尺度/旋转：scale=2 时步长=2；rot=90 时行列互换
    c2 = pointing_to_center(pt, (64, 64), 2.0, 0.0)
    _fw2, h2 = frame_wcs_from_center(cmeta, (64, 64), 2.0, 0.0, c2)
    co2 = detector_to_canvas(h2, cmeta, (64, 64))
    c3 = pointing_to_center(pt, (64, 64), 1.0, 90.0)
    _fw3, h3 = frame_wcs_from_center(cmeta, (64, 64), 1.0, 90.0, c3)
    co3 = detector_to_canvas(h3, cmeta, (64, 64))
    res["criteria"]["scale2_step"] = [float(co2[0, 1, 0] - co2[0, 0, 0]),
                                      float(co2[1, 0, 1] - co2[1, 0, 0])]
    def _jac_cr(co):
        # (cx, cy) 对 (col, row) 的 Jacobian（与 detector_to_canvas 的 CD 作用顺序一致）
        return np.array([[float(co[1, 0, 1] - co[1, 0, 0]), float(co[1, 1, 0] - co[1, 0, 0])],
                         [float(co[0, 0, 1] - co[0, 0, 0]), float(co[0, 1, 0] - co[0, 0, 0])]])
    cd_c = np.array(cmeta["wcs"]["cd"], dtype=float)
    inv_c = np.linalg.inv(cd_c)
    # 期望 Jacobian = inv(CD_c) @ (s*R(theta)) @ CD_c（**不是**纯 R：CD 本身带 ~2e-5 各向异性）
    exp90 = inv_c @ _rot(90.0) @ cd_c
    exp2 = inv_c @ (2.0 * _rot(0.0)) @ cd_c
    jac90 = _jac_cr(co3)
    jac2 = _jac_cr(co2)
    res["criteria"]["rot90_matrix"] = jac90.tolist()
    res["criteria"]["rot90_expected"] = exp90.tolist()
    res["criteria"]["rot90_max_dev"] = float(np.abs(jac90 - exp90).max())
    res["criteria"]["scale2_jacobian_max_dev"] = float(np.abs(jac2 - exp2).max())
    res["criteria"]["rot90_note"] = ("CD 矩阵各向异性 ~2e-5（CD1_1 与 CD2_2 不等）=> "
                                     "inv(CD)@R@CD 不是纯旋转；判据比对**期望 Jacobian** 而非单位阵")
    res["verdicts"]["V5_scale_and_rotation"] = bool(
        abs(res["criteria"]["scale2_step"][0] - 2.0) < 1e-6
        and abs(res["criteria"]["scale2_step"][1] - 2.0) < 1e-6
        and res["criteria"]["rot90_max_dev"] < 1e-4
        and res["criteria"]["scale2_jacobian_max_dev"] < 1e-4)
    # --- 真实模板先载入：V6 的「真实路径」臂复用这里的读数（不重复读 268MB FITS） ---
    v10 = _selftest_real_canvas()
    # V6 画布平滑：**存活**噪声方差份额必须 = Σg² = 1/(4*pi*sigma_k^2)（结构守恒）。
    # G08-04 整改 R2：旧版在 selftest 内**自己** import scipy、用写死的 sk=0.8 **另算
    # 一份**来核对 1/(4πσ²) —— 纯「验 scipy」，零生产耦合。实测删掉生产平滑 / σ 翻倍 /
    # theory 分母 4π→2π 三个缺陷，V6 **3/3 全绿**。本版接上生产路径 `_smooth_canvas`
    # （load_canvas 的平滑段本体），并用 V10 已载入的真实模板读数做交叉核对。
    v6 = _selftest_canvas_smoothing_budget(v10)
    res["criteria"]["canvas_smoothing"] = v6
    res["verdicts"]["V6_canvas_smoothing_budget"] = bool(v6["verdict"])
    # V7 **端到端采样渲染**（合成画布，不读真实帧）：天光 Poisson 通道 + 抖动几何 + 加性负例
    v7 = _selftest_render_end_to_end()
    res["criteria"]["end_to_end_render"] = v7
    res["verdicts"]["V7_end_to_end_sky_poisson_and_dither"] = bool(
        v7["sigma_ratio_high_over_low"] > 1.5 and v7["dither_shift_px_err"] < 0.02
        and v7["additive_arm_sigma_rel_change"] < 1e-12)
    # V8 seeing 单位（探测器 px vs 画布 px）——曾把 *s 写成 /s 使 seeing 旋钮失效
    res["verdicts"]["V8_seeing_unit_on_detector_grid"] = bool(
        not v7["seeing_floor_reached"] and v7["seeing_rel_err"] < 0.10)
    # V9 通量守恒（面亮度 -> 探测器像素计数）：常数画布 -> 采样率必须 = 画布值 * s^2
    rate9 = np.full((96, 96), 0.37)
    cmeta9 = {"band": "SYNTH", "line": "selftest", "path": "selftest://flux",
              "shape": [96, 96], "bunit": "ELECTRONS/S", "real_exptime_s": 100.0, "ndrizim": 1,
              "pedestal_e_per_s": 0.0, "smooth_sigma_px": 0.0,
              "residual_base_noise_sigma_e_per_s_measured": 0.0,
              "residual_base_noise_variance_fraction_theory": 1.0,
              "wcs": dict(cmeta["wcs"], crpix1=48.5, crpix2=48.5)}
    canvas9 = {"rate": rate9, "valid": np.ones((96, 96), dtype=bool),
               "meta": cmeta9, "base_psf": {"fwhm_px": 2.0, "n_stars": 0, "method": "selftest"}}
    base9 = copy.deepcopy(SAMPLING_DEFAULTS)
    base9["scene_id"] = "selftest_flux"
    base9["shape"] = [24, 24]
    base9["canvas"] = {"smooth_sigma_px": 0.0}
    base9["sampling"] = {"pixel_scale_arcsec": 0.1, "rotation_deg": 0.0,
                         "base_fwhm_px": 2.0, "resample_order": 3}
    base9["psf"] = {"model": "moffat4", "fwhm_px": 3.2, "beta": 4.0}
    base9["detector"] = {"gain_e_per_adu": 1.5, "read_noise_e": 0.0, "bias_adu": 1000.0,
                         "full_well_e": 1e9, "dark_current_e_per_s": 0.0}
    base9["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "vignette": 0.0, "seed": 1}
    base9["sky"] = {"level_e_per_s": 0.0}
    base9["frames"] = [{"frame_id": "flux", "exposure_s": 1.0, "seed": 5,
                        "pointing": {"y": 0, "x": 0, "dy": 0.0, "dx": 0.0},
                        "psf": {"fwhm_px": 3.2}, "sky": {"level_e_per_s": 0.0}}]
    _f9, t9, _v9, sig9 = render_sampling_frame(base9, canvas9, frame_index=0)
    ratio9 = float(np.median(sig9) / 0.37)
    res["criteria"]["flux_conservation"] = {
        "s": float(t9["sampling"]["scale_factor"]), "expected_ratio": float(
            t9["sampling"]["scale_factor"] ** 2), "measured_ratio": ratio9,
        "rel_err": float(abs(ratio9 - t9["sampling"]["scale_factor"] ** 2)
                         / t9["sampling"]["scale_factor"] ** 2)}
    res["verdicts"]["V9_flux_conservation_over_pixel_area"] = bool(
        res["criteria"]["flux_conservation"]["rel_err"] < 1e-3)
    # V10 **真实 M16 FITS 加载路径**（G08-04 T5）：此前自检从不调 load_canvas，
    #     对真实模板恒真。模板缺失时本条记红（不静默跳过、不设 waiver 开关）。
    #     载入动作在 V6 之前执行（见上），此处只落读数与判词。
    res["criteria"]["real_canvas_load"] = v10
    res["verdicts"]["V10_real_canvas_load"] = bool(v10["verdict"])
    res["all_pass"] = bool(all(res["verdicts"].values()))
    if verbose:
        print(json.dumps(res, indent=2, ensure_ascii=False, default=float))
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description="M16 真实信号 -> 仿真采样帧 生成器")
    ap.add_argument("--scene", type=str)
    ap.add_argument("--out", type=str)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--list-scenes", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest()["all_pass"] else 1
    if a.list_scenes:
        for s in list_scenes():
            print(s)
        return 0
    if not a.scene or not a.out:
        ap.error("--scene and --out are required (or use --list-scenes/--selftest)")
    render_sampling_dataset(a.scene, a.out, seed=a.seed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())