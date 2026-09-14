
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
impact = json.load(open('ci/impact_map.json', encoding='utf-8'))
by = {c['id']: c for c in cs}
order = {c['id']: i for i, c in enumerate(cs)}

PROD = {'build-tree': ['LINUX-MAIN-BUILD-TREE'], 'fixtures': ['LINUX-MAIN-FIXTURES'],
        'gcc-release': ['BUILD-GCC-RELEASE'], 'ctest-full': ['CTEST-LINUX-FULL'],
        'win-build': ['WIN-BUILD-RELEASE'], 'win-package': ['WIN-PACKAGE-CANDIDATE']}
# consumers identified by build-dir / run-dir / fixture args
def consumers_of(tok):
    return [c['id'] for c in cs if any(a == tok for a in c['command']) or any(o == tok for o in c.get('outputs',[]))]
print('=== consumers by artifact token (from command/outputs) ===')
for tok in ['run/ci/build-gcc-release','run/temp','run/ci/win-candidate','build']:
    ids = [c['id'] for c in cs if any(tok == a for a in c['command'])]
    print('  %-28s -> %d consumers: %s' % (tok, len(ids), ids[:6]))
print()
print('=== rule-level co-selection test: does any impact rule select a consumer without its producer? ===')
producers = {'LINUX-MAIN-BUILD-TREE','LINUX-MAIN-FIXTURES','BUILD-GCC-RELEASE','CTEST-LINUX-FULL','WIN-BUILD-RELEASE','WIN-PACKAGE-CANDIDATE'}
cons_ids = [c['id'] for c in cs if 'run/ci/build-gcc-release' in c['command'] or 'run/temp' in ' '.join(c['command']) or 'run/ci/win-candidate' in ' '.join(c['command'])]
in_rules = set()
for r in impact['rules']:
    s = set(r['checks']); in_rules |= s
    cons = s & set(cons_ids); prod = s & producers
    if cons:
        print('  rule paths=%s : consumers=%d producers=%s' % (r['paths'][:3], len(cons), sorted(prod) or 'NONE'))
print('  fallback producers:', sorted(set(impact['fallback']) & producers), ' fallback consumers:', len(set(impact['fallback']) & set(cons_ids)))
print()
print('=== are CTEST-*/BUILD-*/DEEP-*/WIN-* reachable via impact_map at all? ===')
fam = {}
for c in cs:
    pre = c['id'].split('-')[0]
    fam.setdefault(pre, [0,0])
    fam[pre][0]+=1
    if c['id'] in in_rules: fam[pre][1]+=1
for k,v in sorted(fam.items()): print('  %-12s total=%-4d in_impact_rules=%d' % (k,v[0],v[1]))
print()
print('=== changed_paths containing the producer-relevant dirs (lib/** etc) for consumers ===')
for cid in ['UT-BACKEND','UT-CLI','UT-API','UT-ABI','UT-ARTIFACT','CTEST-P1STAR-MAD','KNOWN-FAILURES-BASELINE-CHECK','CTEST-LINUX-FULL','WIN-TEST-UNIT','LINUX-MAIN-BUILD-TREE','LINUX-MAIN-FIXTURES','BUILD-GCC-RELEASE']:
    c = by[cid]
    print('  %-30s idx=%-4d profiles=%-28s changed_paths=%s' % (cid, order[cid], ','.join(c['profiles']), c['changed_paths']))
