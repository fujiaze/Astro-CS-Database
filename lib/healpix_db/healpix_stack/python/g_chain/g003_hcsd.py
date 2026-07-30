# -*- coding: utf-8 -*-
"""
g003_hcsd.py - G-003 HCSD 生产层 + 可开关调试质量层

任务契约 (G-003):
  1. 生成 HCSD 文件 (天球数据库)
  2. 生产层: 融合 signal + support + SNR
  3. 调试质量层 (可开关): 每帧贡献图、排异图、权重图
  4. HCSD 格式参考 lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md §3

禁止捷径:
  - 调试层开关 (生产层必写, 调试层可选)
  - 不得产生硬边 (G-002 已连续加权)

HCSD 格式 (HEALPIX_FORMAT_SPEC.md §3):
  - Magic "HCSD" (4B)
  - JSON 头 (uncomp_len u32 + comp_len u32 + zstd JSON)
  - 子叶索引表 (49152 × 24B = 1,179,648B, nside=64 分区)
  - ipix 数组 (n_pix × 8B u64, 升序)
  - pixel 数组 (n_pix × 4B f32)

生产层 (单文件 T4_RED_GalaxyCenter_fused.hcsd):
  - pixel = fused.signal
  - meta: nside/nested/n_pix/filter/n_frames/total_exposure_s/sigma_clip/stack_stats
    + snr_metadata (snr_eff 数组存为附属 .hcsd.snr 文件, 因 HCSD 格式只存单 pixel 数组)

调试层 (可选, 单文件 T4_RED_GalaxyCenter_fused.hcsd.debug):
  - 多通道: weight_map / support_count / per_panel_contrib / rejected_per_panel
  - 用 .npz 格式存储 (便于浏览器后续支持, 不影响生产层 HCSD)

实现说明:
  - HCSD 格式只支持单 pixel 数组, 故 SNR 单独存为 .hcsd.snr (f32, 同 ipix 顺序)
  - 调试层用 .npz (numpy 原生压缩), 含 ipix + 各调试图
  - 生产层 HCSD 可被浏览器直接读取 (符合格式规范)
"""
from __future__ import annotations

import os
from typing import Dict, Optional

import numpy as np

from g_common import (
    CorrectedPanel, FusionResult, HcsdLayers, PANEL_NAMES, setup_logger, LOG_DIR,
    GF_OUTPUT_DIR, hcsd_write, hcsd_read, json_default,
)


# ============================================================================
# HCSD 写入参数
# ============================================================================

class HcsdParams:
    """G-003 HCSD 写入参数。"""
    write_debug_layers = True   # 调试层开关 (默认开, 可关)
    write_snr_file = True       # SNR 附属文件开关 (生产层 SNR)


# ============================================================================
# G-003 主算法
# ============================================================================

def build_hcsd_layers(fused: FusionResult,
                      corrected: Dict[str, CorrectedPanel],
                      params: HcsdParams = HcsdParams(),
                      logger=None) -> HcsdLayers:
    """构建 HCSD 生产层 + 调试质量层。

    生产层: ipix + signal + support + snr (snr 存附属文件)
    调试层: weight_map + support_count + per_panel_contrib + rejected_per_panel

    Args:
        fused: G-002 融合结果
        corrected: G-001 校正后排异结果 (用于调试层 rejected_per_panel)
        params: HCSD 写入参数

    Returns:
        HcsdLayers
    """
    if logger is None:
        logger = setup_logger('g003')
    logger.info('=' * 70)
    logger.info('G-003: HCSD 生产层 + 可开关调试质量层')
    logger.info('=' * 70)
    logger.info(f'  调试层开关: {"ON" if params.write_debug_layers else "OFF"}')

    # 生产层
    layers = HcsdLayers(
        nside=int(fused.nside),
        nested=bool(fused.nested),
        ipix=fused.ipix,
        signal=fused.signal,
        support=fused.coverage,
        snr=fused.snr_eff,
        debug_enabled=bool(params.write_debug_layers),
    )

    # 调试层
    if params.write_debug_layers:
        layers.weight_map = fused.weight
        layers.support_count = fused.support_count
        layers.per_panel_contrib = dict(fused.per_panel_contrib)
        # rejected_per_panel: 每帧排异图 (按该帧原始 ipix 顺序)
        layers.rejected_per_panel = {
            pid: corrected[pid].rejected.copy() for pid in PANEL_NAMES
        }
        logger.info(
            f'  调试层: weight_map + support_count + per_panel_contrib + '
            f'rejected_per_panel (3 frames)')

    logger.info(
        f'  生产层: {fused.n_pix} 像素, nside={fused.nside}, '
        f'signal median={float(np.median(fused.signal)):.1f}')
    return layers


