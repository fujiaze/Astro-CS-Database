# 核验-CIT-07｜14 条 P-1 引用文献核验

- 基线：`c8f64e9a`（只读审计，零 git 写、零构建、零 `eng/**` 执行）
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` 全条 + 派单追加纪律
- 批次清单：`独立审计/批次清单/CIT-07.txt`
- 网络手段清单：
  - `export.arxiv.org/api/query?id_list=<id>`（arXiv 官方 API，给题名/作者/日期/版次/journal_ref）
  - `api.crossref.org/works/<doi>`（DOI 解析；同轮并发 ≤2，网关失败不占取证配额、重试）
  - `aspbooks.org/custom/publications/paper/<卷>-<页>.html`（ASP 会议集）
  - ADS 对本环境两种 URL 一律 405 ⇒ 仅 bibcode 无其它标识符者按句柄不可核处理
  - 读正文走 HTML 途径（`arxiv.org/html/<id>vN`、`ar5iv`），PDF 常乱码
- 配额：单条取证上限 6 次网络请求（前言基线 3 次为目标值），超限即落 UNPROVEN 并附已走途径
- 三字段：存在性 / 版次·载体 / 关联（关联判定必须打开断言原文）

## 骨架（14 条，逐条 append 结论）

| 序号 | 标识符 | 断言位点 | 核验态 |
|---|---|---|---|
| 185 | DOI 10.1051/0004-6361/202039587；Evans | 实验/absolute-snr/docs/snr-propagation-design.md:1144 | 已核·无错 |
| 191 | DOI 10.1051/0004-6361/202243880；arXiv:2206.06205 | docs/science/PHOTOMETRY.md:311 | 已核·无错 |
| 197 | DOI 10.1051/aas:1996164 | 实验/absolute-snr/docs/snr-propagation-design.md:1114 | P1·载体错挂（关联 UNPROVEN） |
| 203 | DOI 10.1086/114121 | docs/algorithms/PLATESOLVE.md:369 | P1·关联错＋卷页作者错 |
| 209 | DOI 10.1086/132749 | docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:184 | 已核·无错 |
| 215 | DOI 10.1086/133670 | docs/algorithms/PLATESOLVE.md:369 | P1·关联错＋卷页作者错 |
| 221 | DOI 10.1086/316595 | docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:190 | 已核·无错 |
| 227 | DOI 10.1086/338393，bibcode | docs/science/DRIZZLE.md:180 | 已核·无错（清单位点行号漂移） |
| 233 | DOI 10.1086/431468 | docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:229 | P1·题名多出 "Landmark/" |
| 239 | DOI 10.1086/524677 | 实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:189 | 已核·无错（"IOP 文章页"措辞不精确） |
| 245 | DOI 10.1088/0067-0049/219/1/12 | 实验/absolute-snr/docs/surveys/f-instr-survey.md:186 | 已核·无错 |
| 251 | DOI 10.1093/mnras/stv1320 | 实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:80 | 已核·无错 |
| 257 | DOI 10.1109/TIP.2010.2056693 | 实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:454 | 已核·无错（式号已读正文核实） |
| 263 | DOI 10.1137/0201010 | docs/algorithms/PHASE2_COVERAGE.md:403 | 已核·无错 |

## 逐条结论

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据（Crossref 回包原文片段） | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 203 | DOI 10.1086/114121（PLATESOLVE.md:369） | 已核（DOI 存在） | **错**：回包是 AJ **91, 1428**（1986-06），断言写 "Groth 1986, **AJ 91, 1244**"——DOI 与所写卷页本身已不自洽 | **关联错** | `"title":"Rediscussion of eclipsing binaries. XV - Alpha Coronae Borealis, a main-sequence system with components of types A and G"`, `"container-title":"The Astronomical Journal", volume 91, page 1428, publisher AAS, published-print 1986-06, authors 2: `[J. Tomkin, D. M. Popper]``。断言原文（PLATESOLVE.md:369）："三角匹配/星表求解：Groth 1986, AJ 91, 1244（DOI 10.1086/114121）"——该文是**食双星光变曲线分析**，与"三角匹配/星表求解"（星像-星表配对算法）无任何关系；作者也不符（Groth vs Tomkin & Popper）。属前言 §4① 高危形态"不存在的标识符配对" | — |
| 215 | DOI 10.1086/133670（PLATESOLVE.md:369） | 已核（DOI 存在） | **错**：回包是 PASP **107, 1131**（1995-12），断言写 "Valdes et al. 1995, **PASP 107, 1119**" | **关联错** | `"title":"The 75th Anniversary Astronomical Debate on the Distance Scale to Gamma-Ray Bursts: an Introduction"`, container `Publications of the Astronomical Society of the Pacific`, volume 107, page 1131, publisher IOP Publishing, published-print 1995-12, authors 1: `[Robert J. Nemiroff]`。断言原文同上句："…Valdes et al. 1995, PASP 107, 1119（DOI 10.1086/133670）"——回包是**伽马暴距离标度辩论的导言**、单作者 Nemiroff，不是 "Valdes et al."（多作者），也不做星表求解/三角匹配 | — |

| 185 | DOI 10.1051/0004-6361/202039587（snr-propagation-design.md:1144） | 已核 | **对**：A&A **649, A3**，2021（online 2021-04-28），42 作者，首位 `M. Riello` ⇒ 与 "Riello et al. 2021, A&A 649, A3" 卷页＋作者＋年份全配 | **关联对** | Crossref：title "Gaia Early Data Release 3", container "Astronomy & Astrophysics", vol 649, page A3, publisher EDP Sciences, authors 42 首位 ('M.','Riello') 次位 ('F.','De Angeli') 三位 ('D. W.','Evans')。arXiv API `2012.01916v1`（PHOTOMETRY.md:312 给同一 DOI 的预印本号）TITLE "Gaia Early Data Release 3: Photometric content and validation", NAUTH 42, 首位 M. Riello；摘要 "…focuses on the photometric content, describing the input data, the algorithms, the processing, and the validation of the results. Particular attention is given to the quality of the data…"。断言原文（snr-propagation-design.md:1144 行前两列＋借鉴列）："Gaia 测光验证 | Riello et al. 2021, A&A 649, A3, DOI 10.1051/0004-6361/202039587 … | 借鉴：参考星质量与定标地板"——"测光验证／数据质量／定标"正是该文副题名与摘要主旨，角色相符。同栏并挂 Evans 2018 A&A 616 A4（DOI …201832756）与 Drimmel 2023 A&A 674 A37 属**邻条**，本批只登 202039587，未核 | — |
| 191 | DOI 10.1051/0004-6361/202243880；arXiv:2206.06205（PHOTOMETRY.md:311） | 已核（DOI 与 arXiv 双件存在） | **对**：Crossref A&A **674, A3**，2023-06，49 作者，前四位 `Montegriffo, De Angeli, Andrae, Riello` ⇒ 与断言串 "Montegriffo, P., De Angeli, F., Andrae, R., Riello, M., et al. 2023, A&A 674, A3" 逐项配；arXiv 侧仅 **v1**（2022-06-13），作者数同 49 | **关联对** | Crossref title "Gaia Data Release 3", vol 674, page A3, EDP Sciences, NAUTH 49 前四位 ('P.','Montegriffo'),('F.','De Angeli'),('R.','Andrae'),('M.','Riello')。arXiv API：`ID: http://arxiv.org/abs/2206.06205v1`，TITLE "Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data"，NAUTH 49 前五位 ['P. Montegriffo','F. De Angeli','R. Andrae','M. Riello','E. Pancino']，摘要 "we focus on the external calibration of low-resolution spectroscopic content… We calibrated an instrument model to relate mean Gaia spectra to the corresponding spectral energy distributions… Particular attention is given to the quality of the data"。断言原文（PHOTOMETRY.md:311）："**7. XP 外定标（仪器响应模型与定标精度）**：Montegriffo… 2023, A&A 674, A3（DOI 10.1051/0004-6361/202243880；arXiv:2206.06205）"——"XP 外定标＋仪器响应模型＋定标精度"与摘要三句一一对应。注：arXiv 回包 journal_ref/doi 字段为空 ⇒ DOI↔预印本的配对是**题名＋49 作者表一致**级证据，非 API 直连字段 | — |
| 251 | DOI 10.1093/mnras/stv1320（REVERSE_VERIFY_BIBLIOGRAPHY.md:80） | 已核 | **对**：MNRAS **452, 809-823**，print 2015-09-01（online 2015-07-08），2 作者 `A. Popowicz, B. Smolka` ⇒ 与 "Popowicz & Smolka 2015, MNRAS 452, 809" 全配 | **关联对** | Crossref title "A method of complex background estimation in astronomical images", container "Monthly Notices of the Royal Astronomical Society", vol 452, page 809-823, publisher OUP, NAUTH 2。断言原文（bib:80 行 4.8）："对**复杂**（结构化、非平坦）天文背景估计的专门处理，可作 SExtractor 网格法的可引用对照，并作为『背景估计是活跃方法学问题而非已解决预处理』的证据"——题名即 "complex background estimation"，角色相符；同行"不借鉴"列已自行标注不确定度声明 `[UNVERIFIED]`，与本条"未读全文"边界一致，不算缺陷 | — |

