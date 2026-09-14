import json,re
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
sc=[]; real={}
for c in d:
    cmd=" ".join(c.get("command",[]))
    m=re.search(r"([\w/]+\.py)\s+(.*)$", cmd)
    if not m: continue
    script=m.group(1); rest=m.group(2)
    isself=bool(re.search(r"--selftest|--selfcheck|--self-only", rest))
    real[script]=real.get(script,0)+(0 if isself else 1)
    if isself: sc.append((c["id"],script,c.get("profiles"),c.get("waivable"),real_mode:=None))
print("=== CI gates that pass --selftest/--selfcheck ===")
for cid,script,prof,wv,_ in sc:
    print("  %-28s %-46s real_mode_ci_entries=%d profiles=%s waivable=%s"%(cid,script,real.get(script,0),prof,wv))
print()
print("total gates in checks.json:",len(d))