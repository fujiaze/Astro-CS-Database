# -*- coding: utf-8 -*-
"""把 batch*.py 中「字段字符串内部的裸 ASCII 双引号」成对替成全角「」。

前提（本清单的书写格式）：每个字段独占一行，形如
    "内容",      或   ["内容",      或   "内容"],
行首第一个 " 与行尾最后一个 " 是 Python 定界符，其间的 " 一律视为正文引号。
"""
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
LINE_RE = re.compile(r'^(\s*(?:\[\s*)?)"(.*)"(\]?,?)\s*$')
# 行尾结构：定界 " 之后可跟 ] （关闭 ROWS 列表项）与 , ；定界 " 本身已由模式吃掉。

for path in sys.argv[1:]:
    lines = open(path, encoding="utf-8").read().split("\n")
    out = []
    n_fix = 0
    for l in lines:
        m = LINE_RE.match(l)
        if m and '"' in m.group(2):
            body = m.group(2)
            res = []
            open_q = True
            for ch in body:
                if ch == '"':
                    res.append("「" if open_q else "」")
                    open_q = not open_q
                    n_fix += 1
                else:
                    res.append(ch)
            l = '%s"%s"%s' % (m.group(1), "".join(res), m.group(3))
        out.append(l)
    open(path, "w", encoding="utf-8").write("\n".join(out))
    print("%-14s repaired %d inner quotes" % (path, n_fix // 2))