| 239 | DOI 10.1086/524677；arXiv:astro-ph/0703454v2（REVERSE_VERIFY_BIBLIOGRAPHY.md:189） | 已核（双件存在） | **对**：ApJ **674, 1217-1233**，print 2008-02-20，24 作者，首位 `Nikhil Padmanabhan` ⇒ 与 "Padmanabhan, N. et al. (2008) … ApJ 674, 1217" 全配；arXiv 回包 `astro-ph/0703454v2`（published 2007-03-19 / updated 2007-10-19）v2 即最新版 ⇒ 钉版次对 | **关联对** | Crossref title "An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data", container ApJ, vol 674, page 1217-1233, publisher AAS, NAUTH 24 前六位 Padmanabhan/Schlegel/Finkbeiner/Barentine/Blanton/Brewington。arXiv API 摘要逐字（用于核 bib:191-193 三条"摘要逐字"引）："We present an algorithm to photometrically calibrate wide field optical imaging surveys, that **simultaneously solves for the calibration parameters and relative stellar fluxes using overlapping observations**. The algorithm **decouples the problem of "relative" calibrations, from that of "absolute" calibrations**…; We **pay special attention to the spatial structure of the calibration errors, allowing one to isolate particular error modes** in subsequent analyses… we achieve **~1% relative calibration errors across 8500 sq.deg. in griz; the errors are ~2% for the u band**"——断言原文（bib:188-197）三处标"摘要逐字"的引文与回包**逐字吻合**（引号嵌套为排版差异），"不借鉴"列的 ~1%/~2% 与 8500 sq.deg. 亦吻合；"只有乘性项、无加性天光"与该文摘要范围（calibration parameters + relative fluxes，误差由大气变化主导）不冲突 | 注：bib:189 括注 "DOI …（公开检索命中 **IOP 文章页**）"——Crossref 记录 publisher 为 **American Astronomical Society**（10.1086 前缀由 AAS 注册），措辞不精确，属出处描述瑕疵非书目错误 |

