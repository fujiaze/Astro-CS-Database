
import csv,os
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
cols=list(rows[0].keys())
vc=[c for c in cols if c.startswith("verified")]
print("verified 列:",vc)
FIX={"M2a-H-1":"机制复算消失：hp_drizzle_run 拒非法值+缺省升 FP64+pm 与块型不符 return -14 fail-closed；累加域随 use_f64 真 binary64；回归 drizzle_precision_default_test T7/T8 两通道同受 choke-point 门，未见同类残站",
"M4-C-01":"机制复算消失：sampler.cpp::kcorr_lookup 改两段分段线性，pf=0.8 命中节点 bitwise 返回原表值，旧表外 +1.5%/+2.1% 不复存在；角点回归 kcorr_lookup_test 注册 phase2_sampler；消费链同一函数无他站",
"M5a-G-001":"resource_gate.h 常量回 85.0/60.0 且被 commands.cpp 实际引用（0.80/0.75/0.50 字面量消失），compute_cores_threshold=0.85*m；残余：commands.cpp:939 段注释仍写旧数（登记型，另立观察不撤条）",
"V2-N-08":"交付样本改取全部测光有效源并与 psf.max_stars 完全解耦，provenance 如实（snr_sample/n_snr_available/truncated/psf_mode 真实模式/n_fit_input），parity 锁 p1snr_frame_parity_test 注册于 CMakeLists:1330，含非恒真负例"}
n=0
for r in rows:
    if r["id"] in FIX:
        if vc:
            r[vc[0]]="FIXED"
            for c in vc[1:]:
                if "note" in c.lower(): r[c]=FIX[r["id"]]
                elif "date" in c.lower() or "when" in c.lower(): r[c]="2026-09-15"
                elif "by" in c.lower(): r[c]="RQS-RC4"
        n+=1
print("标记",n)
hdr=list(rows[0].keys())
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=hdr); w.writeheader(); w.writerows(rows)
d=sum(1 for r in rows if (r.get(vc[0]) if vc else ""))
print("总 verified 行数",d,"/ 行",len(rows))

