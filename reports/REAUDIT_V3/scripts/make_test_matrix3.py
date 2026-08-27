#!/usr/bin/env python3
"""Rebuild test_execution_matrix.csv from accurate name lists extracted from ctest log."""
import csv, os, collections
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "05_tests")
os.makedirs(OUT, exist_ok=True)

fields = ["test_id","path","module","type","build_id","platform","backend","precision","data_kind","command","timeout_s","exit_code","status","skip_reason","oracle","tolerance","seed","duration_s","peak_rss_kb","log_path","log_sha256"]
rows = []

passed = [l.strip() for l in open("/tmp/passed_names.txt") if l.strip()]
skipped = [l.strip() for l in open("/tmp/skip_names.txt") if l.strip()]
failed = [l.strip() for l in open("/tmp/fail_names.txt") if l.strip()]

for tid in passed:
    rows.append({"test_id": tid, "path": "lib/phase2/tests/synthetic_gate.cpp", "module": "phase2",
                 "type": "gtest", "build_id": "phase2_clean_release_ompOFF", "platform": "linux-x86_64",
                 "backend": "cpu", "precision": "fp64", "data_kind": "synthetic",
                 "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                 "exit_code": "0", "status": "PASS", "skip_reason": "",
                 "oracle": "in-test oracle", "tolerance": "in-test", "seed": "not recorded in ctest",
                 "duration_s": "", "peak_rss_kb": "", "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})
for tid in skipped:
    rows.append({"test_id": tid, "path": "lib/phase2/tests/synthetic_gate.cpp", "module": "phase2",
                 "type": "gtest", "build_id": "phase2_clean_release_ompOFF", "platform": "linux-x86_64",
                 "backend": "cpu/cuda", "precision": "fp64", "data_kind": "real/cuda",
                 "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                 "exit_code": "", "status": "SKIP",
                 "skip_reason": "requires CUDA bridge or real HiPS data unavailable on this Linux 2C2G host",
                 "oracle": "", "tolerance": "", "seed": "", "duration_s": "", "peak_rss_kb": "",
                 "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})
for tid in failed:
    rows.append({"test_id": tid, "path": "lib/phase2/tests/ivar_wiring_test.cpp", "module": "phase2",
                 "type": "gtest", "build_id": "phase2_clean_release_ompOFF", "platform": "linux-x86_64",
                 "backend": "cpu", "precision": "fp64", "data_kind": "synthetic",
                 "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                 "exit_code": "32512", "status": "FAIL",
                 "skip_reason": "test invokes astrocs-stage2.exe (Windows exe name) which does not exist on Linux; sh: not found rc=32512",
                 "oracle": "", "tolerance": "", "seed": "", "duration_s": "", "peak_rss_kb": "",
                 "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})

with open(os.path.join(OUT, "test_execution_matrix.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
c = collections.Counter(r["status"] for r in rows)
print("test_execution_matrix.csv rows=" + str(len(rows)) + " statuses=" + str(dict(c)))
