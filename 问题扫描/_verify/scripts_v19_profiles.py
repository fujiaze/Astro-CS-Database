
import json, collections
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
byprof = collections.defaultdict(list)
for c in cs:
    for p in c['profiles']: byprof[p].append(c['id'])
print('=== profile membership counts ===')
for p,v in byprof.items(): print('  %-14s %d' % (p, len(v)))
print()
print('=== checks whose ONLY profile is X (unreachable if no job runs X) ===')
for p in ['fast','fatduck','linux-deep','linux-main','windows-main']:
    only=[c['id'] for c in cs if c['profiles']==[p]]
    print('  profiles==[%s] only: %d -> %s' % (p, len(only), only[:20]))
print()
print('=== checks in fast but NOT in linux-main/windows-main/linux-deep ===')
sel=[c['id'] for c in cs if 'fast' in c['profiles'] and not (set(c['profiles']) & {'linux-main','windows-main','linux-deep'})]
print('  ', len(sel), sel)
print()
print('=== any check with platform=fatduck or profile fatduck ===')
print('  platform==fatduck:', [c['id'] for c in cs if c['platform']=='fatduck'])
print('  profile contains fatduck:', [c['id'] for c in cs if 'fatduck' in c['profiles']])
print()
print('=== linux-deep membership (schedule-only) ===')
print('  ', byprof['linux-deep'])
