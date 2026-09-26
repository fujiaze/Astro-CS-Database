# refs.md — P2 跨帧绝对 SNR · 文献核验台账（单元级汇总）

**口径**：只收一手 VERIFIED 条目（格式：DOI/bibcode/arXiv + 核验方式 + 用途）。核验工作由独立审计三路（互不通信）各自完成，本台账逐条转录其**实际抓取字段**，未新增任何未经核验的标识；核验层级不足的条目单列为「标注级」，绝不冒充 VERIFIED。
**来源**：路线1 refs.md（R1–R7）、路线2 refs.md（R1–R12）、路线3 refs.md（L1–L10）、补实验-control_variance refs.md。核验日期 2026-09-26（补实验为同期）。
**拒绝采信项与 UNRESOLVED 项见文末**；二者永不进入论文正文。

## A. VERIFIED 一手条目

| # | 文献 | 标识 | 核验方式 | 用途 |
|---|---|---|---|---|
| 1 | Pogson, N. (1856), MNRAS 17, 12–15 | DOI 10.1093/mnras/17.1.12 | Crossref 逐字段（题名/卷/页/日期），路线2 R1 | P-CST-01/02：星等标度 2.5 与 −0.4 为定义常数 [文献] |
| 2 | Rousseeuw & Croux (1993), JASA 88(424), 1273–1283 | DOI 10.1080/01621459.1993.10476408 | Crossref works API（路线3 L1；KU Leuven 官方 PDF 在案） | P-CST-03：MAD 稳健尺度与 ARE≈37% [文献] |
| 3 | Croux & Rousseeuw (1992), Computational Statistics, 411–428 | DOI 10.1007/978-3-662-26811-7_58 | Crossref works API（路线3 L2） | P-CST-03 归属订正：PHASE2_SAMPLER.md:219 归属注应指本文 |
| 4 | Huber, P. J. (1981), Robust Statistics, Wiley（2004 二版） | 专著；二版 DOI 10.1002/0471725250 | Crossref（type=monograph）；审查所引页码级定位未核 | P-CST-03/07：MAD 渐近方差与中位数 SE 渐近式参照 |
| 5 | Stigler, S. M. (1977), Ann. Statist. 5(6), 1055–1098 | DOI 10.1214/aos/1176343997 | Crossref works API（路线3 L4）；真题名 “Do Robust Estimators Work with Real Data?” | P-CST-06：截尾均值类估计子实证经典 [文献] |
| 6 | Moffat, A. F. J. (1969), A&A 3, 455 | bibcode 1969A&A.....3..455M | bibcode 级（ADS 原文被人机验证拦截，未逐页）；**标注级** | P-CST-05/15：Moffat 轮廓出处；β=4 闭式由推导腿自足 |
| 7 | Horne, K. (1986), PASP 98, 609 | DOI 10.1086/131801 | Crossref works API（路线3 L6） | P-CST-11/23：最优提取方差组成 σ_F² = 1/Σ(P²/σ²) [文献] |
| 8 | Fruchter & Hook (2002), PASP 114, 144–152 | DOI 10.1086/338393；arXiv:astro-ph/9808087 | Crossref + arXiv abs 页（路线2 R5）。**注意**：旧 DOI 10.1086/341773 经 Crossref 实测为他文（儿科病毒学），仓内引用一律订正 | P-CST-10/22：重采样相关噪声与方差传播现象学 [文献] |
| 9 | Bertin & Arnouts (1996), A&AS 117, 393–404 | DOI 10.1051/aas:1996164 | Crossref works API（路线3 L8） | P-CST-15：背景估计/掩膜**实践参照**（k=0.1 本身为项目约定，不注文献出处，负责人已批） |
| 10 | Newberry, M. V. (1991), PASP 103, 122 | DOI 10.1086/132801 | Crossref works API（路线3 L10 独立复核） | P-CST-11：天空扣除 SNR 噪声组成口径 [文献] |
| 11 | Stetson, P. B. (1987), PASP 99, 191 | DOI 10.1086/131977 | Crossref works API（路线3 L9） | 逐源测光背景，链上参照 |
| 12 | IVOA (2017), HiPS REC 1.0 | DOI 10.5479/ADS/bib/2017ivoa.spec.0519F | 官方 REC 页 + 官方 PDF 流级解压，正文实测 hips_tile_width = 512（路线2 R6） | P-CST-16：Δ = 512/8 = 64 px 的 tile 侧出处 [文献] |
| 13 | Riello et al. (2020), A&A 649, A3 | arXiv:2012.01916 | arXiv abs 页实测题名/作者/日期（路线2 R7） | P-CST-19：Gaia G 星等制与零点口径参照系（不构成 m_ref=6.0 的锚） |
| 14 | Goldberg, D. (1991), ACM Comput. Surv. 23(1), 5–48 | 无 DOI（公认综述） | 标准综述 + np.finfo(float64).eps 实测双锚（路线2 R11） | P-CST-13：浮点门值 2.22×10⁻¹⁶ = 1 ulp [文献] |
| 15 | Janesick (2001), Scientific Charge-Coupled Devices, SPIE PM83；Howell (2006), Handbook of CCD Astronomy, 2nd ed. | 标准专著 | 专著（路线2 R12） | P-CST-11/15：CCD 噪声组成通行出处 [文献] |
| 16 | Zackay & Ofek (2017), ApJ 836, 187/188 | arXiv:1512.06872、arXiv:1512.06879 | arXiv 摘要页（路线1 R5；II 明确 background dominated noise limit） | SNR 传递链与背景主导极限的相容性佐证 [文献] |
| 17 | Pinelis, I. (2022), ALEA Lat. Am. J. Probab. Math. Stat. 19, 359 | 期刊全文取回 | 全文 PDF（补实验 refs） | control_variance 渐近系数 (π/2) 的样本中位数方差依据 [文献] |
| 18 | Akinshin, A. | arXiv:2209.12268、arXiv:2207.12005 | arXiv 摘要级（补实验 refs） | MAD 有限样本偏置修正因子现象（N=5 时 c_mad2=0.906 的文献旁证） [文献] |
| 19 | Shepard, D. (1968), Proc. 23rd ACM Nat. Conf., 517–524 | 无 DOI | **二手核验**（ACM DL 反爬；方法命名页确认归属无疑义） | P-CST-24：IDW 出处（配置级，插值参数已批配置化，P4 承载） |

