#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 公共库：**帧内 SNR 的物理建模与重建**。

本单元把「帧内 SNR/噪声场重建」从**空间插值问题**改写为**物理建模问题**：

    Var(p) = sigma0^2 + D(p)/g          （泊松 + 读出的教科书式，D = 平滑弥散分量 [ADU]）

* D(p) 由**结构分离**后的平滑弥散分量给出（星点 / 线状结构 / 宇宙线不得进入）；
* sigma0^2 与 1/g 由**控制点上的稳健方差估计**对该模型做稳健回归得到（帧级特性）；
* 稠密 sigma 场（进而 SNR 场）由模型**逐像素求值**，不是对稀疏控制点做几何插值。

口径（与 SCI-B 既有单元严格一致，禁止混用）
------------------------------------------
* sigma 一律是**逐像素**标准差 [ADU]；SNR 一律是**通量型**点源 SNR
  SNR = F_ref / (sigma * sqrt(A_NEA))（docs/plugins/algorithms_phase1/07_noise_snr.md 4.1；
  实验/absolute-snr/docs/frame-snr-canon.md 2.2）。
* 本单元全部判据都是**比值**，F_ref 与 sqrt(A_NEA) 在比值中相消 ⇒
  报告时取 F_ref = 1 ADU、A_NEA = 1 px 的规范口径，并显式给出换算 SNR = 1/sigma。
* **两个真值口径必须分开登记**（本单元的核心澄清之一）：
  - T1 = var_bg：**空背景口径**（天光 + 暗流 + 读出 + 量化），**不含源自身泊松**
    —— 这是 AstroCS 帧级 SNR 的冻结口径（NOISE_MODEL 9a、frame-snr-canon 1.2 第 4 条）；
  - T2 = var_local：**逐像素总方差口径**（再加源自身泊松 src_e/g^2）
    —— 这是「逐像素逆方差最优加权」在数学上需要的量。
  两者在源像素上差异巨大，本单元**同时报告**，不得只报一个。
* **caliber P**（逐像素显著性 I(p)/sigma(p)）在本单元只作为**错误臂**出现：
  它不是 AstroCS 的 SNR 口径（frame-snr-canon 1.3 RED-A/RED-B），
  但正是「逐像素代入亮度 ⇒ 星点异常高信噪比」这一失效模式所在的口径。

只读边界：只 import 既有单元的公共库（exp02/exp03/exp05、exp04/operators、shared/synthetic），
不修改、不复制它们的任何文件；不运行任何 AstroCS 可执行文件；无 git 写操作。
固定 seed：SEED = 20260927。
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
_CODE = HERE.parent
for _p in (_CODE / "exp02", _CODE / "exp03", _CODE / "exp04", _CODE / "exp05", _CODE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import exp02_common as C2       # noqa: E402  生产 recipe 逐字重写（只读）
import exp05_common as X5       # noqa: E402  口径/度量（只读）
import operators as OP          # noqa: E402  EXP-04 重建算子库（只读）

ROOT = Path(__file__).resolve().parents[4]
UNIT = ROOT / "实验" / "absolute-snr"
RESULTS = UNIT / "results"
TESTDATA = ROOT / "testdata"
SHARED = ROOT / "实验" / "shared" / "synthetic"

SEED = 20260927
DELTA_PX = 64                    # 稀疏控制点间隔（= hips.tile_width/8）
K_MAD = 1.482602218505602
QUANT_VAR = 1.0 / 12.0
VAR_FLOOR = 1e-12
LEVER_MIN = 0.15                 # 斜率可辨识阈值（lever_arm_diag.lever_var，见报告 3.2）

# 规范口径（比值判据不变）：F_ref = 1 ADU，A_NEA = 1 px  ⇒  SNR = 1/sigma
F_REF_CANON = 1.0
A_NEA_CANON = 1.0


def rng(offset: int = 0) -> np.random.Generator:
    return np.random.default_rng(SEED + int(offset))


def save_json(path, obj) -> str:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=1, default=float),
                 encoding="utf-8")
    return str(p)


# ===========================================================================
# 1. 口径换算
# ===========================================================================
def snr_of_sigma(sigma: np.ndarray) -> np.ndarray:
    """sigma 场 -> 通量型 SNR 场（规范口径 F_ref=1 ADU、A_NEA=1 px）。"""
    s = np.asarray(sigma, dtype=np.float64)
    return np.where(s > 0, F_REF_CANON / (s * math.sqrt(A_NEA_CANON)), np.nan)


def var_of_sigma(sigma: np.ndarray) -> np.ndarray:
    return np.asarray(sigma, dtype=np.float64) ** 2


def sigma_of_var(var: np.ndarray) -> np.ndarray:
    return np.sqrt(np.maximum(np.asarray(var, dtype=np.float64), 0.0))


# ===========================================================================
# 2. 解析合成场景（臂 A：真值完全已知）
# ===========================================================================
def moffat_stamp(kernel_half: int, alpha: float, beta: float) -> np.ndarray:
    """归一化 Moffat 核（sum = 1），与 shared/synthetic/render.py 同族但本单元自持。"""
    yy, xx = np.mgrid[-kernel_half:kernel_half + 1, -kernel_half:kernel_half + 1]
    r2 = (yy * yy + xx * xx).astype(np.float64)
    k = (1.0 + r2 / (alpha * alpha)) ** (-beta)
    return k / k.sum()


def _add_stamp(canvas: np.ndarray, kern: np.ndarray, y: float, x: float, amp: float) -> None:
    h = kern.shape[0] // 2
    ny, nx = canvas.shape
    iy, ix = int(round(y)), int(round(x))
    y0, y1 = max(0, iy - h), min(ny, iy + h + 1)
    x0, x1 = max(0, ix - h), min(nx, ix + h + 1)
    if y1 <= y0 or x1 <= x0:
        return
    ky0, kx0 = y0 - (iy - h), x0 - (ix - h)
    canvas[y0:y1, x0:x1] += amp * kern[ky0:ky0 + (y1 - y0), kx0:kx0 + (x1 - x0)]


