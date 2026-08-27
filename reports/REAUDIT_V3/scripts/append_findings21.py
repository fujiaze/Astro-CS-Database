import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_rows = [
['P2-11','P2','CONFIRMED','portability','snr_estimator','lib/snr_estimator/cpp/Makefile','12','-static in -shared link breaks Linux',
 'documented make builds snr_estimator.dll on Linux',
 'Makefile link line `-shared -o snr_estimator.dll ... -static` fails on Linux (crtbeginT.o relocation R_X86_64_32 against __TMC_END__); dropping -static builds OK -> ELF so sha256 e2765c6e...',
 'documented command works',
 'make (logs/snr_build.log) + workaround (logs/snr_build_work.log)',
 'clean-clone reproducibility of snr_estimator FAIL without Makefile edit',
 'remove -static (it is a Windows static-RT flag); use Linux-friendly link',
 'rebuild from fresh archive',
 '04_build/build_matrix.csv snr_estimator-linux']
]
for nr in new_rows:
    if nr[0] not in seen: rows.append(nr)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' by_severity=' + str(dict(c)))
