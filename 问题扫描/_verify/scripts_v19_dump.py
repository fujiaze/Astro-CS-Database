
import json, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
print('TOP TYPE:', type(d).__name__)
if isinstance(d, dict):
    for k, v in d.items():
        n = len(v) if hasattr(v, '__len__') else v
        print(' KEY', repr(k), type(v).__name__, n)
checks = d['checks'] if isinstance(d, dict) and 'checks' in d else d
print('CHECKS LEN:', len(checks))
keys = collections.Counter()
for c in checks:
    for k in c: keys[k] += 1
print('FIELD FREQ of %d:' % len(checks))
for k, n in keys.most_common(): print('  %-28s %d' % (k, n))
print('ID ORDER:')
for i, c in enumerate(checks): print('%3d %s' % (i, c.get('id')))
