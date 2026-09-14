import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
for c in reg:
    cmd=[str(a) for a in c['command']]
    if 'ctest-target' in cmd:
        if not c.get('ctest_targets'): print('NO ctest_targets field:', c['id'], 'target=', cmd[cmd.index('--target')+1])
        if '--fail-if-no-tests' in cmd: print('has fail-if-no-tests:', c['id'])
print('--- gates with ctest_targets but no --target in command ---')
for c in reg:
    if c.get('ctest_targets') and '--target' not in [str(a) for a in c['command']]:
        print('   ', c['id'], c['ctest_targets'], ' '.join(map(str,c['command']))[:90])
print()
print('--- UT-* non-waivable count ---')
print(sum(1 for c in reg if c['id'].startswith('UT-')), [c['id'] for c in reg if c['id'].startswith('UT-') and c['waivable']])
print('--- linux-main non-waivable gates with no evidence ---')
ev=set()
import glob, os
for rd in glob.glob('artifacts/ci/*/*/checks/*.json'):
    try: ev.add(json.load(open(rd,encoding='utf-8')).get('id'))
    except Exception: pass
noev=[c['id'] for c in reg if 'linux-main' in c['profiles'] and not c['waivable'] and c['id'] not in ev]
print(len(noev), noev)
