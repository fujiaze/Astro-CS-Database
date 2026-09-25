# -*- coding: utf-8 -*-
"""修 s6c_gen.py 的文件名清洗：保留 CJK 与字母数字，只替换文件系统禁用字符。"""
import sys
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
q = "复算/d8agg/s6c_gen.py"
t = open(q, encoding="utf-8").read()

OLD = [l for l in t.splitlines() if l.strip().startswith("safe = ") and "re.sub" in l]
print("找到待替换行:", OLD)
assert len(OLD) == 1

NEW = ('    safe = "".join("-" if c in \'\\\\/:*?\"<>|\' else ("-" if c in "\\u2013\\u2014 \\u00a0" else c)'
       ' for c in t["short"]).strip("-")')
lines = t.splitlines()
for i, l in enumerate(lines):
    if l.strip().startswith("safe = ") and "re.sub" in l:
        lines[i] = NEW
        break
open(q, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("新行:", NEW)
