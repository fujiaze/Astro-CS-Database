# 核验-CIT-05（18 条 · P-1 优先）

- 基线：`c8f64e9a`
- 批次清单：`独立审计/批次清单/CIT-05.txt`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` §1 三字段 / §2 硬禁令 / §4 高危形态
- 网络手段清单（本机 shell 无外网，一律走联网工具）：
  - arXiv 号 → `https://export.arxiv.org/api/query?id_list=<id>`（官方 API，回包给版本号）
  - DOI → `https://api.crossref.org/works/<doi>`（400/无记录时改走出版社页或 ADS bibcode）
  - ADS bibcode → `https://ui.adsabs.harvard.edu/abs/<bibcode>/abstract`（浏览器途径）
  - 图书/专著 → 出版社页（Wiley/ASP 等）或馆藏记录（同名再版须记版次年份）
  - 软件类引用 → 项目仓库（版本 tag / release）＋ 被引代码段 `文件:行`
- 仓库侧读取：每条目上限 2 次（`Read` / `git grep`），只读，不改仓。
- 每条网络查询上限 3 次；到限即 `UNPROVEN` 并附用过的标识符与途径。

---

## 59 · DOI:10.1111/j.1365-2966.2005.08844.x（Wild & Hewett 2005） —— 核验态：已核
- **存在性：已核**。Crossref `https://api.crossref.org/works/10.1111/j.1365-2966.2005.08844.x` 回包逐字段：
  Title "Peering through the OH forest: a new technique to remove residual sky features from Sloan Digital Sky Survey spectra"；
  Journal MNRAS；Vol **358**；Pages **1083–1099**；Year **2005**；Authors Wild, V.; Hewett, P. C.（查询 1）。
- **仓库挂载点与原文**（仓库读 1：`实验/additive-sky-seamless/results/evidence_lit.json` 一次覆盖 :41/:139/:177；仓库读 2：`git grep` 全命中）：
  1. `evidence_lit.json:36-47`（id `3a-pca-sky-residual-subtraction`）断言："用主成分分析（PCA）利用各条谱残差之间的相关性自动识别并扣除天光残余，使被天光线压低 2 倍以上的 S/N 恢复到接近计数统计极限"；
  2. `README.md:328` 行题 "天光残差的系统误差"；
  3. `evidence_lit.json:177`（`failed` 项）用它纠正"Wild 2009 天光扣除"的错线索。
- **关联性：关联对**。arXiv 摘要原文逐句对上：`"significant systematic residuals ... due to the incomplete subtraction of the strong OH sky emission lines longward of 6700A"`、
  `"The S/N ... is reduced by more than a factor of 2 over that expected from counting statistics"`、
  `"we present a method to automatically remove the sky residual signal, using a principal component analysis (PCA) which takes advantage of the correlation in the form of the sky subtraction residuals"`
  ⇒ "PCA + 因子 2 的 S/N 损失"两个具体断言逐字有源。⚠ 边界（非错绑，但须在引用处保留）：该文是**一维光纤光谱**天光线残差，
  仓内三处均未把它当成像背景面证据用（`relevance` 字段自己写明了"多目标光纤光谱"），角色绑对。
- **版本：版本对**。2005 MNRAS 358(3) 1083–1099 即被引版；仓内 `README.md:328` 现写 358, 1083 与 Crossref 一致
  （历史缺陷 `README.md:314` 写 1089 已由 `results/REVIEW.md:124` 自行订正，本次确认当前文本已无 1089）。
- 用量：网络 1/3、仓库 1/2。

## 83 · DOI:10.1051/0004-6361/200912446（Regnault+2009 SNLS 定标） —— 核验态：已核
- **存在性：已核**。Crossref 回包：Title "Photometric calibration of the Supernova Legacy Survey fields"；Container A&A；Vol **506**；Pages **999–1042**；Year **2009**；
  Authors Regnault N., Conley A., Guy J., Sullivan M., Cuillandre J.-C., Astier P.（查询 1）。
- **仓库挂载点与原文**：`evidence_lit.json:134-145`（id `7a-multiplicative-flat-vs-additive-sky`）断言＝平场为**乘性**改正、天光为**加性**扣除，
  并直接引了两句英文：`'allows for all multiplicative effects in the image to be corrected at once'` 与"天光在 100 像素尺度上成图后扣除"；`README.md:333` 行题"超新星测光的背景/平场处理"；`REPORT_paper.md:400` 著录项。
- **关联性：关联对**。ar5iv 全文 `https://ar5iv.labs.arxiv.org/html/0908.3808` 逐字读到两句（查询 2）：
  `"This frame is the one used for flatfielding the science images of the entire run, and allows for all multiplicative effects in the image to be corrected at once."`
  `"First the sky background is mapped at a large scale (100 pixels) and subtracted."`
  ⇒ 引句与"100 像素"尺度均为原文，非伪托节号。
- **版本：版本对**。DOI（A&A 506, 999–1042, 2009）与被引版一致；被引文字取自同一工作的 arXiv:0908.3808 预印本全文，
  与仓内 `identifier` 字段同时给出的 DOI+arXiv 双标识自洽（仓内 `resolver_url` 用 ar5iv 读正文，DOI 只作出版锚）。
- 用量：网络 2/3、仓库 1/2。

