import csv, os
ROOT = open('/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt').read().strip()
P = os.path.join(ROOT, 'package', '14_findings', 'findings.csv')
rows = list(csv.reader(open(P, encoding='utf-8')))
seen = set(r[0] for r in rows)
new_rows = [
['P2-12','P2','CONFIRMED','portability','gaia_xpsd_client','lib/gaia_xpsd_client/Makefile','17','-fPIC missing in shared build',
 'documented make builds gaia_client.dll on Linux',
 'Makefile shared-object build omits -fPIC -> R_X86_64_PC32 against stderr relocation error; adding -fPIC builds gaia_client.dll ELF so sha256 7131da01... (same class as astro_image_io P1-04)',
 'documented command works',
 'make (logs/gaia_build.log) + -fPIC workaround (logs/gaia_build_fpic.log)',
 'clean-clone reproducibility of gaia client FAIL without Makefile edit',
 'add -fPIC to shared build',
 'rebuild from fresh archive',
 '04_build/build_matrix.csv gaia_xpsd_client-linux']
]
for nr in new_rows:
    if nr[0] not in seen: rows.append(nr)
with open(P, 'w', newline='', encoding='utf-8') as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print('findings rows=' + str(len(rows)-1) + ' by_severity=' + str(dict(c)))
