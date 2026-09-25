#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 公共库：「稀疏层存绝对 SNR」是否正确 的表示层推导与实验装置。

任务单元 SNR-ABS-DERIVE-01。负责人指示（原话）：
  「我只知道稀疏 SNR 是对的。而这个对不对我不知道。需要推导。」

本模块只提供**表示层**的东西（不改任何生产代码、不改任何已落地 schema）：
  1. 两个表示的定义式（相对 / 绝对）与其**精确**代数关系；
  2. 帧内共模因子的**可证条件**（重建算子的尺度等变性）与检验；
  3. 电平误差 / 权重效率 / 跨帧组合误差的度量（全部非退化）；
  4. 生产 sigma 链的只读镜像（整帧与逐 patch 的同一 recipe）。

单位与约定
----------
* sigma 一律是**逐像素**标准差 [ADU]；SNR 一律是**通量型**点源 SNR
  SNR = F_ref / sigma_F，sigma_F = sigma * sqrt(A_NEA)（生产 gain-free 天空受限分支，
  见 docs/plugins/algorithms_phase1/07_noise_snr.md 4.1 与
  实验/absolute-snr/docs/frame-snr-canon.md 2.2）。
* 本单元的全部判据都是**比值**，F_ref 与 sqrt(A_NEA) 在比值中相消 ⇒ 报告时取
  F_ref = 1 ADU、A_NEA = 1 px 的**规范口径**，并显式给出与 sigma 的换算：
  SNR_X/SNR_true = sigma_true/sigma_X（严格）。
* 固定 seed：SEED = 20260926。

只读边界：只 import exp02_common（生产 recipe 独立重写）与 exp03_common（区域估计器），
不修改、不复制这两个单元的任何文件；不运行任何 ACSD 可执行文件；无 git 写操作。
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
_EXP02 = _CODE / "exp02"
_EXP03 = _CODE / "exp03"
for _p in (str(_EXP02), str(_EXP03)):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import exp02_common as C  # noqa: E402  （生产 recipe 逐字重写）
import exp03_common as E3  # noqa: E402  （区域估计器族）

ROOT = Path(__file__).resolve().parents[4]
UNIT = ROOT / "实验" / "absolute-snr"
RESULTS = UNIT / "results"
TESTDATA = ROOT / "testdata"

SEED = 20260926
DELTA_PX = 64          # 稀疏控制点间隔（= hips.tile_width/8，07_noise_snr.md 4.5）
K_MAD = 1.482602218505602

# 生产 recipe（与 star_detector.cpp::estimate_background 同参数）
prod_sigma = C.production_clip_sigma


# ===========================================================================
# 0. 保存
# ===========================================================================
def save_json(path, obj) -> str:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=1, default=float),
                 encoding="utf-8")
    return str(p)


# ===========================================================================
# 1. 生产 sigma 链的只读镜像
# ===========================================================================
def sigma_frame(img: np.ndarray) -> float:
    """**帧级 sigma（生产口径）**：整帧 2 轮 median +- 3*1.4826*MAD 裁剪后的 RMS。

    代码锚（按符号名，不写死行号）：StarDetector::estimate_background
    （lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp）-> StarCatalog::noise_sigma
    -> module_adapters 写 f_frame[fi] 的 "noise_sigma"
    -> cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0) -> snr_frame_science
    -> snr_source_snr_f64 的 gain<=0 分支 var_f = sigma_sky^2/sum_p2。
    本函数直接调用 exp02_common.production_clip_sigma（逐字重写，未改）。
    """
    return float(prod_sigma(img)["sigma"])


def _mesh_view(a: np.ndarray, box: int) -> np.ndarray:
    return E3._mesh_view(a, box)


