import os,re,json
root="/workspace/Astro CS Database"
print("(a) 0.73167 在 findings/账本 是否已报：")
for base in ["问题扫描/findings","问题扫描/账本","问题扫描/_merge"]:
    for dp,dn,fn in os.walk(os.path.join(root,base)):
        for f in fn:
            p=os.path.join(dp,f)
            try: s=open(p,encoding="utf-8",errors="ignore").read()
            except Exception: continue
            for i,l in enumerate(s.splitlines(),1):
                if "0.73167" in l:
                    print(f"   {os.path.relpath(p,root)}:{i}: {l.strip()[:170]}")
print()
d=json.load(open(os.path.join(root,"ci/checks.json")))
ks=d['checks'] if isinstance(d,dict) and 'checks' in d else d
print("(b) checks.json 中含 machine/consistency 的项：")
for c in ks:
    s=json.dumps(c,ensure_ascii=False)
    if 'docs_machine' in s or 'consistency' in s.lower():
        print("   ",c.get('id'),"|",str(c.get('command'))[:110],"| waivable=",c.get('waivable'))
print("   TOTAL checks:",len(ks))
print()
tr=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
print("(c) 200000 字面副本（生产/合同/文档，排除 run/与报告）：")
for f in sorted(tr):
    if not f.startswith(("lib/","include/","contracts/","docs/algorithms","docs/contracts")): continue
    try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
    except Exception: continue
    for i,l in enumerate(lines,1):
        if "200000" in l or "MAX_STARS_RESULT" in l:
            print(f"   {f}:{i}: {l.strip()[:140]}")
print()
print("(d) ACS_FIO_PATH_MAX 定义与 512 具名点：")
for f in sorted(tr):
    if not f.startswith(("lib/","include/")): continue
    try: s=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read()
    except Exception: continue
    if "ACS_FIO_PATH_MAX" in s:
        for i,l in enumerate(s.splitlines(),1):
            if "ACS_FIO_PATH_MAX" in l and ("define" in l or "const" in l or "enum" in l):
                print(f"   {f}:{i}: {l.strip()[:140]}")
