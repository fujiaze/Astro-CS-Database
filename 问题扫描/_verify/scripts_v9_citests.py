import re, os, glob
tot=0; sel=0
for f in sorted(glob.glob('ci/tests/test_*.py')):
    t=open(f,encoding='utf-8',errors='ignore').read()
    n=len(re.findall(r'def (test_\w+)', t))
    picked = bool(re.match(r'.*/test_ci001b_.*\.py$', f))
    tot+=n; sel+= n if picked else 0
    print('%-44s test-defs=%-4d %s' % (os.path.basename(f), n, 'CI-BINDING-TESTS 覆盖' if picked else '—'))
print()
print('total test defs:', tot, ' covered by CI-BINDING-TESTS:', sel, ' (%.0f%%)' % (100.0*sel/max(tot,1)))
