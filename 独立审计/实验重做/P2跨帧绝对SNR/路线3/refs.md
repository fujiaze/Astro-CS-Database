# P2 跨帧绝对 SNR · 路线 3 · 文献核验记录（refs.md）

**身份**：独立审计三路独立研究之路线 3（本路）。
**核验日期**：2026-09-26。核验工具：Crossref REST API（DOI 级一手书目数据）、arXiv API、web 检索。
**纪律**：只记真实核验到的条目；未核到的如实记 UNRESOLVED；审查件**建议引用**的文献同样逐条核验，不因"审查员写了"而采信。

---

## 一、已核验为真的一手文献（A 腿采用）

| # | 文献 | 核验到的书目事实 | 核验途径 | 用于 |
|---|---|---|---|---|
| L1 | Rousseeuw, P. J. & Croux, C. 1993, "Alternatives to the Median Absolute Deviation", **JASA 88(424), 1273–1283** | DOI `10.1080/01621459.1993.10476408`，作者 Rousseeuw, Croux，刊名 Journal of the American Statistical Association，卷 88 页 1273–1283，1993-12 | Crossref works API | P-CST-03（MAD 稳健尺度估计的权威文献；ARE(MAD)≈37% 出处） |
| L2 | Croux, C. & Rousseeuw, P. J. 1992, "Time-Efficient Algorithms for Two Highly Robust Estimators of Scale", **Computational Statistics, 411–428** | DOI `10.1007/978-3-662-26811-7_58`，CompStat 1992 会议文集 | Crossref works API | P-CST-03（《已确立》§3 行 3 指认的归属文献："PHASE2_SAMPLER.md:219 的归属注错应指 Croux & Rousseeuw 1992"） |
| L3 | Huber, P. J. 1981, *Robust Statistics*, Wiley（2004 二版 DOI `10.1002/0471725250`，monograph） | Crossref 记录 type=monograph，Wiley Series in Probability and Mathematics Statistics | Crossref works API | P-CST-03（书存在性已核；审查-1 所引"p.128 Eq.(33)"页码级定位**未核**，见 UNRESOLVED） |
| L4 | Stigler, S. M. 1977, "Do Robust Estimators Work with Real Data?", **The Annals of Statistics 5**, 1055–1098 | DOI `10.1214/aos/1176343997`，1977 | Crossref works API | P-CST-06（截尾均值的实证经典；注意审查-1 给出的题名与此不同，见"审查件内幻觉"） |
| L5 | Moffat, A. F. J. 1969, "A Theoretical Investigation of Focal Stellar Images", **A&A 3, 455** | bibcode `1969A&A.....3..455M`（仓库 PSF.md §14 与 NOISE_MODEL.md §14a 两处独立引用一致；A&A 1969 卷无 Crossref DOI） | bibcode 级（ADS 直接抓取被人机验证拦截，未取得原文，**未逐页核验**） | P-CST-05 / P-CST-15（Moffat 轮廓出处） |
| L6 | Horne, K. 1986, "An Optimal Extraction Algorithm for CCD Spectroscopy", **PASP 98, 609** | DOI `10.1086/131801`，PASP 98, 609, 1986-06 | Crossref works API | P-CST-11 / P-CST-23（σ_F⁻² = ΣP²/σ_i² 最优提取方差公式的文献锚） |
| L7 | Fruchter, A. S. & Hook, R. N. 2002, "Drizzle: A Method for the Linear Reconstruction of Undersampled Images", **PASP 114, 144–152** | DOI `10.1086/338393`（注意：`10.1086/341387` 是另一篇文章，勿混） | Crossref works API | P-CST-10（重采样相关噪声的上游文献）、链条位置 P3 佐证 |
| L8 | Bertin, E. & Arnouts, S. 1996, "SExtractor: Software for source extraction", **A&AS 117, 393–404** | DOI `10.1051/aas:1996164` | Crossref works API | P-CST-15（背景估计/掩膜实践参照） |
| L9 | Stetson, P. B. 1987, "DAOPHOT", **PASP 99, 191** | DOI `10.1086/131977` | Crossref works API | P2 逐源测光背景（链上参照） |
| L10 | Newberry, M. V. 1991, "Signal-to-Noise Considerations for Sky-Subtracted CCD Data", **PASP 103, 122** | DOI `10.1086/132801`，1991-01 | Crossref works API | 噪声组成（P-CST-11 口径参照；NOISE_MODEL §14 已引，本路独立复核 DOI 为真） |