def analytic_scene(
    shape: Tuple[int, int] = (1024, 1024),
    *,
    seed: int = 0,
    exposure_s: float = 300.0,
    gain: float = 1.3,
    read_noise_e: float = 10.0,
    bias_adu: float = 1000.0,
    dark_e_per_s: float = 0.02,
    sky_e_per_s: float = 0.5,
    sky_grad_e_per_s: float = 0.0,
    sky_theta_deg: float = 0.0,
    blobs: Sequence[Dict[str, Any]] = (),
    n_stars: int = 0,
    star_flux_log10: Tuple[float, float] = (3.0, 6.0),
    star_fwhm_px: float = 3.0,
    star_beta: float = 4.0,
    filament: Optional[Dict[str, Any]] = None,
    cr_rate_per_frame: float = 0.0,
    cr_mean_charge_e: float = 900.0,
    prnu_rms: float = 0.0,
    flat_low_order: float = 0.0,
    quantize: bool = True,
    noise: bool = True,
    rn_map: Optional[np.ndarray] = None,
    dark_map: Optional[np.ndarray] = None,
) -> Dict[str, Any]:
    # rn_map / dark_map：逐像素读出噪声 [e-] 与暗流 [e-/pix] 图（**亮度解释不了的噪声结构**），
    # 用于刻画「物理模型的作用域边界」（见 e5_scope）：平滑弥散分量恒定时，
    # 探测器噪声的空间变化只能由控制点插值（EXP-04 算子）承载。
    """构造**解析可算真值**的一帧。

    物理链（与 shared/synthetic/noise_model.py::expose 的 physical 模式同构）::

        lam_e(p) = t*(sky_rate(p) + src_rate(p))*m(p) + t*D
        adu(p)   = Poisson(lam_e)/g + N(0, RN)/g + bias      （可含量化取整）

    真值（**解析**，不需要仿真器自陈）::

        var_bg(p)    = (sky_e(p)*m + dark_e)/g^2 + RN^2/g^2 + (1/12 if quantize)
        var_local(p) = (sky_e(p)*m + src_e(p)*m + dark_e)/g^2 + RN^2/g^2 + (1/12 if quantize)

    其中 sky_e = t*sky_rate、src_e = t*src_rate、dark_e = t*D、m = 平场响应。

    返回 dict：adu、truth（含 var_bg / var_local / 各分量面）、meta。
    """
    r = rng(seed)
    ny, nx = int(shape[0]), int(shape[1])
    yy, xx = np.mgrid[0:ny, 0:nx]
    u = xx / max(nx - 1, 1) - 0.5
    v = yy / max(ny - 1, 1) - 0.5
    th = math.radians(float(sky_theta_deg))

    # --- 平滑天光面 [e-/s] ---
    sky_rate = np.full((ny, nx), float(sky_e_per_s), dtype=np.float64)
    if sky_grad_e_per_s:
        sky_rate = sky_rate + float(sky_grad_e_per_s) * (math.cos(th) * u + math.sin(th) * v)
    for b in blobs:
        cy, cx = float(b.get("cy", ny / 2)), float(b.get("cx", nx / 2))
        sig = float(b.get("sigma_px", 60.0))
        amp = float(b.get("amp_e_per_s", 1.0))
        sky_rate = sky_rate + amp * np.exp(-((yy - cy) ** 2 + (xx - cx) ** 2) / (2.0 * sig * sig))
    sky_rate = np.maximum(sky_rate, 0.0)

    # --- 源面 [e-]（曝光积分后）：点源 + 线状结构 ---
    src_e = np.zeros((ny, nx), dtype=np.float64)
    kern_half = int(math.ceil(6.0 * star_fwhm_px))
    alpha = float(star_fwhm_px) / (2.0 * math.sqrt(2.0 ** (1.0 / float(star_beta)) - 1.0))
    kern = moffat_stamp(kern_half, alpha, float(star_beta))
    star_cat: List[Dict[str, float]] = []
    for _ in range(int(n_stars)):
        sy = float(r.uniform(0, ny))
        sx = float(r.uniform(0, nx))
        fl = float(10.0 ** r.uniform(*star_flux_log10))
        _add_stamp(src_e, kern, sy, sx, fl)
        star_cat.append({"y": sy, "x": sx, "flux_e": fl})
    fil_meta: Dict[str, Any] = {}
    if filament:
        y1, x1 = float(filament.get("y1", 0.0)), float(filament.get("x1", 0.0))
        y2, x2 = float(filament.get("y2", ny - 1.0)), float(filament.get("x2", nx - 1.0))
        w = float(filament.get("width_px", 3.0))
        amp = float(filament.get("amp_e", 1e4))
        vy, vx = (y2 - y1), (x2 - x1)
        L2 = vy * vy + vx * vx
        tt = np.clip(((yy - y1) * vy + (xx - x1) * vx) / max(L2, 1e-9), 0.0, 1.0)
        d2 = (yy - (y1 + tt * vy)) ** 2 + (xx - (x1 + tt * vx)) ** 2
        src_e = src_e + amp * np.exp(-d2 / (2.0 * w * w))
        fil_meta = {"y1": y1, "x1": x1, "y2": y2, "x2": x2, "width_px": w, "amp_e": amp}

    # --- 平场（乘性、几何均值归一） ---
    m = np.ones((ny, nx), dtype=np.float64)
    if flat_low_order:
        m = m * (1.0 + float(flat_low_order) * (u + 0.5 * v))
    if prnu_rms:
        m = m * (1.0 + float(prnu_rms) * r.normal(0.0, 1.0, size=(ny, nx)))
    m = np.maximum(m, 1e-3)
    m = m / float(np.exp(np.mean(np.log(m))))

    t = float(exposure_s)
    g = float(gain)
    rn = float(read_noise_e)
    if rn_map is not None:
        rn_map = np.asarray(rn_map, dtype=np.float64)
        if rn_map.shape != (ny, nx):
            raise ValueError("rn_map shape mismatch")
    if dark_map is not None:
        dark_map = np.asarray(dark_map, dtype=np.float64)
        if dark_map.shape != (ny, nx):
            raise ValueError("dark_map shape mismatch")
    dark_e = np.full((ny, nx), t * float(dark_e_per_s), dtype=np.float64)
    if dark_map is not None:
        dark_e = dark_e + dark_map

    sky_e = sky_rate * t * m
    src_e_m = src_e * m
    lam_e = sky_e + src_e_m + dark_e

    cr_e = np.zeros((ny, nx), dtype=np.float64)
    if cr_rate_per_frame > 0:
        n_cr = int(r.poisson(float(cr_rate_per_frame)))
        if n_cr > 0:
            idx = r.choice(ny * nx, size=min(n_cr, ny * nx), replace=False)
            cr_e.flat[idx] = r.exponential(float(cr_mean_charge_e), size=idx.size)

    rn_eff = (np.full((ny, nx), rn, dtype=np.float64) if rn_map is None else rn_map)
    if noise:
        ne = r.poisson(lam_e).astype(np.float64) + cr_e + r.normal(0.0, 1.0, size=(ny, nx)) * rn_eff
        adu = ne / g + float(bias_adu)
        if quantize:
            adu = np.round(adu)
    else:
        adu = lam_e / g + float(bias_adu)

    q = QUANT_VAR if quantize else 0.0
    var_bg = (sky_e + dark_e) / (g * g) + (rn_eff * rn_eff) / (g * g) + q
    var_local = (sky_e + src_e_m + dark_e) / (g * g) + (rn_eff * rn_eff) / (g * g) + q

    return {
        "adu": adu,
        "truth": {
            "var_bg": var_bg, "var_local": var_local,
            "sigma_bg": sigma_of_var(var_bg), "sigma_local": sigma_of_var(var_local),
            "sky_e": sky_e, "src_e": src_e_m, "dark_e": dark_e,
            "diffuse_e": sky_e + dark_e, "flat": m, "cr_e": cr_e,
            "src_e_unflat": src_e,
        },
        "meta": {
            "seed": int(seed), "shape": [ny, nx], "exposure_s": t, "gain": g,
            "read_noise_e": rn, "bias_adu": float(bias_adu),
            "dark_e_per_s": float(dark_e_per_s), "sky_e_per_s": float(sky_e_per_s),
            "sky_grad_e_per_s": float(sky_grad_e_per_s), "sky_theta_deg": float(sky_theta_deg),
            "blobs": [dict(b) for b in blobs], "n_stars": int(n_stars),
            "star_flux_log10": list(star_flux_log10), "star_fwhm_px": float(star_fwhm_px),
            "star_beta": float(star_beta), "stars": star_cat, "filament": fil_meta,
            "cr_rate_per_frame": float(cr_rate_per_frame),
            "cr_mean_charge_e": float(cr_mean_charge_e),
            "prnu_rms": float(prnu_rms), "flat_low_order": float(flat_low_order),
            "quantize": bool(quantize), "noise": bool(noise),
            "sigma0_adu_true": float(math.sqrt((rn * rn) / (g * g) + q)),
            "gain_true": g,
            "model_exact": "var_bg = sigma0^2 + (sky_e+dark_e)/g^2 ; var_local 再加 src_e/g^2",
        },
    }


