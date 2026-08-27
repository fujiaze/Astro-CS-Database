import csv, os, re
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
ids = set(r[0] for r in rows[1:])
# pick next free P2-XX
n = 1
while True:
    cand = f"P2-{n:02d}"
    if cand not in ids:
        newid = cand; break
    n += 1
# the last row is our new one (doc refs); reassign its id
last = rows[-1]
print("last row id was:", last[0])
last[0] = newid
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("reassigned to", newid, "rows:", len(rows)-1)
