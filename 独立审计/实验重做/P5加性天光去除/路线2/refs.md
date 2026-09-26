# P5 加性天光去除 · 文献核验记录（路线2 独立取证）

本件是本路线全部一手文献核验的原始台账：每条给出核验渠道、命中的元数据、与仓内引用的差异、以及未核到时的如实登记。
核验日期：2026-09-26（会话内实时取证）。核验渠道：Crossref API（DOI 元数据）、arXiv abs/PDF 直取、OUP/JSTOR 检索面。

## R1 Huber 1964（robust 位置估计原始框架；Huber 损失出处链）

- 仓内引用：docs/science/PHASE2_UPM.md §7/§14a——Huber, P. J. 1964, Ann. Math. Statist. **35**, 73-101, DOI 10.1214/aoms/1177703732。
- 核验渠道：Crossref api.crossref.org/works/10.1214/aoms/1177703732。
- 命中元数据：题名 *Robust Estimation of a Location Parameter*；作者 Huber, Peter J.；容器 *The Annals of Mathematical Statistics*；卷 35；页 73–101；出版 1964-03。**与仓内引用逐项一致。**
- 直取 Project Euclid 全文页被站点防护（Incapsula）拦截，未取得全文；δ=1.345 的具体取值表不在该文而在 Holland & Welsch 1977（R2）与 Huber & Ronchetti 2009（R7，书，未逐页）。
- 判定：**VERIFIED（元数据级）**。

## R2 Holland & Welsch 1977（IRLS 实现 + δ 取值表）

- 仓内引用：§14a——Holland & Welsch 1977, Comm. Statist. A6, 813, DOI 10.1080/03610927708827533。
- 核验渠道：Crossref api.crossref.org/works/10.1080/03610927708827533。
- 命中元数据：题名 *Robust regression using iteratively reweighted least-squares*；作者 Holland, Paul W.；Welsch, Roy E.；*Communications in Statistics - Theory and Methods* A6(9), 813–827, 1977。**与仓内引用一致（补出期号 9 与止页 827）。**
- 判定：**VERIFIED（元数据级）**。δ=1.345 的数值本身由本路线 E5 以闭式渐近效率方程独立复算（δ(0.95)=1.3449975，见 report M5），不依赖该表逐页。

## R3 Serfling 1980（Var(median) 渐近理论正本）

- 仓内引用：§5 注释——Serfling 1980 §2.3.2（ISBN 0-471-02403-1 / DOI 10.1002/9780470316481）。
- 核验渠道：Crossref api.crossref.org/works/10.1002/9780470316481。
- 命中元数据：*Approximation Theorems of Mathematical Statistics*，Serfling, Robert J.，Wiley，1980-11-24，monograph。**与仓内引用一致。**
- 未核到：§2.3.2 的页级内容（书章节，在线不可逐页）。√(π/2)=1.2533141 中位数渐近标准误系数由本路线 E1/E3 以 MC 独立复现（高斯 N=289 实测 ratio 1.5808，渐近 π/2=1.5708）。
- 判定：**VERIFIED（书目级）；章内页码 UNRESOLVED（在线不可得，不构成障碍：结论已由 MC 独立复现）。**

## R4 Andrae, Schulze-Hartung & Melchior 2010（自由度 dof = N − r_eff）

- 仓内引用：docs/science/PHASE2_UPM.md §7a 规则 4、docs/plugins/algorithms_phase2/11_upm.md §4.7——"Andrae, Schulze-Hartung & Melchior 2010, arXiv:1012.3754 式 (9)"。
- 核验渠道：arXiv abs 页与 PDF 全文直取。
- 命中：arXiv:1012.3754，题名 ***Dos and don'ts of reduced chi-squared***（提交 2010-12-16），作者 Rene Andrae, Tim Schulze-Hartung, Peter Melchior。
- 式 (9) 逐字核验（PDF 文本）：K = N − Peff ≥ N − P（其式 (8)：Peff = tr(H) = rank(X)）。**仓内把 dof 取 n_obs − r_eff 与式 (9) 的 K = N − Peff 一致（r_eff 即有效秩计数 Peff）。**
- 注意：若按 n_obs − n_params 取分母，在秩亏时系统性低估 χ²_red——E8 实测低估因子 1.018（n_obs=120, r_eff=7, n_params=9），与 Andrae 文的告诫方向一致。
- 判定：**VERIFIED（全文级，式 (9) 逐字）。**

## R5 Fruchter & Hook 2002（Drizzle 与输出噪声相关性）

