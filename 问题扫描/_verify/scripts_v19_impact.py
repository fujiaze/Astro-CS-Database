
import json
m = json.load(open('ci/impact_map.json', encoding='utf-8'))
print('TOP:', list(m.keys()) if isinstance(m,dict) else type(m))
rules = m.get('rules', [])
print('rules n=', len(rules))
for r in rules:
    print('  %-30s paths=%s' % (r.get('id','?'), r.get('paths')))
    print('        checks(%d)=%s' % (len(r.get('checks',[])), r.get('checks')))
print('fallback n=', len(m.get('fallback',[])), m.get('fallback'))
