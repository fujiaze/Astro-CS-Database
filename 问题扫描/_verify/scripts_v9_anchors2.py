import json, os, re, subprocess, glob as _glob, sys
CONTRACT_REL="docs/algorithms/anchors/anchor_contract.json"
EXTS=("cpp","cc","cxx","h","hpp","hh","py","sh","ps1","txt","json","yaml","yml","md","cmake","in")
_FILE=r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:"+"|".join(EXTS)+r"))"
_ONE=r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
_CONT=r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*"
ANCHOR_RE=re.compile(_FILE+_ONE+_CONT)
CONT_RE=re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
ARCHIVE=("/archive/","/legacy/","/.git/","/third_party/","/build/","/out/","/run/","/worktrees/")
root=os.getcwd()
def read_lines(rel):
    raw=open(rel,'rb').read(); t=raw.decode('utf-8',errors='replace')
    if t.endswith('\n'): t=t[:-1]
    return t.split('\n')
def count_lines(rel):
    try: return len(read_lines(rel))
    except OSError: return -1
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
def build_index():
    idx={}
    for dp,dns,fns in os.walk(root):
        d=dp.replace(os.sep,'/')+'/'; dns[:]=[x for x in dns if x!='.git']
        if any(s in d for s in ARCHIVE): dns[:]=[]; continue
        for fn in fns:
            rel=os.path.relpath(os.path.join(dp,fn),root).replace(os.sep,'/')
            idx.setdefault(fn,[]).append(rel)
    for k in idx: idx[k].sort()
    return idx
contract=json.load(open(CONTRACT_REL,encoding='utf-8'))
resolvers=contract.get('resolvers',[]); exemptions=contract.get('exemptions',[]); bindings=contract.get('bindings',[])
tracked=subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines()
tset=set(tracked)
bidx=build_index()
docs=[]
for pat in contract.get('doc_globs',[]):
    for p in sorted(_glob.glob(pat)): docs.append(os.path.relpath(p,root).replace(os.sep,'/'))
docs=sorted(set(docs))
errors=[]; anchors=[]
def resolve(doc,base):
    direct=os.path.normpath(base).replace(os.sep,'/')
    if not direct.startswith('../') and os.path.isfile(direct): return direct,'exact'
    rel=os.path.normpath(os.path.join(os.path.dirname(doc),base)).replace(os.sep,'/')
    if not rel.startswith('../') and os.path.isfile(rel): return rel,'doc-relative'
    bn=os.path.basename(base)
    for r in resolvers:
        if r.get('doc') and r['doc']!=doc: continue
        if r.get('doc_prefix') and not doc.startswith(r['doc_prefix']): continue
        if r.get('basename')!=bn: continue
        return r['path'],'contract-rule'
    c=bidx.get(bn,[])
    if len(c)==1: return c[0],'basename-unique'
    if not c: return None,'no-such-file'
    return None,'ambiguous:'+','.join(c[:6])
def fail(code,detail): return {'severity':'ERROR','code':code,'detail':detail}
for d in docs:
    if not os.path.isfile(d): errors.append(fail('C1_docs_tracked','missing doc: '+d))
    elif d not in tset: errors.append(fail('C1_docs_tracked','untracked doc: '+d))
ex_keys={(e['doc'],e['raw']) for e in exemptions}; matched=set()
for doc in docs:
    try: lines=read_lines(doc)
    except OSError: continue
    for i,line in enumerate(lines,1):
        for raw,base,start,end in expand(line):
            rec={'doc':doc,'doc_line':i,'raw':raw,'base':base,'start':start,'end':end}
            if (doc,raw) in ex_keys:
                matched.add((doc,raw)); rec['status']='EXEMPT'; rec['resolved']=None; anchors.append(rec); continue
            resolved,how=resolve(doc,base); rec['how']=how
            if resolved is None:
                rec['status']='UNRESOLVED'; errors.append(fail('C2_anchor_resolved','%s:%d %s (%s)'%(doc,i,raw,how))); anchors.append(rec); continue
            rec['resolved']=resolved
            if resolved not in tset:
                rec['status']='UNTRACKED'; errors.append(fail('C2_anchor_resolved','%s:%d %s -> untracked %s'%(doc,i,raw,resolved))); anchors.append(rec); continue
            n=count_lines(resolved); rec['target_lines']=n
            if start<1 or end<start or end>n:
                rec['status']='OUT_OF_BOUNDS'; errors.append(fail('C3_range_in_bounds','%s:%d %s -> %s has %d lines'%(doc,i,raw,resolved,n)))
            else: rec['status']='OK'
            anchors.append(rec)
for doc,raw in sorted(ex_keys):
    if (doc,raw) not in matched: errors.append(fail('C5_exemptions_live','stale exemption: %s %s'%(doc,raw)))
for b in bindings:
    doc=b['doc']; target=b['target']; sym=b['symbol']; tid=b.get('id','|'.join([doc,target,sym]))
    if not os.path.isfile(target): errors.append(fail('C4_symbol_binding','%s: target missing %s'%(tid,target))); continue
    tl=read_lines(target); hits=[i for i,l in enumerate(tl,1) if sym in l]
    if not hits: errors.append(fail('C4_symbol_binding','%s: STALE_BINDING symbol absent'%tid)); continue
    ok=any(a['doc']==doc and a.get('resolved')==target and a['status']=='OK' and sym in chr(10).join(tl[a['start']-1:a['end']]) for a in anchors)
    if not ok: errors.append(fail('C4_symbol_binding','%s: BINDING_VIOLATION symbol now at lines %s'%(tid,hits[:6])))
import collections
st=collections.Counter(a['status'] for a in anchors)
print('docs=%d anchors=%d status=%s'%(len(docs),len(anchors),dict(st)))
print('ERRORS=%d => DOC-LINE-ANCHORS verdict: %s'%(len(errors),'FAIL' if errors else 'PASS'))
by=collections.Counter(e['code'] for e in errors)
print('by code:',dict(by))
for e in errors[:30]: print('   [%s] %s'%(e['code'],e['detail']))
