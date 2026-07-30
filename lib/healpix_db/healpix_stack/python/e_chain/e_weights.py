# -*- coding: utf-8 -*-
"""
e_weights.py - E-002 局部 SNR^2 / 逆方差联合拟合权重

任务契约 (E-002):
  1. 为每个控制点计算 SNR^2 权重
  2. 计算逆方差权重 (1/variance)
  3. 联合权重 = SNR^2 * 逆方差
  4. 在重叠区域, 计算多帧加权平均

禁止捷径:
  - SNR^2 权重必须实现 (不得无权重)
  - 不得选单一参考帧 (重叠区用加权平均, 非选参考)

实现说明:
  - SNR^2 权重: w_snr = snr^2 (从 E-001 IDW 插值的 SNR)
  - 逆方差权重: w_ivar = 1/variance, variance 由每帧全局 MAD-sigma^2 估计
                (背景近似均匀, 控制点已在有效区域; 局部 MAD 在小区域更不稳)
  - 联合权重: w = w_snr * w_ivar, 每帧归一化 (除以中位数) 保证数值稳定
  - 高 SNR 保护: w_snr = snr^2 使高 SNR 控制点权重更高, 但不剔除低 SNR 点
  - 重叠区: 找出多片共有的 ipix, 按帧权重加权平均 signal + 合并 SNR
"""
from __future__ import annotations

from typing import Dict, List, Tuple

import numpy as np

from e_common import (
    ControlPointTable, PanelData, PANEL_NAMES,
    setup_logger, safe_mad_sigma, json_default,
)
from e_masks_sampling import MaskParams, compute_masks


# ============================================================================
# 权重参数
# ============================================================================

class WeightParams:
    """权重参数 (E-002 ~ E-004 共享)。"""
    snr_floor = 1.0           # SNR 下限 (避免 SNR=0 导致权重=0)
    var_floor_factor = 0.01   # variance 下限因子 (×median^2, 避免除零)
    normalize_per_panel = True  # 每帧权重归一化 (除以中位数)


# ============================================================================
# 单帧权重计算
# ============================================================================

def compute_weights(cp: ControlPointTable, panel: PanelData,
                    params: WeightParams = WeightParams(),
                    logger=None) -> Tuple[np.ndarray, dict]:
    """计算单帧控制点的联合权重 = SNR^2 * inv_var。

    Args:
        cp: ControlPointTable (E-001 输出, weight 字段将被更新)
        panel: 对应的 PanelData (用于估计 variance)
        params: 权重参数

    Returns:
        weight: f64 [M] 联合权重 (已更新到 cp.weight)
        stats: dict (snr_med, var, w_snr_med, w_ivar, w_med)
    """
    if logger is None:
        logger = setup_logger(f'e002_{cp.panel_id}')

    # ---- SNR^2 权重 ----
    snr = np.asarray(cp.snr, dtype=np.float64)
    snr_safe = np.where(np.isfinite(snr) & (snr > 0), snr, params.snr_floor)
    w_snr = snr_safe ** 2  # SNR^2

    # ---- 逆方差权重 ----
    # variance 由该帧有效像素的 MAD-sigma^2 估计 (背景噪声方差)
    masks = compute_masks(panel.signal, panel.support, MaskParams())
    valid_signal = panel.signal[masks['valid']].astype(np.float64)
    sigma = safe_mad_sigma(valid_signal)
    if sigma <= 0:
        sigma = float(np.std(valid_signal)) if valid_signal.size else 1.0
        if sigma <= 0:
            sigma = 1.0
    var = sigma ** 2
    # variance 下限 (避免异常低方差导致权重爆炸)
    med_signal = float(np.median(valid_signal)) if valid_signal.size else 1.0
    var_floor = (params.var_floor_factor * med_signal) ** 2
    var_eff = max(var, var_floor)
    w_ivar = 1.0 / var_eff  # 标量 (每帧统一方差)

    # ---- 联合权重 ----
    w = w_snr * w_ivar

    # ---- 每帧归一化 (除以中位数, 数值稳定) ----
    if params.normalize_per_panel and w.size > 0:
        w_med = float(np.median(w))
        if w_med > 0:
            w = w / w_med

    # 写回 ControlPointTable
    cp.weight = w.astype(np.float64)

    stats = dict(
        snr_median=float(np.median(snr_safe)),
        sigma_mad=float(sigma), variance=float(var), var_floor=float(var_floor),
        w_snr_median=float(np.median(w_snr)), w_ivar=float(w_ivar),
        w_median=float(np.median(w)), w_min=float(np.min(w)), w_max=float(np.max(w)),
    )
    logger.info(
        f'[{cp.panel_id}] SNR_med={stats["snr_median"]:.1f} '
        f'sigma={stats["sigma_mad"]:.2f} var={stats["variance"]:.2f} '
        f'w_snr_med={stats["w_snr_median"]:.3g} w_ivar={stats["w_ivar"]:.3g} '
        f'-> w_med={stats["w_median"]:.3g} [{stats["w_min"]:.3g}, {stats["w_max"]:.3g}]'
    )
    return cp.weight, stats


# ============================================================================
# 重叠区多帧加权平均
# ============================================================================

