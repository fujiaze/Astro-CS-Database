# per-doc: docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-C-02 | 7-8,81,153,157,182 | 纯加性 calibrated=raw−C_f(p)，8×8 control cell；"乘性尺度差已撤销"；§3a "无 WCS/天球参与" | 与 ASTROCS_DESIGN.md:213（g·s+b）、:246（跨帧联合稀疏天光面）互斥；插件 10_sampling:32-56、11_upm:26-56 与设计一致 | CONTRADICTS-DESIGN | DESIGN-OK-DOC-WRONG |
| R2-H-28 | 22 | k_corr 标定表 1.2112–2.8971 | sampler.cpp:96-99 实际 max=3.2035 | WRONG | DESIGN-OK-DOC-WRONG |
| R2-H-29 | 61-62,169 | k_corr=1.4 由 pixfrac=0.8 MC 冻结为"保守" | 表值 pixfrac=1.0@300″=1.4980>1.4；生产默认 pixfrac 已改 1.0；MC 测试未注册不可复跑 | INSUFFICIENT-EVIDENCE | DESIGN-OK-EVIDENCE-MISSING |
| R2-H-19 | 176 | Padmanabhan 2008 ApJ 674,1217 | Crossref 10.1086/524677 ✅ | CORRECT | — |
| R2-H-26 | 177,§14a | Gruen 2014 / Holland-Welsch 1977 / Huber 1964 | Crossref ✅ | CORRECT | — |

## 处置建议

1. 走变更 claim 把 PHASE2_UPM.md 重写为目标模型 y_k=g_k·s(x)+b_k(x)（WCS 联合参考天光面 B_ref + 逐帧梯度 δ_k + SNR 加权最小 RMS），或明确标注"现行实现口径，已被设计 §4.2/§4.4 取代"；FROZEN 状态需一并处理。
2. k_corr 区间改 1.2112–3.2035；重跑/注册 control_median_mc_test，按 pixfrac=1.0 与真实角尺度重标定；删"保守"或给证据。
3. 该条决定 mosaic 是否可宣称"相对定标"（设计 §4.1 使命）——建议上呈负责人作为治理裁决。
