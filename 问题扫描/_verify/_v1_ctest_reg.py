import json,re,pathlib,fnmatch
REPO=pathlib.Path('.')
SKIP={'run','build','out','artifacts','.git','third_party','node_modules','.venv','__pycache__','BASS DR3','AstroCS.wiki'}
SUB=('archive','superseded')
def skipped(rel):
    if set(rel.parts)&SKIP: return True
    return any(s in str(rel).replace('\\','/') for s in SUB)
NAME=re.compile(r'add_test\s*\(\s*NAME\s+([^\s()#]+)')
ANY=re.compile(r'(?<![A-Za-z0-9_.])add_test\s*\(')
targets={}; errors=[]
for pat in ('CMakeLists.txt','*.cmake'):
    for p in REPO.rglob(pat):
        rel=p.relative_to(REPO)
        if skipped(rel): continue
        raw=p.read_text(encoding='utf-8',errors='replace')
        text='\n'.join(l for l in raw.splitlines() if not l.lstrip().startswith('#'))
        names=NAME.findall(text); total=len(ANY.findall(text))
        if total!=len(names): errors.append('C1 %s: add_test %d vs NAME %d'%(rel,total,len(names)))
        for n in names:
            n=n.strip('"')
            if n: targets[n]=str(rel)
reg=json.load(open('ci/checks.json',encoding='utf-8')); base=set(t for t in json.load(open('ci/ctest_baseline.json',encoding='utf-8'))['targets'] if isinstance(t,str))
pats=[]
for c in reg.get('checks',[]):
    for pp in c.get('ctest_targets',[]) or []:
        if isinstance(pp,str) and pp: pats.append((c['id'],pp,list(c.get('command',[]))))
explicit={}
for cid,pp,cmd in pats:
    for t in targets:
        if fnmatch.fnmatchcase(t,pp): explicit[t]=cid
unreg=sorted(t for t in targets if t not in set(explicit)|base)
print('TOTAL_TARGETS',len(targets),'BASELINE',len(base),'PATTERNS',len(pats))
print('C1 errors:',errors[:10])
print('UNREGISTERED',len(unreg))
for t in unreg: print('  ',t,'<-',targets[t])