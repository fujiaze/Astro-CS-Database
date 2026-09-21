> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

# 文献与开源研究包：帧级 SNR、PSF 信号权重与逆方差叠加

> 上游：ASTROCS_DESIGN.md §2.2（创新点二：跨帧绝对信噪比）、附录 B（外部标准与文献）

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
| **PSFSNR**（PixInsight 公开文档式[18]） | ratio-of-powers 的**未加权原始信噪比**：`c3·(Σ_j f_j)²/(c4·σ_n²)`，分子为和的平方（功率比口径，SNR² 量级） | `frame_snr` 对标其方法学（PSF/孔径混合测光取信号、稳健噪声、独立背景）；数学定义采用通量型 `F_ref/σ_F` 口径以保证 `w=SNR²/F_ref²` 严格成立，不逐字套用功率比式；常数独立标定 |
| **PSF Signal Weight（PSFSW）** | 综合图像**质量权重**：信号总量×信号集中度（含 FWHM 惩罚）/（稳健噪声×稳健平均背景） | 是权重不是信噪比；仅显式 `weight_mode=psfsw_robust` 使用，四分量独立存储 |（已按 §9.73 A44 作废：该概念不存在）
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
   https://pixinsight.com/forum/index.php?topic=4010.15 （原 `forum.old` 链接已 404，DOC-404 核验订正）

精读要求：把 PSFSNR、PSFSW、标准 SNR、PSF flux、mean PSF flux、M* 背景、N*/MRS 噪声每个量的**完整公式、量纲、归一化常数、标定数据条件**整理成推导笔记，标注页码/章节。

## 4. 必须研读的开源代码（真实仓库）

| 项目 | 许可证 | 仓库/入口 | 对照什么 |
|---|---|---|---|
| **Siril** | GPL-3.0 | https://gitlab.com/free-astro/siril ；重点 `src/stacking/median_and_mean.c:1111-1230`（帧权重 `w_i=1/(pscale_i²·bgnoise_i²)`、wFWHM、星数）、`src/algos/`（PSF 拟合、统计、`bgnoise` 背景噪声估计）、IKSS 归一化实现 | 逆方差型帧权重、稳健背景噪声、加权叠加与拒绝的工程实现；注意 Siril 直接把权重当叠加系数并按帧均值归一，与本项目组内中值归一的无量纲权重不同 |
| **SWarp** | GPL-3.0 | https://www.astromatic.net/software/swarp/ （源码随发行包；用户手册 PDF）；重点 `src/coadd.c:1279-1311`、`src/back.c:361-389` | 逐像素 ivar 组合 `out=Σ(x_k/var_k)/Σ(1/var_k)`、`var_out=1/Σ(1/var_k)`（与本项目对角协方差传播同构）、`RESCALE_WEIGHTS` 按实测噪声重标定、大图像虚拟内存映射与缓冲（对照流式内存设计） |
| **DeepSkyStacker** | BSD-3-Clause | https://github.com/DeepSkyStacker/DSS （原 `DeepSkyStacker/DeepSkyStacker` 链接已 404，DOC-404 核验订正）；重点 `RegisterEngine.cpp:86-118`、`avx_output.cpp:463-575` | 帧评分（圆度加权质量，与 SNR/FWHM 乘积无关）、稳健叠加权重 `w=1/(1+(x−µ)²/σ²)`；无 ivar/读噪项，与本项目设计不同，仅作工程对照 |
| **Source Extractor / SEP** | SExtractor 为 GPL-3.0；SEP 为 LGPL-3.0 | SExtractor：https://www.astromatic.net/software/sextractor/ （重点 `src/analyse.c:200-203,304-310`）；SEP：https://github.com/kbarbary/sep （重点 `src/aperture.c:516-570`） | 背景网格（Background2D）、FLUXERR 误差传播 `Var(F)=Σ(σ_bkg²+F_pix/gain)`、孔径方差 `σ²_sum=Σvar_pix·w²+Σ/gain`（与本项目 CCD 方程同构） |
| **photutils / astropy** | BSD-3-Clause | https://github.com/astropy/photutils ；重点 `photutils/utils/errors.py:91-92`、`photutils/background/core.py:464-531` | aperture/PSF 测光通量与误差、Background2D、SigmaClip、SNR 计算（已数值对拍，通量差 <1%） |
| **properimage** | BSD-3-Clause | https://pypi.org/project/properimage/ ；重点 `properimage/operations.py:457-577` | Zackay & Ofek Paper II 的频域参考实现（`R=IFFT(Ŝ/√P̂)`、有效 PSF P_r），核对 proper coaddition 与本项目信息层的等价性 |
| **SCAMP** | GPL-3.0 | https://www.astromatic.net/software/scamp/ ；重点 `src/photsolve.c` | 只做**相对光度零点与天体测量**求解；核心源文件中无像素背景归一，**UPM 的天光面 g_k/b_k 不引 SCAMP 为依据** |

