# -*- coding: utf-8 -*-
P="大报告_历代控制包.md"
t=open(P,encoding="utf-8").read()
old = "> 交付物 B。素材：设计大纲/_素材/B1 代际叙述素材.md、B2 口径演进素材.md、B3 形态陷阱与对接素材.md，以及本人对 reports/packs/ 下 16 份组报告、7 份代际报告、pack_events.md、coverage.md、digest_verify.md、reports/_前台独立核验.md 与 _evidence/ 底座的直接复算。全篇只描述不评价。"
new = "> 交付物 B。素材：_素材/ 下 B1、B2、B3 三份抽取件，加本人对 reports/packs/（16 份组报告、7 份代际报告、pack_events、coverage、digest_verify）、reports/_前台独立核验.md 与 _evidence/ 底座的复算。全篇只描述不评价。"
n=t.count(old); print("count",n)
if n==1:
    t=t.replace(old,new)
    open(P,"w",encoding="utf-8").write(t)
import re
han=len([c for c in t if 0x4E00<=ord(c)<=0x9FFF])
hb=len([c for c in t if (0x4E00<=ord(c)<=0x9FFF) or (0x3000<=ord(c)<=0x303F) or (0xFF00<=ord(c)<=0xFFEF) or ord(c) in (0x2014,0x2018,0x2019,0x201C,0x201D,0x2026)])
print("han",han,"han+punct",hb,"bytes",len(t.encode()))
PYEOF_HACK=None