# ===========================================================================
# 3. 结构分离：平滑弥散分量 D(p)
# ===========================================================================
def mesh_bg_grid(img: np.ndarray, box: int = DELTA_PX, n_rounds: int = 2, k: float = 3.0
                 ) -> Tuple[np.ndarray, np.ndarray]:
    """逐 mesh 的**裁剪中位数背景**与保留比（生产 recipe，逐字复用 exp02_common）。"""
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    by, bx = ny // box, nx // box
    mm = (a[:by * box, :bx * box].reshape(by, box, bx, box)
          .transpose(0, 2, 1, 3).reshape(by * bx, box * box))
    bg = np.empty(by * bx)
    kf = np.empty(by * bx)
    for i in range(by * bx):
        st = C2.production_clip_sigma(mm[i], n_rounds=int(n_rounds), k=float(k))
        bg[i] = st["background"]
        kf[i] = st["keep_frac"]
    return bg.reshape(by, bx), kf.reshape(by, bx)


def diffuse_component(img: np.ndarray, box: int = DELTA_PX, mode: str = "spline",
                      n_iter: int = 3, min_keep: float = 0.5, filter_size: int = 3,
                      op_name: str = "spline_natural_clip", n_rounds: int = 2,
                      k: float = 3.0) -> Tuple[np.ndarray, Dict[str, Any]]:
    """**结构分离后的平滑弥散分量** D(p) [ADU]（物理模型的驱动量）。

    mode = "bilin_med3"：SExtractor 语义同构物（逐 mesh 裁剪中位数 + 3x3 mesh 中值滤波 +
        双线性展开），即 exp02_common.mesh_background_map；
    mode = "spline"（默认）：同上但**去掉 mesh 中值滤波**、并用 EXP-04 的推荐算子
        （自然边界双三次样条 + 值域钳制）把 mesh 网格展开到像素级。

    为什么默认去掉中值滤波：实测（本单元 3.2）在**平滑**弥散场上，3x3 mesh 中值滤波会
    注入幅度约 曲率*Delta^2 的**棋盘状**偏置（|D_hat - D_true| 中位数 4.9 ADU vs 1.0 ADU），
    它在每个 cell 内形成斜坡，把残差 RMS 抬高数个百分点的**系统性**误差，
    足以让物理模型的斜率（=1/g）估计偏出 40% 以上。
    中值滤波的正当用途是剔除**坏 mesh**，本实现改为显式 keep_frac 判据 + 最近有效填充。
    """
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    B = np.zeros((ny, nx), dtype=np.float64)
    n_bad_last = 0
    if mode == "bilin_med3":
        B, meta = C2.mesh_background_map(a, box=int(box), filter_size=int(filter_size),
                                         n_iter=int(n_iter))
        return B, dict(meta, mode=mode)
    if mode != "spline":
        raise ValueError("diffuse_component: unknown mode %r" % mode)
    for _ in range(max(int(n_iter), 1)):
        g, kf = mesh_bg_grid(a - B, box=int(box), n_rounds=int(n_rounds), k=float(k))
        bad = (~np.isfinite(g)) | (kf < float(min_keep))
        n_bad_last = int(bad.sum())
        if bad.any():
            filled, _ = OP.prefill_nan(np.where(bad, np.nan, g), "nearest_valid")
            g = np.where(np.isfinite(filled), filled, float(np.nanmedian(g)))
        dB, nbad = OP.run_operator(op_name, g, int(box), (ny, nx))[:2]
        B = B + np.where(np.isfinite(dB), dB, 0.0)
    return B, {"mode": mode, "op_name": op_name, "n_iter": int(n_iter),
               "n_bad_mesh_last": n_bad_last, "min_keep": float(min_keep),
               "box": int(box)}


def diffuse_unseparated(img: np.ndarray, sigma_px: float = 32.0) -> np.ndarray:
    """**未做结构分离**的驱动量（错误臂）：整帧大尺度高斯平滑，结构性亮源全部进入。"""
    from scipy.ndimage import gaussian_filter
    return gaussian_filter(np.asarray(img, dtype=np.float64), float(sigma_px), mode="nearest")


# ===========================================================================
# 4. 控制点上的 sigma 估计（沿用 EXP-03/EXP-05 的估计器族，口径不变）
# ===========================================================================
def sigma_ctrl_raw(img: np.ndarray, delta: int = DELTA_PX) -> np.ndarray:
    """R0：逐 cell 对**原始像素**套生产 recipe（不扣局部背景）—— 结构污染臂。"""
    return X5.sigma_patch_raw(img, delta)