def write_hcsd(layers: HcsdLayers, output_dir: str = GF_OUTPUT_DIR,
               filter_name: str = 'Red', n_frames: int = 3,
               total_exposure_s: float = 540.0,
               logger=None) -> Dict[str, str]:
    """写入 HCSD 生产层 + 调试层文件。

    输出文件:
      - {stem}.hcsd          生产层 (signal, 符合格式规范)
      - {stem}.hcsd.snr      生产层 SNR 附属文件 (f32, 同 ipix 顺序)
      - {stem}.hcsd.debug    调试层 (.npz, 可开关)

    Args:
        layers: HcsdLayers
        output_dir: 输出目录
        filter_name: 滤光片名
        n_frames: 帧数
        total_exposure_s: 总曝光时间

    Returns:
        {kind: path} 已写入文件路径
    """
    if logger is None:
        logger = setup_logger('g003_write')
    os.makedirs(output_dir, exist_ok=True)
    stem = f'T4_RED_GalaxyCenter_fused'
    paths = {}

    # 1. 生产层 HCSD (signal)
    hcsd_path = os.path.join(output_dir, f'{stem}.hcsd')
    meta = {
        'filter': filter_name,
        'n_frames': int(n_frames),
        'total_exposure_s': float(total_exposure_s),
        'sigma_clip': {'sigma': 3.0, 'max_iter': 5},
        'stack_stats': {
            'mean_pixel_count': float(np.mean(layers.support_count)) if layers.support_count is not None else 1.0,
            'median_exposure': float(total_exposure_s / n_frames),
        },
        'source': 'G-003 SNR^2 连续加权融合 (Gate G)',
        'pipeline': 'E-001~E-003 + G-001~G-003',
    }
    hcsd_write(hcsd_path, layers.nside, layers.nested,
               layers.ipix, layers.signal, meta)
    paths['hcsd'] = hcsd_path
    logger.info(f'  生产层 HCSD: {hcsd_path} ({os.path.getsize(hcsd_path)} bytes)')

    # 2. 生产层 SNR 附属文件 (f32, 同 ipix 顺序)
    snr_path = os.path.join(output_dir, f'{stem}.hcsd.snr')
    snr_arr = np.ascontiguousarray(layers.snr, dtype='<f4')
    with open(snr_path, 'wb') as f:
        f.write(snr_arr.tobytes())
    paths['snr'] = snr_path
    logger.info(f'  生产层 SNR: {snr_path} ({os.path.getsize(snr_path)} bytes)')

    # 3. 调试层 (.npz, 可开关)
    if layers.debug_enabled:
        debug_path = os.path.join(output_dir, f'{stem}.hcsd.debug.npz')
        debug_data = {
            'ipix': layers.ipix,
            'weight_map': layers.weight_map,
            'support_count': layers.support_count,
        }
        if layers.per_panel_contrib is not None:
            for pid, contrib in layers.per_panel_contrib.items():
                debug_data[f'contrib_{pid}'] = contrib
        if layers.rejected_per_panel is not None:
            for pid, rej in layers.rejected_per_panel.items():
                debug_data[f'rejected_{pid}'] = rej
        np.savez_compressed(debug_path, **debug_data)
        paths['debug'] = debug_path
        logger.info(f'  调试层: {debug_path} ({os.path.getsize(debug_path)} bytes)')

    return paths


def verify_hcsd(path: str, logger=None) -> dict:
    """验证 HCSD 文件可读性 (生产层)。

    Returns:
        dict: 验证结果 (nside, n_pix, ipix_min/max, signal_stats, leaf_stats)
    """
    if logger is None:
        logger = setup_logger('g003_verify')
    logger.info(f'验证 HCSD: {path}')
    nside, nested, ipix, pixel, meta = hcsd_read(path)
    n_pix = int(ipix.size)
    # ipix 升序检查
    is_sorted = bool(np.all(ipix[:-1] <= ipix[1:]))
    # 子叶分布
    from g_common import _compute_leaf_ipix
    leaf_ipix = _compute_leaf_ipix(ipix, nside)
    n_leaves = int(np.unique(leaf_ipix).size)
    result = dict(
        path=path,
        nside=int(nside), nested=bool(nested), n_pix=n_pix,
        ipix_min=int(ipix.min()), ipix_max=int(ipix.max()),
        ipix_sorted=is_sorted,
        n_leaves_with_data=n_leaves,
        signal_median=float(np.median(pixel)),
        signal_min=float(np.min(pixel)),
        signal_max=float(np.max(pixel)),
        meta_keys=list(meta.keys()),
        filter=meta.get('filter', ''),
        n_frames=int(meta.get('n_frames', 0)),
    )
    logger.info(
        f'  nside={nside} n_pix={n_pix} ipix_sorted={is_sorted} '
        f'n_leaves={n_leaves} signal_median={result["signal_median"]:.1f}')
    logger.info(f'  meta: filter={result["filter"]} n_frames={result["n_frames"]}')
    return result


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('g003', log_dir=LOG_DIR)
    log.info('=' * 70)
    log.info('G-003 自测 (依赖 G-001 + G-002)')
    log.info('=' * 70)
    from g001_reject import process_all_panels, RejectParams
    from g002_fusion import fuse_panels, FusionParams
    from g_common import load_all_panels, load_e003_result
    panels = load_all_panels()
    e003 = load_e003_result()
    corrected = process_all_panels(panels, e003, RejectParams(), logger=log)
    fused = fuse_panels(corrected, FusionParams(), logger=log)
    layers = build_hcsd_layers(fused, corrected, HcsdParams(), logger=log)
    paths = write_hcsd(layers, logger=log)
    verify_hcsd(paths['hcsd'], logger=log)
    log.info('G-003 自测 PASS')
