import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
seen = set(r["layer"] for r in rows)
nr = ["ipv-plate_solve-linux","Makefile (lib/plate_solve/cpp/ipv)","PASS(direct g++) / FAIL(document make)","logs/ipv_build.log + ipv_build_direct.log + ipv_kvector_run.log","doc=2(shell syntax); direct=0",
 "Makefile uses Windows cmd.exe shell rules (@if exist ... del /q; @if not exist ... mkdir) that sh cannot parse, plus -lkernel32 link; direct g++ (drop -lkernel32, -fPIC) builds ipv_solver.dll ELF so sha256 625e335b...; k-vector unit test 10/10 PASS on Linux"]
if nr[0] not in seen:
    rows.append(dict(zip(keys, nr)))
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("build_matrix rows:", len(rows))
