import json
d=json.load(open("/workspace/Astro CS Database/ci/checks.json",encoding='utf-8'))
hits=[c['id'] for c in d['checks'] if 'validate_registry' in json.dumps(c.get('command'),ensure_ascii=False)]
print("checks referencing validate_registry:", hits)
bind=[c for c in d['checks'] if c['id']=='CI-BINDING-TESTS'][0]
print("CI-BINDING-TESTS command:", json.dumps(bind.get('command'),ensure_ascii=False))
abi=[c for c in d['checks'] if c['id']=='UT-ABI'][0]
print("UT-ABI:", json.dumps(abi.get('command'),ensure_ascii=False), abi.get('profiles'), abi.get('waivable'), abi.get('timeout_s'))
