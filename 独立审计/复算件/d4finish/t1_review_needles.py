# -*- coding: utf-8 -*-
"""Task 1.1 ② refined-3 —— needle-by-needle membership of the concrete numeric
constants / tolerances / thresholds / powers named in the layer-2 review docs,
against the master table.  Prints ABSENT list used to decide added rows.
"""
import io, re, sys, csv

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
rows = [r for r in csv.reader(io.open(MASTER, encoding='utf-8-sig', newline=''))][1:]
rows = [r for r in rows if len(r) >= 16]

NEEDLES = {
    'reference_flux_adu': r'reference_flux_adu',
    'F_ref(参考通量)': r'F_ref|参考通量|reference flux',
    'snr_noise_model_v1 场模型': r'snr_noise_model_v1',
    'spatial_field 平面阶数': r'spatial_field',
    'a+b·x+c·y 一阶场': r'b\s*·\s*x|一次多项式|平面场|一阶场',
    'rel_step 出厂接缝门 1e-2': r'rel_step|seam_footprint',
    'excess 实验判据': r'off-?locus|rel_excess',
    'base_win=64': r'base_win',
    'halfwin': r'halfwin',
    'order=2 基线': r'order\s*=\s*2|二阶多项式|order 2',
    'converged 四态枚举': r'converged',
    'upm_converged manifest 键': r'upm_converged',
    'max_iter': r'max_iter|最大迭代',
    '1e-9 容差下限': r'1e-9|max\(tol',
    'reference_overlap MC 1%': r'reference_overlap',
    '逐 leaf 独立 MC 容差 4.1e-13': r'4\.1e-13|3\.0e-20',
    '质心 0.01 px 判据': r'0\.01\s*px|0\.063',
    'fixture bg=1200': r'1200',
    'sigma_field_fast 1.4826': r'1\.4826|sigma_field_fast',
    'rmse_log_rho': r'rmse_log_rho',
    'bilinear_upsample': r'bilinear_upsample',
    'REQUIRED_SELFTEST_CASES': r'REQUIRED_SELFTEST_CASES',
    'ULP 绝对容差 1e-6': r'ULP|ulp',
    'p95 质心容差': r'p95',
    'arc-chord 预算': r'arc-chord',
    'pixel_area_power': r'pixel_area_power',
    'leaf 阶数/N SIDE': r'NODE_SPACING',
}

absent = []
for name, pat in NEEDLES.items():
    rx = re.compile(pat, re.I)
    hits = [i for i, r in enumerate(rows) if any(rx.search(c) for c in r)]
    keys = [i for i, r in enumerate(rows) if rx.search(r[0])]
    tag = 'PRESENT' if hits else 'ABSENT '
    if not hits:
        absent.append(name)
    print('%s %-28s rows=%-4d keys=%-3d %s' % (tag, name, len(hits), len(keys),
          ('; '.join(rows[i][0][:36] for i in (keys or hits)[:5]))))
print('\nABSENT list (%d):' % len(absent))
for a in absent:
    print('  -', a)
