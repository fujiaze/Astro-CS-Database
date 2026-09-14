import json
d=json.load(open("/workspace/Astro CS Database/ci/checks.json",encoding='utf-8'))
for c in d['checks']:
    j=json.dumps(c.get('command'),ensure_ascii=False)
    if 'ctest-full' in j or 'ctest_target' in j or 'ctest-target' in j:
        print(c['id'], c.get('profiles'), j[:150])
