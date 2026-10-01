#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACSD RELEASE-02 / M16-SCENE —— 哈勃 M16 场景模板的**前向渲染接口**。

定位（与 DATA-TYPE-MATRIX 的分工，**不另起炉灶**）
-----------------------------------------------
* 物理噪声链**复用** 实验/shared/synthetic/noise_model.py（NM.expose，唯一事实源）；
* PSF 核 / 星点叠加**复用** 实验/shared/synthetic/render.py（R.psf_kernel / R.stamp）；
* 本模块只补 M16 特有的三件事：
    1) 真实 drz 帧 -> **期望率面**（掩膜 + 去噪 + 基座分离）；
    2) **PHOTFLAM 一手绝对标定** -> 真实星等尺度（AB / ST 零点），
       使注入星点与真实结构处在同一测光坐标系；**不做任何物理闭合反推**（§9.42）；
    3) 掩膜随帧落盘（MASK/VALID 扩展 HDU），供下游剔除。

为什么 M16 三帧只能当**场景模板**、不能当「原始帧」
-----------------------------------------------
三帧都是 drizzle 合成品（头里 NDRIZIM=32，D001OUUN=cps，BUNIT=ELECTRONS/S）：
  * 噪声已被 32 次曝光 + drizzle 重采样**压低并相关化**（像素间相关长度 ~ drizzle 核）；
  * 因此**不是**独立泊松样本，直接拿来做噪声统计会**低估**方差、并给出错误的
    「噪声 vs 天光」标度关系；
  * 但它的**结构**（真实星场 + 真实星云 + HST 分辨率）是理想底图。
=> 本接口把真实帧**去噪成期望面**，再按 §9.41 重新生成物理噪声。

单位与「速率 vs 计数」（关键）
-----------------------------
BUNIT = ELECTRONS/S => 真实帧像素值是**速率** r [e-/s]（EXPTIME 已除掉）。
合成时：电子数 = r * exposure_s，**然后**才进泊松。
因此同一场景换曝光时间时，源/天光的**率**不变、**计数**随 t 线性变、噪声随 sqrt(t) 变。
真实帧的 EXPTIME（F657N 9600 / F673N 14400 / F502N 16000 s）只用于
**解释**底图速率（并作为 sky 基座的来源），**不**等于合成曝光时间。

物理噪声链（§9.41，逐项可配置）
------------------------------
    lam_e  = t * (src_rate + sky_rate) * m(x,y)  +  t * D(T) * hot(x,y)   [e-]
    n_e    = Poisson(lam_e) + Normal(0, sigma_R * sqrt(stack_n)) + CR      [e-]
    adu    = round(n_e / g + bias)  ，硬钳位到满阱                              [ADU]
    invalid 像素 -> 0（与真实 drz 的零填充一致），并写入 MASK 扩展
其中 m(x,y) 为乘性平场响应（PRNU + 低阶空间项 + 渐晕），sky(x,y) 为可含梯度的天光面。
**严禁**纯加性天光：天光进泊松，B↑ 必然 sigma↑（noise_model 的 mode="additive" 臂
作为**显式负例**保留，用于证明判据「真值无效应时归零」）。

PHOTFLAM 星等尺度（§9.42 合规用法）
----------------------------------
头里的 PHOTFLAM/PHOTPLAM 是**一手绝对流量标定**（STScI 发布，非本仓反推）：

    f_lambda = PHOTFLAM * r          [erg cm^-2 s^-1 A^-1]，r 为源速率 [e-/s]
    ZP_ST    = -2.5*log10(PHOTFLAM) + PHOTZPT        （PHOTZPT = -21.10，头内自洽校验）
    ZP_AB    = -2.5*log10(PHOTFLAM) - 5*log10(PHOTPLAM) - 2.408
    m_AB     = -2.5*log10(r) + ZP_AB

=> 星等尺度**直接来自头部**，不需要（也不允许）任何「增益 x 口径 x 曝光」闭合式。

诚实边界（drz 合成品在噪声建模上的固有局限，**不得省略**）
--------------------------------------------------------
1. **像素间噪声相关**：真实 drz 的噪声经 drizzle 重采样后是**相关**的；本接口生成的是
   **逐像素独立**泊松/高斯噪声。=> 合成帧的噪声**功率谱**与真实 drz 不同；
   凡对**相关长度**敏感的判据（如稀疏 SNR 控制点间距、UPM 平滑尺度）不得直接用合成帧定标。
2. **底图残余噪声**：真实帧被高斯平滑成期望面，残余方差份额 = 1/(4*pi*sigma_k^2)
   （逐帧登记）。该残差**不是**白噪声，且**未**计入合成帧。
3. **底图 PSF 展宽**：平滑会同时展宽真实星点 => 底图特征有效 PSF = sqrt(PSF_real^2 + sigma_k^2)；
   **注入星点用配置 PSF**（不受平滑影响）=> 同一帧里两套 PSF，逐帧登记。
4. **无权重/无 DQ**：只有 SCI HDU，没有 _wht / DQ => 覆盖信息只能由零值几何反推；
   本接口对无效像素的期望面用**最近有效邻元**填充（避免 0 值在平滑时把边缘拉黑），
   但输出帧在这些位置写 0 并置 MASK=0。
5. **底图自身的天光已含在基座里**：默认 sky 基座 = 真实帧的 p(subtract_percentile)
   => 源结构（星云 + 星）通量守恒、星等尺度不变；若显式改 sky 水平，则是在模拟
   「不同天光条件下的同一天区」，此时星云弥散分量的一部分被重新归类为天光（**定义性选择**）。
6. 未建模：溢出/辉散、非线性、CTE、fringing、云、导星漂移、drizzle 相关噪声。

场景配方字段（M16 扩展；其余同 render.py）
----------------------------------------
  renderer          "m16_scene"（data/synthetic/generate.py 的派发键）
  real_base         {band, crop:[y0,x0,h,w], mask_dir, smooth_sigma_px,
                     subtract_percentile, flux_scale, exclude_spike, invalid_fill}
  photometric       {zeropoint:"ab"|"st", flux_scale, anchor:{...}|null}
  inject_stars      {n, mode:"abmag"|"eflux", mag_range, eflux_log10_range, seed,
                     avoid_bright_base, avoid_k_sigma, min_separation_px,
                     positions:[[y,x],...] (显式位置，供 A6 类实验)}
  stack_n           等效子曝光数（只把读出噪声放大 sqrt(n)；泊松总和不变）
                    默认 32 = 真实 drz 的 NDRIZIM（读出噪声 sqrt(32)*3.1 = 17.536 e-）
  sky               {mode:"from_real"|"explicit", level_e_per_s, grad_*, moon_*}

CLI
---
    export TMPDIR=/dev/shm/astrocs_m16
    python3 m16_scene.py --list-scenes
    python3 m16_scene.py --scene scenes/m16_nebula_core.json \
        --out ../../../run/reverse_verify/m16_scene/frames --seed 20260924
    python3 m16_scene.py --selftest      # 噪声链 + 星等尺度 + 掩膜传播 自检
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]                      # 仓库根
sys.path.insert(0, str(HERE))
import noise_model as NM          # noqa: E402  物理噪声链（唯一事实源）
import render as R                # noqa: E402  PSF 核 / 星点叠加 / 场景 IO
import m16_mask as MK             # noqa: E402  有效域掩膜

