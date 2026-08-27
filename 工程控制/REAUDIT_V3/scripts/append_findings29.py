import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_row = ['P2-13','P2','CONFIRMED','portability','plate_solve/ipv','lib/plate_solve/cpp/ipv/Makefile','69-71','Windows cmd.exe shell syntax in Makefile',
 'documented make builds ipv_solver.dll on Linux',
 'Makefile recipe lines use cmd.exe syntax (@if exist ... del /q; @if not exist ... mkdir) which /bin/sh cannot parse (make fails with shell syntax error), and links -lkernel32; a direct g++ build (drop -lkernel32, -fPIC) succeeds -> ELF so sha256 625e335b..., and the k-vector unit test runs 10/10 PASS on Linux',
 'documented command works',
 'make (logs/ipv_build.log shell error) + direct g++ (logs/ipv_build_direct.log) + test (logs/ipv_kvector_run.log)',
 'clean-clone reproducibility of ipv FAIL without Makefile rewrite',
 'rewrite recipe lines in portable sh; guard -lkernel32',
 'rebuild from fresh archive',
 '04_build/build_matrix.csv ipv-plate_solve-linux']
if new_row[0] not in seen: rows.append(new_row)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' by_severity=' + str(dict(c)))
