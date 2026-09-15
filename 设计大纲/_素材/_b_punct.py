# -*- coding: utf-8 -*-
import re
p="大报告_历代控制包.md"
t=open(p,encoding="utf-8").read()
bad_pat = ["。，","；。","：。","、。","。。","（）","、，","，。","；，","，、","的的","。）（","）（","两两"]
for b in bad_pat:
    for m in re.finditer(re.escape(b), t):
        s=max(0,m.start()-40); print("PUNCT", repr(b), "...", t[s:m.end()+30].replace(chr(10),"/"))
# dangling empty parens or orphan pointers
for m in re.finditer(r"[（(]\s*[)）]", t): print("EMPTY-PAREN", t[max(0,m.start()-30):m.end()+20])
for m in re.finditer(r"（[，、；：]", t): print("LEAD-PUNCT-IN-PAREN", t[max(0,m.start()-25):m.end()+25].replace(chr(10),'/'))
for m in re.finditer(r"[，、；：）]\s*）", t): print("TAIL-PUNCT-IN-PAREN", t[max(0,m.start()-25):m.end()+20].replace(chr(10),'/'))
for m in re.finditer(r"[，、；]\s*[。；]", t): print("DOUBLE-STOP", t[max(0,m.start()-30):m.end()+15].replace(chr(10),'/'))
print("--- paragraph starts after cuts ---")
for line in t.split(chr(10)):
    if line.strip().startswith(("；","，","、","）","：")): print("ODD-START", line[:60])
print("done")
