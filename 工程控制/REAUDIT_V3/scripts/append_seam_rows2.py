import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '02_historical_seam', 'seam_semantic_diff.csv')
rows = list(csv.DictReader(open(P, encoding='utf-8')))
keys = list(rows[0].keys())
new_rows = [
['b38b446e63d0d27eac672b85ce30527399a057fc', '83471979a1dd778b4e557a9c7a92e22c137107f3', 'lib/phase2', 'lib/phase2/src/upm.cpp', 'control-node set: obs-driven only -> full coverage geometry incl. single-frame regions (V13)', 'node_topology_change',
 'A collects control nodes ONLY from observations (for i in n_obs loop over obs); B adds V13 nodes from full coverage geometry incl. single-frame regions; obs provide data items only',
 'VERIFIED: A upm.cpp L235-245 (obs-only loop); B upm.cpp L241-249 (V13 comment + node-first collection); comment at B L241 absent at A',
 'the UPM control-node set/geometry differs between A and B/C - single-frame-region nodes exist only at B/C; directly affects seam experiment and model hash',
 'SEAM_RELEVANT_NODE_TOPOLOGY_CHANGED_A_to_B',
 'single-frame regions become explicit control nodes only at B',
 'git show A:upm.cpp vs B:upm.cpp around control collection; V13 comment at B L241'],
['83471979a1dd778b4e557a9c7a92e22c137107f3', '535e73879662346ee1f599d7a9cae96c6c23680d', 'lib/phase2', 'lib/phase2/src/upm.cpp', 'unobserved_geometry_nodes counter added (sentinel no-gauge bookkeeping)', 'bookkeeping_added',
 'C counts unobserved geometry nodes separately (m->unobserved_geometry_nodes, kNoData sentinel, excluded from reference-frame gauge); B does not have this counter',
 'VERIFIED: grep unobserved_geometry_nodes -> C=present (upm.cpp L430, L465), B=0 matches, A=0 matches',
 'bookkeeping only; the underlying all-frame-weighted M for no-data nodes existed (A/B comments) but was not counted explicitly',
 'MINOR_REFINEMENT (not a semantic flip)',
 'explicit accounting of geometry-only nodes',
 'git grep unobserved_geometry_nodes at A/B/C'],
]
for nr in new_rows:
    rows.append(dict(zip(keys, nr)))
with open(P, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print('seam_semantic_diff.csv rows:', len(rows))
