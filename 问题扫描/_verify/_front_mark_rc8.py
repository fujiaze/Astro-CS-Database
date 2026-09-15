import csv
fp='问题扫描/账本/FIX_LEDGER.csv'
rw=list(csv.DictReader(open(fp,encoding='utf-8-sig')))
for r in rw:
    k=r['id']
    if k in {'M1a-C-002','M3-A-001','M8-F-002'}: r['verified_state']='FIXED';r['verified_by']='RQS-RC8'
    if k=='V12-N-15': r['verified_state']='CANNOT-REPRODUCE';r['verified_by']='RQS-RC8'
    if k in {'M4-F-04','M5b-C-03','M5b-G-06','M6a-D-009','M6b-G-001','V1-N-02','V11-N-09','V12-N-07','V15-N-16'}: r['verified_state']=(r['verified_state'] or 'STILL')+'-SUBITEM-EXPIRED';r['verified_by']='RQS-RC8'
    if k=='M1a-D-002': r['verified_state']='STILL-WAITING-UNTRACKED-LOCK';r['verified_by']='RQS-RC8'
for r in rw:
    if r.get('verified_by')=='RQS-RC8': r['verified_date']='2026-09-15'
with open(fp,'w',encoding='utf-8-sig',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rw[0].keys()));w.writeheader();w.writerows(rw)
print('rc8 marked',sum(1 for r in rw if r.get('verified_by')=='RQS-RC8'))

