import os, re, subprocess, pathlib, json
tracked=set(x for x in subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines())
D='$'+'{'
# collect root-graph cmake files via walk (reuse expansion)
REPO=pathlib.Path('.')
ADD=re.compile(r'add_subdirectory\s*\(\s*([^)\s]+)')
seen=set()
def walk(rel):
    if rel in seen or not (REPO/rel).is_file(): return
    seen.add(rel)
    txt='\n'.join(l for l in (REPO/rel).read_text(encoding='utf-8',errors='replace').splitlines() if not l.strip().startswith('#'))
    d=os.path.dirname(rel)
    for m in ADD.finditer(txt):
        arg=m.group(1)
        if D in arg:
            if 'CMAKE_CURRENT_SOURCE_DIR}' in arg:
                arg=arg.split('CMAKE_CURRENT_SOURCE_DIR}',1)[1].lstrip('/')
                arg=os.path.normpath(os.path.join(d,'..','..',arg)).replace(os.sep,'/')
            else: continue
        cand=os.path.normpath(os.path.join(d,arg)).replace(os.sep,'/')
        while cand.startswith('./'): cand=cand[2:]
        walk(cand+'/CMakeLists.txt')
walk('CMakeLists.txt')
SRC=re.compile(r'([A-Za-z0-9_./\-]+\.(?:cpp|c|h|hpp|cc|S))')
print('root-graph cmake files:', len(seen))
missing_in_graph=[]
for rel in sorted(seen):
    base=os.path.dirname(rel)
    txt=(REPO/rel).read_text(encoding='utf-8',errors='replace')
    body='\n'.join(l for l in txt.splitlines() if not l.strip().startswith('#'))
    body=re.sub(r'\$<[^>]*>',' ',body)
    for m in SRC.finditer(body):
        tok=m.group(1)
        if tok.startswith('/'): continue
        # resolve candidates
        cands=[os.path.normpath(os.path.join(base,tok)).replace(os.sep,'/'), os.path.normpath(os.path.join(tok)).replace(os.sep,'/')]
        # expand var prefixes crudely
        hit=None
        for c in cands:
            if c.startswith('../'): c=c.lstrip('./')
            if os.path.exists(c): hit=c; break
        if hit and hit not in tracked and os.path.isfile(hit):
            missing_in_graph.append((rel,hit))
print()
print('== files existing on disk but NOT tracked, referenced by tracked root-graph CMake ==')
seenpair=set()
for r,h in missing_in_graph:
    if (r,h) in seenpair: continue
    seenpair.add((r,h)); print('  %-52s -> %s' % (r,h))
print('COUNT', len(seenpair))
print()
print('== p1snr / p1noise CMake source refs tracked? ==')
for f in ['tests/unit/p1snr/CMakeLists.txt','lib/snr_estimator/tests/p1noise/CMakeLists.txt']:
    if not os.path.exists(f): print('  MISSING', f); continue
    txt=open(f,encoding='utf-8',errors='replace').read()
    for m in SRC.finditer(txt):
        tok=m.group(1)
        cands=[os.path.normpath(os.path.join(os.path.dirname(f),tok)).replace(os.sep,'/'), os.path.normpath(tok)]
        found=[c for c in cands if os.path.exists(c)]
        tag='? ' if not found else ('T' if any(c in tracked for c in found) else 'U')
        if tag!='T' or not found: print('  %s %-40s %s -> %s' % (tag, f, tok, found))
