import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
print("existing:", [r[0] for r in rows[1:]])
new_row = ["P1-08","P1","CONFIRMED","api_contract","phase2 upm","lib/phase2/src/upm.cpp:1107-1136 p2_upm_raw_weight","upm.h:127-136 contract text","p2_upm_raw_weight does NOT implement its documented contract",
 "raw_w = quality_factor x control_ivar (production); ivar<=0/non-finite -> rc=2; ablation path snr^2/(1+snr^2)/max(unc^2,floor^2)",
 "EXECUTABLE PROBES (builds/raww_probe.c/.2, real libphase2.a): (a) production returns a CONSTANT 15.841584 regardless of control_ivar (civar=4/16/1 all give the same raw_w) and equals neither qf*civar nor any documented formula; (b) civar=0 and civar<0 return rc=0 with the constant, NOT the documented rc=2 explicit refusal; (c) ablation path matches qf*sp*snr^2/(1+snr^2)/unc^2 exactly (1.8) so only the production branch deviates. Header comment (upm.h:127-136) and code comment (SCI-UPM-WEIGHT-001) both contradict the binary behavior.",
 "documented formula holds",
 "executable probes + source read at HEAD 535e7387",
 "production science weights do not follow SCI-UPM-WEIGHT-001 as documented; the ivar-refusal guard is absent in the compiled library; UPM weighting results cannot be reproduced from the documented formula",
 "reconcile implementation with SCI-UPM-WEIGHT-001 or update docs; add unit tests pinning raw_weight values",
 "re-run probes after fix",
 "08_science_oracles/UPM_RAW_WEIGHT_DEVIATION.md"]
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
