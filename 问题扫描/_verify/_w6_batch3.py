import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
print("=== (1) preset-contract 路径引用面 vs 实际文件 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","preset-contract"], capture_output=True, text=True).stdout.splitlines()
for l in out:
    if l.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/")): continue
    print("   ", l[:150])
print("   实际存在:", os.path.exists("packaging/schemas/preset-contract.json"), os.path.exists("packaging/schemas/preset-contract.schema.json"))
print()
print("=== (2) install 的 schemas 目录载荷 vs contract SCHEMA units ===")
print("   packaging/schemas 受跟踪文件:", subprocess.run(["git","--no-optional-locks","ls-files","packaging/schemas/*"], capture_output=True, text=True).stdout.split())
con = json.load(open("packaging/install-tree.contract.json", encoding="utf-8"))
print("   contract 中 schemas/ 条目:", [u["install_path"] for u in con["units"] if u["install_path"].startswith("schemas/")])
print("   contract 中 licenses/ 条目:", [u["install_path"] for u in con["units"] if u["install_path"].startswith("licenses/")])
print("   packaging/licenses 受跟踪:", subprocess.run(["git","--no-optional-locks","ls-files","packaging/licenses/*"], capture_output=True, text=True).stdout.split())
print()
print("=== (3) browser_qt / echo 是否有其它构建入口 ===")
for key in ["healpix_browser_qt", "conformance/echo", "astrocs_echo"]:
    o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln",key,"--","*CMakeLists.txt","*.cmake","*.yml","*.json","*.sh","*.py"], capture_output=True, text=True).stdout.splitlines()
    o = [x for x in o if not x.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/","run/"))]
    print("   %-22s -> %s" % (key, o))
print()
print("=== (4) UT-BACKEND / UT-IO 的 numpy/astropy 裸 import 与声明 ===")
checks = {c["id"]: c for c in json.load(open("ci/checks.json", encoding="utf-8"))["checks"]}
for gid in ["UT-BACKEND","UT-IO","UT-API","UT-CLI","UT-QUALITY","UT-ARCH","UT-MONITORING"]:
    c = checks.get(gid)
    if not c: continue
    d = None
    for i,a in enumerate(c["command"]):
        if a == "discover": d = c["command"][i+2] if i+2 < len(c["command"]) else None
    mods = collections.Counter()
    if d and os.path.isdir(d):
        for f in os.listdir(d):
            if not f.endswith(".py"): continue
            txt = open(os.path.join(d,f), encoding="utf-8", errors="replace").read()
            for m in re.findall(r"^\s*(?:from|import)\s+(numpy|astropy|scipy|yaml|matplotlib|photutils)", txt, re.M):
                mods[m] += 1
    print("   %-14s dir=%-16s declared=%s 实测裸 import(文件数)=%s" % (gid, d, c.get("prerequisite_tools") or [], dict(mods)))