## B. 标注级引用（负责人批准后进入科学文档，核验层级不足，如实声明）

| 文献 | 标识与状态 | 用途 |
|---|---|---|
| Aitken, A. C. (1935), Proc. R. Soc. Edinburgh 55, 42–48 | 标注级：卷期页在案、DOI 未核；经负责人批准作为逆方差加权口径引用进入科学文档（已批事项） | w = 1/σ_F² 的逆方差加权口径 [文献] |
| Serfling, R. J. (1980), Approximation Theorems of Mathematical Statistics, Wiley | 书目级：小节号存疑（§2.3.2 vs §2.3.3），标注后引用 | U 统计量/序统计量渐近一般参照 |
| Cramér, H. (1946), Mathematical Methods of Statistics, §28.5 | 书目级：章号经二手独立证实、逐字引文未核，标注后引用 | 样本中位数渐近有效性 |

## C. 拒绝采信（核验为幻觉/失真，任何成稿不得引用）

| 主张 | 核验结果 | 定性 |
|---|---|---|
| 审查-1 建议引用 “Peacock, P. (1984), Comparing Distributions, ApJ 283, 387” | Crossref 无此文；相近真文 Peacock, J. A. 1983（DOI 10.1093/mnras/202.3.615）主题（2D KS 检验）与高斯 FWHM 换算无关 | 审查件自带文献幻觉（路线3 X1） |
| 审查-1 建议引用 Stigler “Do Estimators of Location Have Optimal Properties?”, North-Holland | 未检索到；Stigler 1977 真题名见 A#5 | 审查件自带文献失真（路线3 X2；与分歧台账 A-P2-02 一致） |
| 审查-1 核心结论“《已确立》§3 不存在 ⇒ 全部为幻觉锚” | 02 文档第 108 行即 “## 3. 有锚常数”，表内容与 05 所引一一对应，行漂移 +2/+3 | 指控不成立（路线3 X3；A-P2-01） |
| 审查-3 “registry:2015-2024 段落编号不存在” | v6_clause_registry_v1.json:2015–2024 实为 FZ-AP1-GLS-QW-RTOL，语义吻合 | 审查-3 误判（路线3 X4） |

## D. UNRESOLVED（不进正文；上呈/待补登记）

1. m_ref = 6.0 的一手文献锚（判定为单位制锚点，冻结纪律替代；两路独立确认无文献值）。
2. ζ = 1.152 的标定登记出处（数值已由 D-04 终裁并获三路 MC 支持；补登属文档待办）。
3. P-CST-11 两基准点的 (RN, g, σ_sky) 原始参数（曲线族已反解验证，x=0.557715/0.954658）。
4. P-CST-10 k_corr=1.3883 原始标定网格（构造性复原非原始数据；按 D-08 改查表，P3 承载）。
5. P-CST-23 声称三数的生成配置（S2 语义 + β≈2.5 + 窗帽 256 下部分复原：3 点同号同单调、2 点同量级）。
6. Moffat 1969 逐页公式核验（ADS 反爬）；Huber 1981 p.128 Eq.(33) 页码级定位。

## E. 核验方法备注

- Crossref REST API：https://api.crossref.org/works/<DOI> 逐条解析题名/作者/刊名/卷页/年；arXiv 官方 abs 页；IVOA 官方 PDF 流级解压全文检索。
- 三路独立核验（互不通信）交叉一致方为 VERIFIED；单路核验条目已注明路线来源。
- 全部数值科学量的正确性均不依赖文献全文：闭式推导腿 + 固定 seed 实验腿自足闭合（例如 Moffat4 常数）。