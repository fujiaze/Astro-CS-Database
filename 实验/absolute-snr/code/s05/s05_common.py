#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05 / P2 公共工具（独立 Oracle 侧，纯 numpy，不链接生产 lib/**）。

口径锚点（逐字取自正本，来源见 REPORT_S05.md §0）：
  · 逐像素方差  sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F*P_i/g      [ADU^2]
      sky 项取 shot_noise_only:  sigma_sky^2 = sky_adu/g
      g 单位 e-/ADU，RN 单位 e-，F 单位 ADU，P_i 归一（sum P_i = 1）
  · 对角 Fisher  sigma_F^{-2} = sum_i P_i^2/sigma_i^2             [ADU^-2]
  · 严格 Fisher  sigma_F^{-2} = P^T C^{-1} P                     [ADU^-2]
  · 绝对信噪比   SNR = F_ref/sigma_F                              [1]
  · 控制点方差   cvar = k_corr * (pi/2) * sigma^2 / N_retained     [ADU^2]
      k_corr 定义（provenance.h:98）: k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]

本文件不产生结论，只提供数值工具；每个度量都由调用脚本带 seed 与分母。
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import platform
import sys
from typing import Dict, Iterable, Sequence, Tuple

import numpy as np

# ---------------------------------------------------------------- 环境指纹


def env_fingerprint() -> Dict[str, str]:
    return {
        "python": sys.version.split()[0],
        "numpy": np.__version__,
        "platform": platform.platform(),
        "machine": platform.machine(),
    }


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def repo_root() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    # 实验/absolute-snr/code/s05 -> 仓库根
    return os.path.abspath(os.path.join(here, "..", "..", "..", ".."))


def dump_json(path: str, obj: dict) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1, sort_keys=False)
        f.write("\n")


# ---------------------------------------------------------------- PSF


def moffat_stamp(fwhm_px: float, beta: float = 4.0, radius_px: float | None = None) -> np.ndarray:
    """归一 Moffat PSF（sum = 1），像素中心采样。

    Moffat 的 FWHM = 2*alpha*sqrt(2^(1/beta)-1)  => alpha = FWHM/(2*sqrt(2^(1/beta)-1))
    """
    if radius_px is None:
        radius_px = max(4.0, 3.0 * fwhm_px)
    n = int(math.ceil(radius_px))
    yy, xx = np.mgrid[-n : n + 1, -n : n + 1].astype(float)
    alpha = fwhm_px / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
    r2 = xx * xx + yy * yy
    p = (1.0 + r2 / (alpha * alpha)) ** (-beta)
    p /= p.sum()
    return p


def psf_on_cutout(p_stamp: np.ndarray, shape: Tuple[int, int], center: Tuple[float, float]) -> np.ndarray:
    """把归一 PSF 盖章放到 shape 的零画布上（双线性投放，保持总和 = 1）。"""
    ny, nx = shape
    out = np.zeros((ny, nx), dtype=float)
    sy, sx = p_stamp.shape
    cy, cx = center
    y0 = cy - (sy - 1) / 2.0
    x0 = cx - (sx - 1) / 2.0
    for j in range(sy):
        for i in range(sx):
            v = p_stamp[j, i]
            if v == 0.0:
                continue
            yf = y0 + j
            xf = x0 + i
            iy = int(math.floor(yf))
            ix = int(math.floor(xf))
            fy = yf - iy
            fx = xf - ix
            for dy, wy in ((0, 1.0 - fy), (1, fy)):
                for dx, wx in ((0, 1.0 - fx), (1, fx)):
                    w = wy * wx
                    if w == 0.0:
                        continue
                    yy = iy + dy
                    xx = ix + dx
                    if 0 <= yy < ny and 0 <= xx < nx:
                        out[yy, xx] += v * w
    out /= out.sum()
    return out


# ---------------------------------------------------------------- 噪声模型

DEFAULT_GAIN = 1.333   # e-/ADU，外部实测产物（见 REPORT_S05 §0 参数来源登记）
DEFAULT_RN = 8.59      # e-


def sigma_pix_doc(F_adu: float, P: np.ndarray, sky_adu: float,
                  gain: float = DEFAULT_GAIN, rn_e: float = DEFAULT_RN) -> np.ndarray:
    """正本 §3.3 逐像素方差（ADU^2）：sky/g + (RN/g)^2 + F*P/g。"""
    return sky_adu / gain + (rn_e / gain) ** 2 + F_adu * P / gain


def sigma_pix_doc_gain_inverted(F_adu: float, P: np.ndarray, sky_adu: float,
                                gain: float = DEFAULT_GAIN, rn_e: float = DEFAULT_RN) -> np.ndarray:
    """注入故障：把 g 的约定反过来（ADU/e-）⇒ 散粒项错一个 g^2 因子（负例臂）。"""
    g_inv = 1.0 / gain
    return sky_adu * gain + (rn_e * gain) ** 2 + F_adu * P * gain


