# 核验-CIT-08 —— 引用文献核验成稿（批次 CIT-08，14 条）

- 基线：`c8f64e9a`（仓库 `F:\Astro dev\Astro CS Normalization Database`，只读；`git rev-parse --short HEAD` = `c8f64e9a` 已确认）
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` 全条 + 派单 CIT-08 补充约束
- 网络手段清单（本批**实际走过**的途径；本机 shell 有外网，故元数据以 API 原始回包为准，未用摘要式转述）：
  1. DOI → `https://api.crossref.org/works/<doi>`（14 条全打，逐条取 title/subtitle/volume/page/issued/author[]/publisher）
  2. DOI → `https://api.openalex.org/works/doi:<doi>`（Crossref 无 abstract 时取摘要与作者数：132749 / 116242 / 154592 / 432977 / 316632 / 664083）
  3. arXiv → `https://export.arxiv.org/api/query?id_list=…` 与 `search_query=ti:"…"`（2208.00211、astro-ph/9808087、1510.09180、astro-ph/0507007）
  4. 正文 → ADS 经典扫描 `https://articles.adsabs.harvard.edu/pdf/<bibcode>`（1996A&AS..117..393B 成功；1976ApJ...208..177L 成功；1990PASP..102.1181B 前 3 次 504、第 4 次成功；2005PASP..117.1113M **403**）
  5. 正文 → arXiv PDF（astro-ph/9808087v2 = Fruchter & Hook 预印本；astro-ph/0507007 = MOPEX 预印本；1510.09180 = Libralato 预印本）
  6. 正文 → 作者版 PDF（`cs.tut.fi/~foi/papers/OptAnscombeInverse-IEEE_TIP-Preprint.pdf`，经 Semantic Scholar `openAccessPdf` 字段定位）
  7. ADS API / ui.adsabs 未用（本环境一律 405）；IOP/EDP 正式排版 PDF 为付费墙，未取