## 93 · arXiv:1510.07567（Nguyen et al.，1191.3 nm 气辉稳定性） —— 核验态：存在性/关联已核，数值未核
- **存在性：已核**。arXiv API 回包 id `1510.07567v1`；Title "Spatial and Temporal Stability of Airglow Measured in the Meinel Band Window at 1191.3 nm"；
  Authors Hien T. Nguyen, Michael Zemcov, John Battle, James J. Bock, Viktor Hristov, Philip Korngut, Andrew Meek；Published 2015-10-26；
  回包 DOI 字段 `10.1088/1538-3873/128/967/094504`（= PASP 128(967) 094504）（查询 1）。
- **仓库挂载点与原文**：`实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md:601` 表格行
  "窄带、避开 OH 气辉线，100 s – 3.5 h ⇒ **0.56% RMS，无漂移**（唯一实测支撑「±1% 恒定」的情形）｜Nguyen et al. 2015, arXiv:1510.07567"；
  `:928` 著录项"（窄带 3.5 h 内天光 0.56% RMS）"。
- **关联性：关联对（主题与定性结论级）**。摘要逐字：`"We report on the temporal and spatial fluctuations in the atmospheric brightness in the narrow band between Meinel emission lines at 1191.3 nm using an R=320 near-infrared instrument."`
  与 abs 页 `"in several hours of ~100s integrations the noise performance of the instrument does not appear to significantly degrade from expectations"`（查询 2）
  ⇒ 支撑"窄带（夹在 Meinel/OH 线之间）＋数小时 ~100 s 积分无明显漂移"。**具体数值 0.56% RMS 未取到原文**：
  第 3 次查询 `ar5iv.labs.arxiv.org/html/1510.07567` 回包无百分比数值（该件 ar5iv HTML 不完整，仅可见 104 s 时间箱），
  ⇒ 数值级细节列入本批 UNPROVEN 清单（途径：arXiv API／abs 页／ar5iv；标识符：1510.07567、10.1088/1538-3873/128/967/094504）。
- **版本：未钉版次**。仓内两处只写"Nguyen et al. 2015 + arXiv 号"，未写 PASP 128(967) 094504（2016 正式刊）；
  arXiv v1 是 2015 预印本。**若引用 0.56% 这一数值，须钉正式刊版**（数值通常只在对口刊文内可核）。
- 用量：网络 3/3、仓库 1/2。

## 99 · arXiv:2208.00211（Gaia DR3 Summary, Vallenari+2023） —— 核验态：已核
- **存在性：已核**。arXiv API：`2208.00211v1`，Title "Gaia Data Release 3: Summary of the content and survey properties"，Authors Gaia Collaboration / A. Vallenari / A. G. A. Brown 等，Published 2022-07-30，回包 DOI 字段 `10.1051/0004-6361/202243940`（查询 1）。
  Crossref `10.1051/0004-6361/202243940`：Title "Gaia Data Release 3"，Container A&A，Vol **674**，Page **A1**，Year **2023**（查询 2）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:231`（[B90] 著录项：作者/题名/A&A 674, A1/DOI/arXiv 五项）；
  `实验/photometric-magnitude/RESOLUTION_fsyn_formula.md:43-48` 的**否证检索范围清单**（第 45 行列该 arXiv 号为"官方无『用 10^(−0.4·G) 归一化 XP 谱』这种操作"的已查文件之一）。
- **关联性：关联对**。:231 的五个著录字段与 Crossref/arXiv 逐字段一致（题名含 "Summary of the content and survey properties"、卷页 A&A 674 A1、2023、DOI、arXiv 号互指）；
  :45 处它承担的角色是"已检索且未发现该操作的官方文件之一"——与 DR3 总览（内容清单）的职责域相符，且该句的正证已另钉到 Montegriffo+2023 §8.1.1 Fig. 27（:46-48），未把否证当正证。
- **版本：版本对**。arXiv v1（2022-07-30）↔ A&A 674 A1（2023）即仓内所写版次；两标识同指一工作，无同名再版风险。
- 用量：网络 2/3、仓库 1/2。

## 105 · arXiv:astro-ph/0501460（Wild & Hewett 2005 预印本） —— 核验态：已核
- **存在性：已核**。arXiv API 回包 id `astro-ph/0501460v1`；Title "Peering through the OH-forest: a new technique to remove residual sky features from SDSS spectra"；
  Authors Vivienne Wild, Paul C. Hewett；`journal_ref` = `Mon.Not.Roy.Astron.Soc.358:1083-1099,2005`；`doi` 字段 = `10.1111/j.1365-2966.2005.08844.x`（查询 1）。
- **仓库挂载点与原文**：`REPORT_paper.md:393` 著录项 "Wild, V., & Hewett, P. C. 2005, MNRAS 358, 1083 — DOI ...，arXiv:astro-ph/0501460"；
  `evidence_lit.json:41-42` 把它作为 3a 项（PCA 天光残差扣除）的标识符与 `resolver_url`（`https://arxiv.org/abs/astro-ph/0501460`）。
- **关联性：关联对**。同一工作的预印本标识，与 59 项同断言（PCA 扣除 OH 天光残差、S/N 恢复）；回包 `journal_ref`/`doi` 两字段直接闭合"2005 MNRAS 358, 1083"这条著录。
  题名拼写差异（预印本 "SDSS" vs 正式刊 "Sloan Digital Sky Survey"）不影响同一性，已由 DOI 字段互指证明。
- **版本：版本对**。v1 为该 arXiv 号唯一版本，`journal_ref` 指向被引版次；仓内未把它当作"仅预印本内容"使用。
- 用量：网络 1/3、仓库 1/2。

