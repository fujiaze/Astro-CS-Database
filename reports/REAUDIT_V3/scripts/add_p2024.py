import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
hdr = rows[0]
# find next P2 id
ids = [r[0] for r in rows[1:]]
import re
maxid = max(int(re.search(r"(\d+)", i).group(1)) for i in ids if re.search(r"(\d+)", i))
nid = f"P2-{maxid+1:02d}"
new = [nid, "P2", "CONFIRMED", "documentation", "docs", "lib/star_detector/README.md; lib/plate_solve/README.md",
  "preview-image links; archive/HISTORY.md",
  "READMEs reference generated preview images / archive path",
  "5 genuinely broken doc refs found by genuine-only re-validation (round 133): star_detector README -> test_output/example.jpg; plate_solve README -> docs/wcs_reproject_preview.png, docs/sip_before.png, docs/sip_after.png, archive/HISTORY.md",
  "expected: referenced preview artifacts / paths exist",
  "reproduction: python3 scripts/doc_ref_clean.py (71 links checked, 5 broken)",
  "low (missing preview images in README; archive path) - documentation hygiene",
  "commit/remove the preview artifacts or use relative stable paths; verify archive/HISTORY.md exists",
  "python3 scripts/doc_ref_clean.py -> 0 genuinely broken",
  "builds/doc_ref_clean.json + 13_static_quality/DOC_REFERENCE_REVALIDATION.md"]
rows.append(new)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("added", nid, "total findings:", len(rows)-1)
