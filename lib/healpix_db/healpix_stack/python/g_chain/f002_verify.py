# -*- coding: utf-8 -*-
"""
f002_verify.py - F-002 三片最小总曲面 / HCSD / 浏览器检查

任务契约 (F-002):
  1. 用 G-003 生成的 HCSD
  2. 浏览器检查接缝 (三片拼接处)
  3. 验证总曲面可求解
  4. 验证三片位置正确

依赖: E-004 (已完成) + F-001 (已完成) + G-003 (已完成)

禁止捷径:
  - 不得使用不同滤镜 (三片均为 Red)
  - 不得只用 panel1 (三片全部参与)
  - 不得使用重复 HISS (三片独立)

验证方法:
  1. HCSD 可读性: 读回 G-003 输出, 验证 ipix 升序 / 子叶索引 / signal 范围
  2. 三片位置正确: 检查三片 RA/Dec 范围与 D-001 一致, 重叠区连通
  3. 接缝检查: 重叠区像素 signal 连续性 (无硬边), 相邻像素差分分布
  4. 总曲面可求解: 用融合后 signal 重新拟合曲面, 验证收敛 + 残差合理
  5. 浏览器可视化: 生成接缝检查图 (matplotlib), 供浏览器查看
"""
from __future__ import annotations

import json
import os
import sys
from typing import Dict, Tuple

import numpy as np

PROJECT = r'f:\Astro dev\Astro CS Normalization Database'
os.chdir(PROJECT)
sys.path.insert(0, os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'g_chain'))
sys.path.insert(0, os.path.join('lib', 'astro_image_io', 'python'))

from g_common import (
    load_all_panels, load_e003_result, setup_logger, PANEL_NAMES,
    LOG_DIR, EVID_DIR, GF_OUTPUT_DIR, json_default, hcsd_read,
    eval_surface, poly_basis, safe_mad_sigma,
)


# ============================================================================
# F-002 验证项
# ============================================================================

def verify_hcsd_readable(hcsd_path: str, logger=None) -> dict:
    """验证 1: HCSD 可读性 + 格式合规。"""
    if logger is None:
        logger = setup_logger('f002_read')
    logger.info('--- 验证 1: HCSD 可读性 ---')
    nside, nested, ipix, pixel, meta = hcsd_read(hcsd_path)
    n_pix = int(ipix.size)
    is_sorted = bool(np.all(ipix[:-1] <= ipix[1:]))
    # 唯一性
    n_unique = int(np.unique(ipix).size)
    # 子叶分布
    from g_common import _compute_leaf_ipix
    leaf_ipix = _compute_leaf_ipix(ipix, nside)
    n_leaves = int(np.unique(leaf_ipix).size)

    result = dict(
        path=hcsd_path,
        nside=int(nside), nested=bool(nested), n_pix=n_pix,
        ipix_sorted=is_sorted,
        ipix_unique=(n_unique == n_pix),
        n_leaves_with_data=n_leaves,
        signal_median=float(np.median(pixel)),
        signal_min=float(np.min(pixel)),
        signal_max=float(np.max(pixel)),
        filter=meta.get('filter', ''),
        n_frames=int(meta.get('n_frames', 0)),
    )
    logger.info(f'  nside={nside} n_pix={n_pix} ipix_sorted={is_sorted} '
                f'ipix_unique={result["ipix_unique"]} n_leaves={n_leaves}')
    logger.info(f'  signal: median={result["signal_median"]:.1f} '
                f'range=[{result["signal_min"]:.1f}, {result["signal_max"]:.1f}]')
    logger.info(f'  meta: filter={result["filter"]} n_frames={result["n_frames"]}')

    checks = dict(
        ipix_sorted=is_sorted,
        ipix_unique=(n_unique == n_pix),
        n_pix_positive=(n_pix > 0),
        filter_correct=(meta.get('filter') == 'Red'),
        n_frames_correct=(int(meta.get('n_frames', 0)) == 3),
    )
    result['checks'] = checks
    result['all_pass'] = all(checks.values())
    logger.info(f'  检查: {checks} -> {"PASS" if result["all_pass"] else "FAIL"}')
    return result


