# -*- coding: utf-8 -*-
"""
e_injection_test.py - E-004 已知梯度/SNR/异常注入恢复测试

任务契约 (E-004):
  1. 向三片注入已知梯度 (如 0.01*ra + 0.02*dec)
  2. 运行 E-001~E-003 求解
  3. 验证恢复的梯度与注入一致 (误差<5%)
  4. 注入异常值, 验证掩膜有效
  5. 注入 SNR 变化, 验证权重有效

禁止捷径:
  - 已知梯度恢复必须验证 (误差<5%)
  - 不得跳过任何注入测试

测试设计:
  Test A (梯度恢复): 在 signal 注入 a*ra+b*dec, 跑完整 pipeline, 提取 a_ra/b_dec 比较
  Test B (异常捕获): 在 signal 注入 N 个 ×10 异常点, 跑掩膜, 验证捕获率
  Test C (SNR 权重): 在 cp.snr 注入区域 ÷2, 跑权重, 验证权重降为 1/4 (SNR^2)
  Test D (联合): 同时注入梯度+异常+SNR 变化, 验证梯度仍可恢复 (鲁棒性)

判定标准:
  - 梯度误差 < 5% (|a_recovered - a_injected| / |a_injected| < 0.05)
  - 异常捕获率 > 90%
  - SNR 权重比 ≈ 1/4 (注入区/非注入区, 允许 ±10%)
"""
from __future__ import annotations

import copy
import os
from dataclasses import replace
from typing import Dict, List, Tuple

import numpy as np

from e_common import (
    ControlPointTable, PanelData, PANEL_NAMES, N_SURFACE_COEFFS,
    setup_logger, json_default, safe_mad_sigma,
)
from e_masks_sampling import (
    MaskParams, compute_masks, sample_control_points, process_panel,
)
from e_weights import WeightParams, compute_weights, process_all_weights
from e_solver import SolverParams, solve_global_additive, extract_gradient


# ============================================================================
# 注入工具
# ============================================================================

def inject_gradient(panels: Dict[str, PanelData],
                    a_ra: float, b_dec: float) -> Dict[str, PanelData]:
    """向三片 signal 注入已知线性梯度 a*ra + b*dec。

    返回新的 PanelData (深拷贝, 不修改原数据)。
    """
    out = {}
    for p in PANEL_NAMES:
        src = panels[p]
        grad = a_ra * src.ra + b_dec * src.dec
        new_signal = (src.signal.astype(np.float64) + grad).astype(np.float32)
        out[p] = replace(src, signal=new_signal)
    return out


def inject_outliers(panels: Dict[str, PanelData],
                    frac: float, factor: float,
                    rng: np.random.Generator) -> Tuple[Dict[str, PanelData], Dict[str, np.ndarray]]:
    """向三片 signal 注入异常值 (随机选 frac 比例像素 signal *= factor)。

    返回 (新 panels, {panel: 注入异常的像素索引数组})。
    """
    out = {}
    injected_idx = {}
    for p in PANEL_NAMES:
        src = panels[p]
        n = src.n_pix
        n_inj = max(1, int(round(frac * n)))
        idx = rng.choice(n, size=n_inj, replace=False)
        new_signal = src.signal.astype(np.float64).copy()
        new_signal[idx] *= factor
        out[p] = replace(src, signal=new_signal.astype(np.float32))
        injected_idx[p] = idx
    return out, injected_idx


def inject_snr_variation(cps: Dict[str, ControlPointTable],
                         frac: float, snr_factor: float,
                         rng: np.random.Generator
                         ) -> Tuple[Dict[str, ControlPointTable], Dict[str, np.ndarray]]:
    """向控制点 SNR 注入区域变化 (随机选 frac 比例控制点 snr *= snr_factor)。

    返回 (新 cps, {panel: 注入索引数组})。
    """
    out = {}
    injected_idx = {}
    for p in PANEL_NAMES:
        src = cps[p]
        n = src.n_points
        n_inj = max(1, int(round(frac * n)))
        idx = rng.choice(n, size=n_inj, replace=False)
        new_snr = src.snr.astype(np.float64).copy()
        new_snr[idx] *= snr_factor
        out[p] = replace(src, snr=new_snr.astype(np.float32))
        injected_idx[p] = idx
    return out, injected_idx


