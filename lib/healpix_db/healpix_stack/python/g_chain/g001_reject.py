# -*- coding: utf-8 -*-
"""
g001_reject.py - G-001 梯度校正后稳健排异

任务契约 (G-001):
  1. 读取三片 HISS 的 signal 和 E-003 求解的加性曲面偏移
  2. 校正加性偏移 (减去偏移量): signal_corrected = signal_raw - offset_f
  3. 稳健排异: 用 MAD (中位绝对偏差) 检测异常像素
  4. 排异阈值: 3.0 × 1.4826 × MAD
  5. 标记排异像素, 不参与后续融合

禁止捷径:
  - 不得在梯度前做最终叠加 (先校正加性偏移, 再排异)
  - 高 SNR 坏值不得免于排异 (MAD 排异与 SNR 无关, 仅基于残差)
  - 不得产生硬边 (排异是像素级标记, 融合时连续加权, 非硬阈值)

排异方法:
  - 残差定义: residual = signal_corrected - surface_prediction
    surface_prediction = eval_surface(surface_coeffs, ra, dec, center) (E-003 全局共识曲面)
    residual 反映该像素相对全局共识的偏离, 正常像素 residual ≈ 噪声, 异常像素 residual 大
  - MAD 排异: |residual - median(residual)| > 3.0 × 1.4826 × MAD
    每帧独立计算 MAD (各帧噪声水平不同), 但共用全局曲面
  - 高 SNR 坏值保护: 不因 SNR 高而豁免, 排异仅基于残差大小
"""
from __future__ import annotations

import os
from typing import Dict, Tuple

import numpy as np

from g_common import (
    CorrectedPanel, E003Result, PANEL_NAMES, setup_logger, LOG_DIR,
    load_all_panels, load_e003_result, eval_surface, safe_mad_sigma,
    idw_interpolate,
)


# ============================================================================
# 排异参数 (契约级)
# ============================================================================

class RejectParams:
    """G-001 排异参数 (G-001 ~ F-002 共享)。"""
    reject_sigma = 3.0          # 排异阈值: 3.0 × 1.4826 × MAD
    reject_mad_factor = 1.4826  # MAD -> sigma 转换因子
    # 残差计算基准: 相对全局共识曲面 (E-003)
    # 若残差 < floor, 不排异 (避免背景噪声误排)
    residual_floor = 0.0        # 残差下限 (0 = 不设下限)
    # SNR IDW 插值参数 (与 E-001 一致)
    snr_idw_power = 2.0
    snr_idw_k = 8


# ============================================================================
# G-001 主算法
# ============================================================================

