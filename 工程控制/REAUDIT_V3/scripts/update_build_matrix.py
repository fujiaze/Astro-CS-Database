import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
p = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
rows = list(csv.DictReader(open(p, encoding="utf-8")))
keys = list(rows[0].keys())
seen = set(r["layer"] for r in rows)
new = [
  ["phase2-subproject-linux-debug", "CMake (lib/phase2)", "PASS", "logs/phase2_dbg_cfg.log + phase2_dbg_build.log", "0", "Debug build; 0 warnings/0 errors; astrocs-stage2 8629328 bytes"],
  ["phase2-subproject-asan-ubsan", "CMake (lib/phase2)", "PASS(build) / PASS(7 sanitizer tests)", "logs/phase2_asan_cfg.log + phase2_asan_build.log + 05_tests/asan_ubsan/", "0", "-fsanitize=address,undefined; 7 synthetic_gate tests PASS, 0 ASan/UBSan errors; LSAN blocked by ptrace sandbox (documented)"],
]
for nr in new:
    if nr[0] not in seen:
        rows.append(dict(zip(keys, nr)))
with open(p, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("build_matrix rows:", len(rows))
