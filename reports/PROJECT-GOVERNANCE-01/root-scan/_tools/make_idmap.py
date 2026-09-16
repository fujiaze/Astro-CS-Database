#!/usr/bin/env python3
"""ROOT-004 helper: build id -> finding-file map from FIX_LEDGER.csv + findings headings.
Read-only. Outputs CSV/JSON under reports/PROJECT-GOVERNANCE-01/root-scan/_gen/."""
import os,re,csv,json,collections,sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
SCAN = os.path.join(ROOT,'问题扫描')
OUT  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
os.makedirs(OUT, exist_ok=True)

led = list(csv.DictReader(open(os.path.join(SCAN,'账本','FIX_LEDGER.csv'), encoding='utf-8-sig')))
L = {r['id']: r for r in led}
ids_sorted = sorted(L, key=len, reverse=True)

heads = []
for cls in sorted(os.listdir(os.path.join(SCAN,'findings'))):
    cdir = os.path.join(SCAN,'findings',cls)
    if not os.path.isdir(cdir): continue
    for pri in sorted(os.listdir(cdir)):
        d = os.path.join(cdir,pri)
        if not os.path.isdir(d): continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith('.md') or fn=='README.md': continue
            p = os.path.join(d,fn)
            rel = os.path.relpath(p, ROOT)
            txt = open(p, encoding='utf-8', errors='replace').read().replace('\r\n','\n')
            for n,ln in enumerate(txt.split('\n'), 1):
                m = re.match(r'^(#{2,4})\s+(.*)$', ln)
                if m: heads.append((rel, n, m.group(1), m.group(2)))

idmap = {}
unmatched = []
for rel,n,lvl,txt in heads:
    hit = None
    for i in ids_sorted:
        if txt.startswith(i) and (len(txt)==len(i) or not re.match(r'[A-Za-z0-9]', txt[len(i)])):
            hit = i; break
    if hit:
        idmap.setdefault(hit, []).append({'file':rel,'line':n,'level':lvl,'heading':txt})
    else:
        unmatched.append({'file':rel,'line':n,'level':lvl,'heading':txt})

rows = []
for i,r in L.items():
    hs = idmap.get(i, [])
    rows.append({
        'id': i,
        'category': r['category'],
        'priority': r['priority'],
        'producer': r['producer'],
        'release_blocker': r['release_blocker'],
        'owner_decision': r['owner_decision'],
        'title': r['title'][:120],
        'fix_state': r['fix_state'],
        'verified_state': r['verified_state'],
        'evidence_file_field': r['evidence_file'],
        'n_headings': len(hs),
        'file': hs[0]['file'] if hs else '',
        'line': hs[0]['line'] if hs else '',
        'files_all': ';'.join(sorted({h['file'] for h in hs})),
    })
rows.sort(key=lambda r:(r['category'],r['priority'],r['id']))
with open(os.path.join(OUT,'idmap.csv'),'w',newline='',encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
json.dump({'idmap':idmap,'unmatched_headings':unmatched}, open(os.path.join(OUT,'idmap_headings.json'),'w',encoding='utf-8'), ensure_ascii=False, indent=1)

mat = collections.Counter((r['category'],r['priority']) for r in led)
print('LEDGER ENTRIES:', len(L))
print('HEADINGS:', len(heads), '| ids with >=1 heading:', len(idmap), '| ids with 0 heading:', len(L)-len(idmap))
print('extra (non-entry) headings:', len(unmatched))
print('matrix  category / P0 / P1 / P2 / P? / total')
cats = sorted({r['category'] for r in led})
for c in cats:
    p0=mat[(c,'P0')]; p1=mat[(c,'P1')]; p2=mat[(c,'P2')]; pq=mat[(c,'P?')]
    print('  %-18s %4d %4d %4d %4d %6d' % (c,p0,p1,p2,pq,p0+p1+p2+pq))
tot = collections.Counter(r['priority'] for r in led)
print('TOTAL by priority:', dict(tot), 'sum', sum(tot.values()))
print('fix_state:', dict(collections.Counter(r['fix_state'] for r in led)))
print('verified_state:', dict(collections.Counter(r['verified_state'] for r in led)))

