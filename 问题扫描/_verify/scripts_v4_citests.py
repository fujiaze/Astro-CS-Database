import json, os
REPO="/workspace/Astro CS Database"
d=json.load(open(REPO+"/ci/checks.json",encoding='utf-8'))
print("== checks whose command mentions ci/tests or test_workflow or UT-CTEST ==")
for i,c in enumerate(d['checks']):
    cmd=c.get('command') or []
    j=json.dumps(cmd,ensure_ascii=False)
    if 'ci/tests' in j or 'test_workflow' in j or '-s' in j and 'ci' in j:
        print(i, c.get('id'), c.get('profiles'), j[:220])
print()
print("== all checks with id containing CI/REGISTRY ==")
for i,c in enumerate(d['checks']):
    cid=str(c.get('id'))
    if any(k in cid for k in ('CI','REGISTRY','WORKFLOW')):
        print(i, cid, c.get('profiles'), json.dumps(c.get('command'),ensure_ascii=False)[:200])
print()
print("== ci/tests dir listing ==")
for f in sorted(os.listdir(REPO+'/ci/tests')):
    print('   ', f)
