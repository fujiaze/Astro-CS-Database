# 文献与开源研究包：帧级 SNR、PSF 信号权重与逆方差叠加

## 1. 本包的用途

本包是给执行 agent 的**研究任务书 + 一手资料清单**，用于把帧级 SNR（`frame_snr`）、PSF 信号权重、Phase2 逆方差叠加的公式与实现建立在可核查的公开资料与开源代码之上，而不是建立在对商业软件行为的转述上。

研究结论与冻结公式写入仓库 `docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/science/NOISE_MODEL.md`；本包只规定研究什么、对照什么、产出什么。

## 2. 已确认的事实（研究起点，不是终点）

### 2.1 PixInsight 的开源情况

- PixInsight 核心程序与 PCL 中 PSF 信号估计的**实现代码是闭源商业软件**，不可复制、不可反编译进本项目；
- 但其**方法学参考文档完全公开**（官网与官方 GitLab Reference-Documentation 仓库），公式、归一化常数、FITS 元数据关键字均有明文，可作为方法学对照；
- 因此路线是：**依据公开方法学 + 独立学术文献 + 开源对照实现，自行推导与实现，常数独立标定**。

### 2.2 三个量必须严格区分（本项目口径）

| 量 | 性质 | AstroCS 立场 |
|---|---|---|
| **PSFSNR**（PixInsight 公开文档） | ratio-of-powers 的**未加权原始信噪比**：`c3·√(Σ_j f_j²)/(c4·σ_n²)` | `frame_snr` 对标其方法学（PSF/孔径混合测光取信号、稳健噪声、独立背景），常数独立标定 |
| **PSF Signal Weight（PSFSW）** | 综合图像**质量权重**：信号总量×信号集中度（含 FWHM 惩罚）/（稳健噪声×稳健平均背景） | 是权重不是信噪比；仅显式 `weight_mode=psfsw_robust` 使用，四分量独立存储 |
| **逆方差权重 w=1/σ²** | 叠加时的最优统计权重 | Phase2 消费时由 SNR 现场换算 `w=SNR²/F_ref²`，不入库、不预计算稠密 |

- PixInsight 官方同样**不把权重写入图像**：校准阶段只写信号/噪声/背景分量元数据（FITS 关键字 PSFFLX/PSFFLP/PSFMFL/PSFMFP/PSFMST/PSFNST/PSFSGN/NOISE 等），集成时才算权重——与本项目"HiPS 数据库存原始 SNR、Phase2 才算权重"一致；
- 官方明确批评标准全局 SNR（`σ²/σ_n²`）：受天光梯度与背景亮度正向影响，会给目标 SNR 实际很低的亮背景帧虚高权重——这正是本项目拒绝普通 SNR 作帧级参考的理由。

## 3. 必须精读的一手公开资料

### 3.1 PixInsight 官方方法学（公开文档，非开源代码）

1. New Image Weighting Algorithms in PixInsight（主文档，含 PSF 拟合、PSF 通量、稳健背景 MMT、MRS/N* 噪声、PSFSW 式(16)、PSFSNR 式(18)、标准 SNR 式(20)、集成式(21)、FITS 关键字表）：
   https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html
2. 官方 GitLab Reference-Documentation（pidoc 源文件，01 概述/02 方法/03 Implementation/04 Examples/05 Linear Regression Analysis，需逐节读完，03 节含完整实现公式）：
   https://gitlab.com/pixinsight/Reference-Documentation/-/tree/master/docs/ImageWeighting
3. PCL API 文档中 `pcl::PSFSignalEstimator`（PSFSignalWeight()/PSFSNR() 等接口签名与语义，用于核对输入输出量纲）：
   https://pixinsight.com/developer/pcl/doc/html/functions_p.html
4. PixInsight 官方论坛 PSFEstimation 讨论（官方人员对 "SNR weight = (MedDev/MSRNoise)²，即 ImageIntegration 噪声评价权重的未归一化估计" 的说明）：
   https://pixinsight.com/forum.old/index.php?topic=4010.15

精读要求：把 PSFSNR、PSFSW、标准 SNR、PSF flux、mean PSF flux、M* 背景、N*/MRS 噪声每个量的**完整公式、量纲、归一化常数、标定数据条件**整理成推导笔记，标注页码/章节。