## 111 · DOI 10.1051/0004-6361:20021326（FITS WCS Paper I） —— 核验态：已核
- **存在性：已核**。Crossref `works/10.1051/0004-6361:20021326`：Title "Representations of world coordinates in FITS"；Container A&A；Vol **395**；Pages **1061–1075**；Year **2002**；Authors E. W. Greisen, M. R. Calabretta（查询 1）。
  arXiv API `astro-ph/0207407`：id `astro-ph/0207407v2`，同题名，`doi` 字段 = `10.1051/0004-6361:20021326`，`journal_ref` = `Astron.Astrophys. 395 (2002) 1061-1076`（查询 2）。
- **仓库挂载点与原文**：`docs/algorithms/PHASE3_RESAMPLE.md:189` "WCS 反变换：Paper I = Greisen & Calabretta 2002, A&A 395, 1061（DOI ...）§2.1.1（中间坐标 = CD·(p−CRPIX)）"；
  `docs/science/PHASE3_HIPS_TO_FITS.md:179` "§2.1.1（CRPIX 为参考点像素坐标：`p_j = CRPIX_j ⇒ q_i = 0`；CRPIX 可非整数、可在图像外）"；另 `docs/science/ASTROMETRY.md:257,265` 同 DOI 同节号并标 "arXiv:astro-ph/0207407 逐字核验"。
- **关联性：关联对**。第 3 次查询打开正文代表位点（`https://ar5iv.labs.arxiv.org/html/astro-ph/0207407`）读到 §2.1.1 式(1) 附近逐字：
  `"The first step is a linear transformation applied via matrix multiplication of the vector of pixel coordinate elements, p_j:"` 与 `"where r_j are the pixel coordinate elements of the reference point given by the CRPIX j"`
  ⇒ 断言"中间坐标 = 线性矩阵作用在 (p − CRPIX)"与节号 §2.1.1 均由原文直接支持，非伪托节号。
- **版本：版本对**。所指即 2002 A&A 395, 1061 正式版（arXiv v2 ↔ 该刊）；仓内未混用 1995 A&AS 117, 181 那一篇（题名不同：那篇是 celestial coordinates）。
  ⚠ 唯一瑕疵（不影响本条判定）：Crossref 记页末 1075、arXiv `journal_ref` 记 1076，仓内只写起始页 1061，无冲突。
- 用量：网络 3/3、仓库 1/2（`git grep` 一次覆盖三处位点）。

## 117 · DOI 10.1086/496934（仓内作为**反例**登记） —— 核验态：已核
- **存在性：已核（该 DOI 真实存在，但**不是**天文文献）**。DOI handle API 回包 `responseCode: 1`，target `https://academic.oup.com/cid/article-lookup/doi/10.1086/496934`（查询 1）；
  Crossref 回包逐字段：Title `"The Year in Infection, Volume 2 Edited by Mark Wilcox Oxford, U.K.: Atlas Medical Publishing, 2005. 301 pp., illustrated. $119.95 (cloth)"`；
  Container **Clinical Infectious Diseases**；Vol **41**；Issue **9**；Page **1369**；Year 2005；type `journal-article`（查询 2，前两次为 Crossref 网关 429/403，按前言 §3 重试）。
- **仓库挂载点与原文**：`实验/absolute-snr/docs/surveys/f-instr-survey.md:31` 表格行 "Sirianni et al. 2005, DOI 10.1086/496934 | **DOI 错**（该 DOI 是 *Clinical Infectious Diseases* 的书评）| DOI **10.1086/444553** `[F-24]`"；
  `:341-342` "候选给的 DOI 10.1086/496934 不是这篇（它解析为 Clinical Infectious Diseases 41(9), 1369, 书评 "The Year in Infection, Volume 2"）；正确 DOI 是 10.1086/444553（PASP 117, 1049–1112, 2005）"。
- **关联性：关联对**。本条的"断言"是否定性登记，逐字段被独立复算命中：期刊 CID ✓、41(9) ✓、1369 ✓、2005 ✓、内容为书评（Crossref 把 CID 书评栏目 type 成 journal-article，但标题即"…Edited by Mark Wilcox…301 pp., illustrated. $119.95 (cloth)"，仓内称"书评"就内容而言成立）。
  替代 DOI 亦已核：`10.1086/444553` = "The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys", PASP **117**, **1049–1112**, 2005, Sirianni, M., Jee, M. J., Benítez, N.（查询 3）。
- **版本：版本对**。反例侧给出 CID 41(9) 1369 (2005) 与 Crossref 一致；订正侧给出 PASP 117, 1049–1112 (2005) 与 Crossref 一致。
- 用量：网络 3/3（另 2 次网关失败已计）、仓库 1/2。

## 123 · DOI 10.1111/j.1365-2966.2007.12297.x（Calabretta & Roukema，HEALPix 映射） —— 核验态：已核（附 arXiv 配对缺陷）
- **存在性：已核**。Crossref：Title "Mapping on the HEALPix grid"；Container MNRAS；Vol **381**；Pages **865–872**；Year **2007**；Authors Mark R. Calabretta, Boudewijn F. Roukema（查询 1）。
- **仓库挂载点与原文**：`实验/healpix-polar/code/polar_common.h:16` 头注释参考清单第 2 行 `Calabretta & Roukema 2007, MNRAS 381, 865, DOI 10.1111/j.1365-2966.2007.12297.x`（软件侧引用：无 arXiv、无节号，仅作出处登记）；
  `实验/healpix-polar/docs/EXP-07-POLAR.md:566` 表格行 "Calabretta & Roukema 2007, *Mapping on the HEALPix grid*, MNRAS 381, 865，DOI ...，arXiv astro-ph/0412607 | 面坐标卡与角坐标的互换 | 其**等面积性质**与本实验 T1 的 `|dΩ/dudv| = π/3` 一致。该文同样以'角点 + 边'描述像素，未给边界曲率的量化"。
