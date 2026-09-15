import csv
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
n=0
for r in rows:
    if r["id"] in ("V19-N-05","V19-N-06"):
        r["verified_state"]="STILL"; r["verified_by"]="RQS-W6"; r["verified_date"]="2026-09-15"; n+=1
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print("标 STILL",n,"verified 非空",sum(1 for r in rows if r["verified_state"]))
