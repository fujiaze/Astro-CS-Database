#!/usr/bin/env python3
"""Build the control->evidence coverage map (every § requirement to evidence file + status)."""
import csv, os

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package")

# section -> (primary evidence files, status, notes)
rows = [
["S2","Identity (HEAD==main==origin/main, branch, porcelain)","DONE","00_identity/head_equality.json + repository_identity.json","verified every round; external AGENTS.md change documented in WORKTREE_CHANGE_NOTE.md"],
["S3","Freeze (git archive/bundle/inventory)","DONE","01_repository/main-source.tar.gz + main-history.bundle + source_inventory.csv","tar -tzf OK (2717); bundle verify complete history; refs/heads/main == HEAD"],
["S4","Seam anchors A/B/C + semantic diff","DONE","02_historical_seam/seam_semantic_diff.csv (3098 rows) + SEAM_SEMANTIC_SUMMARY.md","11 verified facts; 3 stability confirmations"],
["S5","32R data sync (Fatduck) + per-file sha256 + FITS parse","DONE","03_testdata/t4_manifest_remote.csv + testdata_manifest.csv + masters_xisf_manifest.json","32 R frames + 3 masters all sha256-verified; WCS/XISF headers parsed"],
["S6","Clean Linux build layers","DONE(partial)","04_build/build_matrix.csv (20 rows)","9 modules build with workarounds; ACR/orchestrator/anchor-A/B FAIL (Windows source); star_detector GSL dep"],
["S7","Test matrix","DONE(partial)","05_tests/TEST_EXECUTION_MATRIX_DETAIL.md + test_inventory_full.csv","phase2 90 ctest (79/1/10); ipv 10/10; calibration 26/26; ASan 7/7; other modules BLOCKED-not-built"],
["S8","Checker truthfulness","DONE","06_checker_truthfulness/checker_truthfulness.csv (12 checkers + aggregator)","every checker probed with positive/negative/mutation; false negatives documented"],
["S9.1","API cross-layer (422-row contract)","DONE(signatures)/NOT_RUN(semantics)","07_cross_layer/clang_ast_api_comparison_full.json + API_SEMANTIC_TABLE_*.md","402/405 param-count match to AST; semantic tables for phase2/AIO/calibration/snr; full 422 semantic completion NOT_RUN"],
["S9.2","Cross-layer matrix","DONE(partial)","07_cross_layer/cross_layer_matrix.csv (67 rows)","1 VERIFIED_AT_SOURCE, 3 CONTRADICTION, 63 PARTIAL"],
["S10","Science oracles","DONE","08_science_oracles/ (7 oracles)","constant-field, Huber, Drizzle S=F/D, complexity (5.3-7.1e3x), frozen constants, pixfrac; reverse-drizzle INCONCLUSIVE"],
["S11","Execution static + OMP wiring","DONE","09_execution/ (4 files)","OPENMP_WIRING_FALSE (phase2 serial, runtime 1 thread); drizzle module OpenMP active (threads=2); ACR runtime NOT-RUN (CUDA stub)"],
["S12","32R A/B/C experiments","BLOCKED","10_real_32R/BLOCKED_NOTE.md + 03_testdata/GAIA_BLOCKER.md","Gaia ~107GB unavailable; anchor A/B Windows-only source; real frames uncalibrated (photscal gate); orchestrator Windows-only"],
["S13","Seam metrics","BLOCKED(real data)","11_seam/seam_metric_tool.py (reference tool shipped)","tool defines §13 metrics 1-9; metrics 1-2 computed on provided data; real 32R data needed for 3-9"],
["S14","Performance","DONE(synthetic)/BLOCKED(32R)","12_performance/PROFILE_NOTES.md + stage2_alloc_churn_estimate.json","drizzle 3-point scaling (0.46/8.13/34.8s); G2 30.7s/99%/366MB; alloc churn ~16M allocs; 32R profile BLOCKED"],
["S15","Static quality + large artifacts","DONE","13_static_quality/ (7 files) + 15_large_artifact_manifest/","markers~0, magic numbers 3138, doc refs, symbol/ABI, orphan headers, thread model, traceability, BASS audit"],
["S16","Package (empty files, sha256sum, tar)","DONE","MANIFEST.json + SHA256SUMS + EMPTY_FILES_NOTE.md","24 empty files documented; 172 sha256 all OK; manifest excludes self"],
["S17","Findings","DONE","14_findings/findings.csv (27 rows)","P0:3 P1:7 P2:15 P3:2"],
["S18","Final gates","DONE","00_READ_FIRST.md (verdict EVIDENCE_INCOMPLETE)","git clean (only external AGENTS.md + 2 input docs); no commits/pushes; verdict EVIDENCE_INCOMPLETE (32R blocked)"],
]
with open(os.path.join(P, "control_coverage_map.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["control_section","requirement","status","evidence","notes"])
    w.writerows(rows)

from collections import Counter
c = Counter(r[2].split("(")[0] for r in rows)
print("coverage map written:", os.path.join(P, "control_coverage_map.csv"))
print("by status:", dict(c))
