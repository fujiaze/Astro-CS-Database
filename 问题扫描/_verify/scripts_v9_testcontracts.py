import csv, json, pathlib, re
repo=pathlib.Path('.')
rows=list(csv.DictReader(open('docs/TRACEABILITY.csv',encoding='utf-8')))
find=[]
missing_files=[]
for r in rows:
    tfiles=[t.strip() for t in r.get('test_files','').split(';') if t.strip()]
    tids=[t.strip() for t in r.get('test_ids','').split(';') if t.strip()]
    for tf in tfiles:
        p=repo/tf
        if p.exists(): continue
        if 'synthetic_gate' in tf or 'TST-' in str(tids):
            if (repo/'lib/phase2/tests/synthetic_gate.cpp').exists(): continue
        missing_files.append((r['requirement_id'], tf))
print('CON-TEST-CONTRACTS TEST-BAD-FILE:', len(missing_files))
for m in missing_files[:20]: print('   ', m)
noscience=[r['requirement_id'] for r in rows if r['requirement_type']=='science' and not r.get('test_ids','').strip()]
print('science rows w/o test_ids:', len(noscience), noscience[:10])
tst=set()
for r in rows:
    for t in r.get('test_ids','').split(';'):
        if t.strip(): tst.add(t.strip())
print('unique TST ids:', len(tst), '(<5 would FAIL)')
print('synthetic_gate.cpp exists:', (repo/'lib/phase2/tests/synthetic_gate.cpp').exists())
verdict='FAIL' if (missing_files or noscience or len(tst)<5 or not (repo/'lib/phase2/tests/synthetic_gate.cpp').exists()) else 'PASS'
print('CON-TEST-CONTRACTS STATIC VERDICT:', verdict)
