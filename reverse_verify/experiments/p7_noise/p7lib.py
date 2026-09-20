#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7-SYNTHETIC-NOISE 实验共享库：理论 SNR 方程、PSF 域最优提取、注入-回收、SP-0 散差度量。

**判据先行**：各 exp 脚本顶部写死阈值；本库只提供估计量与解析预测，不含阈值。

理论 SNR 方程（逐条给一手出处，报告 §1 有核验记录）
----------------------------------------------------
1) **CCD 方程**（Howell 2006, *Handbook of CCD Astronomy* 2nd ed., DOI 10.1017/CBO9780511807909；
   同 Merline & Howell 1995, Exp. Astron. 6, 163, DOI 10.1007/BF00421131）::

       SNR = N* / sqrt( N* + n_pix*( N_S + N_D + N_R^2 ) )            [电子域]

   N* 源电子；n_pix 孔径内像素数；N_S/N_D 每像素天光/暗电流电子；N_R 每像素读出噪声电子。

2) **PSF 加权最优提取**（Horne 1986, PASP 98, 609, DOI 10.1086/131801；Naylor 1998, MNRAS 296, 339）::

       F_hat  = sum_i (P_i/sigma_i^2)(I_i - b) / sum_i (P_i^2/sigma_i^2)
       sigma_F = 1/sqrt( sum_i P_i^2/sigma_i^2 )
       SNR     = F/sigma_F = F*sqrt(sum_i P_i^2/sigma_i^2) / sigma_pix   （sigma 空间均匀时）

3) **LSST SMTN-002**（Jones, DOI 10.71929/rubin/3408482）::

       SNR = C / sqrt( C/g + (B/g + sigma_instr^2)*n_eff )
       n_eff = 2.266 * (FWHM_eff / pixelScale)^2

4) **光子传递曲线 PTC**（Janesick 2001, *Scientific Charge-Coupled Devices*, DOI 10.1117/3.374903）::

       Var[ADU] = (1/g)*Mean_net[ADU] + ( sigma_R^2/g^2 + 1/12 )

单位：本库内部一律用 **电子域**；ADU 只在渲染/落盘处出现。
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

HERE = Path(__file__).resolve()
RV = HERE.parents[2]                       # reverse_verify/
ROOT = RV.parent
sys.path.insert(0, str(RV / "synthetic"))
sys.path.insert(0, str(RV / "experiments" / "f_instr"))
import noise_model as NM      # noqa: E402
import render as R            # noqa: E402
import m16_scene as M16       # noqa: E402

QUANT_VAR_ADU2 = 1.0 / 12.0
P7_DATA = ROOT / "run" / "RELEASE-02" / "paper" / "data" / "P7"


# ---------------------------------------------------------------------------
# 0. 理论 SNR 方程
# ---------------------------------------------------------------------------
def snr_ccd_equation(*, src_e: float, n_pix: float, sky_e_per_pix: float,
                     dark_e_per_pix: float, read_noise_e: float) -> float:
    """CCD 方程（Howell 2006 / Merline & Howell 1995）：SNR = N*/sqrt(N* + n_pix*(N_S+N_D+N_R^2))。"""
    var = max(float(src_e), 0.0) + float(n_pix) * (
        max(float(sky_e_per_pix), 0.0) + max(float(dark_e_per_pix), 0.0)
        + float(read_noise_e) ** 2)
    return float(src_e) / math.sqrt(var) if var > 0 else 0.0


def n_eff_smtn002(*, fwhm_px: float, pixel_scale_arcsec: float) -> float:
    """LSST SMTN-002 的有效噪声像素数 n_eff = 2.266*(FWHM/pixelScale)^2。"""
    return 2.266 * (float(fwhm_px) / float(pixel_scale_arcsec)) ** 2


def snr_smtn002(*, counts_e: float, sky_e_per_pix: float, read_noise_e: float,
                gain_e_per_adu: float = 1.0, fwhm_px: float,
                pixel_scale_arcsec: float) -> float:
    """LSST SMTN-002 形式（电子域等价写法，g=1 时与原文 ADU 写法一致）。"""
    g = float(gain_e_per_adu)
    neff = n_eff_smtn002(fwhm_px=fwhm_px, pixel_scale_arcsec=pixel_scale_arcsec)
    var = counts_e / g + (sky_e_per_pix / g + read_noise_e ** 2) * neff
    return counts_e / math.sqrt(var) if var > 0 else 0.0


