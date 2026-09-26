#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mc_kcorr.py - P3 平面到球面通量守恒映射算子补实验: k_corr=1.3883 的受控蒙特卡洛溯源

正本几何(唯一记录: lib/algorithms/drizzle/healpix_drizzle/tests/control_median_mc_test.cpp,
配合 docs/science/PHASE2_UPM.md SS5/SS4 与 docs/algorithms/PHASE2_SAMPLER.md SS5.4 的文字记载):
  - 单帧 20x20 源图, 源像素角尺度 300"/px (deg_per_px = 300/3600, 测试源码 :56)
  - HEALPix nside=512 NESTED, 输出 leaf 等面积尺度 = 211076.28514206142/512 = 412.2552"/px
    (DRIZZLE_GEOMETRY.md SS3: HEALPIX_SCALE_PER_NSIDE_ARCSEC, Gorski 2005 等面积基数)
  - pixfrac = 0.8 (生产默认), tile_depth = 9
  - NMC = 2000 个独立高斯噪声实现 (seed = 20260816 + r, r=0..1999)
  - patch = 首个 tile 的全部 touched leaf (support>0), N_retained ~ 251
  - sigma_bg = 逐实现 1.482602218505602 x MAD(patch - median(patch)), 取跨实现中位数
  - k_corr = Var_emp(median) / [ (pi/2) x sigma_bg^2 / N_retained ]

