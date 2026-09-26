# P5 加性天光去除 · 路线3 文献核验记录

核验日期：2026-09-26。核验渠道：Crossref API（DOI 题录）、Semantic Scholar API、arXiv abs/PDF 原文抓取（scholar_fetch）。凡未标 ✅ 的条目如实登记核验深度，绝不补编。

---

## R1 ✅ Huber 1964（Q5 理论/文献腿主锚）

- 题录：Peter J. Huber, **Robust Estimation of a Location Parameter**, *The Annals of Mathematical Statistics* 35(1): 73–101, 1964-03。
- DOI：[10.1214/aoms/1177703732](https://doi.org/10.1214/aoms/1177703732)（Crossref 核验：出版商 Institute of Mathematical Statistics，卷期页码一致，被引 5571）。
- 关键内容：M-估计、minimax 位置稳健性；Huber ψ 函数与 IRLS 形式（后者经 R2 落地）。
- 适用域：位置估计、对称污染分布；标度需另行估计（本项目以 MAD×1.4826 供给）。
- 用于：Q5（δ=1.345 的 95% 渐近效率）。

## R2 ✅ Holland & Welsch 1977（Q5 IRLS 与 δ 取值表）

- 题录：P. W. Holland, Roy E. Welsch, **Robust regression using iteratively reweighted least-squares**, *Communications in Statistics* 6(9): 813–827, 1977。
- DOI：[10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533)（Semantic Scholar 题录核验：作者/年份/题名/DOI 一致）。
- 关键内容：IRLS 算法化；给出高斯 95% 效率对应的 Huber 常数推荐值 δ=1.345（该值后来亦载于 Huber–Ronchetti 2009 表）。
- 用于：Q5。

## R3 题录级 Huber & Ronchetti 2009

- **Robust Statistics**, 2nd ed., Wiley, 2009（ISBN 978-0-470-12990-6）。书籍级支撑（ARE 公式与 1.345 取值的标准现代出处），本次未逐页核对，效力次于 R1/R2。

## R4 ✅ Serfling 1980（Q3 渐近中位数方差）

- 题录：Robert J. Serfling, **Approximation Theorems of Mathematical Statistics**, Wiley, 1980（§2.3.2：样本分位数/中位数的渐近正态与方差 1/(4Nf(m)²)）。
- DOI：[10.1002/9780470316481](https://doi.org/10.1002/9780470316481)（Crossref 核验：书名/年份/DOI 一致）。
- 用于：Q3（control_variance 中 `Var(median)=1/(4Nf(m)²)`，高斯特例 πσ²/(2N)）。

## R5 题录级 Kendall & Stuart, *The Advanced Theory of Statistics*, Vol.1

- 次序统计量与 MAD→σ 常数（1/Φ⁻¹(3/4)=1.48260221850560…）的教科书出处；该恒等式本身为数学定理，本路线以**解析推导 + 数值验证（误差 2.2e-16，`results/q3_median_variance.json`）**双重闭环，不依赖书籍页码。

## R6 ✅ Andrae, Schulze-Hartung & Melchior 2010（Q6 主锚，逐字核验）

- 题录：Rene Andrae, Tim Schulze-Hartung, Peter Melchior, **Dos and don'ts of reduced chi-squared**, arXiv:1012.3754, 2010-12-16。
- 原文 PDF 已抓取（arXiv），式 (8)(9) 逐字核验：
  - 式(8)：`P_eff = tr(H) = Σ H_nn = rank(X)`（hat 矩阵迹 = 有效参数数 = 设计矩阵秩）；
  - 式(9)：`K = N − P_eff ≥ N − P`（线性模型自由度由秩决定）；正文明确 "The standard claim is that a linear model with P parameters removes P degrees of freedom … Is this correct? No, not necessarily so."
- 用于：Q6（χ²_red 分母必须取 n_obs − r_eff）。实验 `exp06` 同时数值验证式(8)（tr(H)=7.0000, 差 8.9e-16）。

## R7 ✅ Fruchter & Hook 2002（Q4 k_corr>1 的概念锚）

- 题录：A. S. Fruchter, R. N. Hook, **Drizzle: A Method for the Linear Reconstruction of Undersampled Images**, *PASP* 114: 144（arXiv:astro-ph/9808087，PDF 已抓取）。
- 关键内容（§7 原文）："Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated."；定义 **noise correlation ratio R** 并指出 "the correlation between pixels, and thus R, depends on the choice of drizzle parameters"（含 pixfrac）。
- 适用域：重采样后相关噪声的方差膨胀；与本项目 k_corr 同概念（N_eff = N_retained/k_corr）。
- 用于：Q4。**注意**：正本 §9 的 MC 实证值 1.3883 来自项目自产 harness（pixfrac=0.8, 2000 次），其几何未在条文级公开，本路线独立几何复测得 2.38（pixfrac=0.8、等尺度网格、亚像素位移 (0.31,0.47)），同量级同方向，不宣称逐位复现（详见 report §Q4 诚实边界）。

## R8 ✅ Padmanabhan et al. 2008（Q1 门槛量级的文献对照锚）

- 题录：N. Padmanabhan, D.J. Schlegel, D.P. Finkbeiner, et al., **An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data**, *ApJ* 674: 1217–1233, 2008（arXiv:astro-ph/0703454，摘要已抓取）。
- 关键内容（摘要原文）："we achieve ~1% relative calibration errors across 8500 deg² in griz; the errors are ~2% for the u band"。
- 用法：1e-2 接缝门槛的**数量级对照锚**——当代巡天重叠定标的相对一致性地板在 1% 量级，故以 1% 作为帧间亮度连续性验收门与业界最好水平同量级。**这不是推导**：正本 §17 已声明 1e-2 非误差预算推导、由 3.5σ 虚警标定内生确定；R8 只回答"该量级是否与文献可比"。

## R9 ✅ Gruen, Seitz & Bernstein 2014（题录核验 + 引用面核查）

- 题录：D. Gruen, S. Seitz, G. M. Bernstein, **Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking**, *PASP* 126: 158–169, 2014-02。DOI：[10.1086/675080](https://doi.org/10.1086/675080)（Crossref 核验）。
- **引用面核查（与正本的差异）**：`docs/science/PHASE2_UPM.md` §14a 将该文列作马赛克逐帧背景扣除/coadd 权重的支撑文献；该文实际内容是 SWarp 中以 clipped mean 做稳健叠加剔除伪影的**实现**，与 robust stacking 相关，但**不是**专门的背景匹配/接缝消除文献。建议正本引用面收窄为"稳健叠加统计"（与本路线结论不冲突，属引用精度问题）。

## R10 ✅ Hoerl & Kennard 1970（Q9 岭收缩偏置）

- 题录：Arthur E. Hoerl, Robert W. Kennard, **Ridge Regression: Biased Estimation for Nonorthogonal Problems**, *Technometrics* 12(1): 55–67, 1970-02。DOI：[10.1080/00401706.1970.10488634](https://doi.org/10.1080/00401706.1970.10488634)（Crossref 核验，被引 13300）。
- 关键内容：岭估计 θ̂=(XᵀX+λI)⁻¹Xᵀy 的偏置-方差交换；正交情形逐坐标收缩因子 Σw/(Σw+λ)——即 `exp09` 验证的闭式。
- 用于：Q9（弱零锚 zero_anchor_weight 的收缩代数）。

## R11 ⚠️ 题录级/未直达 Tikhonov 1963

- 标准引用：A. N. Tikhonov, **Solution of incorrectly formulated problems and the regularization method**, *Soviet Math. Dokl.* 4: 1035–1038, 1963。
- 核验状态：**UNRESOLVED（题录级）**。Crossref 未收录该 1963 Doklady 俄文译刊条目；Semantic Scholar 查询遭限流（HTTP 429）两次未成。本路线 Q9 的收缩代数内容已由 R10（Hoerl & Kennard 1970，✅）完全承载，Tikhonov 原文不构成 Q9 的必要依赖。

## R12 ✗ quality_factor 降权档 0.1/0.5 —— 文献腿 UNRESOLVED（结构性豁免）

- 检索（arXiv/Crossref/web）未找到任何管线给出普适的"质量旗降权因子"文献值；主流管线（SDSS ubercal、SWarp）对质量旗采取**直接剔除**而非固定档位降权。
- 处置：按纪律作**结构性豁免**——它是无物理真值可对照的策略常数（详见 report §Q12），科学内核（方向单调 + 泄漏可预言）已由 `exp12` 实验腿闭合；数值本身应按修复包 `02` §4 第 4 类登记"待确认"（与 D-56 处置一致）。

---

## 核验方式附注

- Project Euclid（Huber 1964 全文页）被 Incapsula 拦截（HTTP 200 但返回拦截页），故 R1 改以 Crossref 题录核验；ARE 数值内容另由 Q5 理论闭式 + MC 独立闭合，不依赖全文。
- `web_search` 工具在本会话返回空结果，全部核验改走 DOI 解析 API 与 arXiv 原文抓取（各调用见上）。
- 所有核验仅记录到题录与被引用的关键命题层级；未逐页核对书籍与长文全文，超出以上登记的核验深度不得外推。
