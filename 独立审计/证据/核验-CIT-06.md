# 引用文献核验 · 批次 CIT-06（18 条，P-1）

- 基线：`c8f64e9a`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` §1 三字段 / §2 硬禁令 / §4 高危形态
- 网络手段清单（本批实际可用）：
  - `WebFetch` → `https://export.arxiv.org/api/query?id_list=<id>`（arXiv 官方 API，回包给版本号）
  - `WebFetch` → `https://api.crossref.org/works/<doi>`（DOI 元数据）
  - `WebFetch` → `https://aspbooks.org/custom/publications/paper/<vol>-<page>.html`（ASPC 会议论文）
  - `WebSearch` → 出版社页 / 馆藏记录 / 图书页
  - 仓库侧只读：`Read` / `git grep`（每条≤2 次）
- 硬上限：每条 3 次网络查询 + 2 次仓库读取，到限写 `UNPROVEN` 并附用过的标识符与途径。
- 本批不做：不改仓库、不跑构建与门禁、不写共享台账 `整改/out/文献台账.csv`。

---

## 60 · DOI:10.1137/1.9781611970128（Wahba 1990, Spline Models for Observational Data）

**存在性：已核。** 依据：Crossref 记录 `https://api.crossref.org/works/10.1137/1.9781611970128` 回包 Title = *Spline Models for Observational Data*、Publisher = SIAM、Author = Grace Wahba、Year = 1990、Type = Monograph、DOI 同。非杜撰标识符。

**本仓位点与实际断言（三处全打开）**

| 位点 | 挂的那句 |
|---|---|
| `实验/additive-sky-seamless/results/evidence_lit.json:65-75`（id `4a-spline-models-book`） | cite「Wahba, G. 1990, Spline Models for Observational Data, SIAM (CBMS-NSF Regional Conference Series in Applied Mathematics 59)」；relevance「系统给出观测数据样条模型（含薄板样条、正则化最小二乘与广义交叉验证）的统计理论与算法，是背景面/接缝面用样条拟合的方法学出处」 |
| `实验/additive-sky-seamless/README.md:330` | 表行「平滑样条/正则化 \| Wahba 1990；Duchon 1977」 |
| `实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:708` | 表行「Wahba, Spline Models for Observational Data \| DOI 10.1137/1.9781611970128，SIAM 1990；**§1.1、§2.4、§3.2** \| thin-plate 在再生核 Hilbert 空间框架内处理，远场/边界行为由核与多项式零空间共同决定」 |

**关联性：关联对**（DOI 唯一指向 Wahba 1990 该专著；「平滑样条/正则化」与「样条拟合方法学出处」的定位与书名/主题一致，不是把 A 文派去干 B 文的活）。
残留：EXP-04:708 的**节号细断言**（§1.1/§2.4/§3.2 承载 thin-plate RKHS 与远场/边界行为）在 3 次网络查询内未能取到目录（SIAM epub 页被 Cloudflare 拦，检索途径未命中 TOC）⇒ 已入文末 UNPROVEN 清单，属高危形态②候选，**不作为"已核"的一部分**。

**版本：版本对**（Crossref `published` 年 1990、type monograph = SIAM 初版，被引主题在初版内；不存在同名再版内容差问题）。附注：CBMS-NSF 系列号 59 与 ISBN 0-89871-244-0 不在 Crossref 回包字段里，仅仓内 note 自述经 Open Library 核对，本次未独立复核。

途径：Crossref×1；WebSearch×2（均未命中 TOC）。仓库读取：`evidence_lit.json` + `README.md` + `git grep`（多模式一次）。

## 85 · DOI:10.1093/mnras/stw474（Soto+2016, ZAP）

**存在性：已核。** Crossref `https://api.crossref.org/works/10.1093/mnras/stw474` 回包：Title = *ZAP – enhanced PCA sky subtraction for integral field spectroscopy*，MNRAS **458(3), 3210–3220, 2016**，Authors = Soto, Lilly, Bacon, Richard, Conseil。仓内 cite「MNRAS 458, 3210」与卷页一致（`results/REVIEW.md:125` 记录早先误写 3110 已订正）。

**本仓位点与实际断言**

| 位点 | 挂的那句 |
|---|---|
| `实验/additive-sky-seamless/README.md:329` | 表行「**天光减除与测光偏差** \| Soto+2016, MNRAS 458, 3210」 |
| `实验/additive-sky-seamless/REPORT_paper.md:394` | 书目第 4 条「Soto, K. T. et al. 2016, MNRAS 458, 3210, *ZAP — enhanced PCA sky subtraction* — DOI …，arXiv:1602.08037」 |
| `实验/additive-sky-seamless/results/evidence_lit.json:50-61` | relevance「ZAP 用 PCA 从 IFU 数据立方自身的主成分中估计并扣除天光光谱，是'把 PCA 用于多帧天光/背景建模与天空扣除'的同行评审实现（MUSE）」 |

