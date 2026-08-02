#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R06 FAST 实验数据分析 - 生成 markdown 报告表格"""
import json
from collections import defaultdict

runs = []
cmps = []
with open('run/logs/r06_fast_exp_run_stdout.jsonl', 'r', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        obj = json.loads(line)
        if obj.get('event') == 'run' and not obj.get('warmup'):
            runs.append(obj)
        elif obj.get('event') == 'comparison':
            cmps.append(obj)

scale_stats = defaultdict(lambda: {
    'p_wall': 0, 'f_wall': 0, 'count': 0,
    'max_sig_mae': 0, 'max_sig_max': 0, 'max_sup_mae': 0,
    'misses': 0, 'speedups': []
})
for c in cmps:
    s = c['source_pixel_scale_arcsec']
    st = scale_stats[s]
    st['p_wall'] += c['precise_wall_ms']
    st['f_wall'] += c['fast_wall_ms']
    st['count'] += 1
    st['max_sig_mae'] = max(st['max_sig_mae'], c['signal_mae'])
    st['max_sig_max'] = max(st['max_sig_max'], c['signal_max_abs'])
    st['max_sup_mae'] = max(st['max_sup_mae'], c['support_mae'])
    st['misses'] += c['candidate_misses']
    if c['precise_wall_ms'] > 0 and c['fast_wall_ms'] > 0:
        st['speedups'].append(c['precise_wall_ms'] / c['fast_wall_ms'])

run_flux = defaultdict(lambda: {'p_err': 0, 'f_err': 0})
for r in runs:
    s = r['source_pixel_scale_arcsec']
    if r['mode'] == 'PRECISE':
        run_flux[s]['p_err'] = max(run_flux[s]['p_err'], r['flux_rel_error'])
    else:
        run_flux[s]['f_err'] = max(run_flux[s]['f_err'], r['flux_rel_error'])

print('| Scale(arcsec) | NSIDE | Cases | Precise wall(ms) | Fast wall(ms) | Mean speedup | Max sig_mae | Max sig_max | Max sup_mae | Max flux_err(P) | Max flux_err(F) | Misses |')
print('|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
nside_map = {0.1: 4194304, 0.5: 524288, 1.0: 262144, 10.0: 32768, 60.0: 4096, 3600.0: 64}
for s in sorted(scale_stats.keys()):
    st = scale_stats[s]
    mean_sp = sum(st['speedups']) / len(st['speedups']) if st['speedups'] else 0
    ns = nside_map.get(s, '?')
    print(f'| {s} | {ns} | {st["count"]} | {st["p_wall"]:.1f} | {st["f_wall"]:.1f} | {mean_sp:.2f} | {st["max_sig_mae"]:.6g} | {st["max_sig_max"]:.6g} | {st["max_sup_mae"]:.6g} | {run_flux[s]["p_err"]:.6g} | {run_flux[s]["f_err"]:.6g} | {st["misses"]} |')

print()
total_p = sum(st['p_wall'] for st in scale_stats.values())
total_f = sum(st['f_wall'] for st in scale_stats.values())
all_sp = [sp for st in scale_stats.values() for sp in st['speedups']]
print(f'Total PRECISE wall: {total_p:.1f} ms')
print(f'Total FAST wall: {total_f:.1f} ms')
print(f'Overall speedup: {total_p / total_f:.2f}x')
print(f'Mean speedup: {sum(all_sp) / len(all_sp):.2f}x')
print(f'Min speedup: {min(all_sp):.2f}x')
print(f'Max speedup: {max(all_sp):.2f}x')
print(f'Total candidate misses: {sum(st["misses"] for st in scale_stats.values())}')
print(f'Total cases: {len(cmps)}')

# 高通量误差 case 详情
print()
print('## 高通量误差 case (flux_rel_error > 0.01)')
print('| Case | Mode | flux_rel_error | nside | scale |')
print('|---|---|---:|---:|---:|')
for r in runs:
    if r['flux_rel_error'] > 0.01:
        print(f'| {r["case_id"]} | {r["mode"]} | {r["flux_rel_error"]:.6g} | {r["nside"]} | {r["source_pixel_scale_arcsec"]} |')
