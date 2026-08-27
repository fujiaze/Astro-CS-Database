import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
# count modules with missing-fPIC class
fpic = [r[0] for r in rows[1:] if "fPIC" in r[11] or "fPIC" in r[12]]
print("existing fPIC-class findings:", fpic)
new_row = ["P2-14","P2","CONFIRMED","portability","dynamic_psf","lib/dynamic_psf/Makefile","14","-fPIC missing in shared build",
 "documented make builds dynamic_psf.dll on Linux",
 "Makefile shared build lacks -fPIC -> R_X86_64_PC32 against stderr relocation error; adding -fPIC builds dynamic_psf.dll ELF so sha256 ac813c13... (3rd instance of the missing-fPIC class after astro_image_io P1-04 and gaia P2-12)",
 "documented command works",
 "make (logs/dpsf_build.log) + -fPIC workaround (logs/dpsf_build_fpic.log)",
 "clean-clone reproducibility of dynamic_psf FAIL without Makefile edit; systemic missing-fPIC across Makefiles",
 "add -fPIC to shared builds repo-wide",
 "rebuild from fresh archive",
 "04_build/build_matrix.csv dynamic_psf-linux"]
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
