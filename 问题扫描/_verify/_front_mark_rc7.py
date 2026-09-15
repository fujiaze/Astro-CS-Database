import csv, collections, os
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
hdr=list(rows[0].keys())
if "verified_note" not in hdr: hdr.append("verified_note")
S={"M3-C-010":"FIXED","M8-H-001":"NOT_A_DEFECT","V12-N-14":"REJECT-NEVER-EXISTED","M8-F-009":"PARTIAL","M7-C-001":"PARTIAL","M2a-C-1":"FIXED","M8-F-001":"FIXED","V6-N-05":"FIXED"}
N={"M3-C-010":"删除声明式闭合，未修全：stage1 调用路径表仍列该诊断；账本原记 OPEN 系误",
"M8-H-001":"竞态本体误报：HEAD 与 BASE 均写点 18 而受 omp atomic 18 裸写 0，原报按 C11 原子检索漏看 OpenMP pragma；残余转 M2a-F-3",
"V12-N-14":"缺失确认故撤案勿记 FIXED：三套 beta 先验在 HEAD 与全史与影子树零载体，git log -S 唯一命中是本 finding 自身提交；现行固定 beta=4",
"M8-F-009":"一修二存：0.80 已随 §18.2 改 85.0 并补边界锁；残为通过输入由被测函数自算故恒过、反向钉死所在文件不被采集故永不运行",
"M7-C-001":"机制闭合但未修全五站：配置面仍 1..64 无整除前置、rc=3 未进两文档、两文档仍宣可配、upm-fit 缺键静默按 8、IR 通道忽略用户 G",
"M2a-C-1":"缓冲机制已消；残为锁仅覆盖尾 miss 形态，非降序与中间 miss 仍产出升序短数组",
"M8-F-001":"真会红但该目录 diff 为空故 BASE 即已修属定稿锚误；残为 README 仍称 36/36 与 R11 自身无 CI 载体即防复发门永不执行",
"V6-N-05":"本窗唯一真修复：虚构补丁与逐字节未变声明整删；未修全是新文本改而虚构两个 p1_op 钩子（同 W1-N-02）。账本原记 OPEN",
"M3-C-002":"证据失实判定不变：原报零命中应为 21 行命中；五站第三参仍全 nullptr 故维持 PARTIAL",
"M6b-E-007":"比原报更重：io 骨架四件逐路径均不在 HEAD；且合同 doc_ref 指向不存在文档而 136 项检查无一校验可达性",
"V3-N-01":"检索须按 rc= 词界（23 命中系日期误匹配）；合同未回带新码 -14 与 M2a-H-4 同源",
"V13-N-05":"铸造行实测 27 枚零宿主（更正 28）；TRACEABILITY.csv 脏改核为纯行尾差异"}
for r in rows:
    for h in hdr: r.setdefault(h,"")
    k=r["id"]
    if k in S:
        r["verified_state"]=S[k]; r["verified_by"]="RQS-RC7"; r["verified_date"]="2026-09-15"
    if k in N:
        r["verified_note"]=N[k]
        if not r["verified_by"]:
            r["verified_by"]="RQS-RC7"; r["verified_date"]="2026-09-15"
            r["verified_state"]=r["verified_state"] or "STILL"
tmp=fp+".tmp"
with open(tmp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=hdr); w.writeheader(); w.writerows(rows)
assert len(rows)>=700, "refuse to shrink"
os.replace(tmp,fp)
print("rows",len(rows),"cols",len(hdr),"verified",sum(1 for r in rows if r["verified_state"]),"note",sum(1 for r in rows if r["verified_note"].strip()))
print(collections.Counter(r["verified_state"] for r in rows if r["verified_state"]))