| 197 | DOI 10.1051/aas:1996164（snr-propagation-design.md:1114） | 已核 | **对**（就论文本身）：A&AS **117, 393-404**，1996-06，2 作者 `E. Bertin, S. Arnouts` ⇒ 与 "Bertin & Arnouts 1996, A&AS 117, 393" 全配；**但被引内容的载体是手册不是论文**（见关联） | **关联 UNPROVEN**（缺 1996 论文正文） | Crossref title "SExtractor: Software for source extraction", container "Astronomy and Astrophysics Supplement Series", vol 117, page 393-404, EDP Sciences, NAUTH 2。**实核到的是手册不是论文**：断言原文（snr-design:1114）"**Bertin & Arnouts 1996, A&AS 117, 393, DOI 10.1051/aas:1996164**（当前手册 **式 (36)**，已逐字核对）｜`FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))`…同页自带「这是下界」的警告"——我抓取 `raw.githubusercontent.com/astromatic/sextractor/HEAD/doc/src/Photom.rst`（HTTP 200, 12896 B）逐字命中：`{\tt FLUXERR} = \sqrt{\sum_{i\in{\cal A}}\, (\sigma_i^2 + \frac{p_i}{g_i})}` 与 "where … $\sigma_i$, $p_i$, $g_i$ respectively the standard deviation of noise (in ADU) estimated from the local background, $p_i$ the measurement image pixel value subtracted from the background, and $g_i$ the effective detector gain" 与 "Note that this error estimate provides a lower limit of the true uncertainty, as it only takes into account photon and detector noise." ⇒ **公式与警告逐字为真，但出处是 SExtractor 在线手册**（该式在手册源里是**无编号**的 `.. math:: :label: fluxerr`，本页 11 个 math 块零 `\begin{equation}`，Sphinx 构建期编号 ⇒ "式 (36)" 在当前手册源中不可复核，且该行未钉手册版本/URL）。1996 论文正文不可达：DOI 302 → `aas.aanda.org/10.1051/aas:1996164`（1996 A&AS 无 HTML 全文），ADS 本环境 405 ⇒ 论文是否含该式**未证** | 走过途径：Crossref（中）、`raw.githubusercontent/.../legacy_doc/prevdoc/Photom.html`（**404**）、GitHub tree（默认分支无 HTML 手册，只有 `doc/src/*.rst`）、doi.org 302 目标探测（无可读全文）；缺的一面＝1996 A&AS 论文正文 |
| 209 | DOI 10.1086/132749（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:184） | 已核 | **对**：PASP **102, 1181**，1990-10，1 作者 `M. S. Bessell` ⇒ 与 "[B51] Bessell, M. S. 1990, PASP 102, 1181" 全配 | **关联对** | Crossref title **"UBVRI passbands"**（与断言自述题名逐字相同）, container PASP, vol 102, page 1181, IOP Publishing, NAUTH 1 ('M. S.','Bessell')。断言原文（archive:184）："[B51] Bessell, M. S. 1990, PASP 102, 1181, \"UBVRI passbands\" DOI 10.1086/132749 [S]"——该位点是纯书目登记，题名/作者/卷页/年份四项与回包一致。**旁注**：同一 DOI 在 `docs/science/PHOTOMETRY.md:313` 被派的角色是"光子计数通带（`λ` 因子的文献依据）…能量计数 vs 光子计数口径"，该角色是否真为 Bessell 1990 所为本轮未核（未读正文），属他条位点 | — |
| 221 | DOI 10.1086/316595（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:190） | 已核 | **对**：PASP **112, 925-931**，2000-07，1 作者 `Peter B. Stetson` ⇒ 与 "[B55] Stetson, P. B. 2000, PASP 112, 925" 全配 | **关联对** | Crossref title "Homogeneous Photometry for Star Clusters and Resolved Galaxies. II. Photometric Standard Stars", container PASP, vol 112, page 925-931, IOP Publishing, NAUTH 1。断言原文（archive:190）："[B55] Stetson, P. B. 2000, PASP 112, 925, DOI 10.1086/316595 [S]"，登记于"**定标 / Gaia XP / 绝对通量（续）**"小节（archive:186）——该文续篇主题即 **测光标准星**，与所在小节"定标"角色相符 | — |
| 227 | DOI 10.1086/338393（inventory 位点 docs/science/DRIZZLE.md:180 实为**行号漂移**） | 已核 | **对**：PASP **114(792), 144-152**，2002-02，2 作者 `A. S. Fruchter, R. N. Hook`；bibcode `2002PASP..114..144F` 与回包卷页自洽（非编造）；arXiv 侧 `astro-ph/9808087v2` 存在（published 1998-08-10 / updated 2001-10-19，2 作者同名）⇒ 断言里钉的 **v2 即最新版**，对 | **关联对** | Crossref title "Drizzle: A Method for the Linear Reconstruction of Undersampled Images", vol 114, page 144-152, IOP Publishing, NAUTH 2；arXiv API `astro-ph/9808087v2` 同题、摘要 "We have developed a method for the linear reconstruction of an image from undersampled, dithered data. The algorithm, known as Variable-Pixel Linear Reconstruction, or informally as Drizzle, preserves photometry and resolution, can weight input images according to the statistical significance of eac[h pixel]…"。断言原文（实际位点 **DRIZZLE.md:207**）："Fruchter… 2002, PASP, 114, 144, \"Drizzle: A Method for the Linear Reconstruction of Undersampled Images\"（DOI 10.1086/338393，bibcode 2002PASP..114..144F；§5 面亮度归一依据其 §2 式(5)…）"、**DRIZZLE.md:215**："（DOI 10.1086/338393；arXiv:astro-ph/9808087v2 §2 式(2)-(5)）。式(5) 为**一致加权均值**"——书目层与摘要层全对；**其 §2 式(5) 已读正文核实**：走 `ar5iv.labs.arxiv.org/html/astro-ph/9808087`（HTTP 200, 110 KB）取到该预印本 HTML，式 (4)(5) 逐式可见——(4) `W_{x_oy_o} = Σ a_{x_iy_ix_oy_o} w_{x_iy_i}`（权重和）、(5) `I_{x_oy_o} = [Σ d_{x_iy_i} a_{x_iy_ix_oy_o} w_{x_iy_i} s²] / W_{x_oy_o}`（输出值＝输入值×重叠面积×权重之和除以 s²·W）⇒ 断言"式(5) 为**一致加权均值**"与正文同构，且 §2 内式 (1)-(6) 编号齐备，与"arXiv:astro-ph/9808087v2 §2 式(2)-(5)"的钉法一致 | — |
| 245 | DOI 10.1088/0067-0049/219/1/12（f-instr-survey.md:186） | 已核 | **对**：ApJS **219(1), 12**，online 2015-07-27，**304 作者**，首位 `Shadab Alam` ⇒ 与 "Alam, S. et al. (2015). The Eleventh and Twelfth Data Releases of the SDSS. ApJS 219, 12" 配（断言题名截去了副题 ": FINAL DATA FROM SDSS-III"，不改变指认） | **关联对** | Crossref title "THE ELEVENTH AND TWELFTH DATA RELEASES OF THE SLOAN DIGITAL SKY SURVEY: FINAL DATA FROM SDSS-III", container ApJS, vol 219, page 12, AAS, published-online 2015-07-27, NAUTH 304 首位 ('Shadab','Alam')。断言原文（f-instr-survey.md:186 [F-13] 行）："### [F-13] SDSS：…；Alam, S. et al. (2015). … ApJS 219, 12. DOI 10.1088/0067-0049/219/1/12 `[CR]` `[arXiv]`"，该行角色是"主流巡天/管线怎么做"里 SDSS 的**巡天身份标识**——DR11/12 最终数据Release 文献正是该身份的规范出处；行内的实质引文（通量定义/nanomaggy）另有其一手来源（SDSS4 DR17 官方页，同行另标 `[page]`），未误记到 Alam 头上 | 注：行内并列的 Stoughton 2002（DOI 10.1086/324741）与 Lupton 2001（ASPC 238, 269）属邻条，本批未核 |

