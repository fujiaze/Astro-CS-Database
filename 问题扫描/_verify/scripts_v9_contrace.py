import csv, json, pathlib, re, sys
repo = pathlib.Path('.')
rows = list(csv.DictReader(open('docs/TRACEABILITY.csv', encoding='utf-8')))
print('ROWS', len(rows))
print('COLS', list(rows[0].keys()))
ids=[r['requirement_id'].strip() for r in rows]
dup=[x for x in set(ids) if ids.count(x)>1]
print('DUP', dup)
missing_doc=[]
for r in rows:
    doc=(r.get('authority_doc') or '').strip()
    if doc and not (repo/doc).exists(): missing_doc.append((r['requirement_id'], doc))
print('TRACE-MISSING-DOC', len(missing_doc))
for m in missing_doc[:20]: print('   ', m)
titles=' '.join(ids).upper()
core=["CAL","PSF","PHOT","NOISE","DRZ","UPM","REJ","INT","ACR"]
miss=[k for k in core if k not in titles]
print('CORE KEYWORDS MISSING:', miss)
print('WCS/AST hit:', any(k in titles for k in ["WCS","AST"]))
empty=[i for i,r in enumerate(rows,2) if not r['requirement_id'].strip()]
print('EMPTY IDS', empty)
verdict = 'FAIL' if (dup or missing_doc or miss or empty) else 'PASS'
print('CON-TRACEABILITY STATIC VERDICT:', verdict)
print()
print('--- ids containing each core kw ---')
for k in core:
    print('  ', k, [i for i in ids if k in i.upper()][:6])
