
import json, os, re, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
by={c['id']:c for c in cs}
order={c['id']:i for i,c in enumerate(cs)}
PROD = {
 'BUILD-GCC-RELEASE':'run/ci/build-gcc-release',
 'CTEST-LINUX-FULL':'run/ci/build-gcc-release junit',
 'LINUX-MAIN-BUILD-TREE':'build/linux-control + build/astrocs + build/cli/astrocs + libphase2.a',
 'LINUX-MAIN-FIXTURES':'run/temp fixtures',
 'WIN-BUILD-RELEASE':'win build tree',
 'WIN-PACKAGE-CANDIDATE':'artifacts/candidate zip',
}
def pats(cid): return by[cid]['changed_paths']
def covered_by(cid, path):
    return any(path.startswith(p[:-3]) if p.endswith('/**') else path==p for p in pats(cid))
print('=== A) consumers whose changed_paths admit a path that NO producer admits (=> focus selects consumer, not producer) ===')
test_paths = ['modules/astrocs.phase2.upm/module.yaml','cmake/install_layout.cmake','providers/cpu/avx2/src/avx2_provider.cpp','include/astrocs/phase2/upm.h','contracts/schemas/x.json','ci/checks.json','docs/science/NOISE.md','third_party/cfitsio/x.c','packaging/launch/a','runtime/logging/y.cpp','engineering/control/z.md','tests/backend/a.py','lib/phase2/src/upm.cpp','VERSION','tools/quality/deep_ci_driver.py','ci/steps/linux_build_root_graph.sh','testdata/index.json']
for tp in test_paths:
    cons=[c['id'] for c in cs if covered_by(c['id'], tp)]
    prods=[p for p in PROD if covered_by(p, tp)]
    bad = [x for x in cons if x not in PROD and ('run/ci/build-gcc-release' in ' '.join(by[x]['command']) or x.startswith(('UT-','CTEST-','WIN-','DEEP-')))]
    if bad:
        print('  path %-44s consumers-selected=%-3d producers-selected=%s' % (tp, len(bad), prods or '[] NONE'))
        print('      e.g. ', bad[:8])
print()
print('=== B) index of each producer vs its consumers (registry order) ===')
for p in PROD:
    idx=order[p]; prof=by[p]['profiles']
    cons=[c['id'] for c in cs if p.split('-')[0].lower() in ' '.join(c['command']).lower() and c['id']!=p]
    print('  %-24s idx=%-4d profiles=%s' % (p, idx, prof))
print()
print('=== C) gates that run BEFORE any build producer but reference build/** or run/ci/build* ===')
first_build = min(order[k] for k in PROD)
print('  earliest producer index:', first_build, [k for k in PROD if order[k]==first_build])
for c in cs:
    j=' '.join(c['command'])
    if order[c['id']] < first_build and re.search(r'run/ci/build|build/', j):
        print('   EARLY-CONSUMER', c['id'], order[c['id']], j[:90])
print()
print('=== D) gates declared mutates_workspace=false but with an output inside a TRACKED dir ===')
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
trackdirs=set()
for t in tracked:
    pp=t.split('/')
    for i in range(1,len(pp)): trackdirs.add('/'.join(pp[:i]))
for c in cs:
    for o in c['outputs']:
        top=o.split('/')[0]
        if o in tracked or top in trackdirs:
            if not o.startswith('run/'):
                print('  %-26s mutates=%-6s profiles=%-28s output=%s (tracked-dir=%s exists=%s)' % (c['id'], c['mutates_workspace'], ','.join(c['profiles']), o, top in trackdirs, os.path.exists(o)))
print()
print('=== E) dirty_ignore exemption total coverage ===')
print('  checks with dirty_ignore_*:', [c['id'] for c in cs if c.get('dirty_ignore_prefixes') or c.get('dirty_ignore_exact')])
print('  outputs used as dirty exemption (all checks):', sum(len(c['outputs']) for c in cs))
