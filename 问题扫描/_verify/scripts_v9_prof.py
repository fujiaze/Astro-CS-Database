import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('gate -> profiles')
for c in reg:
    if 'linux-deep' in c['profiles'] or 'fast' not in c['profiles'] and 'linux-main' in c['profiles']: pass
print('BUILD-GCC-RELEASE', [c['profiles'] for c in reg if c['id']=='BUILD-GCC-RELEASE'])
print('DEEP-* :', {c['id']:c['profiles'] for c in reg if c['id'].startswith('DEEP')})
print('linux-main only (not fast):', [c['id'] for c in reg if 'linux-main' in c['profiles'] and 'fast' not in c['profiles']][:20])
print('total in fast:', sum(1 for c in reg if 'fast' in c['profiles']))
