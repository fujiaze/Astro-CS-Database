# 核验-CIT-12 —— 文献核验路成稿

- 基线：`c8f64e9a`
- 批次：CIT-12（14 条未核引用，P-1 优先）
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` 全条硬约束
- 网络手段清单：Crossref REST `api.crossref.org/works/<doi>`（并发 ≤2）；arXiv 官方 API `export.arxiv.org/api/query?id_list=`；出版社自有页（A&A / IOP / OUP / IEEE / SPIE / Princeton Scholarship）；ADS 需 token ⇒ 仅 bibcode 者标 UNPROVEN；正文走 HTML 途径。本机 shell 无外网，一律走联网工具。
- 三字段：存在性 / 版次与载体 / 关联（打开断言原文对照；给不出并列证据落 UNPROVEN，不硬判"关联错"）。
- 硬纪律：不凭记忆断定存在、不编造标识符；`UNPROVEN` 是合格交付。

## 结论表

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 190 | DOI 10.1051/0004-6361/202243709；arXiv:2206.06215 | 已核 | 版本对（A&A 674 A33, 2023；arXiv v2 同期） | 关联对（三段逐字引文全命中） | Crossref→A&A 674/A33/2023＋作者串；arXiv API→2206.06215v2 题名＋摘要逐字；aanda.org 全文页命中 passband 定义句 | — |
| 196 | DOI 10.1051/0004-6361:20021569 | 已核（**号挂错件**） | **版本错**：应指 `10.1051/0004-6361:20021571`（A&A 398, 785–800, 2003） | 关联对（curvelet 多尺度表示，与"选型对照"口径不冲突） | Crossref：`:20021569`=Homeier+ A&A **397, 585** WR 巡天；书目检索得真件 Starck/Donoho/Candès "Astronomical image representation by the curvelet transform" A&A 398,785 `:20021571`；句柄 responseCode=1（可解析） | — |
| 202 | DOI 10.1080/03610927708827533 | 已核 | 版本对（Comm. Stat. Theory Methods 6(9), 813–827, 1977） | **UNPROVEN**（"δ 取值表／δ=1.345 出处"需正文） | Crossref 书目全对（Holland & Welsch, 题名逐字） | T&F `doi/abs` 403；Semantic Scholar 无摘要；Crossref 无 abstract 域 |
| 208 | DOI 10.1086/132232 | 已核 | 版本对（PASP 100, 754, 1988） | UNPROVEN（题名同向；0.0015–0.002 mag 数值无锚） | Crossref→Gilliland & Brown, "Time-resolved CCD photometry of an ensemble of stars" | IOP 302→Radware bot 墙；Crossref `select=abstract` 400 |
| 214 | DOI 10.1086/133378 | 已核 | 版本对（PASP 106, 250, 1994） | **无挂靠断言**（`[B82]` 仅清单行，正文 0 次） | Crossref→Stetson, "…CFHT and HST Observations, ALLFRAME reductions"，与括注 ALLFRAME 相符 | — |
| 220 | DOI 10.1086/316475 | 已核 | 版本对（PASP 111, 1559, 1999） | UNPROVEN（主题同向；CTE 3%→40% 数值无锚） | Crossref→Whitmore/Heyer/Casertano "Charge-Transfer Efficiency of WFPC2"，题名逐字对 | IOP 墙；ADS 需 token |
| 226 | DOI 10.1086/323894 | 已核 | 版本对（PASP 113, 1420–1427, 2001）；仓内"卷页已核"复核成立 | 关联对（题名级：CR 剔除方法映射） | Crossref→van Dokkum "Cosmic-Ray Rejection by Laplacian Edge Detection" | — |
| 232 | DOI 10.1086/382735 | 已核 | 版本对（PASP 116, 266, 2004） | UNPROVEN＋疑角色错绑（"FWHM≲2 px 失效"门限是否出自本文） | Crossref→Bakos/Noyes/Kovács/Stanek/Sasselov/Domsa "Wide-Field Millimagnitude Photometry with the HAT" | 未取正文（IOP 墙、ADS 需 token），仅书目核对 1 次 |
| 238 | DOI 10.1086/502778 | 已核 | 版本对（PASP 118, 560, 2006） | 关联对＝**正确的证伪反例**（否定式登记，占用卷页核实） | Crossref→Branch et al. "Comparative Direct Analysis of Type Ia Supernova Spectra. II. Maximum Light" | — |
| 244 | DOI 10.1088/0004-637x/799/2/133 | 已核 | 版本对（ApJ 799, 133, 2015；前三作者首字母逐一对上） | **该挂未挂**（正文 `:14` 点名 SCR 方法却只挂 [B6][B7]） | Crossref→"STELLAR COLOR REGRESSION…"，Haibo Yuan/Xiaowei Liu/Maosheng Xiang 等 | — |
| 250 | DOI 10.1093/mnras/stac2659 | 已核 | 版本对（MNRAS 517, 484, 2022） | **无挂靠断言**（`[B83]` 仅清单行） | Crossref→Nardiello+ "Photometry and astrometry with JWST – I. NIRCam PSFs…" | — |
| 256 | DOI 10.1109/MSP.2007.914731 | 已核 | 版本对（IEEE SPM 25(2), 21, **2008**；DOI 串的 2007 是 IEEE 编号段） | 关联对（题名＋载体级：教程自述与 "An Introduction to" 同向） | Crossref→Candès & Wakin "An Introduction To Compressive Sampling" | — |
| 262 | DOI 10.1117/3.725073 | 已核（书） | **未钉版次**：2007/Janesick/SPIE/ISBN 9780819478382 已核，副题 "DN → λ" 元数据里没有 | UNPROVEN（PTC 测 g/σ_R、DSNU/PRNU 需正文；且未给 PM 卷号） | Crossref（Edited book）＋doi.org 302→`spiedigitallibrary.org/ebooks/PM/Photon-Transfer/…` | SPIE 书页 Incapsula；OpenLibrary、Google Books `fetch failed` |
| 268 | DOI 10.23943/princeton/9780691151687.001.0001 | 已核（专著） | 版本对（Princeton UP 2014，ISBN 键 ⇒ 不漂版） | UNPROVEN（"教科书重述 `w=1/σ²` 最优"需章节正文） | Crossref→monograph "Statistics, Data Mining, and Machine Learning in Astronomy"，Ivezic/Connolly/VanderPlas/Gray | PSO `/view/` 301→坏跳转；press.princeton.edu 403；Google Books/OpenLibrary 不通 |

## 逐条取证记录

### 190 · PHOTOMETRY.md:310（A&A + arXiv:2206.06215） —— 核验态：已核

断言原文（`docs/science/PHOTOMETRY.md:310`，§14a 第 6 条）：「XP 合成测光与 passband 定义（§2a.4/§2a.5 的一手依据）：Gaia Collaboration, Montegriffo, P., Bellazzini, M., De Angeli, F., et al. 2023, A&A 674, A33（DOI 10.1051/0004-6361/202243709；arXiv:2206.06215）」＋三段逐字引文。

- 存在性 = 已核，双标识符各自解析到同一件：Crossref `10.1051/0004-6361/202243709` → container A&A, **vol 674, page A33, 2023**，作者串 `Gaia Collaboration, P. Montegriffo, M. Bellazzini, F. De Angeli, R. Andrae, …`（与仓内前四作者写法逐字一致）；arXiv 官方 API `id_list=2206.06215` → `2206.06215v2`，题名 "Gaia Data Release 3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra"，published 2022-06-13 / updated 2023-01-10（与 A&A 2023 接收-出版时序一致）。
- 版次/载体 = 版本对：仓内以 A&A 674 A33 为正刊载体，同时给 arXiv 号且未引预印本专有内容；v2 即 2023-01 修订版，与正刊同期。
- 关联 = 关联对：该文正是"用 XP 谱做任意通带合成测光"的一手依据，且通带=滤镜×探测器×光学件（+大气）的定义出自该文。**三段逐字引文全部命中**：
  (a) "Synthetic photometry directly tied to a flux in physical units can be obtained from these spectra for any passband fully enclosed in this wavelength range." — 摘要逐字命中（arXiv API 回包）；
  (b) "Existing top-quality photometry can be reproduced within a few per cent over a wide range of magnitudes and colour, …" — 摘要逐字命中；
  (c) "actual TCs, which in the following we also refer to as passbands, are defined by the combination of the TC of an optica…" — 出版社全文页命中（`aanda.org` full_html aa43709-22），位置在 **§1 Introduction**（仓内只标"原文（passband 含探测器）"，未指节号 ⇒ 无节号可错）。
- 本条无缺陷。取证 3 次（Crossref / arXiv API / 出版社 HTML；另有 1 次网关 403 不计）。

### 196 · NOISE_MODEL.md 的 Starck–Donoho–Candès 2003 —— 核验态：已核（**DOI 错号，P1**）

断言原文（`docs/science/NOISE_MODEL.md` §14a「多尺度稳健噪声（MRS/N*）」条，grep 命中行 378；清单给的代表位点 :370 行号漂移，以内容锚为准）：「Starck, Donoho & Candès 2003, A&A 398, 785（DOI 10.1051/0004-6361:20021569）。**核验状态**：文章级；ACSD 现状**未采用**小波 MRS/N*，该条只作选型对照」。

- 存在性（所指文献）= 已核：Crossref 书目检索命中真件 —— **`10.1051/0004-6361:20021571`**, Starck, J. L.; Donoho, D. L.; Candès, E. J., "Astronomical image representation by the curvelet transform", A&A **398, 785–800, 2003**。仓内作者/年份/卷/起始页全对。
- 存在性（登记的 DOI）= 该号存在但**不是这篇**：`10.1051/0004-6361:20021569` Crossref 记录 = Homeier N. L., Blum R. D., Conti P. S., Damineli A., "A near–infrared survey for Galactic Wolf-Rayet stars", A&A **397, 585**, 2003；DOI 句柄 API responseCode=1（可解析，指向 aanda.org ⇒ 不是死号）。
- 版次 = 版本错（应指 DOI `10.1051/0004-6361:20021571`，2003，A&A 398, 785–800）：末两位 69↔71 之差把读者送到一篇完全不相干的**银河 Wolf-Rayet 星近红外巡天文**（A&A 397, 585）。属派单前言 §4 高危形态①"真号只差末位"。
- 关联 = 关联对（就"多尺度表示／去噪选型对照"这一口径）：真件是 curvelet 天文图像表示与去噪文，与该条小标题及"只作选型对照"的自述不冲突 ⇒ 缺陷落在标识符，不落在角色。
- 取证 6 次（Crossref DOI / 句柄 API / doi.org 重定向 / 两次 Crossref 书目检索 / WebSearch 无果）。订正面已核：**该错号在全仓只出现 2 处** —— `docs/science/NOISE_MODEL.md:378`（正本）与 `site/vendor/docs/noise-model.md`（站内镜像）；订正只改号、不动作者-年-卷-页，镜像按生成流程重出。

### 202 · PHASE2_UPM.md 的 Holland & Welsch 1977 —— 核验态：书目已核 / 内容未取到

断言原文：`docs/science/PHASE2_UPM.md:137`「*Comm. Statist.* A6, 813, DOI 10.1080/03610927708827533（**IRLS 实现与 δ 取值表**）」＋`:316`「（**δ=1.345 的 IRLS 出处**）」＋`docs/references/SCIENTIFIC_REFERENCES.md:107`「用途：Huber δ=1.345（Gaussian 95% 渐近效率）与 IRLS 权重实现出处」（清单代表位点 :299 行号漂移，以内容锚为准；同串另见 `docs/algorithms/PHASE2_SESSION.md:135`、`docs/algorithms/PLATESOLVE.md:196`、`实验/additive-sky-seamless/` 三处）。

- 存在性 = 已核：Crossref `10.1080/03610927708827533` → Holland, Paul W.; Welsch, Roy E., "Robust regression using iteratively reweighted least-squares", **Communications in Statistics - Theory and Methods, vol 6, issue 9, pp 813–827, 1977**。仓内"Comm. Statist. A6, 813"的卷页与年份全对（"A" 是当年分部历史写法，元数据刊名为 Theory and Methods）。
- 版次/载体 = 版本对（1977 单版，无再版内容差问题）。
- 关联 = **UNPROVEN**：断言是"该文给出 δ 取值表 + δ=1.345 的 IRLS 出处"，属表/式级指认，必须读正文。全文不可得：T&F `doi/abs/…` HTTP 403（Cloudflare），Semantic Scholar 记录无摘要，Crossref 无 abstract 字段 ⇒ 既不能判"关联对"，也无证据判红。
- 缺口：能读到 H&W 1977 正文表/§3 的副本，或 Huber 1981 / Huber & Ronchetti 2009（仓内已并列引用）中 δ=1.345（95% 效率）的出处页码。若 δ 的原始出处在 Huber 侧，则该条角色应下调为"IRLS 实现"半句。
- 取证 3 次（Crossref / Semantic Scholar / T&F 403）。

### 208 · 综述档案 [B78] Gilliland & Brown 1988 —— 核验态：书目已核 / 数值未取到

断言原文（`:216` 列表项；正文挂靠 = 同文件 `:108`「时序孔径（系综）可达 **0.0015–0.002 mag/曝光**（12–13 等、1 min）[B78][B79]」）。

- 存在性 = 已核：Crossref `10.1086/132232` → Gilliland, Ronald L. & Brown, Timothy M., "Time-resolved CCD photometry of an ensemble of stars", **PASP 100, 754, 1988**。作者/年/刊/卷/页逐项与仓内一致。
- 版次 = 版本对（PASP 100 (1988) 单版）。
- 关联 = UNPROVEN（方向对、数值无法核）：题名即"系综时变 CCD 测光"，与"时序孔径系综精度"同号；但 **0.0015–0.002 mag/曝光、12–13 等、1 min** 三个定量值需正文/摘要，出版社途径不通（`iopscience.iop.org` 302 → Radware bot-manager 验证页；ADS 需 token）。
- 取证 3 次（Crossref / IOP 重定向墙 / Crossref `select=abstract` 400）。

### 214 · 综述档案 [B82] Stetson 1994 (ALLFRAME) —— 核验态：已核（但**列而不用**）

- 存在性 = 已核：Crossref `10.1086/133378` → Stetson, Peter B., "The center of the core-cusp globular cluster M15: CFHT and HST Observations, ALLFRAME reductions", **PASP 106, 250, 1994**。
- 版次 = 版本对；仓内括注 "(ALLFRAME)" 与题名 "ALLFRAME reductions" 相符（方法在该文里使用/描述）。
- 关联 = **无挂靠断言**：`[B82]` 在整份档案里只出现 1 次（`:220` 清单项本身），正文从未用它做任何断言 ⇒ 属已登记形态⑭"列而不用"，不是引用错；批次清单给的"代表位点"就是清单行本身，与此一致。
- 取证 1 次。

### 220 · 综述档案 [B32] Whitmore, Heyer & Casertano 1999 —— 核验态：书目已核 / 数值未取到

断言原文（`:165` 列表项；正文挂靠 = `:99`「平场/非线性/CTE/电荷弥散：WFPC2 的 **CTE 损失从 ~3% 增至 ~40%** [B32]」与 `:67`「饱和/非线性：两者都要显式剔除 [B32]」）。

- 存在性 = 已核：Crossref `10.1086/316475` → Whitmore, Bradley; Heyer, Inge; Casertano, Stefano, "Charge-Transfer Efficiency of WFPC2", **PASP 111, 1559, 1999**；仓内引号内题名逐字一致。
- 版次 = 版本对。
- 关联 = UNPROVEN：主题（WFPC2 的 CTE）与断言同向、挂靠合法；但 **3%→40%** 这一对具体数值取自正文何处未核到（IOP 墙、ADS 需 token）。同行"电荷弥散使 PSF 变宽/高斯化"半句同理。
- 取证 2 次。

### 226 · COSMETIC_ALGORITHMS.md:420 van Dokkum 2001 —— 核验态：已核（无缺陷）

断言原文：「宇宙线剔除（单帧）：van Dokkum 2001, PASP **113, 1420**（LA Cosmic，DOI 10.1086/323894，**卷页已核**）」（`docs/algorithms/COSMETIC_ALGORITHMS.md:420`；清单写 `docs/algorithms/` 正确，我初读时误进 `lib/algorithms/` ⇒ 路径以仓内实际为准）。

- 存在性 = 已核：Crossref `10.1086/323894` → van Dokkum, Pieter G., "Cosmic-Ray Rejection by Laplacian Edge Detection", **PASP 113, 1420–1427, 2001**。仓内自述"卷页已核"经独立复核成立。
- 版次 = 版本对。
- 关联 = 关联对，证据层级 = 题名级：断言只做"该方法叫 L'A Cosmic、用于单帧宇宙线剔除"这一映射，题名直接支撑；断言未引式号/表号 ⇒ 无载体错挂面。
- 取证 2 次（`?fields=title` 途径 Crossref 回 400，改整记录二次）。

### 232 · 综述档案 [B75] Bakos 2004 (HAT) —— 核验态：书目已核 / 断言无法核（疑角色错绑）

断言原文（`:213` 列表项；正文挂靠 = `:65`「**FWHM ≲ 2 px 时中心定位/PSF 拟合/插值开始失效** [B75]」）。

- 存在性 = 已核：Crossref `10.1086/382735` → Bakos, G.; Noyes, R. W.; Kovács, G.; Stanek, K. Z.; Sasselov, D. D.; Domsa, I., "Wide-Field Millimagnitude Photometry with the HAT: A Tool for Extrasolar Planet Detection", **PASP 116, 266, 2004**；仓内 "Bakos, G. Á., et al. 2004, PASP 116, 266" 与元数据相容（卷页年全对）。
- 版次 = 版本对。
- 关联 = UNPROVEN（登记为疑点）：断言是**采样率下限的定量结论**（FWHM≲2 px ⇒ 定位/拟合/插值失效），而该文自述范围是 HAT 仪器与宽场 mmag 测光演示；未取到正文 ⇒ 不判"关联错"，但这条属"由题名外推到定量门限"的高危形态。下一轮需拿正文精度/采样段落定位；若文中无此门限则改引（**不猜号**）。
- 取证 1 次。

### 238 · f-instr-survey.md:176 Branch et al. 2006 —— 核验态：已核，**正确的证伪反例（不是缺陷）**

断言原文（极性=否定式登记）：「任务书候选『Anderson & King 2006, PASP 118, 560』**未核到** —— PASP 118, 560 **实为** Branch et al. 2006（超新星光谱，DOI 10.1086/502778）」。

- 存在性 = 已核：Crossref `10.1086/502778` → Branch, David; Dang, Leeann Chau; Hall, Nicholas; Ketchum, Wesley; et al., "Comparative Direct Analysis of Type Ia Supernova Spectra. II. Maximum Light", **PASP 118, 560, 2006**。
- 版次 = 版本对（卷 118／起始页 560／2006 与仓内"PASP 118, 560 实为 Branch et al. 2006"逐字相符；"超新星光谱"与 Ia 型超新星光谱主题相符）。
- 关联 = 关联对：该号在此处承担**反证**角色（证明 118/560 不是 Anderson & King），而它确实占据那个卷页 ⇒ 与前批"医学期刊书评"同族，是仓内**故意**登记的证伪反例，不得报成缺陷。
- 取证 1 次。

### 244 · 综述档案 [B8] Yuan+2015（Stellar Color Regression） —— 核验态：已核（**一手依据缺挂，P1 候选**）

- 存在性 = 已核：Crossref `10.1088/0004-637x/799/2/133` → **The Astrophysical Journal 799, 133, 2015**，题名（元数据全大写写法）"STELLAR COLOR REGRESSION: A SPECTROSCOPY-BASED METHOD FOR COLOR CALIBRATION TO A FEW MILLIMAGNITUDE ACCURACY AND THE RECALIBRATION OF …"（回包尾段截断，故此处不逐字引全题名），作者串 `Haibo Yuan, Xiaowei Liu, Maosheng Xiang, Yang Huang, Huihua Zhang, Bingqiu Chen`。
- 版次 = 版本对；仓内 "Yuan, H., Liu, X. & Xiang, M. 2015, ApJ 799, 133" 的**前三作者首字母逐一对上**（H.Y./X.L./M.X.，非凭空缩写），卷页年全对；仓内题名写作 "Stellar Color Regression..." 带省略号，是正式题名的**逐字前缀** ⇒ 无插词问题。
- 关联 = **该挂未挂**：`[B8]` 在整份档案只出现 1 次（`:134` 清单项），正文从不挂靠；而正文 `:14` 恰恰点名这个方法——"S-PLUS / J-PLUS 用同一思路（改进的 XP 合成测光 XPSP + **Stellar Color Regression**）…[B6][B7]"——**方法名出现在断言里，出处却只给了两篇应用它的方法文（arXiv:2309.11533 / 2309.11225），方法本尊 [B8] 没挂上去** ⇒ 与形态⑭"列而不用"相邻但更重：这里是"用了概念而未给一手依据"。（[B6][B7] 只给 arXiv 号、无 DOI，不属本批，未取证。）
- 取证 2 次（Crossref 整记录 / 作者串单独回查）。

### 250 · 综述档案 [B83] Nardiello 2022 —— 核验态：已核（但**列而不用**）

- 存在性 = 已核：Crossref `10.1093/mnras/stac2659` → Nardiello, D.; Bedin, L. R.; Burgasser, A.; Salaris, M.; Cassisi, S.; Griggio, M. 等, "Photometry and astrometry with JWST – I. NIRCam point spread functions and the first JWST colour–magnitude diagrams of a globular cluster", **MNRAS 517, 484, 2022**；仓内 "Nardiello, D., et al. 2022, MNRAS 517, 484" 全对。
- 版次 = 版本对（题名带 "– I."＝系列第一篇；仓内未写题名，无插词面）。
- 关联 = **无挂靠断言**：`[B83]` 全文只出现在 `:221` 清单项 ⇒ 形态⑭"列而不用"。
- 取证 1 次。

### 256 · REVERSE_VERIFY_BIBLIOGRAPHY.md:109 Candès & Wakin —— 核验态：已核（无缺陷）

断言原文：「Candès & Wakin 2008, IEEE Signal Processing Magazine 25, 21. DOI 10.1109/MSP.2007.914731 —— 压缩感知条件的教程级陈述…它是教程而非一手结果；定理请引 6.6/6.7」。

- 存在性 = 已核：Crossref `10.1109/msp.2007.914731` → Candès, E. J.; Wakin, M. B., "An Introduction To Compressive Sampling", **IEEE Signal Processing Magazine, vol 25, issue 2, p. 21, 2008**。
- 版次 = 版本对；DOI 串里的 "2007" 是 IEEE 的 DOI 编号段，元数据出版年为 2008，与仓内写法不矛盾（**勿误判为年份漂移**）。
- 关联 = 关联对，证据层级 = 题名＋载体级：题名自带 "An Introduction to"，载体 SPM 为综述/教程刊物，与"教程级陈述、非一手结果"自述同向；该条还显式把定理需求推给 6.6/6.7，角色边界已写清。
- 取证 1 次。

### 262 · REVERSE_VERIFY_BIBLIOGRAPHY.md:44 Janesick —— 核验态：书目已核 / 副题与内容未取到

断言原文：「Janesick 2007, Photon Transfer: DN → λ, SPIE Press. DOI 10.1117/3.725073 `[CR]` — 书 | photon transfer curve 测**转换增益** `g`(e⁻/DN)、读出噪声 `σ_R`，区分时间噪声与固定图案噪声(DSNU/PRNU) ⇒ 方差模型 `gain`/`readnoise` 输入的经验支柱」＋不取项（多放大器增益漂移、理想化线性传感器模型）。

- 存在性 = 已核：Crossref `10.1117/3.725073` → type **Edited book**，title "**Photon Transfer**"，author **James R. Janesick**，publisher **SPIE**，ISBN **9780819478382**，year **2007**；DOI 句柄 302 → `spiedigitallibrary.org/ebooks/PM/Photon-Transfer/eISBN-9780819478382/10.1117/3.725073`（URL 中的 `PM` 即 SPIE Press Monograph 系列 ⇒ "SPIE Press 书"的载体写法成立）。
- 版次 = 2007 单版已核，但**副题未钉**：Crossref 题名域只有 "Photon Transfer"，仓内多写的 "DN → λ" 无法在出版社页核实（SPIE Digital Library 被 Incapsula 拦；OpenLibrary 与 Google Books 两途径本机 `fetch failed`）⇒ 不判红也不判对，标 **未钉版次**。另：仓内此条**未给系列卷号（PM 编号）**，而同项目对 Janesick 2001 用 "SPIE PM83" 写法（`docs/science/NOISE_MODEL.md:379`）⇒ 两条 Janesick 载体粒度不一致；两件非同一书，不冲突。
- 关联 = UNPROVEN（角色方向合理）：断言实质（PTC 测 g 与 σ_R、区分时间噪声与 DSNU/PRNU 固定图案）超出题名可证范围，需正文；付费墙未取到任何一页 ⇒ 证据层级只到"题名＋载体"，按前言 §2 降级。
- 取证 5 次（Crossref / doi.org 302 / SPIE 页被拦 / OpenLibrary 不通 / Google Books 不通）。

### 268 · REVERSE_VERIFY_BIBLIOGRAPHY.md:149 Ivezić+2014 —— 核验态：书目已核 / 教科书内容未取到

断言原文（论断 3「逆方差加权 `w = 1/σ²` 是最小方差无偏线性组合」的受支持来源列表末项）：「教科书重述 **Ivezić, Connolly, VanderPlas & Gray 2014, Princeton UP. DOI 10.23943/princeton/9780691151687.001.0001 `[CR]`**」。

- 存在性 = 已核：Crossref `10.23943/princeton/9780691151687.001.0001` → type **monograph**，"Statistics, Data Mining, and Machine Learning in Astronomy: A Practical Python Guide for the Analysis of Survey Data"，作者 Željko Ivezic, Andrew J. Connolly, Jacob T. VanderPlas, Alexander Gray，publisher **Princeton University Press**，year **2014**，ISBN 9780691151687（印刷）／9781400848911（电子）。仓内四人姓＋年＋社＋DOI 全对（"Ivezić" 的变音符差异属排版，不算错）。
- 版次 = 版本对：该 DOI **以 ISBN 为键** ⇒ 天然钉在 2014 这一版，不属"概念 DOI 漂到最新版"那种形态；`001.0001` 是 PSO 的书级（front matter）对象号，与"引整本书"的用法一致。
- 关联 = UNPROVEN：断言派给它的是"`w=1/σ²` 最优（Gauss–Markov/BLUE）的**教科书重述**"这一具体角色，须看该书相应节是否真给出该最优性陈述（此书定位是实用 Python 指南，重述到哪一层不能由题名断定）。途径全断：`princeton.universitypressscholarship.com/view/…` 301 → `academic.oup.com/[Journals:]`（平台迁移后的坏跳转，非正文）、`press.princeton.edu` 403、Google Books／OpenLibrary 网络不通 ⇒ 不判红。
- 取证 4 次。

<!-- PROGRESS: 14/14 -->

## 本批 P1 缺陷清单

1. **196｜DOI 错号（唯一硬红，属前言 §4 形态①）**：`docs/science/NOISE_MODEL.md` §14a「多尺度稳健噪声（MRS/N*）」条把 Starck, Donoho & Candès 2003, A&A 398, 785 绑到 `10.1051/0004-6361:20021569`，而该号 Crossref 记录是 Homeier et al., A&A **397, 585**（银河 WC 型星近红外巡天）；真号 **`10.1051/0004-6361:20021571`**（题名 "Astronomical image representation by the curvelet transform"，A&A 398, 785–800, 2003）。作者/年/卷/起始页全对、**只有号错** ⇒ 任何按 DOI 自动解析的核对都会把读者送到不相干文章；该号可解析（句柄 responseCode=1），所以死号检测也放得过它。订正面已核：错号在全仓仅 2 处（`docs/science/NOISE_MODEL.md:378` 正本 ＋ `site/vendor/docs/noise-model.md` 镜像），改号即可，不动书目字段。
2. **244｜承重断言缺一手挂靠（"该挂未挂"）**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:14` 的定标链叙述点名 "Stellar Color Regression" 方法，出处只给 [B6][B7]（两篇应用文），而方法本尊 [B8]（ApJ 799,133，已核为真件）躺在清单里从未挂靠 ⇒ 创新点①方向的方法学断言**未锚到一手依据**。修法：把 [B8] 挂进 `:14` 那句，或明确 `:14` 的实际依据就是 [B6][B7] 并写清 [B8] 的层级。