SCENES_DIR = HERE / "scenes"
DEFAULT_MASK_DIR = "run/reverse_verify/m16_scene/masks"
PIXSCALE_ARCSEC = 0.04            # 板比例（头里 D001SCAL=0.04 / IDCSCALE=0.03962）
AB_ZP_CONST = 2.408               # ZP_AB = -2.5log10(PHOTFLAM) - 5log10(PHOTPLAM) - 2.408
C_LIGHT_ANGSTROM_PER_S = 2.99792458e18


# ---------------------------------------------------------------------------
# 0. 光度学：PHOTFLAM -> 星等尺度（一手标定，无物理闭合）
# ---------------------------------------------------------------------------
def photometric_scale(photflam: float, photplam: float,
                      photzpt: Optional[float] = None) -> Dict[str, Any]:
    """由头里的 PHOTFLAM/PHOTPLAM 给出 AB/ST 零点与单位换算（纯定义式）。"""
    if not (photflam > 0) or not (photplam > 0):
        raise ValueError("PHOTFLAM/PHOTPLAM must be positive")
    zp_st = -2.5 * math.log10(photflam) + (photzpt if photzpt is not None else -21.10)
    zp_ab = -2.5 * math.log10(photflam) - 5.0 * math.log10(photplam) - AB_ZP_CONST
    out = {
        "photflam": photflam, "photplam": photplam, "photzpt": photzpt,
        "zp_ab": zp_ab, "zp_st": zp_st,
        # ST->AB 的独立换算（与 zp_ab 互校）：m_AB = m_ST - 5log10(lambda) + 2.5log10(c) - 27.50
        "zp_ab_from_st": (zp_st - 5.0 * math.log10(photplam)
                          + 2.5 * math.log10(C_LIGHT_ANGSTROM_PER_S) - 27.50),
        "e_per_s_for_ab_20": 10.0 ** (-0.4 * (20.0 - zp_ab)),
        "e_per_s_for_ab_25": 10.0 ** (-0.4 * (25.0 - zp_ab)),
        "formula": "m_AB = -2.5*log10(rate_e_per_s) + ZP_AB; "
                   "ZP_AB = -2.5log10(PHOTFLAM) - 5log10(PHOTPLAM) - 2.408",
        "note": "PHOTFLAM/PHOTPLAM 来自 FITS 头（STScI 一手绝对流量标定）；"
                "本条**不含**任何增益/口径/曝光的物理闭合反推（GAP_AUDIT 9.42）",
    }
    return out


def rate_to_ab_mag(rate_e_per_s, zp_ab: float):
    r = np.asarray(rate_e_per_s, dtype=float)
    with np.errstate(divide="ignore", invalid="ignore"):
        m = -2.5 * np.log10(np.where(r > 0, r, np.nan)) + zp_ab
    return m if m.ndim else float(m)


def ab_mag_to_rate_e_per_s(mag, zp_ab: float):
    return 10.0 ** (-0.4 * (np.asarray(mag, dtype=float) - zp_ab))


def surface_brightness_mag_per_arcsec2(rate_e_per_s_per_px, zp_ab: float,
                                       pixscale_arcsec: float = PIXSCALE_ARCSEC):
    """点源速率 -> 面亮度 [mag/arcsec^2]（每像素立体角 = pixscale^2）。"""
    r = np.asarray(rate_e_per_s_per_px, dtype=float)
    with np.errstate(divide="ignore", invalid="ignore"):
        m = -2.5 * np.log10(np.where(r > 0, r, np.nan) / (pixscale_arcsec ** 2)) + zp_ab
    return m if m.ndim else float(m)


# ---------------------------------------------------------------------------
# 1. 默认配置
# ---------------------------------------------------------------------------
M16_DEFAULTS: Dict[str, Any] = {
    "renderer": "m16_scene",
    "shape": [1024, 1024],
    "exposure_s": 9600.0,
    "temp_c": -20.0,
    "stack_n": 32,
    "psf": {"model": "moffat4", "fwhm_px": 2.0, "beta": 4.0},
    # 探测器：默认取 WFC3/UVIS 头部公开工程值（CCDGAIN/ATODGN/READNSE），
    # 作为**可配置场景参数**使用，不用于反推任何仪器量。
    "detector": {"gain_e_per_adu": 1.5, "read_noise_e": 3.1, "bias_adu": 1000.0,
                 # full_well = NDRIZIM(32) * 7e4 e-（叠加满阱，见 scenes/*.json 的 calibration 块）
                 "full_well_e": 2240000.0, "dark_current_e_per_s": 0.002,
                 "dark_ref_temp_c": -20.0, "dark_double_temp_c": 6.0},
    "flat": {"prnu_rms": 0.01, "low_order": 0.02, "tilt_x": 1.0, "tilt_y": 0.5,
             "vignette": 0.05},
    "sky": {"mode": "from_real"},
    "photometric": {"zeropoint": "ab", "flux_scale": 1.0, "anchor": None},
    "real_base": {"band": "F657N", "crop": [5504, 896, 1024, 1024],
                  "mask_dir": DEFAULT_MASK_DIR, "smooth_sigma_px": 2.0,
                  "subtract_percentile": 5.0, "flux_scale": 1.0,
                  "exclude_spike": False, "invalid_fill": "nearest"},
    "inject_stars": {"n": 0, "mode": "abmag", "mag_range": [18.0, 24.0],
                     "eflux_log10_range": [2.0, 5.0], "seed": 1616,
                     "clustering": "uniform", "avoid_bright_base": True,
                     "avoid_k_sigma": 5.0, "min_separation_px": 12.0,
                     "positions": None},
    "artifacts": {"cr_rate_per_frame": 0.0, "cr_mean_charge_e": 1000.0},
    "mode": NM.MODE_PHYSICAL,
    "frames": None,
}


def load_scene(path) -> Dict[str, Any]:
    p = Path(path)
    if not p.is_absolute():
        cand = [Path.cwd() / p, SCENES_DIR / p.name]
        p = next((c for c in cand if c.exists()), cand[0])
    raw = json.loads(p.read_text(encoding="utf-8"))
    return R.deep_merge(M16_DEFAULTS, raw)


# ---------------------------------------------------------------------------
# 2. 真实底 -> 期望率面（掩膜 + 去噪 + 基座）
# ---------------------------------------------------------------------------
def _fill_nearest(arr: np.ndarray, valid: np.ndarray) -> np.ndarray:
    """用最近有效邻元填充无效像素（距离变换，确定性）。"""
    from scipy.ndimage import distance_transform_edt
    if valid.all():
        return arr.copy()
    idx = distance_transform_edt(~valid, return_distances=False, return_indices=True)
    return arr[tuple(idx)]


