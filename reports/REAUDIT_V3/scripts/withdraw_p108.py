import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
kept = [rows[0]] + [r for r in rows[1:] if r[0] != "P1-08"]
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(kept)
from collections import Counter
c = Counter(r[1] for r in kept[1:])
print("P1-08 withdrawn. findings rows=" + str(len(kept)-1) + " by_severity=" + str(dict(c)))
