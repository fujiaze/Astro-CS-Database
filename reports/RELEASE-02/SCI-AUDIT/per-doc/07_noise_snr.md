# per-doc: docs/plugins/algorithms_phase1/07_noise_snr.md

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-H-01 | 47 | PSFSNR 式[18] c3(Σf)²/(c4σ_n²)（已订正） | .pidoc 逐字 | CORRECT | — |
| R2-H-06 | 41,114 | 帧级 SNR 对加性背景免疫、对天光散粒噪声敏感（已订正） | 复算 E6 | CORRECT | — |
| R2-A-01 | 38-39,57-64 | frame_snr=未加权通量型 SNR，w=SNR²/F_ref² | Horne 1986；复算 E7 | CORRECT | — |
| R2-A-05 | 47,66 | "对标 PSFSNR 方法学"与通量型口径的区分（已澄清） | .pidoc；E8 | CORRECT（澄清到位） | — |
| R2-A-03 | 24,98 | 声称 frame_snr 写 HiPS 头 | 实装未产出整帧 SNR 标量（module_adapters.cpp:3177） | 实现缺口 | DESIGN-OK-DOC-WRONG |

## 结论

该插件文档在本轮文档包中已完成 PSFSNR 公式与天光措辞订正，与最高设计一致；剩余为实现缺口（R2-A-03）。