- 仓库侧读取：只读 `Read` / `git grep`；14 条断言原文**全部逐条打开**（见"逐条依据"），未以题名比对替代
- 特别纪律执行：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` 的 `[V]/[S]` 标签一律不采信，落该文件的 192/204/216/252 四条全部重跑 Crossref+OpenAlex/arXiv 元数据

## 结论表

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据（API 原文片段） | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 186 | DOI 10.1051/0004-6361/202039709 | 已核 | 版本对（A&A 649, A2, 2021-04-28，97 位作者，一作 L. Lindegren） | 关联对 | Crossref：`"title":["Gaia Early Data Release 3"],"subtitle":["The astrometric solution"],"volume":"649","page":"A2"` | — |
| 192 | DOI 10.1051/0004-6361/202243940 | 已核 | 版本对（A&A 674, A1, 2023-06，456 位作者；预印本 arXiv:2208.00211v1 同题） | 关联对（按 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:49`）；**档案内列而不用**（全仓 `B90` 仅出现在定义行） | Crossref：`"subtitle":["Summary of the content and survey properties"],"volume":"674","page":"A1"`；arXiv API：`2208.00211v1`，`<published>2022-07-30` | — |
| 198 | DOI 10.1051/aas:1996164（§3） | 已核 | 版本对（A&AS 117, 393-404, 1996-06，Bertin & Arnouts 2 人） | 关联对（§2=背景网格、§3=检测，逐字对上） | Crossref：`"title":["SExtractor: Software for source extraction"],"volume":"117","page":"393-404"`；ADS 扫描 p.394 §3 原文："SExtractor uses Lutz\'s one-pass algorithm (Lutz 1979) to extract 8-connected contiguous pixels from a "template frame"" | — |
| 204 | DOI 10.1086/116242 | 已核 | 版本对（AJ 104, 340, 1992-07，单作者 Arlo U. Landolt） | 关联对 | Crossref：`"title":["UBVRI photometric standard stars in the magnitude range 11.5-16.0 around the celestial equator"],"volume":"104","page":"340"`；OpenAlex 摘要："UBVRI photoelectric observations have been made on the Johnson-Kron-Cousins photometric system of 526 stars centered on the celestial equator" | — |
| 210 | DOI 10.1086/132749（UBVRI） | 已核 | 版本对（PASP 102, 1181, 1990-10，单作者 M. S. Bessell） | **关联错（角色错绑）** | Crossref：`"title":["UBVRI passbands"],"volume":"102","page":"1181"`；OpenAlex 摘要全篇无光子计数口径字样；正文扫描（OCR 98,954 字符）关键词计数 `counting`=0、`integrat`=0、`per unit wavelength`=0，`photon` 仅 2 处且均为 CCD 涂层语境 | — |
| 216 | DOI 10.1086/154592 | 已核 | 版本对（ApJ 208, 177, 1976-08，Lampton/Margon/Bowyer 3 人） | **角色错绑** | Crossref：`"title":["Parameter estimation in X-ray astronomy"],"volume":"208","page":"177"`；正文原文："The covariance matrix for the desired parameters, Vqq, is obtained by simply deleting the unwanted rows and columns from Vpp (Eadie et al. 1971, p. 199)"、"this method must be used with great caution because its derivation explicitly depends upon model linearity" | — |
| 222 | DOI 10.1086/316632 | 已核 | 版本对（PASP 112, 1360-1382, 2000-10，Anderson & King 2 人） | 关联对 | Crossref：`"title":["Toward High-Precision Astrometry with WFPC2. I. Deriving an Accurate Point-Spread Function"],"volume":"112","page":"1360-1382"`；OpenAlex 摘要："…fraught with dangers when the images are undersampled. … We apply the concept of the effective PSF (ePSF)" | — |
| 228 | DOI 10.1086/338393（§5） | 已核 | 版本对（PASP 114, 144-152, 2002-02；同一文预印本 astro-ph/9808087v2） | **关联错（节号）**：相关噪声在 §7 不在 §5 | Crossref：`"title":["Drizzle: A Method for the Linear Reconstruction of Undersampled Images"],"volume":"114","page":"144-152"`；预印本节结构 `5. PHOTOMETRY / 6. ASTROMETRY / 7. NOISE IN DRIZZLED IMAGES`，§7.1 原文："As a result, the noise in adjacent pixels will be correlated."，§7.2 原文："Using the relatively typical values of p = 0.6 and s = 0.5, one finds R = 1.662." | 正式版节号未直读：IOP 付费墙、ADS 扫描 `AccessDenied`(403)、`cdsads` 无响应 |