| 233 | DOI 10.1086/431468；arXiv:astro-ph/0504244（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:229） | 已核（双件存在） | **载体对、题名不符**：Crossref PASP **117, 810-822**，2005-08，**7 作者**，首位 `Maximilian Stritzinger`；arXiv 回包 `astro-ph/0504244v2`（2005-04-11/05-02）与 **v1 落地页**（`arxiv.org/abs/astro-ph/0504244v1`，HTTP 200）题名均为 **"An Atlas of Spectrophotometric Landolt Standard Stars"** ⇒ 断言串里的 **"Landmark/"** 在**出版社版与两个预印本版三处载体均不存在**，属题名被改动；断言未给卷页（未钉卷页，不算错） | **关联对** | Crossref title "An Atlas of Spectrophotometric Landolt Standard Stars", container PASP, vol 117, page 810-822, IOP Publishing, NAUTH 7（Stritzinger/Suntzeff/Hamuy/Challis/Demarco/Germany/Soderberg）；arXiv API `astro-ph/0504244v2` 题名同、7 作者同、摘要 "We present CCD observations of 102 Landolt standard stars obtained with the R-C spectrograph on the CTIO 1.5 m telescope…"。断言原文（archive:229，小节"**孔径改正 / 通带颜色项 / Gaia XP 追加**"）："[B88] Stritzinger, M., et al. 2005, \"An Atlas of Spectrophotometric Landmark/Landolt Standard Stars\" DOI 10.1086/431468, arXiv:astro-ph/0504244"——作者/年/两个标识符指认一致，光谱亮度计标准星星表用于通带与绝对通量角色相符；**缺陷在题名文本**（多出 "Landmark/"），已入本批 P1 清单 | 走过途径（v1 追查）：`export.arxiv.org/api/query?id_list=astro-ph/0504244v1`→ 服务器 internal error；`auth_abs_num_list=astro-ph/0504244v1`→ ENTRIES 0；改走 `arxiv.org/abs/…v1` 落地页 **成功**（citation_title 无 "Landmark"） |
| 263 | DOI 10.1137/0201010（PHASE2_COVERAGE.md:403） | 已核 | **对**：SIAM J. Comput. **1(2), 146-160**，print 1972-06（online 2006-07-13），1 作者 `Robert Tarjan` ⇒ 与 "Tarjan 1972, SIAM J. Comput. 1, 146" 全配（DOI 串里的 0201 是期刊码不是卷号，卷 1 正确） | **关联对** | Crossref title "Depth-First Search and Linear Graph Algorithms", container "SIAM Journal on Computing", vol 1, issue 2, page 146-160, publisher SIAM, NAUTH 1。断言原文（PHASE2_COVERAGE.md:403）："连通分量分解：Tarjan 1972, SIAM J. Comput. 1, 146（DOI 10.1137/0201010）；Hopcroft & Tarjan 1973, Comm. ACM 16, 372。"——Semantic Scholar 回包 tldr 原文："The value of depth-first search or 'backtracking' as a technique for solving problems is illustrated by two examples of an improved version of an algorithm for **finding the connected components** of a directed graph."⇒ "连通分量分解"正是该文承担的角色（同栏并列的 Hopcroft & Tarjan 1973 属邻条，本批未核）。**边界**：SIAM 正文付费墙、`openAccessPdf.status=CLOSED`，具体小节号/伪码未读；断言本身未给节号，故按书目＋tldr 级判定 | — |

