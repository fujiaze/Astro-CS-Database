#!/usr/bin/env python3
"""ROOT-004: mechanical anchor re-check for all 93 P0 entries against the CURRENT tree.
For each P0: extract file[:line] anchors from its own text, check existence + whether the
quoted snippet on that line still matches (token overlap). Read-only."""
import os, re, json, sys, glob, difflib
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
GEN  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
dig  = json.load(open(os.path.join(GEN,'p0_digest.json'), encoding='utf-8'))
PATH = re.compile(r'([A-Za-z0-9_][A-Za-z0-9_./\-]*\.(?:cpp|hpp|h|c|py|md|json|csv|txt|jsonl|in|ps1|sh|cmake|yaml|yml))(?::(\d+)(?:\s*[-–]\s*(\d+))?)?')
SKIP_DIRS = ('run/','build/','out/','artifacts/','evidence/','reports/','graph/','worktrees/','Testing/','logs/','third_party/','设计大纲/','问题扫描/')
def norm_tokens(s):
    return set(re.findall(r'[A-Za-z_][A-Za-z0-9_]{3,}|[0-9]+\.[0-9]+', s))
out=[]
for d in dig:
    txt = open(os.path.join(GEN,'p0_sections', d['id']+'.md'), encoding='utf-8').read()
    seen=set(); checks=[]
    for m in PATH.finditer(txt):
        p, l1, l2 = m.group(1), m.group(2), m.group(3)
        if p.startswith(SKIP_DIRS) or p.startswith('./'): continue
        key=(p,int(l1) if l1 else 0)
        if key in seen: continue
        seen.add(key)
        full=os.path.join(ROOT,p)
        exists=os.path.exists(full)
        rec={'path':p,'line':int(l1) if l1 else 0,'exists':exists,'lineok':None,'cur':''}
        if exists and l1:
            try:
                lines=open(full,encoding='utf-8',errors='replace').read().replace('\r\n','\n').split('\n')
                n=int(l1)
                if n<=len(lines):
                    cur=lines[n-1]
                    # quoted text on the same source line of the finding
                    src=d['id']
                    head=txt.split('\n')
                    ql=[x for x in head if ('('+p+':'+l1) in x or ('（'+p+':'+l1) in x]
                    q=' '.join(ql)
                    a=norm_tokens(cur); b=norm_tokens(q)
                    rec['lineok'] = bool(a & b) and (len(a & b) >= max(1,min(3,len(a)//4)) if a else False)
                    rec['cur']=cur.strip()[:110]
                    rec['ov']=len(a&b)
            except Exception as e:
                rec['err']=str(e)[:60]
        checks.append(rec)
    ok=sum(1 for c in checks if c['exists'])
    lok=sum(1 for c in checks if c.get('lineok'))
    lnum=sum(1 for c in checks if c['line'])
    bad=[c for c in checks if not c['exists']]
    miss=[c for c in checks if c['exists'] and c['line'] and c.get('lineok') is False]
    out.append({'id':d['id'],'cat':d['cat'],'n':len(checks),'exists':ok,'linenum':lnum,'lineok':lok,
                'bad':[c['path'] for c in bad][:4],'miss':[(c['path'],c['line'],c.get('ov'),c['cur']) for c in miss][:4]})
json.dump(out, open(os.path.join(GEN,'p0_anchor_check.json'),'w',encoding='utf-8'), ensure_ascii=False, indent=1)
tot=sum(o['n'] for o in out); tote=sum(o['exists'] for o in out); totl=sum(o['linenum'] for o in out); totlo=sum(o['lineok'] for o in out)
print('P0 entries:',len(out),'| anchors:',tot,'| existing:',tote,'| with line:',totl,'| line-match:',totlo)
print('entries with >=1 missing path:',sum(1 for o in out if o['bad']))
print('entries with >=1 line-mismatch:',sum(1 for o in out if o['miss']))
lo,hi=int(sys.argv[1]),int(sys.argv[2])
for o in out[lo:hi]:
    print('%-11s %-14s anchors=%-2d exists=%-2d line=%-2d match=%-2d %s %s' % (o['id'],o['cat'],o['n'],o['exists'],o['linenum'],o['lineok'],
        ('MISS='+','.join(o['bad'])) if o['bad'] else '', ('LINEΔ='+' ; '.join('%s:%s(ov=%s)'%(p,l,v) for p,l,v,_ in o['miss'])) if o['miss'] else ''))

