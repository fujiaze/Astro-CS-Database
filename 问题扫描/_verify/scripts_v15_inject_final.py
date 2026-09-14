# -*- coding: utf-8 -*-
"""V15 §4 终口径：注入式反向锁分三类，逐类给可复核的判据与名单。"""
import os, re, json
ROOT="/workspace/Astro CS Database"
d=json.load(open(os.path.join(ROOT,"问题扫描/_verify/scripts_v15_injectB_strict.json"),encoding="utf-8"))
py=[x for x in d if x["file"].endswith(".py")]; cpp=[x for x in d if not x["file"].endswith(".py")]
print("粗口径合计:", len(d), " 其中 Python:", len(py), " C/C++:", len(cpp))

# 真「篡改在册真实工件 → 跑真门 → 断言红」判据：函数体里同时有
#  (1) 读取真实仓库工件 (REPO/docs/evidence/VERSION/*.csv/*.md/*.yml) 或整树 copy
#  (2) 写出篡改副本
#  (3) 断言被检门返回码非 0 / 结论 FAIL
TIGHT_REAL = re.compile(r"(docs/|evidence/|VERSION|\.csv|\.md|\.yml|\.json|workflow|ledger|台账|manifest)", re.I)
RCFAIL = re.compile(r"(returncode|rc)\s*[,)]|!= 0|assertNotEqual|FAIL", re.I)
def body_of(rel, line, nxt_line):
    p=os.path.join(ROOT, rel[2:])
    t=open(p,encoding="utf-8",errors="replace").read().split("\n")
    return "\n".join(t[line-1:nxt_line])
rows=[]
byfile={}
for x in py: byfile.setdefault(x["file"],[]).append(x["line"])
for f,ls in byfile.items():
    ls=sorted(ls)
    p=os.path.join(ROOT,f[2:]); t=open(p,encoding="utf-8",errors="replace").read().split("\n")
    for i,l in enumerate(ls):
        e = ls[i+1]-1 if i+1<len(ls) else min(len(t), l+40)
        b="\n".join(t[l-1:e])
        real_src = bool(re.search(r"REPO|docs/|evidence/|VERSION|checks\.json|known_failures|workflow|TASK_LEDGER", b))
        copy_mut = bool(re.search(r"shutil\.copy|copytree|mkdtemp|TemporaryDirectory|write_text|open\([^)]*[\"']w", b))
        mut_act  = bool(re.search(r"replace\(|\.remove|unlink|rename|del |json\.dump|writerow|sub\(", b))
        expectfail = bool(re.search(r"assertNotEqual\([^,]+,\s*0|returncode,\s*[1-9]|returncode\s*[!=]=\s*0|assertGreater\([^,]*returncode|must_fail|应失败|判红", b))
        if all([real_src,copy_mut,mut_act,expectfail]):
            rows.append((f,l,round(1.0*sum([real_src,copy_mut,mut_act,expectfail])/4,2)))
print()
print("三类中【甲｜篡改真实工件→跑真门→断言红】函数数 =", len(rows), " 文件数 =", len({f for f,_,_ in rows}))
for f,l,_ in sorted(rows): print(f"   {f}::{l}")
PY=None
