import json, collections
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('== platform vs profile mismatch (=> FAIL(prerequisite) on the other platform) ==')
bad=[]
for c in reg:
    p=c['platform']
    for prof in c['profiles']:
        if p=='linux' and prof=='windows-main': bad.append((c['id'],p,prof,c['waivable']))
        if p=='windows' and prof in ('linux-main','fast'): bad.append((c['id'],p,prof,c['waivable']))
for b in bad: print('  ', b)
print('COUNT', len(bad))
print()
ln=set(c['id'] for c in reg if 'linux-main' in c['profiles'])
wn=set(c['id'] for c in reg if 'windows-main' in c['profiles'])
fa=set(c['id'] for c in reg if 'fast' in c['profiles'])
print('linux-main=%d windows-main=%d fast=%d'%(len(ln),len(wn),len(fa)))
print('in linux-main NOT windows-main:', len(ln-wn))
print('in windows-main NOT linux-main:', sorted(wn-ln))
print('in windows-main NOT fast:', len(wn-fa))
print()
print('platform histogram per profile:')
for prof in ('fast','linux-main','windows-main','linux-deep'):
    print(' ', prof, dict(collections.Counter(c['platform'] for c in reg if prof in c['profiles'])))
