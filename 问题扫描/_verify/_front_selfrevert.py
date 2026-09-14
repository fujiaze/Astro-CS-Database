import os,re
D="问题扫描/findings/D_COMMENT/p1/L28c.md"; s=open(D,encoding="utf-8").read()
s=re.sub(r"- 建议优先级: \*\*\*\*P1\*\*（[^）]*）\*\*（原判 P1；降级理由与升级条件见「分级自检」段）", "- 建议优先级: **P1**（原判 P1；合并阶段曾降为 P2，前台按 C-06 恢复为 P1，目录 p1/ 为定级源。注：原此处引用的「分级自检」段在本文件不存在，属悬空引用，已删。）", s)
open(D,"w",encoding="utf-8").write(s)
F="问题扫描/findings/G_GOV_GATE/p1/FD_shadow_agents_md.md"; t=open(F,encoding="utf-8").read()
k=t.find("## FD-G-003")
if k<0: k=t.find("FD-G-003")
nxt=[t.find(x) for x in ["## FD-G-004","FD-G-004"] if t.find(x)>k]
e=min(nxt) if nxt else len(t)
blk=t[k:e]
blk=blk.replace("（前台按 C-06 恢复：L28c/FD/M3 合并曾把本条降为 P2，判定列以恢复后的 P1 为准；目录 p1/ 为权威，见 40_OWNER_DECISIONS.md C-06 与 SUMMARY.md 簇表）","").replace("**优先级**：**P1**","**优先级**：**P2**（前台自查回改：本条正文自述「事实本身是正面证据，但它制造一处必须写清的歧义」，不构成交付面缺陷 ⇒ 我上一轮按 C-06 批量升 P1 属盲改，予以撤销；目录随之改 p2/）")
t=t[:k]+t[e:]
open(F,"w",encoding="utf-8").write(t.rstrip()+"\n")
os.makedirs("问题扫描/findings/G_GOV_GATE/p2",exist_ok=True)
P2F="问题扫描/findings/G_GOV_GATE/p2/FD_shadow_agents_md.md"
pre="" if not os.path.exists(P2F) else open(P2F,encoding="utf-8").read().rstrip()+"\n\n"
open(P2F,"w",encoding="utf-8").write(pre+"# FD 轴 · G_GOV_GATE × P2（自查回改归位）\n\n"+blk.strip()+"\n")
print("done")
