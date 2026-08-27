import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
new_row = ["P2-15","P2","CONFIRMED","dead_api","orchestrator+astro_image_io","lib/orchestrator/cpp/include/{spill_manager,admission_controller,resource_monitor}.h","36 API rows (18+10+8)","Orphaned API headers - no implementation source includes them",
 "documented APIs have live implementations",
 "36 API_CONTRACTS rows describe headers that NO .cpp includes at HEAD (verified by grep across lib/); aio_ahpx_format.h has 0 exports; healpix_browser_core.h is Qt-only",
 "referenced APIs are live",
 "grep include + scripts/orphan_headers.py",
 "the contract table documents 36 APIs with no live reference at HEAD; cannot be VERIFIED as production APIs",
 "wire headers into implementations or mark legacy",
 "re-verify after orchestrator Linux port",
 "13_static_quality/ORPHAN_HEADER_NOTE.md"]
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