| 234 | DOI 10.1086/432977 | 已核 | 版本对（PASP 117, 1113-1128, 2005-10，Makovoz & Marleau 2 人；预印本 astro-ph/0507007v1 题名无连字符 "Point Source Extraction with MOPEX"） | **角色错绑** | Crossref：`"title":["Point-Source Extraction with MOPEX"],"volume":"117","page":"1113-1128"`；正文原文："Since mosaicking is part of MOPEX (Makovoz & Khan 2004), point source extraction benefits from such capabilities as creating properly resampled mosaic images"；全文 `overlap` 2 处均属 blend 拟合区、`interpolat` 3 处均属 PRF 插值，无"tile 重叠区两候选值插值 + 插值不确定度传播" | — |
| 240 | DOI 10.1086/664083（photonic） | 已核 | 版本对（PASP 124, 140-157, 2012-02，2 作者） | 关联对（但**作者缩写与出版社记录不符**，见 P1-5） | Crossref：`"title":["Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI, Hipparcos, and Tycho Photometry"],"author":[("Michael","Bessell"),("Simon","Murphy")]`；OpenAlex 摘要："We have calculated improved photonic passbands for the UBVRI, Hipparcos Hp, and Tycho BT and VT standard systems … revised synthetic zero points were determined." | — |
| 246 | DOI 10.1093/biomet/35.3-4.246 | 已核 | 版本对（Biometrika 35, 246-254, 1948，单作者 F. J. Anscombe） | 关联对 | Crossref：`"title":["THE TRANSFORMATION OF POISSON, BINOMIAL AND NEGATIVE-BINOMIAL DATA"],"container":["Biometrika"],"volume":"35","page":"246-254"` | — |
| 252 | DOI 10.1093/mnras/stv2628 | 已核 | 版本对（MNRAS 456, 1137-1162；Crossref `issued` 2015-12-23 为网络首发，卷期年 2016；4 作者 Libralato/Bedin/Nardiello/Piotto；预印本 arXiv:1510.09180） | 关联对（量化项逐字对上摘要） | Crossref：`"title":["A PSF-based approach to Kepler/K2 data – I. Variability within the K2 Campaign 0 star clusters M 35 and NGC 2158"],"volume":"456","page":"1137-1162"`；arXiv 摘要原文："…as the basis for PSF neighbour subtraction, we are able to reach magnitudes as faint as KP24 with a photometric precision of 10% over 6.5 hours… At the bright end, our photometric precision reaches 30 parts-per-million." | — |
| 258 | DOI 10.1109/TIP.2010.2056693 | 已核 | 版本对（IEEE TIP 20, 99-109, 2011-01；Crossref/IEEE 记录剥变音符作 `Makitalo`，作者全名 Markku Mäkitalo ⇒ 仓内 "Mäkitalo" 写法正确） | 关联对（§II.A 式 (2)(4) 号对上） | Crossref：`"title":["Optimal Inversion of the Anscombe Transformation in Low-Count Poisson Image Denoising"],"volume":"20","page":"99-109"`；作者版全文（IEEE 双栏）`II. PRELIMINARIES → A. Poisson noise`：式 (2) `E{zi j yi} D yi D var{zi j yi}`、式 (4) `f .z/ D 2 p z C 3=8`（正文称 "the forward Anscombe transformation (4)"）、随后 `A. Exact unbiased inverse` | IEEE Xplore 正式 PDF 未取（付费墙）；式号按作者接收版核对 |
| 264 | DOI 10.1145/368996.369025 | 已核 | 版本对（Commun. ACM 5, 558-562, 1962-11，单作者 A. B. Kahn） | 关联对 | Crossref：`"title":["Topological sorting of large networks"],"container":["Communications of the ACM"],"volume":"5","page":"558-562","issued":{"date-parts":[[1962,11]]}` | — |

**本批存在性 14/14 全真、无假 DOI、无同篇双登**（形态①④⑤本批 0 命中；形态②"同一篇两个标识符当两条"本批 0 命中：210 与 240 是同作者两篇不同文，均各自成立）。

## 逐条依据与断言原文

### 186 · 10.1051/0004-6361/202039709
断言原文（`docs/algorithms/GAIA_QUERY.md:285`）：「Gaia 天体测量解：Lindegren et al. 2021, A&A 649, A2（DOI 10.1051/0004-6361/202039709）。」
Crossref 回包 `subtitle = "The astrometric solution"`、`page = A2`、`author[0] = ("L.","Lindegren")`、97 位作者 ⇒ 作者、年份、卷、文章号、角色五项全对。同文件 :284 另引「2021, A&A 649, A1（EDR3）」指 Gaia Collaboration（Brown 等），两条不冲突。

### 192 · 10.1051/0004-6361/202243940
断言原文（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:231`）：「[B90] Gaia Collaboration, Vallenari, A., Brown, A. G. A., et al. 2023, A&A 674, A1, "Gaia Data Release 3: Summary of the content and survey properties" DOI 10.1051/0004-6361/202243940, arXiv:2208.00211」——该条**无核验标签**，且逐字题名与 Crossref `title+subtitle` 拼接完全一致（未插词）。
`git grep B90` 全仓命中：档案定义行 :231、档案 :5 与 `DOCUMENT_INDEX.yaml:1249` 的"B1–B90"计数说明、`SCIENTIFIC_REFERENCES.md:49/51/53` 的"B1–B90"范围说明 ⇒ 档案正文**没有任何断言挂 [B90]**（列而不用）。真正带角色的是 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:49` G7「DR3 总览与内容清单（XP 谱、光度、星表规模）」，与该 DR3 Summary 文相符 ⇒ 关联对。

