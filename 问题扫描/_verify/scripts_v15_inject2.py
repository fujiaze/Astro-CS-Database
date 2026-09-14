# -*- coding: utf-8 -*-
"""V15 只读：注入式反向锁分档计数（口径写在输出里）。"""
import os, re, json
ROOT="/workspace/Astro CS Database"
SKIP={"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports","工程控制",
      "GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描","third_party",
      "testdata","CS","Database","graph",".git","astrocs_p1sess_neg","astrocs_p1sess_perf",
      "astrocs_p1sess_props","astrocs_p1sess_test"}
def files():
    for dp,dn,fns in os.walk(ROOT):
        dn[:]=[d for d in dn if d not in SKIP]
        for fn in fns:
            p=os.path.join(dp,fn)
            r="./"+os.path.relpath(p,ROOT).replace(os.sep,"/")
            if r.startswith(("./tests/","./ci/","./tools/")): yield p,r
            elif r.startswith("./lib/") and (("/tests/" in r) or "selftest" in r or "_test." in r): yield p,r

# (a) 两阶段 selfcheck: 同文件里既有 baseline 期望 PASS，又有注入期望 FAIL（fork/execve）
a=[]; b=[]
names=set()
for p,r in files():
    if not r.endswith(".cpp"): continue
    try: t=open(p,encoding="utf-8",errors="replace").read()
    except Exception: continue
    for m in re.finditer(r'ASTROCS_([A-Z0-9_]*?)FAULT', t): pass
    if re.search(r'ASTROCS_[A-Z0-9_]*FAULT', t):
        two = ("execve" in t or "fork" in t) and re.search(r'child_rc|WEXITSTATUS', t) and re.search(r'恒 PASS|必 FAIL|必败', t)
        (a if two else b).append(r)
# 注入名全集（全仓，含生产码）
allnames={}
for dp,dn,fns in os.walk(ROOT):
    dn[:]=[d for d in dn if d not in SKIP]
    for fn in fns:
        if not fn.endswith((".cpp",".c",".h",".hpp",".py")): continue
        p=os.path.join(dp,fn); r="./"+os.path.relpath(p,ROOT).replace(os.sep,"/")
        try: t=open(p,encoding="utf-8",errors="replace").read()
        except Exception: continue
        for n in re.findall(r'ASTROCS_[A-Z0-9_]*FAULT', t): allnames.setdefault(n,set()).add(r)
print("口径 (a) 两阶段 selfcheck（fork+execve 子进程注入 → 必 FAIL + 基线 → 必 PASS）文件数 =", len(a))
for x in sorted(a): print("    ", x)
print()
print("口径 (b) 仅读 env FAULT 或仅单侧断言的注入文件数 =", len(b))
for x in sorted(b): print("    ", x)
print()
print("口径 (c) 全仓不同注入名种类 =", len(allnames))
for n in sorted(allnames): print(f"    {n:34s} 出现于 {len(allnames[n])} 文件: {sorted(allnames[n])[:3]}")

# 注入名是否在 ctest 注册面（add_test）
reg=set()
for dp,dn,fns in os.walk(ROOT):
    dn[:]=[d for d in dn if d not in SKIP]
    if "CMakeLists.txt" not in fns: continue
    try: t=open(os.path.join(dp,"CMakeLists.txt"),encoding="utf-8",errors="replace").read()
    except Exception: continue
    for m in re.finditer(r'add_test\(\s*NAME\s+([A-Za-z0-9_\-\.]+)', t): reg.add(m.group(1))
selfcheck_targets=sorted(x for x in reg if "selfcheck" in x.lower())
print()
print("口径 (d) add_test 名含 selfcheck 的注册目标数 =", len(selfcheck_targets))
for x in selfcheck_targets: print("    ", x)
PY5=None