def sigma_ctrl_resid(img: np.ndarray, delta: int = DELTA_PX, n_iter: int = 3) -> np.ndarray:
    """R1：mesh 局部背景扣除后逐 cell 残差裁剪 RMS（结构感知）。"""
    return X5.sigma_patch_resid(img, delta, n_iter=n_iter)


def sigma_ctrl_resid_fam(img: np.ndarray, fam: int, delta: int = DELTA_PX,
                         n_iter: int = 3) -> np.ndarray:
    """**单一阵列族**的结构感知逐 patch sigma（hold-out 用，零像素重叠）。"""
    return X5.sigma_patch_resid_fam(img, fam, delta, n_iter=n_iter)


def truth_ctrl(var_map: np.ndarray, delta: int = DELTA_PX) -> np.ndarray:
    """控制点真值 sigma = sqrt(cell 内逐像素方差均值)（与 EXP-03/EXP-05 同口径）。"""
    return X5.patch_truth_from_var(np.asarray(var_map, dtype=np.float64), delta)


def sample_at_ctrl(field: np.ndarray, delta: int = DELTA_PX) -> np.ndarray:
    """在控制点（cell 中心）采样一个逐像素场，返回 (ny_ctrl, nx_ctrl)。"""
    a = np.asarray(field, dtype=np.float64)
    ny, nx = a.shape
    by, bx = ny // delta, nx // delta
    iy = np.clip(((np.arange(by) + 0.5) * delta).astype(int), 0, ny - 1)
    ix = np.clip(((np.arange(bx) + 0.5) * delta).astype(int), 0, nx - 1)
    return a[np.ix_(iy, ix)]


# ===========================================================================
# 5. 物理模型：Var = a + c*(D - D_ref)（= sigma0^2 + D/g 的等价参数化）
# ===========================================================================
def _robust_scale(x: np.ndarray) -> float:
    x = np.asarray(x, dtype=np.float64)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return float("nan")
    return float(K_MAD * np.median(np.abs(x - np.median(x))))


def _solve_lin(x: np.ndarray, y: np.ndarray, fix_c: Optional[float]) -> Tuple[float, float]:
    """解 Var = a + c*x；fix_c 非空时固定 c、只估 a（一维均值，无病态性）。"""
    if fix_c is not None:
        return float(np.mean(y - float(fix_c) * x)), float(fix_c)
    A = np.column_stack([np.ones(x.size), x])
    coef, *_ = np.linalg.lstsq(A, y, rcond=None)
    return float(coef[0]), float(coef[1])


def lever_arm_diag(D_ctrl: np.ndarray, V_ref: float, valid: Optional[np.ndarray] = None,
                   g_assumed: float = 1.3) -> Dict[str, float]:
    """**条件数诊断**：斜率 c 的可辨识性由「背景动态范围 / 方差动态范围」决定。

    lever_var = (p95(D) - p05(D))/g_assumed / V_ref 是背景变化引起的方差**相对**变化。
    实测（本单元 3.2）：lever_var 小于约 0.15 时，斜率估计的相对误差被放大一个量级
    （delta_c/c ~ 2*eps/lever_var，eps = sigma_hat 的系统性相对误差），
    此时**不得**用自由斜率拟合，必须用帧级增益固定斜率。
    """
    D = np.asarray(D_ctrl, dtype=np.float64).ravel()
    ok = np.isfinite(D)
    if valid is not None:
        ok = ok & np.asarray(valid, dtype=bool).ravel()
    if int(ok.sum()) < 8 or not (V_ref > 0):
        return {"D_p05": float("nan"), "D_p95": float("nan"), "D_range_rel": float("nan"),
                "lever_var": float("nan"), "n": int(ok.sum())}
    Dv = D[ok]
    p05, p95 = float(np.percentile(Dv, 5)), float(np.percentile(Dv, 95))
    return {"D_p05": p05, "D_p95": p95,
            "D_range_rel": (p95 - p05) / max(abs(float(np.median(Dv))), 1e-12),
            "lever_var": (p95 - p05) / float(g_assumed) / float(V_ref),
            "n": int(ok.sum())}


def fit_nlf(D_ctrl: np.ndarray, s_ctrl: np.ndarray, valid: Optional[np.ndarray] = None,
            n_iter: int = 3, k: float = 3.0,
            fix_c: Optional[float] = None) -> Dict[str, Any]:
    """稳健拟合**噪声电平函数**（noise level function）Var = a + c*(D - D_ref)。

    被拟合量是**方差**（不是 sigma），因为物理关系在方差域是严格线性的。
    D_ref = median(D_ctrl) ⇒ a 是参考电平处的方差，c 是斜率（物理上 = 1/g）。

    稳健化：以 1.4826*MAD 为尺度做 k 倍迭代裁剪（<= n_iter 轮），
    剔除被未分辨源污染的控制点（它们使 sigma_hat 偏高 ⇒ 残差为正的离群点）。

    fix_c 非空时**固定斜率**、只估电平 a（帧级增益已知时的推荐用法）：
    此时模型的病态性消失（见 lever_arm_diag）。
    """
    D = np.asarray(D_ctrl, dtype=np.float64).ravel()
    s = np.asarray(s_ctrl, dtype=np.float64).ravel()
    ok = np.isfinite(D) & np.isfinite(s) & (s > 0)
    if valid is not None:
        ok = ok & np.asarray(valid, dtype=bool).ravel()
    if int(ok.sum()) < 8:
        return {"ok": False, "reason": "n_valid<8", "n_valid": int(ok.sum())}
    Dv, sv = D[ok], s[ok]
    D_ref = float(np.median(Dv))
    x = Dv - D_ref
    y = sv * sv
    keep = np.ones(x.size, dtype=bool)
    n_rounds = 0
    min_keep = max(6, int(0.25 * x.size))     # 防迭代裁剪塌缩（见报告 3.3）
    for _ in range(max(int(n_iter), 1)):
        if int(keep.sum()) < min_keep:
            break
        a_k, c_k = _solve_lin(x[keep], y[keep], fix_c)
        r = y - (a_k + c_k * x)
        sc = _robust_scale(r[keep])
        if not np.isfinite(sc) or sc <= 0:
            break
        new = np.abs(r) <= k * sc
        if int(new.sum()) < min_keep:
            break                              # 本轮裁剪会塌缩 ⇒ 保留上一轮
        n_rounds += 1
        if np.array_equal(new, keep):
            keep = new
            break
        keep = new
    if int(keep.sum()) < 6:
        return {"ok": False, "reason": "robust clip collapsed", "n_valid": int(ok.sum()),
                "n_used": int(keep.sum())}
    nk = int(keep.sum())
    a, c = _solve_lin(x[keep], y[keep], fix_c)
    resid = y[keep] - (a + c * x[keep])
    dof = max(nk - (1 if fix_c is not None else 2), 1)
    s2 = float(np.sum(resid ** 2) / dof)
    if fix_c is not None:
        a_se, c_se = float(math.sqrt(s2 / max(nk, 1))), 0.0
    else:
        A = np.column_stack([np.ones(nk), x[keep]])
        try:
            cov = s2 * np.linalg.inv(A.T @ A)
            a_se, c_se = float(math.sqrt(cov[0, 0])), float(math.sqrt(cov[1, 1]))
        except np.linalg.LinAlgError:            # pragma: no cover
            a_se = c_se = float("nan")
    ss_tot = float(np.sum((y[keep] - np.mean(y[keep])) ** 2))
    r2 = float(1.0 - np.sum(resid ** 2) / ss_tot) if ss_tot > 0 else float("nan")
    return {
        "ok": True, "a": a, "c": c, "a_se": a_se, "c_se": c_se, "D_ref": D_ref,
        "gain_hat": (1.0 / c) if c > 0 else float("inf"),
        "gain_hat_se": (c_se / (c * c)) if c > 0 else float("nan"),
        "r2": r2, "n_valid": int(ok.sum()), "n_used": int(keep.sum()),
        "n_clipped": int(ok.sum() - keep.sum()), "n_rounds": n_rounds,
        "resid_scale_var": _robust_scale(resid), "sigma_at_ref": float(math.sqrt(max(a, 0.0))),
    }


