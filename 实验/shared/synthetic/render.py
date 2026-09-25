#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACSD RELEASE-02 / DATA-TYPE-MATRIX —— 参数化**场景渲染器**（场景配方 → 帧）。

上游：noise_model.py（物理噪声过程）。本模块只负责把"场景配方（JSON）"变成
"期望电子面 + 一帧 ADU"，并把**用了什么**逐项写进产物（provenance），供论文方法节引用。

场景配方字段（全部可缺省；缺省值见 DEFAULTS）
------------------------------------------------
  scene_id / kind / description        : 标识与数据类型（kind 见 data/README.md 矩阵）
  shape            [ny, nx]            : 帧尺寸 [pix]
  psf              {model, fwhm_px, beta}
                   model ∈ {moffat4, moffat, gaussian}；**必须记录用了哪个**
                   moffat4 : beta=4，sigma = fwhm/1.230310（生产 canon 约定，二阶矩等效）
                   moffat  : 自由 beta（beta>2），sigma 同上式推广
                   gaussian: sigma = fwhm/2.354820
  detector         {gain_e_per_adu, read_noise_e, bias_adu, full_well_e,
                    dark_current_e_per_s, dark_ref_temp_c, dark_double_temp_c, ...}
  exposure_s / temp_c                  : 曝光时间 [s] / 探测器温度 [C]
  flat             {prnu_rms, low_order, tilt_x, tilt_y, vignette}
  sky              {level_e_per_s, grad_x_e_per_s, grad_y_e_per_s, grad_quad_e_per_s,
                    theta_deg, moon_halo_e_per_s, moon_center, moon_scale_px}
  nebula           [ {type, ...}, ... ]   type ∈ {powerlaw_core, gaussian_blob,
                                             exponential_disk, gaussian_ring, filament}
  stars            {n, flux_log10_range:[lo,hi], clustering, cluster_n, cluster_sigma_px,
                    canvas_margin_px, seed, avoid_nebula_core}
  real_base        {path, hdu, crop:[y0,x0,h,w] | auto_bright:{size,block,mode},
                    e_per_adu, subtract_sky, smooth_sigma_px, flux_scale}
  artifacts        {cr_rate_per_frame, cr_mean_charge_e, hot_pixel_fraction,
                    hot_pixel_dark_gain}
  mode             physical | additive | mean_only | variance_override（默认 physical）
  pointing         {offset_px:[dy,dx]}   望远镜指向平移（同一天区不同指向 = 马赛克）
  frames           [ {每帧覆盖字段}, ... ]  可选：序列/马赛克；缺省单帧

物理正确性要点（不得违背）
--------------------------
  * 星点/星云先叠加为**期望电子率面**，再做 PSF 卷积（在率面上做，线性），
    **然后**才进入泊松采样 —— 即噪声是"在真实场景之上"采出来的；
  * 天光面 sky(x,y) 是**逐像素泊松均值**，因此 B↑ 必然带来方差↑；
  * 真实数据作底时，真实帧先被**平滑去噪**再当作期望面（否则会把原帧噪声二次采样），
    残差噪声方差份额 = 1/(4*pi*sigma_k^2)，逐帧登记（见 real_base_surface 的返回值）。

诚实边界
--------
  * 真实帧是**归一化产品**（float32，任意 ADU 标度），不是原始帧；因此
    e_per_adu 是**显式的场景参数**，不得据此反推任何真实仪器参数（§9.42）；
  * 平滑去噪会同时展宽真实星点：base 特征的有效 PSF = sqrt(PSF_real^2 + sigma_k^2)，
    逐帧登记；注入星点用的是配置 PSF（不受此影响）；
  * 未建模：溢出/辉散、非线性、CTE、fringing、云、导星误差、色差。

CLI
---
  python3 render.py --list-scenes
  python3 render.py --scene scenes/nebula_core_m42_analytic.json \
      --out ../../../run/reverse_verify/data_matrix/nebula_core --seed 20260919
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import noise_model as NM  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]          # 仓库根（/workspace/Astro CS Database）
SCENES_DIR = Path(__file__).resolve().parent / "scenes"