### 198 · 10.1051/aas:1996164 §2/§3
断言原文（**实际在 `docs/science/NOISE_MODEL.md:377`，批次清单给的 :369 是行号漂移**，:369 讲的是 `min_samples=64` 的导出依据，无 DOI）：「Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（**§2** 背景网格与稳健估计：k-σ 裁剪、`mode = 2.5·median − 1.5·mean`、中值滤波、双线性插值、32–128 像元网格；DOI 10.1051/aas:1996164。§3 讲的是**检测**（峰值/阈值、Lutz 单遍连通域、template frame 卷积），不是背景）」
ADS 扫描 p.394（= 文章第 2 页）逐字命中：§3 "Detection" 段「one can consider two classical detection techniques: peak finding and thresholding … SExtractor uses Lutz's one-pass algorithm (Lutz 1979) to extract 8-connected contiguous pixels from a "template frame". The template frame results from the convolution "on-the-fly" of the original image with some appropriate convolution mask」；同页 §2 段（式 (1) `mode = 2.5 × median − 1.5 × mean`、"Once the grid is set up, a median filter can be applied to it… The resulting background map is then simply a bilinear interpolation between the meshes of the grid… On most images, a width of 32 to 128 pixels works fine."）⇒ 节号与内容双对，**不是伪托节号**。

### 204 · 10.1086/116242
断言原文（`…ARCHIVE.md:15`）：「文献中「归一化到测光坐标系」= **photometric zero point（零点定标）+ color term（色项）+ spatial zero-point variation（空间依赖零点）**：选一批已知星等/已知合成通量的参考星，拟合 `m_std − m_instr = ZP + c·(color) + s(x,y)`，再把 ZP 转移到科学帧 [B1][B2][B28][B29]」
Landolt 1992 正是"一批已知 UBVRI 星等的参考星"一手来源（摘要：526 颗天赤道星、29 次测量、11.5–16.0 mag）⇒ 关联对。档案侧标签 `[V]` 不采信；题名被档案截断为 "UBVRI photometric standard stars..."（省略号），非插词。

### 210 · 10.1086/132749 —— 关联错（角色错绑）
断言原文（`docs/science/PHOTOMETRY.md:313` item 9）：「**9. 光子计数通带（`λ` 因子的文献依据）**：Bessell, M. S. 1990, PASP 102, 1181（DOI 10.1086/132749，UBVRI passbands；能量计数 vs 光子计数口径）」
该文正文（ADS 扫描，OCR 98,954 字符）：`counting` 0 命中、`integrat` 0 命中、`per unit wavelength` 0 命中；`photon` 2 处均为"CCD 荧光涂层把 UV-blue photon 转成红端 photon"（Table 6 讨论）；`energy` 1 处为"stellar energy distribution"（颜色项语境）。该文对通带的表述是「passbands used (filter transmission times detector response)」，主题是"把观测通带匹配到 Johnson-Cousins 标准系统"。⇒ **该文不做能量计数 vs 光子计数口径的比较，也不给 λ 因子**。
同仓反证：`实验/absolute-snr/docs/surveys/f-instr-survey.md:38` 自己写「λ 因子应引 **Bessell & Murphy 2012 `[F-23]`**」⇒ 同一件事两处口径互斥，把 λ 因子派给 1990 文是错绑；`docs/research/PHOTOMETRY_RESEARCH_PACK.md:77` P1「…以及能量计数 vs 光子计数口径」重复同一错绑（同源扩散）。

### 216 · 10.1086/154592 —— 角色错绑
断言原文（`…ARCHIVE.md:98`）：「PSF 拟合协方差：独立像素 `Cov(θ) = (AᵀWA)⁻¹, W = diag(1/σ_i²)` [B33]；相关像素必须 `W = C⁻¹`…[B22][B24]」
Lampton, Margon & Bowyer 1976 正文（ADS 扫描）：该文给的是 χ² 统计量性质与置信区域方法评估——「Methods for generating point estimates can, for linear models, be extended to error estimation through covariance matrix methods」、「the p-dimensional covariance matrix Vpp provides a complete description of the fitting errors. The covariance matrix for the desired parameters, Vqq, is obtained by simply deleting the unwanted rows and columns from Vpp **(Eadie et al. 1971, p. 199)**」、以及反向警告「this method must be used with great caution because its derivation explicitly depends upon model linearity … we should not in general expect a linear matrix decomposition to satisfactorily remove unwanted parameter constraints」。关键词 `second derivativ`/`inverse`/`curvature` 全文 0 命中 ⇒ 该文**不产出** `(AᵀWA)⁻¹` 闭式（构造转引 Eadie et al. 1971），且其主旨恰是该闭式在**非线性**模型下不可靠；被引用来支撑"PSF 拟合（非线性）协方差闭式"属角色错绑。摘要级证据同向："Minimization of the chi-squared statistic is recommended for testing the validity of classes of models. The chi-squared min + 1 error estimation method is evaluated and found to be unsuitable…"