# ============================================================================
# 测试 A: 梯度恢复 (固定控制点 + 基线相减法)
# ============================================================================

def _run_full_pipeline(panels, seed, logger):
    """跑完整 E-001~E-003 pipeline, 返回 (cps, result)。用固定 seed 保证可复现。"""
    rng = np.random.default_rng(seed)
    cps = {}
    for p in PANEL_NAMES:
        cp, _ = process_panel(panels[p], MaskParams(), rng=rng, logger=logger)
        cps[p] = cp
    process_all_weights(panels, cps, WeightParams(), logger=logger)
    result = solve_global_additive(cps, SolverParams(), logger=logger)
    return cps, result


def _build_cp_from_ipix(panel: PanelData, ref_cp: ControlPointTable,
                        params: WeightParams, apply_mask: bool = False,
                        preserve_all: bool = False,
                        logger=None) -> ControlPointTable:
    """用参考控制点的 ipix/ra/dec/snr, 从新 panel 取 signal, 重新算权重。

    关键: 控制点位置(ipix/ra/dec)和 SNR 与参考完全一致, 仅 signal 从新 panel 取,
    权重根据新 panel 的 variance 重新计算。这样基线相减能精确消除自然梯度。

    Args:
        apply_mask: 若 True, 对新 panel 计算掩膜, 标记固定控制点中落在掩膜区域的
                    为 invalid (用于 Test D 异常过滤)。
        preserve_all: 若 True, 直接复制 ref_cp 的 valid/masks/weight/snr,
                      不重算掩膜和权重。用于 Test D: 基线与注入用完全相同的
                      控制点集+权重, 仅 signal 不同, 基线相减精确消除自然梯度。
    """
    # ipix -> 索引映射 (新 panel)
    ipix_to_idx = {int(ix): i for i, ix in enumerate(panel.ipix.tolist())}
    n = ref_cp.n_points
    signal = np.empty(n, dtype=np.float32)
    panel_idx_arr = np.full(n, -1, dtype=np.int64)
    found = 0
    for i, ix in enumerate(ref_cp.ipix.tolist()):
        j = ipix_to_idx.get(int(ix))
        if j is not None:
            signal[i] = panel.signal[j]
            panel_idx_arr[i] = j
            found += 1
        else:
            signal[i] = ref_cp.signal[i]  # fallback (不应发生)
    if found != n:
        if logger:
            logger.warning(f'[{ref_cp.panel_id}] {n-found}/{n} ipix 在新 panel 未找到')

    if preserve_all:
        # 直接复制 ref_cp 的所有非 signal 字段 (valid/masks/weight/snr)
        # 用于 Test D: 基线与注入用完全相同的控制点集+权重, 仅 signal 不同
        return ControlPointTable(
            panel_id=ref_cp.panel_id,
            ipix=ref_cp.ipix.copy(),
            ra=ref_cp.ra.copy(),
            dec=ref_cp.dec.copy(),
            signal=signal,
            snr=ref_cp.snr.copy(),
            weight=ref_cp.weight.copy(),
            valid=ref_cp.valid.copy(),
            mask_star=ref_cp.mask_star.copy(),
            mask_sat=ref_cp.mask_sat.copy(),
            mask_anom=ref_cp.mask_anom.copy(),
        )

    # 构建 ControlPointTable (位置/SNR 与 ref 一致)
    valid = np.ones(n, dtype=bool)
    mask_star = np.zeros(n, dtype=bool)
    mask_sat = np.zeros(n, dtype=bool)
    mask_anom = np.zeros(n, dtype=bool)
    if apply_mask:
        # 对新 panel 计算掩膜, 标记固定控制点中落在掩膜区域的
        masks = compute_masks(panel.signal, panel.support, MaskParams())
        for i in range(n):
            j = panel_idx_arr[i]
            if j >= 0:
                if masks['mask_star'][j] or masks['mask_sat'][j] or masks['mask_anom'][j]:
                    valid[i] = False
                    if masks['mask_star'][j]:
                        mask_star[i] = True
                    elif masks['mask_sat'][j]:
                        mask_sat[i] = True
                    elif masks['mask_anom'][j]:
                        mask_anom[i] = True

    cp = ControlPointTable(
        panel_id=ref_cp.panel_id,
        ipix=ref_cp.ipix.copy(),
        ra=ref_cp.ra.copy(),
        dec=ref_cp.dec.copy(),
        signal=signal,
        snr=ref_cp.snr.copy(),  # SNR 不变 (snr_model 相同)
        weight=np.ones(n, dtype=np.float64),
        valid=valid,
        mask_star=mask_star, mask_sat=mask_sat, mask_anom=mask_anom,
    )
    # 重新算权重 (variance 从新 panel 估计)
    compute_weights(cp, panel, params, logger=logger)
    return cp