## 二、核验为"审查件内幻觉/失真"的引用（拒绝采信）

| # | 审查件主张 | 核验结果 | 定性 |
|---|---|---|---|
| X1 | 审查-1 P-CST-04 订正建议："Peacock, P. (1984). Comparing Distributions, Ap. J. 283, 387" | Crossref 检索无此文；存在的相近真文是 **Peacock, J. A. 1983, "Two-dimensional goodness-of-fit testing in astronomy", MNRAS 202, 615–627**（DOI `10.1093/mnras/202.3.615`），且其主题（2D KS 检验）与高斯 FWHM 换算无关 | 审查件自带的文献幻觉（卷/年/刊/题名均不符）；且即便改指真文也与该常数无关 |
| X2 | 审查-1 P-CST-03 订正建议："Stigler ... 'Do Estimators of Location Have Optimal Properties?', p.267-284 North-Holland" | 未检索到该文；Stigler 1977 真文题名为 "Do Robust Estimators Work with Real Data?"（Ann. Statist. 5，DOI 见 L4） | 审查件自带文献失真（题名/出版信息不符） |
| X3 | 审查-1 核心结论："《已确立》文档无 §3 节 ⇒ '《已确立》§3 行 X' 全部为幻觉锚" | `02_已确立的算法与验证程序.md` 第 108 行即 `## 3. 有锚常数`；§3 表 18 行内容与 05 所引常数一一对应 | **审查-1 的致命 finding 本身不成立**；真实问题是"行 X＝表内行号"且表增行后漂移 +2/+3（见报告锚核查表） |
| X4 | 审查-3 幻觉锚清单："registry:2015-2024 的 JSON 路径不存在该段落编号" | `v6_clause_registry_v1.json` 第 2015–2024 行实为 `FZ-AP1-GLS-QW-RTOL` 条目，subject 恰为"Q/W==GLS 与 Var=1/W 相对容差"，与 05 引用语义一致 | 审查-3 误判；该锚真实有效 |

## 三、UNRESOLVED（本路查不到、绝不编造）

| # | 事项 | 状态 |
|---|---|---|
| U1 | P-CST-07 `√(π/2)=1.2533…` 的**免费可核验一手电子源**（教科书级渐近结果：SE(median)=√(π/2)·σ/√N） | UNRESOLVED：多轮检索未定位到可抓取的一手页；理论腿（本报告自足推导）与实验腿（EXP-03 MC）已闭合，仅缺文献标识 |
| U2 | P-CST-10 标定值 **1.3883** 的复现 | UNRESOLVED：原始标定网格（"实验网格"）在仓内未登记，本路以 AR(1) 机制实验证明 k_corr 依赖相关模型声明，无法从现有信息复算该数 |
| U3 | P-CST-19 `m_ref = 6.0` 的文献依据 | UNRESOLVED：未找到任何一手出处支持 6.0 mag 作为参考星等档（Gaia G 制）；05 已按"待确认"登记，本路确认"无文献值"这一事实本身 |
| U4 | P-CST-22 的 23.3%/36.3% 两数对应的**具体（相关模型, 轮廓, 窗口）组合** | UNRESOLVED：EXP-05 H3 证明偏差比随相关长度变号（−47.6%…+133%），两数只能在特定模型下复现；P-OPEN-02 悬置正确 |
| U5 | P-CST-23 存档量级三数（SNR 偏 +4e-6 / +2.4e-4 / +1.33%）的精确复现 | UNRESOLVED：05 未给出生成该三数的 FWHM/窗/轮廓配置，欠定；EXP-06 H2 给出同机制完整敏感性曲线 |
| U6 | Moffat 1969 逐页公式核验 | bibcode 级核验（L5）；ADS 原文抓取被人机验证拦截 |
| U7 | 审查-1 提议的 "Huber 1981, p.128, Eq.(33)" 页码级定位 | 书已核验为真（L3），页码/式号未核验 |

## 四、核验方法备注

- Crossref 查询形如 `https://api.crossref.org/works/<DOI>` 与 `...?query.bibliographic=...`，返回 JSON 元数据（题名/作者/刊名/卷页/年）。
- 本路不采信任何"据称存在"的文献：每条 A 腿锚至少给出 DOI 或 bibcode 级事实；两条（L5、L3 页码）明确标注核验层级不足。
- 审查-1 提议的 A&S 26.2.9（正态分位数）一类数学手册引用：该常数为数学恒等式，本路按"公式导出，无外部文献需求"口径处理（与审查-3 的备选方案一致），不引入无关文献。
