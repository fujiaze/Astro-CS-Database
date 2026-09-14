import json, os, re, pathlib, fnmatch
D='$'+'{'
REPO=pathlib.Path('.').resolve()
ADD=re.compile(r'add_subdirectory\s*\(\s*([^)\s]+)')
NAME=re.compile(r'add_test\s*\(\s*NAME\s+([^\s()#]+)')
seen=set(); targets={}
def walk(rel):
    if rel in seen: return
    seen.add(rel)
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
            if arg.startswith(D+'CMAKE_CURRENT_SOURCE_DIR}'):
                rest=arg.split('}',1)[1].lstrip('/')
                cand=os.path.normpath(os.path.join(os.path.dirname(rel), rest)).replace(os.sep,'/')
                while cand.startswith('../'): cand=cand[3:]
                arg=cand
            else:
                continue
        cand=os.path.normpath(os.path.join(os.path.dirname(rel), arg)).replace(os.sep,'/')
        while cand.startswith('./'): cand=cand[2:]
        walk(cand+'/CMakeLists.txt')
walk('CMakeLists.txt')
print('reachable cmake files:', len(seen))
print('targets in root graph:', len(targets))
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
NEW=('CTEST-P1SNR-SCIENCE-ALL','CTEST-P1SNR-LINUX-ALL','CTEST-P1DRZ-TASKSET-INVARIANCE','CTEST-P1STAR-ANGLE-GUARD','CTEST-IPV-TRIANGLE-BUDGET')
for c in reg:
    if c['id'] not in NEW: continue
    print()
    print(c['id'], '| profiles', c['profiles'], '| waivable', c['waivable'], '| platform', c['platform'])
    print('  cmd:', ' '.join(map(str,c['command'])))
    for pat in c.get('ctest_targets',[]):
        hits=[t for t in targets if fnmatch.fnmatchcase(t,pat)]
        print('  pattern', pat, '-> matched IN ROOT GRAPH:', hits)
alltgt=set(targets)
print()
print('ALL add_test in cmake files scanned by CTEST-REG (approx 202) vs root-graph:', len(alltgt))
