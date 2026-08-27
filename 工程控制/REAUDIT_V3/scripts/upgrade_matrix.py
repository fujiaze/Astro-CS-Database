import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "07_cross_layer", "cross_layer_matrix.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
hdr = rows[0]
# locate column indices
ci = {c: i for i, c in enumerate(hdr)}
updates = {
  "SCI-UPM-WEIGHT-001": ("VERIFIED_AT_SOURCE",
    "raw_w=quality_factor x control_ivar verified NUMERICALLY at executable level (oracle 10.22 zero/NaN/Inf/neg rejection + ivar 1:4 -> 1:4 EXACT, UPMW-001/002/006 7/7 PASS round 68; model_hash determinism 10.24)"),
  "ALG-UPM-FRAME-BIND-001": ("VERIFIED_AT_SOURCE",
    "frame-bind guard verified at executable level (oracle 10.30: unknown frame rc=1 output untouched) + seam fact #13 (B silently fell back to frame-0 params, C fails explicitly)"),
  "SCI-UPM-PERSIST-001": ("VERIFIED_AT_SOURCE",
    "persistence round-trip lossless verified (oracle 10.25: evaluate/calibrate identical + hash stable) + CROSS-PLATFORM (oracle 10.41: Fatduck-Windows model opens on Linux, hash matches EXACTLY)"),
  "ENG-THREAD-001": ("VERIFIED_AT_SOURCE",
    "thread-safety verified at runtime (oracle 10.39: 8 threads x 200k iters evaluate_c/raw_weight, 0 mismatches/0 NaN) complementing THREAD_MODEL_VERIFICATION.md"),
  "ALG-UPM-CONTROL-IVAR-001": ("VERIFIED_AT_SOURCE",
    "control_ivar contract verified on REAL 5-frame sampler run (oracle 10.32: 51,402 accepted obs all with finite positive control_ivar; k_corr MC test UPMW-005 in 7/7 Phase2Weight PASS; real 2-frame E2E model C finite 10.33)"),
  "DATA-UPM-CONTROL-UNC-001": ("VERIFIED_AT_SOURCE",
    "real sampler accepted-observation sanity: value finite + control_ivar>0 on real gc 5-frame run (oracle 10.32); production stage2 real UPM models all have positive control_ivar (10.35-10.36)"),
  "DATA-UPM-MODEL-001": ("VERIFIED_AT_SOURCE",
    "production-persisted model reopens with hash matching diagnostics EXACTLY (oracle 10.36: 95c4236d...); cross-platform stable (10.41: 866de8ef... identical on Linux)"),
}
n_up = 0
for r in rows[1:]:
    sid = r[ci["sci_id"]] if len(r) > ci["sci_id"] else ""
    if sid in updates:
        st, ev = updates[sid]
        r[ci["consistency_status"]] = st
        r[ci["evidence"]] = ev
        r[ci["current_test_result"]] = "VERIFIED at executable + real-data level this audit"
        n_up += 1
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[ci["consistency_status"]] for r in rows[1:])
print("rows:", len(rows)-1, "upgraded:", n_up)
print("status:", dict(c))