**关联性：关联对（附边界）。** 摘要原文（arXiv abs 页逐字）：“We introduce Zurich Atmosphere Purge (ZAP), an approach to sky subtraction based on principal component analysis (PCA) that we have developed for the Multi Unit Spectrographic Explorer (MUSE) integral field spectrograph. ZAP employs filtering and data segmentation to enhance the inherent capabilities of PCA for sky subtraction. Extensive testing shows that ZAP reduces sky emission residuals while robustly **preserving the flux** and line shapes of astronomical sources.” ⇒ README 行标签「天光减除与测光偏差」中"天光减除"直接对应、"测光"仅由"preserving the flux"这一句支撑。
**边界必须登记**：该文对象是 MUSE **IFU 光谱数据立方**，不是宽带成像 CCD 帧；只能作"PCA 天光建模扣除的同行先例"，不能作"成像测光偏差"的定量依据。仓内 `evidence_lit.json` 的定位（PCA 天光扣除先例）在边界内，无错绑。

**版本：版本对**（被引内容=题名/方法/结论，2016 MNRAS 正式版内；对应 arXiv 仅 v1（2016-02-25 提交），不存在"只有 v2 才有"的问题）。

途径：Crossref×1＋arXiv abs 摘要×1（与 #94 共用同一文献的取证）。仓库读取：`evidence_lit.json`＋`git grep`＋`README.md`＋`REPORT_paper.md`。

## 94 · arXiv:1602.08037（ZAP 预印本号）

**存在性：已核。** arXiv 官方 API `https://export.arxiv.org/api/query?id_list=1602.08037` 回包 `<id> = 1602.08037v1`，Title = *ZAP -- Enhanced PCA Sky Subtraction for Integral Field Spectroscopy*，Authors = Kurt T. Soto, Simon J. Lilly, Roland Bacon, Johan Richard, Simon Conseil，journal ref = MNRAS (2016)，提交 2016-02-25。号与文一一对应，非"真号差末位"形态。

**本仓位点与实际断言**：`实验/additive-sky-seamless/REPORT_paper.md:394`（同 #85 那行的 arXiv 对偶标识）与 `results/evidence_lit.json:55-56`（`"identifier": "DOI:10.1093/mnras/stw474 ; arXiv:1602.08037"`，`resolver_url` 指向 `https://arxiv.org/abs/1602.08037`）。

**关联性：关联对**（该 arXiv 号与所配 DOI 指向同一篇 ZAP 论文，题名/作者逐项一致；预印本号在此仅作同一文献的第二标识，未派它做独立佐证）。

**版本：版本对**（API 返回的最新版本即 v1，全仓引用未写版本号亦无歧义；被引的题名/方法在 v1 内）。

途径：arXiv API×1（版本）＋abs 页摘要×1（与 #85 共取证）。仓库读取：`git grep`＋`evidence_lit.json`＋`REPORT_paper.md`。

## 100 · arXiv:2209.12268（Akinshin, Finite-sample Rousseeuw-Croux scale estimators）

**存在性：已核。** arXiv 官方 API `id_list=2209.12268` 回包 `<id> = 2209.12268v1`，Title = *Finite-sample Rousseeuw-Croux scale estimators*，Author = Andrey Akinshin，提交 2022-09-25；abs 页摘要逐字：“The Rousseeuw-Croux $S_n$, $Q_n$ scale estimators **and the median absolute deviation $\operatorname{MAD}_n$** can be used as consistent estimators for the standard deviation under normality.”＋“The original work by Rousseeuw and Croux (1993) provides only rough approximations of the finite-sample bias-correction factors…”＋该文提供**精化的有限样本偏差校正因子与效率值**。

**本仓位点与实际断言**：`docs/algorithms/COSMETIC_ALGORITHMS.md:424`（全文打开）——“**`1.4826·MAD` 的归属**：Rousseeuw & Croux 1993 …该文不是本模块 MAD 有限样本校正的来源；本模块使用**渐近常数**、不做有限样本校正。若要做，来源为 Akinshin 2022（arXiv:2207.12005 / **arXiv:2209.12268**）…”；同句在 `docs/science/CALIBRATION.md:462`（清单给的 :422 已漂 40 行，见文末缺陷形态）。

**关联性：关联对。** 该断言是"若要给 MAD 做有限样本校正，出处在此"，而回包摘要同时命中 `MAD_n` 与 finite-sample bias-correction factors，正是该角色；且本条与它给 Rousseeuw & Croux 1993 的"降级"（不作 MAD 校正来源）互相自洽。

**版本：版本对。** API 最新即 v1（无 v2/v3 歧义），年份 2022 与仓内"Akinshin 2022"一致；被引内容（MAD_n 有限样本校正在此可做）在 v1 内。

途径：批量 id_list×1＋单号 API×1＋abs 页×2（其中一次为换途径复跑，本条实耗 4 次，超限 1 次已登记）。仓库读取：`git grep` 一次（命中 COSMETIC_ALGORITHMS.md:424 与 CALIBRATION.md:462 整句）。

## 106 · arXiv:astro-ph/0703454v2（Padmanabhan+2008 ubercal）

**存在性：已核。** arXiv 官方 API 回包 `<id> = astro-ph/0703454v2`，Title = *An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data*，第一作者 N. Padmanabhan，`journal_ref = Astrophys.J.674:1217-1233,2008`，`doi = 10.1086/524677`。版本号 **v2 = 当前最新版**，仓内钉的 v2 真实存在。

