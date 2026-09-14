import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
im=json.load(open('ci/impact_map.json',encoding='utf-8'))
mapped=set(); rules={}
for rule in im.get('rules',[]):
    for cid in rule.get('checks',[]): mapped.add(cid); rules.setdefault(cid,[]).append(rule.get('id') or rule.get('paths'))
fb=set(im.get('fallback',[]))
fast=[c['id'] for c in reg if 'fast' in c['profiles']]
lm=[c['id'] for c in reg if 'linux-main' in c['profiles']]
print('fast gates NOT reachable via --changed-from (不在 rules 且不在 fallback):', len([x for x in fast if x not in mapped and x not in fb]))
print('   ', [x for x in fast if x not in mapped and x not in fb])
print()
print('linux-main gates unreachable via --changed-from:', len([x for x in lm if x not in mapped and x not in fb]))
print('   sample:', [x for x in lm if x not in mapped and x not in fb][:40])
print()
print('=== rules touching docs/TRACEABILITY.csv or docs/** ===')
for rule in im.get('rules',[]):
    ps=rule.get('paths',[])
    if any('docs' in p for p in ps): print('  ', rule.get('id'), ps, '->', len(rule.get('checks',[])), 'checks')
print()
print('=== fallback ids ==='); print('  ', sorted(fb))
print()
print('=== CON-TRACEABILITY / TRACEABILITY-MATRIX / DOC-LINE-ANCHORS reachable at all? ===')
for cid in ('CON-TRACEABILITY','TRACEABILITY-MATRIX','DOC-LINE-ANCHORS','CTEST-REGISTRATION','UT-BACKEND','CON-API-CONTRACTS','CON-COMMENTS','CON-FULL-INTEGRATION','KNOWN-FAILURES-BASELINE-CHECK'):
    print('  %-32s rules=%s fallback=%s' % (cid, rules.get(cid,'—'), cid in fb))