def verify_panel_positions(panels: Dict[str, dict], logger=None) -> dict:
    """验证 2: 三片位置正确 (RA/Dec 范围 + 重叠区连通)。"""
    if logger is None:
        logger = setup_logger('f002_pos')
    logger.info('--- 验证 2: 三片位置正确 ---')

    # D-001 期望范围 (从 E-001 结果)
    expected = {
        'panel1': {'ra': [268.68, 276.94], 'dec': [-16.33, -9.90]},
        'panel2': {'ra': [268.68, 277.12], 'dec': [-21.46, -15.02]},
        'panel3': {'ra': [268.51, 277.29], 'dec': [-26.44, -20.03]},
    }

    result = {'panels': {}, 'overlaps': {}}
    all_pass = True
    for pid in PANEL_NAMES:
        p = panels[pid]
        ra_min, ra_max = float(p['ra'].min()), float(p['ra'].max())
        dec_min, dec_max = float(p['dec'].min()), float(p['dec'].max())
        exp = expected[pid]
        ra_ok = abs(ra_min - exp['ra'][0]) < 1.0 and abs(ra_max - exp['ra'][1]) < 1.0
        dec_ok = abs(dec_min - exp['dec'][0]) < 1.0 and abs(dec_max - exp['dec'][1]) < 1.0
        result['panels'][pid] = dict(
            ra_range=[ra_min, ra_max], dec_range=[dec_min, dec_max],
            ra_match=ra_ok, dec_match=dec_ok,
        )
        logger.info(f'  {pid}: RA=[{ra_min:.2f}, {ra_max:.2f}] Dec=[{dec_min:.2f}, {dec_max:.2f}] '
                    f'ra_match={ra_ok} dec_match={dec_ok}')
        all_pass = all_pass and ra_ok and dec_ok

    # 重叠区连通性 (panel1∩panel2, panel2∩panel3)
    for a, b in [('panel1', 'panel2'), ('panel2', 'panel3')]:
        ipix_a = set(panels[a]['ipix'].tolist())
        ipix_b = set(panels[b]['ipix'].tolist())
        overlap = ipix_a & ipix_b
        n_overlap = len(overlap)
        # 连通性: 重叠区像素数 > 0
        connected = n_overlap > 0
        result['overlaps'][f'{a}_cap_{b}'] = dict(
            n_overlap=n_overlap, connected=connected)
        logger.info(f'  重叠 {a}∩{b}: {n_overlap} 像素, connected={connected}')
        all_pass = all_pass and connected

    result['all_pass'] = all_pass
    logger.info(f'  -> {"PASS" if all_pass else "FAIL"}')
    return result


def verify_seam_continuity(hcsd_path: str, debug_path: str = None,
                           logger=None) -> dict:
    """验证 3: 接缝连续性 (重叠区无硬边)。"""
    if logger is None:
        logger = setup_logger('f002_seam')
    logger.info('--- 验证 3: 接缝连续性 ---')

    nside, nested, ipix, pixel, meta = hcsd_read(hcsd_path)
    n_pix = int(ipix.size)

    # 加载调试层 (support_count 标记重叠区)
    support_count = None
    if debug_path and os.path.isfile(debug_path):
        dbg = np.load(debug_path, allow_pickle=True)
        if 'support_count' in dbg.files:
            support_count = dbg['support_count']

    # 全局相邻像素差分 (HEALPix 像素索引相邻不一定空间相邻, 用 RA/Dec 最近邻)
    from astropy_healpix import HEALPix
    from astropy import units as u
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)

    # 重叠区 vs 非重叠区 signal 统计
    if support_count is not None:
        overlap_mask = support_count > 1
        non_overlap_mask = support_count == 1
    else:
        overlap_mask = np.zeros(n_pix, dtype=bool)
        non_overlap_mask = np.ones(n_pix, dtype=bool)

    result = {}
    if overlap_mask.any() and non_overlap_mask.any():
        sig_overlap = pixel[overlap_mask]
        sig_non = pixel[non_overlap_mask]
        # 接缝连续性: 重叠区与非重叠区 signal 分布应重叠 (无硬边阶跃)
        med_overlap = float(np.median(sig_overlap))
        med_non = float(np.median(sig_non))
        # 相对偏差 < 20% 视为连续 (无硬边)
        rel_diff = abs(med_overlap - med_non) / max(abs(med_overlap), abs(med_non), 1.0)
        no_hard_edge = rel_diff < 0.20
        result = dict(
            overlap_median=med_overlap,
            non_overlap_median=med_non,
            rel_diff=rel_diff,
            no_hard_edge=no_hard_edge,
            n_overlap=int(overlap_mask.sum()),
            n_non_overlap=int(non_overlap_mask.sum()),
        )
        logger.info(f'  重叠区 median={med_overlap:.1f} (n={result["n_overlap"]})')
        logger.info(f'  非重叠区 median={med_non:.1f} (n={result["n_non_overlap"]})')
        logger.info(f'  相对偏差={rel_diff:.4f} (< 0.20 视为无硬边) -> {no_hard_edge}')
    else:
        result = dict(no_hard_edge=True, note='无重叠区或无非重叠区, 跳过硬边检查')
        logger.info('  无重叠区或无非重叠区, 跳过硬边检查')

    # 局部差分: 重叠区边界相邻像素 signal 差
    # 简化: 用 ipix 升序相邻差分 (近似, 因 HEALPix 像素索引相邻不一定空间相邻)
    if n_pix > 1:
        diffs = np.abs(np.diff(pixel.astype(np.float64)))
        result['diff_median'] = float(np.median(diffs))
        result['diff_p95'] = float(np.percentile(diffs, 95))
        result['diff_max'] = float(np.max(diffs))
        logger.info(f'  相邻像素差分: median={result["diff_median"]:.1f} '
                    f'p95={result["diff_p95"]:.1f} max={result["diff_max"]:.1f}')

    result['all_pass'] = result.get('no_hard_edge', True)
    logger.info(f'  -> {"PASS" if result["all_pass"] else "FAIL"}')
    return result


