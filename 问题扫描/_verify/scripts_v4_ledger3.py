import csv, json
p="/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(p,newline='',encoding='utf-8')))
for r in rows:
    i=(r.get('\ufeffid') or r.get('id') or '')
    if i in ('M5b-F-02','M8-G-001','M8-F-004','M8-F-001','FD-B2','M5a-G-003','M8a-G-001'):
        print('='*90)
        for k in ('id','priority','category','title','fix_state','fix_commit','regression_test','fix_note'):
            kk = '\ufeff'+k if k=='id' else k
            v=r.get(kk) or r.get(k)
            if v: print(f"  {k}: {v[:900]}")
