import csv, json
p="/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(p,newline='',encoding='utf-8')))
for k in ('UT-CLI','c1959436','dbaa2e18','memory_report','memory-report','F-14','MON-002','mon002'):
    hits=[]
    for r in rows:
        blob=json.dumps(r,ensure_ascii=False)
        if k in blob:
            hits.append(((r.get('\ufeffid') or r.get('id') or '')),)
    print("###",k,len(hits),hits[:20])
print()
for r in rows:
    i=(r.get('\ufeffid') or r.get('id') or '')
    if i in ('M5b-G-11','M5a-G-004','M5a-G-005','M5a-G-006','M5a-G-007','M5a-D-001','M5a-D-002','M5a-C-002','M8-F-009','M2a-C-7','M2a-C-8'):
        print(i,'|',r['fix_state'],'|',r['title'][:110])
