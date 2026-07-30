# -*- coding: utf-8 -*-
"""run_g_pipeline.py - 运行 G-001~G-003 真实数据 pipeline, 生成 JSON 证据 + HCSD 产物."""
from __future__ import annotations
import json
import os
import sys
import time
from datetime import datetime

PROJECT = r'f:\Astro dev\Astro CS Normalization Database'
os.chdir(PROJECT)
sys.path.insert(0, os.path.join('lib', 'healpix_db', 'healpix_stack', 'python', 'g_chain'))
sys.path.insert(0, os.path.join('lib', 'astro_image_io', 'python'))

import numpy as np
from g_common import (
    load_all_panels, load_e003_result, setup_logger, PANEL_NAMES,
    LOG_DIR, EVID_DIR, GF_OUTPUT_DIR, json_default, safe_mad_sigma,
)
from g001_reject import RejectParams, process_all_panels
from g002_fusion import FusionParams, fuse_panels
from g003_hcsd import HcsdParams, build_hcsd_layers, write_hcsd, verify_hcsd

# 创建证据目录
for d in [LOG_DIR,
          os.path.join(EVID_DIR, 'G-001'),
          os.path.join(EVID_DIR, 'G-002'),
          os.path.join(EVID_DIR, 'G-003'),
          GF_OUTPUT_DIR]:
    os.makedirs(d, exist_ok=True)

log = setup_logger('g_pipeline', log_dir=LOG_DIR)
log.info('=' * 70)
log.info('G-001~G-003 真实数据 pipeline (D-001 三片 Red HISS)')
log.info('=' * 70)

# ============================================================================
# 加载数据 + E-003 结果
# ============================================================================
t0 = time.time()
panels = load_all_panels()
e003 = load_e003_result()
for p in PANEL_NAMES:
    log.info('  %s: n_pix=%d nside=%d signal_median=%.1f' % (
        p, panels[p]['ipix'].size, panels[p]['nside'],
        float(np.median(panels[p]['signal']))))
log.info('  E-003 offsets: %s' % e003.offsets)
load_time = time.time() - t0

# ============================================================================
# G-001: 加性偏移校正 + MAD 稳健排异
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('G-001: 加性偏移校正 + MAD 稳健排异')
log.info('=' * 70)
t1 = time.time()
corrected = process_all_panels(panels, e003, RejectParams(), logger=log)
g001_time = time.time() - t1

g001_result = {
    'task': 'G-001',
    'params': {'reject_sigma': RejectParams.reject_sigma,
               'reject_mad_factor': RejectParams.reject_mad_factor},
    'panels': {},
    'elapsed_s': round(g001_time, 3),
}
total_rej = 0
total_pix = 0
total_valid = 0
for p in PANEL_NAMES:
    cp = corrected[p]
    n_rej = int(cp.rejected.sum())
    n_valid = int((~cp.rejected & (cp.support == 1)).sum())
    g001_result['panels'][p] = {
        'n_pix': cp.n_pix,
        'offset': cp.offset,
        'residual_median': float(np.median(cp.residual)),
        'residual_mad_sigma': float(safe_mad_sigma(cp.residual)),
        'n_rejected': n_rej,
        'reject_pct': round(100.0 * n_rej / cp.n_pix, 3),
        'n_valid': n_valid,
        'snr_median': float(np.median(cp.snr[np.isfinite(cp.snr)])),
    }
    total_rej += n_rej
    total_pix += cp.n_pix
    total_valid += n_valid
g001_result['total_rejected'] = total_rej
g001_result['total_pix'] = total_pix
g001_result['total_valid'] = total_valid
g001_result['total_reject_pct'] = round(100.0 * total_rej / total_pix, 3)

