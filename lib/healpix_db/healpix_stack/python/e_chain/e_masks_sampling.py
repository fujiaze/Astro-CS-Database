# -*- coding: utf-8 -*-
"""
e_masks_sampling.py - E-001 星点/饱和/异常掩膜 + 稀疏控制点采样

任务契约 (E-001):
  1. 读取三片 HISS 的 signal 和 support
  2. 星点掩膜: 检测 signal 中的亮源 (>3σ 或 中位数+N×MAD)
  3. 饱和掩膜: 检测 signal 接近上限的像素
  4. 异常掩膜: 检测 NaN/Inf/负值
  5. 稀疏控制点采样: 在有效区域 (非掩膜) 均匀采样控制点
  6. 记录每个控制点的 (ipix, ra, dec, signal, snr)

禁止捷径:
  - 只采有效覆盖 (support=1)
  - 星点/饱和必须掩膜

实现说明:
  - 星点阈值: median + 5*MAD-sigma (稳健, 避免背景误掩; 5σ 为亮源标准)
  - 饱和阈值: percentile(signal, 99.9) (近上限, 兼顾真实数据无硬饱和)
  - 异常: NaN/Inf/<=0 (信号应为正)
  - 采样: 分层随机 (按 dec 分箱, 每箱等量抽样), 保证空间均匀
  - SNR: 从 V1 snr_model 用 IDW(power=2.0) 插值到采样点
"""
from __future__ import annotations

import os
from typing import Dict, Optional

import numpy as np

from e_common import (
    ControlPointTable, PanelData, PANEL_NAMES, N_SURFACE_COEFFS,
    setup_logger, safe_mad_sigma, idw_interpolate,
)


# ============================================================================
# 掩膜参数 (契约级, E-004 注入测试需复用)
# ============================================================================

class MaskParams:
    """掩膜参数 (E-001 ~ E-004 共享, 保证一致性)。"""
    star_sigma = 5.0          # 星点: median + 5*MAD-sigma
    sat_percentile = 99.9     # 饱和: p99.9
    sat_max_margin = 0.0      # 饱和: signal >= max - margin (此处仅用 percentile)
    target_cp_per_panel = 800 # 每片目标控制点数 (nside=512 ~3928 像素, 采样率 ~20%)
    n_strata = 8              # 分层数 (按 dec 分箱, 保证空间均匀)
    snr_idw_power = 2.0       # SNR IDW 幂次 (与 V1 snr_model.idw_power 一致)
    snr_idw_k = 8             # SNR IDW 最近邻数


# ============================================================================
# 掩膜计算
# ============================================================================

def compute_masks(signal: np.ndarray, support: np.ndarray,
                  params: MaskParams = MaskParams()) -> Dict[str, np.ndarray]:
    """计算三类掩膜 (星点/饱和/异常) + 有效标记。

    Args:
        signal: f32 [N] 信号
        support: u1 [N] 覆盖标记
        params: 掩膜参数

    Returns:
        dict 含:
          mask_star: bool [N] True=星点 (亮源)
          mask_sat: bool [N] True=饱和
          mask_anom: bool [N] True=异常 (NaN/Inf/<=0)
          valid: bool [N] True=有效 (support=1 且非掩膜)
          stats: dict (median, mad_sigma, star_thresh, sat_thresh)
    """
    s = np.asarray(signal, dtype=np.float64)
    n = s.size

    # ---- 异常掩膜: NaN/Inf/<=0 ----
    mask_anom = ~np.isfinite(s) | (s <= 0.0)

    # ---- 在有限正值上估计背景统计 ----
    finite_pos = s[~mask_anom]
    if finite_pos.size == 0:
        # 全异常, 全部无效
        return dict(
            mask_star=np.ones(n, dtype=bool),
            mask_sat=np.ones(n, dtype=bool),
            mask_anom=mask_anom,
            valid=np.zeros(n, dtype=bool),
            stats=dict(median=0.0, mad_sigma=0.0, star_thresh=0.0, sat_thresh=0.0,
                       n_finite_pos=0),
        )
    med = float(np.median(finite_pos))
    sigma_mad = safe_mad_sigma(finite_pos)
    if sigma_mad <= 0:
        # MAD=0 退化 (数据过于均匀), 用 std 兜底
        sigma_mad = float(np.std(finite_pos))
        if sigma_mad <= 0:
            sigma_mad = 1.0  # 极端退化, 避免除零

    # ---- 星点掩膜: median + N*MAD-sigma ----
    star_thresh = med + params.star_sigma * sigma_mad
    mask_star = (~mask_anom) & (s > star_thresh)

    # ---- 饱和掩膜: percentile 99.9 (近上限) ----
    sat_thresh = float(np.percentile(finite_pos, params.sat_percentile))
    # 饱和 = 接近上限的像素 (>= p99.9), 与星点独立 (星点是亮源, 饱和是传感器上限)
    # 实际中星点中心常同时饱和, 两者并集
    mask_sat = (~mask_anom) & (s >= sat_thresh)

    # ---- 有效: support=1 且非掩膜 ----
    sup = np.asarray(support, dtype=np.uint8) == 1
    valid = sup & (~mask_star) & (~mask_sat) & (~mask_anom)

    stats = dict(
        median=med, mad_sigma=sigma_mad,
        star_thresh=star_thresh, sat_thresh=sat_thresh,
        n_finite_pos=int(finite_pos.size),
        n_star=int(mask_star.sum()), n_sat=int(mask_sat.sum()),
        n_anom=int(mask_anom.sum()), n_valid=int(valid.sum()),
    )
    return dict(mask_star=mask_star, mask_sat=mask_sat, mask_anom=mask_anom,
                valid=valid, stats=stats)


