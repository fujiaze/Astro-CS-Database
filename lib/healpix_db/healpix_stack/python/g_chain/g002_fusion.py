# -*- coding: utf-8 -*-
"""
g002_fusion.py - G-002 独立 SNR^2 连续加权融合

任务契约 (G-002):
  1. 对每个球面像素, 计算 SNR^2 加权平均
  2. 权重 = SNR^2 × support (只参与有覆盖的帧)
  3. 连续加权 (非硬阈值)
  4. 输出融合后的 signal 和 weight map

禁止捷径:
  - 不得在梯度前做最终叠加 (已由 G-001 校正加性偏移)
  - 高 SNR 坏值不得免于排异 (G-001 已排异, 此处只融合非排异像素)
  - 不得产生硬边 (连续加权, 非硬阈值, 非重叠区也单帧直通)

融合方法:
  - 按 ipix 聚合: 重叠区多帧加权平均, 非重叠区单帧直通
  - 权重: w_i = SNR_i^2 × support_i  (support=1 时有权重, 0 时无)
  - 融合: signal_fused = Σ(w_i × signal_corrected_i) / Σ(w_i)
  - 总权重: weight_map = Σ(w_i)
  - 贡献帧数: support_count = Σ(support_i & ~rejected_i)
  - 有效 SNR: snr_eff = sqrt(Σ(w_i × snr_i^2) / Σ(w_i)) (加权合并, 保持 SNR 量纲)
  - 连续加权: 无硬阈值, 即使单帧也用其 SNR^2 加权 (单帧时权重归一, 结果=该帧信号)
"""
from __future__ import annotations

from typing import Dict

import numpy as np

from g_common import (
    CorrectedPanel, FusionResult, PANEL_NAMES, setup_logger, LOG_DIR,
)


# ============================================================================
# 融合参数
# ============================================================================

class FusionParams:
    """G-002 融合参数。"""
    snr_floor = 1.0           # SNR 下限 (避免 SNR=0 导致权重=0)
    weight_floor = 1e-12      # 权重下限 (避免除零)
    # SNR^2 权重: w = SNR^2 (连续加权, 非硬阈值)
    # support 因子: support=1 时有权重, 0 时无 (此处 CorrectedPanel 已只含 support=1 像素)
    use_snr_squared = True    # 权重 = SNR^2 (True) 或 SNR (False)


# ============================================================================
# G-002 主算法
# ============================================================================