with open(os.path.join(EVID_DIR, 'G-001', 'g001_result.json'), 'w', encoding='utf-8') as f:
    json.dump(g001_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('G-001 完成: rejected=%d/%d (%.3f%%) valid=%d (%.3fs)' % (
    total_rej, total_pix, g001_result['total_reject_pct'], total_valid, g001_time))

# ============================================================================
# G-002: 独立 SNR^2 连续加权融合
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('G-002: 独立 SNR^2 连续加权融合')
log.info('=' * 70)
t2 = time.time()
fused = fuse_panels(corrected, FusionParams(), logger=log)
g002_time = time.time() - t2

n_overlap = int((fused.support_count > 1).sum())
g002_result = {
    'task': 'G-002',
    'params': {'use_snr_squared': FusionParams.use_snr_squared,
               'snr_floor': FusionParams.snr_floor},
    'n_pix_fused': fused.n_pix,
    'n_overlap': n_overlap,
    'overlap_pct': round(100.0 * n_overlap / fused.n_pix, 3),
    'signal_median': float(np.median(fused.signal)),
    'signal_min': float(np.min(fused.signal)),
    'signal_max': float(np.max(fused.signal)),
    'weight_median': float(np.median(fused.weight)),
    'weight_min': float(np.min(fused.weight)),
    'weight_max': float(np.max(fused.weight)),
    'snr_eff_median': float(np.median(fused.snr_eff)),
    'snr_eff_min': float(np.min(fused.snr_eff)),
    'snr_eff_max': float(np.max(fused.snr_eff)),
    'support_count_dist': {int(k): int(v) for k, v in
                           zip(*np.unique(fused.support_count, return_counts=True))},
    'per_panel_contrib_count': {p: int(fused.per_panel_contrib[p].sum()) for p in PANEL_NAMES},
    'elapsed_s': round(g002_time, 3),
}
with open(os.path.join(EVID_DIR, 'G-002', 'g002_result.json'), 'w', encoding='utf-8') as f:
    json.dump(g002_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('G-002 完成: fused=%d overlap=%d (%.2f%%) (%.3fs)' % (
    fused.n_pix, n_overlap, g002_result['overlap_pct'], g002_time))

# ============================================================================
# G-003: HCSD 生产层 + 调试质量层
# ============================================================================
log.info('')
log.info('=' * 70)
log.info('G-003: HCSD 生产层 + 可开关调试质量层')
log.info('=' * 70)
t3 = time.time()
layers = build_hcsd_layers(fused, corrected, HcsdParams(), logger=log)
paths = write_hcsd(layers, output_dir=GF_OUTPUT_DIR, logger=log)
verify = verify_hcsd(paths['hcsd'], logger=log)
g003_time = time.time() - t3

g003_result = {
    'task': 'G-003',
    'params': {'write_debug_layers': HcsdParams.write_debug_layers},
    'files': paths,
    'verify': verify,
    'elapsed_s': round(g003_time, 3),
}
with open(os.path.join(EVID_DIR, 'G-003', 'g003_result.json'), 'w', encoding='utf-8') as f:
    json.dump(g003_result, f, indent=2, default=json_default, ensure_ascii=False)
log.info('G-003 完成: hcsd=%s (%.3fs)' % (os.path.basename(paths['hcsd']), g003_time))

# ============================================================================
# 汇总
# ============================================================================
total_time = time.time() - t0
log.info('')
log.info('=' * 70)
log.info('Pipeline 全部完成: load=%.3fs G001=%.3fs G002=%.3fs G003=%.3fs total=%.3fs' % (
    load_time, g001_time, g002_time, g003_time, total_time))
log.info('=' * 70)

summary = {
    'pipeline': 'G-001~G-003',
    'data': 'D-001 三片 Red HISS (GalaxyCenter) + E-003 加性共识曲面',
    'timestamp': datetime.now().isoformat(),
    'timings': {
        'load_s': round(load_time, 3),
        'g001_s': round(g001_time, 3),
        'g002_s': round(g002_time, 3),
        'g003_s': round(g003_time, 3),
        'total_s': round(total_time, 3),
    },
    'g001': g001_result,
    'g002': g002_result,
    'g003': g003_result,
}
with open(os.path.join(EVID_DIR, 'G-003', 'g_pipeline_summary.json'), 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, default=json_default, ensure_ascii=False)
print('DONE: G-001~G-003 results saved to engineering_authoritative/evidence/G-001~G-003/')
print('HCSD files saved to output/G-F/')
