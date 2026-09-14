import json, os, re, pathlib
REPO = pathlib.Path('.').resolve()
ADD_RE = re.compile(r'add_subdirectory\s*\(\s*([^)\s]+)')
NAME_RE = re.compile(r'add_test\s*\(\s*NAME\s+([^\s()#]+)')
IF_RE = re.compile(r'^\s*if\s*\(', re.I)
END_RE = re.compile(r'^\s*(endif|else|elseif)\b', re.I)
DYN = '${'
targets={}; seen=set(); subdirs_dyn=[]; missing_sub=[]; visited=[]
def scan(rel):
    p = REPO/rel
    if not p.is_file(): return {}
    out={}; depth=0
    for ln in p.read_text(encoding='utf-8', errors='replace').splitlines():
        s=ln.strip()
        if s.startswith('#'): continue
        if IF_RE.match(ln): depth+=1
        elif END_RE.match(ln): depth=max(0,depth-1)
        for mm in NAME_RE.finditer(ln):
            out[mm.group(1).strip().strip('"')]=(rel, depth>0)
    return out
def walk(rel, guardchain):
    if rel in seen: return
    seen.add(rel); visited.append(rel)
    d=os.path.dirname(rel)
    for k,v in scan(rel).items():
        targets[k]=(v[0], guardchain or v[1])
    txt=(REPO/rel).read_text(encoding='utf-8', errors='replace')
    for m in ADD_RE.finditer(txt):
        arg=m.group(1)
        if DYN in arg:
            subdirs_dyn.append((rel,arg)); continue
        cand=os.path.normpath(os.path.join(d,arg)).replace(os.sep,'/')
        while cand.startswith('./'): cand=cand[2:]
        sub=cand+'/CMakeLists.txt'
        if not (REPO/sub).is_file():
            missing_sub.append((rel,cand)); continue
        walk(sub, False)
walk('CMakeLists.txt', False)
print('ROOT-GRAPH CMakeLists visited:', len(visited))
print('targets defined in reachable files:', len(targets))
print('DYNAMIC add_subdirectory:', subdirs_dyn)
print('MISSING subdir targets (would break configure):', missing_sub)
print()
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('== CTEST-gate target reachability ==')
bad=[]
for c in reg:
    for t in c.get('ctest_targets',[]):
        if t in targets:
            srcf,guarded=targets[t]
            print('  %-4s %-34s %-40s %s%s' % ('GUARD' if guarded else 'OK', c['id'], t, srcf, ''))
        else:
            print('  NO   %-34s %-40s NOT-IN-ROOT-GRAPH' % (c['id'], t)); bad.append((c['id'],t))
print('UNREACHABLE:', len(bad), bad)
print('reachable-with-guard:', sum(1 for c in reg for t in c.get('ctest_targets',[]) if t in targets and targets[t][1]))
