# refs.md · 文献核验台账（P1 通量积分拟合 / 实验/photometric-magnitude）

**口径**：只收一手 **VERIFIED** 条目（核验途径 + 关键原句或书目级证据可回溯）。本台账在实验重做三路 refs（`独立审计/实验重做/P1通量积分拟合/路线{1,2,3}/refs.md`）基础上收口；分歧裁决一律以 `独立审计/实验重做/总编对账/分歧台账.md` 为准。
**格式**：DOI/arXiv ＋ 核验方式 ＋ 用途。

---

## V1 Kafadar 1983（c = 4.685 ⇔ 正态 95% 渐近效率的一手出处）

- **书目**：Karen Kafadar (1983), "The Efficiency of the Biweight as a Robust Estimator of Location", *J. Res. Natl. Bur. Stand.* 88(2), 105–116. DOI: [10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)，全文 PMC: [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/)。
- **核验方式**：全文取回（PMC），关键原句照抄："Asymptotically, c = 4.685 yields 95% asymptotic efficiency at the Gaussian…"。**台账 D-06**：父代理直验原文含 c=4.685 原句 ⇒ **锚有效**；路线2 的"PMC 锚内容不符"误判撤回。
- **用途**：`tukey_c = 4.685` 的文献锚（`PHOTOMETRY.md` §14 维持不改，按 D-06）。

## V2 statsmodels `robust/_tables.py`（c = 4.685 的开源逐字锚）

- **核验方式**：开源逐字核验，值 4.685065 与本实验解析解 c* = 4.6850649 六位一致 [实验:code/redo/route2/exp1_robust_constants.py]。
- **用途**：S1 三腿闭合的独立第三方数值锚。

## V3 Beaton & Tukey 1974（Tukey biweight 出处）

- **书目**：A. E. Beaton, J. W. Tukey (1974), "The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data", *Technometrics* 16, 147–185. DOI: [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)。
- **核验方式**：Crossref API 书目级（authors/volume/page/DOI 全符）；Crossref 与 OpenAlex 均记 issue=2（原文未取回，期号以出版商元数据为准）。
- **用途**：biweight 权函数 w=(1−u²)² 的历史出处（书目级引用）。

## V4 Holland & Welsch 1977（IRLS 权重常数表）

- **书目**：P. W. Holland, R. E. Welsch (1977), "Robust regression using iteratively reweighted least-squares", *Communications in Statistics – Theory and Methods* 6(8), 813–827. DOI: [10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533)。
- **核验方式**：Crossref API 书目级（本轮补核：卷 6、页 813–827、1977）。
- **用途**：IRLS 权函数与权重常数表的经典出处（与 V3 配对引用）。

## V5 Rousseeuw & Croux 1993（MAD 效率 37% / 标准化方差 1.361）

- **书目**：P. J. Rousseeuw, C. Croux (1993), "Alternatives to the Median Absolute Deviation", *JASA* 88(424), 1273–1283. DOI: [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)。
- **核验方式**：Crossref 书目 ＋ KU Leuven 官方镜像 PDF 全文 OCR；关键原句照抄："the MAD is only 37% efficient"；Table 2 n=∞ 行 MAD 标准化方差 = **1.361**。
- **用途**：判据因子 1.166 = √1.361 的文献锚（**按台账 A-P1-01 订正解释标签**：1.361 是 MAD 尺度估计量的**标准化方差**，1.166 = √1.361 是 σ̂ 的相对标准差因子）。

## V6 Gaia DR3 总览（源计数与密度归一）