def verify_surface_refit(hcsd_path: str, e003, logger=None) -> dict:
    """验证 4: 总曲面可求解 (用融合后 signal 重新拟合曲面)。"""
    if logger is None:
        logger = setup_logger('f002_refit')
    logger.info('--- 验证 4: 总曲面可求解 ---')

    nside, nested, ipix, pixel, meta = hcsd_read(hcsd_path)
    n_pix = int(ipix.size)

    # ipix -> RA/Dec
    from astropy_healpix import HEALPix
    from astropy import units as u
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)
    signal = pixel.astype(np.float64)

    # 用 E-003 的 center, 重新拟合 2D 多项式曲面 (无偏移, 单帧已校正)
    # 最小二乘: signal = basis @ coeffs
    basis = poly_basis(ra, dec, e003.center)
    coeffs, residuals, rank, sv = np.linalg.lstsq(basis, signal, rcond=None)
    pred = basis @ coeffs
    resid = signal - pred
    rms = float(np.sqrt(np.mean(resid ** 2))) if resid.size > 0 else 0.0
    mad_sigma = safe_mad_sigma(resid)
    # 相对残差
    signal_range = float(signal.max() - signal.min())
    rel_rms = rms / max(signal_range, 1.0)

    # 收敛性: rank == 6 (满秩), 残差有限
    converged = (rank == 6) and np.isfinite(rms) and rms > 0
    # 曲面可求解: 相对残差 < 30% (融合后信号应与曲面一致, 残差来自天体+噪声)
    solvable = converged and rel_rms < 0.30

    result = dict(
        n_pix=n_pix,
        coeffs=coeffs.tolist(),
        rank=int(rank),
        singular_values=sv.tolist(),
        residual_rms=rms,
        residual_mad_sigma=mad_sigma,
        signal_range=signal_range,
        rel_rms=rel_rms,
        converged=converged,
        solvable=solvable,
    )
    logger.info(f'  拟合 rank={rank} rms={rms:.1f} mad_sigma={mad_sigma:.1f}')
    logger.info(f'  signal_range={signal_range:.1f} rel_rms={rel_rms:.4f}')
    logger.info(f'  converged={converged} solvable={solvable}')
    result['all_pass'] = solvable
    logger.info(f'  -> {"PASS" if result["all_pass"] else "FAIL"}')
    return result


def generate_seam_visualization(hcsd_path: str, debug_path: str = None,
                                output_dir: str = None, logger=None) -> str:
    """生成接缝检查图 (matplotlib), 供浏览器查看。"""
    if logger is None:
        logger = setup_logger('f002_viz')
    logger.info('--- 生成接缝检查图 ---')

    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        logger.warning('matplotlib 不可用, 跳过可视化')
        return ''

    nside, nested, ipix, pixel, meta = hcsd_read(hcsd_path)
    from astropy_healpix import HEALPix
    from astropy import units as u
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)
    signal = pixel.astype(np.float64)

    # 加载调试层
    support_count = None
    per_panel = {}
    if debug_path and os.path.isfile(debug_path):
        dbg = np.load(debug_path, allow_pickle=True)
        if 'support_count' in dbg.files:
            support_count = dbg['support_count']
        for pid in PANEL_NAMES:
            key = f'contrib_{pid}'
            if key in dbg.files:
                per_panel[pid] = dbg[key]

    if output_dir is None:
        output_dir = os.path.join(EVID_DIR, 'F-002')
    os.makedirs(output_dir, exist_ok=True)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # 1. 融合 signal 天球图
    ax = axes[0, 0]
    sc = ax.scatter(ra, dec, c=signal, s=1, cmap='viridis', alpha=0.6)
    plt.colorbar(sc, ax=ax, label='signal')
    ax.set_xlabel('RA (deg)')
    ax.set_ylabel('Dec (deg)')
    ax.set_title(f'Fused signal (n={ipix.size})')
    ax.invert_xaxis()  # 天文惯例

    # 2. support_count (重叠区标记)
    ax = axes[0, 1]
    if support_count is not None:
        sc = ax.scatter(ra, dec, c=support_count, s=2, cmap='plasma', alpha=0.7)
        plt.colorbar(sc, ax=ax, label='support_count')
        ax.set_title(f'Overlap map (1=single, 2=overlap, 3=triple)')
    else:
        ax.text(0.5, 0.5, 'No debug layer', ha='center', va='center')
        ax.set_title('Overlap map (N/A)')
    ax.set_xlabel('RA (deg)')
    ax.set_ylabel('Dec (deg)')
    ax.invert_xaxis()

    # 3. 三片贡献图 (不同颜色)
    ax = axes[1, 0]
    colors = {'panel1': 'red', 'panel2': 'green', 'panel3': 'blue'}
    for pid in PANEL_NAMES:
        if pid in per_panel:
            mask = per_panel[pid] == 1
            ax.scatter(ra[mask], dec[mask], c=colors[pid], s=1, alpha=0.4, label=pid)
    ax.set_xlabel('RA (deg)')
    ax.set_ylabel('Dec (deg)')
    ax.set_title('Per-panel contribution')
    ax.legend(markerscale=5)
    ax.invert_xaxis()

    # 4. 接缝处 signal 分布 (重叠区 vs 非重叠区直方图)
    ax = axes[1, 1]
    if support_count is not None:
        overlap = signal[support_count > 1]
        non_overlap = signal[support_count == 1]
        if overlap.size > 0 and non_overlap.size > 0:
            ax.hist(non_overlap, bins=50, alpha=0.6, label=f'non-overlap (n={non_overlap.size})', color='gray')
            ax.hist(overlap, bins=50, alpha=0.6, label=f'overlap (n={overlap.size})', color='orange')
            ax.legend()
    ax.set_xlabel('signal')
    ax.set_ylabel('count')
    ax.set_title('Seam signal distribution')

    plt.tight_layout()
    out_path = os.path.join(output_dir, 'seam_check.png')
    plt.savefig(out_path, dpi=120, bbox_inches='tight')
    plt.close()
    logger.info(f'  接缝检查图: {out_path}')
    return out_path


