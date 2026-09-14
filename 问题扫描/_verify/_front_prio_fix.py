import re
NOTE="（前台按 C-06 恢复：L28c/FD/M3 合并曾把本条降为 P2，判定列以恢复后的 P1 为准；目录 p1/ 为权威，见 40_OWNER_DECISIONS.md C-06 与 SUMMARY.md 簇表）"
for p,ids in [("问题扫描/findings/D_COMMENT/p1/L28c.md",["L28c-D-001","L28c-E-001"]),("问题扫描/findings/G_GOV_GATE/p1/FD_shadow_agents_md.md",["FD-G-003"]),("问题扫描/findings/I_DOC_HYGIENE/p1/M3_L05_L07.md",["M3-I-002"])]:
    s=open(p,encoding="utf-8").read()
    for i in ids:
        k=s.find(i)
        if k<0: print("未找到",i,p); continue
        nxt=[s.find(x,s.index("\n",k)) for x in ids if x!=i]
        nxt=[n for n in nxt if n>0]
        e=min(nxt) if nxt else len(s)
        blk=s[k:e]
        new=re.sub(r"(\*\*优先级\*\*[^\n]*?)P2", lambda m: m.group(1)+"**P1**"+NOTE, blk, count=1)
        if new==blk: new=re.sub(r"(优先级[^\n]*?)P2", lambda m: m.group(1)+"**P1**"+NOTE, blk, count=1)
        s=s[:k]+new+s[e:]
    open(p,"w",encoding="utf-8").write(s)
    print("已改",p)