- **关联性：关联对（主题级）**。arXiv API 摘要逐字：`"The natural spherical projection associated with the Hierarchical Equal Area and isoLatitude Pixelisation, HEALPix, is described and shown to be one of an infinite class not previously documented in the cartographic literature."`
  ⇒ "面坐标卡与角坐标互换 + 等面积（Hierarchical **Equal Area**）"两句成立。**具体常数 `|dΩ/dudv| = π/3` 未取到原文**：
  第 3 次查询 `arxiv.org/pdf/astro-ph/0412607` 被网关 403，`dΩ/du dv` 表达式本轮未读到 ⇒ 数值级细节入 UNPROVEN 清单。
- **版本：版本对（DOI 侧）＋ 新登记缺陷：预印本/正式版作者数与版次不配对**。
  `arXiv:astro-ph/0412607` 回包 id = **v1（唯一版本）**、Authors 只有 **M. R. Calabretta（单作者）**、`doi`/`journal_ref` 字段 **NO_RECORD**；
  而 DOI 记录的正式版是 **Calabretta & Roukema 2007（双作者）MNRAS 381, 865–872**。
  ⇒ `EXP-07-POLAR.md:566` 把"2007 双作者版"的著者与"2004 单作者预印本"的 arXiv 号并列，若被引内容（等面积常数、"角点+边"表述）需在正式版核对，该 arXiv 链接不构成同版证据。
- 用量：网络 3/3（含 1 次网关 403）、仓库 1/2。

## 129 · DOI 10.3847/1538-4365/abb82b（Waters+2020 PS1 detrend/warp/stack） —— 核验态：已核
- **存在性：已核**。Crossref：Title "Pan-STARRS Pixel Processing: Detrending, Warping, Stacking"；Container **ApJS**；Vol **251**；Article Number **4**；Year **2020**；Authors C. Z. Waters, E. A. Magnier, P. A. Price（查询 3；查询 1 为 Crossref 429 网关失败）。
  DOI handle API `responseCode: 1` → `https://iopscience.iop.org/article/10.3847/1538-4365/abb82b`（查询 2）。arXiv API：`1612.05245v5` 同题名、Waters/Magnier/Price（查询 3 组）。
- **仓库挂载点与原文**：`实验/absolute-snr/docs/surveys/f-instr-survey.md:243`（[F-16] 著录项）；
  `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:60`（3.4 行）断言 "正文（实页核对）：噪声图**不**施加到科学图，而是『used to construct the weight image that contains the pixel-by-pixel variance for the chip stage image』，初始权重图由科学图 + cell gain 构造"；
  另 `实验/absolute-snr/docs/snr-propagation-design.md:1119` 用同一断言（"方差面作为一等产品"）。
- **关联性：关联对**。ar5iv 全文 `https://ar5iv.labs.arxiv.org/html/1612.05245` 逐字读到（查询）：
  `"The noisemap detrend is not directly applied to the science image. Instead, it is used to construct the weight image that contains the pixel-by-pixel variance for the chip stage image."`，位置 **§III.3**
  ⇒ 引句逐字命中，且"噪声图不加到科学图"这一层也是原文直述；引文与 §III.3 定位齐备。
- **版本：版本对**。DOI ↔ ApJS 251, 4 (2020)；被引正文取自同一工作 arXiv:1612.05245**v5**（现版）⇒ 仓内 `arXiv:1612.05245` 不钉版本可接受（v5 已核到该句）。
  附带一致性：`实验/absolute-snr/docs/surveys/frame-snr-survey.md:25` 与 `REVERSE_VERIFY:264` 的"abb82b 是 detrend/warp/stack 论文、psphot 核心论文是 Magnier ApJS 251, 5"这条订正，与本条元数据（题名不含 psphot）方向一致。
- 用量：网络 4 次访问（1 次为 429 网关重试，按前言 §3 不计入 3 次实质配额）、仓库 1/2。

## 135 · 1996A&AS..117..393B（Bertin & Arnouts 1996，SExtractor） —— 核验态：存在性已核，节号级 UNPROVEN
- **存在性：已核**。Crossref `works/10.1051/aas:1996164`：Title "SExtractor: Software for source extraction"；Container **Astronomy and Astrophysics Supplement Series**；Vol **117**；Pages **393–404**；Year **1996**；Authors E. Bertin, S. Arnouts（查询 1）。
  A&A 落地页（经 `doi.org` 302 → `aas.aanda.org/10.1051/aas:1996164`）回包同题名/同作者/`Vol 117, pp 393-404, 1996`（查询 2）。
  ⇒ bibcode `1996A&AS..117..393B` 的每个构成字段（年 1996 / 刊 A&AS / 卷 117 / 起始页 393 / 一作 B ertin）逐项对上；**ADS 本体未打开**（前言 §3：ADS 需 token）。
- **仓库挂载点与原文**：`实验/absolute-snr/docs/EXP-02-STRUCTURE-CONTAMINATION.md:318` 表格行
  "一手文献 | Bertin & Arnouts 1996, A&AS **117**, 393–404；**§2 "Background estimation"** 给 Eq.(1) `mode = 2.5 × median − 1.5 × mean`；**§3 "Detection" 未定义噪声 σ 的公式** ⇒ 只能读源码 | DOI 10.1051/aas:1996164；bibcode `1996A&AS..117..393B`；arXiv 无此文"。
