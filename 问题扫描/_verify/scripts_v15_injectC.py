# -*- coding: utf-8 -*-
"""V15 §4 最后一口径：按"函数名含缺陷语义 + 文件级存在真门调用"计反向锁名册。"""
import os, re
ROOT="/workspace/Astro CS Database"
SKIP={"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports","工程控制",
      "GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描","third_party",
      "testdata","CS","Database","graph",".git","astrocs_p1sess_neg","astrocs_p1sess_perf",
      "astrocs_p1sess_props","astrocs_p1sess_test"}
NAME = re.compile(r"(mutation|mutate|must_fail|should_fail|_fails\b|forged|tamper|inject|reject|bogus|negative|bad_|stale|drift|missing|corrupt)", re.I)
CALLGATE = re.compile(r"subprocess\.run\(|run_check\(|_run_runner\(|check_call|self\._mut|checker|CHECKER|main\(")
EXPECTFAIL = re.compile(r"assertNotEqual\(|returncode,\s*[1-9]|returncode\s*[!=]=\s*0|assertEqual\(rc, 1|WEXITSTATUS|child_rc")
hits=[]; files={}
for dp,dn,fns in os.walk(ROOT):
    dn[:]=[d for d in dn if d not in SKIP]
    for fn in fns:
        if not fn.endswith(".py"): continue
        p=os.path.join(dp,fn); rel="./"+os.path.relpath(p,ROOT).replace(os.sep,"/")
        if not rel.startswith(("./tests/","./ci/","./tools/")): continue
        try: t=open(p,encoding="utf-8",errors="replace").read()
        except Exception: continue
        ms=[m for m in re.finditer(r'^\s*def (test_\w+|\w+)\s*\(self\)', t, re.M) if NAME.search(m.group(1))]
        if not ms: continue
        if not (CALLGATE.search(t) and EXPECTFAIL.search(t)): continue
        n=0
        for m in ms:
            s=m.start(); e=t.find("\n    def ", s+1)
            body=t[s:e if e>0 else len(t)]
            if EXPECTFAIL.search(body) or NAME.search(body):
                n+=1; hits.append((rel,m.group(1)))
        if n: files[rel]=n
print("丙口径（函数名含缺陷语义 + 文件内有真门调用 + 体内期望失败）=", len(hits), "处 /", len(files), "文件")
for f in sorted(files, key=lambda x:-files[x])[:18]:
    print(f"  {files[f]:3d}  {f}")
PY=None
