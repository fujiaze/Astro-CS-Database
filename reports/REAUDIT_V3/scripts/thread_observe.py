#!/usr/bin/env python3
"""Runtime thread observation: run a phase2 synthetic test and sample /proc/PID/status Threads + omp runtime."""
import subprocess, time, os, re, json

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
BIN = os.path.join(ROOT, "builds/phase2_clean_release_ompOFF/phase2_synthetic_gate")
OUT = os.path.join(ROOT, "package", "09_execution")

# launch a longer test (G1SpatialFieldTruth ~6s) so we can sample threads
cmd = [BIN, "--gtest_filter=Phase2Upm.G1SpatialFieldTruth:Phase2Upm.G2PersistenceAndHashSensitivity"]
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
samples = []
start = time.time()
while p.poll() is None and time.time() - start < 300:
    try:
        with open("/proc/%d/status" % p.pid) as f:
            for line in f:
                if line.startswith("Threads:"):
                    samples.append((round(time.time()-start,2), line.strip().split()[1]))
                    break
    except Exception:
        pass
    time.sleep(0.3)
out, _ = p.communicate(timeout=30)
threads = [int(s[1]) for s in samples]
result = {
    "command": " ".join(cmd),
    "exit_code": p.returncode,
    "thread_samples": samples,
    "max_threads": max(threads) if threads else None,
    "min_threads": min(threads) if threads else None,
    "distinct_thread_counts": sorted(set(threads)) if threads else None,
    "verdict": "SERIAL_1_THREAD" if threads and max(threads) == 1 else "MULTI_THREAD" if threads else "NO_SAMPLE",
}
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, "phase2_runtime_thread_observation.json"), "w").write(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
print("--- test stdout tail ---")
print(out[-600:] if out else "(no output)")
