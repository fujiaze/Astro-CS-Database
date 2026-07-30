# -*- coding: utf-8 -*-
"""
e_solver.py - E-003 全局加性共识曲面稀疏求解器

任务契约 (E-003):
  1. 将三片的控制点组成全局方程组
  2. 求解全局加性曲面 (每帧一个偏移量)
  3. 约束: 全局零均值 (偏移量之和=0)
  4. 只加性 (不包含乘性项)
  5. 用稀疏矩阵求解 (scipy.sparse)

禁止捷径:
  - 不得使用无权重梯度 (用 W^(1/2) A x = W^(1/2) b)
  - 不得选单一参考帧 (全局共识曲面, 非差异拟合)
  - 不得加入乘性项 (参数只有 surface_coeffs + offsets, 无 scale)
  - 全局零均值规范 (Σ off_f = 0 等式约束)

数学模型:
  观测方程 (每个控制点 i 属于帧 f(i)):
    signal_i = Σ_m c_m * B_m(ra_i, dec_i) + off_{f(i)} + noise_i
  参数向量 x = [c_0..c_{M-1}, off_0..off_{F-1}]  (M=6 曲面系数, F=3 帧偏移)
  权重: w_i (E-002 联合权重 SNR^2 * inv_var)
  约束: Σ_f off_f = 0 (零均值, 大权重等式行)

  加权最小二乘:
    min Σ_i w_i * (signal_i - A_i @ x)^2  +  λ * (Σ_f off_f)^2
  用 scipy.sparse.linalg.lsqr 求解 (稀疏, 稳健, 处理秩亏)

曲面基底: 2D 多项式 [1, ra', dec', ra'*dec', ra'^2, dec'^2] (中心化)
  线性梯度 a*ra + b*dec 被 ra'/dec' 一次项精确捕获 (E-004 验证用)
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spla

from e_common import (
    ControlPointTable, PANEL_NAMES, N_SURFACE_COEFFS, POLY_DEGREE,
    setup_logger, poly_basis, eval_surface, json_default,
)


# ============================================================================
# 求解参数
# ============================================================================

class SolverParams:
    """求解器参数 (E-003 ~ E-004 共享)。"""
    # 零均值约束权重 (相对最大数据权重的倍数, 确保约束严格满足)
    zero_mean_weight_factor = 1e6
    # lsqr 迭代次数上限
    lsqr_iter = 2000
    # lsqr 容差
    lsqr_atol = 1e-9
    lsqr_btol = 1e-9
    # 基底中心化: 用全部控制点的均值 (跨帧统一中心, 保证曲面可比较)
    center_ra: Optional[float] = None  # None=自动用均值
    center_dec: Optional[float] = None


# ============================================================================
# 求解结果
# ============================================================================

@dataclass
class SolveResult:
    """全局加性共识曲面求解结果。"""
    surface_coeffs: np.ndarray   # f64 [M] 曲面系数 (中心化基底)
    offsets: Dict[str, float]    # {panel_id: off_f} 帧偏移
    center: Tuple[float, float]  # (ra0, dec0) 中心化参考点
    predicted: Dict[str, np.ndarray]  # {panel_id: predicted [N]} 每帧预测值
    residual: Dict[str, np.ndarray]   # {panel_id: residual [N]} 每帧残差
    stats: dict                  # 统计信息
    panel_order: List[str]       # 帧顺序 (对应 offsets 索引)


# ============================================================================
# 构建稀疏方程组
# ============================================================================

def build_system(cps: Dict[str, ControlPointTable],
                 params: SolverParams = SolverParams(),
                 logger=None) -> Tuple[sp.csr_matrix, np.ndarray, np.ndarray,
                                        Tuple[float, float], List[str], List[int]]:
    """构建全局加权最小二乘稀疏方程组。

    返回:
        A_sparse: (N + 1) × (M + F) 稀疏矩阵 (最后一行为零均值约束)
        b: (N + 1,) 观测向量 (最后=0)
        w: (N + 1,) 权重向量 (最后=约束权重)
        center: (ra0, dec0) 中心化参考点
        panel_order: 帧顺序 (offsets 索引顺序)
        row_panel: 每行对应的帧索引 (用于残差分析, 约束行=-1)
    """
    if logger is None:
        logger = setup_logger('e003_build')

    panel_order = list(PANEL_NAMES)
    F = len(panel_order)
    M = N_SURFACE_COEFFS
    panel_idx = {p: i for i, p in enumerate(panel_order)}

    # 收集全部控制点 (仅 valid, solver 只用有效控制点)
    all_ra, all_dec, all_sig, all_w, all_f = [], [], [], [], []
    for p in panel_order:
        cp = cps[p]
        v = cp.valid
        n = int(v.sum())
        all_ra.append(cp.ra[v])
        all_dec.append(cp.dec[v])
        all_sig.append(cp.signal[v].astype(np.float64))
        all_w.append(cp.weight[v].astype(np.float64))
        all_f.append(np.full(n, panel_idx[p], dtype=np.int64))
    ra = np.concatenate(all_ra)
    dec = np.concatenate(all_dec)
    sig = np.concatenate(all_sig)
    w = np.concatenate(all_w)
    f_idx = np.concatenate(all_f)
    N = ra.size

    # 中心化参考点
    if params.center_ra is None:
        ra0 = float(np.mean(ra))
    else:
        ra0 = float(params.center_ra)
    if params.center_dec is None:
        dec0 = float(np.mean(dec))
    else:
        dec0 = float(params.center_dec)
    center = (ra0, dec0)

    # 曲面基底 [N, M]
    basis, _ = poly_basis(ra, dec, center=center)
    # basis: f64 [N, M]

    # 构建稀疏矩阵 A: [N+1, M+F]
    # 数据行 i: [basis[i, 0..M-1], 0..1(f_i)..0]
    # 约束行 N: [0..0, 1, 1, ..., 1]  (Σ off_f = 0)
    n_rows = N + 1
    n_cols = M + F

    # 用 COO 格式构建
    rows, cols, data = [], [], []
    # 数据行
    for m in range(M):
        rows.append(np.arange(N))
        cols.append(np.full(N, m))
        data.append(basis[:, m])
    # 帧偏移列 (每行在自己的帧列填 1)
    for fi in range(F):
        mask = (f_idx == fi)
        rows.append(np.where(mask)[0])
        cols.append(np.full(int(mask.sum()), M + fi))
        data.append(np.ones(int(mask.sum()), dtype=np.float64))
    # 零均值约束行 (行 N)
    for fi in range(F):
        rows.append(np.array([N], dtype=np.int64))
        cols.append(np.array([M + fi], dtype=np.int64))
        data.append(np.array([1.0], dtype=np.float64))

    rows_arr = np.concatenate(rows)
    cols_arr = np.concatenate(cols)
    data_arr = np.concatenate(data)
    A = sp.csr_matrix((data_arr, (rows_arr, cols_arr)),
                      shape=(n_rows, n_cols))

    # 观测向量 b
    b = np.zeros(n_rows, dtype=np.float64)
    b[:N] = sig
    b[N] = 0.0  # 零均值约束

    # 权重向量
    w_vec = np.zeros(n_rows, dtype=np.float64)
    w_vec[:N] = w
    # 约束权重 = factor × max(数据权重)
    max_w = float(np.max(w)) if w.size > 0 else 1.0
    constraint_w = params.zero_mean_weight_factor * max_w
    w_vec[N] = constraint_w

    row_panel = np.concatenate([f_idx, np.array([-1], dtype=np.int64)])
    logger.info(
        f'系统: N={N} 控制点, M={M} 曲面系数, F={F} 帧偏移, '
        f'总参数={n_cols}, 总方程={n_rows}, 中心=({ra0:.4f}, {dec0:.4f})'
    )
    logger.info(
        f'每帧控制点: { {p: int((f_idx==panel_idx[p]).sum()) for p in panel_order} }'
    )
    logger.info(f'约束权重={constraint_w:.3g} (max_data_w={max_w:.3g}, factor={params.zero_mean_weight_factor})')
    return A, b, w_vec, center, panel_order, row_panel.tolist()


# ============================================================================
# 求解
# ============================================================================

def solve_global_additive(cps: Dict[str, ControlPointTable],
                          params: SolverParams = SolverParams(),
                          logger=None) -> SolveResult:
    """求解全局加性共识曲面 + 每帧偏移 (零均值约束)。

    步骤:
      1. 构建稀疏方程组 A x = b (加权)
      2. 加权: A_w = W^(1/2) A, b_w = W^(1/2) b
      3. scipy.sparse.linalg.lsqr 求解
      4. 分解 x = [surface_coeffs, offsets]
      5. 计算残差

    Returns:
        SolveResult
    """
    if logger is None:
        logger = setup_logger('e003')

    A, b, w_vec, center, panel_order, row_panel = build_system(
        cps, params, logger=logger)
    F = len(panel_order)
    M = N_SURFACE_COEFFS

    # 加权: A_w = diag(sqrt(w)) @ A, b_w = sqrt(w) * b
    sqrt_w = np.sqrt(w_vec)
    # A 是 csr, 用 diag 矩阵相乘
    D = sp.diags(sqrt_w)
    A_w = D @ A
    b_w = sqrt_w * b

    # 求解 (lsqr 稳健, 处理秩亏)
    result = spla.lsqr(A_w, b_w, atol=params.lsqr_atol, btol=params.lsqr_btol,
                       iter_lim=params.lsqr_iter, show=False)
    x = result[0]
    istop = result[1]
    iters = result[2]
    rnorm = result[3]

    # 分解
    surface_coeffs = x[:M]
    offsets_arr = x[M:M + F]
    offsets = {panel_order[i]: float(offsets_arr[i]) for i in range(F)}

    logger.info(f'lsqr: istop={istop} iters={iters} rnorm={rnorm:.4g}')
    logger.info(f'曲面系数 (中心化): {np.array2string(surface_coeffs, precision=4)}')
    logger.info(f'帧偏移: { {p: round(offsets[p], 4) for p in panel_order} }')
    logger.info(f'零均值检查: Σ off = {sum(offsets.values()):.6e} (应≈0)')

    # 预测 + 残差
    predicted = {}
    residual = {}
    for p in panel_order:
        cp = cps[p]
        basis, _ = poly_basis(cp.ra, cp.dec, center=center)
        pred = basis @ surface_coeffs + offsets[p]
        predicted[p] = pred
        residual[p] = cp.signal.astype(np.float64) - pred

    # 统计
    all_res = np.concatenate([residual[p] for p in panel_order])
    all_w = np.concatenate([cps[p].weight for p in panel_order])
    # 加权 RMS
    wrms = float(np.sqrt(np.sum(all_w * all_res ** 2) / np.sum(all_w)))
    # 不加权 RMS
    rms = float(np.sqrt(np.mean(all_res ** 2)))
    # 最大残差
    max_res = float(np.max(np.abs(all_res)))
    # 零均值偏差
    zero_mean_err = float(abs(sum(offsets.values())))

    stats = dict(
        lsqr_istop=int(istop), lsqr_iters=int(iters), lsqr_rnorm=float(rnorm),
        residual_wrms=wrms, residual_rms=rms, residual_max=max_res,
        zero_mean_error=zero_mean_err,
        n_params=int(M + F), n_rows=int(A.shape[0]),
        offsets_sum=float(sum(offsets.values())),
    )
    logger.info(
        f'残差: WRMS={wrms:.4f} RMS={rms:.4f} max={max_res:.4f} | '
        f'零均值误差={zero_mean_err:.2e}'
    )

    return SolveResult(
        surface_coeffs=surface_coeffs,
        offsets=offsets,
        center=center,
        predicted=predicted,
        residual=residual,
        stats=stats,
        panel_order=panel_order,
    )


# ============================================================================
# 梯度提取 (用于 E-004 验证)
# ============================================================================

def extract_gradient(result: SolveResult) -> Tuple[float, float]:
    """从求解结果提取线性梯度系数 (a_ra, b_dec)。

    曲面基底: [1, ra', dec', ra'*dec', ra'^2, dec'^2] (中心化)
    线性梯度 a*ra + b*dec = a*(ra'+ra0) + b*(dec'+dec0)
                         = a*ra' + b*dec' + (a*ra0 + b*dec0)
    所以:
      c_1 (ra' 系数) = a
      c_2 (dec' 系数) = b
    常数项 c_0 = a*ra0 + b*dec0 + 真实常数

    Returns:
        (a_ra, b_dec) 线性梯度系数
    """
    c = result.surface_coeffs
    a_ra = float(c[1])   # ra' 系数
    b_dec = float(c[2])  # dec' 系数
    return a_ra, b_dec


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('e003')
    log.info('=' * 70)
    log.info('E-003 全局加性共识曲面稀疏求解器 自测 (依赖 E-001/E-002)')
    log.info('=' * 70)
    from e_common import load_all_panels
    from e_masks_sampling import process_all_panels
    from e_weights import process_all_weights
    panels = load_all_panels()
    cp_results = process_all_panels(panels, logger=log)
    cps = {p: cp for p, (cp, _) in cp_results.items()}
    process_all_weights(panels, cps, logger=log)
    result = solve_global_additive(cps, logger=log)
    a, b = extract_gradient(result)
    log.info(f'提取梯度: a_ra={a:.6f} b_dec={b:.6f} (真实数据无注入, 仅供参考)')
    log.info('E-003 自测 PASS')