def load_real_base(rb: Dict[str, Any], shape: Tuple[int, int],
                   *, mask_dir: Optional[str] = None, verbose: bool = False
                   ) -> Tuple[np.ndarray, np.ndarray, Dict[str, Any]]:
    """读 M16 真实帧的裁剪块 -> (期望源率面 [e-/s], 有效掩膜 bool, meta)。

    期望面 = gaussian(sci, sigma_k) - pedestal，**不含**天光（天光由场景 sky 控制）；
    pedestal = percentile(sci[valid], subtract_percentile)，默认由 sky.mode="from_real" 加回。
    """
    from astropy.io import fits
    from scipy.ndimage import gaussian_filter
    band = str(rb["band"])
    if band not in MK.BANDS:
        raise ValueError("unknown band %r; expected one of %s" % (band, list(MK.BANDS)))
    src = MK.ROOT / "testdata" / "HST_M16" / MK.BANDS[band]["file"]
    if not src.exists():
        raise FileNotFoundError("M16 frame not found: %s" % src)
    md = Path(mask_dir or rb.get("mask_dir") or DEFAULT_MASK_DIR)
    if not md.is_absolute():
        md = ROOT / md
    y0, x0, h, w = [int(v) for v in rb["crop"]]
    ny, nx = shape
    if (h, w) != (ny, nx):
        raise ValueError("real_base.crop size %s != shape %s" % ((h, w), (ny, nx)))
    with fits.open(src, memmap=True) as f:
        hdr = f[0].header
        sci = np.asarray(f[0].data[y0:y0 + h, x0:x0 + w], dtype=np.float32)
    valid = np.ones((h, w), dtype=bool)
    mask_meta: Dict[str, Any] = {"used": False}
    try:
        vm, mask_meta_full = MK.load_valid_mask(band, md)
        valid = vm[y0:y0 + h, x0:x0 + w].astype(bool)
        mask_meta = {"used": True, "mask_dir": str(md), "band": band,
                     "valid_fraction_in_crop": float(valid.mean()),
                     "mask_valid_fraction_full": mask_meta_full["valid_fraction"]}
        if rb.get("exclude_spike"):
            with fits.open(md / ("%s_flags.fits" % band), memmap=True) as f:
                fl = np.asarray(f[0].data[y0:y0 + h, x0:x0 + w])
            n_sp = int(np.count_nonzero(fl & MK.F_SPIKE))
            valid = valid & ((fl & MK.F_SPIKE) == 0)
            mask_meta["exclude_spike"] = True
            mask_meta["spike_pixels_excluded"] = n_sp
    except FileNotFoundError as exc:
        mask_meta = {"used": False, "reason": str(exc)}
        if verbose:
            print("[m16_scene] WARNING: mask not found (%s) -> all pixels treated valid" % exc)

    phot = photometric_scale(float(hdr["PHOTFLAM"]), float(hdr["PHOTPLAM"]),
                             float(hdr.get("PHOTZPT", -21.10)))
    # --- 期望面 ---
    fill = _fill_nearest(sci.astype(np.float64), valid)
    pct = float(rb.get("subtract_percentile", 5.0))
    ped = float(np.percentile(fill[valid], pct))
    sig_k = float(rb.get("smooth_sigma_px", 2.0))
    if sig_k > 0:
        base = gaussian_filter(fill, sigma=sig_k, mode="nearest")
        resid_frac = 1.0 / (4.0 * math.pi * sig_k * sig_k)
    else:
        base = fill
        resid_frac = 1.0
    rate = (base - ped) * float(rb.get("flux_scale", 1.0))
    rate = np.where(valid, rate, 0.0)
    vv = sci[valid]
    meta: Dict[str, Any] = {
        "band": band, "line": MK.BANDS[band]["line"], "path": str(src.relative_to(ROOT)),
        "crop": [y0, x0, h, w], "hdu": 0,
        "header_subset": {k: hdr.get(k) for k in
                          ("TELESCOP", "INSTRUME", "DETECTOR", "FILTER", "APERTURE",
                           "TARGNAME", "PROPOSID", "EXPTIME", "TEXPTIME", "NDRIZIM",
                           "BUNIT", "PHOTFLAM", "PHOTPLAM", "PHOTZPT", "PHOTMODE",
                           "CCDGAIN", "ATODGNA", "READNSEA", "CRVAL1", "CRVAL2",
                           "CD1_1", "CD2_2", "ORIENTAT", "D001SCAL", "CAL_VER")
                          if k in hdr},
        "real_exptime_s": float(hdr.get("EXPTIME", 0.0)),
        "bunit": hdr.get("BUNIT"), "ndrizim": int(hdr.get("NDRIZIM", 0)),
        "photometric": phot,
        "mask": mask_meta,
        "pedestal_e_per_s": ped, "subtract_percentile": pct,
        "smooth_sigma_px": sig_k,
        "residual_base_noise_variance_fraction": resid_frac,
        "effective_base_psf_note":
            "底图特征有效 PSF = sqrt(PSF_real^2 + smooth_sigma^2)；注入星点用配置 PSF",
        "crop_stats_e_per_s": {
            "valid_fraction": float(valid.mean()),
            "median": float(np.median(vv)), "mean": float(vv.mean()),
            "p1": float(np.percentile(vv, 1)), "p99": float(np.percentile(vv, 99)),
            "p99.9": float(np.percentile(vv, 99.9)), "max": float(vv.max())},
        "crop_mag_arcsec2": {
            "median": surface_brightness_mag_per_arcsec2(float(np.median(vv)), phot["zp_ab"]),
            "p99": surface_brightness_mag_per_arcsec2(float(np.percentile(vv, 99)),
                                                      phot["zp_ab"]),
            "p99.9": surface_brightness_mag_per_arcsec2(float(np.percentile(vv, 99.9)),
                                                        phot["zp_ab"])},
        "negative_rate_fraction_after_pedestal": float(np.mean(rate[valid] < 0)),
        "units_note": "真实帧 BUNIT=ELECTRONS/S => 像素值是速率；合成时乘 exposure_s 得电子数",
    }
    return rate, valid, meta


# ---------------------------------------------------------------------------
# 3. 星表注入（真实星等尺度）
# ---------------------------------------------------------------------------
def make_star_catalog(sc: Dict[str, Any], shape: Tuple[int, int],
                      zp_ab: float, exposure_s: float,
                      base_rate: np.ndarray, valid: np.ndarray,
                      rng: np.random.Generator) -> Tuple[List[Dict[str, float]], Dict[str, Any]]:
    """生成注入星表。flux_e = 曝光积分后的总电子数（与 render.py 约定一致）。"""
    cfg = sc.get("inject_stars", {}) or {}
    n = int(cfg.get("n", 0))
    ny, nx = shape
    info: Dict[str, Any] = {"requested": n, "mode": cfg.get("mode", "abmag")}
    pos = cfg.get("positions")
    fixed_flux = None
    if pos is not None:
        ys = np.array([float(p[0]) for p in pos]); xs = np.array([float(p[1]) for p in pos])
        n = len(pos)
        info["mode"] = "explicit_positions"
        if len(pos[0]) >= 3:      # [y, x, flux_e]：显式真值通量（跨臂固定，供 A6 类实验）
            fixed_flux = np.array([float(p[2]) for p in pos])
            info["mode"] = "explicit_positions_and_flux"
    elif n > 0:
        ys, xs = [], []
        guard = 0
        k = float(cfg.get("avoid_k_sigma", 5.0))
        rmin = float(cfg.get("min_separation_px", 0.0))
        bmed = float(np.median(base_rate[valid])) if valid.any() else 0.0
        bsig = 1.4826 * float(np.median(np.abs(base_rate[valid] - bmed))) if valid.any() else 1.0
        thr = bmed + k * max(bsig, 1e-12)
        while len(ys) < n and guard < 200 * max(n, 1):
            guard += 1
            y = float(rng.uniform(8, ny - 8)); x = float(rng.uniform(8, nx - 8))
            iy, ix = int(y), int(x)
            if not valid[iy, ix]:
                continue
            if cfg.get("avoid_bright_base", True):
                w = base_rate[max(0, iy - 3):iy + 4, max(0, ix - 3):ix + 4]
                if w.size and float(w.max()) > thr:
                    continue
            if rmin > 0 and ys and min((y - a) ** 2 + (x - b) ** 2
                                       for a, b in zip(ys, xs)) < rmin * rmin:
                continue
            ys.append(y); xs.append(x)
        ys = np.array(ys); xs = np.array(xs)
        info["guard_draws"] = guard
        info["avoid_bright_base"] = bool(cfg.get("avoid_bright_base", True))
        info["avoid_threshold_e_per_s"] = thr if n > 0 else None
    else:
        ys = np.array([]); xs = np.array([])
    if len(ys) == 0:
        info["n_injected"] = 0
        return [], info
    if fixed_flux is not None:
        flux_e = fixed_flux
        mags = rate_to_ab_mag(flux_e / exposure_s, zp_ab)
        info["explicit_flux_e"] = True
    elif str(cfg.get("mode", "abmag")) in ("abmag", "mag"):
        lo, hi = cfg.get("mag_range", [18.0, 24.0])
        mags = rng.uniform(float(lo), float(hi), size=len(ys))
        rate_e_per_s = ab_mag_to_rate_e_per_s(mags, zp_ab)
        flux_e = rate_e_per_s * exposure_s
    else:
        lo, hi = cfg.get("eflux_log10_range", [2.0, 5.0])
        flux_e = 10.0 ** rng.uniform(float(lo), float(hi), size=len(ys))
        mags = rate_to_ab_mag(flux_e / exposure_s, zp_ab)
    cat = [{"y": float(a), "x": float(b), "flux_e": float(c), "true_ab_mag": float(m),
            "true_rate_e_per_s": float(c) / exposure_s}
           for a, b, c, m in zip(ys, xs, flux_e, mags)]
    info.update({"n_injected": len(cat),
                 "mag_range": list(cfg.get("mag_range", [])) if str(cfg.get("mode")) == "abmag"
                              else None,
                 "mag_median": float(np.median(mags)),
                 "flux_e_median": float(np.median(flux_e)),
                 "zp_ab": zp_ab})
    return cat, info