def snr_psf_weighted(*, flux_e: float, sum_p2: float, sigma_pix_e: float) -> float:
    """PSF 加权最优提取的 SNR（空间均匀 sigma）。sum_p2 = sum_i P_i^2（离散归一化 PSF）。"""
    if sigma_pix_e <= 0:
        return math.inf
    return float(flux_e) * math.sqrt(float(sum_p2)) / float(sigma_pix_e)


def sigma_pix_e(*, src_e_per_pix: float, sky_e_per_pix: float, dark_e_per_pix: float,
                read_noise_e: float) -> float:
    """逐像素电子域噪声（不含量化；量化只在 ADU 域出现）。"""
    return math.sqrt(max(src_e_per_pix + sky_e_per_pix + dark_e_per_pix, 0.0)
                     + read_noise_e ** 2)


def sum_p2_of_kernel(kern: np.ndarray) -> float:
    """离散归一化 PSF 的 sum P_i^2。"""
    k = np.asarray(kern, dtype=float)
    k = k / k.sum()
    return float((k ** 2).sum())


# ---------------------------------------------------------------------------
# 1. 估计量
# ---------------------------------------------------------------------------
def aperture_photometry(img_adu: np.ndarray, y: float, x: float, r_pix: float,
                        *, ann_r_in: float, ann_r_out: float,
                        gain: float, bias: float) -> Dict[str, float]:
    """圆孔径 + 环形局部天光（在**真正渲染的像素**上做，自动含结构泄漏）。"""
    ny, nx = img_adu.shape
    y0, y1 = max(int(y - ann_r_out - 1), 0), min(int(y + ann_r_out + 2), ny)
    x0, x1 = max(int(x - ann_r_out - 1), 0), min(int(x + ann_r_out + 2), nx)
    sub = np.asarray(img_adu[y0:y1, x0:x1], dtype=float) - bias
    yy, xx = np.mgrid[y0:y1, x0:x1]
    rr = np.hypot(yy - y, xx - x)
    ap = rr <= r_pix
    an = (rr >= ann_r_in) & (rr <= ann_r_out)
    n_ap = float(ap.sum())
    if n_ap == 0 or an.sum() < 4:
        return {"flux_adu": float("nan"), "n_ap": n_ap, "sky_per_pix_adu": float("nan")}
    sky_pp = float(np.median(sub[an]))
    flux = float(sub[ap].sum() - sky_pp * n_ap)
    return {"flux_adu": flux, "flux_e": flux * gain, "n_ap": n_ap,
            "sky_per_pix_adu": sky_pp, "sky_per_pix_e": sky_pp * gain}


def shifted_kernel(kern: np.ndarray, y: float, x: float) -> Tuple[np.ndarray, int, int]:
    """复刻 render.stamp 的**双线性亚像素位移**，返回 (K_eff, y0, x0)。

    render.stamp 把核按 4 个整数位移 (dy,dx) ∈ {0,1}² 以权重 w=wy*wx 叠加，
    基准角 (floor(y)-kh//2, floor(x)-kw//2)。故有效核

        K_eff[j,i] = sum_{dy,dx} w(dy,dx) * kernel[j-dy, i-dx]      shape (kh+1, kw+1)

    **这一步是必需的**：若估计量用未位移的核去拟合被亚像素位移过的星，
    会系统性丢通量（本工作区首版即因此得到 fwhm=1px 时 +0.45 mag 的假偏置）。
    """
    k = np.asarray(kern, dtype=float)
    k = k / k.sum()
    kh, kw = k.shape
    fy = float(y) - math.floor(float(y))
    fx = float(x) - math.floor(float(x))
    eff = np.zeros((kh + 1, kw + 1), dtype=float)
    for dy, wy in ((0, 1.0 - fy), (1, fy)):
        for dx, wx in ((0, 1.0 - fx), (1, fx)):
            w = wy * wx
            if w == 0.0:
                continue
            eff[dy:dy + kh, dx:dx + kw] += w * k
    y0 = int(math.floor(y)) - kh // 2
    x0 = int(math.floor(x)) - kw // 2
    return eff, y0, x0