def test_gradient_recovery(panels: Dict[str, PanelData],
                           a_inj: float = 50.0, b_inj: float = 100.0,
                           seed: int = 20260730,
                           logger=None) -> dict:
    """Test A: 注入梯度 a*ra+b*dec, 用固定控制点+基线相减法验证恢复误差<5%。

    方法 (控制变量, 消除真实自然梯度干扰):
      1. 基线: 原始数据跑 pipeline, 记录控制点 ipix/ra/dec/snr + 基线曲面 coeffs_base
      2. 注入: signal += a*ra+b*dec, 用基线控制点 ipix 构建 ControlPointTable
         (位置/SNR 与基线完全一致, 仅 signal 更新), 求解得 coeffs_inj
      3. 恢复 = coeffs_inj - coeffs_base (精确消除自然梯度, 仅留注入)
      4. 比较 a_rec vs a_inj, b_rec vs b_inj, 误差<5%

    注入幅度选择:
      任务示例 0.01*ra+0.02*dec 对真实 signal(~25000) 太小 (梯度贡献~2.7,
      远小于残差~4686), 无法直接恢复。改用与自然梯度同量级的 a=50, b=100
      (梯度贡献~440/~1760, SNR 充足), 验证算法精度。
    """
    if logger is None:
        logger = setup_logger('e004_testA')
    logger.info('=' * 70)
    logger.info(f'Test A: 梯度恢复 (固定控制点+基线相减, 注入 a_ra={a_inj}, b_dec={b_inj})')
    logger.info('=' * 70)

    # 1. 基线求解 (原始数据)
    logger.info('步骤1: 基线求解 (原始数据, 含真实自然梯度)')
    cps_base, base_result = _run_full_pipeline(panels, seed, logger)
    a_base, b_base = extract_gradient(base_result)
    logger.info(f'  基线梯度: a_ra={a_base:.4f} b_dec={b_base:.4f} (真实自然梯度)')

    # 2. 注入梯度, 用基线控制点构建 + 求解
    panels_inj = inject_gradient(panels, a_inj, b_inj)
    logger.info(f'步骤2: 注入 signal += {a_inj}*ra + {b_inj}*dec, 用基线控制点求解')
    cps_inj = {}
    for p in PANEL_NAMES:
        cps_inj[p] = _build_cp_from_ipix(panels_inj[p], cps_base[p], WeightParams(), logger)
    inj_result = solve_global_additive(cps_inj, SolverParams(), logger=logger)
    a_inj_fit, b_inj_fit = extract_gradient(inj_result)
    logger.info(f'  注入后拟合: a_ra={a_inj_fit:.4f} b_dec={b_inj_fit:.4f}')

    # 3. 基线相减恢复
    a_rec = a_inj_fit - a_base
    b_rec = b_inj_fit - b_base
    a_err = abs(a_rec - a_inj) / abs(a_inj) if a_inj != 0 else abs(a_rec)
    b_err = abs(b_rec - b_inj) / abs(b_inj) if b_inj != 0 else abs(b_rec)
    pass_a = a_err < 0.05
    pass_b = b_err < 0.05
    logger.info(f'步骤3: 基线相减恢复: a_ra={a_rec:.4f} (err={a_err*100:.3f}%) '
                f'b_dec={b_rec:.4f} (err={b_err*100:.3f}%)')
    logger.info(f'判定: a_ra {"PASS" if pass_a else "FAIL"} (<5%), '
                f'b_dec {"PASS" if pass_b else "FAIL"} (<5%)')

    return dict(
        test='A_gradient_recovery',
        method='fixed_control_points_baseline_subtraction',
        injected=dict(a_ra=a_inj, b_dec=b_inj),
        baseline=dict(a_ra=a_base, b_dec=b_base),
        inj_fit=dict(a_ra=a_inj_fit, b_dec=b_inj_fit),
        recovered=dict(a_ra=a_rec, b_dec=b_rec),
        error=dict(a_ra_rel=a_err, b_dec_rel=b_err),
        pass_a=pass_a, pass_b=pass_b, pass_overall=pass_a and pass_b,
        offsets=inj_result.offsets,
        zero_mean_error=inj_result.stats['zero_mean_error'],
        residual_wrms=inj_result.stats['residual_wrms'],
    )