- 用途：k_corr>1 的物理背景（drizzle 重采样使输出像素噪声相关、N_eff < N_retained）。
- 核验渠道：arXiv abs 直取 + web 检索（ADS 2002PASP..114..144F）。
- 命中：arXiv:astro-ph/9808087（v2 2001-10-19），题名 *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*，作者 A. S. Fruchter, R. N. Hook，摘要自述 "accepted for publication in February 2002 PASP"（= PASP 114, 144）。摘要明言讨论 "the noise characteristics of output images"。
- **注意：该文讨论了 drizzle 输出的噪声特性与相关性，但没有给出 "k_corr = 1.4（pixfrac=0.8）" 这一数值**；该数值是项目自产 MC（control_median_mc_test，声称 1.3883）。本路线 E2 的独立 drizzle MC 无法从已文档化的信息复现 1.3883（见 report M2 诚实边界）。
- 判定：**VERIFIED（文献存在与论旨）；k_corr=1.3883 的可复现性 UNRESOLVED。**

## R6 Clopper–Pearson 1934（二项比例区间，§17.2 经验虚警率的区间口径）

- 核验渠道：OUP/JSTOR 检索面（web_search）。
- 命中：Clopper, C. J. & Pearson, E. S. 1934, *The use of confidence or fiducial limits illustrated in the case of the binomial*, Biometrika **26**(4), 404（OUP: academic.oup.com/biomet/article/26/4/404/291538；JSTOR: jstor.org/stable/2331986）。
- 判定：**VERIFIED（元数据级）。** §17.2 的 "1/114 ⇒ 0.88%，95% 区间 [0.022%, 4.8%]" 的区间算法与该文一致（本路线未逐点复算该区间数值，列入边界）。

## R7 Huber & Ronchetti 2009（Robust Statistics 2nd ed., 效率表）

- 仓内引用：§14a（ISBN 978-0-470-12990-6 / DOI 10.1002/9780470434697）。
- 未核到：书页级在线不可得；本路线未 fetch。δ=1.345 的 95% 效率已由 R1+R2 出处链与本路线 E5 闭式复算独立支撑，该书仅定位级引用。
- 判定：**UNVERIFIED-DETAIL（定位级接受）。**

## R8 Duchon 1977 / Wahba 1990（薄板样条与样条正则化）

- 仓内引用：§14a（稀疏天光面表示候选）。
- 未 fetch（教科书/会议文集级，在线不可逐页）。本路线 M7 的正则化强度实验不依赖该书具体公式，仅用二阶差分弯曲能惩罚的通用形式。
- 判定：**UNVERIFIED-DETAIL（定位级接受）。**

## R9 Padmanabhan et al. 2008（SDSS 重叠观测联合相对定标；链条上下游对照）

- 仓内引用：§14a（多帧相对光度/gauge/连通性）。
- 本路线未独立 fetch（取证预算让位于本模块核心数值链）；其在本报告中仅作链条背景对照，不承载任何本路线数值结论。
- 判定：**NOT-VERIFIED（未取证，仅背景引用，不作证据使用）。**

## R10 Φ⁻¹(3/4) 与 MAD 一致性常数 1.4826

- 理论恒等式：MAD 一致性常数 = 1/Φ⁻¹(3/4) = 1/0.674490 = 1.482602…。由标准正态分位函数直接计算（E1/E3 代码内使用），无需外部文献。
- 判定：**VERIFIED（解析恒等式，代码内可复算）。**

## 汇总表

| # | 文献 | 渠道 | 核验级 | 与仓内引用差异 |
|---|---|---|---|---|
| R1 | Huber 1964, AnnMS 35,73 | Crossref | 元数据级 | 无 |
| R2 | Holland & Welsch 1977, CSTM A6(9),813 | Crossref | 元数据级 | 无（补全期号/止页） |
| R3 | Serfling 1980 (Wiley) | Crossref | 书目级 | 无；章内页码未核 |
| R4 | Andrae+ 2010, arXiv:1012.3754 | arXiv 全文 | 全文级（式9逐字） | 无 |
| R5 | Fruchter & Hook 2002, PASP 114,144 | arXiv/ADS | 元数据级 | 无；**1.3883 非该文内容** |
| R6 | Clopper & Pearson 1934, Biometrika 26,404 | OUP/JSTOR | 元数据级 | 无 |
| R7 | Huber & Ronchetti 2009 | — | 定位级 | 未逐页 |
| R8 | Duchon 1977 / Wahba 1990 | — | 定位级 | 未逐页 |
| R9 | Padmanabhan+ 2008 | — | 未取证 | 仅背景 |
| R10 | Φ⁻¹(3/4) 恒等式 | 解析 | 恒等式 | — |