**本仓位点与实际断言**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:188-197`（[P1SG-R1] 全条打开）——"可核对标识：arXiv:astro-ph/0703454v2（arXiv API 按标题检索命中，标题/第一作者 N. Padmanabhan/摘要逐字一致）"＋三条**摘要逐字**引用＋"不借鉴点：只有**乘性**项，没有加性天光/梯度项"。

**关联性：关联对。** 三条"摘要逐字"经 API 回包核对：
- “We present an algorithm to photometrically calibrate wide field optical imaging surveys, that **simultaneously solves for the calibration parameters and relative stellar fluxes using overlapping observations**.” ✓ 逐字；
- “The algorithm **decouples the problem of "relative" calibrations, from that of "absolute" calibrations**; the absolute calibration is reduced to a few numbers for the entire survey.” ✓ 逐字（仓内引用止于从句，无改写）；
- “We **pay special attention to the spatial structure of the calibration errors**, allowing one to isolate particular error modes in downstream analyses.” ✓ 逐字。
"不借鉴点"中"摘要未见加性天光项"与回包摘要一致（摘要通篇只谈 photometric calibration/相对星流量，无 sky/background 项）。

**版本：版本对。** v2 即最新，且 v2 的 journal_ref 指向 2008 ApJ 674 1217-1233 出版版；被引三段摘要在预印本与出版版共有。

途径：单号 API×2（版本/题录、journal_ref+doi）＋一次 429 失败＋复跑 1 次取摘要逐字（本条实耗 4 次，超限 1 次已登记）。仓库读取：`git grep`＋`Read(REVERSE_VERIFY_BIBLIOGRAPHY.md:184-197)`。

## 112 · DOI 10.1051/0004-6361:20021327（Calabretta & Greisen 2002, FITS WCS Paper II）

**存在性：已核。** Crossref `works/10.1051/0004-6361:20021327` → Title *Representations of celestial coordinates in FITS*，A&A **395, 1077-1122**，2002，Authors Calabretta & Greisen；arXiv 预印本 `astro-ph/0207413v1` 的 journal_ref = `Astron.Astrophys. 395 (2002) 1077`（两途径同号同文）。DOI 尾号 327↔1077 与 326↔1061 未绑反。

**本仓位点与实际断言（两处都打开）**
- `docs/science/PHASE3_HIPS_TO_FITS.md:179`：Paper II §2.2（**Reference point of the projection**：native↔celestial 三 Euler 角旋转、θ0/φ0、LONPOLE 默认规则）与 §5.1.3 式(54)(55)（TAN = gnomonic，`Rθ = (180°/π)·cot θ`）。
- `docs/algorithms/PHASE3_RESAMPLE.md:191`：§2.2（三 Euler 角旋转核）/**Table 1（TAN: R=(180/π)·cotθ）**。

**关联性：关联错（仅 Table 1 那处）。** 出版版正文（仓内自存的链接 `https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html`，即 :32 行登记的那条全文位点）读到：§2.2 标题逐字为 **"Reference point of the projection"** ✓；**§5.1.3 "TAN: Gnomonic"，Eq. (54): `R_θ = 180/π cot θ`** ✓（故 :179 的节号与式号(54) 对，(55) 未单独取到但不构成错）；而 **Table 1 不是投影函数表**——出版版 Table 1 = “Summary of important variable names and other symbols used throughout the paper”，预印本途径给的是 RADESYS 取值表 ⇒ **两途径一致地否定"TAN 的 R(θ) 在 Table 1"**。`PHASE3_RESAMPLE.md:191` 把被引表达式挂错载体，应改挂 §5.1.3 Eq.(54)。

**版本：版本对（含一记警示）。** 仓内引的是 A&A 出版版（DOI 指向），式号 (54) 与**出版版**一致；**arXiv v1 预印本同一处式号为 (56)**——即"预印本/出版版式号漂移"真实存在，任何改引 arXiv:astro-ph/0207413 的写法都必须换算式号，仓内 `f-instr-survey`/`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE` 目前只给 DOI，安全。

途径：Crossref×1、A&A `full_html` 猜路径 404×1（真实站点错误，非拒答）、A&A 出版版全文（仓内已登记链接）×1、arXiv 标题检索×1（复跑换途径）。仓库读取：`git grep`＋`Read(PHASE3_RESAMPLE.md:183-198)`。

## 118 · DOI 10.1088/0004-6256/135/6/2055（Anderson+2008, ACS GC Survey V）

**存在性：已核。** Crossref → Title *THE ACS SURVEY OF GLOBULAR CLUSTERS. V. GENERATING A COMPREHENSIVE STAR CATALOG FOR EACH CLUSTER*，container *The Astronomical Journal*，**135(6), 2055-2073, 2008**，Authors Jay Anderson, Ata Sarajedini, Luigi R. Bedin。DOI 段/页/年与仓内写法逐项一致。

**本仓位点与实际断言**：`实验/absolute-snr/docs/surveys/f-instr-survey.md:413`（[F-29] 全条）——"Anderson, J., Sarajedini, A., Bedin, L. R. et al. (2008). The ACS Survey of Globular Clusters. V… AJ 135, 2055–2073. DOI … `[CR]`；通量定义：ePSF 拟合测光＋大规模人工星测试；借鉴点：人工星测试的规模与判据设计、**欠采样**下 ePSF 的构建；不借鉴点：球状星团拥挤场"；另一处 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:215` 仅列题录（[B77]，"分场景定量补充"）。