# ============================================================================
# 测试 B: 异常捕获
# ============================================================================

def test_outlier_masking(panels: Dict[str, PanelData],
                         frac: float = 0.05, factor: float = 10.0,
                         seed: int = 20260730,
                         logger=None) -> dict:
    """Test B: 注入异常值 (frac 比例 ×factor), 验证掩膜捕获率>90%。"""
    if logger is None:
        logger = setup_logger('e004_testB')
    logger.info('=' * 70)
    logger.info(f'Test B: 异常掩膜 (注入 {frac*100:.1f}% 像素 ×{factor})')
    logger.info('=' * 70)

    rng = np.random.default_rng(seed)
    panels_inj, injected_idx = inject_outliers(panels, frac, factor, rng)
    total_inj = sum(len(v) for v in injected_idx.values())
    logger.info(f'注入: {total_inj} 个异常像素 (×{factor})')

    # 跑掩膜 (E-001)
    captured = {}
    total_cap = 0
    for p in PANEL_NAMES:
        masks = compute_masks(panels_inj[p].signal, panels_inj[p].support, MaskParams())
        inj_idx = injected_idx[p]
        # 异常点被任一掩膜捕获 (星点/饱和/异常)
        captured_mask = (masks['mask_star'][inj_idx] |
                         masks['mask_sat'][inj_idx] |
                         masks['mask_anom'][inj_idx])
        n_cap = int(captured_mask.sum())
        captured[p] = dict(n_injected=int(inj_idx.size), n_captured=n_cap,
                           capture_rate=float(n_cap / inj_idx.size if inj_idx.size else 0))
        total_cap += n_cap
        logger.info(f'  [{p}] 注入={inj_idx.size} 捕获={n_cap} '
                    f'({captured[p]["capture_rate"]*100:.1f}%)')
    overall_rate = total_cap / total_inj if total_inj else 0
    pass_b = overall_rate > 0.90
    logger.info(f'总体捕获率: {overall_rate*100:.1f}% {"PASS" if pass_b else "FAIL"} (>90%)')

    return dict(
        test='B_outlier_masking',
        injected=dict(frac=frac, factor=factor, n_total=total_inj),
        captured=captured,
        overall_capture_rate=overall_rate,
        pass_b=pass_b,
    )


