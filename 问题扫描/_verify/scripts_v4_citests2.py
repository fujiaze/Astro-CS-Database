import json
d=json.load(open("/workspace/Astro CS Database/ci/checks.json",encoding='utf-8'))
for i,c in enumerate(d['checks']):
    j=json.dumps(c.get('command'),ensure_ascii=False)
    if 'ci/tests' in j or 'validate_registry' in j:
        print("HIT", i, c.get('id'), c.get('profiles'), j[:260])
print("---- checks running pytest ----")
for i,c in enumerate(d['checks']):
    j=json.dumps(c.get('command'),ensure_ascii=False)
    if 'pytest' in j: print(i, c.get('id'), j[:200])
print("---- any check mentioning negative_guards/quality suite ----")
for i,c in enumerate(d['checks']):
    j=json.dumps(c,ensure_ascii=False)
    if 'negative_guards' in j or 'test_deep_profiles' in j: print(i,c.get('id'),j[:200])