### 222 · 10.1086/316632
断言原文两处：`…ARCHIVE.md:65`「欠采样（FWHM ≲ 1.5–2 px）：孔径流量分数随亚像元相位变化几 %，必须用 **ePSF** [B13]」；`实验/absolute-snr/docs/surveys/f-instr-survey.md:35`（批次清单代表位点）「Anderson & King 2006, PASP 118, 560 ｜ **未核到** ｜ 用 **Anderson & King 2000, PASP 112, 1360, DOI 10.1086/316632** 与 2003, PASP 115, 113, DOI 10.1086/345491 `[F-12]`」；`f-instr-survey.md:169` [F-12] 条目逐字题名与 Crossref 一致。
OpenAlex 摘要直接命中"undersampled / ePSF"角色 ⇒ 关联对。附注：同句里"孔径流量分数随亚像元相位变化几 %"的量化项不属该文（该文是 astrometry/PSF 导出，非孔径流量分数），属断言侧未标来源的量化，不计本 DOI 缺陷。

### 228 · 10.1086/338393 §5 —— 关联错（节号）
断言原文（`docs/science/UNCERTAINTY_AND_COVARIANCE.md:142`）：「**Drizzle 后相邻像素相关与方差低估**：Fruchter & Hook 2002, PASP 114, 144（DOI 10.1086/338393，**§5 相关噪声**）」
同一 DOI 的另一处仓内用法（`…ARCHIVE.md:103`）：「Fruchter & Hook 2002 **§7** 在 **`pixfrac = 0.6`、`scale = 0.5`** 的算例给出噪声相关比 **`R = 1.662`**」⇒ 仓内两处对同一篇给出两个节号，必有一错。
可直读版本（arXiv:astro-ph/9808087v2，题名与 PASP 版逐字同）节结构：`1. INTRODUCTION / 2. THE METHOD / 3. COSMIC RAY DETECTION / 4. IMAGE FIDELITY / 5. PHOTOMETRY / 6. ASTROMETRY / 7. NOISE IN DRIZZLED IMAGES（7.1 The Nature of the Problem、7.2 The Calculation）`；§7.1 原句「Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated.」；§7.2 原句「Using the relatively typical values of p = 0.6 and s = 0.5, one finds R = 1.662.」⇒ 相关噪声与 R=1.662 均在 §7，§5 是 PHOTOMETRY ⇒ `UNCERTAINTY_AND_COVARIANCE.md:142` 的"§5 相关噪声"判**关联错（节号）**，正确写法为 §7（7.1/7.2）。
诚实边界：正式 PASP 排印本的节号未能直读（IOP 付费墙；`articles.adsabs.harvard.edu/pdf/2002PASP..114..144F` 返回 S3 `AccessDenied`；`cdsads.u-strasbg.fr` 无响应）。预印本与正式版是否同序，缺 PASP PDF 才能钉死；但仓内自相矛盾已足以判该条至少不成立。

### 234 · 10.1086/432977 —— 角色错绑
断言原文（`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:34`，编号 1.6）：「Makovoz & Marleau 2005 (MOPEX), PASP 117, 1113. DOI 10.1086/432977 ｜ 借鉴点：tile **重叠区**对两个候选值插值并**传播插值结果的不确定度** ⇒ 本文『稀疏控制点→插值校正场』最接近的公开先例，也是『插值误差进方差预算』的可引基线」
该文正文（arXiv:astro-ph/0507007）自己把 mosaicking 让给另一篇：「Since mosaicking is part of MOPEX **(Makovoz & Khan 2004)**, point source extraction benefits from such capabilities as creating properly resampled mosaic images, cosmic ray hits masking, etc.」；§2 只说「In addition to the mosaic image there are two optional input images: the coverage map and uncertainty image. … These two images are created **when mosaicking is done with MOPEX**. … If no uncertainty images exist they can be estimated by MOPEX using the model consisting of three components – photon noise, read noise, and confusion noise.」⇒ 该文把不确定度图当**输入**消费，不描述"重叠区两候选值插值 + 插值不确定度传播"的算法；全文 `overlap` 仅 2 处（blend 拟合区重叠）、`interpolat` 3 处（PRF 双线性/双三次插值）⇒ 角色错绑到姊妹文（Makovoz & Khan 2004）。注：Makovoz & Khan 2004 的 DOI/卷页本路**未取证**，不在此给出（见"不敢判"）。