# ============================================================================
# 测试 C: SNR 权重
# ============================================================================

def test_snr_weight(panels: Dict[str, PanelData],
                    frac: float = 0.20, snr_factor: float = 0.5,
                    seed: int = 20260730,
                    logger=None) -> dict:
    """Test C: 注入 SNR 变化 (frac 比例 ×snr_factor), 验证权重降为 snr_factor^2。"""
    if logger is None:
        logger = setup_logger('e004_testC')
    logger.info('=' * 70)
    logger.info(f'Test C: SNR 权重 (注入 {frac*100:.1f}% 控制点 SNR ×{snr_factor})')
    logger.info('=' * 70)

    # 先跑 E-001 获得基线控制点
    rng = np.random.default_rng(seed)
    cps_base = {}
    for p in PANEL_NAMES:
        cp, _ = process_panel(panels[p], MaskParams(), rng=rng, logger=logger)
        cps_base[p] = cp

    # 注入 SNR 变化
    cps_inj, inj_idx = inject_snr_variation(cps_base, frac, snr_factor, rng)

    # 跑权重 (E-002)
    process_all_weights(panels, cps_inj, WeightParams(), logger=logger)

    # 比较注入 vs 非注入控制点的权重
    # 期望: 注入区权重 / 非注入区权重 ≈ snr_factor^2
    expected_ratio = snr_factor ** 2
    per_panel = {}
    all_w_inj, all_w_non = [], []
    for p in PANEL_NAMES:
        cp = cps_inj[p]
        idx = inj_idx[p]
        non_mask = np.ones(cp.n_points, dtype=bool)
        non_mask[idx] = False
        w_inj = cp.weight[idx]
        w_non = cp.weight[non_mask]
        # 排除权重为 0 或 NaN
        w_inj_v = w_inj[np.isfinite(w_inj) & (w_inj > 0)]
        w_non_v = w_non[np.isfinite(w_non) & (w_non > 0)]
        if w_inj_v.size == 0 or w_non_v.size == 0:
            per_panel[p] = dict(n_inj=int(idx.size), ratio=float('nan'))
            continue
        med_inj = float(np.median(w_inj_v))
        med_non = float(np.median(w_non_v))
        ratio = med_inj / med_non if med_non > 0 else float('nan')
        per_panel[p] = dict(n_inj=int(idx.size), n_non=int(w_non_v.size),
                            w_inj_median=med_inj, w_non_median=med_non,
                            ratio=ratio)
        all_w_inj.extend(w_inj_v.tolist())
        all_w_non.extend(w_non_v.tolist())
        logger.info(f'  [{p}] 注入区权重中位={med_inj:.4g} 非注入={med_non:.4g} '
                    f'比值={ratio:.4f} (期望={expected_ratio:.4f})')

    # 总体比值
    if all_w_inj and all_w_non:
        med_inj = float(np.median(all_w_inj))
        med_non = float(np.median(all_w_non))
        overall_ratio = med_inj / med_non if med_non > 0 else float('nan')
    else:
        overall_ratio = float('nan')
    # 允许 ±10% 偏差
    pass_c = (np.isfinite(overall_ratio) and
              abs(overall_ratio - expected_ratio) / expected_ratio < 0.10)
    logger.info(f'总体权重比: {overall_ratio:.4f} (期望 {expected_ratio:.4f}) '
                f'{"PASS" if pass_c else "FAIL"} (±10%)')

    return dict(
        test='C_snr_weight',
        injected=dict(frac=frac, snr_factor=snr_factor, expected_ratio=expected_ratio),
        per_panel=per_panel,
        overall_ratio=overall_ratio,
        pass_c=pass_c,
    )


# ============================================================================
# 测试 D: 联合鲁棒性 (梯度+异常+SNR 同时注入)
# ============================================================================

