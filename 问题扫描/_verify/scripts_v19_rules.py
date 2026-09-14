
import json, os, re, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
impact=json.load(open('ci/impact_map.json',encoding='utf-8'))
by={c['id']:c for c in cs}
PRODS=['BUILD-GCC-RELEASE','CTEST-LINUX-FULL','LINUX-MAIN-BUILD-TREE','LINUX-MAIN-FIXTURES','WIN-BUILD-RELEASE','WIN-PACKAGE-CANDIDATE']
CONS=[c['id'] for c in cs if 'run/ci/build-gcc-release' in ' '.join(c['command']) and c['id'] not in PRODS]
print('consumers of run/ci/build-gcc-release (excl producers):', len(CONS))
print('=== A) impact rules: rules that select >=1 consumer but NO producer ===')
for i,r in enumerate(impact['rules']):
    s=set(r['checks']); cons=s&set(CONS); prod=s&set(PRODS)
    if cons and not prod:
        print('  rule#%d paths=%s -> consumers=%d producers=NONE' % (i, r['paths'], len(cons)))
        print('     consumers:', sorted(cons)[:10])
    elif cons and prod:
        print('  rule#%d paths=%s -> consumers=%d producers=%s OK' % (i, r['paths'][:2], len(cons), sorted(prod)))
print('  rules total:', len(impact['rules']))
print()
print('=== B) do CONSUMERS appear in any rule at all? ===')
inrules=set()
for r in impact['rules']: inrules|=set(r['checks'])
inrules|=set(impact['fallback'])
missing=[c for c in CONS if c not in inrules]
print('  consumers not reachable via impact_map at all: %d/%d' % (len(missing), len(CONS)))
print('  producers reachable:', {p:(p in inrules) for p in PRODS})
print()
print('=== C) the driver script tools/quality/deep_ci_driver.py in changed_paths / impact rules ===')
print('  changed_paths contain it:', [c['id'] for c in cs if any('deep_ci_driver' in p for p in c['changed_paths'])])
print('  impact rules whose paths could match it:', [ (i, r['paths']) for i,r in enumerate(impact['rules']) if any(p in ('tools/quality/**','tools/**') for p in r['paths'])])
for i,r in enumerate(impact['rules']):
    if any(p in ('tools/quality/**','tools/**') for p in r['paths']):
        chk=set(r['checks'])
        print('    rule#%d tools paths=%s selects CTEST/BUILD producers? %s' % (i, r['paths'], sorted(chk&set(PRODS)) or 'NONE'), '| consumers selected:', sorted(chk&set(CONS))[:6])
print()
print('=== D) testdata/ refs (gitignored except index.json) inside CI-loaded code ===')
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
for c in cs:
    roots=roots_for(c)
    hits=[]
    for f in [x for x in sorted(tracked) if x.endswith('.py') and any(x==r or x.startswith(r+'/') for r in roots)]:
        src=open(f,encoding='utf-8',errors='replace').read()
        for i,l in enumerate(src.splitlines(),1):
            if re.search(r'["\x27]testdata/|os\.path\.join\([^)]*"testdata"', l): hits.append((f,i,l.strip()[:90]))
    if hits:
        print('  %-24s %d hits e.g. %s' % (c['id'], len(hits), hits[0]))
print()
print('=== E) L1/L2/L3 for dead changed_paths patterns ===')
for cid,pat in [('PRODUCTION-GRAPH','graph/**'),('ACR-DORMANT','legacy/**'),('GLOSSARY-DOCS','docs/glossary/**'),('LOG-CONTRACT-SELFCHECK','schemas/**'),('TRACEABILITY-MATRIX','schemas/traceability_matrix.schema.json'),('RECONCILE-STATE','engineering/control/CONTROL_TASK_LEDGER.csv')]:
    pre=pat.replace('/**','')
    l1=[t for t in tracked if t.startswith(pre)]
    base=os.path.basename(pat)
    l2=[t for t in tracked if base and base in t]
    print('  %-22s %-46s L1(prefix hits)=%d  L2(basename anywhere)=%d %s' % (cid, pat, len(l1), len(l2), l2[:3]))
