import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
for r in rows:
    if r and r[0] == "P1-02":
        # append complexity oracle evidence to the evidence column (index 11)
        while len(r) < 13: r.append("")
        r[11] = r[11] + " + 08_science_oracles/upm_complexity_oracle.json (numeric: doc O(iter*(obs+K log K)) vs actual per-frame full-K CG x200 -> ~5.3-7.1e3x undercount)"
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("P1-02 evidence updated")
