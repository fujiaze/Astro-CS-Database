import json
p="/workspace/Astro CS Database/ci/checks.json"
d=json.load(open(p,encoding='utf-8'))
print("TOPKEYS:", list(d.keys())[:20])
def walk(obj, path=""):
    if isinstance(obj, dict):
        if 'id' in obj and ('command' in obj or 'commands' in obj):
            yield path, obj
        for k,v in obj.items():
            yield from walk(v, path+"/"+str(k))
    elif isinstance(obj, list):
        for i,v in enumerate(obj):
            yield from walk(v, path+f"[{i}]")
checks = list(walk(d))
print("NCHECKS:", len(checks))
import re
for path,c in checks:
    cid = str(c.get('id'))
    blob = json.dumps(c, ensure_ascii=False)
    if re.search(r'ABI|WIN-TEST-UNIT|discover', blob):
        print("-"*90)
        print("id:", cid, "| path:", path)
        for k in ('command','commands','timeout_seconds','waivable','profiles','platform','enabled','when'):
            if k in c: print("   ",k,":",json.dumps(c[k],ensure_ascii=False)[:600])