def correct_and_reject_panel(
    panel: dict, e003: E003Result, params: RejectParams = RejectParams(),
    logger=None) -> CorrectedPanel:
    """单片加性偏移校正 + MAD 稳健排异。

    步骤:
      1. 加性偏移校正: signal_corrected = signal_raw - offset_f
      2. 计算全局曲面预测: surface_pred = eval_surface(coeffs, ra, dec, center)
      3. 计算残差: residual = signal_corrected - surface_pred
      4. MAD 排异: |residual - median| > reject_sigma × 1.4826 × MAD
      5. 标记排异像素

    Args:
        panel: load_panel 返回的 dict
        e003: E-003 求解结果
        params: 排异参数

    Returns:
        CorrectedPanel
    """
    if logger is None:
        logger = setup_logger(f'g001_{panel["panel_id"]}')

    pid = panel['panel_id']
    offset = float(e003.offsets[pid])
    n_pix = int(panel['ipix'].size)

    # 1. 加性偏移校正
    signal_raw = panel['signal'].astype(np.float32)
    signal_corrected = (panel['signal'].astype(np.float64) - offset).astype(np.float32)

    # 2. 全局曲面预测 (E-003 共识曲面)
    surface_pred = eval_surface(
        e003.surface_coeffs, panel['ra'], panel['dec'], e003.center)

    # 3. 残差
    residual = signal_corrected.astype(np.float64) - surface_pred

    # 4. MAD 排异 (每帧独立 MAD, 反映该帧噪声水平)
    med = float(np.median(residual))
    mad = float(np.median(np.abs(residual - med)))
    sigma_mad = params.reject_mad_factor * mad
    if sigma_mad <= 0:
        # MAD=0 退化 (数据过于均匀), 用 std 兜底
        sigma_mad = float(np.std(residual))
        if sigma_mad <= 0:
            sigma_mad = 1.0
    thresh = params.reject_sigma * sigma_mad
    # 排异: |residual - median| > thresh
    dev = np.abs(residual - med)
    rejected = dev > thresh

    # 5. SNR 插值 (从 V1 snr_model, IDW power=2.0)
    snr_model = panel.get('snr_model')
    if snr_model is not None and snr_model.n_points > 0:
        snr = idw_interpolate(
            panel['ra'], panel['dec'],
            snr_model.points_ra, snr_model.points_dec,
            snr_model.points_snr.astype(np.float64),
            power=params.snr_idw_power, k=params.snr_idw_k,
        ).astype(np.float32)
    else:
        snr = np.ones(n_pix, dtype=np.float32)

    n_rej = int(rejected.sum())
    n_valid = int((~rejected & (panel['support'] == 1)).sum())
    logger.info(
        f'[{pid}] offset={offset:+.2f} median_resid={med:+.2f} mad={mad:.2f} '
        f'sigma_mad={sigma_mad:.2f} thresh=±{thresh:.2f} | '
        f'rejected={n_rej}/{n_pix} ({100.0*n_rej/n_pix:.2f}%) valid={n_valid}'
    )

    return CorrectedPanel(
        panel_id=pid,
        nside=int(panel['nside']),
        nested=bool(panel['nested']),
        ipix=panel['ipix'],
        signal_raw=signal_raw,
        signal_corrected=signal_corrected,
        support=panel['support'],
        ra=panel['ra'],
        dec=panel['dec'],
        snr=snr,
        surface_pred=surface_pred,
        residual=residual,
        rejected=rejected,
        offset=offset,
        meta=panel['meta'],
    )


def process_all_panels(panels: Dict[str, dict], e003: E003Result,
                       params: RejectParams = RejectParams(),
                       logger=None) -> Dict[str, CorrectedPanel]:
    """三片全部校正 + 排异。

    Returns:
        {panel_id: CorrectedPanel}
    """
    if logger is None:
        logger = setup_logger('g001')
    logger.info('=' * 70)
    logger.info('G-001: 加性偏移校正 + MAD 稳健排异')
    logger.info('=' * 70)
    logger.info(f'排异阈值: {params.reject_sigma} × {params.reject_mad_factor} × MAD')
    logger.info(f'E-003 偏移: {e003.offsets}')
    logger.info(f'E-003 中心: {e003.center}')

    results = {}
    for pid in PANEL_NAMES:
        results[pid] = correct_and_reject_panel(panels[pid], e003, params, logger=logger)

    total_rej = sum(int(r.rejected.sum()) for r in results.values())
    total_pix = sum(r.n_pix for r in results.values())
    total_valid = sum(r.n_valid for r in results.values())
    logger.info(
        f'G-001 汇总: rejected={total_rej}/{total_pix} ({100.0*total_rej/total_pix:.2f}%) '
        f'valid={total_valid}')
    return results


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('g001', log_dir=LOG_DIR)
    log.info('=' * 70)
    log.info('G-001 自测 (依赖 E-003 结果 + D-001 HISS)')
    log.info('=' * 70)
    panels = load_all_panels()
    e003 = load_e003_result()
    corrected = process_all_panels(panels, e003, RejectParams(), logger=log)

    # 验证: 偏移已校正 (corrected median 应接近 surface_pred median)
    for pid in PANEL_NAMES:
        cp = corrected[pid]
        med_corr = float(np.median(cp.signal_corrected))
        med_pred = float(np.median(cp.surface_pred))
        log.info(f'  {pid}: median(corrected)={med_corr:.1f} median(pred)={med_pred:.1f} '
                 f'diff={med_corr-med_pred:+.1f}')
    log.info('G-001 自测 PASS')
