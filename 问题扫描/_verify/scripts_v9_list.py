import json, os, re, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
ids = [c['id'] for c in ch]
print('TOTAL', len(ch), 'UNIQUE', len(set(ids)))
cnt = collections.Counter(ids)
print('DUPES', [i for i,n in cnt.items() if n>1])
pf = collections.Counter()
for c in ch:
    for p in c['profiles']: pf[p]+=1
print('PROFILES', dict(pf))
print('PLATFORM', dict(collections.Counter(c['platform'] for c in ch)))
print('WAIVABLE', dict(collections.Counter(bool(c['waivable']) for c in ch)))
print('NONWAIVABLE %d:' % sum(1 for c in ch if not c['waivable']), json.dumps([c['id'] for c in ch if not c['waivable']]))
print('HEAVY', dict(collections.Counter(bool(c.get('heavy')) for c in ch)))
print('MUTATES', dict(collections.Counter(bool(c.get('mutates_workspace')) for c in ch)))
print('REQUIRES_MONITOR', dict(collections.Counter(bool(c.get('requires_monitor')) for c in ch)))
print('OUTPUTS_NONEMPTY', sum(1 for c in ch if c.get('outputs')))
print('WITH_CTEST_TARGETS', sum(1 for c in ch if 'ctest_targets' in c))
print('WITH_PREREQ', sum(1 for c in ch if 'prerequisite_tools' in c))
print()
for c in ch:
    print('%-32s %-12s w=%-5s %s' % (c['id'], c['platform'], str(c['waivable'])[:5], ' '.join(map(str,c['command']))[:220]))
