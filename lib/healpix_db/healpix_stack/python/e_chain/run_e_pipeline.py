# -*- coding: utf-8 -*-
"""run_e_pipeline.py - 运行 E-001~E-003 真实数据 pipeline, 生成 JSON 证据 + TASK_REPORT.md."""
from __future__ import annotations
import json
import os
import sys
import time
from datetime import datetime

PROJECT = r'f:\Astro dev\Astro CS Normalization Database'
os.chdir(PROJECT)
sys.path.insert(0, os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'e_chain'))
sys.path.insert(0, os.path.join('lib', 'astro_image_io', 'python'))

import numpy as np
from e_common import (
    load_all_panels, setup_logger, PANEL_NAMES, N_SURFACE_COEFFS, json_default,
)
from e_masks_sampling import MaskParams, process_all_panels
from e_weights import WeightParams, process_all_weights
from e_solver import SolverParams, solve_global_additive, extract_gradient

EVID = os.path.join('engineering_authoritative', 'evidence')
LOG_DIR = os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'e_chain', 'logs')
SEED = 20260730
for d in [LOG_DIR,
          os.path.join(EVID, 'E-001'),
          os.path.join(EVID, 'E-002'),
          os.path.join(EVID, 'E-003')]:
    os.makedirs(d, exist_ok=True)

log = setup_logger('e_pipeline', log_dir=LOG_DIR)
log.info('=' * 70)
log.info('E-001~E-003 真实数据 pipeline (D-001 三片 Red HISS)')
log.info('=' * 70)

# ============================================================================
# 加载数据
# ============================================================================
t0 = time.time()
panels = load_all_panels()
for p in PANEL_NAMES:
    log.info('  %s: n_pix=%d nside=%d signal_median=%.1f' % (
        p, panels[p].n_pix, panels[p].nside, float(np.median(panels[p].signal))))
load_time = time.time() - t0

# ============================================================================
# E-001: 掩膜 + 稀疏控制点采样
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('E-001: 星点/饱和/异常掩膜 + 稀疏控制点采样')
log.info('=' * 70)
t1 = time.time()
cp_results = process_all_panels(panels, MaskParams(), seed=SEED, logger=log)
e001_time = time.time() - t1

e001_result = {
    'task': 'E-001',
    'seed': SEED,
    'panels': {},
    'elapsed_s': round(e001_time, 3),
}
total_valid = 0
total_cp = 0
for p in PANEL_NAMES:
    cp, st = cp_results[p]
    e001_result['panels'][p] = {
        'n_pix': panels[p].n_pix,
        'nside': panels[p].nside,
        'median': st['median'],
        'mad_sigma': st['mad_sigma'],
        'star_thresh': st['star_thresh'],
        'sat_thresh': st['sat_thresh'],
        'n_star': st['n_star'],
        'n_sat': st['n_sat'],
        'n_anom': st['n_anom'],
        'n_valid': st['n_valid'],
        'valid_pct': round(100.0 * st['n_valid'] / panels[p].n_pix, 2),
        'n_ctrl_pts': cp.n_points,
        'ra_range': [float(cp.ra.min()), float(cp.ra.max())],
        'dec_range': [float(cp.dec.min()), float(cp.dec.max())],
        'snr_median': float(np.median(cp.snr[np.isfinite(cp.snr)])),
    }
    total_valid += st['n_valid']
    total_cp += cp.n_points
e001_result['total_valid'] = total_valid
e001_result['total_ctrl_pts'] = total_cp
e001_result['target_cp_per_panel'] = MaskParams.target_cp_per_panel
e001_result['n_strata'] = MaskParams.n_strata

with open(os.path.join(EVID, 'E-001', 'e001_result.json'), 'w', encoding='utf-8') as f:
    json.dump(e001_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('E-001 完成: total_valid=%d total_cp=%d (%.3fs)' % (
    total_valid, total_cp, e001_time))

# ============================================================================
# E-002: SNR^2 / 逆方差联合权重 + 重叠区共识
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('E-002: SNR^2/逆方差联合权重 + 重叠区多帧加权平均')
log.info('=' * 70)
t2 = time.time()
cps = {p: cp_results[p][0] for p in PANEL_NAMES}
w_stats, o_stats = process_all_weights(panels, cps, WeightParams(), logger=log)
e002_time = time.time() - t2

e002_result = {
    'task': 'E-002',
    'weight_stats': w_stats,
    'overlap_stats': {},
    'elapsed_s': round(e002_time, 3),
}
for key, st in o_stats.items():
    e002_result['overlap_stats'][key] = {
        'panels': st['panels'],
        'n_overlap': st['n_overlap'],
        'delta_median_per_panel': st['delta_median_per_panel'],
        'delta_mad_per_panel': st['delta_mad_per_panel'],
    }
with open(os.path.join(EVID, 'E-002', 'e002_result.json'), 'w', encoding='utf-8') as f:
    json.dump(e002_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('E-002 完成 (%.3fs)' % e002_time)

# ============================================================================
# E-003: 全局加性共识曲面稀疏求解器
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('E-003: 全局加性共识曲面稀疏求解器 (零均值约束, 只加性)')
log.info('=' * 70)
t3 = time.time()
result = solve_global_additive(cps, SolverParams(), logger=log)
e003_time = time.time() - t3
a_ra, b_dec = extract_gradient(result)

e003_result = {
    'task': 'E-003',
    'surface_coeffs': result.surface_coeffs.tolist(),
    'offsets': result.offsets,
    'center': list(result.center),
    'panel_order': result.panel_order,
    'gradient': {'a_ra': a_ra, 'b_dec': b_dec},
    'stats': result.stats,
    'elapsed_s': round(e003_time, 3),
}
with open(os.path.join(EVID, 'E-003', 'e003_result.json'), 'w', encoding='utf-8') as f:
    json.dump(e003_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('E-003 完成: a_ra=%.4f b_dec=%.4f wrms=%.2f zero_mean_err=%.2e (%.3fs)' % (
    a_ra, b_dec, result.stats['residual_wrms'],
    result.stats['zero_mean_error'], e003_time))

total_time = time.time() - t0
log.info('')
log.info('=' * 70)
log.info('Pipeline 全部完成: load=%.3fs E001=%.3fs E002=%.3fs E003=%.3fs total=%.3fs' % (
    load_time, e001_time, e002_time, e003_time, total_time))
log.info('=' * 70)

# 保存汇总
summary = {
    'pipeline': 'E-001~E-003',
    'data': 'D-001 三片 Red HISS (GalaxyCenter)',
    'seed': SEED,
    'timestamp': datetime.now().isoformat(),
    'timings': {
        'load_s': round(load_time, 3),
        'e001_s': round(e001_time, 3),
        'e002_s': round(e002_time, 3),
        'e003_s': round(e003_time, 3),
        'total_s': round(total_time, 3),
    },
    'e001': e001_result,
    'e002': e002_result,
    'e003': e003_result,
}
with open(os.path.join(EVID, 'E-003', 'pipeline_summary.json'), 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, default=json_default, ensure_ascii=False)
print('DONE: all results saved to engineering_authoritative/evidence/E-001~E-003/')
