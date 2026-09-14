import json, collections, os, subprocess
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('total',len(reg),'unique',len({c['id'] for c in reg}))
print('waivable',[c['id'] for c in reg if c['waivable']])
print('profiles',dict(collections.Counter(p for c in reg for p in c['profiles'])))
print('platform',dict(collections.Counter(c['platform'] for c in reg)))
print('mutates_workspace',[c['id'] for c in reg if c.get('mutates_workspace')])
print('heavy',sum(1 for c in reg if c.get('heavy')),'monitor',sum(1 for c in reg if c.get('requires_monitor')))
print('outputs non-empty',sum(1 for c in reg if c.get('outputs')),'prereq_tools',sum(1 for c in reg if c.get('prerequisite_tools')))
print('ctest_targets',sum(1 for c in reg if c.get('ctest_targets')),'ctest-target cmds',sum(1 for c in reg if 'ctest-target' in ' '.join(map(str,c['command']))))
print('UT gates',sum(1 for c in reg if c['id'].startswith('UT-')))
print('all have changed_paths:', all(c['changed_paths'] for c in reg), 'all have profiles:', all(c['profiles'] for c in reg))
# command .py targets existence
missing=[]
for c in reg:
    for a in map(str,c['command']):
        if a.endswith('.py') and not a.startswith('-'):
            if not os.path.exists(a) and '*' not in a: missing.append((c['id'],a))
print('missing .py targets:', missing)
kf=json.load(open('ci/known_failures.json',encoding='utf-8'))
print('KF failures:', [(f.get('check_id') or f.get('target'), f.get('kind'), f.get('expected')) for f in kf['failures']], 'removals', len(kf.get('removals',[])))