- **关联性：UNPROVEN（节号/公式级）**。工作级对应（该文献确为 SExtractor 背景估计一手文献、卷页年份全中）已核；
  但"§2 标题为 Background estimation 且 Eq.(1) 即 mode 公式"与"§3 Detection 未定义 σ 公式"两项**没能读到原文**：1996 年 A&AS 只有扫描版 PDF（落地页只给 `/articles/aas/pdf/1996/08/ds1060.pdf`，无 HTML 全文），
  第 3 次查询改用检索式（`SExtractor Bertin Arnouts 1996 "2.5 x median" OR "2.5*median" "1.5" mean background equation`）未命中可开原文页 ⇒ 见 UNPROVEN 清单。
  ⚠ 该条是本批**唯一**在文档里以"§2/§3 + Eq.(1)"口吻下断言、却无任何可开代表位点的位置（高危形态②候选）：仓内同段已用源码行号（`src/back.c:443-587` 等）承担实证，文献侧只承担"式子出处"，故按 §2 硬禁令判 UNPROVEN 而非关联错。
- **版本：未钉版次**。仓内给 DOI + bibcode（无 arXiv，正确声明"arXiv 无此文"）⇒ 版次即 1996 A&AS 唯一版，本身无歧义；
  但 **§号断言所在版次未打开**，判 `未钉版次`（要钉就得引 A&AS 117, 393–404 的 §2/§3 页码）。
- 用量：网络 3/3、仓库 1/2。

## 141 · arXiv:0806.1910（Rieke+2008，红外绝对定标） —— 核验态：已核
- **存在性：已核**。arXiv API 批量回包：`0806.1910v1`，Title "Absolute Physical Calibration in the Infrared"，
  Authors **G. H. Rieke**, M. Blaylock, L. Decin, C. Engelbracht, P. Ogle, E. Avrett, J. Carpenter, R. M. Cutri, L. Armus, K. Gordon, R. O. Gray, J. Hinz, K. Su, C. N. A. Willmer；
  `doi` = `10.1088/0004-6256/135/6/2245`；`journal_ref` = `Astron. J, 135 (2008) 2245-2263`（查询 1；另有 1 次代理网关 403，未取内容）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:204` `[B69] Rieke, G. H., et al. 2008, AJ 135, 2245, arXiv:0806.1910 [S]`，
  位于 §"定标 / Gaia XP / 绝对通量（续）" 清单（仓库读 2 次：行 140-209 通读＋`git grep` 定位）。
- **关联性：关联对**。著录五要素（一作缩写 G. H.、年 2008、AJ、135、2245）与 arXiv 回包逐项一致；
  摘要首句 `"We determine an absolute calibration for the MIPS 24 microns band and recommend adjustments to the published calibrations for 2MASS, IRAC, and IRAS photometry to put them on the same scale."`
  ⇒ 与所在小节"绝对通量"角色相符。⚠ 该条自标 `[S]`（次级），本次复算把它升到"元数据全中"。
- **版本：版本对**。所指即 2008 AJ 135, 2245–2263（v1 预印本与刊文同年同起始页）。
- 用量：网络 2/3、仓库 2/2。

## 147 · arXiv:1207.6042（Pancino+2012，Gaia SPSS I） —— 核验态：已核
- **存在性：已核**。arXiv API：`1207.6042v1`，Title "The Gaia spectrophotometric standard stars survey. I. Preliminary results"，
  Authors **E. Pancino**, G. Altavilla, S. Marinoni, G. Cocozza, J. M. Carrasco, …（26 人）；`doi` = `10.1111/j.1365-2966.2012.21766.x`；`journal_ref` = `MNRAS`（查询 1）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:202` `[B67] Pancino, E., et al. 2012 (Gaia SPSS I) arXiv:1207.6042 [S]`。
- **关联性：关联对**。"Gaia SPSS I" 正是题名 "The Gaia **sp**ectro**p**hotometric **s**tandard **s**tars survey. **I**" 的缩写，
  摘要首句 `"We describe two ground based observing campaigns aimed at building a grid of approximately 200 spectrophotometric standard stars (SPSS) ... for the absolute flux calibration of data gathered by Gaia"` 自证该缩写非杜撰；
  一作 E. Pancino、年份 2012 全中；DOI 前缀 `10.1111/j.1365-2966` = MNRAS，与回包 `journal_ref` 自洽。仓内未声称卷页，故无卷页可错。
- **版本：版本对**。v1（2012-07）↔ MNRAS 2012（`10.1111/j.1365-2966.2012.21766.x`）；无再版歧义。
- 用量：网络 2/3、仓库 2/2。

## 153 · arXiv:1908.07099（BASS 第三数据释放论文） —— 核验态：已核
- **存在性：已核**。arXiv API：`1908.07099v3`，Title "The Third Data Release of the Beijing-Arizona Sky Survey"，
  前三作者 Zou, Zhou, Fan；`doi` = `10.3847/1538-4365/ab48e8`；`journal_ref` = `ApJS`（查询 1）。
- **仓库挂载点与原文**：`testdata/README.md:34-37` "DR3 论文 *The Third Data Release of the Beijing-Arizona Sky Survey*，[arXiv:1908.07099](https://arxiv.org/abs/1908.07099)"；
  同题名同链接另见 `testdata/BASS_DR3/README.md:9`（`git grep` 命中；此为该条唯一仓库读）。