研读要求：每个项目输出一节"实现了什么权重/噪声/背景量、公式或经验式、输入输出、与 AstroCS 设计的异同"。**GPL 代码只作理解与数值行为对照，不得复制源码进本仓库**（许可证不兼容时算法可独立重写并注明出处）。

## 5. 学术文献清单（按主题，卷期由 agent 核对后补全）

### 5.1 最优叠加、逆方差与 matched filter（Phase2 核心）

1. Zackay, B., & Ofek, E. O. 2017, *How to COAAD Images. I. Optimal Source Detection and Photometry of Point Sources Using Ensembles of Images*, ApJ **836**, 187（arXiv:1512.06872，DOI 10.3847/1538-4357/836/2/187；作者仅 Zackay & Ofek，无 Gal-Yam）——每帧先用**各自的 PSF 做 matched filter 再加权求和**才最优；先 PSF 均质化再叠加会损失灵敏度。直接约束本项目 integration 的 point_information 模式。
2. Zackay, B., & Ofek, E. O. 2017, *How to COAAD Images. II. A Coaddition Image that is Optimal for Any Purpose in the Background-dominated Noise Limit*, ApJ **836**, 188（arXiv:1512.06879）——背景主导噪声极限下任意用途最优的合成图（proper coaddition），核对最终叠加量与方差传播。
3. **【消歧，勿与第 1 条混引】** Zackay, B., Ofek, E. O., & Gal-Yam, A. 2016, *Proper Image Subtraction—Optimal Transient Detection, Imaging and Photometry in the Presence of Point Sources and Galactic Background Noise*（ZOGY），ApJ **830**, 27（arXiv:1601.02655，DOI 10.3847/0004-637X/830/1/27）——这是**图像相减/暂现源检测**论文，不是 COAAD I；卷号 830 与 COAAD I 的 836 分属两篇。
4. Horne, K. 1986, *An optimal extraction algorithm for CCD spectroscopy*, PASP 98, 609——逆方差（1/σ²）最优加权的经典源头。
5. Naylor, T. 1998, *An optimal extraction algorithm for imaging photometry*, MNRAS 296, 339——成像测光的最优加权。
6. Fruchter, A. S., & Hook, R. N. 2002, *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP 114, 144——IIDR/drizzle 权重与像素保留（对照 export/重采样）。

### 5.2 PSF 测光与通量定义（Phase1 SNR 信号侧）

7. Stetson, P. B. 1987, *DAOPHOT: A Computer Program for Crowded-Field Stellar Photometry*, PASP 99, 191。
8. Moffat, A. F. J. 1969, *A Theoretical Investigation of Focal Stellar Images in the Photographic Emulsion and Application to Photographic Photometry*, A&A 3, 455（Moffat 轮廓）。
9. PixInsight 文档引用的 PSF 拟合基础：Levenberg–Marquardt（非线性最小二乘）；Marquardt 1963 与 Moré 1978 的标准实现文献。

### 5.3 稳健噪声与背景估计（Phase1 噪声/天光侧）

10. Starck, J.-L., & Murtagh, F. 1998, *Automatic Noise Estimation from the Multiresolution Support*, PASP 110, 193——MRS 噪声估计（PixInsight 默认）。
11. Starck, Murtagh & Bijaoui, *Image Processing and Data Analysis: The Multiscale Approach*, Cambridge 1998——多尺度中值变换（MMT）与 starlet（对照稳健背景面与星点结构隔离）。
12. Maples, M. P., Reichart, D. E., et al. 2018, *Robust Chauvenet Outlier Rejection*（卷期待 agent 核对）——PixInsight PSF 通量离群剔除所用方法。
13. Bertin, E., & Arnouts, S. 1996, *SExtractor: Software for source extraction*, A&AS 117, 393——分块背景网格与检测阈值噪声模型。
14. Rousseeuw & Croux 1993（MAD/MedDev 尺度估计，MedDev/MSRNoise 类稳健噪声的统计基础，卷期核对）。

### 5.4 投影、HiPS 与球面索引（Phase3 与存储）