**关联性：关联对（附一处未读到节）。** 题名（生成完备星表）+ 卷页 + 前三作者逐项命中，ePSF 拟合测光与拥挤星团场景与该文工作流一致；**"大规模人工星测试的判据设计"这一子句未读到具体小节/表**（全文未取得），故该子句按 UNPROVEN 登记，不影响本条引用件身份与主要角色。

**版本：版本对。** 2008 AJ 135(6) 2055-2073 = 出版版，仓内只给 DOI（无预印本号），不存在版次漂移面。

途径：Crossref×1。仓库读取：`git grep`＋`Read(f-instr-survey.md:411-422)`。

## 124 · DOI 10.1214/ss/1038425655（Eilers & Marx 1996, P-splines）

**存在性：已核。** Crossref → Title *Flexible smoothing with B-splines and penalties*，container *Statistical Science*，**vol 11, issue 2, 1996**，Authors Paul H. C. Eilers, Brian D. Marx；`https://doi.org/10.1214/ss/1038425655` 302 跳转到 `projecteuclid.org/journals/statistical-science/volume-11/issue-2/Flexible-smoothing-with-B-splines-and-penalties/10.1214/ss/1038425655.full`（可复核，题名与卷期在 URL 内自证）。

**本仓位点与实际断言**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:104`（[6.3] 全行）——"Eilers & Marx 1996 (**P-splines**), Statistical Science 11, 89. DOI …：P-spline = B 样条基 + 系数上的**离散差分惩罚**…是**线性**光滑器 ⇒ 协方差有闭式"；`实验/absolute-snr/docs/snr-propagation-design.md:1126` 同义行；`实验/shared/references/reverse_verify_bibliography.bib:252` 记 `volume={11}, pages={89--121}`。

**关联性：关联对。** 被引内容"B 样条基＋差分惩罚"＝题名 "Flexible smoothing with **B-splines and penalties**" 的字面主题，角色绑定正确（P-spline 的原始出处）。

**版本：版本对（页码子项 UNPROVEN）。** 1996 Statistical Science 11(2) 钉对；但**起始页 89 与终止页 121 均未被独立回包证实**（Crossref `page` 字段缺失，Project Euclid 页为 JS 渲染未给出引文行）⇒ 仓内 "11, 89" 与 `.bib` 的 "89--121" 记入 UNPROVEN 清单，标识符：`10.1214/ss/1038425655`，下轮可走 JSTOR/Project Euclid 引文条目或 INSPIRE。

途径：Crossref×2（含换途径复跑）＋doi.org 解析×1＋WebSearch×1（未命中）。仓库读取：`git grep` 一次（命中 :104 / :1126 / .bib:252 三处整行）。

## 130 · DOI 10.3847/1538-4365/ac00b3（DES DR2 论文）

**存在性：已核。** Crossref → Title *The Dark Energy Survey Data Release 2*，container *ApJS*，**255(2), article 20, 2021**，第一作者 T. M. C. Abbott；arXiv `2101.05765v3` 的 `journal_ref` 正是该 DOI（双途径同指一文）。

**本仓位点与实际断言**
- `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:268`（[FSNR-04] 全条）——"Abbott, T. M. C. et al. (2021). The Dark Energy Survey Data Release 2. ApJS 255, 20. DOI …；arXiv:2101.05765"，借鉴点引的是 **SExtractor 文档**里的 `FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))` 与 `p_i` "subtracted from the background" 逐字（可核对标识另给了 readthedocs + GitHub 配置）。
- 同文件 `:275` 与 `实验/absolute-snr/docs/surveys/frame-snr-survey.md:26`——**纠正句**："DES DR2 论文 = arXiv:2101.02242 **错**。DES DR2 = **arXiv:2101.05765**, ApJS 255, 20, DOI …"。

**关联性：关联对，且该"纠错"本身经核实成立。** 官方 API：`2101.05765v3 = "The Dark Energy Survey Data Release 2"`（journal_ref 指回本 DOI）；`2101.02242v1 = "Determination of stellar parameters for Ariel targets…"`（A. Brucalassi，journal_ref 10.1007/s10686-020-09695-4）⇒ 仓内"任务书给的 2101.02242 是 Brucalassi 的 Ariel 光谱论文"这句判断**正确**，此 DOI 的角色（巡天身份＋测光流程出处）绑定无误。
（注：`FLUXERR` 那两句逐字出自 SExtractor 文档而非本 ApJS 文，仓内也是这么标的——不存在把 ApJS 文当 SExtractor 公式出处的错绑。）

**版本：版本对。** ApJS 255, 20 (2021) 与 DOI 记录逐项一致；arXiv 侧最新 v3 亦指同一 DOI。

途径：Crossref×1＋arXiv 批量 id_list×1。仓库读取：`git grep`＋`Read(REVERSE_VERIFY_BIBLIOGRAPHY.md:262-275)`。

<!-- PROGRESS: 9/18 -->

## 136 · ADS-bibcode 2008ApJ...674.1217P（Padmanabhan+2008）

**存在性：已核。** 两途径：① arXiv 官方 API `astro-ph/0703454v2` 的 `journal_ref = Astrophys.J.674:1217-1233,2008`、`doi = 10.1086/524677`、第一作者 N. **P**admanabhan ⇒ bibcode 各字段（2008 / ApJ / 674 / 1217 / P）逐项可推且相符；② IOP 文章页 `https://iopscience.iop.org/article/10.1086/524677` 页面显示的 Bibcode 串正是 `2008ApJ...674.1217P`，标题 *An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data*。（Simbad `sim-ref?query=bibcode:2008ApJ...674.1217P` 回 **NO_REF**，与前言对 Simbad 的告警一致，不能据此判不存在。）

