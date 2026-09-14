# -*- coding: utf-8 -*-
"""V15 普查器 D：Python 侧四类弱断言的精确抽取（含被读对象、字面量、窗口）。"""
import os, re, json
ROOT = "/workspace/Astro CS Database"
SKIP_DIRS = {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports",
             "工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描",
             "third_party","testdata","CS","Database","graph",".git","astrocs_p1sess_neg",
             "astrocs_p1sess_perf","astrocs_p1sess_props","astrocs_p1sess_test"}

FILE_READ = re.compile(r'(read_text\(|\.read\(\)|json\.load|open\()')
ASSERTS = re.compile(r'self\.assert(In|NotIn|Regex|NotRegex)\(|re\.search\(|re\.match\(|re\.findall\(')
COUNT_POS = re.compile(r'self\.assertGreater(?:Equal)?\(([^,]+),\s*([0-9]+)\)|self\.assertIsNotNone\(|os\.path\.exists\(|\.exists\(\)|len\(([^)]*)\)\s*(?:>=|>)\s*[01]\b')
MSG = re.compile(r'(msg|message|reason|stderr|stdout|out|log|detail|diagnosis|text|content|body|line)\b', re.I)

def scope_files():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if not fn.endswith(".py"): continue
            p = os.path.join(dirpath, fn)
            r = "./" + os.path.relpath(p, ROOT).replace(os.sep, "/")
            if r.startswith("./tests/") or r.startswith("./ci/") or r.startswith("./tools/"):
                yield p, r
            elif r.startswith("./lib/") and (("/tests/" in r) or "selftest" in r):
                yield p, r

c1, c3, c4 = [], [], []
for p, r in scope_files():
    try: lines = open(p, encoding="utf-8", errors="replace").read().split("\n")
    except Exception: continue
    for i, ln in enumerate(lines):
        s = ln.strip()
        if s.startswith("#"): continue
        # ① 文本命中：assertIn/regex 的字面量参数 + 附近有文件读取
        if ASSERTS.search(s):
            win = "\n".join(lines[max(0,i-10):i])
            if FILE_READ.search(win):
                lit = re.search(r'["\']([^"\']{3,60})["\']', s)
                c1.append({"f": r, "l": i+1, "s": s[:200], "lit": lit.group(1) if lit else ""})
        # ③ 诊断文案：断言字面量落在 msg/stderr/reason 上
        if re.search(r'self\.assert(In|NotIn)\(', s) and re.search(r'\[[\'"](msg|message|stderr|stdout|reason|detail|diagnosis|text)[\'"]\]|\.stderr|\.stdout|\bmsg\b', s):
            c3.append({"f": r, "l": i+1, "s": s[:200]})
        # ④ 正向存在/计数
        m = COUNT_POS.search(s)
        if m and not re.search(r'self\.assertGreater(?:Equal)?\([^,]+,\s*[2-9][0-9]', s):
            c4.append({"f": r, "l": i+1, "s": s[:200]})

print("①(py) 读文件后文本命中:", len(c1))
print("③(py) 诊断文案断言:", len(c3))
print("④(py) 正向存在/计数(阈值<=1 或 len<=1):", len(c4))
json.dump({"c1":c1,"c3":c3,"c4":c4}, open(os.path.join(ROOT,"问题扫描/_verify/scripts_v15_py.json"),"w",encoding="utf-8"), ensure_ascii=False, indent=1)
byf={}
for x in c1: byf.setdefault(x["f"],0); byf[x["f"]]+=1
print("\n-- ① top files --")
for f in sorted(byf, key=lambda k:-byf[k])[:25]: print(f"{byf[f]:4d}  {f}")
byf={}
for x in c4: byf.setdefault(x["f"],0); byf[x["f"]]+=1
print("\n-- ④ top files --")
for f in sorted(byf, key=lambda k:-byf[k])[:25]: print(f"{byf[f]:4d}  {f}")
