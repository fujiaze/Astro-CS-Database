#!/usr/bin/env python3
"""ROOT-004: P0 anchor re-check v2 — basename resolution + drift-aware line match. Read-only.
For each of the 93 P0 entries and each file[:line] anchor mentioned in its own text:
  - resolve path (direct, else unique basename in the real-source tree)
  - if a line number is given, search +/-60 lines for the best token match with the quoted text
  - classify: EXACT(<=2) / NEAR(<=10) / DRIFT(>10) / ABSENT(no overlap) / NORESOLVE
Output: _gen/p0_anchor_v2.json + per-entry summary line."""
import os, re, json, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..'))
GEN  = os.path.join(ROOT,'reports','PROJECT-GOVERNANCE-01','root-scan','_gen')
PRUNE = {'run','build','out','artifacts','evidence','graph','worktrees','Testing','logs','third_party','设计大纲','.git','node_modules','__pycache__','.pytest_cache','.cache'}
# ---- basename index over real source tree
index={}
for dp, dns, fns in os.walk(ROOT):
    dns[:] = [d for d in dns if d not in PRUNE]
    for fn in fns:
        index.setdefault(fn, []).append(os.path.relpath(os.path.join(dp,fn), ROOT))
print('basename index entries:', len(index))
PATH = re.compile(r'([A-Za-z0-9_][A-Za-z0-9_./\-]*\.(?:cpp|hpp|h|c|py|md|json|csv|txt|jsonl|in|ps1|sh|cmake|yaml|yml))(?::(\d+))?')
TOK  = re.compile(r'[A-Za-z_][A-Za-z0-9_]{4,}|[0-9]+\.[0-9]+')
def toks(s): return set(TOK.findall(s))
def resolve(p):
    full=os.path.join(ROOT,p)
    if os.path.exists(full) and os.path.isfile(full): return p
    cands=[c for c in index.get(os.path.basename(p),[]) if c.endswith(p.replace('./','')) or os.path.basename(c)==os.path.basename(p)]
    if len(cands)==1: return cands[0]
    if cands: return sorted(cands, key=len)[0] + ('  (+%d同名)'%(len(cands)-1) if len(cands)>1 else '')
    return None
dig = json.load(open(os.path.join(GEN,'p0_digest.json'), encoding='utf-8'))
report=[]
for d in dig:
    txt = open(os.path.join(GEN,'p0_sections', d['id']+'.md'), encoding='utf-8').read()
    seen=set(); recs=[]
    for m in PATH.finditer(txt):
        p, l1 = m.group(1), m.group(2)
        if p.startswith(('run/','build/','out/','artifacts/','evidence/','reports/','graph/','worktrees/','Testing/','logs/','third_party/','设计大纲/')): continue
        key=(p,int(l1) if l1 else 0)
        if key in seen: continue
        seen.add(key)
        rp=resolve(p)
        r={'ref':p,'line':int(l1) if l1 else 0,'resolved':rp,'cls':'NORESOLVE','best':None,'ov':0}
        if rp and l1:
            real=rp.split('  (')[0]
            try:
                lines=open(os.path.join(ROOT,real),encoding='utf-8',errors='replace').read().replace('\r\n','\n').split('\n')
            except Exception: lines=[]
            n=int(l1)
            # quoted text = 220 chars following the anchor mention
            i=txt.find(m.group(0)); q=txt[i:i+260]
            qt=toks(q)
            best=(0,None)
            if lines:
                lo=max(1,n-60); hi=min(len(lines),n+60)
                for k in range(lo,hi+1):
                    o=len(qt & toks(lines[k-1]))
                    if o>best[0]: best=(o,k)
                if best[0]==0: r['cls']='ABSENT'
                else:
                    delta=abs(best[1]-n)
                    r['cls']='EXACT' if delta<=2 else ('NEAR' if delta<=10 else 'DRIFT')
                    r['best']=best[1]; r['ov']=best[0]
            r['nlines']=len(lines)
        elif rp: r['cls']='NO-LINE'
        recs.append(r)
    order={'EXACT':0,'NEAR':1,'DRIFT':2,'ABSENT':3,'NO-LINE':4,'NORESOLVE':5}
    worst=sorted(recs,key=lambda r:order[r['cls']])
    report.append({'id':d['id'],'cat':d['cat'],'n':len(recs),
                   'cnt':{k:sum(1 for r in recs if r['cls']==k) for k in order},
                   'recs':recs})
json.dump(report, open(os.path.join(GEN,'p0_anchor_v2.json'),'w',encoding='utf-8'), ensure_ascii=False, indent=1)
agg={k:0 for k in ('EXACT','NEAR','DRIFT','ABSENT','NO-LINE','NORESOLVE')}
for o in report:
    for k,v in o['cnt'].items(): agg[k]+=v
print('total anchors:',sum(o['n'] for o in report),'| classes:',agg)
lo,hi=int(sys.argv[1]),int(sys.argv[2])
for o in report[lo:hi]:
    bad=[r for r in o['recs'] if r['cls'] in ('DRIFT','ABSENT')]
    s=' '.join('%s:%d->%s(%s,ov=%d)'%(r['ref'].split('/')[-1],r['line'],r['best'],r['cls'],r['ov']) for r in bad[:3])
    print('%-11s %-14s n=%-2d E=%-2d N=%-2d D=%-2d A=%-2d NR=%-2d | %s' % (o['id'],o['cat'],o['n'],o['cnt']['EXACT'],o['cnt']['NEAR'],o['cnt']['DRIFT'],o['cnt']['ABSENT'],o['cnt']['NORESOLVE'],s))