**本仓位点与实际断言**：`docs/references/SCIENTIFIC_REFERENCES.md:37`（§E 第 17 条整行）——"Padmanabhan, N. et al. 2008, 'An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data', ApJ 674, 1217. [ADS](http://ui.adsabs.harvard.edu/abs/2008ApJ...674.1217P/abstract)。用途：**重叠观测联合相对光度标定、gauge/连通性**"。

**关联性：关联对。** 摘要逐字（API 回包）：“…simultaneously solves for the calibration parameters and relative stellar fluxes using **overlapping observations**”＋“The algorithm **decouples** the problem of 'relative' calibrations, from that of 'absolute' calibrations” ⇒ "重叠观测联合相对标定"直接命中；"gauge/连通性"对应"相对/绝对解耦＋需要外部绝对锚"的那句，属同义表述而非另派角色。

**版本：版本对。** bibcode 钉的是 ApJ 出版版（674, 1217-1233, 2008），与仓内"用途"所依赖的算法描述在预印本 v2 与出版版共有；本行未同时给 arXiv 号，无跨版次式号风险（该文无被引公式号）。

途径：Simbad×1（NO_REF）＋WebSearch×1（未直接命中 bibcode 页）＋IOP 文章页×1。仓库读取：`git grep`＋`Read(SCIENTIFIC_REFERENCES.md:28-45)`。

## 142 · arXiv:0904.0638（Landolt 2009, AJ 137, 4186）

**存在性：已核。** arXiv 官方 API 批量回包：`0904.0638v1`，Title *UBVRI Photometric Standard Stars Around the Celestial Equator: Updates and Additions*，第一作者 Landolt，`journal_ref = Astron.J.137:4186,2009`。

**本仓位点与实际断言**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:191`——"[B56] Landolt, A. U. 2009, AJ 137, 4186, arXiv:0904.0638 `[S]`"，位于「定标 / Gaia XP / 绝对通量（续）」小节，与同节 [B2] Landolt 1992（标准星表）并列作**初级测光标准星**出处。

**关联性：关联对**（题名即赤道带 UBVRI 标准星更新与增补，角色＝定标锚点星表，未派它做别的事）。

**版本：版本对。** arXiv 仅 v1；API 自带 journal_ref 直接对上 AJ 137, 4186 (2009) ⇒ 年/卷页钉对。

途径：arXiv 批量 id_list×1。仓库读取：`Read(PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:170-215)`＋`git grep`。

## 148 · arXiv:1302.4764（Bickerton & Lupton 2013, MNRAS 431, 1275）

**存在性：已核（但仓内作者首字母写错）。** arXiv 官方 API：`1302.4764v1`，Title *An Algorithm for Precise Aperture Photometry of Critically Sampled Images*，Authors = **Steven Bickerton, Robert Lupton**，`journal_ref` DOI = 10.1093/mnras/stt244。Crossref `works/10.1093/mnras/stt244`：*An algorithm for precise aperture photometry of critically sampled images*，MNRAS **431(2), 1275-1285, 2013**，Authors **S. J. Bickerton, R. H. Lupton**。⇒ 文献真实、卷页年与仓内一致。

**本仓位点与实际断言**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:176`——"[B43] **Bickerton, J. W.** & Lupton, R. H. 2013, MNRAS 431, 1275, arXiv:1302.4764 `[S]`"，挂在「提取与场景补充」小节（孔径/PSF 提取方法族）。

**关联性：关联对；缺陷在著录本身**——两途径一致给 **S. J. / Steven**（Crossref 缩写名 `S. J. Bickerton`，arXiv 全名 `Steven Bickerton`），仓内写 "J. W." 在两个途径里都不存在 ⇒ 属**作者首字母伪值**（不是"文献不存在"，也不是角色错绑），整改：`Bickerton, J. W.` → `Bickerton, S. J.`。

**版本：版本对。** 仓内 2013/MNRAS 431/1275 与 Crossref 出版版逐项相符；预印本 v1 唯一。

途径：arXiv 批量×1＋单号 API×1＋Crossref×1（Crossref 走 `stt244` 而非仓内未给的 DOI，属补全途径）。仓库读取：`Read(…ARCHIVE.md:170-215)`。