# ---------------------------------------------------------------------------
# 3b. 源面装配（**单位不变量**：速率 vs 计数）—— P9 定位的 x t 缺陷的常驻防线
# ---------------------------------------------------------------------------
def assemble_source_rate(base_rate: np.ndarray, cat: List[Dict[str, float]],
                         kern: np.ndarray, exposure_s: float
                         ) -> Tuple[np.ndarray, np.ndarray]:
    """把**真实底速率面** [e-/s] 与**注入星点**（flux_e [e-]，已含曝光积分）装配成期望源率面。

    单位契约（noise_model.expose：lam_e = t*(src_e_per_s + sky_e_per_s)*m + t*D）::

        src_rate [e-/s] = base_rate [e-/s]  +  sum_stars(stamp(flux_e [e-])) / t [s]

    **反例（历史缺陷，P9 首次定位）**：src_rate = (base_rate + stars_e) / t
    —— 注释只对星点成立，却把真实底也除了 t，随后 expose 再乘 t
    ⇒ 真实底在合成帧里等价于「以 e/s 数值当电子数」，被额外衰减 x t。

    返回 (src_rate, inj_rate)；inj_rate = 注入星单独的率面（供自检/诊断）。
    """
    t = float(exposure_s)
    if t <= 0:
        raise ValueError("exposure_s must be > 0, got %r" % exposure_s)
    inj_rate = np.zeros_like(base_rate, dtype=float)
    for s in cat:
        R.stamp(kern, inj_rate, s["y"], s["x"], float(s["flux_e"]) / t)
    return base_rate + inj_rate, inj_rate


def base_rate_transport_residual(frame, base_rate: np.ndarray,
                                 sky_e_per_s: np.ndarray, exposure_s: float,
                                 dark_rate_e_per_s: float,
                                 valid=None, inj_rate_e_per_s=None) -> Dict[str, Any]:
    """常驻自检：由落盘真值面反解「真实底以什么身份进入」，返回相对偏差。

    由 expose 的物理链精确反解（truth_e = t*(src+sky)*m + t*D）::

        implied_base_rate = truth_e / (t*m) - sky - D/m

    注入星为 0 的场景（本仓 4 个 M16 数据集都是）下 implied_base_rate 必须**逐像素等于**
    base_rate；历史缺陷下该比值 = 1/t。判据：max|ratio-1| <= 1e-9。
    """
    t = float(exposure_s)
    m = np.asarray(frame.flat, dtype=float)
    with np.errstate(divide="ignore", invalid="ignore"):
        implied = (np.asarray(frame.truth_e, dtype=float) / (t * m)
                   - np.asarray(sky_e_per_s, dtype=float)
                   - float(dark_rate_e_per_s) / m)
        if inj_rate_e_per_s is not None:      # 扣掉注入星率面 => 只剩真实底的贡献
            implied = implied - np.asarray(inj_rate_e_per_s, dtype=float)
    base = np.asarray(base_rate, dtype=float)
    # 判据像素的选择必须**按条件数**给，不是「非零即取」。
    #
    # implied 是若干 O(sky) 的大数相减得到的量，它的**绝对**误差底 ≈ eps·(|sky|+|inj|+|D/m|)，
    # 与 base 的大小无关。真实底的逐像素分布里含大量近零像素（本仓 M16 真实底底值中位
    # 0.024 e-/s，但全域跨度 −503…655），对这类像素做比值，比值完全由绝对误差底决定，
    # 与被测物理无关 —— 真实底口径正确的情形下 max|ratio−1| 也会顶到 1 附近。
    #
    # 因此只取「真实底显著高于反解残差绝对误差底」的像素：
    #   base > K · eps · max(|sky|, |inj|, |D|)
    # 取 K=1e3 留三个数量级余量；低于该底的像素其比值不承载判据信息，计入 n_px_skipped。
    eps = float(np.finfo(float).eps)
    ref = float(np.max(np.abs(np.asarray(sky_e_per_s, dtype=float))))
    if inj_rate_e_per_s is not None:
        ref = max(ref, float(np.max(np.abs(np.asarray(inj_rate_e_per_s, dtype=float)))))
    ref = max(ref, abs(float(dark_rate_e_per_s)))
    floor = 1e3 * eps * ref
    in_mask = np.ones(base.shape, dtype=bool) if valid is None else np.asarray(valid, bool)
    sel = in_mask & (base > floor)
    n_skipped = int((in_mask & ~sel).sum())
    if not sel.any():
        return {"n_px": 0, "n_px_skipped": n_skipped, "max_abs_rel_dev": 0.0,
                "median_ratio": 1.0, "selection_floor": floor,
                "expected_if_defect": 1.0 / t - 1.0}
    ratio = implied[sel] / base[sel]
    return {"n_px": int(sel.sum()), "n_px_skipped": n_skipped,
            "max_abs_rel_dev": float(np.max(np.abs(ratio - 1.0))),
            "median_ratio": float(np.median(ratio)),
            "selection_floor": floor,
            "expected_if_defect": 1.0 / t - 1.0}