def slope_by_cv(D_ctrl: np.ndarray, s_ctrl: np.ndarray, valid: Optional[np.ndarray] = None,
                n_iter: int = 3, k: float = 3.0) -> Dict[str, Any]:
    """**用控制点做 2 折交叉验证选斜率**（数据驱动，不依赖任何阈值）。

    候选只有两个：``c = 0``（噪声本底与亮度无关）与 ``c = 自由拟合``。
    判据 = 留出折上**方差域**的预测误差（RMS of log10 预测/实测，两侧中位数归一）。
    折的划分 = 控制网格的棋盘（(i+j) mod 2），两折的亮度动态范围相同 ⇒ 比较公平。

    这条规则替代「按 lever_var 阈值切换」：实测 B3_hst_lowsky（lever_var=0.165）上
    自由斜率把 E 从 1.3e-6 抬到 1.5e-3，而 A5_unresolved（lever_var=0.285）上自由斜率
    把 E 从 2.0e-3 压到 1.5e-4 —— 单靠 lever_var 无法分开这两种情形。
    """
    D = np.asarray(D_ctrl, dtype=np.float64)
    s = np.asarray(s_ctrl, dtype=np.float64)
    ok = np.isfinite(D) & np.isfinite(s) & (s > 0)
    if valid is not None:
        ok = ok & np.asarray(valid, dtype=bool)
    if int(ok.sum()) < 16:
        return {"ok": False, "reason": "n_valid<16", "n_valid": int(ok.sum())}
    ny, nx = D.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    fold = ((yy + xx) % 2 == 0)
    err_free, err_zero, cs = [], [], []
    for f in (True, False):
        tr = ok & (fold == f)
        te = ok & (fold != f)
        if int(tr.sum()) < 8 or int(te.sum()) < 8:
            return {"ok": False, "reason": "fold too small"}
        fitf = fit_nlf(D[tr], s[tr], valid=None, n_iter=n_iter, k=k)
        if not fitf.get("ok") or not np.isfinite(fitf.get("c", float("nan"))):
            return {"ok": False, "reason": "free fit failed on fold"}
        fit0 = fit_nlf(D[tr], s[tr], valid=None, n_iter=n_iter, k=k, fix_c=0.0)
        for fit, acc in ((fitf, err_free), (fit0, err_zero)):
            pred = fit["a"] + fit["c"] * (D[te] - fit["D_ref"])
            m = np.isfinite(pred) & (pred > 0)
            if int(m.sum()) < 8:
                return {"ok": False, "reason": "empty test prediction"}
            r = np.log10(pred[m] / (s[te][m] ** 2))
            acc.append(float(np.sqrt(np.mean((r - np.median(r)) ** 2))))
        cs.append(float(fitf["c"]))
    ef, ez = float(np.mean(err_free)), float(np.mean(err_zero))
    choose_free = bool(ef < ez)
    return {"ok": True, "choose_free": choose_free, "c_free_mean": float(np.mean(cs)),
            "cv_err_free": ef, "cv_err_zero": ez, "n_valid": int(ok.sum()),
            "rule": "c=free" if choose_free else "c=0（CV 判定亮度项无增益）"}


def fit_nlf_power(D_ctrl: np.ndarray, s_ctrl: np.ndarray, valid: Optional[np.ndarray] = None,
                  n_iter: int = 2, k: float = 3.0) -> Dict[str, Any]:
    """自由指数版本 Var = a + c*sign(x)*|x|^p（x = D - D_ref）——**证伪 p=1 的检验**。

    物理模型预言 p = 1（泊松方差正比于电平）。若数据给出的 p 显著偏离 1，
    则物理形式不成立，本单元必须判红（而不是继续用线性模型）。
    """
    from scipy.optimize import curve_fit
    D = np.asarray(D_ctrl, dtype=np.float64).ravel()
    s = np.asarray(s_ctrl, dtype=np.float64).ravel()
    ok = np.isfinite(D) & np.isfinite(s) & (s > 0)
    if valid is not None:
        ok = ok & np.asarray(valid, dtype=bool).ravel()
    if int(ok.sum()) < 12:
        return {"ok": False, "reason": "n_valid<12", "n_valid": int(ok.sum())}
    Dv, sv = D[ok], s[ok]
    D_ref = float(np.median(Dv))
    x = Dv - D_ref
    y = sv * sv
    lin = fit_nlf(Dv, sv, valid=None, n_iter=n_iter, k=k)
    a0 = float(lin.get("a", np.median(y)))
    c0 = float(lin.get("c", 1e-3))

    def model(xx, a, c, p):
        return a + c * np.sign(xx) * np.abs(xx) ** p

    try:
        popt, pcov = curve_fit(model, x, y, p0=[a0, max(c0, 1e-9), 1.0],
                               sigma=np.full(x.size, max(_robust_scale(y), 1e-12)),
                               bounds=([-np.inf, 0.0, 0.2], [np.inf, np.inf, 3.0]),
                               maxfev=20000)
        perr = np.sqrt(np.diag(pcov))
    except Exception as exc:                                  # pragma: no cover
        return {"ok": False, "reason": "curve_fit failed: %s" % exc}
    p_hat, p_se = float(popt[2]), float(perr[2])
    # **判据防退化**：p_se 精确为 0（或非有限）时 |p-1| <= 3*p_se 会退化成 |p-1| <= 0 的恒真门
    # （实测 A0 有两个 seed 的 p_se = 0.0000，而 curve_fit 恰好返回 p = 1.0）。
    # 此时不产出布尔结论，改登记为 degenerate，由门侧要求「非退化样本数」而不是让它自动 PASS。
    se_ok = bool(np.isfinite(p_se) and p_se > 1e-9)
    at_bound = bool(p_hat <= 0.2 + 1e-9 or p_hat >= 3.0 - 1e-9)
    return {"ok": True, "a": float(popt[0]), "c": float(popt[1]), "p": p_hat,
            "p_se": p_se, "c_se": float(perr[1]), "D_ref": D_ref,
            "p_se_degenerate": (not se_ok), "p_at_bound": at_bound,
            "p_consistent_with_1": (bool(abs(p_hat - 1.0) <= 3.0 * p_se) if se_ok else None)}