| 257 | DOI 10.1109/TIP.2010.2056693（EXP-06-SNR-PHYS.md:454） | 已核 | **对**：IEEE TIP **20(1), 99-109**，print **2011-01**（DOI 串里的 "2010" 是投稿/注册年，不是出版年），2 作者 `M. Makitalo, A. Foi` ⇒ 与断言 "Mäkitalo & Foi 2011（IEEE TIP 20, 99–109, DOI 10.1109/TIP.2010.2056693）" 卷页年份全配；S2 回包 year 2011 / venue IEEE TIP / DBLP `journals/tip/MakitaloF11` 三源一致 | **关联对（式号层已读正文核实）** | Crossref title "Optimal Inversion of the Anscombe Transformation in Low-Count Poisson Image Denoising", vol 20, page 99-109, IEEE。断言原文（EXP-06:454）："Anscombe 1948…；公式 `f(z)=2*sqrt(z+3/8)` **转引自已核验全文的** Mäkitalo & Foi 2011（…）式 (4)"＋（EXP-06:457）"泊松时 `E[z|y] = y = var[z|y]`，Mäkitalo & Foi 2011 **§II.A 式 (2)**"＋（附录 A7，:606）"§II.A 式 (2) 泊松方差=均值；式 (4) Anscombe"。取到作者版 PDF（S2 `openAccessPdf` → `cs.tut.fi/~foi/papers/OptAnscombeInverse-IEEE_TIP-Preprint.pdf`，2,832,291 B，页眉 "MÄKITALO AND FOI, OPTIMAL INVERSION OF THE ANSOMBE TRANSFORMATION…"）逐式读得：式 **(2)** `E[z_i | y_i] = y_i = var[z_i | y_i]`、式 **(4)** `f(z) = 2\sqrt{z + \tfrac{3}{8}}`（正文标 "the Anscombe transformation [8]"），两式均落在 §II.A/§II.B 区（§II.B 标题 "Variance stabilization and the Anscombe transformation"）⇒ **式号与内容双双对上，"已核验全文"的自述成立** | 边界：读的是作者存档 PDF（IEEE Xplore 正文付费、`openAccessPdf.status` 对 263 为 CLOSED 而本条 GREEN）；预印本与出版社正版的式号一般一致（本 PDF 已是 IEEE 双栏定稿版式），但**未与正版逐页比对**，若整改需以正版页码为准再确认一次 |

