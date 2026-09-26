# refs.md · 文献核验记录（路线 3 · P1 通量积分拟合）

**身份**：独立审计实验重做·路线 3。本件记录报告.md 引用的每一条文献的核验过程与状态。
**纪律**：只记真实发生过的核验；未取到原文的一律标明，绝不把"听说"写成"已核"。

状态图例：
VERIFIED-FULL = 取回并读到原文相关段落；VERIFIED-ABS = 取回并读到官方摘要页；
VERIFIED-URL = 检索命中权威镜像/收录页（未取全文）；PARTIAL = 部分要素已核；
UNRESOLVED = 查不到或不满足判据。

---

## L1. Tukey biweight c=4.685 ⇒ 95% 渐近效率

- **Kafadar, K. (1983). "The Efficiency of the Biweight as a Robust Estimator of Location."**
  *J. Res. Natl. Bur. Stand.*, 88(2), 105–116. doi:[10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)
  全文经 PMC 取回：[PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/) — **VERIFIED-FULL**。
  原文关键句（逐字）："Asymptotically, c = 4.685 yields 95% asymptotic efficiency at the Gaussian
  [i.e., n T converges in distribution to N(0,1.0526)]"。
  该文同时给出 c=6、n=20 时 98.2% 效率等有限样本行为 —— 本路线 S01 的"n=20 有限样本效率
  0.914、n=200 0.946"与该文"渐近值属 n→∞"的口径一致。
- **Beaton, A.E. & Tukey, J.W. (1974).** "The Fitting of Power Series, Meaning Polynomial..."
  *Technometrics* 16(2), 147–185（biweight/bisquare ψ 的原始出处）。
  **UNRESOLVED（原文未在线取回）**——只作为历史出处提及，本路线的数值断言全部挂在 L1 主条与 S01 实验上，不挂此条。
- **Holland, P.W. & Welsch, R.E. (1977).** "Robust regression using iteratively reweighted
  least-squares." *Comm. Statist.-Theory Meth.* A6, 813–827。**VERIFIED-URL（下限）**：
  在 Kafadar 1983 参考文献中以编号 [19] 出现（已取回文本可见），原表未取回。
  注：审查员 01/C11 曾引用该文为 1.345 的书目——本路线不为其背书，也不使用。

## L2. MAD 一致性常数 0.6744897501960817 = Φ⁻¹(3/4)

