import json, os, re, pathlib, fnmatch
D='$'+'{'
REPO=pathlib.Path('.').resolve()
ADD=re.compile(r'add_subdirectory\s*\(\s*([^)\s]+)')
NAME=re.compile(r'add_test\s*\(\s*NAME\s+([^\s()#]+)')
seen={}; targets={}
def walk(rel, chain):
    if rel in seen: return
    seen[rel]=chain
    p=REPO/rel
    if not p.is_file(): return
    lines=p.read_text(encoding='utf-8',errors='replace').splitlines()
    for ln in lines:
        if ln.strip().startswith('#'): continue
        for m in NAME.finditer(ln): targets.setdefault(m.group(1).strip().strip('"'), rel)
    txt=chr(10).join(lines)
    for m in ADD.finditer(txt):
        arg=m.group(1)
        if D in arg:
            arg=re.sub(D+r'CMAKE_CURRENT_SOURCE_DIR}','',arg)
            arg=os.path.normpath(os.path.join(os.path.dirname(rel), arg.lstrip('/'))).replace(os.sep,'/')
        cand=os.path.normpath(os.path.join(os.path.dirname(rel), arg)).replace(os.sep,'/')
        while cand.startswith('./'): cand=cand[2:]
        walk(cand+'/CMakeLists.txt', chain+' > '+cand)
walk('CMakeLists.txt','root')
print('reachable cmake files', len(seen))
for k,v in sorted(seen.items()):
    if 'snr' in k or 'ipv/test' in k or 'p1star' in k or 'p1drz' in k: print('  REACH', k, '::', v[:160])
# now full scan (like CTEST-REG) to find unreachable files
SKIP={'run','build','out','artifacts','.git','third_party','node_modules','.venv','__pycache__','BASS DR3','AstroCS.wiki'}
allf=set()
for pat in ('CMakeLists.txt','*.cmake'):
    for p in REPO.rglob(pat):
        rel=str(p.relative_to(REPO)).replace(os.sep,'/')
        if set(rel.split('/')) & SKIP: continue
        if 'archive' in rel or 'superseded' in rel: continue
        allf.add(rel)
unreach=sorted(allf-set(seen))
print()
print('CMake sources NOT reachable from root graph:', len(unreach))
for u in unreach: print('   ', u)
print()
print('targets only in unreachable files:')
for t,f in sorted(targets.items()): pass
allt={}
for u in unreach:
    p=REPO/u
    if not p.is_file(): continue
    for ln in p.read_text(encoding='utf-8',errors='replace').splitlines():
        if ln.strip().startswith('#'): continue
        for m in NAME.finditer(ln): allt.setdefault(m.group(1).strip().strip('"'),u)
only=sorted(set(allt)-set(targets))
print('  unreachable-only target names:', len(only))
for t in only: print('    ', t, '<-', allt[t])
