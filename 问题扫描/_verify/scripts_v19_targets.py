
import json, os, re, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
# collect add_test names from tracked CMakeLists (add_test(NAME x ...) or add_test(x ...))
names=set()
for f in tracked:
    if not f.endswith('CMakeLists.txt') and not f.endswith('.cmake'): continue
    src=open(f,encoding='utf-8',errors='replace').read()
    for m in re.finditer(r'add_test\s*\(\s*(?:NAME\s+)?([A-Za-z0-9_.\-]+)', src):
        names.add(m.group(1))
    for m in re.finditer(r'astrocs_add[_a-z]*test[_a-z]*\s*\(\s*([A-Za-z0-9_.\-]+)', src):
        names.add(m.group(1))
base=json.load(open('ci/ctest_baseline.json',encoding='utf-8'))
def flatten(o, acc):
    if isinstance(o,str): acc.add(o)
    elif isinstance(o,dict):
        for v in o.values(): flatten(v,acc)
    elif isinstance(o,list):
        for v in o: flatten(v,acc)
bl=set(); flatten(base,bl)
print('add_test-like names found in tracked cmake files:', len(names), '| ctest_baseline.json string tokens:', len(bl))
print()
print('=== per ctest-target gate: does its --target exist as an add_test name / glob family? ===')
miss=[]
for c in cs:
    j=' '.join(c['command'])
    m=re.search(r'--target (\S+)', j)
    if not m: continue
    tg=m.group(1)
    if tg.endswith('.*'):
        pre=tg[:-2]; matched={n for n in names if n.startswith(pre)}
        kind='GLOB'
    else:
        matched={n for n in names if n==tg}; kind='EXACT'
    inbase = tg in bl or any(tg in str(x) for x in [])
    if not matched:
        miss.append((c['id'],tg,kind,inbase, sorted(x for x in bl if tg.split('_')[0] in x)[:5]))
    print('  %-38s %-34s %-5s cmake=%-4s in_baseline=%s' % (c['id'], tg, kind, len(matched), inbase))
print()
print('=== targets with ZERO cmake add_test hit ===')
for x in miss: print('  ', x)