模型: 平面极限下的 drizzle 线性算子。
  依据 DRIZZLE_GEOMETRY.md SS10 DISP-DRZ-009 段, A_drop = pixfrac^2 x A_pixel 只在平面
  (仿射)极限下精确, 球面残差 delta = 1.9e-7 (theta=300"/px, pixfrac=0.8),
  对二阶统计量(方差/协方差)的影响 O(delta) << MC 统计噪声(~3%),
  故平面方形网格模型是该正本几何的忠实实现。
  输出值 = sumFlux/sumArea (正本测试 :115/:160 的取值口径):
    v_p = sum_j (a_jp / D_p) * (x_j / A_drop_j),  D_p = sum_j a_jp
  其中 a_jp = drop_j 与输出像素 p 的交叠面积, 权重 (a_jp/D_p) 对 j 凸组合归一。
  x_j/A_drop_j 为全局常数因子, 不影响 k_corr(无量纲比值), 故实现中取 v = M @ x。

纪律: 纯 Python + numpy; 不 import 仓库任何代码; 全部 seed 写死为具名常数。
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from typing import Sequence

import numpy as np

# ---------------- 冻结常数(写死) ----------------
SEED_BASE = 20260816              # 正本 control_median_mc_test.cpp:139 的基 seed
NMC_CANONICAL = 2000              # 正本 NMC
NMC_SCAN = 4000                   # 扫描档(减 MC 噪声, sqrt(2/4000)=2.2%)
SIGMA_SRC = 10.0                  # 正本 SIGMA (ADU/源像素)
W_SRC = 20                        # 正本 W=H=20
H_SRC = 20
SRC_SCALE_ARCSEC = 300.0          # 正本源像素角尺度
NSIDE_CANONICAL = 512             # 正本 nside
HEALPIX_BASE_ARCSEC = 211076.28514206142   # sqrt(pi/3)*(180/pi)*3600 (DRIZZLE_GEOMETRY.md SS3)
MAD_C = 1.482602218505602         # 正本 MAD->sigma 常数(与 sampler.cpp 一致)
PI_HALF = 0.5 * math.pi


def healpix_leaf_arcsec(nside: int) -> float:
    return HEALPIX_BASE_ARCSEC / float(nside)


# ---------------- 平面 drizzle 算子 ----------------

@dataclass
class Operator:
    """平面 drizzle 线性算子: v = M @ x, M[p,j] = a_jp / D_p (凸组合权重)."""
    M: np.ndarray                 # (n_out, n_src)
    out_x0: np.ndarray            # (n_out,) 输出像素左下角 x (touched)
    out_y0: np.ndarray
    out_step: float
    touched_idx: np.ndarray       # D_p>0 的行号
    D: np.ndarray                 # (n_out,) 覆盖面积 sum_j a_jp
    meta: dict = field(default_factory=dict)


def build_operator(
    w: int = W_SRC, h: int = H_SRC,
    src_scale: float = SRC_SCALE_ARCSEC,
    out_scale: float | None = None,
    pixfrac: float = 0.8,
    phase: tuple = (0.0, 0.0),
    frame_offsets: Sequence = ((0.0, 0.0),),
) -> Operator:
    """构造算子. out_scale 缺省 = 正本 nside=512 的 leaf 尺度.

    frame_offsets: 多帧 dither (单位 = 源像素), 每帧一个独立源网格平移;
      多帧 = 各帧源像素并入同一源清单(对应引擎逐帧累加 sumFlux/sumArea 的口径).
    phase: 输出网格相对源网格原点的亚像素平移(单位 = 源像素, 模拟 HEALPix
      网格与帧 WCS 的任意相对相位; 正本为某一固定但未记载的相位).
    """
    if out_scale is None:
        out_scale = healpix_leaf_arcsec(NSIDE_CANONICAL)
    s = src_scale
    half = 0.5 * pixfrac * s            # drop 半边长
    xs, ys, n_per_frame = [], [], []
    for (dx, dy) in frame_offsets:
        cx = (np.arange(w) + 0.5 + dx) * s
        cy = (np.arange(h) + 0.5 + dy) * s
        gx, gy = np.meshgrid(cx, cy)
        xs.append(gx.ravel())
        ys.append(gy.ravel())
        n_per_frame.append(int(gx.size))
    cx = np.concatenate(xs)
    cy = np.concatenate(ys)
    n_src = int(cx.size)

    ox0 = phase[0] * s
    oy0 = phase[1] * s
    xmin, xmax = ox0 - half, ox0 + w * s + half
    ymin, ymax = oy0 - half, oy0 + h * s + half
    nx = int(math.ceil((xmax - xmin) / out_scale))
    ny = int(math.ceil((ymax - ymin) / out_scale))
    ix = np.arange(nx)
    iy = np.arange(ny)
    gx, gy = np.meshgrid(ix, iy)
    out_x0 = xmin + gx.ravel() * out_scale
    out_y0 = ymin + gy.ravel() * out_scale
    n_out = int(out_x0.size)

    # 交叠面积: 全部 (out, src) 对, 直接广播 (n_out x n_src ~ 1e6)
    dx1 = np.minimum(out_x0[:, None] + out_scale, (cx + half)[None, :])
    dx0 = np.maximum(out_x0[:, None], (cx - half)[None, :])
    dy1 = np.minimum(out_y0[:, None] + out_scale, (cy + half)[None, :])
    dy0 = np.maximum(out_y0[:, None], (cy - half)[None, :])
    C = np.clip(dx1 - dx0, 0.0, None) * np.clip(dy1 - dy0, 0.0, None)
    D = C.sum(axis=1)
    touched = np.where(D > 0)[0]
    M = C[touched] / D[touched, None]
    meta = dict(
        n_src=n_src, n_out=n_out, n_touched=int(touched.size),
        n_per_frame=n_per_frame,
        out_scale=out_scale, src_scale=src_scale, pixfrac=pixfrac,
        rho=out_scale / src_scale, phase=list(phase),
        frame_offsets=[list(o) for o in frame_offsets],
    )
    return Operator(M=M, out_x0=out_x0[touched], out_y0=out_y0[touched],
                    out_step=out_scale,
                    touched_idx=np.arange(int(touched.size)),  # 指 M 的行域
                    D=D[touched], meta=meta)


# ---------------- patch 选取 ----------------

def patch_nearest(op: Operator, n: int, cx: float, cy: float) -> np.ndarray:
    """距图中心最近的 n 个 touched 输出像素(tie 按 (y,x) 索引序, 确定性)."""
    d2 = (op.out_x0 + op.out_step / 2 - cx) ** 2 + (op.out_y0 + op.out_step / 2 - cy) ** 2
    order = np.lexsort((np.arange(d2.size), d2))
    return np.sort(order[:n])


def patch_shapes(op: Operator, cx: float, cy: float) -> dict:
    """N=5 的三种代表性形状: 十字(紧凑), 1x5 线, 远散(互距 >= 3 个输出像素)."""
    px = op.out_x0 + op.out_step / 2 - cx
    py = op.out_y0 + op.out_step / 2 - cy
    d2 = px ** 2 + py ** 2
    near = np.lexsort((np.arange(d2.size), d2))[:9]      # 3x3 紧凑块
    cen = int(near[0])
    cxx, cyy = px[cen], py[cen]
    step = op.out_step

    def at(tx, ty):
        cand = np.where((np.abs(px - tx) < 0.3 * step) & (np.abs(py - ty) < 0.3 * step))[0]
        return int(cand[0]) if cand.size else None

    cross = [cen]
    for tx, ty in ((cxx + step, cyy), (cxx - step, cyy), (cxx, cyy + step), (cxx, cyy - step)):
        i = at(tx, ty)
        if i is not None:
            cross.append(i)
    line = [cen]
    for k in (1, 2):
        i = at(cxx + k * step, cyy)
        if i is not None:
            line.append(i)
        j = at(cxx - k * step, cyy)
        if j is not None:
            line.append(j)
    far = [cen]
    for i in np.argsort(d2):
        if len(far) >= 5:
            break
        if all((px[i] - px[j]) ** 2 + (py[i] - py[j]) ** 2 >= (3 * step) ** 2 for j in far):
            far.append(int(i))
    return {"cross5": np.array(cross[:5]), "line5": np.array(line[:5]), "far5": np.array(far[:5])}


# ---------------- MC 核心 ----------------

def mc_medians(M: np.ndarray, patch: np.ndarray, nmc: int, sigma: float, seed0: int,
               n_src: int, batch: int = 250) -> np.ndarray:
    """返回 (nmc, len(patch)) 的 patch 值矩阵. x ~ N(0, sigma^2) (SKY 常数已减除)."""
    rng = np.random.default_rng(seed0)
    Mp = np.asarray(M, dtype=np.float64)
    out = np.empty((nmc, patch.size))
    done = 0
    while done < nmc:
        b = min(batch, nmc - done)
        x = rng.normal(0.0, sigma, size=(b, n_src))
        V = x @ Mp.T                             # (b, n_out)
        out[done:done + b] = V[:, patch]
        done += b
    return out


def median_stats(patch_vals: np.ndarray) -> dict:
    """正本口径: median(偶数取中间两值平均, numpy 同), 逐实现 MAD 尺度, 跨实现中位."""
    meds = np.median(patch_vals, axis=1)
    var_emp = float(meds.var(ddof=1))
    mad_sigma = MAD_C * np.median(np.abs(patch_vals - meds[:, None]), axis=1)
    sigma_bg = float(np.median(mad_sigma))
    n = int(patch_vals.shape[1])
    baseline = PI_HALF * sigma_bg ** 2 / n
    return dict(N=n, var_emp=var_emp, sigma_bg=sigma_bg,
                baseline=baseline, k_corr=var_emp / baseline,
                median_mean=float(meds.mean()))


def iid_marginal_arm(pool: np.ndarray, n: int, nmc: int, seed: int) -> dict:
    """形状臂: 从边际经验分布 iid 重采样, 隔离(非高斯边际形状)对 k_corr 的贡献."""
    rng = np.random.default_rng(seed)
    draws = rng.choice(pool, size=(nmc, n), replace=True)
    return median_stats(draws)


def exact_cov_diag(M: np.ndarray, patch: np.ndarray, sigma: float) -> dict:
    """由线性算子精确计算 patch 协方差结构(对高斯输入是精确的)."""
    Mp = M[patch]                                  # (N, n_src)
    Cov = sigma ** 2 * (Mp @ Mp.T)
    n = patch.size
    var_mean = float(Cov.sum() / n ** 2)
    var_marg = float(np.mean(np.diag(Cov)))
    n_eff_mean = n * var_marg / var_mean
    return dict(N=int(n), n_eff_mean_cov=float(n_eff_mean),
                var_mean_exact=var_mean, var_marg_mean_exact=var_marg)


def neighbor_correlation(op: Operator, patch: np.ndarray, sigma: float) -> dict:
    """最近邻(4 邻域内)平均相关系数: 由算子精确算."""
    Mp = op.M[patch]
    Cov = sigma ** 2 * (Mp @ Mp.T)
    d = np.sqrt(np.diag(Cov))
    R = Cov / np.outer(d, d)
    step = op.out_step
    xs = op.out_x0[patch] + step / 2
    ys = op.out_y0[patch] + step / 2
    vals = []
    for i in range(patch.size):
        for j in range(i + 1, patch.size):
            dx = abs(xs[i] - xs[j])
            dy = abs(ys[i] - ys[j])
            if max(dx, dy) <= 1.5 * step and min(dx, dy) <= 0.5 * step:
                vals.append(float(R[i, j]))
    return dict(mean_nn_corr=float(np.mean(vals)) if vals else None,
                n_nn_pairs=len(vals))


def run_config(op: Operator, patch: np.ndarray, nmc: int, sigma: float,
               seed0: int, iid_seed: int, with_diag: bool = True,
               n_batches: int = 8) -> dict:
    V = mc_medians(op.M, patch, nmc, sigma, seed0, n_src=op.M.shape[1])
    full = median_stats(V)
    shape = iid_marginal_arm(V.ravel(), full["N"], nmc, iid_seed)
    # MC 统计误差: 分 n_batches 段各算 k_corr, 段间标准差 / sqrt(n_batches)
    bs = nmc // n_batches
    kbs = [median_stats(V[i * bs:(i + 1) * bs])["k_corr"] for i in range(n_batches)]
    k_se = float(np.std(kbs, ddof=1) / np.sqrt(n_batches))
    res = dict(
        seed0=seed0, nmc=nmc, k_corr_mc_se=k_se,
        k_corr=full["k_corr"], var_emp=full["var_emp"], sigma_bg=full["sigma_bg"],
        N=full["N"], baseline=full["baseline"],
        k_shape=shape["k_corr"], sigma_bg_iid=shape["sigma_bg"],
        k_geom=(full["k_corr"] / shape["k_corr"]) if shape["k_corr"] else None,
    )
    if with_diag:
        res["exact"] = exact_cov_diag(op.M, patch, sigma)
        res["nn_corr"] = neighbor_correlation(op, patch, sigma)
    return res


def fh_ratio(op: Operator, sigma: float) -> dict:
    """用本算子对 Fruchter & Hook 2002 式(8) 的 R = sigma_c/sigma_p 做逐像素数值复核.

    F&H 口径(w=1, 等方差 sigma^2):
      sigma_p^2 = sigma^2 x sum_{j in P} a_jp^2 / D_p^2          (式(7), 除以 A_drop^2 后)
      sigma_c^2 = sigma^2 / |C_p|                                (式(6), |C_p|>=1; 除以 A_drop^2)
      其中 C_p = 中心落在输出像素 p 内的源像素集合, P = drop 与 p 有交叠的源像素集合.
    返回 patch 范围内的逐像素 R 统计(仅 |C_p|>=1 的像素).
    注: F&H 式(9)/(10) 闭式假设均匀充满 dither(多帧等覆盖), 与单帧正本几何不同,
    故只作量级对照, 不作恒等断言.
    """
    half_drop = 0.5 * op.meta["pixfrac"] * op.meta["src_scale"]
    # 重建源网格中心(单帧口径; fh_ratio 只用于单帧算子)
    w_src = round((op.M.shape[1]) ** 0.5)
    s = op.meta["src_scale"]
    sx = (np.arange(w_src) + 0.5) * s
    gx, gy = np.meshgrid(sx, sx)
    cx = gx.ravel()
    cy = gy.ravel()
    out_cx = op.out_x0 + op.out_step / 2
    out_cy = op.out_y0 + op.out_step / 2
    Rvals = []
    for p in range(op.M.shape[0]):
        inC = ((np.abs(cx - out_cx[p]) <= op.out_step / 2) &
               (np.abs(cy - out_cy[p]) <= op.out_step / 2))
        nC = int(inC.sum())
        if nC == 0:
            continue
        row = op.M[p]
        var_p = sigma ** 2 * float(np.sum(row ** 2))          # 除以 A_drop^2 后
        var_c = sigma ** 2 / nC                               # 同口径
        if var_p <= 0:
            continue
        Rvals.append(math.sqrt(var_c / var_p))
    Rarr = np.array(Rvals)
    return dict(n_pix=int(Rarr.size), R_median=float(np.median(Rarr)),
                R_mean=float(Rarr.mean()), R_p16=float(np.percentile(Rarr, 16)),
                R_p84=float(np.percentile(Rarr, 84)),
                fh_closed_form_r=float(op.meta["pixfrac"] / op.meta["rho"]),
                note="closed form valid only for filled uniform dither (multi-frame)")