# ---------------------------------------------------------------------------
# 4. 渲染一帧
# ---------------------------------------------------------------------------
def render_m16_frame(scene: Dict[str, Any], *, seed: int, frame_index: int = 0,
                     verbose: bool = False) -> Tuple[NM.Frame, Dict[str, Any], np.ndarray]:
    """渲染一帧 -> (Frame, truth, valid_mask)。"""
    frames = scene.get("frames") or [{}]
    ov = frames[frame_index] if frame_index < len(frames) else {}
    sc = R.deep_merge(scene, ov)
    sc.pop("frames", None)
    shape = tuple(int(v) for v in sc["shape"])
    ny, nx = shape
    rng = np.random.default_rng(int(seed))

    # --- 真实底（期望率面） ---
    base_rate, valid, rb_meta = load_real_base(sc["real_base"], shape, verbose=verbose)
    zp = rb_meta["photometric"]
    zp_ab = float(zp["zp_ab"]) if str(sc["photometric"].get("zeropoint", "ab")) == "ab" \
        else float(zp["zp_st"])
    flux_scale = float(sc["photometric"].get("flux_scale", 1.0))
    if flux_scale != 1.0:
        base_rate = base_rate * flux_scale
    t = float(sc["exposure_s"])

    # --- PSF 与注入星点 ---
    kern, psf_meta = R.psf_kernel(sc["psf"])
    cat, inj_meta = make_star_catalog(sc, shape, zp_ab, t, base_rate, valid, rng)
    # **单位不变量（P9 定位的 x t 缺陷的修复点）**：
    #   base_rate 单位 = e-/s（真实帧 BUNIT=ELECTRONS/S，load_real_base 的返回）；
    #   星表 flux_e 单位 = e-（已含曝光积分）。
    #   => 注入星必须**先除以 t 换成率**，再与真实底**相加**；整体除以 t 会把真实底衰减 x t。
    src_rate, inj_rate = assemble_source_rate(base_rate, cat, kern, t)
    clamped = float(np.mean(src_rate[valid] < 0)) if valid.any() else 0.0

    # --- 天光（进泊松） ---
    sky_cfg = dict(sc.get("sky", {}))
    mode = str(sky_cfg.pop("mode", "from_real"))
    if mode == "from_real":
        sky_cfg["level_e_per_s"] = rb_meta["pedestal_e_per_s"]
    sky = NM.sky_surface_e_per_s(shape, **sky_cfg)
    sky = np.where(valid, sky, 0.0)

    # --- 探测器 / 平场 / 热像素（固定图样，不随曝光变） ---
    det_cfg = dict(sc["detector"])
    stack_n = int(sc.get("stack_n", 32))     # STACKN32-001: 全局默认 32（真实 drz NDRIZIM=32）
    rn0 = float(det_cfg.get("read_noise_e", 3.1))
    det_cfg["read_noise_e"] = rn0 * math.sqrt(max(stack_n, 1))
    det = NM.Detector(**det_cfg)
    flat_cfg = dict(sc.get("flat", {}))
    flat_seed = int(flat_cfg.pop("seed", 1234))
    flat = NM.flat_response(shape, np.random.default_rng(flat_seed), **flat_cfg)
    art = sc.get("artifacts", {}) or {}
    hot = NM.hot_pixel_map(shape, np.random.default_rng(int(art.get("hot_seed", 4321))),
                           float(art.get("hot_pixel_fraction", 0.0)),
                           float(art.get("hot_pixel_dark_gain", 50.0)))
    frame = NM.expose(
        src_e_per_s=src_rate, sky_e_per_s=sky, det=det, exptime_s=t, rng=rng,
        temp_c=float(sc.get("temp_c", det.dark_ref_temp_c)), flat=flat, hot_map=hot,
        cr_rate_per_frame=float(art.get("cr_rate_per_frame", 0.0)),
        cr_mean_charge_e=float(art.get("cr_mean_charge_e", 1000.0)),
        mode=str(sc.get("mode", NM.MODE_PHYSICAL)),
        variance_override_e2=sc.get("variance_override_e2"),
        additive_offset_adu=float(sc.get("additive_offset_adu", 0.0)),
    )
    # 无效像素 -> 0（与真实 drz 的零填充一致）；**必须写回 Frame**，
    # 否则落盘的是未掩膜的帧（曾因此使 HDU MASK 与 SCI 不一致，交付自检抓出）。
    frame.adu = np.where(valid, frame.adu, 0.0)
    adu = frame.adu

    # --- 常驻自检：真实底必须以**速率**身份进入（P9 定位的 x t 缺陷的回归防线） ---
    # 由 truth_e 精确反解 base_rate；历史缺陷下 max|ratio-1| = 1-1/t ~ 1（t=9600 时 0.99990）。
    transport = base_rate_transport_residual(
        frame, base_rate, sky, t, det.dark_current_at(float(sc.get("temp_c",
                                                                   det.dark_ref_temp_c))),
        valid, inj_rate_e_per_s=inj_rate)
    if float(sc.get("base_transport_tol", 1e-9)) > 0:
        if not (transport["max_abs_rel_dev"] <= float(sc.get("base_transport_tol", 1e-9))):
            raise AssertionError(
                "M16 base-rate transport violated: max|implied/base - 1| = %.6e > tol "
                "(defect signature 1-1/t = %.6e); real base must enter as a RATE [e-/s] "
                "and be summed with injected source rates BEFORE multiplying by exposure_s"
                % (transport["max_abs_rel_dev"], transport["expected_if_defect"]))

    truth: Dict[str, Any] = {
        "scene_id": sc.get("scene_id"), "kind": sc.get("kind"),
        "frame_id": sc.get("frame_id", "%s_f%02d" % (sc.get("scene_id"), frame_index)),
        "frame_index": frame_index, "seed": int(seed), "shape": list(shape),
        "renderer": "m16_scene",
        "exposure_s": t, "stack_n": stack_n, "temp_c": float(sc.get("temp_c")),
        "psf": psf_meta, "detector": det.as_dict(), "detector_read_noise_before_stack_e": rn0,
        "photometric": {"zeropoint_used": str(sc["photometric"].get("zeropoint", "ab")),
                        "zp_ab": zp_ab, "zp_st": float(zp["zp_st"]),
                        "flux_scale": flux_scale, "photflam": zp["photflam"],
                        "photplam": zp["photplam"],
                        "pixscale_arcsec": PIXSCALE_ARCSEC,
                        "formula": zp["formula"], "note": zp["note"]},
        "real_base": rb_meta,
        "injected_stars": inj_meta, "stars_in_frame": cat,
        "base_rate_transport": dict(transport, tol=float(sc.get("base_transport_tol", 1e-9)),
                                    check="implied_base_rate = truth_e/(t*m) - sky - D/m "
                                          "must equal base_rate (+ injected rates); "
                                          "defect signature = 1-1/t"),
        "base_rate_stats_e_per_s": {
            "max": float(base_rate[valid].max()) if valid.any() else 0.0,
            "median": float(np.median(base_rate[valid])) if valid.any() else 0.0,
            "sigma": float(np.std(base_rate[valid])) if valid.any() else 0.0,
            "expected_base_electrons_max": float(t * base_rate[valid].max())
            if valid.any() else 0.0},
        "injected_rate_peak_e_per_s": float(inj_rate.max()) if cat else 0.0,
        "sky": {"params": {k: v for k, v in sky_cfg.items()}, "mode": mode,
                "level_e_per_s_mean": float(sky.mean()), "max_e_per_s": float(sky.max()),
                "sky_e_total_mean": float(sky.mean() * t),
                "poisson": True,
                "note": "天光逐像素进泊松；纯加性天光是非物理负例臂（mode=additive）"},
        "flat": {"params": sc.get("flat", {}), "seed": flat_seed, "fixed_pattern": True,
                 "min": float(flat.min()), "max": float(flat.max()), "mean": float(flat.mean())},
        "artifacts": art, "mode": str(sc.get("mode", NM.MODE_PHYSICAL)),
        "valid": {"fraction": float(valid.mean()), "n_invalid": int((~valid).sum()),
                  "policy": "invalid pixels -> ADU 0 (same as real drz); mask written to HDU 1",
                  "invalid_all_zero_check": bool(np.all(frame.adu[~valid] == 0.0))
                  if (~valid).any() else True},
        "truth_e_stats": {"min": float(frame.truth_e.min()), "median": float(np.median(frame.truth_e)),
                          "max": float(frame.truth_e.max())},
        "clamped_negative_source_fraction": clamped,
        "adu_stats": {"min": float(adu.min()), "median": float(np.median(adu[valid])) if valid.any() else 0.0,
                      "max": float(adu.max()),
                      "saturated_pixels": int(np.count_nonzero(
                          adu >= det.saturation_adu)) if det.saturate else 0},
        "provenance": frame.provenance,
    }
    return frame, truth, valid