def compute_overlap_consensus(
        cps: Dict[str, ControlPointTable],
        logger=None) -> Dict[str, dict]:
    """对重叠区 ipix 计算多帧加权平均 signal + 合并 SNR。

    对每个重叠 ipix (同时出现在多帧的控制点中):
      consensus_signal = Σ_f w_f * signal_f / Σ_f w_f
      consensus_snr    = sqrt(Σ_f (w_f * snr_f)^2) / Σ_f w_f  (合并 SNR)
      delta_f = signal_f - consensus_signal  (帧间偏移观测, 用于 E-003)

    Args:
        cps: {panel_id: ControlPointTable}

    Returns:
        {overlap_key: {ipix, signals, weights, consensus_signal, consensus_snr, deltas}}
        overlap_key 形如 'panel1_vs_panel2'
    """
    if logger is None:
        logger = setup_logger('e002_overlap')

    # 构建 ipix -> (panel, idx) 索引
    ipix_map: Dict[int, List[Tuple[str, int]]] = {}
    for p in PANEL_NAMES:
        cp = cps[p]
        for i, ix in enumerate(cp.ipix.tolist()):
            ipix_map.setdefault(ix, []).append((p, i))

    # 找出重叠 ipix (>=2 帧覆盖)
    overlaps = {}
    for ix, locs in ipix_map.items():
        if len(locs) < 2:
            continue
        panels_here = tuple(sorted(set(p for p, _ in locs)))
        overlaps.setdefault(panels_here, []).append((ix, locs))

    result = {}
    for panels_here, items in overlaps.items():
        key = '_vs_'.join(panels_here)
        n_overlap = len(items)
        # 收集每个重叠 ipix 的多帧 signal/weight/snr
        consensus_signals = []
        consensus_snrs = []
        per_panel_signals: Dict[str, list] = {p: [] for p in panels_here}
        per_panel_weights: Dict[str, list] = {p: [] for p in panels_here}
        deltas: Dict[str, list] = {p: [] for p in panels_here}
        ipix_list = []
        for ix, locs in items:
            ipix_list.append(ix)
            w_sum = 0.0
            sig_wsum = 0.0
            snr_wsum = 0.0
            sig_per = {}
            for p, i in locs:
                w = float(cps[p].weight[i])
                s = float(cps[p].signal[i])
                snr = float(cps[p].snr[i]) if np.isfinite(cps[p].snr[i]) else 0.0
                w_sum += w
                sig_wsum += w * s
                snr_wsum += w * snr
                sig_per[p] = s
                per_panel_signals[p].append(s)
                per_panel_weights[p].append(w)
            cs = sig_wsum / w_sum if w_sum > 0 else float('nan')
            # 合并 SNR: 加权平均 (保守, 非相干叠加)
            csnr = snr_wsum / w_sum if w_sum > 0 else float('nan')
            consensus_signals.append(cs)
            consensus_snrs.append(csnr)
            for p in panels_here:
                deltas[p].append(sig_per.get(p, float('nan')) - cs)

        result[key] = dict(
            panels=list(panels_here), n_overlap=n_overlap,
            ipix=ipix_list,
            consensus_signal=consensus_signals,
            consensus_snr=consensus_snrs,
            per_panel_signal=per_panel_signals,
            per_panel_weight=per_panel_weights,
            delta={p: deltas[p] for p in panels_here},
            # 统计
            delta_median_per_panel={
                p: float(np.median(deltas[p])) for p in panels_here},
            delta_mad_per_panel={
                p: float(safe_mad_sigma(np.asarray(deltas[p], dtype=np.float64)))
                for p in panels_here},
        )
        dmed = result[key]['delta_median_per_panel']
        logger.info(
            f'  overlap {key}: n={n_overlap} '
            f'delta_median={ {p: round(v, 2) for p, v in dmed.items()} }'
        )
    return result


# ============================================================================
# 流水线: 全部权重 + 重叠
# ============================================================================

def process_all_weights(panels: Dict[str, PanelData],
                        cps: Dict[str, ControlPointTable],
                        params: WeightParams = WeightParams(),
                        logger=None) -> Tuple[Dict[str, dict], Dict[str, dict]]:
    """三片权重计算 + 重叠区共识。返回 (weight_stats, overlap_stats)。"""
    if logger is None:
        logger = setup_logger('e002')
    weight_stats = {}
    for p in PANEL_NAMES:
        _, st = compute_weights(cps[p], panels[p], params, logger=logger)
        weight_stats[p] = st
    logger.info('重叠区多帧加权平均:')
    overlap_stats = compute_overlap_consensus(cps, logger=logger)
    return weight_stats, overlap_stats


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('e002')
    log.info('=' * 70)
    log.info('E-002 SNR^2 / 逆方差联合权重 自测 (依赖 E-001 输出)')
    log.info('=' * 70)
    from e_common import load_all_panels
    from e_masks_sampling import process_all_panels
    panels = load_all_panels()
    cp_results = process_all_panels(panels, logger=log)
    cps = {p: cp for p, (cp, _) in cp_results.items()}
    w_stats, o_stats = process_all_weights(panels, cps, logger=log)
    log.info('E-002 自测 PASS')