def psf_optimal_flux(img_adu: np.ndarray, y: float, x: float, kern: np.ndarray,
                     *, gain: float, bias: float, sigma_pix_adu: float,
                     local_bg: Optional[float] = None,
                     bg_ann: Tuple[float, float] = (8.0, 14.0),
                     shift_kernel: bool = True) -> Dict[str, float]:
    """PSF 域最优提取（Horne 1986, PASP 98, 609）：F = sum(P/s^2 (I-b)) / sum(P^2/s^2)，单位电子。

    * 核按星点**真实亚像素相位**做双线性位移（与 render.stamp 一致）；
    * 局部本底 b 由环形中位数给出（若未给）；也可由调用者传**真值本底**做精确零假设；
    * sigma_pix 在 ADU 域先取背景 rms，再用 sqrt(s^2 + F*P/g) 迭代一次（弱化亮星处低估）。
    """
    img = np.asarray(img_adu, dtype=float)
    if shift_kernel:
        k, y0, x0 = shifted_kernel(kern, y, x)
    else:
        kk = np.asarray(kern, dtype=float); kk = kk / kk.sum()
        k = kk; y0 = int(round(y)) - kk.shape[0] // 2; x0 = int(round(x)) - kk.shape[1] // 2
    ky, kx = k.shape
    if y0 < 0 or x0 < 0 or y0 + ky > img.shape[0] or x0 + kx > img.shape[1]:
        return {"flux_e": float("nan"), "snr": float("nan"), "n_pix": 0}
    sub = img[y0:y0 + ky, x0:x0 + kx] - bias
    if local_bg is None:
        yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
        rr = np.hypot(yy - y, xx - x)
        an = (rr >= bg_ann[0]) & (rr <= bg_ann[1])
        local_bg = float(np.median(img[an] - bias))
    var = np.full(k.shape, max(sigma_pix_adu, 1e-9) ** 2)
    w = k / var
    F0 = float((w * (sub - local_bg)).sum() / (k * w).sum()) if (k * w).sum() > 0 else 0.0
    if F0 > 0:
        var = var + F0 * k / max(gain, 1e-9)
    w = k / var
    denom = float((k * w).sum())
    F = float((w * (sub - local_bg)).sum() / denom) if denom > 0 else float("nan")
    sig_F_adu = 1.0 / math.sqrt(denom) if denom > 0 else float("nan")
    return {"flux_e": F * gain, "sigma_F_e": sig_F_adu * gain,
            "snr": (F * gain) / (sig_F_adu * gain) if sig_F_adu > 0 else float("nan"),
            "n_pix": int(k.size), "local_bg_adu": float(local_bg)}


def clipped_mean(x: np.ndarray, mask: Optional[np.ndarray] = None,
                 *, clip_sigma: float = 5.0, iters: int = 3) -> float:
    """与 paired_var_adu2 **同一套** 5σ 迭代裁剪后的均值（供"该像素子集的有效源电子数"用）。"""
    v = np.asarray(x, dtype=float)
    v = v[mask] if mask is not None else v.ravel()
    v = v[np.isfinite(v)]
    for _ in range(max(int(iters), 1)):
        mu = float(np.mean(v)); sd = float(np.std(v))
        if sd <= 0:
            break
        keep = np.abs(v - mu) <= clip_sigma * sd
        if keep.all():
            break
        v = v[keep]
    return float(np.mean(v)) if v.size else float("nan")


def robust_sigma(x: np.ndarray) -> float:
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x)]
    if x.size < 2:
        return float("nan")
    return float(1.4826 * np.median(np.abs(x - np.median(x))))


def paired_var_adu2(i1: np.ndarray, i2: np.ndarray,
                    mask: Optional[np.ndarray] = None,
                    *, clip_sigma: float = 5.0, iters: int = 3
                    ) -> Tuple[np.ndarray, float]:
    """配对差分方差估计（**无偏**）：Var = <(I1-I2)^2>/2；返回 (逐像素图, 标量估计)。

    **必须用均值、不能用中位数**（本工作区实测踩过的坑，登记备查）：
    d = I1-I2 服从 N(0, 2σ²) ⇒ d²/2 是 σ² 的无偏估计（E=σ²），
    但 median(χ²₁) = 0.4549 ⇒ median(d²/2) = 0.4549σ²，**系统性偏低 54.5%**
    （σ 偏低 32.6%）。首版即因此得到 S2 相对偏差 -0.3266 的假失败。
    这里用 σ 裁剪均值（剔除宇宙线/热像素尾）以兼顾无偏与稳健。
    """
    d = np.asarray(i1, dtype=float) - np.asarray(i2, dtype=float)
    vm_full = 0.5 * d * d                     # **全尺寸**方差图（供 PSF 域噪声传播用）
    v = vm_full[mask] if mask is not None else vm_full.ravel()
    v = v[np.isfinite(v)]
    for _ in range(max(int(iters), 1)):
        mu = float(np.mean(v)); sd = float(np.std(v))
        if sd <= 0:
            break
        keep = np.abs(v - mu) <= clip_sigma * sd
        if keep.all():
            break
        v = v[keep]
    return vm_full, (float(np.mean(v)) if v.size else float("nan"))


