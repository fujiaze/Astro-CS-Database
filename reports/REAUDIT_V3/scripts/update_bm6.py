import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "04_build", "build_matrix.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
seen = set(r["layer"] for r in rows)
nr = ["dynamic_psf-linux","Makefile (lib/dynamic_psf)","PASS(workaround) / FAIL(document cmd)","logs/dpsf_build.log + dpsf_build_fpic.log","doc=2; workaround=0",
 "Makefile lacks -fPIC for shared build (R_X86_64_PC32 against stderr relocation); adding -fPIC builds dynamic_psf.dll ELF so sha256 ac813c13... (same missing-fPIC class as astro_image_io P1-04 / gaia P2-12)"]
if nr[0] not in seen:
    rows.append(dict(zip(keys, nr)))
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("build_matrix rows:", len(rows))