<!-- PROGRESS: 12/18 -->

## 154 · arXiv:2011.08625（Altavilla+2021, Gaia SPSS IV）

**存在性：已核。** arXiv 官方 API：`2011.08625v2`，Title *The Gaia spectrophotometric standard stars survey -- IV. Results of the absolute photometry campaign*，第一作者 G. Altavilla，`journal_ref = MNRAS`，`doi = 10.1093/mnras/staa3655`。

**本仓位点与实际断言**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:201`——"[B66] Altavilla, G., et al. 2021 (Gaia SPSS IV) arXiv:2011.08625 `[S]`"，列在「定标 / Gaia XP / 绝对通量（续）」。

**关联性：关联对**（题名＝Gaia 光谱光度标准星巡天 IV·绝对测光 Campaign 结果，与"绝对通量/定标"角色一致；仓内"SPSS IV"编号即题名罗马数字 IV，非杜撰）。

**版本：版本对**（API 最新 v2；出版载体为 MNRAS/10.1093/mnras/staa3655。仓内只写 2011 号＋2021 年，未写卷页 ⇒ 无冲突；`2011.xxxxx` 是 2020-11 的 arXiv 序号，与"2021"发表年不矛盾，登记以免被误读成"按首字母/数字反推"）。

途径：arXiv 批量×1＋单号 API×1。仓库读取：`Read(…ARCHIVE.md:170-215)`。

## 160 · arXiv:2309.11225（Xiao+2023, J-PLUS 重标定）

**存在性：已核。** arXiv 官方 API：`2309.11225v2`，Title *J-PLUS: Photometric Re-calibration with the Stellar Color Regression Method and an Improved Gaia XP Synthetic Photometry Method*，第一作者 Kai Xiao，journal 字段 "ApJS accepted"。

**本仓位点与实际断言**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:133`——"[B7] Xiao, K., Yuan, H., López-Sanjuan, C., et al. 2023, 'J-PLUS: Photometric Re-calibration …' arXiv:2309.11225 `[V]`"；紧邻 [B6] 另立一条 **S-PLUS** 同名式论文 arXiv:2309.11533。

**关联性：关联对**（该号确为 **J-PLUS** 那篇，未与 [B6] 的 S-PLUS 论文绑反；SCR＋XP 合成测光角色与 [B5]/[B60] 同族）。

**版本：未钉版次**（最新为 v2、已被 ApJS 接受，仓内写 "2023 … `[V]`" 且不标版本；若"改进的 Gaia XP 合成测光方法"这一被引要点属 v2 增补内容，须钉 `arXiv:2309.11225v2`。本批未逐版比对 v1/v2 差异 ⇒ 判"未钉版次"，整改＝补版本号或改为期刊卷页）。

途径：arXiv 批量×1＋单号 API×1。仓库读取：`Read(…ARCHIVE.md:128-157)`。

## 166 · arXiv:2411.09049（Bohlin+2024, 暗弱白矮星光度标准）

**存在性：已核。** arXiv 官方 API：`2411.09049v1`，Title *Faint white dwarf flux standards: data and models*，作者表首名 Ralph C. Bohlin（含 Deustua, Narayan, Saha, Calamida, Gordon, Holberg, Hubeny, Matheson, Rest）。

**本仓位点与实际断言**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:200`——"[B65] Bohlin, R. C., et al. **2024 (CALSPEC 扩充)** arXiv:2411.09049 `[S]`"。

**关联性：关联对**，且"扩充"二字有逐字支撑——摘要原文：“Fainter standard stars are essential for the calibration of larger telescopes. **This work adds to the CALSPEC (calibration spectra) database 19 faint white dwarfs** (WDs) with all-sky coverage and V magnitudes between 16.5 and 18.7.”

**版本：版本对。** v1（2024-11 提交）即最新，年 2024 与仓内一致；被引句在 v1 内。

途径：单号 API×1。仓库读取：`Read(…ARCHIVE.md:170-215)`。

## 172 · DOI 10.1002/opph.201190082（Jähne 2010, EMVA 1288）

**存在性：已核。** Crossref（两途径复跑读数一致）→ Title *EMVA 1288 Standard for Machine Vision*，container *Optik & Photonik*，**5(1), 53-54, 2010**，Author Bernd Jähne，DOI 同。

**本仓位点与实际断言**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:48`（[2.5] 全行）——"EMVA 1288 标准, emva.org URL `[page]`；**Jähne 2010, Optik & Photonik 5, 53, DOI 10.1002/opph.201190082 `[CR]`**；Borek 2023 … — 标准+文档｜转换增益、时间暗噪声、SNR、线性度/PRNU/DSNU 表征的标准化可复现定义……EMVA 1288 **不覆盖**天文量级暗流散粒噪声/天光/亚电子噪声"；另 `.bib:84` 以 "see also" 引用同 DOI。

**关联性：关联对。** 该文题名即"EMVA 1288 标准"本身（Jähne 为该标准主要作者），角色被限定为"标准的文档侧说明"，标准正文另以 emva.org `[page]` 承担 ⇒ 没有把 2 页说明文当标准全文的错绑（仓内分工已明说）。

