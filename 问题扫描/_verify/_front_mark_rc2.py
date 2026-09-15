import csv
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
FULL={"M2a-C-7","M9-H-1","V2-N-01"}
PART={"M3-E-001","M4-F-06","FD-F-002"}
CLOSED={"M3-I-002"}
for r in rows:
    k=r["id"]
    if k in FULL: r["verified_state"]="FIXED"
    elif k in PART: r["verified_state"]="FIXED-PARTIAL"
    elif k in CLOSED: r["verified_state"]="CLOSED-ANCHOR-DEAD"
    else: continue
    r["verified_by"]="RQS-RC2"; r["verified_date"]="2026-09-15"
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print("verified 非空",sum(1 for r in rows if r["verified_state"]),"/ 总行",len(rows))