3. **232｜疑角色错绑（未判红，需正文才能定）**：`:65` 的定量门限"FWHM ≲ 2 px 时中心定位/PSF 拟合/插值开始失效"挂 [B75] = Bakos+2004（HAT 仪器与宽场 mmag 测光文），该文自述范围不提供该门限的论证面。全文不可达 ⇒ 本轮不判"关联错"，但这条**不该继续以 `[S]` 身份充当定量门限的依据**。

## 不敢判的与缺什么才能判

| 条 | 悬着的判断 | 缺的东西 |
|---|---|---|
| 202 | Holland & Welsch 1977 是否真给"δ 取值表"并含 δ=1.345 | 全文（T&F 403、Semantic Scholar 无摘要、Crossref 无 abstract 域）。替代：可读到 §3/表 1 的副本，或 Huber 1981／Huber & Ronchetti 2009（仓内并列引用，ISBN 978-0-470-12990-6）中 δ=1.345（95% 效率）的出处页——若出处在 Huber 侧，H&W 的角色应缩为"IRLS 实现"半句 |
| 208 / 220 | 0.0015–0.002 mag/曝光（12–13 等、1 min）；CTE 损失 ~3%→~40% | 两篇 PASP 正文或摘要：IOP 走 Radware bot 墙、ADS 需 token、Crossref `select=abstract` 回 400。书目本身全对，仅数值无锚 ⇒ 属"证据层级不匹配"（形态⑮），不是错号 |
| 232 | Bakos+2004 是否为 FWHM≲2 px 失效门限提供任何论证 | PASP 116, 266 正文；或改由仓内已核的 ePSF/欠采样文献（[B13]/[B14]）承担该门限 |
| 262 | 副题 "DN → λ" 是否属书名页正式题名；PTC/DSNU 的具体章号 | SPIE 版权页或馆藏记录（现：Incapsula 拦、OpenLibrary/Google Books 网络不通）⇒ 现判"未钉版次" |
| 268 | 该书是否真重述了 `w=1/σ²` 的最优性（Gauss–Markov/BLUE） | 相应章节正文；PSO 迁移后跳转坏（301→`academic.oup.com/[Journals:]`）、出版社页 403 |
| 190 | 第三条逐字引文实测在正文 **§1 Introduction** | 无需再查：仓内未指节号 ⇒ 无节号可错（登记以免下轮误判成缺陷） |
| 244 | [B6][B7] 是否真为 arXiv:2309.11533 / 2309.11225 | 不属本批清单，未取证 ⇒ 留给对应批次 |

