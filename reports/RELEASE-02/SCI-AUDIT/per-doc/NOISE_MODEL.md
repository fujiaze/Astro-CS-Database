# per-doc: docs/science/NOISE_MODEL.md（SCI-NOISE-001）

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| P1A-001 | 53 | σ_bg=1.482602218505602·MAD | 复算 1/Φ⁻¹(3/4) 逐位相等；FP32 舍入 +1.3591e-8 与文档 +1.36e-8 一致 | CORRECT | — |
| P1A-002 | 124 | 0.7316727929211932 | 复算 0.7316730952806134（rel 4.13e-7） | WRONG | DESIGN-OK-DOC-WRONG |
| P1A-004 | 131 | 1.253=√(π/2) | 复算 1.2533141373155001（rel 2.5e-4）；实现硬编码 1.253（snr_science.cpp:246） | WRONG | DESIGN-OK-DOC-WRONG |
| P1A-005 | 86 vs 115,492 | 天空预算系数 1.44 / 1.144 / 1.152 | 三个系数互斥；理论 MAD-σ 渐近 SE=1.1664/√N（本审计员独立推导）；9216 只与 1.44 自洽 | AMBIGUOUS | DESIGN-OK-DOC-WRONG |
| P1A-006 | 184 | Starck/Donoho/Candès 2003 A&A 398,785，DOI 10.1051/0004-6361:20021569 | Crossref：20021569=Homeier 2002 A&A 397,585；正确 **10.1051/0004-6361:20021571** | WRONG | DESIGN-OK-DOC-WRONG |
| R2-H-07 | 202-203 | N*_Sn=2.03636·S_n | .pidoc 式[15] 与 PCL NStar_Sn 均 2.03636；PCL 注释 2.05435 为 PI 内部冲突 | CORRECT | — |
| R2-H-28/29 | （UPM k_corr） | — | 见 PHASE2_UPM per-doc | — | — |

## 处置
统一天空预算系数并重推 N_sky；改 DOI；常数全精度。