def sigma_patch_raw(img: np.ndarray, delta: int = DELTA_PX) -> np.ndarray:
    """**逐 patch 朴素 sigma（R0）**：对 patch 原始像素套**同一个**生产 recipe。

    这是「只把作用域从整帧换成 patch、别的不动」的对照臂 —— EXP-03 已证它不足以
    去掉结构污染（结构在 patch 内仍存活）。本单元把它作为**局部估计器的一个臂**，
    与 R1（结构感知）并列，以把「表示」与「估计器」两个问题解耦。
    """
    a = np.asarray(img, dtype=np.float64)
    mm = _mesh_view(a, delta)
    out = np.empty(mm.shape[0], dtype=np.float64)
    for i in range(mm.shape[0]):
        out[i] = float(prod_sigma(mm[i])["sigma"])
    ny, nx = a.shape
    return out.reshape(ny // delta, nx // delta)


def sigma_patch_resid(img: np.ndarray, delta: int = DELTA_PX,
                      n_iter: int = 3) -> np.ndarray:
    """**结构感知的逐 patch sigma（R1）**：mesh 局部背景扣除后逐 patch 残差裁剪 RMS。

    只读复用 exp03_common.region_sigma_resid（EXP-03 3.2 的 R1）。
    """
    r = E3.region_sigma_resid(img, delta, n_iter=n_iter)
    return np.asarray(r["sigma"], dtype=np.float64)


def sigma_patch_resid_fam(img: np.ndarray, fam: int, delta: int = DELTA_PX,
                          n_iter: int = 3, filter_size: int = 3,
                          min_pix: int = 64) -> np.ndarray:
    """**家族感知的结构感知 patch sigma（R1-fam）**：只用 fam 家族像素估计。

    与 sigma_patch_resid（EXP-03 的 R1）同构（迭代 mesh 背景 + 逐 patch 残差裁剪 RMS），
    但**背景与残差都只吃 fam 家族像素** ⇒ 与另一家族的 hold-out 真值**零像素重叠**。
    这是真实数据臂上把「表示」与「估计器」解耦所必需的。
    """
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    m = ((yy + xx) % 2 == fam) & np.isfinite(a)
    by, bx = ny // delta, nx // delta
    B = np.zeros((ny, nx), dtype=np.float64)
    for _ in range(max(int(n_iter), 1)):
        r = np.where(m, a - B, np.nan)
        sub = r[:by * delta, :bx * delta]
        mm = (sub.reshape(by, delta, bx, delta).transpose(0, 2, 1, 3)
              .reshape(by * bx, delta * delta))
        loc = np.full(by * bx, np.nan)
        for i in range(by * bx):
            v = mm[i][np.isfinite(mm[i])]
            if v.size >= min_pix:
                loc[i] = float(prod_sigma(v)["background"])
        g = loc.reshape(by, bx)
        if not np.isfinite(g).any():
            break
        g = np.where(np.isfinite(g), g, float(np.nanmedian(g)))
        g = C._median_filter_nan(g, filter_size)
        g = np.where(np.isfinite(g), g, float(np.nanmedian(g)))
        dB = C._expand_mesh_map(g, (ny, nx))
        B = B + np.where(np.isfinite(dB), dB, 0.0)
    resid = np.where(m, a - B, np.nan)
    sub = resid[:by * delta, :bx * delta]
    mm = (sub.reshape(by, delta, bx, delta).transpose(0, 2, 1, 3)
          .reshape(by * bx, delta * delta))
    out = np.full(by * bx, np.nan)
    for i in range(by * bx):
        v = mm[i][np.isfinite(mm[i])]
        if v.size >= min_pix:
            out[i] = float(prod_sigma(v)["sigma"])
    return out.reshape(by, bx)


def patch_truth_from_var(vmap: np.ndarray, delta: int) -> np.ndarray:
    """patch 级真值 sigma = sqrt(patch 内逐像素方差均值)（**已开方**，与 EXP-03 同口径）。"""
    ny, nx = vmap.shape
    by, bx = ny // delta, nx // delta
    v = vmap[:by * delta, :bx * delta].reshape(by, delta, bx, delta).transpose(0, 2, 1, 3)
    return np.sqrt(np.mean(v.reshape(by * bx, delta * delta), axis=1)).reshape(by, bx)


# ===========================================================================
# 2. 两个表示的定义式（本单元的核心对象）
# ===========================================================================
def frame_common_factor(sigma_patch: np.ndarray, sigma_frame_scalar: float) -> float:
    """**精确**帧内共模因子（sigma 空间）c_eff = sigma_frame * median(1/sigma_patch)。

    推导（报告 1）：相对表示在 **SNR 空间**中位归一，即 rho_c = SNR_c/median_p(SNR_c)。
    由 SNR_rel = SNR_frame * rho_c = SNR_c * (SNR_frame/median_p(SNR_c)) = SNR_c * c_S，
    其中 c_S = SNR_frame/median_p(SNR_c)（SNR 空间的常数）。换算到 sigma 空间
    （SNR 正比于 1/sigma）：
        sigma_rel_eff = sigma_c / c_S = sigma_c * median_p(SNR_c) / SNR_frame
                      = sigma_c * sigma_frame * median_p(1/sigma_c)
    ⇒ c_eff = sigma_frame * median(1/sigma_c)。

    **注意**：median(1/sigma) != 1/median(sigma)（Jensen 型不等式，只在 sigma 场无离散时相等）
    ⇒ 「sigma_frame/median(sigma_c)」只是 c_eff 的一阶近似（见 frame_common_factor_approx）。
    这是相对表示「锚点」定义本身的一处二阶歧义，报告 1.3 已登记。
    """
    s = np.asarray(sigma_patch, dtype=np.float64)
    s = s[np.isfinite(s) & (s > 0)]
    if s.size == 0 or not np.isfinite(sigma_frame_scalar) or not (sigma_frame_scalar > 0):
        return float("nan")
    return float(sigma_frame_scalar) * float(np.median(1.0 / s))


def frame_common_factor_approx(sigma_patch: np.ndarray,
                               sigma_frame_scalar: float) -> float:
    """c 的**一阶近似** sigma_frame/median(sigma_patch)（旧口径，仅作对照与文献对齐用）。"""
    s = np.asarray(sigma_patch, dtype=np.float64)
    med = float(np.median(s[np.isfinite(s)]))
    if not (med > 0) or not np.isfinite(sigma_frame_scalar):
        return float("nan")
    return float(sigma_frame_scalar) / med


def represent_absolute(sigma_patch: np.ndarray) -> np.ndarray:
    """**绝对表示**的落盘值：控制点直接存 SNR_c = F_ref/(sigma_c*sqrt(A_NEA))。

    规范口径（F_ref=1 ADU、A_NEA=1 px）下即 1/sigma_c；比值判据对该选择不变。
    """
    s = np.asarray(sigma_patch, dtype=np.float64)
    return np.where(s > 0, 1.0 / np.maximum(s, 1e-300), np.nan)


def represent_relative(sigma_patch: np.ndarray) -> np.ndarray:
    """**相对表示**的落盘值：rho_c = SNR_c / median(SNR_c)（中位归一，p50 == 1）。

    注意：rho **不是**信噪比 —— 它是信噪比的比值。物理量只有在乘回帧级标量之后才出现。
    """
    snr = represent_absolute(sigma_patch)
    med = float(np.median(snr[np.isfinite(snr)]))
    return snr / med if med > 0 else snr * np.nan


def recon_absolute(sigma_patch: np.ndarray) -> np.ndarray:
    """绝对表示的重建：控制点值**直接**就是该点的绝对 SNR。"""
    return represent_absolute(sigma_patch)


def recon_relative(sigma_patch: np.ndarray, sigma_frame_scalar: float) -> np.ndarray:
    """相对表示的重建：SNR_rel = SNR_frame * rho_c（= 乘回帧级标量）。"""
    rho = represent_relative(sigma_patch)
    snr_frame = 1.0 / float(sigma_frame_scalar) if sigma_frame_scalar > 0 else float("nan")
    return rho * snr_frame


def effective_sigma_relative(sigma_patch: np.ndarray,
                             sigma_frame_scalar: float) -> np.ndarray:
    """相对表示**等效的** sigma 场：sigma_rel_eff = sigma_c * c_sigma（严格）。

    由 SNR_rel = SNR_abs*c_S 与 SNR 正比于 1/sigma 直接得到。
    """
    c = frame_common_factor(sigma_patch, sigma_frame_scalar)
    return np.asarray(sigma_patch, dtype=np.float64) * c


# ===========================================================================
# 3. 重建算子（控制点 -> 稠密场）与尺度等变性
# ===========================================================================
def _cell_centers(n: int, delta: int) -> np.ndarray:
    return (np.arange(n) + 0.5) * delta


def upsample_nearest(grid: np.ndarray, delta: int, shape: Tuple[int, int]) -> np.ndarray:
    """最近控制点（规则网格），越界按边缘 clamp（与生产 nearest_control_point_v1 同构）。"""
    g = np.asarray(grid, dtype=np.float64)
    ny, nx = int(shape[0]), int(shape[1])
    iy = np.clip((np.arange(ny) // delta), 0, g.shape[0] - 1)
    ix = np.clip((np.arange(nx) // delta), 0, g.shape[1] - 1)
    return g[np.ix_(iy, ix)]


def upsample_bilinear(grid: np.ndarray, delta: int, shape: Tuple[int, int]) -> np.ndarray:
    """规则网格双线性（生产 bilinear_regular_grid_v1 同构：控制点在 cell 中心，边缘 clamp）。"""
    g = np.asarray(grid, dtype=np.float64)
    ny, nx = int(shape[0]), int(shape[1])
    cy = _cell_centers(g.shape[0], delta)
    cx = _cell_centers(g.shape[1], delta)

    def _axis(centers: np.ndarray, n: int):
        y = np.arange(n, dtype=np.float64) + 0.5
        idx = np.searchsorted(centers, y) - 1
        idx = np.clip(idx, 0, centers.size - 2) if centers.size > 1 else np.zeros(n, int)
        x0 = centers[idx]
        x1 = centers[np.minimum(idx + 1, centers.size - 1)]
        w = np.where(x1 > x0, (y - x0) / np.maximum(x1 - x0, 1e-300), 0.0)
        return idx, np.minimum(idx + 1, centers.size - 1), np.clip(w, 0.0, 1.0)

    iy, iy1, wy = _axis(cy, ny)
    ix, ix1, wx = _axis(cx, nx)
    wy = wy[:, None]
    wx = wx[None, :]
    v00 = g[np.ix_(iy, ix)]
    v01 = g[np.ix_(iy, ix1)]
    v10 = g[np.ix_(iy1, ix)]
    v11 = g[np.ix_(iy1, ix1)]
    return ((1 - wy) * ((1 - wx) * v00 + wx * v01)
            + wy * ((1 - wx) * v10 + wx * v11))


def _pdist(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    d = a[:, None, :] - b[None, :, :]
    return np.sqrt(np.sum(d * d, axis=2))


def upsample_kriging_like(grid: np.ndarray, delta: int, shape: Tuple[int, int],
                          corr_px: float = 96.0, nugget_frac: float = 1e-6) -> np.ndarray:
    """**类 kriging 的线性重建**（指数协方差 + 常数均值 GLS），用于检验尺度等变性。

    本函数**不是**生产实现，也不是选型结论；它只用来检验「重建算子是尺度等变的」
    这一条件对带均值拟合的算子是否仍然成立（报告 1 条件 C2）。
    """
    g = np.asarray(grid, dtype=np.float64)
    ny, nx = int(shape[0]), int(shape[1])
    cy = _cell_centers(g.shape[0], delta)
    cx = _cell_centers(g.shape[1], delta)
    gy, gx = np.meshgrid(cy, cx, indexing="ij")
    pts = np.stack([gy.ravel(), gx.ravel()], axis=1)
    vals = g.ravel()
    ok = np.isfinite(vals)
    pts, vals = pts[ok], vals[ok]
    n = pts.shape[0]
    K = np.exp(-_pdist(pts, pts) / corr_px) + nugget_frac * np.eye(n)
    try:
        Kc = np.linalg.cholesky(K)
    except np.linalg.LinAlgError:                              # pragma: no cover
        Kc = None
    out = np.empty((ny, nx), dtype=np.float64)
    yy = np.arange(ny, dtype=np.float64) + 0.5
    xx = np.arange(nx, dtype=np.float64) + 0.5
    alpha = np.linalg.solve(Kc, vals) if Kc is not None else None
    blk = 64
    for y0 in range(0, ny, blk):
        y1 = min(y0 + blk, ny)
        Y, X = np.meshgrid(yy[y0:y1], xx, indexing="ij")
        q = np.stack([Y.ravel(), X.ravel()], axis=1)
        k = np.exp(-_pdist(q, pts) / corr_px)
        if Kc is not None:
            pred = k @ np.linalg.solve(Kc.T, alpha)
        else:                                                  # pragma: no cover
            pred = k @ np.linalg.solve(K, vals)
        out[y0:y1, :] = pred.reshape(y1 - y0, nx)
    return out


def scale_equivariance(op, grid: np.ndarray, alpha: float, delta: int,
                       shape: Tuple[int, int]) -> float:
    """检验 op[alpha*grid] == alpha*op[grid]（尺度等变性）的最大相对偏差。"""
    a = op(grid, delta, shape)
    b = op(grid * alpha, delta, shape) / alpha
    m = np.isfinite(a) & np.isfinite(b)
    den = np.maximum(np.abs(a[m]), 1e-300)
    return float(np.max(np.abs(a[m] - b[m]) / den)) if m.any() else float("nan")


# ===========================================================================
# 4. 度量（全部是比值口径，F_ref/A_NEA 相消）
# ===========================================================================
def _flat(x):
    return np.asarray(x, dtype=np.float64).ravel()


def level_dev(est_sigma: np.ndarray, true_sigma: np.ndarray) -> Dict[str, float]:
    """电平偏差（**sigma 空间**）：r = median(est/true)；SNR 偏差 = 1/r - 1（严格）。

    * median_ratio  —— 稳健的中心比（对尾部不敏感）；
    * snr_rel_dev   —— 等效的 SNR 相对偏差 1/r - 1（SNR 偏低为正、偏高为负）；
    * p16/p84       —— 逐点比值的 p16/p84 区间（形状误差，与电平正交）。
    """
    e, t = _flat(est_sigma), _flat(true_sigma)
    m = np.isfinite(e) & np.isfinite(t) & (t > 0)
    if not m.any():
        return {"median_ratio": float("nan"), "snr_rel_dev": float("nan"),
                "p16": float("nan"), "p84": float("nan"), "n": 0}
    r = e[m] / t[m]
    med = float(np.median(r))
    return {"median_ratio": med,
            "snr_rel_dev": (1.0 / med - 1.0) if med > 0 else float("nan"),
            "p16": float(np.percentile(r, 16)), "p84": float(np.percentile(r, 84)),
            "p95_abs_dev": float(np.percentile(np.abs(r - 1.0), 95)), "n": int(m.sum())}


def frame_est_bias(sigma_frame_scalar: float, true_frame_rms: float) -> float:
    """帧级估计器偏差 b_f = sigma_frame/sigma_true_RMS（sigma 空间）。"""
    return (float(sigma_frame_scalar) / float(true_frame_rms)
            if true_frame_rms > 0 else float("nan"))


def aggregation_mismatch(true_sigma_field: np.ndarray) -> float:
    """聚合口径错配 c_agg = RMS(sigma_true)/median_patch(sigma_true)（真值层面，纯定义性）。

    帧级估计量是**平方聚合**（RMS），控制点中位是**中位聚合** —— 只要 sigma 场有空间离散，
    两者就不同。这是「相对表示电平锚点」误差的**第二个独立来源**（与结构污染正交）。
    """
    t = _flat(true_sigma_field)
    t = t[np.isfinite(t) & (t > 0)]
    if t.size == 0:
        return float("nan")
    return float(math.sqrt(np.mean(t * t)) / np.median(t))


def weight_efficiency(w: np.ndarray, var_true: np.ndarray) -> float:
    """权重效率损失 E = Var_w/Var_opt - 1（>=0；E=0 当且仅当 w 正比于 1/var_true）。

    全局尺度不变 ⇒ **帧内共模因子对 E 严格无影响**（这正是 EXP-1 实测 ratio=1.000000 的原因）。
    """
    ww, vv = _flat(w), _flat(var_true)
    m = np.isfinite(ww) & np.isfinite(vv) & (vv > 0)
    if m.sum() < 2:
        return float("nan")
    ww, vv = ww[m], vv[m]
    sw = float(np.sum(ww))
    if not (sw > 0):
        return float("nan")
    var_w = float(np.sum(ww * ww * vv)) / (sw * sw)
    var_opt = 1.0 / float(np.sum(1.0 / vv))
    return var_w / var_opt - 1.0


def combined_snr_sq(snr_list: Sequence[np.ndarray]) -> np.ndarray:
    """跨帧组合 SNR_combined^2 = sum_k SNR_k^2（CONTROL_WEIGHT_SNR.md 8a 第 3 条定案式）。"""
    acc = None
    for s in snr_list:
        a = np.asarray(s, dtype=np.float64) ** 2
        acc = a if acc is None else acc + a
    return acc if acc is not None else np.zeros(0)


def cross_frame_ratio_dev(est: Sequence[np.ndarray], true: Sequence[np.ndarray],
                          ref: int = 0) -> Dict[str, Any]:
    """跨帧比值的相对偏差（SCI-B 核心目标的直接判据）。

    est / true 是同一位置、K 帧的 sigma 场。取第 ref 帧为基准，逐点算
    (est_k/est_ref)/(true_k/true_ref) 的中位与散布。共模因子 c_k 不同 ⇒ 该比值被
    c_k/c_ref 污染；c_k 相同 ⇒ 严格相消（负例）。
    """
    out: Dict[str, Any] = {}
    for k in range(len(est)):
        if k == ref:
            continue
        e = _flat(est[k]) / np.maximum(_flat(est[ref]), 1e-300)
        t = _flat(true[k]) / np.maximum(_flat(true[ref]), 1e-300)
        m = np.isfinite(e) & np.isfinite(t) & (t > 0)
        if not m.any():
            continue
        r = e[m] / t[m]
        out["frame_%d_over_%d" % (k, ref)] = {
            "median_ratio": float(np.median(r)),
            "snr_rel_dev": float(1.0 / np.median(r) - 1.0),
            "p95_abs_dev": float(np.percentile(np.abs(r - 1.0), 95)),
            "n": int(m.sum())}
    return out


# ===========================================================================
# 5. 门（能红能绿；每条都带显式阈值与实测值）
# ===========================================================================
def gate(gates: List[Dict[str, Any]], name: str, passed: bool, detail: str,
         value: Any = None, expect: str = "") -> None:
    gates.append({"gate": name, "verdict": "PASS" if passed else "FAIL",
                  "expect": expect, "detail": detail, "value": value})


def all_pass(gates: Sequence[Dict[str, Any]]) -> bool:
    return all(g["verdict"] == "PASS" for g in gates)