15. Greisen, E. W., & Calabretta, M. R. 2002, *Representations of world coordinates in FITS*, A&A 395, 1061（WCS Paper I）；Calabretta & Greisen 2002, A&A 395, 1077（Paper II，投影公式，TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA 的权威出处）。
16. Górski, K. M., et al. 2005, *HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere*, ApJ 622, 759。
17. Fernique, P., et al. 2015, *Hierarchical Progressive Surveys (HiPS)*, A&A 578, A114。
18. Bertin, E., et al. 2002, *The TERAPIX Pipeline*, ASP Conf. Ser. 281（SWarp 方法与权重重标定）。

## 6. 给执行 agent 的研究任务

1. **公式重建**：依据 §3 官方文档写出 PSFSNR、PSFSW、标准 SNR 的完整公式与量纲推导；依据 §5.1 文献从极大似然/最小方差原理推出"每像素逆方差权重 w=1/σ²"与 matched-filter 的关系，证明 `w ∝ SNR²`（在公共通量尺度下）并写明成立条件（高斯噪声、背景主导、已归一化）。
2. **PSF 差异处理**：依据 Zackay & Ofek Paper I 核对 integration 的 point_information 模式——不同 PSF 的帧是否需要先各自 matched filter（与本项目 HiPS 球面存储、分块按需计算如何兼容），给出工程等价实现与数值代价分析；若现设计有偏差，提科学订正控制包。
3. **噪声与背景选型**：对照 MRS、N*、MAD/MedDev、SExtractor 背景网格、Siril bgnoise，选定本项目稳健噪声与天光背景估计器；合成数据实验验证两件事：①注入恒定天光偏置/梯度时信号项不被虚高（普通 SNR 会虚高，frame_snr 不会）；②注入天光散粒噪声增强时 σ_n 如实增大、frame_snr 按理论下降。
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

---

## 8. DOC-404 增补：一手出处核验与行锚（2026-09-20）

> **状态声明**：本增补**不改写** §1–§7 的历史正文（其概念性作废声明继续有效：本项目全程只有 SNR，权重是阶段二按天球像素对应帧集合现场算出的派生量）。本增补只做三件事：① 把 §3–§5 的文献/官方文档补成**可解析、带行锚**的一手出处；② 明确「信噪比 vs 权重」「PSF 有效面积 vs 功率口径」的对应关系；③ 留下核验记录与复跑命令。核验证据：`run/DOC-404/evidence/`。

### 8.1 PixInsight 官方方法学（公开文档，非开源代码；闭源实现不复制）

**权威 URL（`[URL]` HTTP 200，138 604 B）**：https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html

**版本化行锚（官方 Reference-Documentation 仓库，`master` = commit `08b8eb85ae17`，2024-06-21）**：
`gitlab.com/pixinsight/Reference-Documentation` → 仓库内 `ImageWeighting/02-PSF_Flux_Weighting_Algorithms.pidoc`

| 内容 | 文件:行 |
|---|---|
| “hybrid PSF/aperture photometry” 与 “ratio of powers paradigm” 总述 | `02-PSF_Flux_Weighting_Algorithms.pidoc:5` |
| PSF 通量评价（FWTM 椭圆测量域、逐像素减拟合局部背景） | 同文件 `:78-122`（测量域定义在 `:88`） |
| **拟合振幅 A 不用于通量**（通量只由采样像素算出 → hybrid 口径） | 同文件 `:122` |
| 稳健噪声估计（MRS / N*，默认 MRS） | 同文件 `:204-235` |
| **PSFSW（权重，式[16]）**：`w_PSF = c1·(Σf_j)·(Σ f̄_j) / (c2·σ_n·M*)`；归一常数 c1=8.0832e-6、c2=9.0e+6（1000 张 4096² 合成图标定） | 同文件 `:236-278`（公式 `:240-245`，常数 `:261-266`） |
| **PSFSNR（信噪比，式[18]）**：`SNR_PSF = c3·(Σf_j)² / (c4·σ_n²)`；c3=1.350e-7、c4=4.987e+6；标定目标 median(SNR)=median(PSFSNR)=3.029 | 同文件 `:279-302`（公式 `:283-287`，常数 `:294-299`） |
| 标准 SNR（式[20]）：全局尺度估计/噪声方差，官方指出受背景梯度与天光正向影响 | 同文件 `:303-320` |

**PCL 接口面（量纲/输入输出语义核对）**：`pcl::PSFSignalEstimator`（`PSFSignalWeight()`/`PSFSNR()` 等）见 PCL Doxygen https://pixinsight.com/developer/pcl/doc/html/functions_p.html `[URL]` HTTP 200。

