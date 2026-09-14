import json, os, re, subprocess, glob as _glob, collections
CONTRACT_REL="docs/algorithms/anchors/anchor_contract.json"
EXTS=("cpp","cc","cxx","h","hpp","hh","py","sh","ps1","txt","json","yaml","yml","md","cmake","in")
_FILE=r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:"+"|".join(EXTS)+r"))"
_ONE=r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
_CONT=r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*"
ANCHOR_RE=re.compile(_FILE+_ONE+_CONT)
CONT_RE=re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
root=os.getcwd()
tracked=sorted(subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines())
tset=set(tracked)
def read_lines(rel):
    raw=open(rel,'rb').read(); t=raw.decode('utf-8',errors='replace')
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
            s2=int(cm.group(1)); e2=int(cm.group(2)) if cm.group(2) else s2
            out.append((line[m.start():cm.end()],base,s2,e2)); pos=cm.end()
    return out
contract=json.load(open(CONTRACT_REL,encoding='utf-8'))
resolvers=contract.get('resolvers',[]); exemptions=contract.get('exemptions',[]); bindings=contract.get('bindings',[])
# CLEAN-CHECKOUT basename index: tracked files only
idx={}
for rel in tracked:
    idx.setdefault(os.path.basename(rel),[]).append(rel)
for k in idx: idx[k].sort()
def resolve(doc,base):
    direct=os.path.normpath(base).replace(os.sep,'/')
    if not direct.startswith('../') and direct in tset: return direct,'exact'
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
    return None,'ambiguous:'+','.join(c[:6])
docs=[]
for pat in contract.get('doc_globs',[]):
    for p in sorted(_glob.glob(pat)): docs.append(os.path.relpath(p,root).replace(os.sep,'/'))
docs=sorted(set(docs))
errors=[]; anchors=[]
def fail(c,d): return {'code':c,'detail':d}
ex_keys={(e['doc'],e['raw']) for e in exemptions}; matched=set()
lc={}
for doc in docs:
    lines=read_lines(doc)
    for i,line in enumerate(lines,1):
        for raw,base,start,end in expand(line):
            rec={'doc':doc,'doc_line':i,'raw':raw,'start':start,'end':end}
            if (doc,raw) in ex_keys:
                matched.add((doc,raw)); rec['status']='EXEMPT'; anchors.append(rec); continue
            resolved,how=resolve(doc,base)
            if resolved is None:
                rec['status']='UNRESOLVED'; errors.append(fail('C2','%s:%d %s (%s)'%(doc,i,raw,how))); anchors.append(rec); continue
            rec['resolved']=resolved
            if resolved not in tset:
                rec['status']='UNTRACKED'; errors.append(fail('C2','%s:%d %s -> untracked %s'%(doc,i,raw,resolved))); anchors.append(rec); continue
            n=lc.get(resolved)
            if n is None:
                try: n=len(read_lines(resolved))
                except OSError: n=-1
                lc[resolved]=n
            if start<1 or end<start or end>n:
                rec['status']='OUT_OF_BOUNDS'; errors.append(fail('C3','%s:%d %s -> %s has %d lines'%(doc,i,raw,resolved,n)))
            else: rec['status']='OK'
            anchors.append(rec)
for doc,raw in sorted(ex_keys):
    if (doc,raw) not in matched: errors.append(fail('C5','stale exemption %s %s'%(doc,raw)))
for b in bindings:
    doc=b['doc']; target=b['target']; sym=b['symbol']; tid=b.get('id','|'.join([doc,target,sym]))
    if target not in tset or not os.path.isfile(target): errors.append(fail('C4','%s: target missing %s'%(tid,target))); continue
    tl=read_lines(target); hits=[i for i,l in enumerate(tl,1) if sym in l]
    if not hits: errors.append(fail('C4','%s: STALE_BINDING absent'%tid)); continue
    ok=any(a['doc']==doc and a.get('resolved')==target and a['status']=='OK' and sym in chr(10).join(tl[a['start']-1:a['end']]) for a in anchors)
    if not ok: errors.append(fail('C4','%s: BINDING_VIOLATION at lines %s'%(tid,hits[:6])))
print('CLEAN-CHECKOUT (tracked-only) DOC-LINE-ANCHORS: errors=%d => %s'%(len(errors),'FAIL' if errors else 'PASS'))
print('by code:', dict(collections.Counter(e['code'] for e in errors)))
for e in errors[:20]: print('   [%s] %s'%(e['code'],e['detail']))