**版本：版本对。** 2010 / Optik & Photonik 5(1) 53-54 与仓内"5, 53"相符；仓内 `Jähne` 在 `.bib` 里写作无变音的 `Jaehne`（著录差异，非事实错误）。

途径：Crossref×2（含换途径复跑）。仓库读取：`git grep`（命中 :48 与 .bib:84 整行）。

## 178 · DOI 10.1051/0004-6361/201015441（Jordi+2010, Gaia broad band photometry）

**存在性：已核。** Crossref → Title *Gaia broad band photometry*，A&A **523, A48, 2010**，Authors Jordi, Gebran, Carrasco, de Bruijne, Voss, Fabricius, Knude, Vallenari, Kohley, Mora；arXiv 侧 `1008.0815v2` 题名相同、journal_ref = A&A ⇒ 号-文-刊三向对齐。

**本仓位点与实际断言**：`实验/absolute-snr/docs/surveys/f-instr-survey.md:289`（[F-19] 全条）——"通量定义：Gaia 名义通带 G/G_BP/G_RP 的**响应曲线定义**（含望远镜+光学+探测器 QE）；借鉴点：**通带必须由'滤光片 × QE'合成**这一约定的出处——与本仓 F_syn=∫F_λ T Q λ dλ 的形式一致；不借鉴点：名义通带是**发射前**模型"。

**关联性：UNPROVEN**（不是关联错）。3 次途径（Crossref、arXiv API、arXiv abs 摘要）能确证的只有题名与主题域；摘要给出的句子是 “based on the **BaSeL3.1** stellar spectral energy distribution library, relationships were obtained for stars with different reddening values”，属颜色变换/星等关系一侧，**未读到"通带 = 滤光片透过率 × 反射率 × QE 的乘积"这一具体节/式**。按前言纪律，全文级证据缺失即降级 UNPROVEN，不以"题名看起来对"充抵。
（同文在 `…ARCHIVE.md:203` 作 [B68] Jordi 2010 A&A 523 A48 arXiv:1008.0815 出现，与本判定共用。）

**版本：版本对。** A&A 523 A48 (2010) 与 arXiv v2（journal_ref 指 A&A）一致，被引主题（Gaia 通带定义）在该版内。

途径：Crossref×1＋arXiv API×1＋arXiv abs 摘要×1（达上限）。仓库读取：`git grep`＋`Read(f-instr-survey.md:277-298)`。

## 184 · DOI 10.1051/0004-6361/202039587 + arXiv:2012.01916（Riello+2021, Gaia EDR3 测光内容）

**存在性：已核。** 两途径：① Crossref `works/10.1051/0004-6361/202039587` → *Gaia Early Data Release 3*（EDR3 系列子篇），A&A **649, A3, 2021**，Authors Riello, De Angeli, Evans；② arXiv 官方 API `2012.01916v1` 的 `journal_ref` 字段就是该 DOI，题名 *Gaia Early Data Release 3: Photometric content and validation*，第一作者 Riello ⇒ **DOI↔arXiv 配对正确，非"两个号各指一篇"**。

**本仓位点与实际断言**：`docs/science/PHOTOMETRY.md:312`——"**8. Gaia G/BP/RP 通带与零点（§2a.6 真实数据判据用到的 G 通带与零点）**：Riello, M., De Angeli, F., Evans, D. W., et al. 2021, A&A 649, A3（DOI …；arXiv:2012.01916）；通带曲线文件 = 本仓 `…/GaiaEDR3_passband.dat`…；Vega 零点 `ZP_VEG(G)=25.6874±0.0028`（官方 §5.4.1 Table 5.4）…"

**关联性：关联对（限定在"通带"这一层）。** 摘要逐字命中通带角色：“**Using one passband over the whole colour and magnitude range** leaves no systematics above the 1% level in magnitude in any of the bands…” ⇒ 作为"Gaia 通带内容与验证"的出处成立。**残留（不并入本条判定）**：同句里的"零点 25.6874±0.0028（官方 §5.4.1 Table 5.4）"这一指针指向 **ESA Gaia DR3 官方文档**而非本 DOI，本批未开该页 ⇒ 高危形态②候选，已入 UNPROVEN 清单（下轮单列核验 `gea.esac.esa.int/…/cu5pho_ssec_photCal.html` 是否真有 Table 5.4 及该数值）。

**版本：版本对。** 2021 A&A 649 A3 = 出版版；arXiv 侧仅 v1；仓内三处（`PHOTOMETRY.md:312`、`f-instr-survey.md:277`、`snr-propagation-design.md:1144`）著录一致，且 `实验/photometric-magnitude/README.md:386` 明确用该 DOI 与 Bohlin 2020 的卷号做过防串号区分。

途径：arXiv 批量×1＋Crossref×1＋arXiv abs 摘要×1。仓库读取：`git grep`（一次命中 :129/:54/:312/:277/:379/:386/:1144/.bib:316 全部位点）。

<!-- PROGRESS: 18/18 -->

---

## 本批三态计数（18 条）

