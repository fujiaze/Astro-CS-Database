
import csv
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
M={"FD-G-002":("FIXED","known_failures removals[UT-CLI] 2026-09-14 + cli_test_hygiene:40 子进程 cwd 归 run/test_cli_cwd；UT-CLI 脱离永久容忍红，残余 failures 仅 p1_noise_adapter 属已裁 conditional"),
"M4-C-02":("FIXED","p2_op fit_upm 显式 uc.zero_anchor_weight=1e-3 + 对称覆盖键 + p2_session:191 + ALG:365 同步，三入口统一使 model_hash 分叉消失；残：文档行锚 3152-3176 已漂至 3916-3943、DISP-P2UPM-003 覆盖键计数对 IR 通道失真"),
"M5a-G-002":("FIXED","resource_gate 增 allocated_capacity_cores 与 cpu_percent_of_allocated_capacity 且 compute_cores_threshold 用同一分母，commands 注入前归一，MON-001 85/60 同分母，工件改记 normalized_cpu_100pct_all_allocated_cores=false 与 cpu_pct_units=percent_of_one_core；残：gate:194 把 granted 与 min 取义留裁决"),
"V2-N-09":("FIXED","psf_mode 按 n_fit_limit 派生 fast/precise 并同源写产物与 manifest（非字面量），p1001 锁由 contains 改 ==fast 值断言并加 precise 直调 n_fit_input 行为断言；残：p1_psf.json 的 psf_mode 仍 contains 不查值"),
"V6-N-02":("MOVED","矩阵迁移 docs/traceability/ 且 checker MATRIX_REL 同步，但新路径首三字节仍 BOM 而 _check_csv_parity 不剥 BOM，SCHEMA_VIOLATION 后 return 使 JSON 与 CSV 同构校验整条短路，CSV 行11 引 p1star 三项而 JSON 零命中——缺陷原样")}
hit=[]
for r in rows:
    k=r["id"]
    if k in M:
        st,note=M[k]; r["verified_state"]=st; r["verified_by"]="RQS-RC5"; r["verified_date"]="2026-09-15"; hit.append(k)
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print("标记",hit)
print("verified 非空行数",sum(1 for r in rows if r["verified_state"]))

