# per-doc: ASTROCS_DESIGN.md（最高设计，只读）

- 角色：需求文档、最高权威。本审计**不修改**，只判断其科学意图是否成立、是否自相矛盾。
- 结论：**设计意图科学成立，未发现需要改设计的理由。** 唯一需要澄清的措辞见 §2。

## 1 独立判断的设计条款

| 条款 | 行 | 独立证据 | 判定 |
|---|---|---|---|
| frame_snr = 未加权通量型 SNR，写 HiPS 头，w=SNR²/F_ref² | 86,155-158,235 | Horne 1986（DOI 10.1086/131801）；复算 E7/E8 | CORRECT |
| SNR 不是权重；Phase2 逆方差现场换算 | 234-235 | Horne/Naylor | CORRECT |
| UPM g·s+b；稀疏天光面跨帧联合 | 213,246 | SCAMP/SWarp 标准；插件 10/11 一致 | CORRECT |
| 稀疏 SNR 层 实际=帧级×帧内 | 163-166,226-233 | 语义自洽 | CORRECT |
| 校准方差传播、共享 master 相关性 | 197 | 独立代数；V6 实现 | CORRECT |
| Drizzle 常量面亮度/总通量守恒 | 486 | Fruchter & Hook 2002 式(3)(7)（DOI 10.1086/338393） | CORRECT |
| C_out=R C_in Rᵀ；相关项拒绝 1/Σw | 486 | Fruchter & Hook 2002；实装 p3_rsmp_covariance.cpp | CORRECT |
| SNR_comb²=ΣSNR_k² | 485 | 逆方差代数恒等 | CORRECT |
| 投影首批 8 种 | 269 | WCS Paper II（DOI 10.1051/0004-6361:20021327） | CORRECT |
| 三命令独立、磁盘+manifest+哈希交换 | 63-66 | 工程架构，无科学冲突 | CORRECT |

## 2 唯一需澄清的措辞（不阻塞）

- 行 235："其信号/噪声估计方法学**对标 PixInsight 公开文档中的 PSFSNR（ratio-of-powers 信噪比）**" 与同句通量型 F_ref/σ_F 并列。
- 官方 PSFSNR 式[18] = c3(Σf)²/(c4σ_n²)（本审计对 .pidoc commit 08b8eb85 逐字核验）；功率比型与 w=SNR²/F_ref² 不能同时成立（复算 E8）。
- 但同句明确"数学定义采用通量型口径以保证逆方差换算严格成立"，操作要求自洽。
- 判定 **AMBIGUOUS / DESIGN-OK-EVIDENCE-MISSING**；建议改为"仅方法学对标，不采用功率比式[18]"。

## 3 设计自指不一致（治理级，非科学）

- 行 379 vs 397：目录树 scheduler/ 与架构图 scheduler+runtime 两口径。
- 行 328：退出码唯一源 include/astrocs/exit_codes.h（实际 lib/infrastructure/cli/exit_codes.h）。
- 行 321,445,447：可执行名 ACSD Cli / acsd_cli（实际 build/astrocs）。
- 行 532-533：Alpha 前不含版本信息（实际 --version 报 0.11.0-alpha.2）。

以上均为 doc/实现命名与版本治理，不构成 BLOCKS-DESIGN。