- **Wikipedia "Median absolute deviation"**：[链接](https://en.wikipedia.org/wiki/Median_absolute_deviation)
  取回并读到推导段："k = 1/(Φ⁻¹(3/4)) ≈ 1/0.67449 ≈ 1.4826" — **VERIFIED-FULL**（二手，作为定义级常识的定位锚）。
- **Rousseeuw, P.J. & Croux, C. (1993).** "Alternatives to the Median Absolute Deviation."
  *JASA* 88(424), 1273–1283。作者机构镜像：
  [KU Leuven PDF](https://wis.kuleuven.be/stat/robust/papers/publications-1993/rousseeuwcroux-alternativestomedianad-jasa-1993.pdf)
  检索命中 — **VERIFIED-URL**（PDF 未逐页读；本路线对 MAD 的数值断言不依赖其正文，由 S02 实验自证）。
- **Akinshin, A. (2022).** "Finite-sample bias-correction factors for the median absolute
  deviation..." arXiv:[2207.12005](https://arxiv.org/abs/2207.12005)。摘要页取回 — **VERIFIED-ABS**。
  摘要逐字："For finite samples, the scale constant should be corrected in order to obtain an
  unbiased estimator... When we use the traditional sample median, the factor values are well known."
  ⇒ "MAD 小样本偏差需要校正、传统中位数版的校正因子是已知经典值"有文献锚；
  本仓 02 式-3 的 n=3 偏低 ≈1.49 倍断言由 S02 蒙特卡洛独立复算（得 1.4918，见 results/exp_S02）。

## L3. 样本中位数的标准误（SE(median)）

- 渐近式 SE(median) → √(π/(2n))·σ（正态）：交叉验证社区讨论页命中
  [stats.stackexchange.com/questions/59838](https://stats.stackexchange.com/questions/59838/standard-error-of-the-median)
  — **VERIFIED-URL**（未逐句取回）。
- 教科书（David & Nagaraja, *Order Statistics*, 3rd ed., Wiley 2003；Kenney & Keeping 1962）：
  **UNRESOLVED**（书目在线正文未核验，不作为数值断言的载体）。
- 补偿措施：S11 在脚本内对 n=3 做了**精确**序统计量数值积分
  （f_{X(2)}(x) = 6·Φ(x)(1−Φ(x))·φ(x)），得 SE(n=3)=0.6698σ，与 MC 0.6694 一致——
  渐近式 1.2533/√n 在 n=3 处**高估 7.4%**（比值 0.9257）。即：本路线对中位数 SE 的
  全部数值断言由解析积分+MC 自证，文献腿只承担"该结果属于标准序统计量"的定位。

## L4. Gaia DR3 XP 采样谱的发布范围与外部定标精度

- **Montegriffo, P. et al. (2023).** "Gaia Data Release 3: External calibration of BP/RP
  low-resolution spectroscopic data." *A&A* 674, A3. arXiv:[2206.06205](https://arxiv.org/abs/2206.06205)。
  全文经 ar5iv 取回 — **VERIFIED-FULL**。两条关键逐字证据：
  1. "BP and RP spectra are also provided in the sampled representation ... **including only
     sources brighter than G = 15 mag** (the exact list can be obtained by selecting entries with
     gaia_source.has_xp_sampled='t')" ⇒ **XP 采样表示官方只对 G<15 发布**（本仓 XPSD 容器的源头）。
  2. 定标器 SPSS "calibrated to the CALSPEC scale ... with flux accuracy of about 1%" ⇒ XP 绝对
     刻度的外部精度为 1% 量级（S06 的 Simpson 误差判据 2nm 网格 <1% 由此定位）。
- **Gaia Collaboration / Montegriffo, P. et al. (2023).** "The Galaxy in your preferred colours.
  Synthetic photometry from Gaia low-resolution spectra." *A&A* 674, A33.
  arXiv:[2206.06215](https://arxiv.org/abs/2206.06215)。摘要页取回 — **VERIFIED-ABS**：
  "flux-calibrated low-resolution spectrophotometry for about 220 million sources in the
  wavelength range 330nm - 1050nm... Synthetic photometry... for **any passband fully enclosed
  in this wavelength range**" ⇒ 式-1 的"通带必须被 XP 覆盖完全包含"正本得到文献直接支持；
  "up to millimag accuracy when synthetic photometry is standardised" ⇒ mmag 档定位（S08 的
  N=2000 ⇒ 1.4 mmag 地板与之同档）。
- **Gaia DR3 文档 §20.12.4**（采样网格 336–1020 nm、步长 2 nm、343 点）：
  **PARTIAL** —— 官方文档页在本次环境取回失败（404）；已核的是：
  (a) 仓库正本 PHOTOMETRY.md:230 的逐字引用存在；
  (b) 网格算术自洽：336 + 2×342 = 1020，343 点 342 区间（偶）—— S06 已复核；
  (c) 与 L4 第一条的 "sampled representation on a default grid" 表述相容。
  该网格数值的**独立**一手核验：UNRESOLVED（遗留）。

## L5. Gaia 天体测量精度与星表范围（匹配半径/星等门背景）

- **Lindegren, L. et al. (2021).** "Gaia Early Data Release 3: The astrometric solution."
  *A&A* 649, A2. arXiv:[2012.03380](https://arxiv.org/abs/2012.03380)。摘要页取回 — **VERIFIED-ABS**：
  "1.812 billion sources in the magnitude range **G = 3 to 21**" ⇒ (a) 全天平均面密度背景
  1.8e9/41253 deg² ≈ 43.6 /deg²（S08 密度模型的锚）；(b) mag_min=6.0 在 Gaia 星表范围 [3,21]
  内、不触及饱和端 —— S09 的 mag_min_note。
  ⚠️ 亮端位置误差 "0.02–0.03 mas @G≲14" 这一**具体数字**未从原文取回核实 →
  对该具体数值记 **UNRESOLVED**；S10 只使用其量级推断（≤0.73"/px 像元下 <1e-3 px），
  且把抖动预算的主项放在本仓质心门 p95≤0.3 px（仓库内锚 N-23）上，不依赖该未核数字。

## L6. 光子计数通带约定（式-1 的 λ 因子）

- **Bessell, M.S. & Murphy, S.J. (2012).** "Spectrophotometric Libraries, Revised Photonic
  Passbands and Zero-points for UBVRI, Hipparcos and Tycho Photometry." *PASP* 124, 140.
  arXiv:[1112.2698](https://arxiv.org/abs/1112.2698)。摘要页取回 — **VERIFIED-ABS**：
  "improved **photonic passbands**..."（光子加权通带是标准约定）。
- Gaia 官方式（5.41，含 λ 权重）经 PHOTOMETRY.md §2a.3 转引 + Montegriffo A3/A33 的
  photonic 口径 + S06 的 F_syn ≡ hc·N_γ 第一性复算（rel_diff <1e-12）三路闭合。
- **Pogson, N. (1856).** MNRAS 17, 12（2.5 星等定义）：**定义常数，豁免**（见报告 §S14），
  未取原文，也不需要——定义无实验可否。

---

## 幻觉锚核验（05_正向规格.md 引锚抽验）

见 results/exp_S00_anchor_verification.json（15 锚全开、判别片段 15/15 命中）。结论摘要：

| 锚 | 判定 |
|---|---|
| filter_curve_json.h:356 / :444 / :207 | 真实，函数签名相符 |
| star_matcher.cpp:493-501 / :518-536 / :540-545 / :555 / :580 / :21-27 | 真实，内容相符（50/1e-6 均为常数字面量） |
| frame_photometry_fit.cpp:166-174 | 锚真实，但内容是**条件**钳位（fov<=0 或 fov>=30 才钳 [1,10]）——05 若声称"现行即无条件钳位"则为内容不符（规格-实现差，非伪造锚） |
| pc_api.cpp:978-1008（阶梯） | 真实；pc_api.cpp:290/:314 注释宣称 "2000-10000" 范围，10000 上界**从未实现**（01/C5 成立） |
| PHOTOMETRY.md:126 | **锚漂移（幻觉）**：该行是 49 帧 M42 的 σ_residual 判据参照；"载体是线性面亮度、星等不落盘"的正本在 **:13** |
| PHOTOMETRY.md §16.5/:400 | **锚漂移**：该行是"冻结门是求解前提不是准入判据"；"帧间独立"正本在 **:15-17** |

## UNRESOLVED 汇总

1. Beaton & Tukey 1974 原文未取回（仅历史出处引用，无数值断言挂靠）。
2. David & Nagaraja / Kenney & Keeping 教科书正文未在线核验（S11 数值断言由解析积分自证，不挂靠）。
3. Gaia DR3 文档 §20.12.4 的 343 点网格未独立一手核验（PARTIAL，见 L4）。
4. Lindegren 2021 亮端位置误差具体数字未核（S10 不依赖）。
5. Rousseeuw & Croux 1993 仅镜像命中、未读全文。