## 4. 必须研读的开源代码（真实仓库）

| 项目 | 许可证 | 仓库/入口 | 对照什么 |
|---|---|---|---|
| **Siril** | GPL v3 | https://gitlab.com/free-astro/siril ；重点 `src/stacking/stacking.c`（加权叠加、拒绝）、`src/algos/`（PSF 拟合、统计、`bgnoise` 背景噪声估计）、IKSS 归一化实现 | 帧权重方法（星数/wFWHM/noise/曝光）、稳健背景噪声、加权叠加与拒绝的工程实现 |
| **SWarp** | GPL | https://www.astromatic.net/software/swarp/ （源码随发行包；用户手册 PDF） | weight-map 输入输出全流程、`RESCALE_WEIGHTS` 按实测噪声重标定权重/方差、大图像虚拟内存映射与缓冲（对照本项目流式内存设计） |
| **DeepSkyStacker** | GPL v3 | https://github.com/DeepSkyStacker/DeepSkyStacker | 帧评分（FWHM/星数/背景/偏移）、叠加权重与校准流程 |
| **Source Extractor / SEP** | SExtractor LGPL/类 GPL；SEP 为 LGPL | https://www.astromatic.net/software/sextractor/ ；SEP：https://github.com/kbarbary/sep | 背景网格（Background2D）、FLUXERR 与检测 SNR、PSF/孔径测光的误差传播 |
| **photutils / astropy** | BSD 3-clause | https://github.com/astropy/photutils | aperture/PSF 测光通量与误差、Background2D、SigmaClip、SNR 计算（可直接数值对拍） |
| **properimage** | 开源（核实许可证） | https://pypi.org/project/properimage/ | Zackay & Ofek 最优 coaddition 的参考 Python 实现，核对 matched-filter 后加权流程 |
| **SCAMP** | GPL | https://www.astromatic.net/software/scamp/ | 光度/背景相对归一化（对照 UPM 的 g_k、b_k） |

研读要求：每个项目输出一节"实现了什么权重/噪声/背景量、公式或经验式、输入输出、与 AstroCS 设计的异同"。**GPL 代码只作理解与数值行为对照，不得复制源码进本仓库**（许可证不兼容时算法可独立重写并注明出处）。

## 5. 学术文献清单（按主题，卷期由 agent 核对后补全）

### 5.1 最优叠加、逆方差与 matched filter（Phase2 核心）

1. Zackay, B., Ofek, E. O., & Gal-Yam, A. 2016, *How to coadd images? I. Optimal Source Detection and Photometry of Point Sources Using Ensembles of Images*, ApJ 830, 27（arXiv:1512.06872）——每帧先用**各自的 PSF 做 matched filter 再加权求和**才最优；先 PSF 均质化再叠加会损失灵敏度。直接约束本项目 integration 的 point_information 模式。
2. Zackay, B., & Ofek, E. O. 2017, *How to coadd images? II. A coaddition image that is optimal for any purpose in the background-dominated noise limit*, ApJ 836（arXiv:1512.06879）——背景主导噪声极限下任意用途最优的合成图（proper coaddition），核对最终叠加量与方差传播。
3. Horne, K. 1986, *An optimal extraction algorithm for CCD spectroscopy*, PASP 98, 609——逆方差（1/σ²）最优加权的经典源头。
4. Naylor, T. 1998, *An optimal extraction algorithm for imaging photometry*, MNRAS 296, 339——成像测光的最优加权。
5. Fruchter, A. S., & Hook, R. N. 2002, *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP 114, 144——IIDR/drizzle 权重与像素保留（对照 export/重采样）。

### 5.2 PSF 测光与通量定义（Phase1 SNR 信号侧）

6. Stetson, P. B. 1987, *DAOPHOT: A Computer Program for Crowded-Field Stellar Photometry*, PASP 99, 191。
7. Moffat, A. F. J. 1969, *A Theoretical Investigation of Focal Stellar Images in the Photographic Emulsion and Application to Photographic Photometry*, A&A 3, 455（Moffat 轮廓）。
8. PixInsight 文档引用的 PSF 拟合基础：Levenberg–Marquardt（非线性最小二乘）；Marquardt 1963 与 Moré 1978 的标准实现文献。

