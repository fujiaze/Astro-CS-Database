import json, os, re, glob as _glob, subprocess, collections
CONTRACT_REL="docs/algorithms/anchors/anchor_contract.json"
EXTS=("cpp","cc","cxx","h","hpp","hh","py","sh","ps1","txt","json","yaml","yml","md","cmake","in")
_FILE=r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:"+"|".join(EXTS)+r"))"
_ONE=r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
ANCHOR_RE=re.compile(_FILE+_ONE+r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*")
CONT_RE=re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
ARCH=("/archive/","/legacy/","/.git/","/third_party/","/build/","/out/","/run/","/worktrees/")
root=os.getcwd()
tracked=[t for t in subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines()]
tset=set(tracked)
# index over TRACKED files only, applying the same archive-marker pruning on the dir path
idx={}
for rel in tracked:
    d='/'+rel.rsplit('/',1)[0]+'/' if '/' in rel else '/'
    if any(s in d for s in ARCH): continue
    idx.setdefault(os.path.basename(rel),[]).append(rel)
for k in idx: idx[k].sort()
def read_lines(rel):
    t=open(rel,'rb').read().decode('utf-8',errors='replace')
    if t.endswith('\n'): t=t[:-1]
    return t.split('\n')
def expand(line):
    out=[]
    for m in ANCHOR_RE.finditer(line):
        base=m.group(1); start=int(m.group(2)); end=int(m.group(3)) if m.group(3) else start
        out.append((m.group(0),base,start,end)); pos=m.end()
        while True:
            cm=CONT_RE.match(line,pos)
            if not cm: break
            out.append((line[m.start():cm.end()],base,int(cm.group(1)), int(cm.group(2)) if cm.group(2) else int(cm.group(1))))
            pos=cm.end()
    return out
contract=json.load(open(CONTRACT_REL,encoding='utf-8'))
resolvers=contract.get('resolvers',[]); exemptions=contract.get('exemptions',[]); bindings=contract.get('bindings',[])
def resolve(doc,base):
    direct=os.path.normpath(base).replace(os.sep,'/')
    if not direct.startswith('../') and direct in tset and os.path.isfile(direct): return direct,'exact'
    rel=os.path.normpath(os.path.join(os.path.dirname(doc),base)).replace(os.sep,'/')
    if not rel.startswith('../') and rel in tset: return rel,'doc-relative'
    bn=os.path.basename(base)
    for r in resolvers:
        if r.get('doc') and r['doc']!=doc: continue
        if r.get('doc_prefix') and not doc.startswith(r['doc_prefix']): continue
        if r.get('basename')!=bn: continue
        return r['path'],'contract-rule'
    c=idx.get(bn,[])
    if len(c)==1: return c[0],'basename-unique'
    if not c: return None,'no-such-file'
    return None,'ambiguous:'+','.join(c[:4])
docs=sorted({os.path.relpath(p,root).replace(os.sep,'/') for pat in contract['doc_globs'] for p in _glob.glob(pat)})
errors=[]; anchors=[]
ex_keys={(e['doc'],e['raw']) for e in exemptions}; matched=set()
lcc={}
for doc in docs:
    for i,line in enumerate(read_lines(doc),1):
        for raw,base,start,end in expand(line):
            rec={'doc':doc,'doc_line':i,'raw':raw,'start':start,'end':end}
            if (doc,raw) in ex_keys:
                matched.add((doc,raw)); rec['status']='EXEMPT'; anchors.append(rec); continue
            resolved,how=resolve(doc,base)
            if resolved is None:
                rec['status']='UNRESOLVED'; errors.append(('C2','%s:%d %s (%s)'%(doc,i,raw,how))); anchors.append(rec); continue
            rec['resolved']=resolved
            if resolved not in tset:
                rec['status']='UNTRACKED'; errors.append(('C2','%s:%d %s -> untracked %s'%(doc,i,raw,resolved))); anchors.append(rec); continue
            n=lcc.get(resolved)
            if n is None:
                n=len(read_lines(resolved)); lcc[resolved]=n
            if start<1 or end<start or end>n:
                rec['status']='OUT_OF_BOUNDS'; errors.append(('C3','%s:%d %s -> %s has %d lines'%(doc,i,raw,resolved,n)))
            else: rec['status']='OK'
            anchors.append(rec)
for doc,raw in sorted(ex_keys):
    if (doc,raw) not in matched: errors.append(('C5','stale exemption %s %s'%(doc,raw)))
for b in bindings:
    doc,tgt,sym=b['doc'],b['target'],b['symbol']; tid=b.get('id','?')
    if not os.path.isfile(tgt): errors.append(('C4','%s target missing %s'%(tid,tgt))); continue
    tl=read_lines(tgt); hits=[i for i,l in enumerate(tl,1) if sym in l]
    if not hits: errors.append(('C4','%s STALE_BINDING absent (target=%s)'%(tid,tgt))); continue
    ok=any(a['doc']==doc and a.get('resolved')==tgt and a['status']=='OK' and sym in chr(10).join(tl[a['start']-1:a['end']]) for a in anchors)
    if not ok: errors.append(('C4','%s BINDING_VIOLATION sym %s@%s lines=%s'%(tid,sym,tgt,hits[:4])))
print('CLEAN-CHECKOUT DOC-LINE-ANCHORS: docs=%d anchors=%d errors=%d => %s'%(len(docs),len(anchors),len(errors),'FAIL' if errors else 'PASS'))
print('by code', dict(collections.Counter(e[0] for e in errors)))
for e in errors: print('  [%s] %s'%e)