# ---------------------------------------------------------------------------
# 5. 落盘 / 数据集
# ---------------------------------------------------------------------------
def write_m16_frame(outdir: Path, truth: Dict[str, Any], frame: NM.Frame,
                    valid: np.ndarray) -> Dict[str, str]:
    from astropy.io import fits
    outdir.mkdir(parents=True, exist_ok=True)
    fid = truth["frame_id"]
    hdr = fits.Header()
    hdr["BUNIT"] = ("ADU", "synthetic frame unit")
    hdr["SCENEID"] = (str(truth["scene_id"]), "scene recipe id")
    hdr["KIND"] = (str(truth["kind"]), "data-type matrix cell kind")
    hdr["FRAMEID"] = (str(fid), "frame id")
    hdr["SEED"] = (int(truth["seed"]), "RNG seed")
    hdr["MODE"] = (str(truth["mode"]), "noise arm: physical|additive|mean_only|variance_override")
    hdr["RENDERER"] = ("m16_scene", "M16 real-base forward renderer")
    hdr["BAND"] = (str(truth["real_base"]["band"]), "HST WFC3/UVIS filter of the real base")
    hdr["REALEXP"] = (float(truth["real_base"]["real_exptime_s"]), "[s] real drz EXPTIME")
    hdr["NDRIZIM"] = (int(truth["real_base"]["ndrizim"]), "n exposures in the real drz")
    hdr["PHOTFLAM"] = (float(truth["photometric"]["photflam"]), "from real header (1st-hand)")
    hdr["PHOTPLAM"] = (float(truth["photometric"]["photplam"]), "[Angstrom]")
    hdr["ZPAB"] = (float(truth["photometric"]["zp_ab"]), "AB zeropoint [mag] for e-/s")
    hdr["PSFMODEL"] = (str(truth["psf"]["model"]), "PSF model used")
    hdr["FWHMPX"] = (float(truth["psf"]["fwhm_px"]), "PSF FWHM [pix]")
    hdr["PSFBETA"] = (float(truth["psf"].get("beta", 0.0)), "Moffat beta (0 if N/A)")
    hdr["GAIN"] = (float(truth["detector"]["gain_e_per_adu"]), "[e-/ADU]")
    hdr["RDNOISE"] = (float(truth["detector"]["read_noise_e"]), "[e-] incl. stack_n")
    hdr["EXPTIME"] = (float(truth["exposure_s"]), "[s] synthetic exposure")
    hdr["STACKN"] = (int(truth["stack_n"]), "equivalent sub-exposures")
    hdr["TEMPC"] = (float(truth["temp_c"]), "[C] detector temperature")
    hdr["SKYEPS"] = (float(truth["sky"]["level_e_per_s_mean"]), "[e-/pix/s] mean sky rate")
    hdr["NINJECT"] = (int(truth["injected_stars"].get("n_injected", 0)), "injected stars")
    hdr["VALIDFRC"] = (float(truth["valid"]["fraction"]), "valid pixel fraction")
    hdr.add_history("ACSD RELEASE-02 M16-SCENE physical noise chain (GAP_AUDIT 9.41):")
    hdr.add_history("lam_e = t*(src_rate+sky_rate)*flat + t*D(T)*hot")
    hdr.add_history("n_e = Poisson(lam_e) + N(0, rdnoise*sqrt(stack_n)) + CR")
    hdr.add_history("adu = round(n_e/gain + bias); invalid -> 0; MASK in HDU 1")
    hdr.add_history("real base: %s crop=%s (denoised, pedestal moved to sky)" % (
        truth["real_base"]["path"], truth["real_base"]["crop"]))
    hdus = [fits.PrimaryHDU(data=np.asarray(frame.adu, dtype=np.float32), header=hdr),
            fits.ImageHDU(data=valid.astype(np.uint8), name="MASK"),
            fits.ImageHDU(data=np.asarray(frame.truth_e, dtype=np.float32), name="TRUTHE")]
    fp = outdir / ("%s.fits" % fid)
    fits.HDUList(hdus).writeto(fp, overwrite=True)
    mp = outdir / ("%s.meta.json" % fid)
    with open(mp, "w", encoding="utf-8") as fh:
        json.dump(truth, fh, indent=1, ensure_ascii=False, default=float)
    return {"fits": str(fp), "meta": str(mp)}


def render_m16_dataset(scene_path, outdir, *, seed: Optional[int] = None,
                       frames: Optional[List[int]] = None, verbose: bool = True
                       ) -> Dict[str, Any]:
    scene = load_scene(scene_path)
    base_seed = int(seed if seed is not None else scene.get("seed", 20260924))
    nframes = len(scene.get("frames") or [{}])
    idxs = list(range(nframes)) if frames is None else list(frames)
    out = Path(outdir)
    if not out.is_absolute():
        out = ROOT / out
    out.mkdir(parents=True, exist_ok=True)
    man: Dict[str, Any] = {"scene_path": str(scene_path), "scene_id": scene.get("scene_id"),
                           "kind": scene.get("kind"), "renderer": "m16_scene",
                           "base_seed": base_seed, "outdir": str(out), "frames": []}
    for k in idxs:
        f, truth, valid = render_m16_frame(scene, seed=base_seed + k, frame_index=k)
        paths = write_m16_frame(out, truth, f, valid)
        rec = {"status": "OK", "frame_id": truth["frame_id"], "frame_index": k,
               "seed": truth["seed"], "mode": truth["mode"], "kind": truth["kind"],
               "band": truth["real_base"]["band"], "exposure_s": truth["exposure_s"],
               "psf": truth["psf"], "zp_ab": truth["photometric"]["zp_ab"],
               "n_injected": truth["injected_stars"].get("n_injected", 0),
               "valid_fraction": truth["valid"]["fraction"],
               "adu_stats": truth["adu_stats"], "sky": truth["sky"]["level_e_per_s_mean"],
               "real_base_crop": truth["real_base"]["crop"]}
        rec.update(paths)
        man["frames"].append(rec)
        if verbose:
            print("[m16] %-24s band=%-5s t=%7.1fs fwhm=%.2f sky=%.4f e-/s inj=%3d "
                  "adu_med=%8.0f sat=%d valid=%.5f" % (
                      truth["frame_id"], truth["real_base"]["band"], truth["exposure_s"],
                      truth["psf"]["fwhm_px"], truth["sky"]["level_e_per_s_mean"],
                      truth["injected_stars"].get("n_injected", 0), truth["adu_stats"]["median"],
                      truth["adu_stats"]["saturated_pixels"], truth["valid"]["fraction"]))
    with open(out / "dataset.json", "w", encoding="utf-8") as fh:
        json.dump(man, fh, indent=1, ensure_ascii=False, default=float)
    return man


