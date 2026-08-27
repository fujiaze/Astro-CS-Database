import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
new_row = ["P3-03","P3","CONFIRMED","constants_consistency","phase2 sampler vs snr_estimator","lib/phase2/src/sampler.cpp:451; lib/snr_estimator/cpp/src/noise_model.cpp:61","-","Two sigma constants coexist (1.4826 literal vs 1.482602218505602)",
 "one canonical sigma constant",
 "p2_stats_mad returns MAD already scaled by inline literal 1.4826 while snr robust_sigma uses full-precision 1.482602218505602; relative diff ~1.5e-5, scientifically immaterial",
 "-",
 "stats probe + source read",
 "documentation/consistency nit only",
 "use the frozen constant everywhere",
 "next constants audit",
 "08_science_oracles/SCIENCE_ORACLES_SUMMARY.md oracle 10.18"]
if new_row[0] not in seen:
    rows.append(new_row)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
