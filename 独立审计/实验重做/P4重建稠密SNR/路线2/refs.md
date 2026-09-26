# 路线2 · 文献核验日志（refs.md）

模块：P4 重建稠密 SNR。核验原则：只登记本路线**亲自打开过原始来源**（arXiv abs 页 / Crossref / DOI 解析页 / 权威索引页）的条目；核验层级分三档并如实标注：

- **全文级**：取回并阅读了正文关键段落；
- **元数据级**：核实了作者/年份/卷期页/DOI 或 arXiv 号与条目内容的对应关系，未逐页读原文；
- **未核验**：仅有二手出处，明确标注。

---

## R-01 Shepard, D. (1968). "A two-dimensional interpolation function for irregularly-spaced data."

- 出处：Proc. 1968 23rd ACM National Conference, pp. 517–524. DOI: 10.1145/800186.810616
- 核验方式与层级：经 IDW 词条（Wikipedia，scholar_fetch 取回）与 ACM DL DOI 条目交叉核对 —— **元数据级**
- 支撑项：I-02（P-CST-24，IDW 权重 p 幂次形式的原始出处：w ∝ 1/d^p，p 由应用自定）
- 用途限定：仅证明"幂次倒数加权插值"这一算子形式有 1968 年一手出处；**p=2 的具体取值 Shepard 原文并不锁定为普适常数**（p 是应用参数）。
- 备注：Shepard 原文推荐的默认幂次与现代常用 p=2 一致，但本路线未逐页核对原式（原文为付费 ACM 档案）。

## R-02 Rousseeuw, P. J. & Croux, C. (1993). "Alternatives to the Median Absolute Deviation." JASA 88(424), 1273–1283.

- DOI: 10.1080/01621459.1993.10476408（Crossref 核验通过：作者/卷期/页码全对上）
- 核验方式与层级：Crossref API 元数据 —— **元数据级**（ARE(MAD)=0.3675 这一数值另经 E08 的蒙特卡洛独立复算闭环：实测 k_eff=1.1614 vs 理论 1.1664，见 exp08）
- 支撑项：I-08（P-CST-03/08：MAD→σ 因子与估计效率、9216 预算链的 k_eff 文献腿）
- 备注：Gaussian 下 ARE(MAD vs 样本标准差)=0.3675 ⇒ k_eff = sqrt(1/(2·0.3675)) = 1.1664，与登记值 1.152 相差 −1.2%（差异来源未定，见报告 UNRESOLVED-1）。

## R-03 Zackay, B. & Ofek, E. O. (2015). "How to Coadd Images? Optimal Combination of Images..."

- arXiv:[1512.06872](https://arxiv.org/abs/1512.06872)（abs 页核验通过）；姊妹篇 arXiv:[1512.06879](https://arxiv.org/abs/1512.06879)（abs 页核验通过）
- 核验方式与层级：arXiv abs 页 —— **元数据级**
- 支撑项：I-01（逆方差定权是最优堆叠的理论结论，γ=2 的物理内容）、I-07（P5 消费段：最优权重堆叠方差 ≤ 等权）
- 用途限定：证明"最优加权（含 SNR² 型权重）优于等权"这一方向性结论；本路线 E01/E07 的结论以自身蒙特卡洛闭环为准，文献仅作方向佐证。

## R-04 Górski, K. M. et al. (2005). "HEALPix: A Framework for High-Resolution Discretization..." ApJ 622, 759.

- arXiv:[astro-ph/0409513](https://arxiv.org/abs/astro-ph/0409513)（abs 页核验通过）
- 核验方式与层级：arXiv abs 页 —— **元数据级**
- 支撑项：I-02/链条位置（drizzle 侧 snr_evaluator 运行在 HEALPix 像元上的 K 近邻/角距语义）
- 备注：仅用于确认球面评估器的几何语境（great-circle 角距 gamma 单位为度），非数值判据来源。

## R-05 Akinshin, G. (2022). "Finite-sample bias correction for the Mean Absolute Deviation."

- arXiv:[2207.12005](https://arxiv.org/abs/2207.12005)（abs 页核验通过）
- 核验方式与层级：arXiv abs 页 —— **元数据级**
- 支撑项：I-08（MAD 有限样本偏差：本路 E08 实测偏差 −7.2e-4 @ n=4000，与"偏差 O(1/n) 且小"一致）
- 备注：本路线未采用其有限样本修正系数改写登记常数——P-CST-03 登记的是渐近常数 1.4826（A 腿 FP64 恒等成立），有限样本偏差量级不改变 9216 预算结论。

## R-06 Astier, P. & Antilogus, P. "The shape of the Photon Transfer Curve of area array CCDs."

- arXiv:[1905.08677](https://arxiv.org/abs/1905.08677)（abs 页核验通过）
- 核验方式与层级：arXiv abs 页 —— **元数据级**
- 支撑项：I-05（PTC 形状：方差随电平的线性段=散粒斜率 1、乘性段斜率 2 的领域文献佐证；本路 E05 log-log 斜率实测 1.000/2.000 独立闭环）
- 备注：仅方向性佐证；§5b 的 relspread 律推导以本路解析推导（方差 ∝ 电平 ⇒ relspread 不变；∝ 电平² ⇒ ×2）为准。

## R-07 Ma, J. & Shang, Z. (2014). "New Method for Computing the Photon Transfer Curve..."

- arXiv:[1407.8280](https://arxiv.org/abs/1407.8280)（abs 页核验通过）
- 核验方式与层级：arXiv abs 页 —— **元数据级**
- 支撑项：I-05（同 R-06，PTC/噪声随电平标度的领域语境）

## R-08 Moffat, A. F. J. (1969). "A Theoretical Investigation of Focal Stellar Images..." A&A 3, 455.

- bibcode: 1969A&A.....3..455M（ADS 文章级条目核验通过）
- 核验方式与层级：**文章级**（题名/卷页经 ADS 条目确认；**页码/式号级未核**，与仓库 PSF.md:177 自我标注的"文章级"状态一致）
- 支撑项：I-09（Moffat 轮廓家族与 FWHM↔尺度参数关系的历史出处；本路 E09 的解析推导 FWHM/σ = 2√2·√(2^{1/4}−1) = 1.2303077 不依赖原文页码，属自足推导）
- 备注：审查-1 对 P-CST-05 的"方案 B（改理论推导）"路径由此闭合，无需找回 SCI-PSF-001 实验件。

## R-09 审查员建议、本路线**未**独立核验的条目（如实登记）

- Peacock, P. (1984). Ap. J. 283, 387（审查-1 为高斯 FWHM↔σ 量化建议）—— 未核验，不引用。
- Stigler, S. M. (1977)（审查-1 为 P-CST-06 截尾均值建议）—— 未核验，不引用；P-CST-06 不在本路线清单内。

---

## 核验方法附注

- arXiv abs 页经 scholar_fetch 取回（HTML→文本），以题名/作者/摘要与用途比对；
- Crossref 经 DOI 解析核对元数据四要素（作者/年份/卷期/页码）；
- 本路线对每条文献的**用途限定**都显式写出：凡数值判据一律以本路实验（固定 seed、含负例）闭环为准，文献腿只承担"算子形式/方法学有出处"的职责。这是三腿模型中文献腿的正确职责边界：文献不能替代实验腿，实验腿也不需要文献替它背书数值。