def fuse_panels(corrected: Dict[str, CorrectedPanel],
                params: FusionParams = FusionParams(),
                logger=None) -> FusionResult:
    """SNR^2 连续加权融合三片。

    步骤:
      1. 收集所有非排异像素 (ipix, signal_corrected, snr, panel_id)
      2. 按 ipix 排序聚合
      3. 对每个 ipix: signal = Σ(w_i × s_i) / Σ(w_i), w_i = snr_i^2
      4. 输出 weight_map = Σ(w_i), support_count, snr_eff

    Args:
        corrected: {panel_id: CorrectedPanel} (G-001 输出)
        params: 融合参数

    Returns:
        FusionResult
    """
    if logger is None:
        logger = setup_logger('g002')
    logger.info('=' * 70)
    logger.info('G-002: 独立 SNR^2 连续加权融合')
    logger.info('=' * 70)
    logger.info(f'权重 = SNR^2 × support (连续加权, 非硬阈值)')

    # 1. 收集所有非排异像素
    all_ipix = []
    all_signal = []
    all_snr = []
    all_panel_idx = []  # 0/1/2 对应 panel1/2/3
    for idx, pid in enumerate(PANEL_NAMES):
        cp = corrected[pid]
        # 非排异 且 support=1
        mask = (~cp.rejected) & (cp.support == 1)
        n = int(mask.sum())
        logger.info(f'  {pid}: 贡献像素 {n}/{cp.n_pix} (排异 {int(cp.rejected.sum())})')
        all_ipix.append(cp.ipix[mask])
        all_signal.append(cp.signal_corrected[mask].astype(np.float64))
        snr = cp.snr[mask].astype(np.float64)
        snr_safe = np.where(np.isfinite(snr) & (snr > 0), snr, params.snr_floor)
        all_snr.append(snr_safe)
        all_panel_idx.append(np.full(n, idx, dtype=np.uint8))

    ipix_all = np.concatenate(all_ipix)
    signal_all = np.concatenate(all_signal)
    snr_all = np.concatenate(all_snr)
    panel_idx_all = np.concatenate(all_panel_idx)
    N = ipix_all.size
    logger.info(f'  总贡献像素 (含重叠区重复): {N}')

    # 2. 计算权重 w = SNR^2 (连续加权)
    if params.use_snr_squared:
        weight_all = snr_all ** 2
    else:
        weight_all = snr_all.copy()
    weight_all = np.maximum(weight_all, params.weight_floor)

    # 3. 按 ipix 排序聚合
    order = np.argsort(ipix_all, kind='stable')
    ipix_sorted = ipix_all[order]
    signal_sorted = signal_all[order]
    snr_sorted = snr_all[order]
    weight_sorted = weight_all[order]
    panel_idx_sorted = panel_idx_all[order]

    # 找唯一 ipix 边界
    unique_ipix, starts = np.unique(ipix_sorted, return_index=True)
    M = unique_ipix.size
    logger.info(f'  唯一 ipix 数 (融合后像素): {M}')
    logger.info(f'  重叠区像素数 (贡献帧数>1): 待计算')

    # 4. 对每个唯一 ipix 聚合
    fused_signal = np.zeros(M, dtype=np.float64)
    fused_weight = np.zeros(M, dtype=np.float64)
    fused_support_count = np.zeros(M, dtype=np.uint8)
    fused_snr_eff = np.zeros(M, dtype=np.float64)
    # per-panel 贡献标记
    per_panel_contrib = {pid: np.zeros(M, dtype=np.uint8) for pid in PANEL_NAMES}

    for i in range(M):
        start = starts[i]
        end = starts[i + 1] if i + 1 < M else N
        w_k = weight_sorted[start:end]
        s_k = signal_sorted[start:end]
        snr_k = snr_sorted[start:end]
        pidx_k = panel_idx_sorted[start:end]

        wsum = float(np.sum(w_k))
        fused_signal[i] = float(np.sum(w_k * s_k) / wsum)
        fused_weight[i] = wsum
        fused_support_count[i] = np.uint8(end - start)
        # 有效 SNR: sqrt(Σ w_i × snr_i^2 / Σ w_i) — 保持 SNR 量纲
        fused_snr_eff[i] = float(np.sqrt(np.sum(w_k * snr_k ** 2) / wsum))
        # per-panel 贡献
        for pidx in np.unique(pidx_k):
            per_panel_contrib[PANEL_NAMES[int(pidx)]][i] = 1

    # 5. 坐标转换 (ipix -> RA/Dec)
    nside = int(corrected[PANEL_NAMES[0]].nside)
    nested = bool(corrected[PANEL_NAMES[0]].nested)
    from astropy_healpix import HEALPix
    from astropy import units as u
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(unique_ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)

    n_overlap = int((fused_support_count > 1).sum())
    logger.info(
        f'  融合完成: {M} 像素, 重叠区 {n_overlap} 像素 ({100.0*n_overlap/M:.2f}%)')
    logger.info(
        f'  signal: median={float(np.median(fused_signal)):.1f} '
        f'range=[{float(np.min(fused_signal)):.1f}, {float(np.max(fused_signal)):.1f}]')
    logger.info(
        f'  weight: median={float(np.median(fused_weight)):.3g} '
        f'range=[{float(np.min(fused_weight)):.3g}, {float(np.max(fused_weight)):.3g}]')
    logger.info(
        f'  snr_eff: median={float(np.median(fused_snr_eff)):.1f} '
        f'range=[{float(np.min(fused_snr_eff)):.1f}, {float(np.max(fused_snr_eff)):.1f}]')

    return FusionResult(
        nside=nside,
        nested=nested,
        ipix=unique_ipix.astype(np.uint64),
        signal=fused_signal.astype(np.float32),
        weight=fused_weight,
        support_count=fused_support_count,
        snr_eff=fused_snr_eff.astype(np.float32),
        coverage=np.ones(M, dtype=np.uint8),
        ra=ra,
        dec=dec,
        per_panel_contrib=per_panel_contrib,
    )


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('g002', log_dir=LOG_DIR)
    log.info('=' * 70)
    log.info('G-002 自测 (依赖 G-001)')
    log.info('=' * 70)
    from g001_reject import process_all_panels, RejectParams
    from g_common import load_all_panels, load_e003_result
    panels = load_all_panels()
    e003 = load_e003_result()
    corrected = process_all_panels(panels, e003, RejectParams(), logger=log)
    fused = fuse_panels(corrected, FusionParams(), logger=log)
    log.info(f'  融合后: {fused.n_pix} 像素, nside={fused.nside}')
    log.info('G-002 自测 PASS')
