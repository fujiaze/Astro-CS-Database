# refs.md — P5 加性天光去除 · 路线1 文献核验记录

核验人：路线1 独立执行者。核验手段：arXiv abs/PDF 直取、Crossref DOI 注册库、Project Euclid 元数据、
ADS PDF、出版方样张目录、archive.org OCR、Wikipedia/软件文档（标注二手）。出版方反爬（T&F/Wiley/Euclid-PDF）
处一律如实标注，不编造页码。状态：VERIFIED（一手题录+关键内容）/ VERIFIED-SECONDARY（二手来源核验）/
PARTIAL / UNRESOLVED。

---

## L1. 中位数渐近方差（C1/C2 的理论腿）

- **[VERIFIED]** Kendall, M. G., *The Advanced Theory of Statistics*, Vol. 1（Griffin）。§9.10 附近 Example 9.7
  一手 OCR 核得：var(median) = 1/(4n·f1^2)；正态父体 f1 = 0.39894/sigma 意味着 SE = 1.2533*sigma/sqrt(n) = sqrt(pi/2)*sigma/sqrt(n)，
  即 **Var(median) = pi*sigma^2/(2N)**。
  来源：https://archive.org/details/in.ernet.dli.2015.57860 （1948 印次，署名 Kendall 单人；引 "Kendall & Stuart"
  后续版次需另行确认章节号）。
- **[VERIFIED-SECONDARY]** Wikipedia "Median" 第 7.2 节："for large samples the variance of the median equals
  (pi/2)*(sigma^2/n)"。https://en.wikipedia.org/wiki/Median
- **[VERIFIED]** Serfling, R. J. 1980, *Approximation Theorems of Mathematical Statistics*, Wiley。
  Crossref：DOI 10.1002/9780470316481，ISBN 9780471024033（=0-471-02403-1），1980-11-24。
  https://api.crossref.org/works/10.1002/9780470316481
  **[UNRESOLVED]** "2.3.2 节" 小节标题：出版方目录显示 2.3 节 = "The Sample Quantiles"（pp.74-87），U 统计量在第 5 章；
  引用应写 **Serfling 1980, 2.3 节（Sample Quantiles）**，不应写 2.3.2。
- **[UNRESOLVED]** Hoaglin, Mosteller & Tukey 1983, *Understanding Robust and Exploratory Data Analysis*：
  书目存在（Google Books），全书借阅受限，pi*sigma^2/(2N) 在其中的存在性未核到。
- **[UNRESOLVED]** NIST/SEMATECH e-Handbook eda351 页只定义中位数，无该公式。
- **[VERIFIED]** Clopper, C. J. & Pearson, E. S. 1934, "The use of confidence or fiducial limits illustrated in
  the case of the binomial", Biometrika 26(4), 404-413, DOI 10.1093/biomet/26.4.404（C3 虚警率区间口径备用）。

## L2. Drizzle 与相关噪声（C2 的文献腿）

- **[VERIFIED 一手 PDF 全文]** Fruchter, A. S. & Hook, R. N. 2002, "Drizzle: A Method for the Linear
  Reconstruction of Undersampled Images", **PASP 114, 144-152**, DOI 10.1086/338393；arXiv:astro-ph/9808087。
  第 7 节关键原文（全文摘录）：
  - "Drizzle frequently divides the power from a given input pixel between several output pixels. As a result,
    **the noise in adjacent pixels will be correlated**."
  - 逐像素方差求和丢掉全部交叉项（"These terms ... represent the correlated noise ... can be significant"）；
    逐像素口径**低估大尺度噪声**。
  - 相关噪声比 R = sigma_c/sigma_p；典型 **p=0.6, s=0.5 时 R = 1.662**（方差校正约等于 R^2）；
    **p 趋于 0 时无相关噪声**（"There is then no correlated noise in the output image"）。
  https://arxiv.org/abs/astro-ph/9808087 ；https://doi.org/10.1086/338393
- **[VERIFIED]** Casertano, S. et al. 2000, "WFPC2 Observations of the Hubble Deep Field-South", AJ 120, 2747,
  arXiv:astro-ph/0010245, DOI 10.1086/316851（drizzle 噪声性质讨论的先声；F&H 2002 正式引用）。
