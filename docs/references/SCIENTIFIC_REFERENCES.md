# AstroCS 科学与格式参考文献档案

文档 ID：`ASTROCS-REFERENCES-001`  
状态：`ACTIVE_REFERENCE`  
规则：项目采用的外部科学/格式依据统一在此登记；具体 SCI/设计文档仍须把引用落实到 claim。外部资料用于支持或约束推导，不能替代项目明确的单位、适用域和验收。

## A. 探测器校准与噪声

1. Newberry, M. V. 1991, “Signal-to-Noise Considerations for Sky-Subtracted CCD Data”, PASP 103, 122. DOI: 10.1086/132801。用途：CCD 信噪、sky/read/bias/dark 项。
2. Janesick, J. R. 2001, *Scientific Charge-Coupled Devices*, SPIE PM83, ISBN 0-8194-3698-4。用途：gain、read noise、photon transfer。
3. [HST ACS Data Handbook §4.4 Flat-Field Reference Files](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-4-flat-field-reference-files)。用途：flat-field 像素响应与低频校正边界。

## B. PSF、最优提取、SNR 与图像权重

4. Horne, K. 1986, “An Optimal Extraction Algorithm for CCD Spectroscopy”, PASP 98, 609. DOI: 10.1086/131801。用途：已知 profile 和方差下的最优提取；AstroCS 借其统计结构定义 `PᵀC⁻¹P`，成像 PSF 需独立验证。
5. Naylor, T. 1998, “An optimal extraction algorithm for imaging photometry”, MNRAS 296, 339. [全文](https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf)。用途：成像最优 PSF 光度。
6. Zackay, B. & Ofek, E. O. 2017, “How to coadd images? I. Optimal source detection and photometry using ensembles of images”, ApJ 836, 187. [arXiv:1512.06872](https://arxiv.org/abs/1512.06872)。用途：每帧按自身 PSF matched filter 后组合；普通先叠加后滤波/PSF homogenization 会损失灵敏度。
7. Zackay, B. & Ofek, E. O. 2017, “How to coadd images? II. A coaddition image that is optimal for any purpose in the background-dominated noise limit”, ApJ 836, 188. [arXiv:1512.06879](https://arxiv.org/abs/1512.06879)。用途：proper coadd 与信息保持表示。
8. [PixInsight Reference: New Image Weighting Algorithms](https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html)。用途：§2.5 PSF Signal Weight、§2.6 PSF SNR、signal concentration 和工程加权语义。裁决：AstroCS 正式支持 `psfsw_robust` conventional-integration 模式；其 PSF signal/concentration/noise/background 四分量和组内归一须可审计，但无量纲复合权重不自动等同严格 `1/Var(F_hat)`。访问核对：2026-09-15。

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
18. Bertin, E. 2010, “SCAMP: automatic astrometric and photometric calibration”, ASP Conf. Ser. 442, 435. [ADS](https://ui.adsabs.harvard.edu/abs/2010ASPC..442..435B/abstract)。用途：多帧天体/光度联合校准实践。
19. Gruen, D., Seitz, S. & Bernstein, G. M. 2014, “Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking”, PASP 126, 158. [ADS](https://ui.adsabs.harvard.edu/abs/2014PASP..126..158G/abstract)。用途：叠加排异与 PSF 差异下的伪影控制。
20. Mosteller, F. & Tukey, J. W. 1977, *Data Analysis and Regression*. 用途：robust biweight；常数与效率必须由专项 SCI 精确定位。

## F. 项目内来源档案

- `设计大纲/大报告_项目历史.md`：实现/口径演进证据，不是当前科学权威。
- `设计大纲/大报告_历代控制包.md`：需求和治理演进证据。
- `reports/review-package-20260915/`：V3 后问题、推导和缺陷账本。
- `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md`：测光文献调查原始留档。
- `run/perf-fix/P5-snr/`：P5 SNR 实验、Oracle 和报告留档；结论须按新统一科学模型重新解释。

## H. 测光专项参考档案（既有 B1–B90 调查）

项目既有测光专项调查已逐条登记 B1–B90 项书目、DOI/URL、定量 claim 与更正记录。为避免在多个活动文件复制后漂移，该原始档案保留于：

- tracked 原文快照 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md`，§“来源与引用” [B1]–[B90]；
- 原工作档 `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md` 及同目录 `lit/` 笔记保留作来源取证。

这些专项资料被本总档案整体纳入，重点包括：Bessell & Murphy 2012（光子/能量通带与零点）、Gaia EDR3/DR3 测光与 XP 外部定标、CALSPEC、Stetson/Anderson & King/Dolphin/Naylor 的 PSF/拥挤/欠采样测光、Howell/Newberry 的 CCD SNR、Fruchter & Hook/Zackay 等的相关噪声，以及 Pan-STARRS/HSC/LSST 的深度定义。

核验标签沿用原档案：`[V]` 为逐字或主来源核验，`[S]` 为次级/摘要定位，`[U]` 为未核实。实施前引用具体数字时必须回到原条目，不得把总档案的“纳入”误当成逐式复核。完整原文现已复制到 tracked 快照；V6 的 `DOC-CONVERGE-001` 仍须把最终采用的 claim 落到对应 SCI/ALG，并保留 DOI/bibcode。

## G. 引用纪律

- 新 SCI claim 必须给作者/年份/稳定 URL 或 DOI、具体节/式及项目推导差异；
- 文献没有定义项目字段单位、schema 和失败语义，这些必须由项目明确；
- 经验质量指标与 Fisher information/逆方差不得仅因名字相似而等同；
- 参考实现不可作为唯一 Oracle；至少一个解析/独立数值实现；
- 链接失效时保留 DOI/bibcode 和访问日期，不删除引用历史。