## 本批三态计数

- **存在性**：已核 14 ／ 不存在 0 ／ UNPROVEN 0。196 属"号存在但挂错件"，缺陷性质是错绑而非虚号 ⇒ 本批未发现不存在的标识符。
- **版次/载体**：版本对 12（190/202/208/214/220/226/232/238/244/250/256/268）／版本错 1（196，应指 `10.1051/0004-6361:20021571`，A&A 398, 785–800, 2003）／未钉版次 1（262，副题无法从出版社记录核实）。
- **关联**：关联对 5（190/196/226/238/256）／关联错 0／角色错绑 0（232 存疑未判）／无挂靠断言 3（214/244/250，其中 244 是"该挂未挂"）／关联性 UNPROVEN 6（202/208/220/232/262/268）。

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

1. **202** `10.1080/03610927708827533`（内容面：δ 表／δ=1.345）— 走过 `api.crossref.org/works/<doi>`（书目成功）、`api.semanticscholar.org/graph/v1/paper/DOI:`（无摘要）、`tandfonline.com/doi/abs/`（403）。换路：图书馆电子刊全文，或从 Huber & Ronchetti 2009 反查常数归属。
2. **208** `10.1086/132232`（数值 0.0015–0.002 mag）— Crossref 成功；`iopscience.iop.org/article/10.1086/132232` 302→bot 验证页；Crossref `select=abstract` 400。换路：带 token 的 ADS 摘要。
3. **220** `10.1086/316475`（CTE 3%→40%）— 同上（书目 OK、全文墙）。
4. **232** `10.1086/382735`（门限是否出自本文）— 仅 Crossref 书目核对（题名/作者/卷页年全对）。
5. **262** `10.1117/3.725073`（副题、章号）— Crossref、doi.org 302 目标、SPIE 书页（Incapsula）、`openlibrary.org/isbn/9780819478382.json`（fetch failed）、`googleapis.com/books/v1/volumes?q=isbn:…`（fetch failed）。
6. **268** `10.23943/princeton/9780691151687.001.0001`（"教科书重述"是否成立）— Crossref（monograph 全字段 OK）、PSO `/view/`（301 坏跳转）、`press.princeton.edu`（403）、Google Books／OpenLibrary（网络不通）。