# ============================================================================
# 稀疏控制点采样 (分层随机, 按 dec 分箱)
# ============================================================================

def sample_control_points(panel: PanelData, masks: Dict[str, np.ndarray],
                          params: MaskParams = MaskParams(),
                          snr_model: Optional[object] = None,
                          rng: Optional[np.random.Generator] = None
                          ) -> ControlPointTable:
    """在有效区域分层随机采样控制点, 并用 IDW 插值 SNR。

    Args:
        panel: PanelData (全像素)
        masks: compute_masks 返回的 dict
        params: 采样参数
        snr_model: V1 HissV2SnrModel (None 则 SNR=NaN)
        rng: numpy 随机数生成器 (可复现)

    Returns:
        ControlPointTable (weight 默认 1.0, E-002 填充)
    """
    if rng is None:
        rng = np.random.default_rng(20260730)

    valid_idx = np.where(masks['valid'])[0]
    n_valid = valid_idx.size
    if n_valid == 0:
        raise ValueError(f'[{panel.panel_id}] 无有效像素, 无法采样')

    target = min(params.target_cp_per_panel, n_valid)

    # 分层随机: 按 dec 分箱, 每箱按比例分配配额
    dec_valid = panel.dec[valid_idx]
    dec_min, dec_max = float(dec_valid.min()), float(dec_valid.max())
    if dec_max <= dec_min:
        # 单层 (dec 恒定)
        strata = np.zeros(n_valid, dtype=np.int64)
    else:
        # 等宽分箱
        edges = np.linspace(dec_min, dec_max + 1e-9, params.n_strata + 1)
        strata = np.clip(np.searchsorted(edges[1:-1], dec_valid, side='right'),
                         0, params.n_strata - 1)

    chosen = []
    for s in range(params.n_strata):
        in_s = np.where(strata == s)[0]
        if in_s.size == 0:
            continue
        # 按比例分配配额 (向上取整, 至少 1)
        quota = max(1, int(round(target * in_s.size / n_valid)))
        quota = min(quota, in_s.size)
        pick = rng.choice(in_s, size=quota, replace=False)
        chosen.append(pick)
    chosen = np.concatenate(chosen) if chosen else np.array([], dtype=np.int64)
    # 若超采 (向上取整累积), 随机裁剪到 target
    if chosen.size > target:
        chosen = rng.choice(chosen, size=target, replace=False)
    chosen = np.sort(chosen)

    # 映射回原像素索引
    sel = valid_idx[chosen]
    ipix = panel.ipix[sel].astype(np.uint64)
    ra = panel.ra[sel].astype(np.float64)
    dec = panel.dec[sel].astype(np.float64)
    signal = panel.signal[sel].astype(np.float32)
    n_cp = ipix.size

    # SNR IDW 插值
    if snr_model is not None and snr_model.n_points > 0:
        snr = idw_interpolate(
            ra, dec,
            np.asarray(snr_model.points_ra, dtype=np.float64),
            np.asarray(snr_model.points_dec, dtype=np.float64),
            np.asarray(snr_model.points_snr, dtype=np.float64),
            power=params.snr_idw_power, k=params.snr_idw_k,
        ).astype(np.float32)
    else:
        snr = np.full(n_cp, np.nan, dtype=np.float32)

    # 掩膜标记 (采样点全部有效, 但保留掩膜字段为 False 以保持结构一致)
    mask_star = np.zeros(n_cp, dtype=bool)
    mask_sat = np.zeros(n_cp, dtype=bool)
    mask_anom = np.zeros(n_cp, dtype=bool)
    valid = np.ones(n_cp, dtype=bool)
    weight = np.ones(n_cp, dtype=np.float64)  # E-002 填充

    return ControlPointTable(
        panel_id=panel.panel_id, ipix=ipix, ra=ra, dec=dec,
        signal=signal, snr=snr, weight=weight, valid=valid,
        mask_star=mask_star, mask_sat=mask_sat, mask_anom=mask_anom,
    )


