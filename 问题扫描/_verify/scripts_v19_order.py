
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}

print('=== A) per-profile execution order (filtered by registry order) : producer vs consumer ===')
PRODUCERS = {'LINUX-MAIN-FIXTURES':'run/temp fixtures','LINUX-MAIN-BUILD-TREE':'build/linux-control + build/astrocs','BUILD-GCC-RELEASE':'run/ci/build-gcc-release','CTEST-LINUX-FULL':'run/ci/build-gcc-release + ctest-full.junit.xml','WIN-BUILD-RELEASE':'win build tree','WIN-PACKAGE-CANDIDATE':'run/ci/win-candidate + artifacts/candidate zip'}
CONS = {}
for c in cs:
    j = ' '.join(c['command'])
    for tok, what in [('run/ci/build-gcc-release','gcc release build tree'),('run/ci/win-candidate','win candidate tree'),('build/astrocs','root build/astrocs')]:
        if tok in j: CONS.setdefault(c['id'], []).append(what)
for prof in ['fast','linux-main','linux-deep','windows-main']:
    seq = [c['id'] for c in cs if prof in c['profiles']]
    idx = {cid:i for i,cid in enumerate(seq)}
    print('  profile %-13s n=%-4d producers present=%s' % (prof, len(seq), sorted(set(seq)&set(PRODUCERS))))
    for cid in seq:
        if cid in ('KNOWN-FAILURES-BASELINE-CHECK','WIN-CANDIDATE-VALIDATE') or cid in CONS:
            need = CONS.get(cid, [])
            for dep in (['CTEST-LINUX-FULL'] if cid=='KNOWN-FAILURES-BASELINE-CHECK' else []):
                if dep in idx: print('     %-34s consumer idx=%-4d producer %-20s idx=%-4d  ORDER_OK=%s' % (cid, idx[cid], dep, idx[dep], idx[dep]<idx[cid]))
                else: print('     %-34s consumer idx=%-4d producer %s NOT IN PROFILE' % (cid, idx[cid], dep))
    for cid in seq:
        if cid not in CONS: continue
        for prod, art in PRODUCERS.items():
            if prod in idx and idx[prod] > idx[cid]:
                print('     !! producer AFTER consumer: %s(%d) after %s(%d)' % (prod, idx[prod], cid, idx[cid]))
print()
print('=== B) linux-main: consumers of build/ root tree (LINUX-MAIN-BUILD-TREE) ===')
seq = [c['id'] for c in cs if 'linux-main' in c['profiles']]
idx = {cid:i for i,cid in enumerate(seq)}
for cid in seq:
    c = [x for x in cs if x['id']==cid][0]
    files=[]
    cmd=c['command']
    if 'discover' in cmd:
        s = cmd[cmd.index('-s')+1]
        files = sorted(t for t in tracked if t.startswith(s+'/') and t.endswith('.py'))
    dep_build = any('build' in open(f,encoding='utf-8',errors='replace').read() for f in files[:200]) if files else ('build/' in ' '.join(cmd))
    if dep_build and cid.startswith('UT'):
        print('  %-16s idx=%-4d (reads build/ in suite; LINUX-MAIN-BUILD-TREE idx=%s)' % (cid, idx[cid], idx.get('LINUX-MAIN-BUILD-TREE')))
