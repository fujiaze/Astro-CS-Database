# -*- coding: utf-8 -*-
"""V15 只读：checks.json 里所有 ctest 类检查项的 -R 采集模式 vs 当前 CMake 注册的全部 add_test 名。
   目的：判定 P8/P5 新增测试是否真的进入 CI 采集面（不跑任何命令，纯文本/JSON 解析）。"""
import json, re, os
ROOT = "/workspace/Astro CS Database"
c = json.load(open(os.path.join(ROOT,"ci/checks.json"),encoding="utf-8"))
ctest_checks = []
for chk in c["checks"]:
    cmd = " ".join(chk["command"])
    if "ctest" in cmd:
        ctest_checks.append((chk["id"], chk["profiles"], cmd, chk.get("waivable")))
for x in ctest_checks:
    print(x[0], "| profiles=", x[1], "| waivable=", x[2] and x[3])
    print("   ", x[2][:300] if len(x[2])>300 else x[2])

# 收集所有 add_test(NAME ...)
names=set()
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports","工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描","third_party","testdata","CS","Database","graph",".git"}]
    for fn in filenames:
        if fn != "CMakeLists.txt": continue
        p=os.path.join(dirpath,fn)
        try: t=open(p,encoding="utf-8",errors="replace").read()
        except Exception: continue
        for m in re.finditer(r'add_test\(\s*NAME\s+([A-Za-z0-9_\-\.]+)', t):
            names.add(m.group(1))
print("\nadd_test 名总数:", len(names))
def rpattern(cmd):
    m=re.search(r'-R[ =]"?([^"\']+)"?', cmd)
    return m.group(1) if m else None
pats={cid:rpattern(cmd) for cid,_,cmd,_ in ctest_checks}
print("各 ctest 检查项的 -R 模式:", pats)
def matched(pat):
    if not pat: return set()
    rx=re.compile(pat)
    return {n for n in names if rx.search(n)}
allm=set()
for cid,p in pats.items():
    mm=matched(p); allm|=mm
print("\nP5/P8/p1snr/p1noise 相关测试名:", sorted(n for n in names if "snr" in n.lower() or "p1snr" in n.lower()))
for n in sorted(n for n in names if "snr" in n.lower() or "p1noise" in n.lower()):
    hit=[cid for cid,p in pats.items() if p and re.search(p,n)]
    print(f"  {n:38s} 被这些检查项采集: {hit if hit else '无（零采集）'}")
print("\n未被任何 ctest 检查项采集的测试名数:", len(names-allm))
for n in sorted(names-allm)[:40]: print("   -", n)
PY_MARKER = None