## 新发现的缺陷形态（本批独有）

- **⑯ 该挂未挂（用了概念、给了二手出处）**：与⑭"列而不用"必须分开登记。⑭ 只是清单冗余；⑯ 是正文**使用了方法名而把出处给了下游应用文**（244），一手方法文躺在清单里 ⇒ 断言失去一手锚，后果重于冗余。
- **⑰ 书目全对、只有 DOI 末位漂移**（196）：该条自标"核验状态：文章级"，说明核对做到了卷页层却没做到 DOI 层——这类件在"卷页已核"的自述下极易放过。⇒ 元数据核对要**双向**：卷页→号 与 号→卷页 各查一次。
- **⑱ 否定式登记的"卷页占用"证据可靠**（238 正例）：以"该卷页实为另一篇"证伪候选引用时，Crossref 一次即可确认（与前批医学期刊书评反例同族）⇒ 核验路遇否定式断言先读极性，再核占用是否真成立。
- **⑲ 出版社元数据题名域可能短于书名页题名**（262："Photon Transfer" vs 仓内 "Photon Transfer: DN → λ"）⇒ 逐字题名类断言不能只靠 Crossref `title` 判红/判对，副题要版权页级载体。
- **途径层面的环境事实**：`api.crossref.org/works/<doi>?select=…` 与 `?fields=…` 一律回 **400**（只能取整记录）；IOP／SPIE／T&F／Princeton 四家出版社站对本环境**全数拦截或坏跳转** ⇒ 1977–1999 年的 PASP/T&F 数值级断言在当前工具链下**结构性不可核**，要么给 ADS token，要么把这些数值降级为"项目自证"。

## 覆盖率自报

14/14 条给出三字段：存在性全部落定（0 条 UNPROVEN）；关联性 5 判对、3 判"无挂靠断言"、6 如实 UNPROVEN；版次/载体 12 对、1 错、1 未钉。取证合计约 30 次网络查询，其中站点拒绝/网络失败 8 次（Cloudflare 403、Incapsula、Radware 302 墙、`press.princeton.edu` 403、OpenLibrary/Google Books `fetch failed`×3、网关 403×1，另 Crossref `select/fields` 参数 400×3 属工具约束不计拒绝）；单条最多 6 次（196、262）。