### 5.3 稳健噪声与背景估计（Phase1 噪声/天光侧）

9. Starck, J.-L., & Murtagh, F. 1998, *Automatic Noise Estimation from the Multiresolution Support*, PASP 110, 193——MRS 噪声估计（PixInsight 默认）。
10. Starck, Murtagh & Bijaoui, *Image Processing and Data Analysis: The Multiscale Approach*, Cambridge 1998——多尺度中值变换（MMT）与 starlet（对照稳健背景面与星点结构隔离）。
11. Maples, M. P., Reichart, D. E., et al. 2018, *Robust Chauvenet Outlier Rejection*（卷期待 agent 核对）——PixInsight PSF 通量离群剔除所用方法。
12. Bertin, E., & Arnouts, S. 1996, *SExtractor: Software for source extraction*, A&AS 117, 393——分块背景网格与检测阈值噪声模型。
13. Rousseeuw & Croux 1993（MAD/MedDev 尺度估计，MedDev/MSRNoise 类稳健噪声的统计基础，卷期核对）。

### 5.4 投影、HiPS 与球面索引（Phase3 与存储）

14. Greisen, E. W., & Calabretta, M. R. 2002, *Representations of world coordinates in FITS*, A&A 395, 1061（WCS Paper I）；Calabretta & Greisen 2002, A&A 395, 1077（Paper II，投影公式，TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA 的权威出处）。
15. Górski, K. M., et al. 2005, *HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere*, ApJ 622, 759。
16. Fernique, P., et al. 2015, *Hierarchical Progressive Surveys (HiPS)*, A&A 578, A114。
17. Bertin, E., et al. 2002, *The TERAPIX Pipeline*, ASP Conf. Ser. 281（SWarp 方法与权重重标定）。

## 6. 给执行 agent 的研究任务

1. **公式重建**：依据 §3 官方文档写出 PSFSNR、PSFSW、标准 SNR 的完整公式与量纲推导；依据 §5.1 文献从极大似然/最小方差原理推出"每像素逆方差权重 w=1/σ²"与 matched-filter 的关系，证明 `w ∝ SNR²`（在公共通量尺度下）并写明成立条件（高斯噪声、背景主导、已归一化）。
2. **PSF 差异处理**：依据 Zackay & Ofek Paper I 核对 integration 的 point_information 模式——不同 PSF 的帧是否需要先各自 matched filter（与本项目 HiPS 球面存储、分块按需计算如何兼容），给出工程等价实现与数值代价分析；若现设计有偏差，提科学订正控制包。
3. **噪声与背景选型**：对照 MRS、N*、MAD/MedDev、SExtractor 背景网格、Siril bgnoise，选定本项目稳健噪声与天光背景估计器，证明 frame_snr 在注入天光梯度/光污染/月光时不漂移（合成数据实验）。
4. **开源对拍**：用同一组合成数据分别在 photutils（测光/背景）、SWarp（权重叠加）、Siril（噪声/权重）上跑等价流程，与本项目实现数值对照，记录偏差与解释。
5. **常数独立标定**：PSFSNR 的 c3/c4、PSFSW 的 c1/c2 是 PixInsight 用其模拟集标定的经验常数，**不照抄**；用本项目 L1 合成数据标定本项目自己的归一化常数（或采用无需经验常数的物理量纲定义），标定条件、数据、脚本、结果冻结入库。
6. **产出与冻结**：更新 `docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/science/NOISE_MODEL.md`，每个公式标注出处或推导编号、每个常数附标定记录；新增公式进入 L1 Oracle 对拍用例。
7. **合规**：GPL 项目代码不进入本仓库；引用的方法、公式、开源实现与许可证在科学文档中列明。

## 7. 完成判据

- 第 2.2 节三个量在科学文档中有完整公式、量纲、出处，且与 `07_noise_snr.md`、`UNIFIED_MODEL.md` 口径一致；
- 逆方差权重与 Zackay & Ofek 结论一致（或给出有据的偏离说明）；
- 合成数据证明 frame_snr 对天光梯度/光污染不敏感、对真实信号/噪声变化敏感；
- 至少三个开源对照实现的行为比对记录归档；
- 所有经验常数有本项目自己的标定记录，无照抄常数、无复制 GPL 代码。