| 字段 | 已核／对 | 问题档 | UNPROVEN |
|---|---|---|---|
| 存在性 | 18 | 不存在 0 | 0 |
| 关联性 | 关联对 16 | 关联错 1（#112） | 1（#178） |
| 版本 | 版本对 17 | 未钉版次 1（#160） | 版本错 0 |

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

| 条 | 未证事项 | 用过的标识符／途径 | 建议下轮换路 |
|---|---|---|---|
| #60 | `EXP-04-RECONSTRUCTION.md:708` 的 **§1.1／§2.4／§3.2** 是否承载 thin-plate RKHS 与远场/边界行为；CBMS-NSF 系列号 **59**；ISBN 0-89871-244-0 | Crossref `10.1137/1.9781611970128`（题录命中）＋WebSearch×2（未命中目录）；SIAM epub 页被 Cloudflare 拦 | Google Books / archive.org 借阅页的 Contents；或 Open Library 该 ISBN 页的目录段 |
| #118 | [F-29] "大规模**人工星测试**的判据设计"未读到具体小节/表 | Crossref `10.1088/0004-6256/135/6/2055`（题录命中） | AJ 出版版全文（需 ADS token 或 IOP 页内 HTML）；或引 Anderson & King 2000 作该子句出处 |
| #124 | Statistical Science 起始页 **89** 与 `.bib` 的 **89--121** | Crossref `10.1214/ss/1038425655`（`page` 字段缺失）→ doi.org 302 → Project Euclid（JS 无引文行）→ WebSearch | JSTOR `10.2307/2246107` 类条目页；INSPIRE `TEXINSPIRE number`；或 Euclid 的 `/api` |
| #178 | "通带 = 滤光片 × 反射率 × QE"的**具体节/式** | Crossref `10.1051/0004-6361/201015441`；arXiv `1008.0815v2`；abs 摘要（只给 BaSeL3.1 句） | A&A 523 A48 全文 §2（aanda 开放全文）；或 Gaia 官方文档通带节 |
| #184 | 同句"Vega 零点 25.6874±0.0028（**官方 §5.4.1 Table 5.4**）" | 本条 3 次查询用在 DOI/arXiv 配对与通带摘要，未开 ESA 文档页 | 直接开仓内已登记页 `gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html` 找 Table 5.4 |
| #85/#94 | ZAP 摘要仅取到前 3 句逐字（后段"instrumental signatures / OH artifacts"未取全） | arXiv API＋abs 页 | ar5iv/全文 §1 若需引用后段结论 |

## 新发现的缺陷形态（本批独有）

1. **作者首字母伪值**（#148）：`Bickerton, J. W.` 而两途径一致给 `S. J. / Steven`——卷、页、年、题名全对，唯作者首字母凭空，是比"文献不存在"更隐蔽的一类（自动引用格式生成时会通过存在性检查）。
2. **载体错挂**（#112）：节号与式号都对，却把 `TAN: R=(180/π)cotθ` 挂到 **Table 1**（出版版 Table 1 是符号汇总、预印本是 RADESYS 取值表，两者都不是投影函数表）。存在性检查与"节号是否存在"检查都放行，只有打开表的题注才红。
3. **预印本/出版版式号漂移**（#112）：同一处 TAN 公式在 A&A 出版版为 **Eq.(54)**、在 arXiv v1 为 **Eq.(56)** ⇒ 式号必须与版次同钉；仓内引 A&A/DOI 是安全侧，任何"改引 arXiv 号"的整改会引入错式号。
4. **清单行号漂移**（#100）：批次清单给 `docs/science/CALIBRATION.md:422`，`git grep` 在钉基线 `c8f64e9a` 上实际命中 **:462**（漂 40 行）；其余 17 条代表位点行号与基线一致 ⇒ 清单行号不可当唯一索引，须带"标识符字符串"作并行键。
5. **Simbad `sim-ref` 对 ApJ bibcode 回 NO_REF**（#136）：该负读数与"bibcode 不存在"无关（IOP 文章页显示的 Bibcode 正是该串），印证前言对 Simbad 的告警应扩到 ApJ 主刊，不止 ASPC。

## 覆盖率自报

**18/18 条**每条给出存在性／关联性／版本三字段判定，无留白、无并列存疑。
网络查询有 **9 条超出 3 次上限**（合计超约 12 次），原因两条且可复核：

- (a) 首轮部分取证经 harness 的文件代理 URL 返回，为保证"可复核依据"必须能在报告里以**真实 URL＋标识符**复算，故对同一对象用 `api.crossref.org`／`export.arxiv.org` 原文地址换途径复跑（复跑读数与首轮一致，无一条反转）；
- (b) **#112 首轮经 ar5iv（= arXiv v1 途径）得到的式号结论（Eq. 56）与出版版（Eq. 54）矛盾**，若按首轮结论会误判仓库"伪托式号"；为不冤枉仓库而重开 A&A 出版版全文，结论改判（节号/式号对、Table 1 指认错）。这正是"版次未定"形态的实例，也是本批唯一一次结论反转。

仓库侧只读：共 12 次读取（5 次多模式 `git grep` + 7 次 `Read`），未改仓库、未跑构建与门禁、未触碰共享台账 `整改/out/文献台账.csv`。
