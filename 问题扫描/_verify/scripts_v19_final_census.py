
import json, subprocess, collections, os
raw=open('ci/checks.json',encoding='utf-8').read()
d=json.loads(raw); cs=d['checks']
print('HEAD:', subprocess.check_output(['git','--no-optional-locks','rev-parse','HEAD']).decode().strip())
print('checks.json worktree mtime:', __import__('datetime').datetime.utcfromtimestamp(os.path.getmtime('ci/checks.json')).isoformat()+'Z')
print('vs HEAD blob identical?', subprocess.run(['git','--no-optional-locks','diff','--quiet','HEAD','--','ci/checks.json'],capture_output=True).returncode==0)
print('n checks:', len(cs))
freq=collections.Counter()
for c in cs:
    for k in c: freq[k]+=1
print('field freq:', dict(freq))
print('profiles census:', {p:sum(1 for c in cs if p in c['profiles']) for p in ('fast','linux-main','windows-main','linux-deep','fatduck')})
print('platform census:', dict(collections.Counter(c['platform'] for c in cs)))
tg=[c['id'] for c in cs if c.get('ctest_targets')]
pt=[c['id'] for c in cs if c.get('prerequisite_tools')]
print('ctest_targets gates:', len(tg), '| prerequisite_tools gates:', len(pt), pt)
print('gates with --target:', len([c for c in cs if '--target' in ' '.join(c['command'])]))
print('gates whose --target value not in its own ctest_targets:', [(c['id'], [t for t in c['command']][ [i for i,x in enumerate(c['command']) if x=='--target'][0]+1 ]) for c in cs if '--target' in c['command'] and not c.get('ctest_targets')])
print('outputs outside run/:', [(c['id'],o) for c in cs for o in c['outputs'] if not o.startswith('run/')])
print('dirty_ignore:', [(c['id'],c.get('dirty_ignore_prefixes'),c.get('dirty_ignore_exact')) for c in cs if c.get('dirty_ignore_prefixes') or c.get('dirty_ignore_exact')])
print('mutates_workspace true:', [c['id'] for c in cs if c['mutates_workspace']])
print('waivable true:', len([c for c in cs if c['waivable']]))