### 240 · 10.1086/664083
断言原文（`docs/science/PHOTOMETRY.md:313` item 9 同句后半）：「Bessell, M. S. & Murphy, S. 2012, PASP 124, 140（DOI 10.1086/664083，photonic passband 与零点）」；档案侧 `…ARCHIVE.md:127` 写「Bessell, M. & Murphy, S. 2012, PASP 124, 140, "Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI..." DOI 10.1086/664083 [V]」
Crossref 题名与摘要双证"photonic passbands + zero points" ⇒ 关联对。**缺陷（形态①）**：该 DOI 的出版社记录作者 given 为 `Michael`（无中名缩写），仓内 `PHOTOMETRY.md:313` 写 "Bessell, M. S."、档案写 "Bessell, M."，同仓两种署名且前者与该 DOI 记录不符（"M. S." 是 1990 单作者文的署名形态）。

### 246 · 10.1093/biomet/35.3-4.246
断言原文（`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:605`，附录 A 表 A6）：「Anscombe F. J., 1948, Biometrika 35, 246–254, DOI 10.1093/biomet/35.3-4.246 ｜ VST（公式转引自 A7）」
Crossref 题名 "THE TRANSFORMATION OF POISSON, BINOMIAL AND NEGATIVE-BINOMIAL DATA"、卷页 35/246-254/1948 全对；仓内已显式声明公式转引自 A7（不冒领原文式号）⇒ 关联对、角色诚实。

### 252 · 10.1093/mnras/stv2628
断言原文（`…ARCHIVE.md:66`）：「PSF 邻星减除后 Kepler/K2 星团暗端 **10% @ `K_P ≈ 24`**、亮端 **~30 ppm** [B74]」
arXiv:1510.09180 摘要逐字命中（见结论表引文）⇒ 关联对。年份：Crossref `issued` 2015-12-23（网络首发）vs 仓内 2016（MNRAS 456 卷期年）⇒ 不算年份漂移；档案标签 `[S]`（"以同样方式核对"）不采信，本条以 Crossref+arXiv 重跑。

### 258 · 10.1109/TIP.2010.2056693
断言原文（`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:606`，附录 A 表 A7）：「Mäkitalo M. & Foi A., 2011, IEEE TIP 20, 99–109, DOI 10.1109/TIP.2010.2056693（闭式近似：TIP 20, 2697–2698, DOI 10.1109/TIP.2011.2121085）｜ §II.A 式 (2) 泊松方差=均值；式 (4) Anscombe；精确无偏逆」
作者版全文（IEEE 双栏排版，经 Semantic Scholar `openAccessPdf` 定位）：`II. PRELIMINARIES → A. Poisson noise` 中式 (2) 即 `E{z_i|y_i} = y_i = var{z_i|y_i}`（"In addition to being the mean of the Poisson variable zi, the parameter yi is also its variance"）、式 (4) 即 Anscombe `f(z)=2√(z+3/8)`（正文回指 "the forward Anscombe transformation (4)"）、随后 `A. Exact unbiased inverse` ⇒ 节号与式号双对。作者名：Crossref/IEEE 记录为 `Makitalo`（剥变音符），全文署名 "Markku Mäkitalo" ⇒ 仓内 "Mäkitalo" 正确，非形态①命中。
未取：IEEE Xplore 正式 PDF（付费墙）⇒ 式号以作者接收版为据。

