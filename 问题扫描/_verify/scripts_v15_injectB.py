# -*- coding: utf-8 -*-
"""V15 只读：注入式反向锁 B 档严格复算（同一 test 函数体内：缺陷变体构造 + 期望失败断言 同时存在）。"""
import os, re, json
ROOT="/workspace/Astro CS Database"
SKIP={"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports","工程控制",
      "GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描","third_party",
      "testdata","CS","Database","graph",".git","astrocs_p1sess_neg","astrocs_p1sess_perf",
      "astrocs_p1sess_props","astrocs_p1sess_test"}
MUT = re.compile(r"(write_text\(|\.write\(.*tamper|shutil\.copy|open\([^)]*[\"']w[\"']\)|"
                  r"\.replace\(|rename\(|renameTo|unlink\(|remove\(|chmod\(|os\.utime|"
                  r"json\.dump\(|csv\.writer|setenv|putenv|ASTROCS_[A-Z0-9_]*FAULT|"
                  r"mutation|mutate|tamper|forged|inject|bogus|corrupt|bad_value|翻转|篡改|伪造|改回|删掉|删除)", re.I)
FAIL = re.compile(r"(assertNotEqual\([^,]+,\s*0|assertNotEqual\([^,]+,\s*[\"']?0[\"']?|"
                  r"assertGreater\(\s*[^,]*returncode|!= 0|assertTrue\([^)]*(fail|reject|red)|"
                  r"assertRaises|rc\s*!=\s*0|child_rc\s*!=\s*0|WEXITSTATUS|expect_fail|must_fail|"
                  r"必败|应失败|判红|变红|returncode,\s*1\b|returncode,\s*[2-9])", re.I)
rows=[]
for dp,dn,fns in os.walk(ROOT):
    dn[:]=[d for d in dn if d not in SKIP]
    for fn in fns:
        if not fn.endswith((".py",".cpp",".c")): continue
        p=os.path.join(dp,fn); rel="./"+os.path.relpath(p,ROOT).replace(os.sep,"/")
        if not (rel.startswith(("./tests/","./ci/","./tools/")) or (rel.startswith("./lib/") and ("_test." in rel or "/tests/" in rel or "selftest" in rel))): continue
        try: t=open(p,encoding="utf-8",errors="replace").read()
        except Exception: continue
        if fn.endswith(".py"):
            # 按 def test_ / def 切函数块
            idxs=[m.start() for m in re.finditer(r'^\s*def (test_\w+|\w*mutation\w*|\w*selftest\w*)\s*\(', t, re.M)]
            for i,s in enumerate(idxs):
                e = idxs[i+1] if i+1<len(idxs) else len(t)
                body=t[s:e]
                if MUT.search(body) and FAIL.search(body):
                    name=re.search(r'def (\w+)', body).group(1)
                    rows.append((rel, t[:s].count("\n")+1, name))
        else:
            idxs=[m.start() for m in re.finditer(r'^(?:static\s+)?(?:void|int)\s+(\w+)\s*\(', t, re.M)]
            for i,s in enumerate(idxs):
                e=idxs[i+1] if i+1<len(idxs) else len(t)
                body=t[s:e]
                if MUT.search(body) and FAIL.search(body):
                    name=re.search(r'(?:void|int)\s+(\w+)', body).group(1)
                    if name=="main": continue
                    rows.append((rel, t[:s].count("\n")+1, name))
print("严格 B 档（同函数体内 构造缺陷变体 + 期望失败）确认条数 =", len(rows))
from collections import Counter
c=Counter(r for r,_,_ in rows)
print("涉及文件数 =", len(c))
for f,n in sorted(c.items(), key=lambda x:-x[1]):
    print(f"  {n:3d}  {f}")
json.dump([{"file":f,"line":l,"fn":n} for f,l,n in rows], open("scripts_v15_injectB_strict.json","w",encoding="utf-8"), ensure_ascii=False, indent=1)
PY=None
