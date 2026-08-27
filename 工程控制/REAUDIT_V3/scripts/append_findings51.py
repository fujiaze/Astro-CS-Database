import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
new_row = ["P2-16","P2","CONFIRMED","code_duplication","multi-module","lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp + ipv_wcs.cpp + dynamic_psf + star_detector + astro_image_io","21 groups","Duplicate code: identical non-trivial function bodies across modules",
 "shared library helpers are used",
 "21 exact-duplicate body groups (>=200 chars) across 1488 functions: Gauss solve (ipv), bilinear resample (dynamic_psf/star_detector), UTF-8 fopen (astro_image_io x3), make_estimate (ACR x3), test helpers",
 "no duplication",
 "scripts/dup_code.py",
 "copy-paste maintenance/debug-divergence risk; scientific helpers not shared",
 "extract shared helpers",
 "re-run after refactor",
 "13_static_quality/DUPLICATE_CODE_NOTE.md"]
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