# ---------------------------------------------------------------------------
# 2. SP-0 散差度量（跨帧空间形状差异）
# ---------------------------------------------------------------------------
def sp0_ratio(var_f: np.ndarray) -> Dict[str, Any]:
    """SP-0 判据：帧级标量权重 vs 逐像素逆方差权重的方差代价。

    var_f : (n_frames, ny, nx) 逐帧逐像素**真值方差** [ADU^2]
        w_f(p) = 1/var_f(p)
        W_f    = 1/mean_p(var_f(p))
        delta_f(p) = W_f/w_f(p) - 1
        R_SP0(p)   = [sum_f W_f^2/w_f(p)]*[sum_f w_f(p)] / (sum_f W_f)^2
    形状相同时 delta_f(p) 与 f 无关 ⇒ std_f(delta_f)=0 且 R_SP0 ≡ 1。
    """
    v = np.asarray(var_f, dtype=float)
    nf = v.shape[0]
    w = 1.0 / v
    W = 1.0 / v.reshape(nf, -1).mean(axis=1)          # (nf,)
    Wb = W.reshape(nf, 1, 1)
    delta = Wb * v - 1.0                               # W_f/w_f - 1 = W_f*var_f - 1
    d_shape = np.median(np.std(delta, axis=0))
    num = (Wb ** 2 * v).sum(axis=0) * w.sum(axis=0)
    den = float(W.sum()) ** 2
    r = num / den
    return {"R_SP0_map": r, "R_SP0_median": float(np.median(r)),
            "R_SP0_p10": float(np.percentile(r, 10)),
            "R_SP0_p90": float(np.percentile(r, 90)),
            "D_shape_median": float(d_shape),
            "delta_f_std_per_frame": [float(x) for x in np.std(delta, axis=(1, 2))]}


# ---------------------------------------------------------------------------
# 3. 渲染辅助
# ---------------------------------------------------------------------------
def scene_file(scene_id: str) -> Path:
    """场景 id -> scenes/<id>.json（带存在性检查）。"""
    p = M16.SCENES_DIR / ("%s.json" % scene_id)
    if not p.exists():
        raise FileNotFoundError("scene not found: %s (expected %s)" % (scene_id, p))
    return p


def render_scene_frame(scene_path: str, seed: int, frame_index: int = 0,
                       overrides: Optional[Dict[str, Any]] = None
                       ) -> Tuple[NM.Frame, Dict[str, Any], np.ndarray, Dict[str, Any]]:
    """渲染一帧 M16 场景（可叠加 overrides），返回 (Frame, truth, valid, scene)。"""
    sc = M16.load_scene(scene_file(scene_path))
    if overrides:
        sc = R.deep_merge(sc, overrides)
    fr, truth, valid = M16.render_m16_frame(sc, seed=seed, frame_index=frame_index)
    return fr, truth, valid, sc


def truth_variance_adu2(frame: NM.Frame, det: NM.Detector) -> np.ndarray:
    """逐像素**真值方差** [ADU^2]（解析，含源/天光/暗散粒 + 读出 + 量化）。"""
    g = det.gain_e_per_adu
    return (frame.truth_e / (g * g)
            + det.read_noise_e ** 2 / (g * g)
            + QUANT_VAR_ADU2)


def det_of(scene: Dict[str, Any]) -> NM.Detector:
    d = dict(scene["detector"])
    rn0 = float(d.get("read_noise_e", 3.1))
    d["read_noise_e"] = rn0 * math.sqrt(max(int(scene.get("stack_n", 32)), 1))  # STACKN32-001: 全局默认 32
    return NM.Detector(**d)


def dump_json(path, obj) -> None:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, indent=2, ensure_ascii=False, default=float),
                 encoding="utf-8")
    print("[p7lib] wrote %s" % p)
