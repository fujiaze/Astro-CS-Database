import json
d=json.load(open("/workspace/Astro CS Database/ci/checks.json",encoding='utf-8'))
for i,c in enumerate(d['checks']):
    blob=json.dumps(c,ensure_ascii=False)
    if 'validate_registry' in blob or 'UT-ABI' in blob or 'ci/tests' in blob:
        print("-"*80)
        print(i, c.get('id'), c.get('profiles'), '| heavy',c.get('heavy'),'| mon',c.get('requires_monitor'),'| waivable',c.get('waivable'))
        print("  cmd:", json.dumps(c.get('command'),ensure_ascii=False))
        print("  changed_paths:", json.dumps(c.get('changed_paths'))[:300])
