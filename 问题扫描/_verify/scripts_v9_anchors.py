import json, os, re, subprocess, pathlib, collections
root='.'
contract=json.load(open('docs/algorithms/anchors/anchor_contract.json',encoding='utf-8'))
print('CONTRACT keys:', list(contract.keys()))
print('doc_globs:', contract.get('doc_globs'))
print('targets/exempt sample:', {k:(v if not isinstance(v,list) else v[:6]) for k,v in contract.items() if k in ('exemptions','bindings','resolvers')} if False else '')
EXTS=("cpp","cc","cxx","h","hpp","hh","py","sh","ps1","txt","json","yaml","yml","md","cmake","in")
_FILE=r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:"+"|".join(EXTS)+r"))"
_ONE=r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
ANCHOR_RE=re.compile(_FILE+_ONE)
docs=[]
for g in contract.get('doc_globs',[]):
    import glob as G
    docs += G.glob(g)
docs=sorted(set(docs))
print('docs in scope:', len(docs))
def linecount(rel):
    try:
        b=open(rel,'rb').read()
    except OSError: return -1
    t=b.decode('utf-8',errors='replace')
    if t.endswith('\n'): t=t[:-1]
    return len(t.split('\n'))
lc={}
viol=[]
for d in docs:
    for i,line in enumerate(open(d,encoding='utf-8',errors='replace').read().splitlines(),1):
        if '/archive/' in line or '/legacy/' in line: continue
        for m in ANCHOR_RE.finditer(line):
            base=m.group(1); start=int(m.group(2)); end=int(m.group(3)) if m.group(3) else start
            rel=os.path.normpath(base).replace(os.sep,'/')
            if rel.startswith('../'): continue
            if not os.path.isfile(rel): continue   # resolution may still succeed via other rules; skip conservative
            if rel not in lc: lc[rel]=linecount(rel)
            n=lc[rel]
            if n<0: continue
            if not (1<=start<=end<=n):
                viol.append((d,i,rel,start,end,n))
print()
print('C3 range_in_bounds violations (exact-path resolution only):', len(viol))
byfile=collections.Counter(v[2] for v in viol)
print('by target file:', dict(byfile))
for v in viol[:25]: print('   doc=%s:%d target=%s range=%d-%d lines=%d' % v)