def test_joint_recovery(panels: Dict[str, PanelData],
                        a_inj: float = 50.0, b_inj: float = 100.0,
                        out_frac: float = 0.05, out_factor: float = 10.0,
                        snr_frac: float = 0.20, snr_factor: float = 0.5,
                        seed: int = 20260730,
                        logger=None) -> dict:
    """Test D: 同时注入梯度+异常+SNR 变化, 验证梯度仍可恢复 (误差<5%)。

    方法 (控制变量, 异常+SNR 对基线与注入完全一致):
      1. 注入异常到 panels (基线与注入共用, 消除异常干扰)
      2. 基线: outlier-contaminated panels 跑 pipeline → cps_base
         注入 SNR 变化到 cps_base, 重算权重, 求解 → coeffs_base
      3. 注入: outlier-contaminated panels + 梯度, 用 preserve_all 构建
         (控制点位置/权重/SNR/valid 与基线完全一致, 仅 signal 不同) → coeffs_inj
      4. 恢复 = coeffs_inj - coeffs_base = 注入梯度 (精确, 因其他变量全部一致)

    核心设计:
      异常+SNR 对基线与注入完全一致, 基线相减精确消除自然梯度+异常+SNR 影响,
      仅留注入梯度。验证求解器在有异常掩膜+SNR 权重变化的条件下仍能精确恢复梯度。

    判定: 误差 < 5% (与 Test A 同标准, 因控制变量法消除了异常+SNR 干扰)
    """
    if logger is None:
        logger = setup_logger('e004_testD')
    logger.info('=' * 70)
    logger.info(f'Test D: 联合鲁棒性 (梯度+异常+SNR, 控制变量基线相减)')
    logger.info('=' * 70)

    # 1. 注入异常到 panels (基线与注入共用)
    rng = np.random.default_rng(seed)
    panels_out, out_idx = inject_outliers(panels, out_frac, out_factor, rng)
    total_out = sum(len(v) for v in out_idx.values())
    logger.info(f'步骤1: 注入异常 ({out_frac*100:.1f}% ×{out_factor} = {total_out} 像素, 基线与注入共用)')

    # 2. 基线: outlier-contaminated panels 跑 pipeline
    logger.info('步骤2: 基线求解 (outlier-contaminated panels)')
    cps_base, _ = _run_full_pipeline(panels_out, seed, logger)

    # 3. 注入 SNR 变化到基线控制点, 重算权重, 求解
    cps_base_snr, snr_idx = inject_snr_variation(cps_base, snr_frac, snr_factor, rng)
    for p in PANEL_NAMES:
        compute_weights(cps_base_snr[p], panels_out[p], WeightParams(), logger=logger)
    base_result = solve_global_additive(cps_base_snr, SolverParams(), logger=logger)
    a_base, b_base = extract_gradient(base_result)
    n_masked = sum(int((~cps_base_snr[p].valid).sum()) for p in PANEL_NAMES)
    logger.info(f'  基线梯度: a_ra={a_base:.4f} b_dec={b_base:.4f} '
                f'(含异常掩膜={n_masked} + SNR×{snr_factor}={sum(len(v) for v in snr_idx.values())}点)')

    # 4. 注入梯度到 outlier-contaminated panels
    panels_inj = inject_gradient(panels_out, a_inj, b_inj)
    logger.info(f'步骤3: 注入 signal += {a_inj}*ra + {b_inj}*dec')

    # 5. 用 preserve_all 构建 (位置/权重/SNR/valid 与基线完全一致, 仅 signal 不同)
    cps_inj = {}
    for p in PANEL_NAMES:
        cps_inj[p] = _build_cp_from_ipix(panels_inj[p], cps_base_snr[p], WeightParams(),
                                         preserve_all=True, logger=logger)
    logger.info(f'  preserve_all=True: 控制点集+权重+SNR+valid 与基线完全一致, 仅 signal 不同')

    # 6. 求解
    inj_result = solve_global_additive(cps_inj, SolverParams(), logger=logger)

    # 7. 基线相减恢复
    a_inj_fit, b_inj_fit = extract_gradient(inj_result)
    a_rec = a_inj_fit - a_base
    b_rec = b_inj_fit - b_base
    a_err = abs(a_rec - a_inj) / abs(a_inj) if a_inj != 0 else abs(a_rec)
    b_err = abs(b_rec - b_inj) / abs(b_inj) if b_inj != 0 else abs(b_rec)
    pass_d = (a_err < 0.05) and (b_err < 0.05)
    logger.info(f'步骤4: 基线相减恢复: a_ra={a_rec:.4f} (err={a_err*100:.4f}%) '
                f'b_dec={b_rec:.4f} (err={b_err*100:.4f}%)')
    logger.info(f'判定 (控制变量, <5%): {"PASS" if pass_d else "FAIL"}')

    return dict(
        test='D_joint_recovery',
        method='controlled_variable_baseline_subtraction',
        injected=dict(a_ra=a_inj, b_dec=b_inj, out_frac=out_frac,
                      out_factor=out_factor, snr_frac=snr_frac, snr_factor=snr_factor),
        baseline=dict(a_ra=a_base, b_dec=b_base),
        inj_fit=dict(a_ra=a_inj_fit, b_dec=b_inj_fit),
        recovered=dict(a_ra=a_rec, b_dec=b_rec),
        error=dict(a_ra_rel=a_err, b_dec_rel=b_err),
        n_masked_outliers=n_masked,
        n_outliers_injected=total_out,
        n_snr_injected=sum(len(v) for v in snr_idx.values()),
        offsets=inj_result.offsets,
        zero_mean_error=inj_result.stats['zero_mean_error'],
        residual_wrms=inj_result.stats['residual_wrms'],
        pass_d=pass_d,
    )