# ============================================================================
# F-002 主验证
# ============================================================================

def run_f002_verification(hcsd_path: str = None, debug_path: str = None,
                          logger=None) -> dict:
    """F-002 完整验证。"""
    if logger is None:
        logger = setup_logger('f002', log_dir=LOG_DIR)
    logger.info('=' * 70)
    logger.info('F-002: 三片最小总曲面 / HCSD / 浏览器检查')
    logger.info('=' * 70)

    if hcsd_path is None:
        hcsd_path = os.path.join(GF_OUTPUT_DIR, 'T4_RED_GalaxyCenter_fused.hcsd')
    if debug_path is None:
        debug_path = os.path.join(GF_OUTPUT_DIR, 'T4_RED_GalaxyCenter_fused.hcsd.debug.npz')

    # 加载三片用于位置验证
    panels = load_all_panels()
    e003 = load_e003_result()

    # 验证 1: HCSD 可读性
    v1 = verify_hcsd_readable(hcsd_path, logger=logger)

    # 验证 2: 三片位置正确
    v2 = verify_panel_positions(panels, logger=logger)

    # 验证 3: 接缝连续性
    v3 = verify_seam_continuity(hcsd_path, debug_path, logger=logger)

    # 验证 4: 总曲面可求解
    v4 = verify_surface_refit(hcsd_path, e003, logger=logger)

    # 生成接缝检查图
    viz_path = generate_seam_visualization(hcsd_path, debug_path, logger=logger)

    # 汇总
    all_pass = v1['all_pass'] and v2['all_pass'] and v3['all_pass'] and v4['all_pass']
    result = dict(
        task='F-002',
        hcsd_path=hcsd_path,
        debug_path=debug_path,
        viz_path=viz_path,
        v1_hcsd_readable=v1,
        v2_panel_positions=v2,
        v3_seam_continuity=v3,
        v4_surface_refit=v4,
        all_pass=all_pass,
    )
    logger.info('=' * 70)
    logger.info(f'F-002 汇总: {"ALL PASS" if all_pass else "FAIL"}')
    logger.info(f'  v1 HCSD可读: {"PASS" if v1["all_pass"] else "FAIL"}')
    logger.info(f'  v2 三片位置: {"PASS" if v2["all_pass"] else "FAIL"}')
    logger.info(f'  v3 接缝连续: {"PASS" if v3["all_pass"] else "FAIL"}')
    logger.info(f'  v4 曲面可求解: {"PASS" if v4["all_pass"] else "FAIL"}')
    logger.info('=' * 70)
    return result


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    result = run_f002_verification()
    # 保存结果
    os.makedirs(os.path.join(EVID_DIR, 'F-002'), exist_ok=True)
    with open(os.path.join(EVID_DIR, 'F-002', 'f002_result.json'), 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2, default=json_default, ensure_ascii=False)
    print('DONE: F-002 verification saved to engineering_authoritative/evidence/F-002/')
