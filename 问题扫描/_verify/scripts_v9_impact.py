import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
ids=[c['id'] for c in reg]
im=json.load(open('ci/impact_map.json',encoding='utf-8'))
mapped=set()
for rule in im.get('rules',[]): mapped |= set(rule.get('checks',[]))
fb=set(im.get('fallback',[]))
print('impact_map rules:', len(im.get('rules',[])), 'mapped ids:', len(mapped), 'fallback:', len(fb))
print('GATES NOT IN impact_map at all:', len(set(ids)-mapped-fb))
print('  ', sorted(set(ids)-mapped-fb))
print()
print('IDS IN impact_map NOT IN registry (悬空):', sorted((mapped|fb)-set(ids)))
