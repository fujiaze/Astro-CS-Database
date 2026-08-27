import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "02_historical_seam", "seam_semantic_diff.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
A = "b38b446e63d0d27eac672b85ce30527399a057fc"
B = "83471979a1dd778b4e557a9c7a92e22c137107f3"
C = "535e73879662346ee1f599d7a9cae96c6c23680d"
new_row = [A + "|" + B + "|" + C, "lib/phase2/src/upm.cpp p2_upm_calibrate_block (B:L884 vs C:L1055)", "B->C: unknown-frame calibration changed from SILENT FALLBACK to explicit rc=1",
 "frame_bind_guard_introduced",
 "B (upm.cpp:892): fi = (it != end) ? it->second : 0 - an unknown frame_id silently uses FRAME-0 parameters and returns 0. C (upm.cpp:1064-1067): unknown frame_id -> return 1 with output untouched, comment '未知 frame_id 必须显式失败，禁止回退 frame 0 参数' (ALG-UPM-FRAME-BIND-001)",
 "VERIFIED by reading both anchors' calibrate_block; executable oracle 10.30 pins the C behavior",
 "SEAM-CRITICAL: any B-vs-C calibration comparison that hits a missing frame measures DIFFERENT semantics - at B the result is silently wrong science (frame-0 correction applied), at C it is an explicit error",
 "FRAME_BIND_GUARD_IS_C_ONLY",
 "unknown-frame handling differs across anchors",
 "git show upm.cpp at A/B/C; oracle 10.30"]
rows.append(dict(zip(keys, new_row)))
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("rows:", len(rows))