# ===========================================================================
# 6. 重建方法（控制点 -> 稠密 sigma 场）
# ===========================================================================
def recon_physics(fit: Dict[str, Any], D_dense: np.ndarray) -> np.ndarray:
    """**物理模型重建**：sigma(p) = sqrt(max(a + c*(D(p) - D_ref), floor))。"""
    D = np.asarray(D_dense, dtype=np.float64)
    v = fit["a"] + fit["c"] * (D - fit["D_ref"])
    return np.sqrt(np.maximum(v, VAR_FLOOR))


def recon_physics_resid(fit: Dict[str, Any], D_dense: np.ndarray,
                        D_ctrl: np.ndarray, s_ctrl: np.ndarray,
                        op_name: str = "spline_natural_clip", delta: int = DELTA_PX
                        ) -> Tuple[np.ndarray, int]:
    """**物理 + 残差插值混合**：模型给电平，插值给模型解释不了的残余空间结构。

    resid_ctrl = s_ctrl^2 - (a + c*(D_ctrl - D_ref))，用 EXP-04 的推荐算子
    （默认自然边界双三次样条 + 值域钳制）把残差插值到像素级，再叠加到模型上。
    """
    base_ctrl = fit["a"] + fit["c"] * (np.asarray(D_ctrl, dtype=np.float64) - fit["D_ref"])
    resid = np.asarray(s_ctrl, dtype=np.float64) ** 2 - base_ctrl
    shape = np.asarray(D_dense).shape
    if not np.isfinite(resid).any():
        return np.full(shape, np.nan), int(resid.size)
    R, nbad = OP.run_operator(op_name, resid, delta, shape)[:2]
    v = fit["a"] + fit["c"] * (np.asarray(D_dense, dtype=np.float64) - fit["D_ref"]) + R
    return np.sqrt(np.maximum(v, VAR_FLOOR)), int(nbad)


def recon_interp(s_ctrl: np.ndarray, delta: int, shape: Tuple[int, int],
                 op_name: str = "spline_natural_clip") -> Tuple[np.ndarray, int]:
    """**纯插值**（EXP-04 框架）：直接把控制点 sigma 值插值为稠密场。"""
    c = np.asarray(s_ctrl, dtype=np.float64)
    if not np.isfinite(c).any():
        return np.full(shape, np.nan), int(c.size)
    out, nbad = OP.run_operator(op_name, c, delta, shape)[:2]
    return np.asarray(out, dtype=np.float64), int(nbad)


def recon_frame_scalar(s_ctrl: np.ndarray, shape: Tuple[int, int]) -> np.ndarray:
    """帧级标量口径（EXP-03/EXP-04 的兜底臂）：控制值中位数铺满全帧。"""
    s = np.asarray(s_ctrl, dtype=np.float64)
    s = s[np.isfinite(s) & (s > 0)]
    med = float(np.median(s)) if s.size else float("nan")
    return np.full(shape, med, dtype=np.float64)


def recon_dense_patch(s_ctrl: np.ndarray, delta: int, shape: Tuple[int, int]) -> np.ndarray:
    """稠密 patch 口径（块常数展开，不做任何重建）。"""
    return X5.upsample_nearest(np.asarray(s_ctrl, dtype=np.float64), delta, shape)