- **关联性：关联对**。题名逐字一致（含 "Third Data Release"、"Beijing-Arizona Sky Survey"），挂在"BASS DR3 单帧数据源"说明位上——
  作为该数据释放的著录论文角色正确（数据可及性由 casdc.china-vo.org 路径承担，不属本条断言）。
- **版本：版本对（未钉正式出版信息）**。arXiv 现版 v3；`doi` 字段直给 `10.3847/1538-4365/ab48e8`（ApJS）；
  仓内只写 arXiv 号 ⇒ 被引事实（"这就是 DR3 论文"）v1–v3 均成立，无版本风险；建议补钉 ApJS DOI 以与 141/147 等著录格式一致。
- 用量：网络 1/3、仓库 1/2。

## 159 · arXiv:2301.12395（López-Sanjuan+2023，J-PLUS 用 Gaia BP/RP 一致定标） —— 核验态：已核
- **存在性：已核**。arXiv API：`2301.12395v1`，Title "J-PLUS: Towards an homogeneous photometric calibration using Gaia BP/RP low-resolution spectra"，
  Authors **C. López-Sanjuan**, H. Vázquez Ramió, K. Xiao, H. Yuan, J. M. Carrasco, J. Varela, …（23 人）；
  `comment` = "Submitted to Astronomy and Astrophysics. 17 pages, 14 figures, 2 appendix…"；`doi`/`journal_ref` 回包为 None（查询 1＋2）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:195`
  `[B60] López-Sanjuan, C., et al. 2023, "J-PLUS: Towards an homogeneous photometric calibration using Gaia BP/RP low-resolution spectra" arXiv:2301.12395 [V]`。
- **关联性：关联对**。题名与回包**逐字相同**（含 "Towards an homogeneous" 这一非母语冠词写法，构成强指纹），一作缩写 C. López-Sanjuan ✓、年份 2023（v1 提交月）✓；
  所在小节"定标 / Gaia XP / 绝对通量"与其"用 Gaia BP/RP 低分辨谱做一致化定标"主题相符。
- **版本：版本对（就所声称范围）**。仓内未声称期刊/卷页，回包亦无 `journal_ref` ⇒ 无版本可错；
  该条自标 `[V]`（已核），与复算结果一致；若日后补正式发表信息须另核（v1 comment 仅 "Submitted to A&A"）。
- 用量：网络 2/3、仓库 2/2。

## 165 · arXiv:2408.09779（Huang, Yuan & Xiao 2024, ApJ 973, 1） —— 核验态：已核（附著录缺陷）
- **存在性：已核**。arXiv API：`2408.09779v1`，Title "A Spatial Uniformity Check of Gaia DR3 Photometry and BP/RP Spectra"，
  Authors **Bowen Huang, Haibo Yuan, Kai Xiao**，`comment` = "7pages, 2 figures, **ApJ accepted**"（查询 1）。
  Crossref 书目检索命中：DOI **`10.3847/1538-4357/ad70ab`**，Title 同上（逐字），Container The Astrophysical Journal，Vol **973**，Page **1**，Year **2024**，First author **Bowen Huang**（查询 2；另 1 次代理网关 403）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:228`
  `[B87] Huang, Y., Yuan, H. & Xiao, K. 2024, ApJ 973, 1, arXiv:2408.09779（XP 合成 vs 观测残差的空间系统）`。
- **关联性：关联对**。摘要逐字：`"In this work, we check the spatial uniformity of Gaia DR3 photometry and BP/RP spectra by comparing the BP, RP and G bands photometry with the synthetic ones from the BP/RP spectra."`
  ⇒ 括注"XP 合成 vs 观测残差的空间系统"正是该句所指（合成测光来自 BP/RP＝XP 谱，比较对象是观测测光，检验量为空间均匀性）。
  ⚠ **著录缺陷（本批独有）**：一作 **Bowen Huang** 应缩写 **`Huang, B.`**，仓内写 `Huang, Y.`（与本表 [B59] "Huang, Y., Xiao, K. & Yuan, H. 2022" 串写所致）；
  卷页/年份/另两作者（Yuan, H.＝Haibo Yuan、Xiao, K.＝Kai Xiao）全中。
- **版本：版本对**。刊元数据（ApJ 973, 1, 2024；DOI 10.3847/1538-4357/ad70ab）与 arXiv v1 的 "ApJ accepted" 一致 ⇒ 被引内容在所指版次。
- 用量：网络 3/3、仓库 2/2。

## 171 · DOI 10.1002/9780470434697（Huber & Ronchetti, Robust Statistics §4 效率表） —— 核验态：书目已核，节号 UNPROVEN
- **存在性：已核**。Crossref：Title "Robust Statistics"；Publisher **Wiley**；Type **monograph**；issued year **2009**；
  ISBN 字段 **9780470129906** 与 **9780470434697**；Authors Peter J. Huber, Elvezio M. Ronchetti（查询 1；此前 3 次代理网关 403 未取内容）。
- **仓库挂载点与原文**：`docs/science/PHASE2_UPM.md:131-143`——断言 "δ=1.345 是**高斯**参考分布下渐近效率 95% 的 Huber 阈值"，
  出处并列 Huber 1964（`10.1214/aoms/1177703732`）＋ Holland & Welsch 1977（`10.1080/03610927708827533`）＋
  `Huber, P. J. & Ronchetti, E. M. 2009, Robust Statistics, 2nd ed., Wiley, ISBN 978-0-470-12990-6 / DOI 10.1002/9780470434697（§4 效率表）`。