<!-- PROGRESS: 14/14 -->

## 本批 P1 缺陷清单（只列判"错"的，逐条给断言原文片段）

1. **[203] DOI 10.1086/114121 挂到完全不相关的论文（食双星 vs 三角匹配）**
   断言原文 `docs/algorithms/PLATESOLVE.md:369`："三角匹配/星表求解：Groth 1986, AJ 91, 1244（DOI 10.1086/114121）"。
   Crossref 回包是 Tomkin & Popper 1986, AJ 91, **1428**, "Rediscussion of eclipsing binaries. XV – Alpha Coronae Borealis…"。
   三处同时错：**作者**（Groth ≠ Tomkin & Popper）、**起始页**（1244 ≠ 1428）、**内容角色**（食双星光变分析 ≠ 星像-星表配对算法）。
   订正阻塞点：本环境无法确定"应指"的正确 DOI——Crossref `query.title+query.author=Groth` 与 `query.bibliographic` 两路均未命中该 1986 AJ 文，ADS 405；须由有 ADS/出版社检索权限的一方给正身。
2. **[215] DOI 10.1086/133670 挂到不相关论文（伽马暴辩论导言 vs Valdes 星表求解）**
   断言原文同句："Valdes et al. 1995, PASP 107, 1119（DOI 10.1086/133670）"。
   Crossref 回包是 Nemiroff 1995, PASP 107, **1131**, "The 75th Anniversary Astronomical Debate on the Distance Scale to Gamma-Ray Bursts: an Introduction"，**单作者**。
   错法同上：作者数（"et al." ≠ 单作者）、页码（1119 ≠ 1131）、内容角色（GRB 距离标度辩论 ≠ 三角匹配）。
   **辐射面（比书目错更重）**：仓库把 "Valdes 1995" 当作**算法选型依据**用在实现侧——`lib/algorithms/platesolve/cpp/ipv/REPORT.md:591`（"Siril 用 Valdes 1995 三角形 (20 颗最亮星)"）、`.../ipv/SIRIL_COMPARISON.md:232`（"三角形匹配 | Valdes 1995 (b/a, c/a, angle)"）、`.../platesolve/memory.md:24`（"IPV算法：基于Valdes 1995三角形匹配"）。这些位点只有**作者+年份**、无 DOI/卷页，本轮无法判定"Valdes 1995 三角形匹配"本身是否为真（另路核验），但它们与 215 共用同一承重墙。
