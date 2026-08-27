import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '02_historical_seam', 'seam_semantic_diff.csv')
rows = list(csv.DictReader(open(P, encoding='utf-8')))
keys = list(rows[0].keys())
new_row = ['b38b446e63d0d27eac672b85ce30527399a057fc', '83471979a1dd778b4e557a9c7a92e22c137107f3', 'lib/phase2', 'lib/phase2/src/upm.cpp', 'Huber standardization: raw-residual -> standardized z=r/sigma_eff (sigma_eff=max(uncertainty,sigma_floor))', 'science_weighting_change',
 'A uses huber_w(r, delta) on the RAW residual (delta=1.345 directly compared to ~0.002 raw residual -> all residuals in linear region, robust almost never engages, per the B comment); B introduces sigma_eff standardization huber_w(r/sigma_eff, delta) with dimensionless delta',
 'VERIFIED: A upm.cpp L497 huber_w(r, ...); B upm.cpp L519-528 (comment documents the fix + huber_w(r/sigma_eff, delta)); sigma_eff present at B (6) but absent at A (0)',
 'MAJOR science-semantic change between anchors: robust weighting effectively inert at A, engages at B - directly relevant to the A/B/C seam experiment and to the doc contradiction (docs claim delta=1.345*median_abs_r, source uses dimensionless 1.345 on z)',
 'SEAM_RELEVANT_HUBER_STANDARDIZATION_A_to_B',
 'robust weighting semantics differ between A and B',
 'git show A:upm.cpp L497 vs B:upm.cpp L519-528; sigma_eff count 0->6'],
rows.append(dict(zip(keys, new_row)))
with open(P, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print('seam_semantic_diff.csv rows:', len(rows))
