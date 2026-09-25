# -*- coding: utf-8 -*-
"""一次性修串器（状态机版）：s6_spec.py 里每行形如  key="...",  /  "..."  的定界符
以外的 ASCII 双引号一律换成「/」配对。"""
import sys, re
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
p = "复算/d8agg/s6_spec.py"
src = open(p, encoding="utf-8").read()
out = []
in_str = False
depth_stack = []
i = 0
n = len(src)
fixed = 0
while i < n:
    ch = src[i]
    if ch == "\\":
        out.append(src[i:i + 2]); i += 2; continue
    if ch == "#":
        j = src.find("\n", i)
        out.append(src[i:j if j > 0 else n]); i = j if j > 0 else n; continue
    if ch == '"':
        if not in_str:
            in_str = True
            out.append(ch); i += 1; continue
        # 判断它是定界符还是中文引号：后面紧跟语法位才算闭合
        nxt = src[i + 1] if i + 1 < n else "\n"
        prev = src[i - 1]
        if nxt in ",)]}\n " or nxt == "":
            # 但也可能是 内容以 ASCII 结尾 + 真闭引号：只要前一个字符不是 CJK 即可判为闭
            in_str = False
            out.append(ch); i += 1; continue
        # 内部中文引号：开或闭看下一个字符
        out.append("「" if not (prev not in ' \n\t(,[「' and prev != '"' and (prev.isalnum() or prev in '.:^_-=+/') and not re.match(r"[\u4e00-\u9fff\uff00-\uffef]", prev)) else "」")
        fixed += 1
        i += 1
        continue
    out.append(ch); i += 1
s2 = "".join(out)
# 再兜一层：把残留的 ASCII 双引号成对换成「」
s2 = re.sub(r'(?<=[\u4e00-\u9\uff00-\uffef`^:.=/\w])"(?=[\u4e00-\u9`])', "「", s2)
open(p, "w", encoding="utf-8").write(s2)
print("状态机改动:", fixed)
