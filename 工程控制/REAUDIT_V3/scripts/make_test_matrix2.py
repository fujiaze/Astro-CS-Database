#!/usr/bin/env python3
"""Build test_execution_matrix.csv v1: phase2 module from real ctest run (robust parser)."""
import csv, os, collections
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
logpath = os.path.join(ROOT, "logs", "phase2_ctest_run.log")
OUT = os.path.join(ROOT, "package", "05_tests")
os.makedirs(OUT, exist_ok=True)

fields = ["test_id","path","module","type","build_id","platform","backend","precision","data_kind","command","timeout_s","exit_code","status","skip_reason","oracle","tolerance","seed","duration_s","peak_rss_kb","log_path","log_sha256"]
rows = []

for line in open(logpath, encoding="utf-8", errors="ignore"):
    line = line.rstrip(chr(10))
    if "Test #" not in line or "/90 Test" not in line:
        continue
    if "***Skipped" in line:
        status, ec, reason = "SKIP", "", "requires CUDA bridge or real HiPS data unavailable on Linux 2C2G"
    elif "***Failed" in line:
        status, ec, reason = "FAIL", "32512", "test invokes astrocs-stage2.exe (Windows exe name) which does not exist on Linux; sh: not found rc=32512"
    elif "Passed" in line:
        status, ec, reason = "PASS", "0", ""
    else:
        continue
    toks = line.split()
    tid = "UNKNOWN"
    for i, t in enumerate(toks):
        if t.startswith("#") and t.endswith(":"):
            tid = toks[i+1] if i+1 < len(toks) else "UNKNOWN"
            break
    dur = "0"
    for i, t in enumerate(toks):
        if i+1 < len(toks) and toks[i+1] == "sec" and t.replace(".", "").isdigit():
            dur = t
            break
    rows.append({"test_id": tid,
                 "path": "lib/phase2/tests/ivar_wiring_test.cpp" if tid.startswith("Phase2IvarWiring") else "lib/phase2/tests/synthetic_gate.cpp",
                 "module": "phase2", "type": "gtest", "build_id": "phase2_clean_release_ompOFF",
                 "platform": "linux-x86_64", "backend": "cpu" if status in ("PASS","FAIL") else "cpu/cuda",
                 "precision": "fp64", "data_kind": "synthetic",
                 "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                 "exit_code": ec, "status": status, "skip_reason": reason,
                 "oracle": "in-test oracle" if status == "PASS" else "", "tolerance": "in-test" if status=="PASS" else "",
                 "seed": "not recorded in ctest" if status == "PASS" else "",
                 "duration_s": dur, "peak_rss_kb": "", "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})

with open(os.path.join(OUT, "test_execution_matrix.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
c = collections.Counter(r["status"] for r in rows)
print("test_execution_matrix.csv rows=" + str(len(rows)) + " statuses=" + str(dict(c)))
