#!/usr/bin/env python3
"""Build test_execution_matrix.csv v1: phase2 module from real ctest run; other modules BLOCKED-not-built."""
import re, csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
LOG = open(os.path.join(ROOT, "logs", "phase2_ctest_run.log"), encoding="utf-8", errors="ignore").read()
OUT = os.path.join(ROOT, "package", "05_tests")
os.makedirs(OUT, exist_ok=True)

fields = ["test_id","path","module","type","build_id","platform","backend","precision","data_kind","command","timeout_s","exit_code","status","skip_reason","oracle","tolerance","seed","duration_s","peak_rss_kb","log_path","log_sha256"]
rows = []

# parse ctest log lines like: "1/90 Test #1: Phase2Upm.S0IdentityCalibrationNoChange ...........   Passed    0.01 sec"
pat = re.compile(r"^(\d+)/90 Test\s+#\d+: (\S+)\s+\.+\s+(\S+)\s+([0-9.]+) sec")
failpat = re.compile(r"^(\d+)/90 Test\s+#\d+: (\S+)\s+\.+\s+\*\*\*Failed\s+([0-9.]+) sec")
skippat = re.compile(r"^(\d+)/90 Test\s+#\d+: (\S+)\s+\.+\s+\*\*\*Skipped\s+([0-9.]+) sec")
for line in LOG.splitlines():
    m = pat.match(line.strip())
    if m:
        tid = m.group(2); status = m.group(3); dur = m.group(4)
        rows.append({"test_id": tid, "path": "lib/phase2/tests/synthetic_gate.cpp or ivar_wiring_test.cpp",
                     "module": "phase2", "type": "gtest", "build_id": "phase2_clean_release_ompOFF",
                     "platform": "linux-x86_64", "backend": "cpu", "precision": "fp64", "data_kind": "synthetic",
                     "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                     "exit_code": 0 if status == "Passed" else 1, "status": "PASS" if status=="Passed" else "FAIL",
                     "skip_reason": "", "oracle": "in-test oracle", "tolerance": "in-test", "seed": "not_recorded_in_ctest",
                     "duration_s": dur, "peak_rss_kb": "", "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})
        continue
    m = skippat.match(line.strip())
    if m:
        tid = m.group(2); dur = m.group(3)
        rows.append({"test_id": tid, "path": "lib/phase2/tests/synthetic_gate.cpp",
                     "module": "phase2", "type": "gtest", "build_id": "phase2_clean_release_ompOFF",
                     "platform": "linux-x86_64", "backend": "cpu/cuda", "precision": "fp64", "data_kind": "real/cuda",
                     "command": "ctest --test-dir builds/phase2_clean_release_ompOFF -j1", "timeout_s": 2400,
                     "exit_code": "", "status": "SKIP", "skip_reason": "requires CUDA bridge or real HiPS data unavailable on Linux 2C2G",
                     "oracle": "", "tolerance": "", "seed": "", "duration_s": dur, "peak_rss_kb": "",
                     "log_path": "logs/phase2_ctest_run.log", "log_sha256": ""})

# the one failing test: mark it
for r in rows:
    if r["test_id"] == "Phase2IvarWiring.WireProductionStage2PerFrameIvar" and r["status"] != "FAIL":
        r["status"] = "FAIL"
        r["skip_reason"] = ""
        r["exit_code"] = "32512"
        r["command"] = "ctest ... (test invokes astrocs-stage2.exe which does not exist on Linux)"

with open(os.path.join(OUT, "test_execution_matrix.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
from collections import Counter
c = Counter(r["status"] for r in rows)
print("test_execution_matrix.csv rows=" + str(len(rows)) + " statuses=" + str(dict(c)))