- **关联性：UNPROVEN（节号级）**。书目级绑定成立（该 DOI/ISBN 确指 2009 年 Huber & Ronchetti 合著的 Wiley 电子书，即 2nd ed.）；
  但"效率表在 **§4**"未读到任何权威目录：`onlinelibrary.wiley.com/doi/book/10.1002/9780470434697` 回包 **403**（查询 2）。
  **仓内自相矛盾（旁证，非外证）**：同一本书另两处被引到**别的节**——`docs/algorithms/PHOTOMETRIC_FIT.md:304`（§6.4 p.133 / §6.5 p.137 / §7.7 p.172）、`:32`（§6.5）；
  而 `docs/science/PHASE2_UPM.md:316` 对**同一个 δ=1.345** 改口为 "Holland & Welsch 1977（δ=1.345 的 IRLS 出处）"不再提 §4；
  `PHOTOMETRIC_FIT.md:304` 又把 biweight 效率/崩溃点表归给 statsmodels `statsmodels/robust/_tables.py` 而非本书。
  按前言 §2 判 UNPROVEN（不判关联错：无证据表明 §4 有、也无证据表明无）。
- **版本：版本对**。DOI、两个 ISBN、2009 年与 Huber+Ronchetti 双作者组合互相自洽（1981 初版为 Huber 单作者），"2nd ed." 声称成立。
- 用量：网络 2 次实质＋3 次网关失败、**仓库 3 次 > 上限 2（超额自报：`git grep` 定位＋`Read` PHASE2_UPM 120-147＋全仓 `Robust Statistics` 交叉核对）**。

## 177 · DOI 10.1046/j.1365-8711.2001.04937.x（Trujillo+2001，Moffat PSF） —— 核验态：已核
- **存在性：已核**。Crossref：Title "The effects of seeing on Sérsic profiles - II. The Moffat PSF"；Container MNRAS；Vol **328**；Pages **977–985**；Year **2001**；
  Authors I. Trujillo, J.A.L. Aguerri, J. Cepa（查询 1；另 1 次代理网关 403）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:214`
  `[B76] Trujillo, I., et al. 2001, MNRAS 328, 977, DOI 10.1046/j.1365-8711.2001.04937.x [S]`（§"分场景定量补充" 清单）。
- **关联性：关联对**。一作 I. Trujillo、年份 2001、刊 MNRAS、卷 328、起始页 977 五项全中；主题（seeing 对 Sérsic 轮廓拟合的影响，Moffat PSF 篇）与该清单的 PSF/测光场景补充角色相符。
  ⚠ 提示（本条不判错）：该文是系列 **Paper II（Moffat PSF）**，仓内既未标篇号也未记题名；他日若引它作"高斯 PSF"结论即成错绑（Paper I 是另一 DOI）。
- **版本：版本对**。所指即 2001 MNRAS 328, 977–985；`10.1046/j.1365-8711.2001.xxxx.x` 为 2001 年 MNRAS 旧 DOI 格式，未向概念 DOI 漂移。
- 用量：网络 2/3、仓库 2/2。

## 183 · DOI 10.1051/0004-6361/201833991（Merlin+2019，A-PHOT） —— 核验态：已核
- **存在性：已核**。Crossref：Title "A-PHOT: a new, versatile code for precision aperture photometry"；Container A&A；Vol **622**；Page **A169**；Year **2019**；
  Authors E. Merlin, S. Pilo, A. Fontana（查询 1；另 1 次代理网关 403）。
- **仓库挂载点与原文**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:152`
  `[B26] Merlin, E., Pilo, S., et al. 2019, A&A 622, A169, "A-PHOT: a new, versatile code for precision aperture photometry" DOI 10.1051/0004-6361/201833991 [V]`（§"孔径 vs PSF / 提取方法"）。
- **关联性：关联对**。题名逐字一致，卷页 A622/A169、年份 2019、前两作者全中。
  说明为何**未记 项目名＋版本＋文件:行**：本条在仓内只作**方法学论文**著录（未据 A-PHOT 的某段实现行为下断言），不属"以软件为证据"的引用；
  若后续文档以 A-PHOT 代码行为立论，须另按软件判据补项目名＋版本＋文件:行。
- **版本：版本对**。DOI ↔ A&A 622 A169 (2019)；DOI 数字段 `201833991` 是 2018 年投稿号，与 2019 出版年不矛盾（A&A 惯例）。
- 用量：网络 2/3、仓库 1/2。

---

## 5 文末必给

### 本批三态计数（18 条）
| 字段 | 档 | 条数 | 条目 |
|---|---|---|---|
| 存在性 | 已核 | **18** | 59 83 93 99 105 111 117 123 129 135 141 147 153 159 165 171 177 183 |
| | 不存在 | 0 | ——（117 的 DOI 真实存在，只是非天文文献，而仓内正是这样登记的） |
| | UNPROVEN | 0 | —— |
| 关联性 | 关联对 | **16** | 59 83 93(主题/定性级) 99 105 111 117 123(主题级) 129 141 147 153 159 165 177 183 |
| | 关联错 / 角色错绑 | 0 | —— |
| | UNPROVEN（节号·式号·数值级） | **2** | 135（§2 Eq.(1)、§3 未定义 σ）、171（§4 效率表） |
| 版本 | 版本对 | **15** | 59 83 99 105 111 117 129 141 147 153 159 165 171 177 183 |
| | 版本错 | **1** | 123（`arXiv:astro-ph/0412607` 只有 **v1 单作者 Calabretta** 预印本、回包无 DOI/journal_ref，却与 **Calabretta & Roukema 2007, MNRAS 381, 865–872** 并列著录 ⇒ arXiv 侧应改指 2007 正式版或删去） |
| | 未钉版次 | **2** | 93（只写"2015＋arXiv"，未钉 PASP 128(967) 094504，而所挂数值须正式版）、135（§号断言所在版次未打开） |