- **[PARTIAL]** Hoffmann, S. L. et al. 2021, *The DrizzlePac Handbook*, v2.0, STScI。
  https://hst-docs.stsci.edu/drizzlepac （正文 JS 渲染，方差校正原文未摘到；定量依据引 F&H 2002 第 7 节最稳）。
- 与本模块常数的关系：PHASE2_UPM.md 第 4 节的 k_corr 即"相关样本方差放大因子"口径，与 F&H 的 R^2 同族
  （R 为噪声比、R^2 为方差比）；本路线 C2 实验独立复现 k_corr > 1 的存在性、pixfrac/尺度比单调性与
  k_corr=1 的低估方向（见 report C2）。量值依赖 fixture 几何，不构成对 1.3883 的复现声明。

## L3. 稳健统计与 Huber（C5 的文献腿）

- **[VERIFIED 题录]** Huber, P. J. 1964, "Robust Estimation of a Location Parameter",
  Ann. Math. Statist. 35(1), 73-101, DOI 10.1214/aoms/1177703732（Crossref + Project Euclid 元数据逐项吻合）。
  **[UNRESOLVED/否定倾向]** "delta=1.345 对应 95% 效率"的**通行出处不是本文**：Euclid 页面与元数据无 1.345；
  所有可核验独立二手来源均把 1.345 归于 Holland & Welsch (1977)。引用时不得把 1.345 归到 Huber 1964 名下。
  https://projecteuclid.org/journals/annals-of-mathematical-statistics/volume-35/issue-1/Robust-Estimation-of-a-Location-Parameter/10.1214/aoms/1177703732.abs
- **[VERIFIED 题录 / UNRESOLVED 原文表]** Holland, P. W. & Welsch, R. E. 1977, "Robust regression using
  iteratively reweighted least-squares", Comm. Statist.-Theory Methods A6(9), 813-827,
  DOI 10.1080/03610927708827533。T&F 页面 403，取值表原文 UNRESOLVED；二手归属强：
  - Meer 1991 (J. Math. Imaging Vision)："To achieve 95% asymptotic efficiency for Gaussian noise Holland and
    Welsch (1977) recommended C_H = 1.345." https://sites.rutgers.edu/peter-meer/wp-content/uploads/sites/69/2018/12/meerrob91.pdf
  - R MASS::rlm 默认 psi.huber k=1.345：https://stat.ethz.ch/R-manual/R-devel/library/MASS/html/rlm.html
  - statsmodels HuberT 默认 t=1.345：https://www.statsmodels.org/stable/generated/statsmodels.robust.norms.HuberT.html
- **[VERIFIED 题录]** Huber, P. J. & Ronchetti, E. M. 2009, *Robust Statistics*, 2nd ed., Wiley,
  ISBN 978-0-470-12990-6, DOI 10.1002/9780470434697（Crossref monograph；书内 2.5c 节原文页码 UNRESOLVED）。
- **[VERIFIED-SECONDARY]** MAD 一致性常数 1.4826 = 1/Phi^-1(3/4)：
  https://en.wikipedia.org/wiki/Median_absolute_deviation （二手；数值复算 1/Phi^-1(0.75)=1.4826022185）。
- 本路线 C5 用 MC 独立测得 delta=1.345 时效率 0.948（高斯），与 95% 口径一致（见 report C5）。

## L4. 自由度与可辨识性（C6 的文献腿）

- **[VERIFIED 一手 PDF 全文]** Andrae, R., Schulze-Hartung, T. & Melchior, P. 2010, "Dos and don'ts of reduced
  chi-squared", arXiv:1012.3754。式 (8) P_eff = tr(H) = rank(X)；**式 (9) K = N - P_eff = N - rank(X)**；
  正文明确"标准说法 K = N - P 未必正确"，秩亏时应取 N - rank（Example 1：theta1+theta2 模型 dof = N-1）。
  https://arxiv.org/abs/1012.3754 ；https://arxiv.org/pdf/1012.3754
