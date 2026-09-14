import json, pathlib, re, fnmatch, sys
REPO = pathlib.Path('.').resolve()
SKIP_DIR_NAMES = {"run","build","out","artifacts",".git","third_party","node_modules",".venv","__pycache__","BASS DR3","AstroCS.wiki"}
SKIP_PATH_SUBSTR = ("archive","superseded")
ADD_TEST_NAME_RE = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")
ADD_TEST_ANY_RE = re.compile(r"(?<![A-Za-z0-9_.])add_test\s*\(")
GLOB_CHARS = "*?["
def is_skip(rel):
    if set(rel.parts) & SKIP_DIR_NAMES: return True
    low = str(rel).replace("\\","/")
    return any(s in low for s in SKIP_PATH_SUBSTR)
found=set()
for pat in ("CMakeLists.txt","*.cmake"):
    for p in REPO.rglob(pat):
        if is_skip(p.relative_to(REPO)): continue
        found.add(p)
sources={}
for p in sorted(found):
    rel = str(p.relative_to(REPO)).replace("\\","/")
    sources[rel] = p.read_text(encoding="utf-8", errors="replace")
targets={}; structural=[]
for rel, raw in sorted(sources.items()):
    text = "\n".join(l for l in raw.splitlines() if not l.lstrip().startswith("#"))
    names = ADD_TEST_NAME_RE.findall(text)
    total = len(ADD_TEST_ANY_RE.findall(text))
    if total != len(names):
        structural.append("C1 %s: add_test calls %d but NAME-form %d" % (rel,total,len(names)))
    for name in names:
        name=name.strip().strip('"')
        if not name: continue
        prev=targets.get(name)
        if prev is not None and prev!=rel:
            structural.append("C1 dup %s: %s vs %s"%(name,prev,rel)); continue
        targets[name]=rel
reg = json.load(open('ci/checks.json',encoding='utf-8'))
base = set(t for t in json.load(open('ci/ctest_baseline.json',encoding='utf-8')).get('targets',[]) if isinstance(t,str) and t)
patterns=[]
for c in reg['checks']:
    for pat in (c.get('ctest_targets') or []):
        if isinstance(pat,str) and pat: patterns.append((c['id'],pat,list(map(str,c.get('command',[])))))
explicit={}; dangling=[]; not_in=[]
for cid,pat,cmd in patterns:
    hits=[t for t in targets if fnmatch.fnmatchcase(t,pat)]
    if not hits: dangling.append(cid+':'+pat)
    for t in hits: explicit.setdefault(t,cid)
    if not any(ch in pat for ch in GLOB_CHARS):
        if pat not in "\n".join(cmd): not_in.append(cid+':'+pat)
registered=set(explicit)|base
unreg=sorted(t for t in targets if t not in registered)
stale=sorted(base-set(targets))
print('SOURCES_SCANNED', len(sources))
print('TARGETS_TOTAL', len(targets), 'BASELINE_TOTAL', len(base), 'EXPLICIT', len(explicit))
print('STRUCTURAL', len(structural))
for s in structural: print('   ', s)
print('UNREGISTERED', len(unreg))
for t in unreg: print('   ', t, '<-', targets[t])
print('STALE_BASELINE', len(stale), stale)
print('DANGLING', len(dangling), dangling)
print('PATTERN_NOT_IN_COMMAND', len(not_in), not_in)
errs = structural + (['C3 unregistered %d'%len(unreg)] if unreg else []) + ['C4 '+x for x in dangling] + ['C5 '+x for x in stale] + ['C6 '+x for x in not_in]
print('VERDICT:', 'FAIL' if errs else 'PASS')