# ============================================================================
# 流水线: 单帧掩膜+采样
# ============================================================================

def process_panel(panel: PanelData, params: MaskParams = MaskParams(),
                  rng: Optional[np.random.Generator] = None,
                  logger=None) -> tuple:
    """单帧: 掩膜 + 采样 + SNR 插值。返回 (ControlPointTable, mask_stats)。"""
    if logger is None:
        logger = setup_logger(f'e001_{panel.panel_id}')
    masks = compute_masks(panel.signal, panel.support, params)
    st = masks['stats']
    logger.info(
        f'[{panel.panel_id}] n_pix={panel.n_pix} '
        f'median={st["median"]:.2f} mad_sigma={st["mad_sigma"]:.2f} '
        f'star_thresh={st["star_thresh"]:.2f} sat_thresh={st["sat_thresh"]:.2f} | '
        f'star={st["n_star"]} sat={st["n_sat"]} anom={st["n_anom"]} '
        f'valid={st["n_valid"]} ({100*st["n_valid"]/panel.n_pix:.1f}%)'
    )
    cp = sample_control_points(panel, masks, params,
                               snr_model=panel.snr_model, rng=rng)
    # SNR 统计
    snr_finite = cp.snr[np.isfinite(cp.snr)]
    snr_med = float(np.median(snr_finite)) if snr_finite.size else float('nan')
    logger.info(
        f'[{panel.panel_id}] sampled {cp.n_points} ctrl pts '
        f'(SNR median={snr_med:.1f}, finite={snr_finite.size}/{cp.n_points})'
    )
    return cp, st


def process_all_panels(panels: Dict[str, PanelData],
                       params: MaskParams = MaskParams(),
                       seed: int = 20260730,
                       logger=None) -> Dict[str, tuple]:
    """三片掩膜+采样。返回 {panel_id: (ControlPointTable, mask_stats)}。"""
    if logger is None:
        logger = setup_logger('e001')
    rng = np.random.default_rng(seed)
    out = {}
    for p in PANEL_NAMES:
        out[p] = process_panel(panels[p], params, rng=rng, logger=logger)
    return out


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('e001')
    log.info('=' * 70)
    log.info('E-001 掩膜 + 稀疏控制点采样 自测 (真实 D-001 数据)')
    log.info('=' * 70)
    from e_common import load_all_panels
    panels = load_all_panels()
    results = process_all_panels(panels, logger=log)
    total_valid = 0
    total_cp = 0
    for p, (cp, st) in results.items():
        total_valid += st['n_valid']
        total_cp += cp.n_points
        log.info(f'  {p}: valid={st["n_valid"]} cp={cp.n_points} '
                 f'ra=[{cp.ra.min():.3f},{cp.ra.max():.3f}] '
                 f'dec=[{cp.dec.min():.3f},{cp.dec.max():.3f}]')
    log.info(f'TOTAL: valid={total_valid} ctrl_pts={total_cp}')
    log.info('E-001 自测 PASS')