- 正本对接：docs/plugins/algorithms_phase2/11_upm.md 4.7 节的 dof_eff = n_obs - r_eff 与
  docs/science/PHASE2_UPM.md 7a 节规则 4/5（唯一判决 tau=rank_rtol、列均衡 H_eq）与 Andrae 式 (9) 同构。
  C6 数值检验：E[chi2] = n_obs - r_eff 成立（119.76 vs 120）；n_obs-n_params 分母**高估** chi2_red 1.0714 倍
  （与 11_upm 4.7 节"系统性低估"的方向表述相反，见 report 相左之处 #2）。

## L5. 多帧联合相对定标 / 背景对齐先例（链条位置与背景引用）

- **[VERIFIED，引用号订正]** Padmanabhan, N., Schlegel, D. J., Finkbeiner, D. P., et al. 2008, "An Improved
  Photometric Calibration of the Sloan Digital Sky Survey Imaging Data", ApJ 674, 1217-1233,
  **arXiv:astro-ph/0703454**, DOI 10.1086/524677。内容确为 SDSS 重叠观测联合相对定标 "ubercalibration"
  （griz 约 1% / 8500 deg^2）。**注意：arXiv:0805.2366 实为 Ivezic et al. 的 LSST 综述**，不得混用。
  https://arxiv.org/abs/astro-ph/0703454
- **[VERIFIED，主题需更正]** Gruen, D., Seitz, S. & Bernstein, G. M. 2014, "Implementation of Robust Image
  Artifact Removal in SWarp through Clipped Mean Stacking", PASP 126, 158-169, DOI 10.1086/675080,
  arXiv:1401.4169。主题是**多帧叠加稳健伪迹剔除**，不是背景匹配/背景定标；引用时不得作背景扣除依据。
- **[VERIFIED]** Bertin, E. 2006, "Automatic Astrometric and Photometric Calibration with SCAMP",
  ASP Conf. Ser. 351, p. 112（ADASS XV；ADS PDF 一手核验；未上 arXiv）。
  https://articles.adsabs.harvard.edu/pdf/2006ASPC..351..112B
- **[UNRESOLVED]** "Borlina et al." 的 drizzle 相关噪声论文：检索零命中，不引用（与仓库 02 件 6.4 节的
  UNPROVEN 登记一致）。

## L6. 正本数值的仓库内权威位（实验腿引用的式子编号）

| 正本条目 | 位点 |
|---|---|
| control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained | docs/science/PHASE2_UPM.md 第 2 节符号表（行 20）、第 5 节 |
| N_retained 在 [min_samples, 289]，min_samples=5 | PHASE2_UPM.md 行 87；lib/algorithms/coverage/include/astro/phase2/sampler.h 行 37 |
| k_corr 定义域 1<k_corr、冻结默认 1.4（MC 1.3883） | PHASE2_UPM.md 第 4 节（行 24）、第 5 节 |
| w_UPM 两级权重三性质 (i)(ii)(iii) | PHASE2_UPM.md 第 5 节（SCI-UPM-WEIGHT-001） |
| Huber 对称性、delta=1.345、sigma_eff=max(|unc|,1e-3) | PHASE2_UPM.md 第 7 节（行 95）；upm.cpp 行 273/279 |
| 唯一判决 tau=rank_rtol、列均衡 H_eq、dof_eff=n_obs-r_eff | PHASE2_UPM.md 7a 节规则 4/5；identifiability.h 行 10-37 |
| 跨 worker rtol=1e-12、scale_obs=5.26e13 | PHASE2_UPM.md 7a 节规则 6（行 100/106） |
| 接缝判据 rel_step(e)、门 1e-2、确定性下限 1.0050%、sigma=1.187e-3、多重性表、1.73% 漏检面 | PHASE2_UPM.md 9a 节（行 295）、17.1-17.4 节（行 387-495） |
| 方差比对电平阶跃原理性失明 | 11_upm.md 4.1 节（行 42）、PHASE2_UPM.md 9a 节 |
| B_ref 表示边界（约 0.80xRMS、Pearson 0.8964、2h 尺度） | PHASE2_UPM.md 16.2 节 |
