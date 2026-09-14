
import json, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']; by={c['id']:c for c in cs}
for cid in ['WIN-PACKAGE-CANDIDATE','KNOWN-FAILURES-BASELINE','CTEST-PHASE2-GATES','CTEST-REGISTRATION','UT-QUALITY','WORKSPACE-ADOPTION']:
    c=by[cid]
    print('---',cid,'---')
    for k in ('profiles','platform','command','mutates_workspace','outputs','waivable','prerequisite_tools','ctest_targets','changed_paths','dirty_ignore_prefixes'):
        if k in c: print('   %-22s %s' % (k, json.dumps(c[k],ensure_ascii=False)[:230]))
print()
print('=== fatduck profile/platform census ===')
print('  profiles containing fatduck:', [c['id'] for c in cs if 'fatduck' in c['profiles']] or 'NONE')
print('  platform==fatduck         :', [c['id'] for c in cs if c['platform']=='fatduck'] or 'NONE')
print()
print('=== outputs written OUTSIDE run/ (all checks) ===')
for c in cs:
    for o in c['outputs']:
        if not o.startswith('run/'): print('   %-26s mutates=%-6s %s' % (c['id'], c['mutates_workspace'], o))
print()
print('=== phase2 targets in ctest_baseline.json? ===')
base=open('ci/ctest_baseline.json',encoding='utf-8').read()
import re
names=sorted(set(re.findall(r'"([A-Za-z0-9_.\-]*phase2[A-Za-z0-9_.\-]*)"', base)))
print('  baseline phase2* tokens:', len(names), names[:12])
declared=set()
for c in cs:
    for t in c.get('ctest_targets') or []: declared.add(t)
print('  any gate declaring phase2* :', [t for t in declared if 'phase2' in t] or 'NONE')
print('  ctest_targets-declaring gates:', len([c for c in cs if c.get('ctest_targets')]), '| ctest-gated by command --target:', len([c for c in cs if '--target' in ' '.join(c['command'])]))
