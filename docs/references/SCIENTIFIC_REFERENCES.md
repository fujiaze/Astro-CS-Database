# Astro Celestial Sphere Database（ACSD） 科学与格式参考文献档案

> 上游：ASTROCS_DESIGN.md 附录 B（外部标准与文献）

规则：项目采用的外部科学/格式依据统一在此登记；具体 SCI/设计文档仍须把引用落实到对应 SCI/ALG 条目。外部资料用于支持或约束推导，不能替代项目明确的单位、适用域和验收。

## A. 探测器校准与噪声

1. Newberry, M. V. 1991, “Signal-to-Noise Considerations for Sky-Subtracted CCD Data”, PASP 103, 122. DOI: 10.1086/132801。用途：CCD 信噪、sky/read/bias/dark 项。
2. Janesick, J. R. 2001, *Scientific Charge-Coupled Devices*, SPIE PM83, ISBN 0-8194-3698-4。用途：gain、read noise、photon transfer。
3. [HST ACS Data Handbook §4.4 Flat-Field Reference Files](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-4-flat-field-reference-files)。用途：flat-field 像素响应与低频校正边界。

## B. PSF、最优提取、SNR 与图像权重

4. Horne, K. 1986, “An Optimal Extraction Algorithm for CCD Spectroscopy”, PASP 98, 609. DOI: 10.1086/131801。用途：已知 profile 和方差下的最优提取；ACSD 借其统计结构定义 `PᵀC⁻¹P`，成像 PSF 需独立验证。
5. Naylor, T. 1998, “An optimal extraction algorithm for imaging photometry”, MNRAS 296, 339. [全文](https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf)。用途：成像最优 PSF 光度。
6. Zackay, B. & Ofek, E. O. 2017, “How to coadd images? I. Optimal source detection and photometry using ensembles of images”, ApJ 836, 187. [arXiv:1512.06872](https://arxiv.org/abs/1512.06872)。用途：每帧按自身 PSF matched filter 后组合；普通先叠加后滤波/PSF homogenization 会损失灵敏度。
7. Zackay, B. & Ofek, E. O. 2017, “How to coadd images? II. A coaddition image that is optimal for any purpose in the background-dominated noise limit”, ApJ 836, 188. [arXiv:1512.06879](https://arxiv.org/abs/1512.06879)。用途：proper coadd 与信息保持表示。
8. [PixInsight Reference: New Image Weighting Algorithms](https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html)。用途：§2.5 PSF Signal Weight、§2.6 PSF SNR、signal concentration 和工程加权语义。现行规定：ACSD 正式支持 `psfsw_robust` conventional-integration 模式；其 PSF signal/concentration/noise/background 四分量和组内归一须可审计，但无量纲复合权重不自动等同严格 `1/Var(F_hat)`。

## C. Drizzle、HEALPix 与 HiPS

9. Fruchter, A. S. & Hook, R. N. 2002, “Drizzle: A Method for the Linear Reconstruction of Undersampled Images”, PASP 114, 144. [ADS](https://ui.adsabs.harvard.edu/abs/2002PASP..114..144F/abstract)。用途：drop、pixfrac、线性重建、相关噪声。
10. [DrizzlePac Handbook](https://www.stsci.edu/files/live/sites/www/files/home/scientific-community/software/drizzlepac/_documents/drizzlepac-handbook-v1.pdf)。用途：Drizzle 实践、pixfrac 与权重/相关噪声。
11. Górski, K. M. et al. 2005, “HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere”, ApJ 622, 759. [ADS](https://ui.adsabs.harvard.edu/abs/2005ApJ...622..759G/abstract)。用途：HEALPix geometry/order。
12. Fernique, P. et al. 2015, “Hierarchical progressive surveys”, A&A 578, A114. [全文](https://www.aanda.org/articles/aa/full_html/2015/06/aa26075-15/aa26075-15.html)。用途：HiPS 层级产品。
13. [IVOA HiPS 1.0 Recommendation](https://www.ivoa.net/documents/HiPS/)。用途：HiPS properties、tile、目录与互操作。

## D. 天体测量、WCS 与 FITS

14. Greisen, E. W. & Calabretta, M. R. 2002, “Representations of world coordinates in FITS”, A&A 395, 1061. [全文](https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.html)。用途：FITS WCS 框架和关键字。
15. Calabretta, M. R. & Greisen, E. W. 2002, “Representations of celestial coordinates in FITS”, A&A 395, 1077. [全文](https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html)。用途：球面投影 TAN/SIN/CAR/AIT 等。
16. [IAU FITS Standard / FITS Working Group](https://fits.gsfc.nasa.gov/iaufwg/)。用途：FITS HDU、关键字、checksum 和互操作。

## E. 全局相对定标、马赛克与排异

17. Padmanabhan, N. et al. 2008, “An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data”, ApJ 674, 1217. [ADS](http://ui.adsabs.harvard.edu/abs/2008ApJ...674.1217P/abstract)。用途：重叠观测联合相对光度标定、gauge/连通性。
18. Bertin, E. 2010, “SCAMP: automatic astrometric and photometric calibration”, ASP Conf. Ser. 442, 435. [ADS](https://ui.adsabs.harvard.edu/abs/2010ASPC..442..435B/abstract)。用途：多帧天体/光度联合校准实践。**勘误**：SCAMP 论文的正确定位为 **Bertin, E. 2006, ASP Conf. Ser. 351, 112, “Automatic Astrometric and Photometric Calibration with SCAMP”**（<http://aspbooks.org/custom/publications/paper/351-0112.html>）；**ASPC 442, 435 为 Bertin, E. 2011, “Automated Morphometry with SExtractor and PSFEx”**（PSFEx 论文，<http://aspbooks.org/custom/publications/paper/442-0435.html>）。ASPC 442, 435 的 bibcode 归属 = Bertin, E. 2011（PSFEx 论文）；SCAMP 引用用 Bertin 2006, ASPC 351, 112。
19. Gruen, D., Seitz, S. & Bernstein, G. M. 2014, “Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking”, PASP 126, 158. [ADS](https://ui.adsabs.harvard.edu/abs/2014PASP..126..158G/abstract)。用途：叠加排异与 PSF 差异下的伪影控制。
20. Mosteller, F. & Tukey, J. W. 1977, *Data Analysis and Regression*. 用途：robust biweight；常数与效率必须由专项 SCI 精确定位。

## F. 项目内来源档案

- `设计大纲/大报告_项目历史.md`：实现/口径演进证据，不是当前科学权威。
- `设计大纲/大报告_历代控制包.md`：需求和治理演进证据。
- `reports/review-package-20260915/`：V3 后问题、推导和缺陷账本。
- `run/perf-fix/P5-snr/`：P5 SNR 实验、Oracle 和报告留档；结论须按新统一科学模型重新解释。

## H. 测光专项参考档案（既有 B1–B90 调查）

项目既有测光专项调查已逐条登记 B1–B90 项书目、DOI/URL、定量结论与更正记录。为避免在多个活动文件复制后漂移，该原始档案保留于：

- 唯一来源 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md`，§“来源与引用” [B1]–[B90]。

这些专项资料被本总档案整体纳入，重点包括：Bessell & Murphy 2012（光子/能量通带与零点）、Gaia EDR3/DR3 测光与 XP 外部定标、CALSPEC、Stetson/Anderson & King/Dolphin/Naylor 的 PSF/拥挤/欠采样测光、Howell/Newberry 的 CCD SNR、Fruchter & Hook/Zackay 等的相关噪声，以及 Pan-STARRS/HSC/LSST 的深度定义。

核验标签沿用原档案：`[V]` 为逐字或主来源核验，`[S]` 为次级/摘要定位，`[U]` 为未核实。实施前引用具体数字时必须回到原条目，总档案的“纳入”只登记范围，逐式复核以原条目为准。完整原文见 tracked 快照；最终采用的引用须落到对应 SCI/ALG，并保留 DOI/bibcode。

## G. 引用纪律

- 新 SCI 条目必须给作者/年份/稳定 URL 或 DOI、具体节/式及项目推导差异；
- 文献没有定义项目字段单位、schema 和失败语义，这些必须由项目明确；
- 经验质量指标与 Fisher information/逆方差各自独立定义，等同须另有依据；
- 参考实现不可作为唯一 Oracle；至少一个解析/独立数值实现；
- 链接失效时保留 DOI/bibcode 和访问日期，不删除引用历史。

## I. PSF、星点检测与最优提取

21. Moffat, A. F. J. 1969, “A Theoretical Investigation of Focal Stellar Images in the Photographic Emulsion”, A&A 3, 455（bibcode 1969A&A.....3..455M）。用途：Moffat 轮廓 I(r)∝(1+r²/α²)^(−β) 及 β 族；ACSD 取 β=4。**核验状态**：文章级定位（bibcode/卷页经多篇文献引用交叉核对），未逐式核验公式号。
22. Stetson, P. B. 1987, “DAOPHOT: A Computer Program for Crowded-Field Stellar Photometry”, PASP 99, 191（DOI 10.1086/131977）。用途：拥挤场 PSF 拟合测光、迭代星表构建、质量代理语义的历史来源。
23. Stetson, P. B. 1990, “On the growth-curve method for calibrating stellar photometry with CCDs”, PASP 102, 932（DOI 10.1086/132719）。用途：孔径改正/增长曲线；ACSD 的解析 flux=2πA·sxsy/3 为整平面值，与此类孔径改正**不互通**（未建模项）。
24. Bertin, E. & Arnouts, S. 1996, “SExtractor: Software for source extraction”, A&AS 117, 393（DOI 10.1051/aas:1996164）。用途：背景网格 + 阈值检测、去混叠、FLUXERR/MAGERR 误差口径、FLUX_AUTO 等测光量的权威定义。
25. Bertin, E. 2011, “Automated Morphometry with SExtractor and PSFEx”, ASP Conf. Ser. 442, 435（<http://aspbooks.org/custom/publications/paper/442-0435.html>）。用途：PSFEx 的 PSF 采样/多项式空间变异建模；ACSD 现状不做空间变异 PSF（PSF.md §1 非目标）。
26. Levenberg, K. 1944, Quart. Appl. Math. 2, 164；Marquardt, D. W. 1963, SIAM J. Appl. Math. 11, 431；Moré, J. J. 1978, in Numerical Analysis (Lecture Notes in Mathematics 630), 105。用途：LM 阻尼最小二乘；ACSD lm_solve 收敛语义的理论来源。
27. Press, W. H. et al. 2007, Numerical Recipes 3rd ed., Ch.15（Levenberg–Marquardt）。用途：LM 实现实践的教科书级对照（非唯一 Oracle）。
28. Horne, K. 1986, PASP 98, 609（已在 §B 第 4 条登记）。用途：图像域最优提取的统计结构 PᵀC⁻¹P。
29. Naylor, T. 1998, MNRAS 296, 339（已在 §B 第 5 条登记）。用途：成像最优 PSF 光度。
30. Young, I. T. & van Vliet, L. J. 1995, “Recursive implementation of the Gaussian filter”, Signal Processing 44, 139。用途：检测侧平滑 GaussianBlur_YvV（IIR 递归高斯）的实现来源。**核验状态**：文章级（期刊/卷/页），未逐式核验。
31. Akima, H. 1970, “A New Method of Interpolation and Smooth Curve Fitting Based on Local Procedures”, J. ACM 17, 589（DOI 10.1145/321607.321609）。用途：spectrum_integrator.cpp 的 Akima 子样条（F_syn=∫F_λ·T·Q·λ dλ 数值积分基元）。

## J. 排异、鲁棒统计与叠加

32. Rosner, B. 1983, “Percentage Points for a Generalized ESD Many-Outlier Procedure”, Technometrics 25, 165（DOI 10.1080/00401706.1983.10487848）。用途：Generalized ESD 的 α/max_outliers 语义来源（REJECTION.md §14 第 1 条）。
33. NIST/SEMATECH, e-Handbook of Statistical Methods, §1.3.5.17 “Grubbs Test for Outliers” 与 §7.1.6（Generalized ESD 的可执行独立实现与临界值表）。用途：ESD/RCR 的独立 Oracle；项目 rejection_oracle_compare 的 NIST 数据集出处。
34. Maples, M. P., Reichart, D. E., Konz, N. C., et al. 2018, “Robust Chauvenet Outlier Rejection”, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d；arXiv:1807.05276）。用途：RCR（reject–clean–refine / Chauvenet 变体）的**论文出处**；项目现仅登记“官方 RCR 2.4.7 软件参考”，缺该论文引用。
35. Konz, N. & Reichart, D. E. 2023, “Robust Chauvenet Rejection: Powerful, but Easy to Use Outlier Detection for Heavily Contaminated Data Sets”, arXiv:2301.07838（期刊卷页**需网络核验**）。用途：RCR 的后续方法学与实现说明。
36. Hoaglin, D. C., Mosteller, F. & Tukey, J. W. (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。用途：winsorization 与稳健尺度的教科书级定义（REJECTION.md §14 第 2 条）。
37. Beaton, A. E. & Tukey, J. W. 1974, “The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data”, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）。用途：Tukey biweight（bisquare）w=(1−u²)² 与 c=4.685 的原始出处（现引 Mosteller & Tukey 1977 为教科书转引）。
38. Gruen, D., Seitz, S. & Bernstein, G. M. 2014, PASP 126, 158（已在 §E 第 19 条登记）。用途：clipped-mean 叠加排异与 PSF 差异伪影控制。
39. Siril 官方文档（free-astro/Siril，GPL-3.0，<https://gitlab.com/free-astro/siril>）的 stacking/rejection 章节。用途：**次生参考实现**（只用于掩码逐元素对拍；`REJECTION_ALGORITHMS.md` F2–F4 的 `*_SIRIL` 冻结标识）。**不是核语义来源**——语义来源见 `docs/science/REJECTION.md` §14a（linear_fit = 官方式[21]/[22] + NR 3rd ed. §15.7.3；winsorized = 官方式[18]/[19]）。
40. PixInsight WeightedBatchPreprocessing (WBPP)（<https://pixinsight.com/doc/scripts/WeightedBatchPreprocessing/WeightedBatchPreprocessing.html>）的 bestRejectionMethod 与 ImageIntegration 参考文档。用途：**档界与算法类型**的非学术软件来源。**可核验版本 = WBPP 2.5.9**：官方更新包 `https://pixinsight.com/update/1.8.9-1/20230203-script.zip`，sha1 `712cc7c3fdb523643ad0e685104592d511996f82`（`product-info.txt` 自述 2.5.9），`WeightedBatchPreprocessing-engine.js:1421-1429`（`bestRejectionMethod()`，`n = activeFrames().length`）：`n<6` percentile / `n≤15` 或 BIAS|DARK winsorized / 否则 **ESD**；`:1349-1412` 为 `rejectionIsGood()`。**仓内旧引文 `BPP-FrameGroup.js:1304-1312` 作废**（该文件名不存在于任何官方包；`n>15 → LinearFit` 在 1.4.2–2.5.9 任何版本均不成立）。WBPP ≥2.6 源码随商业安装分发、公开不可核验。
61. IRAF `combine`/`imcombine` 共用引擎（IRAF/NOAO 许可，非 OSI；<https://github.com/iraf-community/iraf>）的参数文件 `noao/imred/ccdred/combine.par`（@main）：`reject` 值域 = `none|minmax|ccdclip|crreject|sigclip|avsigclip|pclip`；`combine=average|median`、`nlow=1`/`nhigh=1`、`nkeep=1`、`mclip=yes`、`lsigma=3.`/`hsigma=3.`、`pclip=-0.5`、`sigscale=0.1`、`rdnoise=0.`/`gain=1.`/`snoise=0.`、`grow=0`、`lthreshold`/`hthreshold=INDEF`。用途：**方法族命名来源**（非核语义来源）。**该引擎没有 `lfitclip` 也没有 `winsorize` 参数**——旧引文中的这两个名字在 IRAF 中不存在，已删去；`imcombine` 任务本体属 `obsutil` 包、在公开社区仓中不可得，故以其**共用引擎**的参数文件为可核验基准。
62. ccdproc.combine（BSD-3-Clause，<https://ccdproc.readthedocs.io/>）：clip_extrema（=IRAF-like minmax）、sigma_clip_low/high_thresh、combine 的加权/裁剪语义。用途：IRAF 排异核的可执行独立实现对照。
63. Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, “Proper Image Subtraction: Optimal Transient Detection, Photometry, and Hypothesis Testing”, ApJ 830, 27（DOI 10.3847/0004-637X/830/1/27；arXiv:1601.02655）。用途：噪声加权最优检验/预测残差方差阈值（plugins/12_rejection.md:23 若引最优检验应锚此）。

## K. UPM、马赛克与背景模型

41. Bertin, E. 2006, ASP Conf. Ser. 351, 112（SCAMP，见 §E 第 18 条勘误）。用途：多帧**相对**天体/光度联合定标、重叠图与连通/gauge 处理；UPM 的 g_k·s+b_k(x) 目标模型属于同一问题族。
42. Bertin, E. et al. 2002, “The TERAPIX Pipeline”, ASP Conf. Ser. 281, 228（SWarp 实践论文）。用途：马赛克重采样、逐帧背景扣除与 coadd 权重；SWarp 源码为 GPL-3.0（LICENSE 逐字核验）。
43. Padmanabhan, N. et al. 2008, ApJ 674, 1217（已在 §E 第 17 条登记）。用途：SDSS 重叠观测联合相对光度定标的线性系统与 gauge。
44. Huber, P. J. 1964, “Robust Estimation of a Location Parameter”, Ann. Math. Statist. 35, 73（DOI 10.1214/aoms/1177703732）。用途：Huber M 估计与 δ=1.345（Gaussian 95% 渐近效率）的来源。
45. Huber, P. J. & Ronchetti, E. M. 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。用途：M 估计/IRLS 收敛与效率常数的权威教科书定位。
46. Tikhonov, A. N. 1963, “Solution of Incorrectly Formulated Problems and the Regularization Method”, Soviet Math. Dokl. 4, 1035。用途：弱零锚（弱 Tikhonov/岭正则）的原始概念出处（**核验状态**：文章级，卷页需网络核验）。
47. Duchon, J. 1977, “Splines minimizing rotation-invariant semi-norms in Sobolev spaces”, in Constructive Theory of Functions of Several Variables, 85。用途：薄板样条/平滑样条基；PHASE2_UPM.md 的“稀疏天光面（B-spline/薄板样条）”目标表示的方法学出处。
48. Wahba, G. 1990, Spline Models for Observational Data, SIAM（ISBN 0-89871-244-0）。用途：样条粗糙度惩罚/节点选择的教科书级定位。
49. Bertin, E. 2011, ASPC 442, 435（PSFEx，见 §I 第 25 条）。用途：空间变异 PSF 与背景/星点采样；与 ACSD 现状（块状共享 PSF、8×8 control cell）差异对照。
64. Holland, P. W. & Welsch, R. E. 1977, “Robust Regression Using Iteratively Reweighted Least-Squares”, Communications in Statistics A6, 813（DOI 10.1080/03610927708827533）。用途：Huber δ=1.345（Gaussian 95% 渐近效率）与 IRLS 权重实现出处。
65. Kendall, M. G. & Stuart, A., The Advanced Theory of Statistics, Vol.1（Distribution Theory）。用途：正态样本中位数渐近方差 Var(median)≈πσ²/(2N) 的教科书定位（UPM control_variance 的 π/2 因子）。

## L. 数值、投影几何与可复现

50. IEEE 754-2019, IEEE Standard for Floating-Point Arithmetic。用途：FP32/FP64 舍入、归约非结合与 1/N worker 容差的判据基础（ACR_EQUIVALENCE.md §7/§9）。
51. Goldberg, D. 1991, “What Every Computer Scientist Should Know About Floating-Point Arithmetic”, ACM Computing Surveys 23, 5（DOI 10.1145/103162.103163）。用途：浮点归约/结合律与容差设定。
52. Higham, N. J. 2002, Accuracy and Stability of Numerical Algorithms, 2nd ed., SIAM（ISBN 0-89871-521-0）。用途：归约误差界、确定性求和顺序。
53. Van Oosterom, A. & Strackee, J. 1983, “The Solid Angle of a Plane Triangle”, IEEE Trans. Biomed. Eng. 30, 125（DOI 10.1109/TBME.1983.325207）。用途：Girard 定理/球面三角面积（DRIZZLE.md §5 Sutherland–Hodgman + Girard）。
54. Sutherland, I. E. & Hodgman, G. W. 1974, “Reentrant Polygon Clipping”, Comm. ACM 17, 32（DOI 10.1145/360767.360802）。用途：球面多边形裁剪的平面算法原型（ACSD 在球面上实施，属 Project-defined 迁移）。
55. Calabretta, M. R. & Greisen, E. W. 2002, A&A 395, 1077（Paper II，已在 §D 第 15 条登记）§2.1/§2.2/Table 1。用途：TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA 的 native↔celestial 旋转、LONPOLE 默认、各投影 native 层；ACSD 八投影冻结集合逐式出自此。
56. Greisen, E. W. & Calabretta, M. R. 2002, A&A 395, 1061（Paper I，已在 §D 第 14 条登记）§2.1.1。用途：CRPIX/CRVAL/CD/1-based 像素定义性不变量。
57. Shupe, D. L. et al. 2005, “The SIP Convention for Representing Distortion in FITS Image Headers”, ASP Conf. Ser. 347, 491（bibcode 2005ASPC..347..491S）。用途：SIP A/B/AP/BP 约定（**核验状态**：bibcode 级）。
58. IVOA HiPS 1.0 Recommendation（<https://www.ivoa.net/documents/HiPS/>，已在 §C 第 13 条登记）。用途：HiPS properties/tile/层级与互操作；ACSD 支持子集为 ICRS/NESTED/W=512/float FITS。
59. IVOA MOC 1.0 Recommendation（<https://www.ivoa.net/documents/MOC/>）。用途：HiPS↔MOC 关系与 coverage 域表达（PHASE3_HIPS_TO_FITS.md §14 第 1 条）。
60. Fernique, P. et al. 2015, A&A 578, A114（已在 §C 第 12 条登记；DOI 10.1051/0004-6361/201526075）。用途：HiPS 层级索引与目录结构。
66. Van Oosterom, A. & Strackee, J. 1983, “The Solid Angle of a Plane Triangle”, IEEE Trans. Biomed. Eng. BME-30(2), 125–126（DOI 10.1109/TBME.1983.325207）。用途：球面多边形面积（S-H 裁剪后的扇形三角剖分；单位向量下 Ω = 2·atan2(a·(b×c), 1+a·b+b·c+c·a)）；ACSD DRIZZLE.md §5 误记为“Girard 定理”，实现实为 Van Oosterom 扇形剖分。**核验状态**：Crossref 已核验（题名/作者/卷期页/DOI/被引 269 次）。代码内旧引 “Eriksson, F. 2018, The area of a spherical triangle” 经 Crossref 书目检索与 arXiv 检索均无此文献，已订正。
67. Starck, J.-L. & Murtagh, F. 1998, “Automatic Noise Estimation from the Multiresolution Support”, PASP 110, 193（DOI 10.1086/316124）。用途：PixInsight MRS/N* 稳健噪声估计（ImageWeighting §2.4）的一手论文；NOISE_MODEL 现状未采用小波 MRS/N*。

## M. 参考代码库（含许可证）

> 许可证均以仓库内 LICENSE/COPYING 原文或托管 API 的 SPDX 标识为准；[V] = 逐字/接口核验，[U] = 需网络核验。
> **GPL/共版许可代码仅作理解与数值行为对照，本仓代码面只含自研实现**（控制包任务边界）。

- Astropy — **BSD-3-Clause** [V]（https://github.com/astropy/astropy）。对照面：WCS/投影（astropy/wcs）、统计、单位。SCI-WCS/SCI-P3/数据口径。
- photutils — **BSD-3-Clause** [V]（https://github.com/astropy/photutils）。对照面：DAOStarFinder/IRAFStarFinder、MoffatPSF/GaussianPSF、孔径与 PSF 测光、背景估计。SCI-PSF/SCI-PHOT/SCI-P1-STAR。
- astropy-healpix — **BSD-3-Clause** [V]（https://github.com/astropy/astropy-healpix）。对照面：ang2pix/pix2ang NESTED、层级。SCI-DRZ/SCI-P3。
- healpy / HEALPix — **GPL-2.0** [V]（https://github.com/healpy/healpy）。对照面：Górski et al. 2005 参考实现的球面几何与 ang2pix。SCI-DRZ/SCI-P3（GPL：只对照不复制）。
- HEALPix C++（Healpix_3.83） — **GPL-2.0** [U]（https://sourceforge.net/projects/healpix/）。对照面：src/cxx/healpix_base.cc；本仓 healpix_core.cpp:338-340 自述移植自 Healpix_3.83。
- CDS Hipsgen / Aladin — **GPL-3.0** [U]（https://github.com/cds-astro/）。对照面：MAPTILES/properties 生成器（tile 内 FITS 序、properties 键值）；HiPS 互操作基准。
- DrizzlePac（drizzlepac） — **BSD-3-Clause** [V]（https://github.com/spacetelescope/drizzlepac）。对照面：drizzle/astrodrizzle 的 pixfrac、drop、权重与相关噪声。SCI-DRZ。
- SWarp — **GPL-3.0** [V]（https://github.com/astromatic/swarp）。对照面：重采样核、逐帧背景扣除、coadd 权重与 clipped-mean 排异。SCI-PHASE2_UPM/SCI-INT。
- SExtractor（sextractor/source-extractor） — **GPL-3.0** [V]（https://github.com/astromatic/sextractor）。对照面：背景网格、检测/去混叠阈值、FLUXERR/MAGERR、FLUX_AUTO。SCI-NOISE/SCI-PHOT/SCI-P1-STAR。
- PSFEx — **GPL-3.0** [V]（https://github.com/astromatic/psfex）。对照面：PSF 采样与空间变异多项式基。SCI-PSF。
- SCAMP — **GPL-3.0** [V]（https://github.com/astromatic/scamp）。对照面：多帧相对天体/光度联合定标、gauge/连通分量。SCI-PHASE2_UPM/SCI-WCS。
- ccdproc — **BSD-3-Clause** [V]（https://github.com/astropy/ccdproc）。对照面：subtract_bias/subtract_dark(scale=True)/flat_correct 与母版约定。SCI-CAL。
- LSST Science Pipelines lsst.ip.isr — **GPL-3.0** [V]（https://github.com/lsst/ip_isr）。对照面：ISR 顺序（bias→dark×K→flat）、饱和掩膜/SAT 面。SCI-CAL/SCI-NOISE（GPL：只对照不复制）。
- Siril — **GPL-3.0** [V]（https://gitlab.com/free-astro/siril，GitLab API license.key=gpl-3.0）。对照面：winsorized/averaged sigma、linear-fit 排异与叠加。SCI-REJ。
- WCSLIB — **LGPL-3.0** [V*]（官方 https://www.atnf.csiro.au/people/mcalabre/WCS/ ；GitHub 镜像 Punzo/wcslib 的 SPDX = LGPL-3.0）。对照面：Paper I/II 的可执行标准、投影实现。SCI-WCS/SCI-P3。（*镜像核验，官方下载页未逐字取原文。）
- CFITSIO — **CFITSIO Software License（类 MIT/宽松，NASA/HEASARC）** [U]（https://heasarc.gsfc.nasa.gov/fitsio/）。对照面：FITS HDU/关键字/BSCALE/BZERO/checksum。SCI-CAL/SCI-P3。
- reproject（astropy） — **BSD-3-Clause** [V]（https://github.com/astropy/reproject）。对照面：WCS→WCS 重采样与方差传播。SCI-DRZ/SCI-P3 重采样。
- SEP（Source Extraction and Photometry） — **LGPL-3.0** [V]（https://github.com/kbarbary/sep；证据 `src/sep.h:9-10` 与 `licenses/LGPL_LICENSE.txt`，SCI-001-S1 复核）。对照面：SExtractor 算法的 C/Python 重实现（背景/检测/去混叠/孔径方差）。SCI-PHOT/SCI-P1-STAR。
- DAOPHOT / IRAF — **IRAF/NOAO 许可（非 OSI 开源）** [U]（https://iraf-community.github.io/）。对照面：拥挤场 PSF 测光（Stetson 1987）。仅文献/行为对照，**不引入代码**。
- PixInsight Class Library（PCL） — **PixInsight 自定义 source-available 许可（非 OSI）** [U]（https://gitlab.com/pixinsight/PCL）。对照面：XISF NormalizeSamples/MaxSampleValue（SCI-CAL §14 第 7 条）与 ImageWeighting 方法学（SCI-CW/PSFSW）。**只作声明制换算因子的取证来源，不复制**。
- 数值/统计基础：NumPy（BSD-3-Clause）、SciPy（BSD-3-Clause）——独立 Python Oracle（FP64 复算）；NIST/SEMATECH e-Handbook——ESD/RCR Oracle。

> 说明：许可证与仓库路径已核验；引用具体代码行号时应回到对应版本的不可变提交或发布 tag，并在报告中登记版本。

## N. 帧级 SNR / PSFSW / 逆方差叠加专项

### N.1 方法学一手来源（PixInsight，公开文档）

- **PixInsight Reference: New Image Weighting Algorithms**（Juan Conejero 等，Pleiades Astrophoto）。官网 <https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html>；**官方源文件**（含 LaTeX 原文）GitLab `Reference-Documentation/docs/ImageWeighting/`：`01-Introduction.pidoc`、`02-PSF_Flux_Weighting_Algorithms.pidoc`、`03-Implementation.pidoc`、`04-Examples.pidoc`、`05-Linear_Regression_Analysis.pidoc`（master）。用途：PSFSW 式[16]、PSFSNR 式[18]、标准 SNR 式[20]、PSF flux 式[7]、mean PSF flux 式[8]、`M*` 式[13]、`N*` 式[14][15]、FITS 关键字表（PSFFLX/PSFMFL/PSFMST/PSFNST/NOISE 等）。**ACSD 不照抄其标定常数**（见 §N.3）。
- **PCL 2.10.4** `pcl::PSFSignalEstimator`（Doxygen `PSFSignalEstimator.h`，2026 年发布）。用途：核对文章版与实现版常数差异（c1=5.326e-6、c3=1.316e-7 vs 文章 c1=8.0832e-6、c3=1.350e-7）；`NStar_MAD=2.48308·MAD`、`NStar_Sn=2.03636·Sn`。**只作方法学取证，不复制**（PCL 为 PixInsight 自定义 source-available 许可，非 OSI）。

### N.2 学术文献

61. Zackay, B. & Ofek, E. O. 2017, “How to COAAD Images. I. Optimal Source Detection and Photometry of Point Sources Using Ensembles of Images”, ApJ 836, 187（DOI 10.3847/1538-4357/836/2/187；arXiv:1512.06872）。用途：每帧按自身 PSF matched filter 后再加权求和才最优；PSF 均质化/先叠加后滤波损失灵敏度。
62. Zackay, B. & Ofek, E. O. 2017, “How to COAAD Images. II. A Coaddition Image that is Optimal for Any Purpose in the Background-dominated Noise Limit”, ApJ 836, 188（DOI 10.3847/1538-4357/836/2/188；arXiv:1512.06879）。用途：proper coaddition、方差归一与有效 PSF。
63. **（消歧）** Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, “Proper Image Subtraction—Optimal Transient Detection, Photometry, and Hypothesis Testing”, ApJ 830, 27（DOI 10.3847/0004-637X/830/1/27；arXiv:1601.02655）——这是 **ZOGY 图像相减**论文，**不是** “How to coadd images? I”。`docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §5.1 第 1 条曾把两者混引，引用时须拆开。
64. Starck, J.-L. & Murtagh, F. 1998, “Automatic Noise Estimation from the Multiresolution Support”, PASP 110, 193（DOI 10.1086/316124）。用途：MRS/starlet 小波稳健噪声（PixInsight 默认噪声估计的方法学来源）。
65. Rousseeuw, P. J. & Croux, C. 1993, “Alternatives to the Median Absolute Deviation”, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。用途：MAD 的 σ 一致化常数 1.4826 与 Sn/Qn 尺度估计；ACSD 用标准 MAD→σ，**未采用** PixInsight 的 2.48308/2.03636。
66. Moffat, A. F. J. 1969, A&A 3, 455（见 §I 第 21 条）。用途：Moffat 轮廓；ACSD 取 β=4，FWHM=1.230310·σ。
67. Stetson, P. B. 1987, PASP 99, 191（DOI 10.1086/131977，见 §I 第 22 条）。用途：拥挤场 PSF 测光。
68. Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（DOI 10.1051/aas:1996164，见 §I 第 24 条）。用途：背景网格、检测阈值、FLUXERR 误差传播。
69. Maples, M. P. et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d，见 §J 第 34 条）。用途：Robust Chauvenet 离群剔除（PixInsight PSF 通量剔除的方法学来源）。

### N.3 参考代码库（含许可证；GPL 只对照不复制）

- **Siril** — **GPL-3.0** [V]（https://gitlab.com/free-astro/siril）。对照面：帧级权重 `compute_noise_weights` `w=1/(pscale²·bgnoise²)`（`src/stacking/median_and_mean.c:1111-1135`）、wFWHM/星数权重、IKSS 稳健尺度、多项式/RBF 背景。与 ACSD 差异：Siril 把权重直接当叠加系数、按帧均值归一；ACSD 的 `W_psfsw` 为组内中值归一无量纲量。
- **SWarp** — **GPL-3.0** [V]（https://github.com/astromatic/swarp）。对照面：`COADD_WEIGHTED` 逆方差组合与输出方差 `1/Σ(1/var_k)`（`src/coadd.c:1279-1311`）、`RESCALE_WEIGHTS` 实测噪声重标定 `sigfac`（`src/back.c:361-389`）。
- **DeepSkyStacker（DSS）** — **BSD-3-Clause** [V]（https://github.com/deepskystacker/DSS）。**更正**：`docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §4 表把它标为 GPL v3 且 URL `github.com/DeepSkyStacker/DeepSkyStacker`（404）——实际为 BSD-3-Clause（LICENSE 全文 + `README.md:13`），仓库 `deepskystacker/DSS`。对照面：帧评分 `ComputeScore`（圆度加权，`RegisterEngine.cpp:86-118`）、自适应加权平均 `w=1/(1+(x−µ)²/σ²)`（`avx_output.cpp:463-575`）。其 quality 与 SNR/FWHM 乘积无关。
- **SExtractor** — **GPL-3.0** [V]（https://github.com/astromatic/sextractor）。对照面：背景网格/众数估计（`src/back.c:449-743`）、`Var(F)=Σ(σ_bkg²+F_pix/gain)`（`src/analyse.c:200-203,304-310`）。
- **SEP** — **LGPL-3.0** [V]（https://github.com/kbarbary/sep）。对照面：孔径方差 `σ²_sum=Σvar_pix·w²+Σ/gain`（`src/aperture.c:516-570`）、背景网格（`src/background.c:277-790`）。
- **photutils / astropy** — **BSD-3-Clause** [V]（https://github.com/astropy/photutils）。对照面：`Background2D` + `SExtractorBackground`（`photutils/background/core.py:464-531`）、孔径误差 `σ²=Σw_frac²·error²`（`_batch_photometry.pyx:273-277`）、总误差 `σ_tot²=σ_bkg²+I/g_eff`（`photutils/utils/errors.py:91-92`）。本分片已用 photutils 3.0.0 做数值对拍（见报告 §3.6）。
- **properimage** — **BSD-3-Clause** [V]（https://github.com/quatrope/properimage，PyPI 0.7.2）。对照面：Zackay & Ofek proper coaddition 的 Python 参考实现（`properimage/operations.py:457-577` 的 `R=IFFT(Ŝ/√P̂)` 与有效 PSF `P_r`），与 ACSD `C_out=R C_in Rᵀ` 最接近的开源实现。
- **SCAMP** — **GPL-3.0** [V]（https://github.com/astromatic/scamp）。对照面：相对光度零点与相对天体测量（`src/photsolve.c:117-409,437-570,782-785`）。**边界**：在核心源文件中未发现像素背景估计/归一代码（SCI-001-S1 复核，未做全仓穷举），故 UPM 的背景归一不应引 SCAMP 为依据。

