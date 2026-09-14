import csv, json
rows=list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))
disp=[r for r in rows if r['fix_state'] not in ('','OPEN')]
print('DISPOSED',len(disp))
for r in disp:
    print('|'.join([r['id'], r['priority'], r['fix_state'], (r['fix_commit'] or '-')[:9], (r['regression_test'] or '-')[:70], (r['title'] or '')[:60]]))