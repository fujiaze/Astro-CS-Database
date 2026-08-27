import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "02_historical_seam", "seam_semantic_diff.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
A = "b38b446e63d0d27eac672b85ce30527399a057fc"
B = "83471979a1dd778b4e557a9c7a92e22c137107f3"
C = "535e73879662346ee1f599d7a9cae96c6c23680d"
new_row = [A + "|" + B + "|" + C, "lib/phase2/src/upm.cpp model_hash payload (B:L~640 vs C:L~720)", "B->C: model_hash payload gained the use_ivar_weight field",
 "hash_payload_field_added",
 "The model_hash payload is otherwise identical (cfg params, input_manifest_hash, frame ids, controls M, cell_index T, C values) but C inserts std::to_string(cfg.use_ivar_weight) into the hashed parameter block; B does not hash this switch",
 "VERIFIED by side-by-side git show of the hash-payload construction at both anchors",
 "Consequence: identical science content built with use_ivar_weight=0 vs 1 gets DIFFERENT model_hash at C (correct - the weighting mode changes the science); at B the ablation/production switch is invisible to the hash. Cross-anchor hash comparison of models built with different weight modes is meaningless by design",
 "HASH_PAYLOAD_INCLUDES_WEIGHT_MODE_C_ONLY",
 "model_hash coverage differs across anchors",
 "git show upm.cpp at A/B/C; oracle 10.24"]
rows.append(dict(zip(keys, new_row)))
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("rows:", len(rows))
