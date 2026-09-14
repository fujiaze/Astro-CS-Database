
import json, os, subprocess, re, glob
d = json.load(open('ci/checks.json', encoding='utf-8'))
cs = d['checks']
tracked = [t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split('\0') if t]
tset=set(tracked)
tdirs=set()
for t in tracked:
    p=t.split('/')
    for i in range(1,len(p)): tdirs.add('/'.join(p[:i]))

print('=== A) unittest discover targets: tracked dir? #tracked test files ===')
for c in cs:
    cmd=c['command']
    if 'discover' in cmd:
        s = cmd[cmd.index('-s')+1] if '-s' in cmd else None
        pat = cmd[cmd.index('-p')+1] if '-p' in cmd else '*_test.py/test_*.py default'
        n = sum(1 for t in tracked if t.startswith(s+'/') and t.endswith('.py'))
        npat = sum(1 for t in tracked if t.startswith(s+'/') and re.match(r'(^|/)test_', os.path.basename(t)))
        print('  %-24s dir=%-32s tracked_dir=%-5s py_files=%-3d test_named=%-3d p=%s' % (c['id'], s, str(s in tdirs), n, npat, cmd[cmd.index('-p')+1] if '-p' in cmd else '-'))

print()
print('=== B) gitignore status of paths referenced (run/, artifacts/, build/, evidence/, reports/) ===')
gi = open('.gitignore', encoding='utf-8', errors='replace').read()
for cand in ['run/ci/build-gcc-release/ctest-full.junit.xml','artifacts/KNOWN_FAILURES_BASELINE.json','run/ci/ctest/aio_abi_units.json','reports/v19r2/evidence/quality/traceability_check.json','evidence/x.json','build/astrocs','artifacts/candidate/AstroCS-candidate.zip','run/ci/complexity/complexity.json','run/ci/win-candidate']:
    r = subprocess.run(['git','--no-optional-locks','check-ignore','-v','--stdin'], input=cand.encode(), capture_output=True)
    print('  %-58s ignored=%s  %s' % (cand, r.returncode==0, r.stdout.decode().strip()[:90]))

print()
print('=== C) dirty_ignore / mutates_workspace / outputs non-tracked (declared outputs) ===')
for c in cs:
    if c.get('dirty_ignore_prefixes') or c.get('dirty_ignore_exact'):
        print('  DIRTY-IGNORE', c['id'], c.get('dirty_ignore_prefixes'), c.get('dirty_ignore_exact'), 'mutates=',c['mutates_workspace'], 'waivable=',c['waivable'])
outs_nonrun = {}
for c in cs:
    for o in c['outputs']:
        if not (o=='run' or o.startswith('run/')):
            outs_nonrun.setdefault(o,[]).append(c['id'])
print('  outputs outside run/ :')
for o,ids in sorted(outs_nonrun.items()):
    ign = subprocess.run(['git','--no-optional-locks','check-ignore','-q','-v','--stdin'], input=o.encode(), capture_output=True).returncode==0
    print('    %-46s ids=%s tracked=%s exists=%s gitignored=%s' % (o, ids, o in tset, os.path.exists(o), ign))
