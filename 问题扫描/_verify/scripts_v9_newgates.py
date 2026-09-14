import json
d=json.load(open('artifacts/ci/48ceee59fe52/20260913T210003Z-766d84ce/CI_RESULT.json',encoding='utf-8'))
old=set(d['selection']['selected_by'].keys())
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
ALL={'fast','linux-main','windows-main','linux-deep'}
cur=set(c['id'] for c in reg if 'linux-main' in c['profiles'])
print('old linux-main selected', len(old), '-> current', len(cur))
print('NEW in linux-main since run:', sorted(cur-old))
print('REMOVED from linux-main since run:', sorted(old-cur))
for p in ('fast','windows-main','linux-deep'):
    print('==', p, len([c for c in reg if p in c['profiles']]))
print('linux-deep ids:', [c['id'] for c in reg if 'linux-deep' in c['profiles']])
print('gates in NO profile:', [c['id'] for c in reg if not set(c['profiles']) & ALL])
print('profile values seen:', sorted({p for c in reg for p in c['profiles']}))
print()
print('== waivable=True gates and their profiles ==')
for c in reg:
    if c['waivable']: print('  ', c['id'], c['profiles'])
