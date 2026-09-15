import csv
fp='问题扫描/账本/FIX_LEDGER.csv'
rows=list(csv.DictReader(open(fp,encoding='utf-8-sig')))
F={'M3-C-001','M4-C-03','V1-N-11','V2-N-10'}
n=0
for r in rows:
    if r['id'] in F: r['verified_state']='FIXED'; r['verified_by']='RQS-RC6'; r['verified_date']='2026-09-15'; n+=1
    if r['id']=='M7-A-129': r['priority']='P2'
with open(fp,'w',encoding='utf-8-sig',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys()));w.writeheader();w.writerows(rows)
print('FIXED',n)