# ===========================================================================
# 6b. 统一方法装配：一帧 -> 全部候选重建场（臂 A/B 共用）
# ===========================================================================
def build_fields(img: np.ndarray, *, gain: Optional[float] = None, delta: int = DELTA_PX,
                 edge_margin: int = 1, smooth_sigma_px: float = 32.0,
                 fit_n_iter: int = 3) -> Dict[str, Any]:
    """把一帧图像变成**全部候选方法的稠密 sigma 场**（口径统一，禁止逐方法调参）。

    gain 非空 ⇒ 斜率固定为 1/gain（帧级特性，推荐）；为空 ⇒ 用自由斜率拟合
    （并给出 lever_arm_diag 诊断，报告斜率是否可辨识）。
    """
    a = np.asarray(img, dtype=np.float64)
    shape = a.shape
    B_sep, dmeta = diffuse_component(a, box=delta, mode="spline")
    B_med, _ = diffuse_component(a, box=delta, mode="bilin_med3")
    B_unsep = diffuse_unseparated(a, sigma_px=smooth_sigma_px)
    s_r1 = sigma_ctrl_resid(a, delta)
    s_r0 = sigma_ctrl_raw(a, delta)
    Dc_sep = sample_at_ctrl(B_sep, delta)
    Dc_unsep = sample_at_ctrl(B_unsep, delta)
    valid = np.ones(Dc_sep.shape, dtype=bool)
    m = int(edge_margin)
    if m > 0:
        valid[:m, :] = False
        valid[-m:, :] = False
        valid[:, :m] = False
        valid[:, -m:] = False

    fix_c = (1.0 / float(gain)) if (gain is not None and float(gain) > 0) else None
    fits: Dict[str, Any] = {}
    fits["free"] = fit_nlf(Dc_sep, s_r1, valid=valid, n_iter=fit_n_iter)
    fits["free_r0"] = fit_nlf(Dc_sep, s_r0, valid=valid, n_iter=fit_n_iter)
    fits["unsep"] = fit_nlf(Dc_unsep, s_r1, valid=valid, n_iter=fit_n_iter, fix_c=fix_c)
    fits["fix"] = (fit_nlf(Dc_sep, s_r1, valid=valid, n_iter=fit_n_iter, fix_c=fix_c)
                   if fix_c is not None else {"ok": False, "reason": "gain unknown"})
    V_ref = fits["fix"]["a"] if fits["fix"].get("ok") else (
        fits["free"]["a"] if fits["free"].get("ok") else float("nan"))
    diag = lever_arm_diag(Dc_sep, V_ref, valid=valid,
                          g_assumed=float(gain) if gain else 1.3)
    diag["power"] = fit_nlf_power(Dc_sep, s_r1, valid=valid)
    diag["n_valid"] = int(valid.sum())

    fields: Dict[str, np.ndarray] = {}
    fields["frame_scalar"] = recon_frame_scalar(s_r1, shape)
    fields["dense_patch"] = recon_dense_patch(s_r1, delta, shape)
    fields["interp_spline"] = recon_interp(s_r1, delta, shape, "spline_natural_clip")[0]
    fields["interp_bilinear"] = recon_interp(s_r1, delta, shape, "bilinear")[0]
    if fits["fix"].get("ok"):
        f = fits["fix"]
        fields["phys"] = recon_physics(f, B_sep)
        fields["phys_resid"] = recon_physics_resid(f, B_sep, Dc_sep, s_r1,
                                                   "spline_natural_clip", delta)[0]
        fields["phys_unsep"] = recon_physics(fits["unsep"], B_unsep)
        fields["phys_med3drv"] = recon_physics(f, B_med)
        fields["naive_pixel"] = np.sqrt(np.maximum(
            f["a"] + f["c"] * (a - f["D_ref"]), VAR_FLOOR))
    if fits["free"].get("ok"):
        f = fits["free"]
        fields["phys_free"] = recon_physics(f, B_sep)
        fields["phys_free_resid"] = recon_physics_resid(f, B_sep, Dc_sep, s_r1,
                                                        "spline_natural_clip", delta)[0]
    # **推荐默认**：斜率由**控制点上的 2 折交叉验证**在 {c = 0, c = 自由拟合} 中二选一
    # （slope_by_cv，折的划分 = 控制网格棋盘，判据 = 留出折方差域预测误差）。
    # 为什么不用 lever_var 阈值：实测 lever_var = 0.165 的场景应退化为 0、lever_var = 0.285
    # 的场景应保留自由斜率，单一阈值无法同时成立（报告 3.4）。
    cv = slope_by_cv(Dc_sep, s_r1, valid=valid, n_iter=fit_n_iter)
    diag["slope_cv"] = cv
    auto = None
    if cv.get("ok") and fits["free"].get("ok"):
        auto = dict(fits["free"])
        if not cv["choose_free"]:
            auto["c"] = 0.0
            auto["c_se"] = 0.0
        auto["slope_source"] = cv["rule"]
        auto["cv_err_free"] = cv["cv_err_free"]
        auto["cv_err_zero"] = cv["cv_err_zero"]
    if auto is not None:
        fields["phys_auto"] = recon_physics(auto, B_sep)
        fields["phys_auto_resid"] = recon_physics_resid(auto, B_sep, Dc_sep, s_r1,
                                                        "spline_natural_clip", delta)[0]
        fits["auto"] = auto
    if fits["free_r0"].get("ok"):
        fields["phys_r0ctrl"] = recon_physics(fits["free_r0"], B_sep)
    return {"fields": fields, "fits": fits, "diag": diag, "diffuse_meta": dmeta,
            "ctrl": {"s_r1": s_r1, "s_r0": s_r0, "D_sep": Dc_sep, "D_unsep": Dc_unsep,
                     "valid": valid, "B_sep": B_sep, "B_unsep": B_unsep}}


def evaluate_fields(fields: Dict[str, np.ndarray], truth: Dict[str, Any],
                    *, border: int = 0, sat_adu: Optional[float] = None,
                    adu: Optional[np.ndarray] = None, frac_of_floor: float = 0.05,
                    src_mask: Optional[np.ndarray] = None) -> Dict[str, Any]:
    """对全部候选场跑同一套判据；同时给 var_bg（T1）与 var_local（T2）两个真值口径。"""
    shape = truth["var_bg"].shape
    emask = eval_mask_of(shape, sat_adu=sat_adu, adu=adu, border=border)
    if src_mask is None:
        src_mask = source_mask(truth, frac_of_floor=frac_of_floor)
    out: Dict[str, Any] = {}
    for k, f in fields.items():
        out[k] = {
            "vs_T1_var_bg": metrics(f, truth["sigma_bg"], emask, src_mask=src_mask),
            "vs_T2_var_local": metrics(f, truth["sigma_local"], emask),
            "dispersion": spatial_dispersion(f, emask),
        }
    out["_common"] = {"src_mask_frac": float(src_mask.mean()),
                      "sigma_bg_median": float(np.median(truth["sigma_bg"][emask])),
                      "sigma_local_over_bg_median": float(
                          np.median(truth["sigma_local"][emask] / truth["sigma_bg"][emask])),
                      "sigma_local_over_bg_p999": float(
                          np.percentile(truth["sigma_local"][emask] / truth["sigma_bg"][emask], 99.9)),
                      "sigma_local_over_bg_max": float(
                          np.max(truth["sigma_local"][emask] / truth["sigma_bg"][emask]))}
    return out


# ===========================================================================
# 7. 度量（全部非退化；真值无效应时必须归零或判红）
# ===========================================================================
def eval_mask_of(shape, *, sat_adu: Optional[float] = None,
                 adu: Optional[np.ndarray] = None, border: int = 0) -> np.ndarray:
    """评价掩膜：有限、未饱和、非边界。"""
    ny, nx = int(shape[0]), int(shape[1])
    m = np.ones((ny, nx), dtype=bool)
    if border > 0:
        m[:border, :] = False
        m[-border:, :] = False
        m[:, :border] = False
        m[:, -border:] = False
    if sat_adu is not None and adu is not None:
        m = m & (np.asarray(adu, dtype=np.float64) < float(sat_adu))
    return m


def source_mask(truth: Dict[str, Any], *, frac_of_floor: float = 0.05) -> np.ndarray:
    """**结构性亮源掩膜**（只用于评价，不进入方法）：源贡献超过本底一定比例的像素。"""
    src = np.asarray(truth["src_e"], dtype=np.float64)
    dif = np.asarray(truth["diffuse_e"], dtype=np.float64)
    thr = np.maximum(frac_of_floor * dif, 1.0)
    return src > thr