DEFAULTS: Dict[str, Any] = {
    "shape": [1024, 1024],
    "psf": {"model": "moffat4", "fwhm_px": 3.0, "beta": 4.0},
    "detector": {},
    "exposure_s": 300.0,
    "temp_c": -20.0,
    "flat": {"prnu_rms": 0.01, "low_order": 0.02, "tilt_x": 1.0, "tilt_y": 0.5,
             "vignette": 0.05},
    "sky": {"level_e_per_s": 0.5},
    "nebula": [],
    "stars": {"n": 300, "flux_log10_range": [3.0, 6.0]},
    "artifacts": {"cr_rate_per_frame": 0.0, "cr_mean_charge_e": 1000.0,
                  "hot_pixel_fraction": 0.0},
    "mode": NM.MODE_PHYSICAL,
    "pointing": {"offset_px": [0, 0]},
}


# ---------------------------------------------------------------------------
# 0. 工具
# ---------------------------------------------------------------------------
def deep_merge(base: Dict[str, Any], over: Dict[str, Any]) -> Dict[str, Any]:
    out = copy.deepcopy(base)
    for k, v in (over or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = deep_merge(out[k], v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def load_scene(path: str | Path) -> Dict[str, Any]:
    p = Path(path)
    if not p.is_absolute():
        cand = [Path.cwd() / p, SCENES_DIR / p.name]
        p = next((c for c in cand if c.exists()), cand[0])
    text = p.read_text(encoding="utf-8")
    if p.suffix.lower() in (".yaml", ".yml"):
        import yaml  # 可选依赖
        raw = yaml.safe_load(text)
    else:
        raw = json.loads(text)
    return deep_merge(DEFAULTS, raw)


def resolve_path(p: str | Path) -> Path:
    q = Path(p)
    return q if q.is_absolute() else (ROOT / q)


# ---------------------------------------------------------------------------
# 1. PSF（可配置；**必须记录用了哪个**）
# ---------------------------------------------------------------------------
def psf_kernel(psf: Dict[str, Any]) -> Tuple[np.ndarray, Dict[str, Any]]:
    """离散归一化 PSF 核（sum = 1）+ 元数据。"""
    model = str(psf.get("model", "moffat4")).lower()
    fwhm = float(psf["fwhm_px"])
    if model in ("moffat4", "moffat"):
        beta = float(psf.get("beta", 4.0))
        if beta <= 2.0:
            raise ValueError("Moffat beta must be > 2 for a finite second moment")
        if abs(beta - 4.0) < 1e-9:
            sigma = fwhm / 1.230310          # 生产 canon 约定（二阶矩等效）
        else:
            # 自由 beta：alpha = fwhm / (2*sqrt(2^(1/beta)-1))；sigma_2nd = alpha/sqrt(beta-2)
            alpha = fwhm / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
            sigma = alpha / math.sqrt(beta - 2.0)
        half = int(max(8, math.ceil(6.0 * fwhm)))
        j, i = np.mgrid[-half:half + 1, -half:half + 1]
        r2 = (i.astype(float) ** 2) + (j.astype(float) ** 2)
        alpha2 = 2.0 * sigma * sigma
        v = (1.0 + r2 / alpha2) ** (-beta)
        v = v / v.sum()
        meta = {"model": model, "fwhm_px": fwhm, "beta": beta,
                "sigma_px": sigma, "half_px": half,
                "convention": "alpha^2 = 2*sigma^2; sigma = fwhm/1.230310 (beta=4, "
                              "second-moment equivalent; same as frame_snr_canon)"}
    elif model in ("gaussian", "gauss"):
        sigma = fwhm / NM.GAUSSIAN_FWHM_OVER_SIGMA
        half = int(max(8, math.ceil(6.0 * fwhm)))
        j, i = np.mgrid[-half:half + 1, -half:half + 1]
        r2 = (i.astype(float) ** 2) + (j.astype(float) ** 2)
        v = np.exp(-r2 / (2.0 * sigma * sigma))
        v = v / v.sum()
        meta = {"model": "gaussian", "fwhm_px": fwhm, "sigma_px": sigma, "half_px": half,
                "convention": "sigma = fwhm/2.354820"}
    else:
        raise ValueError("unknown PSF model %r" % model)
    meta["sum_p2"] = float(np.sum(v ** 2))          # canon 式 (2.7) 用
    meta["a_nea_pix"] = 1.0 / meta["sum_p2"]
    return v, meta


def stamp(kernel: np.ndarray, canvas: np.ndarray, y: float, x: float, amp: float) -> None:
    """把核按 amp 叠到 canvas 的 (y,x)（双线性亚像素位移，保证质心正确）。"""
    kh, kw = kernel.shape
    y0 = int(math.floor(y)) - kh // 2
    x0 = int(math.floor(x)) - kw // 2
    fy = y - math.floor(y)
    fx = x - math.floor(x)
    # 4 个整数位移的线性组合 = 双线性插值（核的位移）
    ny, nx = canvas.shape
    for dy, wy in ((0, 1.0 - fy), (1, fy)):
        for dx, wx in ((0, 1.0 - fx), (1, fx)):
            w = wy * wx
            if w == 0.0:
                continue
            yy, xx = y0 + dy, x0 + dx
            sy0, sy1 = max(yy, 0), min(yy + kh, ny)
            sx0, sx1 = max(xx, 0), min(xx + kw, nx)
            if sy0 >= sy1 or sx0 >= sx1:
                continue
            canvas[sy0:sy1, sx0:sx1] += w * amp * kernel[
                sy0 - yy:sy1 - yy, sx0 - xx:sx1 - xx
            ]


# ---------------------------------------------------------------------------
# 2. 源面（星云 + 星点 + 可选真实底）
# ---------------------------------------------------------------------------
def _dist2(shape: Tuple[int, int], cy: float, cx: float) -> np.ndarray:
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    return (yy - cy) ** 2 + (xx - cx) ** 2


def nebula_surface(shape: Tuple[int, int], comps: List[Dict[str, Any]]) -> np.ndarray:
    """解析星云面 [e-/pix/s]（在**期望率面**上叠加，线性）。"""
    out = np.zeros(shape, dtype=float)
    for c in comps or []:
        t = str(c.get("type", "gaussian_blob"))
        cy, cx = c.get("center", [shape[0] / 2.0, shape[1] / 2.0])
        peak = float(c.get("peak_e_per_s", 1.0))
        if t == "gaussian_blob":
            s = float(c.get("sigma_px", 30.0))
            out += peak * np.exp(-_dist2(shape, cy, cx) / (2.0 * s * s))
        elif t == "powerlaw_core":
            rc = float(c.get("core_radius_px", 5.0))
            slope = float(c.get("slope", 2.0))
            trunc = float(c.get("truncation_px", 0.0))
            r2 = _dist2(shape, cy, cx)
            v = peak * (1.0 + r2 / (rc * rc)) ** (-0.5 * slope)
            if trunc > 0:
                v = v * np.exp(-r2 / (2.0 * trunc * trunc))
            out += v
        elif t == "exponential_disk":
            rs = float(c.get("scale_px", 40.0))
            out += peak * np.exp(-np.sqrt(_dist2(shape, cy, cx)) / rs)
        elif t == "gaussian_ring":
            s = float(c.get("sigma_px", 10.0))
            r0 = float(c.get("radius_px", 60.0))
            r = np.sqrt(_dist2(shape, cy, cx))
            out += peak * np.exp(-((r - r0) ** 2) / (2.0 * s * s))
        elif t == "filament":
            y1, x1 = c.get("p1", [cy, cx])
            y2, x2 = c.get("p2", [cy, cx])
            w = float(c.get("width_px", 10.0))
            ny, nx = shape
            yy, xx = np.mgrid[0:ny, 0:nx]
            vy, vx = (y2 - y1), (x2 - x1)
            L2 = vy * vy + vx * vx
            tt = np.clip(((yy - y1) * vy + (xx - x1) * vx) / max(L2, 1e-9), 0.0, 1.0)
            d2 = (yy - (y1 + tt * vy)) ** 2 + (xx - (x1 + tt * vx)) ** 2
            out += peak * np.exp(-d2 / (2.0 * w * w))
        else:
            raise ValueError("unknown nebula component %r" % t)
    return out


def star_catalog(
    canvas_shape: Tuple[int, int],
    stars: Dict[str, Any],
    rng: np.random.Generator,
    *,
    avoid: Optional[np.ndarray] = None,
) -> List[Dict[str, float]]:
    """生成星表（位置 + 总通量 [e-]，**曝光积分后的真值**）。"""
    n = int(stars.get("n", 0))
    if n <= 0:
        return []
    ny, nx = canvas_shape
    lo, hi = stars.get("flux_log10_range", [3.0, 6.0])
    mode = str(stars.get("clustering", "uniform"))
    if mode == "clustered":
        ncl = int(stars.get("cluster_n", 6))
        csig = float(stars.get("cluster_sigma_px", 150.0))
        cy = rng.uniform(0, ny, size=ncl)
        cx = rng.uniform(0, nx, size=ncl)
        which = rng.integers(0, ncl, size=n)
        y = np.clip(cy[which] + rng.normal(0, csig, size=n), 0, ny - 1)
        x = np.clip(cx[which] + rng.normal(0, csig, size=n), 0, nx - 1)
    else:
        y = rng.uniform(0, ny, size=n)
        x = rng.uniform(0, nx, size=n)
    flux = 10.0 ** rng.uniform(lo, hi, size=n)
    if avoid is not None and stars.get("avoid_nebula_core", False):
        bad = avoid[np.clip(y.astype(int), 0, ny - 1), np.clip(x.astype(int), 0, nx - 1)]
        keep = ~bad
        y, x, flux = y[keep], x[keep], flux[keep]
    return [{"y": float(a), "x": float(b), "flux_e": float(f)} for a, b, f in zip(y, x, flux)]


def real_base_surface(
    rb: Dict[str, Any],
    shape: Tuple[int, int],
    offset_px: Tuple[int, int],
    margin: int,
) -> Tuple[np.ndarray, Dict[str, Any]]:
    """真实帧作底：读 → 选块 → 平滑去噪 → 转 e-/s 率面（含 provenance）。

    返回 (rate_e_per_s, meta)。rate 面**已扣除自身天光中位数**（天光由场景 sky 字段控制），
    并可为负（真实帧的低谷），由 expose 的 max(.,0) 兜底 —— 逐帧登记负值份额。
    """
    from astropy.io import fits  # 延迟导入
    pat = str(rb["path"])
    if any(ch in pat for ch in "*?["):        # 通配：支持"数据未到位则登记"的接口
        pats = [q.strip() for q in pat.split("|") if q.strip()]
        hits = sorted({h for q in pats for h in ROOT.glob(q) if h.is_file()})
        idx = int(rb.get("glob_index", 0))
        if not hits:
            raise FileNotFoundError(
                "real_base glob matched 0 files: patterns=%s" % (pats,))
        p = hits[min(idx, len(hits) - 1)]
        glob_info = {"pattern": pat, "n_matched": len(hits), "index": idx}
    else:
        p = resolve_path(pat)
        glob_info = None
    try:
        rel = str(p.relative_to(ROOT))
    except ValueError:
        rel = str(p)
    meta: Dict[str, Any] = {"path": rel, "available": p.exists(), "glob": glob_info}
    if not p.exists():
        raise FileNotFoundError("real_base not found: %s" % p)
    hdu = int(rb.get("hdu", 0))
    with fits.open(p, memmap=True) as h:
        d = h[hdu].data
        hdr = h[hdu].header
        ny, nx = d.shape
        crop = rb.get("crop")
        if crop is None:
            ab = rb.get("auto_bright", {"size": min(shape), "block": 256, "mode": "mean"})
            size = int(ab.get("size", min(shape)))
            block = int(ab.get("block", 256))
            by, bx = ny // block, nx // block
            sub = np.asarray(d[:by * block, :bx * block], dtype=np.float64)
            blk = sub.reshape(by, block, bx, block)
            red = blk.mean(axis=(1, 3)) if ab.get("mode", "mean") == "mean" else blk.max(axis=(1, 3))
            iy, ix = np.unravel_index(int(np.argmax(red)), red.shape)
            y0 = int(min(max(iy * block - size // 2, 0), max(ny - size, 0)))
            x0 = int(min(max(ix * block - size // 2, 0), max(nx - size, 0)))
            crop = [y0, x0, size, size]
        y0, x0, h_, w_ = [int(v) for v in crop]
        # 指向平移：马赛克不同板块取真实帧的不同区域
        dy, dx = offset_px
        y0 = int(np.clip(y0 - dy, 0, max(ny - h_, 0)))
        x0 = int(np.clip(x0 - dx, 0, max(nx - w_, 0)))
        sub = np.asarray(d[y0:y0 + h_, x0:x0 + w_], dtype=np.float64)
        if sub.shape != shape:      # 尺寸不匹配时裁剪/补零到帧尺寸（显式登记）
            fixed = np.zeros(shape, dtype=np.float64)
            hh, ww = min(shape[0], sub.shape[0]), min(shape[1], sub.shape[1])
            fixed[:hh, :ww] = sub[:hh, :ww]
            meta["crop_shape_adjusted_from"] = list(sub.shape)
            sub = fixed
        meta.update({"crop_used": [y0, x0, h_, w_], "hdu": hdu,
                     "fits_header_subset": {k: hdr.get(k) for k in
                                            ("TELESCOP", "INSTRUME", "FILTER", "EXPTIME",
                                             "DATE-OBS", "OBJECT", "FWHM", "ZMAG", "GAIN",
                                             "CCD-TEMP", "APTDIA", "XPIXSZ")
                                            if k in hdr}})
    # 参考基座：用**低百分位**而不是中位数 —— 亮星云视场的中位数就是星云本身，
    # 减去中位数会把一半星云压到负值并被 expose 的 max(.,0) 截断（会毁掉结构）。
    pct = float(rb.get("subtract_percentile", 5.0))
    ref_adu = float(np.percentile(sub, pct))
    p999 = float(np.percentile(sub, 99.9))
    if rb.get("target_p999_e") is not None:
        # 显式锚定动态范围（可复跑、可比较）：把 p99.9 映射到指定电子数
        e_per_adu = float(rb["target_p999_e"]) / max(p999 - ref_adu, 1e-9)
        e_per_adu = e_per_adu * float(rb.get("flux_scale", 1.0))
        anchor = {"target_p999_e": float(rb["target_p999_e"]), "p99.9_adu": p999,
                  "ref_percentile": pct, "ref_adu": ref_adu, "e_per_adu": e_per_adu}
    else:
        e_per_adu = float(rb.get("e_per_adu", 30.0)) * float(rb.get("flux_scale", 1.0))
        anchor = {"e_per_adu_explicit": e_per_adu, "p99.9_adu": p999,
                  "ref_percentile": pct, "ref_adu": ref_adu}
    med = ref_adu
    sig_k = float(rb.get("smooth_sigma_px", 2.0))
    if sig_k > 0:
        from scipy.ndimage import gaussian_filter
        base = gaussian_filter(sub - med, sigma=sig_k, mode="nearest")
        resid_frac = 1.0 / (4.0 * math.pi * sig_k * sig_k)
    else:
        base = sub - med
        resid_frac = 1.0
    rate = base * e_per_adu
    neg_frac = float(np.mean(rate < 0))
    top_frac = float(np.mean(sub >= 0.98 * sub.max()))
    meta.update({
        "crop_top_range_fraction": top_frac,
        "crop_top_range_note": "原始真实帧顶部编码区（含原帧饱和平顶）占比；"
                               "作底时其平顶被当作结构保留（诚实登记，非真实结构）",
        "crop_reference_adu": med, "dynamic_range_anchor": anchor,
        "smooth_sigma_px": sig_k, "e_per_adu": e_per_adu,
        "residual_base_noise_variance_fraction": resid_frac,
        "negative_rate_fraction": neg_frac,
        "effective_base_psf_note":
            "真实底特征有效 PSF = sqrt(PSF_real^2 + smooth_sigma^2)；注入星点用配置 PSF",
        "units_note": "真实帧为归一化产品（任意 ADU 标度）；e_per_adu 是场景参数，"
                      "不得据此反推真实仪器参数（GAP_AUDIT §9.42）",
    })
    return rate, meta


# ---------------------------------------------------------------------------
# 3. 渲染一帧
# ---------------------------------------------------------------------------
def render_frame(
    scene: Dict[str, Any],
    *,
    seed: int,
    frame_index: int = 0,
    canvas_cache: Optional[Dict[str, Any]] = None,
) -> Tuple[NM.Frame, Dict[str, Any]]:
    """渲染一帧：返回 (Frame, truth_meta)。"""
    shape = tuple(int(v) for v in scene["shape"])
    ny, nx = shape
    frames = scene.get("frames") or [{}]
    ov = frames[frame_index] if frame_index < len(frames) else {}
    sc = deep_merge(scene, ov)
    sc.pop("frames", None)

    rng_scene = np.random.default_rng(int(sc.get("stars", {}).get("seed", 12345)))
    rng = np.random.default_rng(int(seed))

    # --- 指向与画布 ---
    offs = [tuple(int(v) for v in (f.get("pointing", {}).get("offset_px", [0, 0])))
            for f in (scene.get("frames") or [{}])]
    # 画布外扩 = 指向平移所需（默认恰为最大平移量；无平移则 margin=0，
    # 使星云/星表坐标 == 帧坐标，避免"默认 margin 造成整体平移"的陷阱）
    need = max([max(abs(o[0]), abs(o[1])) for o in offs] + [0])
    margin = max(int(sc.get("stars", {}).get("canvas_margin_px", need)), need)
    cshape = (ny + 2 * margin, nx + 2 * margin)
    dy, dx = tuple(int(v) for v in sc.get("pointing", {}).get("offset_px", [0, 0]))

    # --- PSF ---
    kern, psf_meta = psf_kernel(sc["psf"])

    # --- 源面（画布坐标） ---
    key = ("src", id(scene), cshape)
    src_canvas = None
    cat = None
    if canvas_cache is not None and key in canvas_cache:
        src_canvas = canvas_cache[key]
        cat = canvas_cache.get(("cat", id(scene)))
    if src_canvas is None:
        neb = nebula_surface(cshape, sc.get("nebula", []))
        avoid = (neb > 0.2 * neb.max()) if (neb.size and neb.max() > 0) else None
        cat = star_catalog(cshape, sc.get("stars", {}), rng_scene, avoid=avoid)
        src_canvas = neb.copy()
        for s in cat:
            stamp(kern, src_canvas, s["y"], s["x"], s["flux_e"])
        if canvas_cache is not None:
            canvas_cache[key] = src_canvas
            canvas_cache[("cat", id(scene))] = cat
    y0 = margin - dy
    x0 = margin - dx
    src_canvas_crop = src_canvas[y0:y0 + ny, x0:x0 + nx]

    # --- 真实底 ---
    rb_meta = {"available": False, "used": False}
    rate = src_canvas_crop.copy()
    if sc.get("real_base", {}).get("path"):
        rb_rate, rb_meta = real_base_surface(sc["real_base"], shape, (dy, dx), margin)
        rb_meta["used"] = True
        rate = rate + rb_rate

    t = float(sc["exposure_s"])
    src_rate = rate / t          # [e-/s]（星表通量已含曝光积分，这里换成率）

    # --- 天光面 / 平场 / 热像素 ---
    sky = NM.sky_surface_e_per_s(shape, **{k: v for k, v in sc.get("sky", {}).items()})
    det = NM.Detector(**{k: v for k, v in sc.get("detector", {}).items()})
    # **固定图样**（关键物理约束）：平场 PRNU 与热像素是仪器的固定属性，
    # 不随曝光/帧变化 —— 因此用**场景级固定种子**，不用逐帧种子。
    # （若逐帧变化，配对差分噪声估计量会把固定图样误当噪声；实测曾使 sigma 闭合偏差达 57%。）
    flat_cfg = dict(sc.get("flat", {}))
    flat_seed = int(flat_cfg.pop("seed", 1234))
    flat = NM.flat_response(shape, np.random.default_rng(flat_seed), **flat_cfg)
    art = sc.get("artifacts", {})
    hot = NM.hot_pixel_map(shape, np.random.default_rng(int(art.get("hot_seed", 4321))),
                           float(art.get("hot_pixel_fraction", 0.0)),
                           float(art.get("hot_pixel_dark_gain", 50.0)))

    mode = str(sc.get("mode", NM.MODE_PHYSICAL))
    add_off = float(sc.get("additive_offset_adu", 0.0))
    frame = NM.expose(
        src_e_per_s=src_rate, sky_e_per_s=sky, det=det, exptime_s=t, rng=rng,
        temp_c=float(sc.get("temp_c", det.dark_ref_temp_c)), flat=flat, hot_map=hot,
        cr_rate_per_frame=float(art.get("cr_rate_per_frame", 0.0)),
        cr_mean_charge_e=float(art.get("cr_mean_charge_e", 1000.0)),
        mode=mode, variance_override_e2=sc.get("variance_override_e2"),
        additive_offset_adu=add_off,
    )

    stars_in_frame = []
    if cat is not None:
        for s in cat:
            yy, xx = s["y"] - y0, s["x"] - x0
            if 0 <= yy < ny and 0 <= xx < nx:
                stars_in_frame.append({"y": yy, "x": xx, "flux_e": s["flux_e"]})
    truth = {
        "scene_id": sc.get("scene_id"),
        "kind": sc.get("kind"),
        "frame_id": sc.get("frame_id", "%s_f%02d" % (sc.get("scene_id"), frame_index)),
        "frame_index": frame_index,
        "seed": int(seed),
        "shape": list(shape),
        "exposure_s": t,
        "temp_c": float(sc.get("temp_c", det.dark_ref_temp_c)),
        "psf": psf_meta,
        "detector": det.as_dict(),
        "sky": {"params": sc.get("sky", {}),
                "level_e_per_s_min": float(sky.min()),
                "level_e_per_s_mean": float(sky.mean()),
                "level_e_per_s_max": float(sky.max()),
                "sky_e_total_mean": float(sky.mean() * t)},
        "flat": {"params": sc.get("flat", {}), "seed": flat_seed,
                 "fixed_pattern": True, "min": float(flat.min()),
                 "max": float(flat.max()), "mean": float(flat.mean())},
        "artifacts": art,
        "mode": mode,
        "additive_offset_adu": add_off,
        "pointing": {"offset_px": [dy, dx], "canvas_margin_px": margin},
        "real_base": rb_meta,
        "stars_in_frame": stars_in_frame,
        "n_stars_in_frame": len(stars_in_frame),
        "truth_e_stats": {"min": float(frame.truth_e.min()),
                          "median": float(np.median(frame.truth_e)),
                          "max": float(frame.truth_e.max())},
        "adu_stats": {"min": float(frame.adu.min()), "median": float(np.median(frame.adu)),
                      "max": float(frame.adu.max()),
                      "saturated_pixels": int(np.count_nonzero(
                          frame.adu >= det.saturation_adu + add_off)) if det.saturate else 0},
        "provenance": frame.provenance,
    }
    return frame, truth


# ---------------------------------------------------------------------------
# 4. 数据集（一个场景 → 多帧 + manifest）
# ---------------------------------------------------------------------------
def write_frame(outdir: Path, truth: Dict[str, Any], frame: NM.Frame) -> Dict[str, str]:
    from astropy.io import fits
    outdir.mkdir(parents=True, exist_ok=True)
    fid = truth["frame_id"]
    fp = outdir / ("%s.fits" % fid)
    hdr = fits.Header()
    hdr["BUNIT"] = ("ADU", "synthetic frame unit")
    hdr["SCENEID"] = (str(truth["scene_id"]), "scene recipe id")
    hdr["KIND"] = (str(truth["kind"]), "data-type matrix cell kind")
    hdr["FRAMEID"] = (str(fid), "frame id")
    hdr["SEED"] = (int(truth["seed"]), "RNG seed")
    hdr["MODE"] = (str(truth["mode"]), "noise arm: physical|additive|mean_only|variance_override")
    hdr["PSFMODEL"] = (str(truth["psf"]["model"]), "PSF model used")
    hdr["FWHMPX"] = (float(truth["psf"]["fwhm_px"]), "PSF FWHM [pix]")
    hdr["PSFBETA"] = (float(truth["psf"].get("beta", 0.0)), "Moffat beta (0 if N/A)")
    hdr["GAIN"] = (float(truth["detector"]["gain_e_per_adu"]), "[e-/ADU]")
    hdr["RDNOISE"] = (float(truth["detector"]["read_noise_e"]), "[e-] read noise")
    hdr["EXPTIME"] = (float(truth["exposure_s"]), "[s]")
    hdr["TEMPC"] = (float(truth["temp_c"]), "[C] detector temperature")
    hdr["SKYEPS"] = (float(truth["sky"]["level_e_per_s_mean"]), "[e-/pix/s] mean sky rate")
    hdr["NSTARS"] = (int(truth["n_stars_in_frame"]), "injected stars in frame")
    hdr["REALBASE"] = (str(bool(truth["real_base"].get("used"))), "real frame used as base")
    hdr["ADDONADU"] = (float(truth["additive_offset_adu"]), "additive-only offset [ADU]")
    hdr.add_history("ACSD RELEASE-02 DATA-TYPE-MATRIX physical noise chain:")
    hdr.add_history("lam_e = t*(src+sky)*flat + t*D(T)*hot")
    hdr.add_history("n_e = Poisson(lam_e) + N(0,rdnoise) + CR")
    hdr.add_history("adu = round(n_e/gain + bias), hard-clipped at full_well")
    hdr.add_history("truth metadata: %s.meta.json (same basename)" % fid)
    fits.PrimaryHDU(data=np.asarray(frame.adu, dtype=np.float32), header=hdr).writeto(
        fp, overwrite=True)
    mp = outdir / ("%s.meta.json" % fid)
    with open(mp, "w", encoding="utf-8") as fh:
        json.dump(truth, fh, indent=1, ensure_ascii=False, default=float)
    return {"fits": str(fp), "meta": str(mp)}


def render_dataset(scene_path: str | Path, outdir: str | Path, *,
                   seed: Optional[int] = None, frames: Optional[List[int]] = None,
                   verbose: bool = True) -> Dict[str, Any]:
    scene = load_scene(scene_path)
    base_seed = int(seed if seed is not None else scene.get("seed", 20260919))
    nframes = len(scene.get("frames") or [{}])
    idxs = list(range(nframes)) if frames is None else list(frames)
    out = Path(outdir)
    out.mkdir(parents=True, exist_ok=True)
    cache: Dict[Any, Any] = {}
    man: Dict[str, Any] = {"scene_path": str(scene_path), "scene_id": scene.get("scene_id"),
                           "kind": scene.get("kind"), "base_seed": base_seed,
                           "outdir": str(out), "frames": []}
    for k in idxs:
        try:
            f, truth = render_frame(scene, seed=base_seed + k, frame_index=k,
                                    canvas_cache=cache)
        except FileNotFoundError as exc:
            # 真实数据未到位：**如实登记**，不静默跳过、不编造
            fid = ((scene.get("frames") or [{}])[k].get("frame_id")
                   if k < len(scene.get("frames") or [{}]) else None) or (
                       "%s_f%02d" % (scene.get("scene_id"), k))
            man["frames"].append({"frame_id": fid, "frame_index": k, "status": "UNAVAILABLE",
                                  "reason": str(exc)})
            if verbose:
                print("[render] %-28s UNAVAILABLE: %s" % (fid, exc))
            continue
        paths = write_frame(out, truth, f)
        rec = {"status": "OK"}
        rec.update({kk: truth[kk] for kk in ("frame_id", "frame_index", "seed", "mode", "kind",
                                        "n_stars_in_frame", "exposure_s", "psf", "adu_stats",
                                        "sky", "real_base", "pointing")})
        rec.update(paths)
        man["frames"].append(rec)
        if verbose:
            print("[render] %-28s mode=%-16s fwhm=%.2f sky=%.3f e-/s stars=%4d "
                  "adu_med=%.0f sat=%d" % (
                      truth["frame_id"], truth["mode"], truth["psf"]["fwhm_px"],
                      truth["sky"]["level_e_per_s_mean"], truth["n_stars_in_frame"],
                      truth["adu_stats"]["median"], truth["adu_stats"]["saturated_pixels"]))
    with open(out / "dataset.json", "w", encoding="utf-8") as fh:
        json.dump(man, fh, indent=1, ensure_ascii=False, default=float)
    return man


def list_scenes() -> List[str]:
    return sorted(p.name for p in SCENES_DIR.glob("*.json"))


def main() -> int:
    ap = argparse.ArgumentParser(description="DATA-TYPE-MATRIX 场景渲染器")
    ap.add_argument("--scene", type=str, help="场景配方 JSON/YAML")
    ap.add_argument("--out", type=str, help="输出目录（建议 run/reverse_verify/data_matrix/...）")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--frames", type=str, default=None, help="逗号分隔的帧号子集")
    ap.add_argument("--list-scenes", action="store_true")
    a = ap.parse_args()
    if a.list_scenes:
        for s in list_scenes():
            print(s)
        return 0
    if not a.scene or not a.out:
        ap.error("--scene and --out are required (or use --list-scenes)")
    fr = [int(v) for v in a.frames.split(",")] if a.frames else None
    man = render_dataset(a.scene, a.out, seed=a.seed, frames=fr)
    print("[render] wrote %d frame(s) -> %s" % (len(man["frames"]), a.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
