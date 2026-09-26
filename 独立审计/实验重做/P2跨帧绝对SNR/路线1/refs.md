# P2 跨帧绝对 SNR —— 路线1 文献腿核验记录（refs）

本文件记录报告 `report.md` 全部文献腿的真实核验过程：来源、访问方式、命中/未命中。
凡未能抵达一手来源者，明确标注 UNRESOLVED 与所用的次级佐证，绝不编造页码或数值。

## R1 稳健统计常数（P-CST-03/04/06/07）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| κ_MAD = 1/Φ⁻¹(3/4) ≈ 1.4826 | Rousseeuw & Croux 1993, JASA 88(423):1273, "Alternatives to the Median Absolute Deviation" | arxiv_search（该文不上 arXiv）→ web_search → [Wikipedia: Median absolute deviation](https://en.wikipedia.org/wiki/Median_absolute_deviation)（给出 1.4826 = 1/Φ⁻¹(3/4) 解析式） | **一手 PDF 未取得**（scholar_fetch 返回空文本，见 R7）；以解析式独立推导（Φ⁻¹(3/4)=0.6744897501960817，倒数=1.4826022185056018，与实现值一致到 1e-15）+ Wikipedia 次级佐证 |
| Stigler 1977, Ann. Statist. 5(6):1055 | DOI 10.1214/aos/1176343997 | web_search + 索引页 | **确认真题名 "Do Robust Estimators Work with Real Data?"**。审查员建议的题名 "Robust Estimators of Location" **不存在**——分歧②（见 report.md §6） |
| 中位数方差 (π/2)/n 渐近 | 标准渐近统计（Kendall & Stuart 型教科书结果） | 推导 + exp01/exp14 MC | MC 与 (π/2)/n 渐近式在 n≥1e4 一致；n=3 精确值 0.4487σ²（实测 0.44877） |

## R2 Moffat4 FWHM/σ（P-CST-05）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| Moffat 1969, A&A 3:455 | bibcode 1969A&A.....3..455M | web_search（ADS 摘要页元数据） | 书目确认。**ADS 原文 PDF 被 Human Verification 挡（HTTP 405）**，一手全文 UNRESOLVED。闭式 FWHM/σ = 2√2·√(2^{1/4}−1) = 1.230307652590102 由 exp02 从 β=4 轮廓定义独立推导并经连续网格+像素化 MC 交叉验证——常数正确性不依赖文献全文 |
| 1.230310 冻结值相对差 | 《正向规格》P-CST-05 | 数值比对 | 相对差 +1.908e-6，与审查员复核一致 |

## R3 星等-流量标度（P-CST-01/02、P-CST-19）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| Pogson 1856, MNRAS 17:12 | ADS bibcode 1856MNRAS..17...12P | web_search + ADS 索引页元数据 | 书目确认。**ADS PDF 405 被挡，一手全文 UNRESOLVED**；0.4 = 1/ln(100^{1/5}) 的解析自洽性由 exp05 数值验证（10^0.4 = 2.5118864…） |
| m_ref = 6.0 的地位 | 无一手文献规定该值 | web_search（肉眼极限星等沿革） | **结论：6.0 是项目约定（肉眼极限星等沿革沿用），不是科学量**。科学内容 = 每星等 2.512 倍单调标度（exp05 验证至 1.2e-15）与帧内权重比不变性（null = 1 ulp） |

## R4 双计偏差与权重恒等式（P-CST-11/13、P-ALG-10）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| PSF 加权最优提取 σ_F² = 1/Σ(P²/σ²) | 同型文献：Horne 1986 PASP 98:609；Naylor 1998 MNRAS 296:1007 | web_search 书目级确认；公式由 exp04 闭式+独立 MC 双轨自证 | 公式正确性不依赖文献全文；realized Var(F̂) 与预测一致到 ±1.4% = MC 误差 |
| float64 恒等式门 | 无外部文献（机器精度性质） | exp07 | 2.22e-16 = 1 ulp；实测 max 2.5 ulp（对门限设置的建议见 report.md P-CST-13 节） |

## R5 重建与协方差（P-CST-22/24、P-OPEN-02）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| Shepard 1968 IDW | Shepard, D. (1968), "A two-dimensional interpolation function...", Proc. 23rd ACM Nat. Conf. | web_search（ACM DL 索引 + 综述引用） | 书目确认；一手 PDF 付费墙 UNRESOLVED。IDW 权重定义由 exp09 直接实现并验证（节点复现 ≤7.1e-14） |
| drizzle 输出相关性 | Fruchter & Hook 2002, PASP 114:144, [arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087) | arxiv_search 命中 + scholar_fetch 摘要页 | **一手摘要页确认**。ρ̄=0.19 为仓库自测值（非文献值），其标定属 P3/P4 域；本报告只检验其对角近似公式 |
| Zackay & Ofek 时代背景 | [arXiv:1512.06872](https://arxiv.org/abs/1512.06872)、[arXiv:1512.06879](https://arxiv.org/abs/1512.06879) | arxiv_search | 摘要确认（II 明确 "background dominated noise limit"；与 P2 的 SNR 传递链相容） |

## R6 k_corr 与天光（P-CST-10、P-ALG-06）

| 目标 | 一手来源 | 核验方式 | 结果 |
|---|---|---|---|
| k_corr = 1.4/1.3883 原始标定 | 仓库内部标定（DATA_SEMANTICS.md:1722、修复包） | grep + exp14 | **UNRESOLVED：原始标定网格不在登记材料内**。exp14 证明：交换相关正态的中位数方差膨胀在 n=3、ρ=0.19 实测 1.233，低于渐近律 1+(n−1)ρ=1.38；1.3883 ↔ 渐近律 (n−1)ρ̄=0.3883（ρ̄=0.194, n_ret=3）自洽，但两读法不可判别 |
| 天光受限 CCD 方程斜率 −0.5 | 教科书级结论（CCD 方程文献族） | 推导 + exp06 | 解析斜率 ∈ (−0.5, 0)、天光主导时 → −0.5；实测 −0.4916（暗源）；文献腿为书目级 |

## R7 访问受阻清单（一手来源不可达汇总）

1. ADS PDF 全站（HTTP 405 Human Verification）：Pogson 1856、Moffat 1969 全文。
2. Rousseeuw & Croux 1993 付费 PDF（scholar_fetch 空文本）。
3. Stigler 1977 全文（题名与书目核验成功，全文未读）。
4. Naylor 1998 / Horne 1986 全文（书目级确认）。

以上均如实降级为"次级佐证 + 独立推导/实验自证"，未虚构任何文献数值。