import csv
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
F={"M3b-F-01","M9-H-2","V5-N-03"}
P={"M3-I-003"}
S={"FD-F-003","V11-N-04","V19-N-05"}
n=0
for r in rows:
    k=r["id"]
    v = "FIXED" if k in F else ("FIXED-RESIDUAL" if k in P else ("STILL" if k in S else None))
    if not v: continue
    r["verified_state"]=v; r["verified_by"]="RQS-RC3" if k not in ("V19-N-05",) else "RQS-W6"; r["verified_date"]="2026-09-15"; n+=1
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print("标记",n,"verified 非空",sum(1 for r in rows if r["verified_state"]))
