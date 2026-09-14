import json
d=json.load(open("/workspace/Astro CS Database/ci/known_failures.json",encoding='utf-8'))
print("keys:", list(d.keys()))
for f in d.get('failures',[]):
    print("-"*70)
    print("check_id:", f.get('check_id'), "| unit:", f.get('unit'), "| expected:", f.get('expected'), "| category:", f.get('category'))
    print("  reason:", str(f.get('reason'))[:500])
    print("  removal_condition:", str(f.get('removal_condition'))[:300])
print("== other top-level ==")
for k,v in d.items():
    if k not in ('failures','contract'):
        print(k, ":", json.dumps(v,ensure_ascii=False)[:400])