def list_scenes() -> List[str]:
    return sorted(p.name for p in SCENES_DIR.glob("m16_*.json"))


# ---------------------------------------------------------------------------
# 6. 自检
# ---------------------------------------------------------------------------
def selftest(verbose: bool = True) -> Dict[str, Any]:
    """自检：星等尺度绝对口径 + 面亮度出口 + 掩膜传播 + 天光泊松（B 升 => sigma 升）+ 纯加性负例。

    G08-04 整改 R2：V2 由「互逆往返」（恒真）改为星等尺度**定义**与 **ST 零点**双锚点；
    V7 新增，给 surface_brightness_mag_per_arcsec2 补判别力（此前零判据引用）。
    """
    res: Dict[str, Any] = {"criteria": {}, "verdicts": {}}
    zp = photometric_scale(2.2290223e-18, 6566.60545, -21.10)
    res["criteria"]["zp_ab_F657N"] = zp["zp_ab"]
    res["criteria"]["zp_st_F657N"] = zp["zp_st"]
    res["criteria"]["zp_ab_from_st"] = zp["zp_ab_from_st"]
    res["verdicts"]["V1_ab_st_consistent"] = bool(abs(zp["zp_ab"] - zp["zp_ab_from_st"]) < 0.01)
    # V2 **星等尺度的绝对口径**（G08-04 整改 R2 换掉往返恒真门）。
    # 旧判据是 `rate_to_ab_mag(ab_mag_to_rate_e_per_s(m)) == m` 的**互逆往返**：两个函数
    # 互为代数逆，往返恒等于输入 ⇒ 任何「成对一致但口径错」的替换都测不出。实测把整套
    # AB 口径换成 exp/ln（每星等效灵敏度错 ln(10)/0.4 = 5.76 倍），旧 V2 仍 exit 0 全绿。
    # 新 V2 不再用这对互逆函数自证，改用两个**外部锚点**：
    #   (a) 定义臂：星等尺度的**定义**就是「1 mag ≡ 物理量 ×10^0.4」。期望值由测试端
    #       自己用 log10 算出（10^(-0.4Δm)），**不问被测函数**；正向映射与反向映射
    #       各自独立比对，故只改一侧也会判红。
    #   (b) ST 零点臂：m_AB = m_ST - 5log10(λ) + 2.5log10(c) - 27.50 与两个被测函数
    #       **零共享**。先用它把同一速率换算成 AB 星等，再要求两个被测函数各自还原它
    #       （正向还原星等、反向还原速率）。该臂**不假装**两条路线逐位相等：
    #       ST→AB 换算本身带 5.18e-5 mag 的常数残差（干净态实测，V1 的容差是 0.01 mag），
    #       故本臂检查两件可判别的事：残差**幅值**在 0.01 mag 内，且残差是**常数**
    #       （随速率的峰峰 < 1e-9）——换底数或丢 2.5 系数都会让残差随速率发散。
    dms2 = (1.0, 5.0, 8.75)
    ra2 = float(ab_mag_to_rate_e_per_s(20.0, zp["zp_ab"]))
    scale_rows = []
    for dm in dms2:
        rb2 = float(ab_mag_to_rate_e_per_s(20.0 + dm, zp["zp_ab"]))
        expect_ratio = 10.0 ** (-0.4 * dm)                 # 星等尺度的定义（测试端独立算）
        dm_back = (float(rate_to_ab_mag(rb2, zp["zp_ab"]))
                   - float(rate_to_ab_mag(ra2, zp["zp_ab"])))
        scale_rows.append({
            "delta_mag": dm,
            "rate_ratio_meas_inverse_map": rb2 / ra2,
            "rate_ratio_expected_10pow_neg04dm": expect_ratio,
            "rel_dev_ratio": abs(rb2 / ra2 - expect_ratio) / expect_ratio,
            "delta_mag_back_via_forward_map": dm_back,
            "rel_dev_delta_mag": abs(dm_back - dm) / dm})
    st_shift2 = (-5.0 * math.log10(zp["photplam"])
                 + 2.5 * math.log10(C_LIGHT_ANGSTROM_PER_S) - 27.50)
    st_rows = []
    for rate2 in (0.5, 3.0, 11.3124, 250.0):
        m_via_st = (-2.5 * math.log10(rate2) + zp["zp_st"]) + st_shift2
        fwd2 = float(rate_to_ab_mag(rate2, zp["zp_ab"]))
        inv2 = float(ab_mag_to_rate_e_per_s(m_via_st, zp["zp_ab"]))
        st_rows.append({
            "rate_e_per_s": rate2, "m_ab_via_st_route": m_via_st,
            "m_ab_via_forward_map": fwd2, "dev_forward_map_mag": fwd2 - m_via_st,
            "rate_back_via_inverse_map": inv2, "rel_dev_inverse_map": inv2 / rate2 - 1.0})
    dev_f = np.array([r["dev_forward_map_mag"] for r in st_rows])
    rel_i = np.array([r["rel_dev_inverse_map"] for r in st_rows])
    res["criteria"]["ab_scale_absolute"] = {
        "definition_arm_rows": scale_rows,
        "definition_arm_max_rel_dev": max(max(r["rel_dev_ratio"], r["rel_dev_delta_mag"])
                                           for r in scale_rows),
        "st_anchor_rows": st_rows,
        "st_anchor_max_abs_dev_mag": float(np.abs(dev_f).max()),
        "st_anchor_dev_ptp_mag": float(dev_f.max() - dev_f.min()),
        "st_anchor_max_rel_dev_rate": float(np.abs(rel_i).max()),
        "st_anchor_rel_dev_ptp": float(rel_i.max() - rel_i.min()),
        "st_shift_mag": st_shift2,
        "criterion": "(a) 速率比必须 = 10^(-0.4Δm)（星等尺度的定义，测试端独立算），"
                     "正向/反向映射各自比对，相对偏差 < 1e-9；"
                     "(b) 与 ST 零点路线的偏差幅值 < 0.01 mag（V1 同款容差，"
                     "干净态 5.18e-5）且该偏差随速率的峰峰 < 1e-9 mag（残差必须是常数）；"
                     "反向映射的速率相对残差同理（< 1e-4 且峰峰 < 1e-9）",
        "why_not_roundtrip": "旧 V2 是互逆往返（恒真）：两个代数逆函数必然往返，"
                             "整套换成 exp/ln 仍全绿。本条不依赖被测函数的自洽。",
    }
    res["verdicts"]["V2_ab_magnitude_scale_absolute"] = bool(
        res["criteria"]["ab_scale_absolute"]["definition_arm_max_rel_dev"] < 1e-9
        and res["criteria"]["ab_scale_absolute"]["st_anchor_max_abs_dev_mag"] < 0.01
        and res["criteria"]["ab_scale_absolute"]["st_anchor_dev_ptp_mag"] < 1e-9
        and res["criteria"]["ab_scale_absolute"]["st_anchor_max_rel_dev_rate"] < 1e-4
        and res["criteria"]["ab_scale_absolute"]["st_anchor_rel_dev_ptp"] < 1e-9)
    # 掩膜传播：造一个 64x64 的假场景（不走真实帧）
    ny, nx = 64, 64
    valid = np.ones((ny, nx), dtype=bool)
    valid[:4, :] = False
    src = np.full((ny, nx), 1.0)
    src[~valid] = 0.0
    det = NM.Detector(gain_e_per_adu=1.5, read_noise_e=3.0, bias_adu=1000.0)
    fr = NM.expose(src_e_per_s=src, sky_e_per_s=np.full((ny, nx), 1.0), det=det,
                   exptime_s=100.0, rng=np.random.default_rng(1))
    adu = np.where(valid, fr.adu, 0.0)
    res["criteria"]["mask_invalid_all_zero"] = bool(np.all(adu[~valid] == 0.0))
    res["verdicts"]["V3_mask_propagates"] = bool(np.all(adu[~valid] == 0.0)
                                                 and np.all(adu[valid] > 0))
    # 天光泊松：B 升 => sigma 升（同源、同种子臂）
    rows = []
    for B in (1.0, 10.0, 100.0):
        f = NM.expose(src_e_per_s=np.zeros((256, 256)), sky_e_per_s=np.full((256, 256), B),
                      det=det, exptime_s=100.0, rng=np.random.default_rng(7))
        rows.append({"sky_e_per_s": B, "sigma_adu": NM.clipped_std_adu(f.adu)})
    mono = all(rows[i + 1]["sigma_adu"] > rows[i]["sigma_adu"] for i in range(len(rows) - 1))
    res["criteria"]["sky_series"] = rows
    res["verdicts"]["V4_sky_poisson_monotone"] = bool(mono)
    # 纯加性负例：配对同种子 => sigma 逐位不变
    a = NM.expose(src_e_per_s=np.zeros((256, 256)), sky_e_per_s=np.full((256, 256), 10.0),
                  det=det, exptime_s=100.0, rng=np.random.default_rng(3)).adu
    b = a + 500.0
    rel = abs(NM.robust_sigma_adu(b) - NM.robust_sigma_adu(a)) / NM.robust_sigma_adu(a)
    res["criteria"]["additive_sigma_rel_change"] = rel
    res["verdicts"]["V5_additive_negative_control_zero"] = bool(rel < 1e-12)
    # V6 **单位不变量（真实底必须以速率身份进入）** —— P9 定位的 x t 缺陷的常驻回归判据。
    #    正例：assemble_source_rate + expose + 反解 => 残差 ~ 0；
    #    负例（历史缺陷式 (base+stars_e)/t）：残差必须 = 1-1/t（t=100 时 0.99）⇒ 判据有功效。
    ny6, nx6, t6 = 96, 96, 100.0
    kern6, _ = R.psf_kernel({"model": "moffat4", "fwhm_px": 2.0, "beta": 4.0})
    base6 = np.full((ny6, nx6), 2.0)
    cat6 = [{"y": 48.0, "x": 48.0, "flux_e": 1000.0}]
    src6, inj6 = assemble_source_rate(base6, cat6, kern6, t6)
    det6 = NM.Detector(gain_e_per_adu=1.5, read_noise_e=0.0, bias_adu=1000.0,
                       dark_current_e_per_s=0.0, quantize=False)
    sky6 = np.full((ny6, nx6), 0.5)
    fr6 = NM.expose(src_e_per_s=src6, sky_e_per_s=sky6, det=det6, exptime_s=t6,
                    rng=np.random.default_rng(11))
    good = base_rate_transport_residual(fr6, base6, sky6, t6, 0.0, inj_rate_e_per_s=inj6)
    # 负例：把同一批星点按历史缺陷式装配（真实底被一起除了 t）
    src_bad = (base6 + inj6 * t6) / t6
    fr_bad = NM.expose(src_e_per_s=src_bad, sky_e_per_s=sky6, det=det6, exptime_s=t6,
                       rng=np.random.default_rng(11))
    bad = base_rate_transport_residual(fr_bad, base6, sky6, t6, 0.0, inj_rate_e_per_s=inj6)
    star_e = float(inj6.sum()) * t6
    res["criteria"]["V6_base_rate_transport"] = {
        "good_max_abs_rel_dev": good["max_abs_rel_dev"],
        "bad_legacy_max_abs_rel_dev": bad["max_abs_rel_dev"],
        "legacy_defect_signature_1_minus_1_over_t": 1.0 - 1.0 / t6,
        "injected_star_flux_recovered_e": star_e}
    res["verdicts"]["V6_base_rate_enters_as_rate"] = bool(
        good["max_abs_rel_dev"] <= 1e-9
        and abs(bad["max_abs_rel_dev"] - (1.0 - 1.0 / t6)) < 1e-9
        and abs(star_e - 1000.0) < 1e-9)
    # V7 **面亮度出口**（G08-04 整改 R2 补零覆盖）：surface_brightness_mag_per_arcsec2
    # 此前**没有任何判据引用它**——把 `/pixscale_arcsec ** 2` 整项去掉（0.04 arcsec/px 下
    # 错 625 倍 = 6.98 mag），`m16_scene --selftest` 仍 exit 0 全绿。判据按面亮度定义的
    # 直接后果写：通量在**速率空间**相加，星等只差**面积**的对数项，
    #     mu_px = m_block + 2.5*log10(A_block)，A_block = (k*pixscale)^2 [arcsec^2]。
    # 期望值由测试端用 log10 现场算出，不取自被测函数。
    ps7, k7, r7 = PIXSCALE_ARCSEC, 4, 7.0
    area7 = (k7 * ps7) ** 2
    mu_px7 = float(surface_brightness_mag_per_arcsec2(r7, zp["zp_ab"], ps7))
    m_blk7 = float(rate_to_ab_mag(k7 * k7 * r7, zp["zp_ab"]))
    mu_expect7 = m_blk7 + 2.5 * math.log10(area7)
    mu_half = float(surface_brightness_mag_per_arcsec2(r7, zp["zp_ab"], 2.0 * ps7))
    res["criteria"]["surface_brightness_per_arcsec2"] = {
        "pixscale_arcsec": ps7, "block_px": k7, "rate_e_per_s_per_px": r7,
        "block_area_arcsec2": area7, "block_rate_e_per_s": k7 * k7 * r7,
        "m_ab_block": m_blk7,
        "mu_per_arcsec2_measured": mu_px7,
        "mu_per_arcsec2_expected": mu_expect7,
        "abs_dev_mag": abs(mu_px7 - mu_expect7),
        "mu_at_2x_pixscale": mu_half,
        "expected_shift_for_2x_pixsec": 2.5 * math.log10(4.0),
        "abs_dev_shift_mag": abs((mu_half - mu_px7) - 2.5 * math.log10(4.0)),
        "criterion": "单像素面亮度 = 块内总速率的 AB 星等 + 2.5log10(块面积 [arcsec^2])，"
                     "偏差 < 1e-9 mag；且板比例翻倍时面亮度必须恰好亮 2.5log10(4) mag",
    }
    res["verdicts"]["V7_surface_brightness_per_arcsec2"] = bool(
        abs(mu_px7 - mu_expect7) < 1e-9 and abs((mu_half - mu_px7) - 2.5 * math.log10(4.0)) < 1e-9)
    res["all_pass"] = bool(all(res["verdicts"].values()))
    if verbose:
        print(json.dumps(res, indent=2, ensure_ascii=False, default=float))
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description="M16 场景模板前向渲染器")
    ap.add_argument("--scene", type=str)
    ap.add_argument("--out", type=str)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--frames", type=str, default=None)
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
    fr = [int(v) for v in a.frames.split(",")] if a.frames else None
    man = render_m16_dataset(a.scene, a.out, seed=a.seed, frames=fr)
    print("[m16] wrote %d frame(s) -> %s" % (len(man["frames"]), a.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
