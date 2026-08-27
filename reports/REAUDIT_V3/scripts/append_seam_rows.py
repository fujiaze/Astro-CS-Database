import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '02_historical_seam', 'seam_semantic_diff.csv')
rows = list(csv.DictReader(open(P, encoding='utf-8')))
keys = list(rows[0].keys())
NA = 'NOT_YET_ASSESSED'
new_rows = [
['83471979a1dd778b4e557a9c7a92e22c137107f3', '535e73879662346ee1f599d7a9cae96c6c23680d', 'lib/phase2', 'lib/phase2/src/stage2_common.cpp', 'weight_mode auto default: 0 (support_x_snr2) -> 2 (ivar)', 'config_default_changed',
 'weight policy of integration changes to ivar (control-ivar based) at C',
 'VERIFIED: B L225-229 auto->0 (support_x_snr2); C L373-380 auto->2 (ivar) - default semantic change',
 'changes per-frame integration weights; ACR block path additionally gated to weight_mode!=2',
 'POTENTIAL_REGRESSION_ANCHOR for seam/UPM numeric behavior between B and C',
 'weight-mode default flip alters integration weights',
 'git grep weight_mode at B vs C; stage2_common.cpp L225-229 (B), L373-380 (C)'],
['83471979a1dd778b4e557a9c7a92e22c137107f3', '535e73879662346ee1f599d7a9cae96c6c23680d', 'lib/phase2', 'lib/phase2/tools/stage2.cpp', 'use_acr_block gate weight_mode!=2', 'gate_condition',
 'ACR block path disabled for ivar weight (production default) -> CPU canonical path',
 'VERIFIED: stage2.cpp L718-724 use_acr_block requires rplan.method==P2_REJECT_SIGMA && acr_route!=cpu && !large_scale && weight_mode!=2',
 'production default (weight_mode=auto->ivar) never takes ACR block -> serial CPU integration',
 'NOT_A_REGRESSION_BUT_EXECUTION_CONTRACT_FALSE (docs claim ACR/Mixed reachable)',
 'ivar weight mode not ACR-equivalent by design',
 'stage2.cpp L715-737 at C'],
['b38b446e63d0d27eac672b85ce30527399a057fc', '83471979a1dd778b4e557a9c7a92e22c137107f3', 'lib/phase2', 'lib/phase2/src/sampler.cpp', 'background-clean config keys added', 'config_key_added',
 'DBE-like background-clean sampling introduced at B',
 'VERIFIED: config keys background_max_contamination, background_contamination_sigma, background_min_retained_fraction, background_neighbor_radius, cs.ra, cs.tile added (hunk index)',
 'sampler control candidate selection changes (53376 -> 9216 clean obs per commit msg)',
 'POTENTIAL_REGRESSION_ANCHOR: control geometry/clean-set differ A vs B',
 'sampler background-clean changes control obs set',
 'hunk_symbol_index.csv A_b38b446->B_8347197 config_key_added rows'],
]
for nr in new_rows:
    rows.append(dict(zip(keys, nr)))
with open(P, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print('seam_semantic_diff.csv rows:', len(rows))
