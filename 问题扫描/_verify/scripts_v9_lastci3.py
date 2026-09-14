import json, collections
d=json.load(open('artifacts/ci/48ceee59fe52/20260913T210003Z-766d84ce/CI_RESULT.json',encoding='utf-8'))
rows=d['checks']
print('run source_sha', d['source_sha'][:12], 'started', d['started_utc'], 'profile', d['profile'])
m=collections.Counter(r['verdict'] for r in rows)
print('VERDICTS', dict(m))
print('ids in run:', len(rows))
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
cur=set(c['id'] for c in reg if 'linux-main' in c['profiles'])
ran=set(r['id'] for r in rows)
print('no evidence (added since):', sorted(cur-ran))
print('evidence but not in linux-main now:', sorted(ran-cur))
print()
for r in rows:
    if r['verdict']!='PASS': print(' NONPASS', r['id'], r['verdict'], str(r.get('reason'))[:90])
print()
print('gate exit codes distribution:', dict(collections.Counter(str(r.get('exit_code')) for r in rows)))
# how many had empty stdout AND stderr while PASS (silent)
silent=[r['id'] for r in rows if r['verdict']=='PASS' and not (r.get('stdout_tail') or '').strip() and not (r.get('stderr_tail') or '').strip()]
print('PASS with EMPTY stdout+stderr:', len(silent), silent)