**两个量的性质差异（本项目的口径立场）**：

| 量 | 数学形态 | 性质 | AstroCS 立场 |
|---|---|---|---|
| PSFSNR | 功率比 `(Σf)²/σ_n²`（本身即 SNR² 量级） | **未加权的原始信噪比**；信号 = FWTM 孔径内减局部背景的像素和，噪声 = 稳健噪声 | `frame_snr` **对标其方法学**（恒星测光取信号、稳健噪声、独立背景），数学上采用**通量型** `F_ref/σ_F` 以保证 `w=SNR²/F_ref²` 严格成立；不照抄 c3/c4 |
| PSFSW | 信号总量 × 集中度 /（稳健噪声 × 稳健背景） | **权重**（含分辨率/FWHM 与背景惩罚），不是信噪比 | 本项目**不产出该权重对象**；HiPS 只存 SNR，权重在 Phase2 现场算（A44 裁决） |

**PSF 有效面积 / 功率口径的对应**（术语对齐，避免混用）：

- 本项目的“PSF 有效面积”是 `A_NEA = 1/Σ_p P_p²`（噪声等效面积），出现在白噪声近似的点源信息 `W_psf = a²/(σ_pix²·A_NEA)`（`docs/science/PSF_SIGNAL_WEIGHT.md` §2；`docs/plugins/algorithms_phase1/07_noise_snr.md` §4）——它是**通量型**口径的方差因子，与“随帧级 SNR 一并落盘”的产品字段同源（`ASTROCS_DESIGN.md` §4.4）；
- PixInsight 的“功率口径”指 `(Σf)²/σ_n²` 这一 **ratio-of-powers** 形态（上表），它没有显式的 `A_NEA` 因子，且不能再做 `SNR²/F_ref²` 换算（`07_noise_snr.md` §4.1 口径澄清）；
- 因此两者**不可互换**：引用 PixInsight 常数或公式时必须带版本，且不得把功率比数值与本项目通量型 SNR 直接比较。

### 8.2 Horne 1986：最优提取与 `PᵀC⁻¹P` 结构

- Horne, K. 1986, *An optimal extraction algorithm for CCD spectroscopy*, PASP **98**, 609 — `[DOI]` 10.1086/131801（Crossref：PASP vol 98, p 609, 1986, a0=Horne）。
- 定位：已知 profile 与方差下的**逆方差最优加权提取**（`PᵀC⁻¹P` 结构）的经典源头；本项目的 `W_psf = a²PᵀC⁻¹P`、`Var(F_hat)=1/W` 是其在点源成像上的同构（`docs/science/PSF_SIGNAL_WEIGHT.md` §2）。
- **边界**：Horne 1986 是**光谱**最优提取；成像测光的对应处理见 Naylor 1998, MNRAS **296**, 339 — `[DOI]` 10.1046/j.1365-8711.1998.01314.x。二者都只给“给定 profile/方差下的最优加权”，不替代本项目的定标语义。

### 8.3 Zackay & Ofek COAAD：point information 最优组合

- Zackay, B., & Ofek, E. O. 2017, *How to COAAD Images. I. Optimal Source Detection and Photometry of Point Sources Using Ensembles of Images*, ApJ **836**, 187 — `[DOI]` 10.3847/1538-4357/836/2/187；`[ARXIV]` 1512.06872（arXiv API 标题核对一致）。
- 结论要点（本项目 `point_information` 模式的依据）：每帧先用**各自 PSF** 做 matched filter 再加权求和才最优；先做 PSF 均质化再叠加会损失灵敏度。对应设计 `ASTROCS_DESIGN.md` §5.3 的 `w = 1/σ² = SNR²/F_ref²`（不是直接用 SNR 加权）。
- Zackay, B., & Ofek, E. O. 2017, *How to COAAD Images. II. A Coaddition Image that is Optimal for Any Purpose in the Background-dominated Noise Limit*, ApJ **836**, 188 — `[DOI]` 10.3847/1538-4357/836/2/188；`[ARXIV]` 1512.06879。背景主导噪声极限下的 proper coaddition。
- **配对性条件**（本项目写法）：`w ∝ SNR²` 只在**同一帧内** `SNR` 与 `F_ref` 同源时成立（`07_noise_snr.md` §4.1）；跨帧 `F_ref,k` 合法地不同，不要求相等。

### 8.4 ZOGY（与 COAAD I 消歧）

