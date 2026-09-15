import csv, os
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
hdr=list(rows[0].keys())
M={"V20-N-04":"账本 V20-N-04 实为轴终报的 N-06（--resource-detail 空壳）；轴终报的 N-04（module_build_id 恒空串）在本账本为 V20-N-12。双向映射以本注为准",
"V20-N-12":"账本 V20-N-12 即轴终报的 N-04；本账本 V20-N-04 号位已被轴分片1 的 N-06 占用，故顺延编号",
"V20-N-02":"补轴终报口径：snr plan 连该键都不输出（:768-772），且 types.h 与三处注释自称租约上限或仅作回显，属注释-合同-代码三方不符",
"V20-N-05":"轴终报补强：projection 连值域校验都没有；问题扫描面 grep 零命中确认本轴新发现"}
for r in rows:
    for h in hdr: r.setdefault(h,"")
    if r["id"] in M: r["verified_note"]=M[r["id"]]
tmp=fp+".tmp"
with open(tmp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=hdr); w.writeheader(); w.writerows(rows)
assert len(rows)==785, len(rows)
os.replace(tmp,fp)
print("rows",len(rows),"V20 注记",sum(1 for r in rows if r["id"].startswith("V20-") and r["verified_note"].strip()))
