import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '02_historical_seam', 'seam_semantic_diff.csv')
rows = list(csv.DictReader(open(P, encoding='utf-8')))
keys = list(rows[0].keys())
new_row = ['83471979a1dd778b4e557a9c7a92e22c137107f3', '535e73879662346ee1f599d7a9cae96c6c23680d', 'lib/phase2', 'lib/phase2/src/upm.cpp', 'use_ivar_weight default added (=1, ivar science weights ON)', 'weighting_policy_change',
 'C adds cfg.use_ivar_weight = 1 default (control-ivar based science weights, SCI-UPM-WEIGHT-001); A and B do not set this default (legacy snr^2/(1+snr^2)/unc^2 path was the implicit default)',
 'VERIFIED: C upm.cpp L231 cfg.use_ivar_weight=1; absent at A and B (git show upm.cpp grep); consistent with weight_mode auto default 0->2 (B->C stage2_common.cpp)',
 'weighting semantics change at C: control-ivar is the production default, legacy snr-based only for ablation/diagnosis; directly affects UPM numeric output between B and C',
 'SEAM_RELEVANT_IVAR_WEIGHT_DEFAULT_B_to_C',
 'ivar science weights become default at C',
 'git show C:upm.cpp L231 vs A/B absent; matches weight_mode flip 0->2'],
rows.append(dict(zip(keys, new_row)))
with open(P, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print('seam_semantic_diff.csv rows:', len(rows))