3. **[233] 题名被改动：多出 "Landmark/"，三处载体均无此词**
   断言原文 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:229`："Stritzinger, M., et al. 2005, "An Atlas of Spectrophotometric **Landmark/**Landolt Standard Stars" DOI 10.1086/431468, arXiv:astro-ph/0504244"。
   实际题名（三个独立载体一致）：Crossref/PASP 117, 810-822 = "An Atlas of Spectrophotometric Landolt Standard Stars"；arXiv v2 API 同；**v1 落地页 `citation_title` 亦同** ⇒ 不是"预印本与期刊版题名漂移"，而是断言串内混入了不属于该文的词（疑似与 CALSPEC/"landmark stars" 用语串扰）。标识符与作者/年/载体本身对得上。
4. **[197] 定位符载体与标识符载体不一致，且"式 (36)"不可复核**
   断言原文 `实验/absolute-snr/docs/snr-propagation-design.md:1114`："**Bertin & Arnouts 1996, A&AS 117, 393, DOI 10.1051/aas:1996164**（当前手册 **式 (36)**，已逐字核对）｜`FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))`…**同页自带「这是下界」的警告**"。
   实核结果：所给公式、σ/p/g 定义与 "lower limit of the true uncertainty" 警告**逐字为真，但出处是 SExtractor 在线手册** `doc/src/Photom.rst`（GitHub HEAD，HTTP 200）；该行挂的 DOI 是 **1996 A&AS 论文**。手册源中该式写作 `.. math:: :label: fluxerr`——**无编号**（本页 11 个 math 块、零 `\begin{equation}`），编号由 Sphinx 构建期生成 ⇒ "**当前手册** 式 (36)" 在当前手册源里核不到，且该行未钉手册版本/URL；1996 论文正文（1996 A&AS 无 HTML 全文、ADS 405）不可达，故论文是否含该式**未证**。
   同件在 `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:73` 的写法是"测光误差方程（.../Photom.html，式 (36)，逐字核对）"——**同一式号在两个文件里挂到不同载体**（一处点明手册页，一处挂在论文 DOI 上），属规范链不一致。

### 补充（不计缺陷，顺带核到）

- 185 同栏邻条 **Evans et al. 2018, A&A 616, A4, DOI 10.1051/0004-6361/201832756** 顺带走 Crossref：回包 "Gaia Data Release 2", A&A **616, A4**, 2018-08, 46 作者首位 `D. W. Evans` ⇒ 该邻条书目**无错**。
- 清单位点行号漂移：批次清单给 227 的代表位点 `docs/science/DRIZZLE.md:180`，而 HEAD（=基线 c8f64e9a）该处内容是 §12 "关联 ALG ID"（Sutherland–Hodgman / Van Oosterom & Strackee），**不含** DOI 10.1086/338393；该 DOI 实际落在 **DRIZZLE.md:207 与 215**。本轮按实际位点取证。

## 本批三态计数

| 字段 | 已核/对 | 错 | UNPROVEN | 条数合计 |
|---|---|---|---|---|
| 存在性 | 14（全部 DOI 均解析到真实记录，无一"不存在"） | 0 | 0 | 14 |
| 版次/载体 | 10（185,191,209,221,227,239,245,251,257,263） | 4（203 卷页＋作者、215 卷页＋作者数、233 题名文本、197 载体归属） | 0 | 14 |
| 关联 | 11（185,191,209,221,227,233,239,245,251,257,263） | 2（203、215） | 1（197） | 14 |

## UNPROVEN 清单（附已走途径，供下轮换路）

1. **[197] 关联字段**：1996 A&AS 论文正文是否含 `FLUXERR` 式——缺"论文正文"这一面。
   已走：Crossref（书目全对）→ `raw.githubusercontent.com/astromatic/sextractor/legacy_doc/prevdoc/Photom.html`（**404**）→ GitHub 默认分支 tree（无 HTML 手册，只有 `doc/src/*.rst`）→ `doi.org` 302 目标 `aas.aanda.org/10.1051/aas:1996164`（1996 卷无 HTML 全文）→ ADS（本环境 405）。
   下一轮可试：EDP Sciences A&AS 扫描 PDF 直链＋OCR；或找 1996 论文的纸质/馆藏扫描件；手册侧则钉 `astromatic.io` 构建页的实际编号并记版本号。
2. **[203]/[215] 的"应指"正身**：只证到"所给 DOI 错"，未证到"正确 DOI 是哪个"。
   已走：Crossref `query.title`＋`query.author`（Groth / Valdes 两路均未命中 1986 AJ / 1995 PASP 该题）、`query.bibliographic`（噪声大）、ADS（405）。
   下一轮可试：AAS 期刊卷页索引（`iopscience` PASP/AJ 卷目录页按页码定位）、`cdsarc`/`vizier` 的 bibliographic 引用、或 Simbad `bibcode` 引用表（前言提示 Simbad 对 ASPC 常 NO_REF，但对 AJ/PASP 应可用）。
3. **[227] 的 §5 面亮度归一**：`DRIZZLE.md:207` 称"§5 面亮度归一依据其 §2 式(5)"——式(5) 本体已核（见行内），但"§5 用它做面亮度归一"是本仓自身推导是否成立，不属文献核验范围，未判。

## 新发现的缺陷形态（本批独有）

1. **"三错同源"型 DOI 填充**（203、215 同行成对）：作者、起始页、内容角色同时错，且两个 DOI 的数字段与所写页码**不同源**（114121→1428、133670→1131，都不是所写页码）⇒ 形态上是"按印象随手填了同前缀的号"，不是抄录漂移。判据价值：只要拿 DOI 回包的 page 与断言页码比一次即可判红，成本一条 API 请求。
2. **定位符与标识符分属两份文档**（197）：式号来自软件**手册**、DOI 指向**论文**，且该式在当前手册源里是**无编号** `:label:` 块（编号由 Sphinx 构建期生成）⇒ 凡引"手册式 (N)"必须同时钉手册版本＋URL，否则定位符天然不可复核。仓库内两份文件对同一式号的载体写法不一致（`REVERSE_VERIFY_BIBLIOGRAPHY.md:73` 点明手册页 / `snr-propagation-design.md:1114` 挂论文 DOI），属"口径唯一"违例。
3. **题名文本污染**（233）：标识符与作者/年/卷都对，但题名多出 "Landmark/"，且**v1、v2、出版社三处载体一致** ⇒ 可判定不是版本漂移而是文本混入；核验时须把"题名逐字"作为独立一项，不能只比标识符。
4. **书目错的辐射面大于书目本身**（215）："Valdes 1995 三角形匹配"在 `lib/algorithms/platesolve/**` 三处被当作**算法选型依据**引用（无 DOI/卷页），一旦该文献指认不成立，受影响的是实现对照而非一条引用。
5. **清单位点行号漂移**（227）：批次清单给 `DRIZZLE.md:180`，HEAD（=基线 c8f64e9a）该处是 §12 ALG 列表，实际 DOI 在 207/215 ⇒ 派单行号只能当线索，取证前必须 `git grep` 重定位。

## 覆盖率自报

- **14/14 条**三字段齐（存在性/版次·载体/关联各 14 个判定），无一条留空；其中 1 条（197）关联字段按判据落 UNPROVEN 并附途径。
- 网络请求用量：合计约 34 次，单条最高 **5 次**（197），其余 1–4 次，均在派单 6 次上限内；前言基线 3 次为目标的达成情况：12/14 条 ≤3 次，2 条（197、233）因载体/版本追查超出到 4–5 次。
- 仓库读取：8 个文件、每处 ≤2 次读，另用 `git grep`/`git show` 做定位与基线核对（只读）；HEAD 实测 `c8f64e9ab6b867e4f108ba1e0073a8f2a97cfde9` = 基线。
- 零 git 写、零 `git config` 写、未跑构建/ctest/`eng/**`；未写仓库内任何文件（取证中途在仓库根落下的 `photom.rst`、`drz.html` 两个临时下载件**已即时删除**，`git status` 复核仅剩本批开始前已存在的 `ACSD整治工作包_AUDIT-06.zip` 与 `site/` 两项未跟踪条目，非本轮产物）。
- 未修改 `整改/out/文献台账.csv`（共享台账），结论只落在本文件。