### 264 · 10.1145/368996.369025
断言原文（`docs/algorithms/PHASE2_SESSION.md:400`）：「DAG/拓扑排序：Kahn 1962, Comm. ACM 5, 558（DOI 10.1145/368996.369025）」
Crossref 题名 "Topological sorting of large networks"、CACM 5, 558-562, 1962-11、单作者 A. B. Kahn ⇒ 三项全对，角色（拓扑排序算法出处）即该文唯一主题。

## 本批 P1 缺陷清单

| # | 条 | 文件:行 | 缺陷 | 正确写法 |
|---|---|---|---|---|
| 1 | 210 | `docs/science/PHOTOMETRY.md:313`（同源扩散 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:77`） | 把"λ 因子 / 能量计数 vs 光子计数口径"派给 Bessell 1990（正文 `counting` 0 命中、通带表述为 filter×detector response），角色错绑 | λ 因子/photonic 口径只挂 Bessell & Murphy 2012（DOI 10.1086/664083，题名即 Photonic Passbands）；Bessell 1990 的角色改为"UBVRI 通带表征与标准系统匹配"。与本仓 `f-instr-survey.md:38` 口径统一 |
| 2 | 216 | `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:98` | 把闭式 `Cov(θ)=(AᵀWA)⁻¹` 派给 Lampton+1976；该文转引 Eadie et al. 1971 且明确警告该法依赖模型线性 | 闭式改引线性最小二乘/GLS 教科书出处（或本仓已登记的 Aitken 1935 / Plackett 1949 一系）；LMB76 只保留"χ² 拟合与误差区域方法评估"的角色 |
| 3 | 228 | `docs/science/UNCERTAINTY_AND_COVARIANCE.md:142` | 节号错：写"§5 相关噪声"，该文相关噪声在 §7（§5 是 PHOTOMETRY）；且与同仓 `ARCHIVE.md:103` 的"§7 + R=1.662"自相矛盾 | 改 `§7（7.1/7.2）`；若要以正式版为准，需 PASP 排印本钉节后统一两处 |
| 4 | 234 | `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:34`（1.6 行） | 角色错绑：把"tile 重叠区插值 + 插值不确定度传播"派给 Makovoz & Marleau 2005（点源提取文），该文自己把 mosaicking 转引 Makovoz & Khan 2004 | 该借鉴点改挂 mosaicking 姊妹文（标识符需另行取证，本批未取）；Makovoz & Marleau 2005 只保留"检测+PRF 拟合+不确定度图作为输入"的角色 |
| 5 | 240 | `docs/science/PHOTOMETRY.md:313` | 作者缩写与出版社记录不符（形态①）：该 DOI 记录 given=`Michael`，仓内写 "Bessell, M. S."；同仓 `ARCHIVE.md:127` 又写 "Bessell, M." | 统一为 "Bessell, M. & Murphy, S."（与 2012 文署名一致），"M. S." 只用于 1990 单作者文 |
| 6 | 192 | `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:231` | 列而不用：`[B90]` 在档案内无任何断言挂载（全仓仅定义行与"B1–B90"计数说明） | 要么在正文断言处挂载（DR3 内容清单类断言），要么按档案 §2 规则降级为登记项；不算书目错，属引用治理 |
| 7 | 198 | 批次清单 `inventory/CIT-08.txt` 第 5 行 | 清单侧代表位点行号漂移：`NOISE_MODEL.md:369` 实为 `:377`（:369 讲 `min_samples=64`，无 DOI） | 前台回写台账时以 `:377` 为准；文献本身三项全对 |

## 我不敢判的 / 缺什么才能判

- **228 的"正式版节号"**：只有 astro-ph/9808087v2 可直读，正式 PASP 114,144 排印本节号未证（IOP 付费墙、ADS 扫描 403、cdsads 无响应）。判"§5 错"的证据是预印本 + 仓内自相矛盾；**要钉死需 PASP PDF**（订阅或 STScI 镜像）。
- **258 的式号**：以作者接收版（IEEE 双栏）核对，未取 IEEE Xplore 正式 PDF。**需要** IEEE 正式版才能排除"预印本与正式版式号漂移"。
- **210 的判词强度**：依据是 1990 年扫描版的 OCR 全文（98,954 字符）关键词 0 命中 + 摘要；若正文以"quantum efficiency / response per unit wavelength"等不同措辞隐含同一口径，判词需从"关联错"软化为"该文不以此为主张"。**需要**可全文检索的 IOP 电子文本。
- **216 的公式细节**：OCR 把公式打乱（`Vpp`/`Vqq` 可读、矩阵符号不可读），`(AᵀWA)⁻¹` 若以矩阵式印出可能未被关键词命中；但"转引 Eadie et al. 1971"与"依赖模型线性"两处文字证据独立成立，故角色错绑的判词不依赖 OCR 公式。**需要** ApJ 1976 电子版原文可完全钉死 §V 的 V_pp 定义式。
- **234 的正确改引对象**：判"应挂 mosaicking 姊妹文"，但 Makovoz & Khan 2004 的 DOI/卷页本路**未取证**，不给（避免编造）；下一轮需先取该文标识符。
- **192 是否算缺陷**：若负责人口径是"参考档案允许只登记不引用"，则 P1-6 应降为 P3/不计；需要负责人一句话。
- **未在本批范围但同句出现**：`f-instr-survey.md:35` 的 DOI 10.1086/345491（Anderson & King 2003）、`EXP-06:606` 的 DOI 10.1109/TIP.2011.2121085（闭式近似）——均不在 CIT-08 清单内，本批未核。

## 本批三态计数

- **存在性**：已核 14 ／ 不存在 0 ／ UNPROVEN 0
- **关联**：关联对 9（186、198、204、222、240、246、252、258、264）／ 关联错 2（210、228）／ 角色错绑 2（216、234）／ 无断言挂载（列而不用）1（192）= 14
  （240 记"关联对"但带作者署名缺陷，见 P1-5；192 的关联按 `PHOTOMETRY_RESEARCH_PACK.md:49` 判对、按档案判"未挂载"）
- **版本/载体**：版本对 14 ／ 版本错 0 ／ 未钉版次 0（其中 228、258 的"可直读载体"是预印本/作者版，已在各条注明边界）

## UNPROVEN 清单

- 存在性与版次层面：**0 条 UNPROVEN**（14 条 DOI 全部经 Crossref 原始回包命中，192/228/234/252 另有 arXiv 交叉）。
- 关联层面的"未完全钉死"（非 UNPROVEN 结论，属边界登记）：228（正式 PASP 版节号）、258（IEEE 正式版式号）、210（OCR 措辞覆盖度）、216（§V 矩阵式原文）。走过的途径见各条与文件头手段清单。

## 新发现的缺陷形态（本批独有）

1. **同仓两文档对同一文献给出不同节号**（228：`UNCERTAINTY_AND_COVARIANCE.md` 说 §5、`ARCHIVE.md` 说 §7 并附该节独有的 R=1.662 数值）——单条审计看不出，跨条比对才暴露；建议门禁做"同一 DOI 的节号一致性"检查。
2. **姊妹文错绑**（234）：把工作绑到同系列论文中"题名更像"的那一篇，而**该文正文自己把该工作转引给另一篇**（"Since mosaicking is part of MOPEX (Makovoz & Khan 2004)"）——判据是文内自证，成本低于外部取证。
3. **同一角色在同仓两处口径互斥**（210 vs `f-instr-survey.md:38`）：λ 因子一处挂 Bessell 1990、另一处明写"应引 Bessell & Murphy 2012"——survey 已订正的结论没有回灌到 `docs/science/` 正本，属"整改单向传导失败"。
4. **作者缩写跨同一作者两篇论文串写**（240）：把 1990 文的署名形态 "M. S." 搬到 2012 文（出版社记录 given=`Michael`），与已知形态①（首字母与出版社记录不符）同源但触发机制不同——**同一人不同文的署名漂移**，不是随机编造。
5. **量化项与 DOI 角色分离**（222 附注）：DOI 支撑"必须用 ePSF"，但同句"几 %"的量化无来源——判"关联对"时须把量化项单独登记来源，否则 DOI 替量化项背书。

## 覆盖率自报

14/14 = 100%（每条三字段齐全；14 条断言原文全部打开；网络取证每条 ≤5 次，均在 6 次上限内；ADS 网关 504 重试按前言 §3 与派单约定不计配额）。


<!-- PROGRESS: 14/14 -->