def sigma_F_diag(P: np.ndarray, var_pix: np.ndarray) -> float:
    return float(1.0 / np.sqrt(np.sum(P * P / var_pix)))


def sigma_F_strict(P: np.ndarray, C: np.ndarray) -> float:
    """sigma_F = 1/sqrt(P^T C^-1 P)；C 必须正定（否则抛错，禁止把奇异当零噪声）。"""
    Cinv_p = np.linalg.solve(C, P)
    info = float(P @ Cinv_p)
    if not (info > 0.0) or not math.isfinite(info):
        raise ValueError(f"P^T C^-1 P = {info} 非正/非有限（C 非正定或数值奇异）")
    return 1.0 / math.sqrt(info)


def gls_amplitude(y: np.ndarray, A: np.ndarray, C: np.ndarray) -> Tuple[float, np.ndarray]:
    """GLS：返回 (参数向量, 参数协方差矩阵)；第 0 个参数是被估量幅度。"""
    Cinv = np.linalg.inv(C)
    AtCi = A.T @ Cinv
    cov = np.linalg.inv(AtCi @ A)
    beta = cov @ (AtCi @ y)
    return float(beta[0]), cov


def sigma_F_freebg(P: np.ndarray, var_pix: np.ndarray) -> float:
    """自由常数背景：J=[P,1]，sigma_F^2 = (A^T C^-1 A)^-1 的 [0,0]。"""
    C = np.diag(var_pix)
    A = np.column_stack([P, np.ones_like(P)])
    _, cov = gls_amplitude(np.zeros_like(P), A, C)
    return float(math.sqrt(cov[0, 0]))


# ---------------------------------------------------------------- 统计


def mad_sigma(x: np.ndarray) -> float:
    """正本估计器口径：MAD 尺度 1.4826*median(|x-median(x)|)。"""
    return float(1.482602218505602 * np.median(np.abs(x - np.median(x))))


def wilson_ci(k: int, n: int, z: float = 1.959963984540054) -> Tuple[float, float]:
    if n == 0:
        return (float("nan"), float("nan"))
    p = k / n
    d = 1.0 + z * z / n
    c = p + z * z / (2 * n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return ((c - h) / d, (c + h) / d)


def coverage_report(z: np.ndarray, levels: Sequence[float] = (0.6826894921370859,
                                                              0.90, 0.95)) -> dict:
    """z 为标准化的 (est-truth)/sigma；覆盖率 + 二项 se + Wilson CI。"""
    from math import erf, sqrt

    out = {"n": int(z.size), "z_rms": float(np.sqrt(np.mean(z ** 2))),
           "z_median": float(np.median(z)), "z_mad": mad_sigma(z)}
    cov = {}
    for lv in levels:
        kz = math.sqrt(2.0) * _erfinv(lv)
        k = int(np.sum(np.abs(z) <= kz))
        lo, hi = wilson_ci(k, z.size)
        cov[f"{lv:.10g}"] = {
            "nominal": float(lv),
            "k": k, "n": int(z.size),
            "cov": float(k / z.size),
            "binomial_se": float(math.sqrt(lv * (1 - lv) / z.size)),
            "wilson95": [lo, hi],
            "contains_nominal": bool(lo <= lv <= hi),
            "z_bound": float(kz),
        }
    out["coverage"] = cov
    return out


def _erfinv(x: float) -> float:
    # Winitzki 近似 + 一步牛顿（精度 ~1e-15，用于覆盖率 z 界）
    a = 0.147
    ln = math.log(1 - x * x)
    t = 2.0 / (math.pi * a) + ln / 2.0
    r = math.copysign(math.sqrt(math.sqrt(t * t - ln / a) - t), x)
    for _ in range(3):
        r = r - (math.erf(r) - x) / (2.0 / math.sqrt(math.pi) * math.exp(-r * r))
    return r


def neff_from_cov(C: np.ndarray, w: np.ndarray | None = None) -> float:
    """有效独立样本数：N_eff = (sum_i c_i)^2 / sum_ij c_i c_j rho_ij 的表达。

    对线性统计量 t = w^T x，Var(t) = w^T C w；等权白噪基准为 sum(w^2)*sigma^2。
    这里取 N_eff = (sum w_i)^2 sigma^2 / (w^T C w)（w 默认全 1）。
    """
    if w is None:
        w = np.ones(C.shape[0])
    sig2 = float(np.mean(np.diag(C)))
    num = float(np.sum(w)) ** 2 * sig2
    den = float(w @ C @ w)
    return num / den if den > 0 else float("inf")


def rng_for(tag: str, seed: int) -> np.random.Generator:
    """seed 与 tag 派生（可复现、与时间/环境无关）。"""
    h = hashlib.sha256(f"{seed}:{tag}".encode("utf-8")).digest()
    s = int.from_bytes(h[:8], "little")
    return np.random.default_rng(s)