- Zackay, B., Ofek, E. O., & Gal-Yam, A. 2016, *Proper Image Subtraction—Optimal Transient Detection, Photometry, and Hypothesis Testing*, ApJ **830**, 27 — `[DOI]` 10.3847/0004-637X/830/1/27；`[ARXIV]` 1601.02655（arXiv API 标题：“Proper image subtraction - optimal transient detection, photometry and hypothesis testing”）。
- **消歧**：这是**图像相减/暂现源检测**论文，**不是** COAAD I（后者 ApJ 836, 187，无 Gal-Yam）；引用时不得混号（§5.1 已登记同一消歧）。

### 8.5 Siril / SWarp 加权对照（开源，GPL；只读引用，不复制代码）

| 项目（许可证） | 版本/tag | 入口 文件:行 | 对照点 |
|---|---|---|---|
| **Siril**（GPL-3.0） | 1.4.4 | `src/stacking/median_and_mean.c:1091-1094` | 帧权重 `1/(pscale²·bgnoise²)` 与其按帧数归一化：逆方差型帧权重 + 稳健背景噪声（`bgnoise`）的工程实现。**差异**：Siril 直接把权重当叠加系数并按帧均值归一；本项目入库的是**未加权 SNR**，权重在 Phase2 现场算 |
| **SWarp**（GPL-3.0） | 2.41.5 | `src/coadd.c:292`（`coadd_fields`）；`src/back.c:413`（`backstat`）；`src/back.c:642`（`backguess`） | 逐像素加权组合与背景统计；官方页 https://www.astromatic.net/software/swarp/ `[URL]` 200 |
| **SCAMP**（GPL-3.0） | v2.15.0 | `src/photsolve.c:117`（`photsolve_fgroups`）；`src/astrsolve.c:117`（`astrsolve_fgroups`） | 相对光度零点 + 天体测量解算；**不引其为天光面/UPM 依据**（核心无像素背景归一，§4 已登记） |

### 8.6 增补核验到的其他一手出处（供 §5 补卷期）

| 主题 | 引用 | 核验 |
|---|---|---|
| MRS 稳健噪声 | Starck, J.-L., & Murtagh, F. 1998, PASP **110**, 193 | `[DOI]` 10.1086/316124 |
| 稳健离群（RCR） | Maples, M. P., et al. 2018, ApJS **238**, 2 | `[DOI]` 10.3847/1538-4365/aad23d |
| CCD 噪声模型 | Mortara, L., & Fowler, A. 1981, SPIE **290**, 28；Merline, W. J., & Howell, S. B. 1995, Exp. Astron. **6**, 163 | `[DOI]` 10.1117/12.965833；10.1007/bf00421131 |
| MAD/MedDev 与 Tukey biweight | Rousseeuw & Croux 1993, JASA **88**, 1273；Beaton & Tukey 1974, Technometrics **16**, 147 | `[DOI]` 10.1080/01621459.1993.10476408；10.1080/00401706.1974.10489171 |
| 背景网格与 FLUXERR | Bertin & Arnouts 1996, A&AS **117**, 393 | `[DOI]` 10.1051/aas:1996164 |
| 多帧点源最优组合 | Zackay & Ofek 2017（§8.3 两条） | `[DOI]`×2 |

### 8.7 未找到一手出处（`[NONE]`，如实登记）

- PixInsight **PCL 源码**（PSFSignalEstimator 实现）：闭源商业软件，无公开一手实现可核验——只核验到公开 Doxygen 接口页（`[URL]` 200）；常数与实现细节以官方文档行锚为准（§8.1）。
- PixInsight 文档中 PSFSW/PSFSNR 常数的**独立复现记录**：官方只给“1000 张 4096² 合成图、单位中值归一”的条件描述，未发布数据与脚本 ⇒ 本项目不照抄常数，自行用 L1 合成图标定（§6 第 5 条）。

### 8.8 历史 §4 表的版本化核验补齐（不重写历史表）

§4 的对照表是历史留痕（无版本列）。为满足「开源对照均给出 项目+版本+文件:行」，本小节按 tag 逐条复验并给出**版本化锚**；语义漂移者如实标注。证据：`run/DOC-404/evidence/oss/` 与 `run/DOC-404/evidence/oss_legacy/`。

