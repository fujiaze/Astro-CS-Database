# -*- coding: utf-8 -*-
"""定位/修复 batch JSON 里字符串内部的裸 ASCII 引号。

规则：一个 JSON 字符串值内若出现成对裸引号，把该对替成「」。
做法：逐行处理，行内引号数超过"合法字段引号数"即判有裸引号。
"""
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
p = sys.argv[1]
lines = open(p, encoding="utf-8").read().split("\n")
for i, l in enumerate(lines, 1):
    # 合法行形如  "内容",   或  "key": "内容",  -> 引号数为偶且内容内无引号
    if l.count('"') % 2:
        print("ODD  %4d | %s" % (i, l[:160]))
try:
    json.load(open(p, encoding="utf-8"))
    print("JSON OK")
except Exception as e:
    print("ERR:", e)
    m = re.search(r"line (\d+) column (\d+)", str(e))
    if m:
        ln = int(m.group(1))
        print("CONTEXT:")
        for j in range(max(0, ln - 3), min(len(lines), ln + 1)):
            print("  %4d | %s" % (j + 1, lines[j][:200]))
