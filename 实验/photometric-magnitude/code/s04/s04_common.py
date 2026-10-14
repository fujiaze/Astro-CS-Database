# -*- coding: utf-8 -*-
"""S04（P1 真实帧光度与颜色误差归因）· 公共层

作用域与纪律
------------
- 本模块**只读**调用 `实验/photometric-magnitude/code/` 的既有实现（scia_common /
  scia_calib / scia_pipeline / scia_gaia / scia_sim / step3_forward_vs_photflam），
  不改其中任何一行、不改任何判据阈值。
- 所有写出路径重定向到 `run/ACSDSDOC-02/S04/**`：既有的 `sc.RUN` / `sc.CACHE` /
  `sc.RESULTS` / `sg.CACHE` / `sg.DUMP_BIN` 在 import 之后被改写，避免写到
  `run/SCI-401/**` 或 `实验/**/results/**`。
- 帧集枚举一律走 `实验/shared/data_catalog.json`（S00 落地的真实帧轻量索引），
  几何逐帧从 FITS 头读取，并按 `AGENTS.md` §13.1 做 CRVAL 天区一致性断言。

量纲约定（本模块统一）
----------------------
- 像素值 `img`：**ADU**。FITS `BITPIX=16` + `BZERO=32768` + `BSCALE=1`，astropy 以
  uint16 无符号约定返回，取值域 0..65535；`img = raw + BZERO` 已由 astropy 施加，
  **不得再减/再加一次**（负例 P1-D5 专测）。
- `flux`：PSF 拟合域通量 [ADU]（含 PSF 归一化面积，见 scia_common.moffat_profile）。
- `f_syn`：参考合成通量 [W·m^-2·nm]（`F_syn = ∫F_λ·T·Q·λ dλ`，不含 10^(-0.4·G)）。
- 残差 `d_i = -2.5·log10(F_instr,i/F_syn,i) - mu_k` [mag]；`mu_k` 为稳健零点。
- `sigma_obs = 2.5·MAD(r_inliers)/0.6744897501960817` [mag]。
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
import time

import numpy as np

# --------------------------------------------------------------------------
# 路径
# --------------------------------------------------------------------------
S04_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                       "..", "..", "..", "..",
                                       "run", "ACSDSDOC-02", "S04"))
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", "..", "..", ".."))
EXPCODE = os.path.join(REPO, "实验", "photometric-magnitude", "code")
CATALOG = os.path.join(REPO, "实验", "shared", "data_catalog.json")

RESULTS = os.path.join(S04_DIR, "results")
FIGURES = os.path.join(S04_DIR, "figures")
CACHE = os.path.join(S04_DIR, "cache")
for _d in (RESULTS, FIGURES, CACHE):
    os.makedirs(_d, exist_ok=True)

if EXPCODE not in sys.path:
    sys.path.insert(0, EXPCODE)

import scia_common as sc          # noqa: E402
import scia_gaia as sg            # noqa: E402
import scia_sim as ss             # noqa: E402
import scia_pipeline as pl        # noqa: E402
from scia_calib import (SIGMA_FLAT_HF_CANONICAL, aperture_flux, calibrate,   # noqa: E402
                        guided_photometry, psf_vs_aperture_systematics)

# --------------------------------------------------------------------------
# 写出重定向（必须在 import 之后、任何库写操作之前执行）
# --------------------------------------------------------------------------
_SC_RUN = os.path.join(CACHE, "_sc_run")
_SC_RESULTS = os.path.join(RESULTS, "_sc_results")
for _d in (_SC_RUN, _SC_RESULTS):
    os.makedirs(_d, exist_ok=True)
sc.RUN = _SC_RUN
sc.CACHE = CACHE
sc.RESULTS = _SC_RESULTS
sg.CACHE = CACHE
sg.DUMP_BIN = os.path.join(CACHE, "bin", "gaia_xp_dump")

# 原口径 Gaia 锥缓存（run/SCI-401 过程缓存，只读来源）：复制进本作用域并记哈希。
ORIG_CONE_SRC = os.path.join(REPO, "run", "SCI-401", "data_cache",
                             "gaia_xp_real_8328_-637.csv")
CONE_TAG = "real_8328_-637"


# --------------------------------------------------------------------------
# 基本工具
# --------------------------------------------------------------------------
def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def angular_sep_deg(ra1, dec1, ra2, dec2):
    """两点球面角距 [deg]（haversine）。"""
    r1, d1, r2, d2 = map(np.radians, (ra1, dec1, ra2, dec2))
    return float(np.degrees(2.0 * np.arcsin(np.sqrt(
        np.sin((d2 - d1) / 2.0) ** 2
        + np.cos(d1) * np.cos(d2) * np.sin((r2 - r1) / 2.0) ** 2))))


def sanitize(o):
    if isinstance(o, float):
        return o if np.isfinite(o) else None
    if isinstance(o, (np.floating,)):
        v = float(o)
        return v if np.isfinite(v) else None
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, np.ndarray):
        return sanitize(o.tolist())
    if isinstance(o, np.bool_):
        return bool(o)
    if isinstance(o, dict):
        return {k: sanitize(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [sanitize(v) for v in o]
    return o


def jdump(obj, name: str) -> str:
    p = os.path.join(RESULTS, name)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(sanitize(obj), f, ensure_ascii=False, indent=2, allow_nan=False)
    return p


def prepare_cone_cache() -> dict:
    """把原口径 Gaia 锥缓存复制进 S04 作用域并记录来源 SHA256（不改动原文件）。"""
    dst = os.path.join(CACHE, f"gaia_xp_{CONE_TAG}.csv")
    rec = dict(src=os.path.relpath(ORIG_CONE_SRC, REPO),
               dst=os.path.relpath(dst, REPO),
               exists=os.path.exists(ORIG_CONE_SRC))
    if rec["exists"]:
        rec["src_sha256"] = sha256_file(ORIG_CONE_SRC)
        if not os.path.exists(dst):
            import shutil
            shutil.copy2(ORIG_CONE_SRC, dst)
        rec["dst_sha256"] = sha256_file(dst)
        rec["sha256_match"] = rec["src_sha256"] == rec["dst_sha256"]
    return rec


def load_fits(path: str, bzero_mode: str = "correct", bscale_mode: str = "correct"):
    """读真实帧。

    bzero_mode: "correct"（默认，astropy 已施加 BZERO；不再加减）
                "double" （错误标度负例：再加一次 BZERO）
                "none"   （错误标度负例：当作有符号 int16，减一次 BZERO=取 raw-32768）
    bscale_mode: "correct" | "ignore"（负例：忽略 BSCALE）
    返回 (img(ADU, float64), hdr, raw, meta)
    """
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        d = np.asarray(h[0].data)
        hdr = h[0].header.copy()
    bzero = float(hdr.get("BZERO", 0.0))
    bscale = float(hdr.get("BSCALE", 1.0))
    raw = d
    x = np.asarray(d, dtype=np.float64)
    if bscale_mode == "ignore":
        x = x / bscale if bscale != 0 else x
    # astropy 对 BITPIX=16/BZERO=32768/BSCALE=1 返回 uint16，BZERO 已并入无符号表示。
    if bzero_mode == "double":
        x = x + bzero
    elif bzero_mode == "none":
        x = x - bzero
    meta = dict(bzero=bzero, bscale=bscale, dtype=str(np.asarray(d).dtype),
                raw_min=float(np.min(d)), raw_max=float(np.max(d)))
    return x, hdr, raw, meta


# --------------------------------------------------------------------------
# 帧枚举与几何（AGENTS §13.1）
# --------------------------------------------------------------------------
def catalog_frames():
    with open(CATALOG, encoding="utf-8") as f:
        cat = json.load(f)
    return cat


def read_geom(path: str) -> dict:
    """逐帧读头几何（产品自身），不按目录名/文件名推断。"""
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        hdr = h[0].header
        keys = ("CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
                "CD1_1", "CD1_2", "CD2_1", "CD2_2")
        present = {k: (k in hdr) for k in keys}
        out = dict(present=present, n_present=int(sum(present.values())),
                   missing=[k for k in keys if not present[k]])
        for k in keys:
            out[k] = float(hdr[k]) if present[k] else None
        for k in ("FILTER", "EXPTIME", "OBSERVER", "INSTRUME", "TELESCOP",
                  "DATE-OBS", "BZERO", "BSCALE", "BITPIX", "NAXIS1", "NAXIS2",
                  "IMAGETYP", "FWHM", "OBJECT"):
            if k in hdr:
                v = hdr[k]
                out[k] = float(v) if isinstance(v, (int, float)) else str(v).strip()
            else:
                out[k] = None
        if out.get("CRVAL1") is not None and out.get("CD1_1") is not None:
            cd = np.array([[out["CD1_1"], out["CD1_2"]],
                           [out["CD2_1"], out["CD2_2"]]], float)
            out["pixel_scale_arcsec"] = float(np.sqrt(abs(np.linalg.det(cd))) * 3600.0)
    return out


# --------------------------------------------------------------------------
# 参考通量与颜色轴
# --------------------------------------------------------------------------
def synth_flux(cat, idx, T, T_wl, Q, Q_wl):
    """逐星 F_syn = ∫F_λ·T·Q·λ dλ [W·m^-2·nm]（不含 10^(-0.4·G)）。"""
    return np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, T, T_wl, Q, Q_wl)
                     for i in idx], float)


def color_axes(cat, idx, curves: dict) -> dict:
    """由同一批 XP 谱算出多条参考色轴 [mag]。

    curves: {name: (T, T_wl, Q, Q_wl)}，name 形如 "Baader R|KAF-16803"。
    返回 {name: f_syn 数组} 与派生色轴。
    """
    fs = {name: synth_flux(cat, idx, *c) for name, c in curves.items()}
    out = dict(f_syn=fs)
    if "R" in fs and "B" in fs:
        out["col_RminusB"] = -2.5 * np.log10(fs["R"] / fs["B"])
    if "R" in fs and "G" in fs:
        out["col_GminusR"] = -2.5 * np.log10(fs["G"] / fs["R"])
    return out


def default_curves():
    """通行带集合：基线 Baader R × KAF-16803；颜色轴用同 QE 的 B/G。

    元组顺序**逐字对齐 `scia_common.f_syn(spectrum, wl_nm, T, T_wl, Q, Q_wl)`**：
    `T` / `Q` 是**取值**，`T_wl` / `Q_wl` 是**波长网格**（与 step8_real_frame.py:264-266
    的 `sc.f_syn(..., Tv, Tw, Qv, Qw)` 同序）。
    """
    Rw, Rv = sc.load_filter("Baader R")
    Bw, Bv = sc.load_filter("Baader B")
    Gw, Gv = sc.load_filter("Baader G")
    Qw, Qv = sc.load_qe("KAF-16803")
    return dict(R=(Rv, Rw, Qv, Qw), B=(Bv, Bw, Qv, Qw), G=(Gv, Gw, Qv, Qw))


# --------------------------------------------------------------------------
# 单帧测量（逐字复刻 step8_real_frame.py:78-197 的算法，只有选帧被替换）
# --------------------------------------------------------------------------
INSTRUMENT_HARDCODE = dict(gain=1.333, read_noise=8.59, dark_rate=0.002,
                           fwhm_px=1.98, beta_fit=4.0, beta_inject=3.5,
                           ellipticity=0.0, saturation_adu=65535.0)


def instrument():
    return ss.Instrument(**INSTRUMENT_HARDCODE)


def measure_frame(path: str, curves=None, tag_suffix: str = "", bzero_mode="correct",
                  bscale_mode="correct", extra_columns=None, keep_spectra=True,
                  tag: str = None, var_map_mode: str = "head", fs_transform=None):
    """真实帧 → 逐星表 + 双边界判据读数。

    tag: 预算 Monte Carlo 的 RNG tag（决定 sigma_fit_robust 的抽样实现）。
         **跨变体比较必须固定同一 tag**（T04 U7：换 tag 使 σ_ceiling 相对极差 0.394%）。
    var_map_mode: "head"   全样本方差 `img/g + (RN/g)²`（HEAD 口径，commit 410f05f13 后）
                  "prerev" 全样本方差 `(img + RN²)/g²`（410f05f13 之前口径，CCD 量纲错误：
                          把 ADU 图当电子期望图，散粒项低估 √g 倍）
    """
    t0 = time.perf_counter()
    if curves is None:
        curves = default_curves()
    img, hdr, raw, fmeta = load_fits(path, bzero_mode=bzero_mode, bscale_mode=bscale_mode)
    H, W = img.shape
    inst = instrument()

    out = dict(path=os.path.relpath(path, REPO), file_sha256=sha256_file(path),
               shape=[int(H), int(W)], fits=fmeta,
               filter=str(hdr.get("FILTER", "")).strip(),
               exptime=float(hdr.get("EXPTIME", np.nan)),
               observer=str(hdr.get("OBSERVER", "")).strip(),
               date_obs=str(hdr.get("DATE-OBS", "")).strip(),
               fwhm_header=float(hdr["FWHM"]) if "FWHM" in hdr else None,
               bzero_mode=bzero_mode, bscale_mode=bscale_mode)

    # ---- 帧内仪器读数 ----
    dd = np.diff(img, axis=1)
    bg_rms_adu = float(np.median(np.abs(dd - np.median(dd))) * sc.MAD_TO_SIGMA / np.sqrt(2.0))
    sky_med = float(np.median(img))
    out["in_frame_instrument"] = dict(
        sky_median_adu=sky_med, bg_rms_adu_adjacent_diff=bg_rms_adu,
        img_p01=float(np.percentile(img, 1)), img_p99=float(np.percentile(img, 99)),
        img_min=float(img.min()), img_max=float(img.max()),
        n_sat_like_65535=int((img >= 65535).sum()),
        sigma_pix_e_model=float(np.sqrt(inst.read_noise ** 2 + sky_med * inst.gain)),
        sigma_pix_e_measured_from_bg_adu=float(bg_rms_adu * inst.gain))

    # ---- 几何（取自产品自身）----
    from astropy.wcs import WCS
    w = WCS(hdr)
    crval = [float(w.wcs.crval[0]), float(w.wcs.crval[1])]
    out["wcs_from_header"] = dict(
        crval=crval, crpix=[float(w.wcs.crpix[0]), float(w.wcs.crpix[1])],
        cd=[[float(x) for x in r] for r in np.asarray(w.wcs.cd)],
        ctype=[str(x) for x in w.wcs.ctype],
        pixel_scale_arcsec=float(np.sqrt(abs(np.linalg.det(np.asarray(w.wcs.cd)))) * 3600.0))

    # ---- Gaia 锥（缓存命中；tag 由 CRVAL 派生，与原口径一致）----
    cone_tag = f"real_{int(crval[0]*100)}_{int(crval[1]*100)}{tag_suffix}"
    cat = sg.XpCatalog(sg.dump_cone(crval[0], crval[1], 0.30, 18.0, cone_tag))
    sep = np.array([angular_sep_deg(a, b_, crval[0], crval[1])
                    for a, b_ in zip(cat.ra, cat.dec)])
    out["gaia_cone"] = dict(tag=cone_tag, center=crval, radius_deg=0.30, mag_high=18.0,
                            n_sources=int(cat.n_src), n_spec=int(cat.n_spec),
                            csv=os.path.relpath(cat.csv, REPO), sha256=cat.sha256,
                            max_sep_from_frame_crval_deg=float(sep.max()),
                            n_within_radius=int((sep <= 0.30 + 1e-9).sum()))

    # ---- WCS 二轮精化 ----
    from step3_forward_vs_photflam import refine_shift
    shift, n_match, n_in, tol = refine_shift(w, img, cat.ra, cat.dec)
    out["wcs_refinement"] = dict(shift_px=[float(shift[0]), float(shift[1])],
                                 shift_arcsec=float(np.hypot(*shift)
                                                    * out["wcs_from_header"]["pixel_scale_arcsec"]),
                                 n_matched_within_tol=int(n_match), n_in_frame=int(n_in),
                                 tol_px=float(tol))

    # ---- 引导测光 ----
    if var_map_mode == "head":
        var = img / inst.gain + (inst.read_noise / inst.gain) ** 2
    elif var_map_mode == "prerev":
        var = (np.clip(img, 0, None) + inst.read_noise ** 2) / inst.gain ** 2
    else:
        raise ValueError(f"unknown var_map_mode {var_map_mode!r}")
    out["var_map_mode"] = var_map_mode
    t1 = time.perf_counter()
    g = guided_photometry(img, w, cat.ra, cat.dec, inst, var_map=var,
                          wcs_shift=shift, mag_limit=16.5, mag_g=cat.magG)
    n_cand = int(g["idx"].size)
    out["guided"] = dict(candidates=n_cand, fit_ok=int(g["fit_ok"].sum()),
                         wall_time_s=float(time.perf_counter() - t1),
                         sec_per_fit=float((time.perf_counter() - t1) / max(n_cand, 1)))
    # ---- 饱和/质量位（逐星，用拟合 stamp 内的饱和像素判定，生产 psf_status 语义）----
    sat_stamp = np.zeros(n_cand, bool)
    xr = np.clip(np.round(np.nan_to_num(g["x"], nan=0.0)).astype(int), 0, W - 1)
    yr = np.clip(np.round(np.nan_to_num(g["y"], nan=0.0)).astype(int), 0, H - 1)
    box = int(np.ceil(3 * inst.fwhm_px))
    for k in range(n_cand):
        if not np.isfinite(g["x"][k]) or not np.isfinite(g["y"][k]):
            continue
        x0, x1 = max(xr[k] - box, 0), min(xr[k] + box, W - 1)
        y0, y1 = max(yr[k] - box, 0), min(yr[k] + box, H - 1)
        sat_stamp[k] = bool(np.any(img[y0:y1 + 1, x0:x1 + 1] >= inst.saturation_adu))

    # 全部候选的拟合质量（用于「质量筛选存活率」诊断；perstar 只保存选中样本）
    out["candidates"] = dict(idx=g["idx"].astype(int), x=g["x"], y=g["y"],
                             flux=g["flux"], flux_err=g["flux_err"], chi2=g["chi2"],
                             fit_ok=g["fit_ok"], mag_g=cat.magG[g["idx"]],
                             shift_px=g["shift_px"], shift_ok=g["shift_ok"],
                             saturated_stamp=sat_stamp)

    # ---- 判据样本选择（逐字）----
    sel = (g["fit_ok"] & np.isfinite(g["flux"]) & (g["flux"] > 0)
           & np.isfinite(g["chi2"]) & (g["chi2"] < 4.0) & (cat.magG[g["idx"]] < 16.0))
    idx_sel = g["idx"][sel]
    F = g["flux"][sel]
    fs = synth_flux(cat, idx_sel, *curves["R"])
    if fs_transform is not None:
        fs = np.asarray(fs_transform(idx_sel, fs, cat, curves), float)
    out["selection"] = dict(rule="fit_ok & flux>0 & finite(chi2) & chi2<4.0 & magG<16.0",
                            n_candidates=n_cand, n_selected=int(F.size))
    if F.size < 8:
        out["gate"] = dict(verdict="INSUFFICIENT_SAMPLE", n=int(F.size))
        return out

    # f_syn 可为 0（xpsd 谱在零附近被截到 0）⇒ 必须先按 calibrate 的 good 掩膜取子集，
    # 否则 log10(0) = -inf 会污染 IRLS 内点集（本脚本首轮即以此报错）。
    good = (np.isfinite(F) & (F > 0) & np.isfinite(fs) & (fs > 0))
    F, fs = F[good], fs[good]
    idx_sel = idx_sel[good]
    sat_sel = sat_stamp[sel][good]
    x_sel = g["x"][sel][good]
    y_sel = g["y"][sel][good]
    fe_sel = g["flux_err"][sel][good]
    chi2_sel = g["chi2"][sel][good]
    out["selection"]["n_positive_fsyn"] = int(F.size)
    if F.size < 8:
        out["gate"] = dict(verdict="INSUFFICIENT_SAMPLE", n=int(F.size))
        return out
    cal = calibrate(F, fs, None, x_sel, y_sel, m_degree=2)
    r_all = np.log10(F / fs)
    ir = sc.irls_tukey(r_all)
    assert int(ir["n_in"]) == int(cal["n_inliers"]), "IRLS inlier 与 calibrate 不一致"
    inl = np.asarray(ir["inliers"])
    loc = float(ir["location"])
    res_mag = 2.5 * (r_all - loc)                      # 逐星星等残差（全部选中样本）
    out["calibration"] = {k: v for k, v in cal.items()
                          if k not in ("resid_mag_inliers", "m_coeffs")}
    out["calibration"]["n_m_terms"] = len(cal["m_coeffs"]) if cal["m_coeffs"] else 0

    # ---- PSF vs 孔径 ----
    f_ap4 = aperture_flux(img, x_sel, y_sel, r_ap=4.0, r_in=6.0, r_out=10.0)
    sysres = psf_vs_aperture_systematics(F, f_ap4, fe_sel)
    f_ap10 = aperture_flux(img, x_sel, y_sel, r_ap=10.0, r_in=12.0, r_out=18.0)
    sysres10 = psf_vs_aperture_systematics(F, f_ap10, fe_sel)
    out["psf_vs_aperture"] = dict(r4=sysres, r10=sysres10)

    # ---- 预算（逐字）----
    sigma_pix_e = float(np.sqrt(inst.read_noise ** 2 + sky_med * inst.gain))
    struct = float(max(1.0, bg_rms_adu / (sigma_pix_e / inst.gain)))
    TT = tag if tag is not None else ("s04" + tag_suffix)
    b = pl.build_budget(F, inst, sigma_pix_e, sysres["sigma_psfsys"], 0.0, 0.0,
                        float(SIGMA_FLAT_HF_CANONICAL), struct, cal["n_inliers"], tag=TT)
    # 修订前口径（σ_flat = 自指 delta_after_m）与 σ_flat = 0 的对照，同一 tag 取项
    b_selfref = pl.build_budget(F, inst, sigma_pix_e, sysres["sigma_psfsys"], 0.0, 0.0,
                                float(cal["delta_after_m"]), struct, cal["n_inliers"],
                                tag=TT)
    b_flat0 = pl.build_budget(F, inst, sigma_pix_e, sysres["sigma_psfsys"], 0.0, 0.0,
                              0.0, struct, cal["n_inliers"], tag=TT)
    out["gate"] = dict(
        n=int(F.size), n_inliers=int(cal["n_inliers"]),
        k_photo=float(cal["k_photo"]), location_dex=loc,
        sigma_obs_mag=float(cal["sigma_obs_mag"]),
        sigma_floor=float(b.sigma_floor), sigma_ceiling=float(b.sigma_ceiling),
        verdict=sc.gate_verdict(cal["sigma_obs_mag"], b),
        gate_scope=b.gate_scope, lower_bound_defined=bool(b.lower_bound_defined),
        budget=b.as_dict(),
        pre_revision=dict(sigma_flat_used=float(cal["delta_after_m"]),
                          sigma_ceiling=float(b_selfref.sigma_ceiling),
                          verdict=sc.gate_verdict(cal["sigma_obs_mag"], b_selfref)),
        sigma_flat0=dict(sigma_ceiling=float(b_flat0.sigma_ceiling),
                         verdict=sc.gate_verdict(cal["sigma_obs_mag"], b_flat0)),
        items=dict(sigma_pix_e=sigma_pix_e, structure_factor=struct,
                   sigma_psfsys_inframe_r4=sysres["sigma_psfsys"],
                   sigma_psfsys_inframe_r10=sysres10["sigma_psfsys"],
                   sigma_flat_canonical=float(SIGMA_FLAT_HF_CANONICAL),
                   sigma_color_in_budget=0.0, sigma_gaia_in_budget=0.0,
                   delta_after_m_diagnostic=float(cal["delta_after_m"]),
                   delta_after_m_role="diagnostic_only（与被测统计量同源，不进预算）"))
    items = dict(sigma_fit_robust=b.sigma_fit_robust, sigma_psfsys=b.sigma_psfsys,
                 sigma_color=b.sigma_color, sigma_gaia=b.sigma_gaia,
                 sigma_flat=b.sigma_flat, sigma_skyres=b.sigma_skyres, sigma_q=b.sigma_q)
    tot = float(sum(v * v for v in items.values()))
    out["variance_share"] = {k: (float(v * v / tot) if tot > 0 else None)
                             for k, v in items.items()}

    # ---- 逐星表 ----
    cols = dict(
        star_index=idx_sel.astype(int),
        ra=cat.ra[idx_sel], dec=cat.dec[idx_sel], mag_g=cat.magG[idx_sel],
        x=x_sel, y=y_sel, flux_adu=F, flux_err_adu=fe_sel,
        chi2=chi2_sel, f_syn_R=fs,
        r_dex=r_all, resid_mag=res_mag, inlier=inl,
        saturated=sat_sel,
        aperture_flux_r10_adu=f_ap10,
        snr=np.where(fe_sel > 0, F / fe_sel, np.nan),
    )
    # 参考色轴与备用通带
    axes = dict()
    for name, c in curves.items():
        if name == "R":
            continue
        axes[name] = synth_flux(cat, idx_sel, *c)
    if "B" in axes:
        cols["f_syn_B"] = axes["B"]
        cols["col_RminusB"] = -2.5 * np.log10(fs / axes["B"])
    if "G" in axes:
        cols["f_syn_G"] = axes["G"]
        cols["col_GminusR"] = -2.5 * np.log10(axes["G"] / fs)
    # 位置几何
    cols["r_from_center_px"] = np.hypot(cols["x"] - W / 2.0, cols["y"] - H / 2.0)
    if extra_columns:
        cols.update(extra_columns(g, F, fs, inl, img, hdr))
    if keep_spectra:
        cols["spectra"] = np.array([cat.spectrum(int(i)) for i in idx_sel], float)
        cols["wl_nm"] = cat.wl_nm
    out["perstar"] = cols
    out["wall_time_s"] = float(time.perf_counter() - t0)
    return out


# --------------------------------------------------------------------------
# 分层与统计工具
# --------------------------------------------------------------------------
def mad_sigma_mag(x):
    """稳健 σ 的星等口径：MAD(x)/0.6744897501960817 [mag]。"""
    x = np.asarray(x, float)
    x = x[np.isfinite(x)]
    if x.size < 2:
        return float("nan")
    return float(np.median(np.abs(x - np.median(x))) / 0.6744897501960817)


def ols_with_ci(y, X, n_boot=2000, seed=20261014, cluster=None):
    """OLS + 非参数 bootstrap 置信区间。

    cluster: 整数数组，指出同一物理观测单元（例如同一颗星的多次观测）。
             **bootstrap 按 cluster 重采样**，避免把同一颗星的重复观测当独立样本。
    返回 dict(coef, se_analytic, ci_lo, ci_hi, n, n_clusters)
    """
    y = np.asarray(y, float)
    X = np.asarray(X, float)
    if X.ndim == 1:
        X = X[:, None]
    n, p = X.shape
    XtX = X.T @ X
    XtXi = np.linalg.pinv(XtX)
    coef = XtXi @ (X.T @ y)
    resid = y - X @ coef
    dof = max(n - p, 1)
    s2 = float(resid @ resid / dof)
    se = np.sqrt(np.maximum(np.diag(s2 * XtXi), 0.0))
    rng = np.random.default_rng(seed)
    if cluster is None:
        cluster = np.arange(n)
    cluster = np.asarray(cluster)
    uniq = np.unique(cluster)
    boot = np.full((n_boot, p), np.nan)
    idx_by_c = [np.nonzero(cluster == c)[0] for c in uniq]
    for b in range(n_boot):
        pick = rng.integers(0, len(uniq), len(uniq))
        rows = np.concatenate([idx_by_c[k] for k in pick])
        Xb, yb = X[rows], y[rows]
        try:
            boot[b] = np.linalg.pinv(Xb.T @ Xb) @ (Xb.T @ yb)
        except np.linalg.LinAlgError:
            pass
    ci_lo = np.nanpercentile(boot, 2.5, axis=0)
    ci_hi = np.nanpercentile(boot, 97.5, axis=0)
    return dict(coef=coef.tolist(), se_analytic=se.tolist(),
                ci95_lo=ci_lo.tolist(), ci95_hi=ci_hi.tolist(),
                n=int(n), n_clusters=int(len(uniq)), n_boot=int(n_boot), seed=int(seed))


def effective_n_clusters(cluster):
    """有效独立样本数 = 互异物理观测单元数（本文档口径：同一颗星跨帧重复不是独立样本）。"""
    return int(np.unique(np.asarray(cluster)).size)