### UNPROVEN 清单（附标识符与已走途径，供下轮换路）
1. **93 · `0.56% RMS` 与 "100 s – 3.5 h" 时标区间**：`arXiv:1510.07567v1`、`10.1088/1538-3873/128/967/094504`；
   途径 arXiv API（仅摘要）→ `arxiv.org/abs`（同段摘要）→ `ar5iv.labs.arxiv.org/html/1510.07567`（HTML 不完整，未见百分比）。下轮换：IOP 文章页（可能被 perfdrive 拦）／ADS 全文（需 token）。
2. **123 · 等面积常数式 `|dΩ/dudv| = π/3`**：`arXiv:astro-ph/0412607v1`、`10.1111/j.1365-2966.2007.12297.x`；
   途径 arXiv API（摘要给 "natural spherical projection / Equal Area"）→ `arxiv.org/pdf`（网关 403）。下轮换：OUP MNRAS 页（Cloudflare 概率高）／ar5iv（若有 HTML）；另注：该常数本可由 `4π/(12·Nside²)` 自行复算，不必依赖文献。
3. **135 · §2 "Background estimation" 给 Eq.(1) 与 §3 未定义 σ**：`10.1051/aas:1996164`、`1996A&AS..117..393B`；
   途径 Crossref（书目全中）→ `doi.org` 302 → `aas.aanda.org` 落地页（**仅 1996 扫描版 PDF `/articles/aas/pdf/1996/08/ds1060.pdf`，无 HTML 全文**）→ 检索式（未命中可开正文）。
   下轮换：OCR/人工读扫描件；或把该断言降级为"文章级"，证据只留已具备的源码行号（`src/back.c:443-587` 等）。
4. **171 · "§4 效率表"节号**：`10.1002/9780470434697`、ISBN `9780470129906`/`9780470434697`；
   途径 Crossref（书目全中）→ Wiley onlinelibrary（**403**）。下轮换：Google Books / HathiTrust / 高校 OPAC 目录页（含章节标题），或改由 Huber 1964 与 Holland & Welsch 1977 原文表格承担（后者仓内已另引）。

### 新发现的缺陷形态（本批独有、前面批次未登记）
- **㊀ 预印本与正式版"作者数·版次"不配对**（123）：同一著录行内 DOI 指 2007 双作者正式版、arXiv 号指 2004 单作者 v1 预印本（回包无 DOI/journal_ref）。
  题名一致完全掩盖它，只有核 arXiv 回包的**作者串＋版本数**才暴露 ⇒ 判据宜加"arXiv 侧作者数须与所引著者一致，否则不得并列"。
- **㊁ 首作者姓名缩写错，成因是同表作者串写**（165：`Huang, B.`（Bowen Huang）写成 `Huang, Y.`，与 [B59] "Huang, Y., Xiao, K. & Yuan, H." 同型）。
  卷页·年份·DOI 全对而缩写错 ⇒ 只核卷页的复查会漏；须核**作者全名**。
- **㊂ 正确文献以"反例"身份入册**（117）：被引 DOI 本身是医学书评（CID 41(9), 1369），仓内把它登记为"已订正的错误候选"。
  一刀切按"该 DOI 是否存在"判会误报"引用了不存在的文献"；须先读**断言极性**（否定式）再判关联。
- **㊃ 精确数值挂在不可开正文的版次上**（93、123、135 同型）：`0.56%`、`π/3`、`Eq.(1)` 三处都是"数字＋文献"硬绑定，而数字所在位置本轮全部不可达（无 HTML／扫描版／出版社 403）
  ⇒ 建议本仓口径：引数值必须带**版次＋页码或式号**，且该版次必须可开；否则数值进"项目自算"通道、文献只承担方法学出处。
- **㊄ 同一事实的两处出处口径不一致**（171 对 `PHASE2_UPM.md:316` 对 `PHOTOMETRIC_FIT.md:304`）：δ=1.345 一处归"本书 §4 效率表"、一处归 Holland & Welsch 1977；
  同一书另被引到 §6.4/§6.5/§7.7，与"§4 是效率表"的章序观感冲突 ⇒ 属**可机检**形态（同一 DOI 在仓内的节号集合应一致或明示不同用途）。
- **㊅ 批次清单存在同文献双计**：59 与 105 是 Wild & Hewett 2005 的 DOI 与 arXiv 两个标识符 ⇒ 18 条 = 17 篇，台账计数与"已核条数"须按文献去重。

### 覆盖率自报
- 分配 18 条，出结论 **18 条 = 100%**（存在性 18/18；关联性 16/18 判"关联对"＋2/18 节号级 UNPROVEN；版本 18/18）。
- 网络访问合计约 31 次，其中 **8 次为网关失败**（Crossref 429 × 2、代理 403 × 6），按前言 §3 不计入实质配额；
  实质取证最多的条目：93 / 117 / 135 / 165 各 3 次（到限）。
- **超额自报**：171 仓库读 3 次（上限 2）。其余各条均在 网络 ≤3（不含网关失败）／仓库 ≤2 之内。
- 未改仓库、未跑构建与门禁、未写共享台账 `整改/out/文献台账.csv`（本批结论只在本文件，由前台回写）。

<!-- PROGRESS: 18/18 -->