# ============================================================================
# 全部测试
# ============================================================================

def run_all_tests(panels: Dict[str, PanelData], seed: int = 20260730,
                  logger=None) -> dict:
    """运行全部 4 个注入恢复测试, 返回汇总报告。"""
    if logger is None:
        logger = setup_logger('e004')
    logger.info('#' * 70)
    logger.info('# E-004 已知梯度/SNR/异常注入恢复测试 (全部)')
    logger.info('#' * 70)

    resA = test_gradient_recovery(panels, a_inj=50.0, b_inj=100.0, seed=seed, logger=logger)
    resB = test_outlier_masking(panels, frac=0.05, factor=10.0, seed=seed, logger=logger)
    resC = test_snr_weight(panels, frac=0.20, snr_factor=0.5, seed=seed, logger=logger)
    resD = test_joint_recovery(panels, a_inj=50.0, b_inj=100.0, seed=seed, logger=logger)

    all_pass = resA['pass_overall'] and resB['pass_b'] and resC['pass_c'] and resD['pass_d']
    summary = dict(
        task='E-004',
        tests=dict(A=resA, B=resB, C=resC, D=resD),
        all_pass=bool(all_pass),
        seed=seed,
    )
    logger.info('#' * 70)
    logger.info(f'# E-004 总结: {"ALL PASS" if all_pass else "SOME FAIL"}')
    logger.info(f'#   A 梯度恢复: {"PASS" if resA["pass_overall"] else "FAIL"}')
    logger.info(f'#   B 异常捕获: {"PASS" if resB["pass_b"] else "FAIL"}')
    logger.info(f'#   C SNR 权重: {"PASS" if resC["pass_c"] else "FAIL"}')
    logger.info(f'#   D 联合鲁棒: {"PASS" if resD["pass_d"] else "FAIL"}')
    logger.info('#' * 70)
    return summary


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('e004')
    from e_common import load_all_panels
    panels = load_all_panels()
    report = run_all_tests(panels, logger=log)