def metrics(est_sigma: np.ndarray, true_sigma: np.ndarray, mask: np.ndarray,
            *, src_mask: Optional[np.ndarray] = None) -> Dict[str, Any]:
    """全套判据（比值口径；F_ref/A_NEA 相消）。

    * level_ratio   median(sigma_hat/sigma_true)（sigma 空间电平比）；snr_dev = 1/level_ratio - 1
    * p95_abs_dev   逐点比值的 p95 绝对偏差（形状误差）
    * rmse_log_rho  两侧中位数归一后的 RMSE(log10) [dex]（EXP-04 A1）
    * eff_loss      权重效率损失 E = Var_w/Var_opt - 1（EXP-04 A3，非退化主判据）
    * r2_var        方差域决定系数 1 - sum((v_hat-v)^2)/sum((v-mean)^2)
    * 源区指标（**负责人点名的失效模式**）：
      - src_level_ratio  源像素上的 median(sigma_hat/sigma_true)
      - src_snr_p05      源像素上 SNR 比值的 p05（最深的"伪暗洞"）
      - src_snr_max      源像素上 SNR 比值的最大值
      - snr_max_all      全帧 SNR 比值最大值（"异常高信噪比"的检出量）
    """
    e = np.asarray(est_sigma, dtype=np.float64)
    t = np.asarray(true_sigma, dtype=np.float64)
    m = (np.asarray(mask, dtype=bool) & np.isfinite(e) & np.isfinite(t) & (e > 0) & (t > 0))
    n = int(m.sum())
    out: Dict[str, Any] = {"n_eval": n}
    if n < 64:
        out.update({"degenerate": True, "reason": "n_eval<64",
                    "level_ratio": float("nan"), "snr_dev": float("nan"),
                    "p95_abs_dev": float("nan"), "rmse_log_rho": float("nan"),
                    "eff_loss": float("nan"), "r2_var": float("nan")})
        return out
    ratio = e[m] / t[m]
    med = float(np.median(ratio))
    a = e[m] / np.median(e[m])
    b = t[m] / np.median(t[m])
    d = np.log10(a) - np.log10(b)
    ve, vt = e[m] ** 2, t[m] ** 2
    ss_res = float(np.sum((ve - vt) ** 2))
    ss_tot = float(np.sum((vt - np.mean(vt)) ** 2))
    out.update({
        "degenerate": False,
        "level_ratio": med,
        "snr_dev": (1.0 / med - 1.0) if med > 0 else float("nan"),
        "p95_abs_dev": float(np.percentile(np.abs(ratio - 1.0), 95)),
        "rmse_log_rho": float(np.sqrt(np.mean(d * d))),
        "eff_loss": X5.weight_efficiency(1.0 / (e[m] ** 2), t[m] ** 2),
        "r2_var": (1.0 - ss_res / ss_tot) if ss_tot > 0 else float("nan"),
    })
    if src_mask is not None:
        sm = np.asarray(src_mask, dtype=bool) & m
        if int(sm.sum()) >= 16:
            rs = e[sm] / t[sm]
            out.update({
                "n_src": int(sm.sum()),
                "src_level_ratio": float(np.median(rs)),
                "src_snr_p05": float(np.percentile(1.0 / rs, 5)),
                "src_snr_max": float(np.max(1.0 / rs)),
            })
        else:
            out.update({"n_src": int(sm.sum()), "src_level_ratio": float("nan"),
                        "src_snr_p05": float("nan"), "src_snr_max": float("nan")})
    out["snr_max_all"] = float(np.max(1.0 / ratio))
    return out


def degenerate_zero_truth(true_var: np.ndarray, mask: np.ndarray, tol: float = 0.0) -> bool:
    """**真值无效应**判定：真值方差在评价掩膜上恒为 0（或 <= tol）。"""
    v = np.asarray(true_var, dtype=np.float64)[np.asarray(mask, dtype=bool)]
    v = v[np.isfinite(v)]
    if v.size == 0:
        return True
    return bool(np.max(v) <= tol)


def spatial_dispersion(field: np.ndarray, mask: np.ndarray) -> float:
    """场的**空间离散度**（真值无效应 ⇒ 必须归零）：max|f/median(f) - 1|。"""
    f = np.asarray(field, dtype=np.float64)[np.asarray(mask, dtype=bool)]
    f = f[np.isfinite(f) & (f > 0)]
    if f.size < 16:
        return float("nan")
    med = float(np.median(f))
    if med <= 0:
        return float("nan")
    return float(np.max(np.abs(f / med - 1.0)))


def caliber_p_overshoot(adu: np.ndarray, est_sigma: np.ndarray, true_frame_snr: float,
                        mask: np.ndarray) -> Dict[str, float]:
    """**caliber P（逐像素显著性 I/sigma）**的异常高信噪比量化。

    SNR_P(p) = I(p)/sigma_hat(p)；与「帧级通量型 SNR 真值」比较：
      * max_ratio_to_frame_snr = max_p SNR_P / SNR_frame_true（星点处应当爆表）；
      * p999_over_p50 = SNR_P 场的 p99.9/p50（空间离散度）。
    """
    I = np.asarray(adu, dtype=np.float64)
    s = np.asarray(est_sigma, dtype=np.float64)
    m = np.asarray(mask, dtype=bool) & np.isfinite(I) & np.isfinite(s) & (s > 0)
    if int(m.sum()) < 64:
        return {"max_ratio_to_frame_snr": float("nan"), "p999_over_p50": float("nan")}
    snr_p = I[m] / s[m]
    p50 = float(np.percentile(snr_p, 50))
    return {
        "max_ratio_to_frame_snr": (float(np.max(snr_p)) / float(true_frame_snr)
                                   if true_frame_snr > 0 else float("nan")),
        "p999_over_p50": (float(np.percentile(snr_p, 99.9)) / p50) if p50 > 0 else float("nan"),
    }


# ===========================================================================
# 8. 门（能红能绿）
# ===========================================================================
def gate(gates: List[Dict[str, Any]], name: str, passed: bool, detail: str,
         value: Any = None, expect: str = "") -> None:
    gates.append({"gate": name, "verdict": "PASS" if passed else "FAIL",
                  "expect": expect, "detail": detail, "value": value})


def all_pass(gates: Sequence[Dict[str, Any]]) -> bool:
    return all(g["verdict"] == "PASS" for g in gates)