- **书目**：Gaia Collaboration et al. (2023), *A&A* 674, A1. arXiv: [2208.00211](https://arxiv.org/abs/2208.00211)。
- **核验方式**：arXiv 摘要页 ＋ 1.8e9 源计数原句（路线1）；**arXiv 号自纠**：审查引文 2205.11321 被证伪（实为计量论文），正确号 2208.00211。
- **用途**：FOV/阶梯实验的天空密度归一（820/deg²，G<16 全天平均）。

## V7 Montegriffo et al. 2023a — Gaia XP 合成测光（F_syn 口径）

- **书目**：Gaia Collaboration, P. Montegriffo et al. (2023), "The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra", *A&A* 674, A33. arXiv: [2206.06215](https://arxiv.org/abs/2206.06215)，DOI: [10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)。
- **核验方式**：Crossref 书目 ＋ 摘要/正文原句照抄："Synthetic photometry directly tied to a flux in physical units…"；"passbands… combination of… filter… the sensitivity curve of a photon-counting detector"。
- **用途**：`F_syn = ∫F_λ·T·Q·λ dλ` 的物理刻度与通带定义佐证。

## V8 Montegriffo et al. 2023b — XP 外定标（±2% 精度）

- **书目**：P. Montegriffo et al. (2023), "External calibration of BP/RP low-resolution spectra", *A&A* 674, A3. arXiv: [2206.06205](https://arxiv.org/abs/2206.06205)。
- **核验方式**：Crossref/arXiv 书目 ＋ §8.1 原句（XP 外定标 ±2%, λ≳400 nm；Fig. 27 作图重标度原句）。
- **用途**：绝对刻度系统差量级；佐证 `10^(−0.4·G)` 不入 F_syn（`RESOLUTION_fsyn_formula.md`）。

## V9 ESA Gaia DR3 官方文档（XP 采样谱与零点定义）

- **核验方式**：官方文档一手原句：§20.12.4 `xp_sampled_mean_spectrum` flux 字段 "Externally-calibrated combined BP and RP flux"，343 点 @2 nm，336–1020 nm；§5.4.1 式 (5.41) ⟨f_λ⟩ = ∫f_λ S λ dλ / ∫S λ dλ；绝对刻度 "1 % is thought to be the current state-of-the art…"。
- **用途**：F_syn 官方定义式（无星等因子）；XP 网格参数；绝对刻度上限。

## V10 Gaia DR3 XP 官方发布（G < 15 域限制）

- **核验方式**：官方发布页（XP 可用域 G<15 的官方声明，三路均引用同一域限制）。
- **用途**：F_syn 可用域（G<15 限制）。

## V11 Aitken 1935（反方差加权口径）

- **书目**：A. C. Aitken (1935), "On Least Squares and Linear Combination of Observations", *Proceedings of the Royal Society of Edinburgh* 55, 42–48. DOI: [10.1017/S0370164600014346](https://doi.org/10.1017/S0370164600014346)。
- **核验方式**：Crossref API 书目级（本轮补核：卷 55、页 42–48；Crossref issued 记 1936，惯例引 1935，如实记录）。
- **用途**：项目反方差（inverse-variance）加权口径的统一定位锚（**负责人已批**：P1/P2 涉及权重处引用）；本单元 IRLS 权为稳健权（非反方差权），反方差口径通过 `ivar′ = ivar/α²` 的量纲变换进入下游（P5）。

## V12 GaiaXPy 2.1.4（开源实现锚）

- **核验方式**：官方开源实现逐字核验：`src/gaiaxpy/spectrum/sampled_spectrum.py:114` 纯线性组合，无星等因子；`calibrate()` vs 官方 `XP_SAMPLED` 产品比值中位 1.000000。
- **用途**：F_syn 绝对口径的实现侧旁证（`RESOLUTION_fsyn_formula.md`）。

---

## 不列 VERIFIED（待补脚注）

- **Croux & Rousseeuw 1992/1993**（有限样本 MAD 偏差表值）：MC 复算佐证偏差 ≤0.93%，原文未取得——**不列 VERIFIED**，作"待补"脚注（简报口径）；相关数值以推导腿 ＋ 本单元 MC 实验（[实验:code/redo/route3/exp_S11_zp_sample_floor.py]）自足承载。
- **Huber & Ronchetti 2009 §6.5**：书目级；页码 UNRESOLVED——降级引用或改引 Kafadar（V1），本单元正文改引 V1。
- **Lindegren 2021 亮端数字**：仍开放（不阻成稿，见诚实边界）。
