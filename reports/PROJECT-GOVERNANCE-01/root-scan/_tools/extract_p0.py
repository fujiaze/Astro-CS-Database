#!/usr/bin/env python3
"""ROOT-004: extract P0 finding sections to _gen/p0_sections/ and a digest. Read-only."""
import csv, os, re, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
GEN  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
OUT  = os.path.join(GEN,'p0_sections'); os.makedirs(OUT, exist_ok=True)
rows = [r for r in csv.DictReader(open(os.path.join(GEN,'idmap.csv'), encoding='utf-8')) if r['priority']=='P0']
digest = []
for r in sorted(rows, key=lambda x:(x['category'],x['id'])):
    p = os.path.join(ROOT, r['file']); start = int(r['line'])
    txt = open(p, encoding='utf-8', errors='replace').read().replace('\r\n','\n').split('\n')
    lvl = len(re.match(r'^(#+)', txt[start-1]).group(1))
    body = []
    for i in range(start-1, len(txt)):
        m = re.match(r'^(#+)\s', txt[i])
        if m and i > start-1 and len(m.group(1)) <= lvl: break
        body.append(txt[i])
    open(os.path.join(OUT, r['id']+'.md'),'w',encoding='utf-8').write('\n'.join(body))
    digest.append({'id':r['id'],'cat':r['category'],'file':r['file'],'line':start,'nlines':len(body),'title':r['title'],'heading':txt[start-1][:200]})
json.dump(digest, open(os.path.join(GEN,'p0_digest.json'),'w',encoding='utf-8'), ensure_ascii=False, indent=1)
print('P0 entries:',len(digest))
import collections
print('by cat:',dict(collections.Counter(d['cat'] for d in digest)))
print('section lines total:',sum(d['nlines'] for d in digest),'| median:',sorted(d['nlines'] for d in digest)[len(digest)//2])
for d in digest: print('%-12s %-16s %s:%d (%d lines) %s' % (d['id'],d['cat'],d['file'].split('/')[-1],d['line'],d['nlines'],d['heading'][:80]))