| §4 历史锚 | 项目@版本 | 复验结论（版本化锚） |
|---|---|---|
| Siril `median_and_mean.c:1111-1230` | Siril 1.4.4 | **有效（行号需精确化）**：该区间落在 `compute_wfwhm_weights`（函数声明在 `:1106`）函数体内，区间内逐字命中 `weighted_fwhm`；逆方差型权重 `1/(pscale²·bgnoise²)` 在 `:1091-1094` |
| SWarp `coadd.c:1279-1311` | SWarp 2.41.5 | **有效**：`COADD_WEIGHTED` 分支的逐像素逆方差组合循环 |
| SWarp `back.c:361-389` | SWarp 2.41.5 | **有效**：权重/方差重标定分支（`VAR_FIELD/WEIGHT_FIELD` 比例统计） |
| DeepSkyStacker `RegisterEngine.cpp:86-118` | DeepSkyStacker/DSS 6.2.2 | **有效**：`:86` 为 `CRegisteredFrame::ComputeScore`（帧评分） |
| DeepSkyStacker `avx_output.cpp:463-575` | DeepSkyStacker/DSS 6.2.2 | **有效**：`:463` 为 `doProcessAutoAdaptiveWeightedAverage`（自适应加权平均） |
| SExtractor `analyse.c:200-203,304-310` | SExtractor 2.28.2 | **有效**：`:200-203` 为 FLUXERR 方差累加（`pix/gain` 项）；`:304-310` 为 `flux/fluxerr` 赋值 |
| SEP `aperture.c:516-570` | SEP v1.4.1 | **有效**：孔径方差累加（`sumvar += scale2·varpix`） |
| photutils `utils/errors.py:91-92` | photutils 3.0.0 | **语义漂移**：`:91-92` 是 docstring 中的 `σ_tot` 公式；函数入口 `calc_total_error` 在 `:12` |
| photutils `background/core.py:464-531` | photutils 3.0.0 | **语义漂移**：该范围在 `BackgroundBase.calc_background` 文档串内；`Background2D` 已迁至 `background/background_2d.py:33` |
| properimage `operations.py:457-577` | properimage v0.7.2 | **有效**：`:457-579` 为 `coadd()`（R 估计量合成，对应 COAAD II） |
| SCAMP `src/photsolve.c`（原无行号） | SCAMP v2.15.0 | **补行号**：`:117` `photsolve_fgroups`；天体测量侧 `src/astrsolve.c:117` `astrsolve_fgroups` |

- 另：§4 表中 DeepSkyStacker 与 PixInsight 论坛两条 URL 原为 404，DOC-404 已就地订正为可解析链接（`https://github.com/DeepSkyStacker/DSS`、`https://pixinsight.com/forum/index.php?topic=4010.15`，均 HTTP 200）。

### 8.9 核验记录与复跑命令

```bash
# PixInsight 官方文档（HTML）与 pidoc 行锚
curl -sL -A "Mozilla/5.0 AstroCS" -o run/DOC-404/evidence/pixinsight-weighting.html \
  https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html
curl -sL -A "Mozilla/5.0 AstroCS" -o run/DOC-404/evidence/pixinsight_02_pidoc.txt \
  "https://gitlab.com/api/v4/projects/pixinsight%2FReference-Documentation/repository/files/docs%2FImageWeighting%2F02-PSF_Flux_Weighting_Algorithms.pidoc/raw?ref=master"
grep -n 'subsection' run/DOC-404/evidence/pixinsight_02_pidoc.txt   # 236 = PSFSW；279 = PSF SNR
# 开源行锚（Siril/SWarp/SCAMP/SExtractor/SEP/photutils/astrometry.net/astropy/DeepSkyStacker/properimage）
# 非退化判据：逐条断言「锚行/锚区间逐字命中预期符号」，符号不在锚位置即判红
python3 run/DOC-404/check_pack_oss_anchors.py   # 期望 anchors=36 ok=36 bad=0
python3 run/DOC-404/verify_oss.py && python3 run/DOC-404/verify_oss2.py && python3 run/DOC-404/verify_oss3.py
# DOI/arXiv（§8.2–§8.6）
python3 run/DOC-404/verify_refs.py doi 10.1086/131801 10.1046/j.1365-8711.1998.01314.x \
  10.3847/1538-4357/836/2/187 10.3847/1538-4357/836/2/188 10.3847/0004-637X/830/1/27
```

**行锚证据文件**：`run/DOC-404/evidence/oss_line_anchors.txt`（含每个 tag 的 raw URL、字节数、命中行）；**pidoc 行锚**：`run/DOC-404/evidence/pixinsight_02_pidoc.txt`；**DOI 落盘**：`run/DOC-404/evidence/crossref/*.json`。
