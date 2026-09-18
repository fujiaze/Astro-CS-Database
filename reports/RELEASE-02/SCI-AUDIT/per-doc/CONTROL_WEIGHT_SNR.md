# per-doc: docs/science/CONTROL_WEIGHT_SNR.md（SCI-CW-001..008，FROZEN）

- 设计关系：其 §2a 把 frame_snr/local_snr 重定义为"相对质量权重场，不是科学信噪比"，与最高设计 §3.4/§4.3 及 UNIFIED_MODEL.md:42 互斥。

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-A-02 | 10-14,32-33,120 | frame_snr 实为相对质量权重，非科学 SNR | 与 ASTROCS_DESIGN.md:86,155-158、UNIFIED_MODEL.md:42 互斥；同文 §4 自述 weight_mode=0 legacy | CONTRADICTS-DESIGN | DESIGN-OK-DOC-WRONG |
| R2-H-20 | 136 | Huang et al. 2017 ApJ 838,110（HSC 深度） | Crossref 无匹配；10.3847/1538-4357/aa6574 实为 ApJ 838,162 | INSUFFICIENT-EVIDENCE（疑似错引） | DESIGN-OK-DOC-WRONG |
| R2-H-22 | 136 | Tonry 2012 ApJ 750,99；Ivezić 2019 ApJ 873,111 | Crossref ✅ | CORRECT | — |

## 处置建议

1. 把 frame_snr 名字让给设计语义（通量型 F_ref/σ_F）；质量权重场统一改名 quality_weight（其 §2a 已使用该名），删除 §2 "frame_snr = 目录值中位数" 的定义。
2. §9 参考文献核 Huang 2017 出处。
3. 走 ENGINEERING_SPEC §3 变更 claim；同步 stage2 消费侧命名与契约 schema。
