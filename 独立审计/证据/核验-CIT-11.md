# 引用文献核验 · 批次 CIT-11（14 条，P-1）

- 基线：`c8f64e9a`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` §1 三字段 / §2 硬禁令 / §4 高危形态；派单补充：三字段＝存在性／版次与载体（"未钉版次"如实登记为缺陷形态）／关联（必须打开断言原文）；同行并引按"件"判不按"行"判。
- 网络手段清单（本批实际可用）：
  - `WebFetch` → `https://export.arxiv.org/api/query?id_list=<id>`（arXiv 官方 API，回包给版本号）
  - `WebFetch` → `https://api.crossref.org/works/<doi>`（DOI 元数据；同轮并发 ≤2，429 即退避）
  - `WebFetch` → 出版社自有页（A&A aanda 全文、IOP、OUP、SPIE、T&F）
  - `WebSearch` → 馆藏记录 / 出版社页兜底
  - ADS 回 405 ⇒ 仅 bibcode 者句柄不可核，标 UNPROVEN
  - 正文走 HTML：`arxiv.org/html/<id>vN`、`ar5iv.labs.arxiv.org/html/<id>`（PDF 常乱码）
  - 仓库侧只读：`Read` / `git grep`
- 硬上限：单条网络取证 ≤6 次；到限写 `UNPROVEN` 并附实际走过的途径与各自结果。
- 本批不做：不改仓库、不跑构建与门禁、不写共享台账 `整改/out/文献台账.csv`。
- 条目书写顺序＝**取证完成顺序**（189/195/201/207 → 219/231 → 213/225/237/243 → 249/255/261/267），非清单序号序；每条以 `## <序号>` 与文末结论行的序号列为准，回写台账按序号取。

---

## 189 · DOI 10.1051/0004-6361/202243680 + arXiv:2206.06143（De Angeli et al. 2023, A&A 674, A2）—— 核验态：已核，关联红

**存在性：已核。** 两个标识符 = **同一件**（同行并引按件判）。Crossref `https://api.crossref.org/works/10.1051/0004-6361/202243680` 回包：container-title *Astronomy & Astrophysics*、volume **674**、article-number **A2**、print 年 **2023**、online 2023-06-16、DOI 同、作者首 5 位 `F. De Angeli, M. Weiler, P. Montegriffo, D. W. Evans, M. Riello`（Crossref 的 title 字段只回短题 "Gaia Data Release 3"，A&A 的拆分式元数据，非缺陷）。arXiv 官方 API `https://export.arxiv.org/api/query?id_list=2206.06143` 回包 `<id> = 2206.06143**v1**`、title *"Gaia Data Release 3: Processing and validation of BP/RP low-resolution spectral data"*、published=updated=2022-06-13、`<arxiv:doi>` = 同一 DOI ⇒ 号与文一一对应，非"真号差末位"。

**位点与实际断言（原文已打开）**：`docs/science/PHOTOMETRY.md:311` §16「本节只补出处与参考实现」第 **7** 条：

> **7. XP 外定标（仪器响应模型与定标精度）**：Montegriffo, P., De Angeli, F., Andrae, R., Riello, M., et al. 2023, A&A 674, A3（DOI …202243880；arXiv:2206.06205）；**De Angeli, F., et al. 2023, A&A 674, A2（DOI 10.1051/0004-6361/202243680；arXiv:2206.06143）**；Carrasco, J. M., et al. 2021, A&A 652, A86（…，XP 内定标）。

**关联：角色错绑（按件判，只落 De Angeli 这一件）。** 打开被引件自身文本（arXiv abs 页摘要逐句）：该文做的是"**calibrate about 65 billion individual transit spectra onto the same mean BP/RP instrument**"、"variations of the line-spread function and dispersion across the focal plane and in time"、"combined for each source in terms of an expansion into continuous basis functions"，其结论句是 "Scientific validation suggests that the **internal calibration** was generally successful"。⇒ 它**确实**承担括号里的"仪器响应模型"（LSF/色散/连续基函数）与"定标精度"（验证与亮端超出标称误差的系统项），但**不承担小标题的"外定标"**——摘要通篇只述内部处理与验证，无外部绝对通量刻度。同句并列的 Montegriffo A3（本批外，另件）才是外定标的候选，仓内另一处也如此定性：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:188` [B52] 把 A3 题作 "**External calibration** of BP/RP low-resolution spectroscopic data"、`:141` [B38] 把本件题作 "**Processing and validation** of BP/RP low-resolution spectral data"，与本次核到的题名一致 ⇒ 本仓**已知**两件的分工，却在 `PHOTOMETRY.md:311` 把本件挂进"外定标"标题下。同一件的第二位点 `实验/absolute-snr/docs/surveys/f-instr-survey.md:305` [F-21] 与 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:51`（"XP 处理与验证链"）角色正确，反证问题只在 §16 第 7 条的小标题措辞。**整改口径**：把小标题改为"XP 处理与定标精度（内部）＋外定标另件"，或把本件移出"外定标"伞下；本仓两处题名订正表（`f-instr-survey.md:32`）本身与本次读数一致。

**版次/载体：版本对。** DOI → A&A 674 A2（2023 出版）；arXiv 侧只有 **v1**（published=updated）⇒ 无"只有 v2 才有被引内容"风险。载体登记：仓内三处（`PHOTOMETRY.md:311`、`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:141`、`f-instr-survey.md:305`）均给"A&A 674, A2 + DOI"，卷/文章号/年一致；`f-instr-survey.md:312` 明言副标题不得写成 "Processing of BP/RP spectra"，与官方题名一致。**残留（不算本条凭据）**：本次对"外定标"这一子句的判定依据是 arXiv v1 摘要，未开 A&A 出版版正文小节；若整改要钉到"该文无外定标节"，需补开 aanda 全文目录。

| 189 | DOI 10.1051/0004-6361/202243680；arXiv:2206.06143v1 | 已核 | 版本对（A&A 674 A2, 2023；arXiv 仅 v1） | 角色错绑（承担"仪器响应模型/定标精度"，不承担小标题"外定标"） | Crossref 记录＋arXiv 官方 API＋arXiv abs 摘要逐句＋仓内 [B38]/[B52] 题名对照 | — |

## 195 · DOI 10.1051/0004-6361:20021326 + arXiv:astro-ph/0207407（Greisen & Calabretta 2002, Paper I）—— 核验态：已核，关联对，载体未钉版次

**存在性：已核。** 两个标识符 = **同一件**。Crossref `…/works/10.1051/0004-6361:20021326` 回包：title *"Representations of world coordinates in FITS"*、*Astronomy & Astrophysics*、**395**、**1061–1075**、print **2002**、ISSN 0004-6361/1432-0746、作者 `E. W. Greisen, M. R. Calabretta`。arXiv 官方 API 回包 `<id> = astro-ph/0207407**v2**`、同题、`published 2002-07-18` / `updated 2002-11-13`、journal_ref "Astron.Astrophys. 395 (2002) 1061-1076"、作者序 Eric W. Greisen, Mark R. Calabretta。
**高危形态①专项已做且判清**：仓内把 **Paper I** 绑到 `…407` —— **绑对了**；顺手核的 `astro-ph/0207406` 是 Lazendic et al. "Molecular Diagnostics of Supernova Remnant Shocks"（与本主题无关），说明末位号并无"Paper II 占 …407"的错绑；`ASTROMETRY.md:266` 的 Paper II（Calabretta & Greisen, A&A 395, 1077）未配 arXiv 号，故不构成配对冲突。附登记：arXiv journal_ref 末 1076 与 Crossref 末 1075 的**页尾差**属两方元数据自身差异，仓内只写起始页 1061，不受影响。

**位点与实际断言（原文已打开）**：`docs/science/ASTROMETRY.md:265` §14a 首条 —— "WCS 框架与 1-based CRPIX/CRVAL：…（Paper I；DOI …，arXiv:astro-ph/0207407 逐字核验）**§2.1.1 式(1)** q_i=Σ_j m_ij(p_j−r_j)（r_j=CRPIX_j）与 **§2.1.4**（整数像素号=像素中心，首像素 0.5→1.5）"，并据此推 §5a 桥接口径。同件另两位点：`ASTROMETRY.md:257`（文章级定位，明说"TAN/SIP 公式号未逐式核验"）与 `docs/science/PHASE3_HIPS_TO_FITS.md:179`（同 DOI 的 §2.1.1 语义复述）。

**关联：关联对（式号/节号逐字命中，非题名级推断）。** 打开 A&A **出版版**全文（`https://www.aanda.org/articles/aa/full/2002/45/aah3859/aah3859.right.html`，即仓内 :257 自己登记的全文 URL）：§2.1.1 式(1) 原文 `q_i = \sum_{j=1}^{N} m_{ij} (p_j - r_j)`，且 "r_j are the pixel coordinate elements of the reference point given by the CRPIX_j"；§2.1.4 原文 "integer pixel numbers refer to the center of the pixel in each axis" 与 "so that, for example, the first pixel runs from pixel number 0.5 to pixel number 1.5 on every axis"。⇒ 仓内所写式(1) 的形式、r_j=CRPIX_j 的识别、§2.1.4 的两条结论**逐项对上**，由 §5a 桥接推出的口径（`det_x = i+0.5`、`x_c = CRPIX − 0.5`）落在被引文本的语义内。

**版次/载体：未钉版次（缺陷形态如实登记）。** 依据链是混着的：仓内句面写"arXiv:astro-ph/0207407 **逐字核验**"却**不给版本号**，而该号实测有 **v1（2002-07-18）与 v2（2002-11-13）两版**；本次逐字比对是在 **A&A 出版版**（= DOI 目标）做的，**未**与 arXiv v1/v2 任一单独版本比对，故"逐字核验"这一声称的载体无法复核 ⇒ 整改口径：要么把"逐字核验"改挂 DOI/出版版，要么钉到 `astro-ph/0207407v2` 并补一次 v1↔v2 差异检查。就 DOI 所指出版版而言内容正确（可判"版本对"的那一半已在上）。

| 195 | DOI 10.1051/0004-6361:20021326；arXiv:astro-ph/0207407（v1/v2 两版，仓内未钉） | 已核 | **未钉版次**（逐字核验声称挂在无版本号的 arXiv 句柄上；出版版一侧内容对） | 关联对（§2.1.1 式(1) 与 §2.1.4 逐字命中 A&A 出版版） | Crossref＋arXiv 官方 API（含 0207406 反证）＋aanda 出版版全文 §2.1.1/§2.1.4 | — |

## 201 · DOI 10.1080/03610927708827533（Holland & Welsch 1977, IRLS）—— 核验态：已核，关联半红

**存在性：已核。** Crossref `…/works/10.1080/03610927708827533` 回包：title *"Robust regression using iteratively reweighted least-squares"*、container-title *"Communications in Statistics - Theory and Methods"*、volume **6**、issue **9**、page **813–827**、print **1977**、publisher Informa UK Limited（Taylor & Francis）、作者 `Paul W. Holland, Roy E. Welsch`。仓内 `docs/references/SCIENTIFIC_REFERENCES.md:107` 题 "Robust Regression Using Iteratively Reweighted Least-Squares"、`docs/algorithms/PLATESOLVE.md:196` 作 *Comm. Theor. Meth.* 6, 813 ⇒ 题名/卷/首完全一致；"A6" 是该刊当年的 Part A 记法，与 Crossref 现名同号同页，**不构成卷页缺陷**。

**位点与实际断言（原文已打开）**：`docs/science/PHASE2_UPM.md:137`（§7 独立不变量 · Huber 对称性条）——在"**δ=1.345 的量纲与出处**：δ 无量纲（单位 = sigma_eff）；δ=1.345 是**高斯**参考分布下渐近效率 95% 的 Huber 阈值，出处 Huber, P. J. 1964 …（稳健位置估计的原始框架）+ Holland, P. W. & Welsch, R. E. 1977, *Comm. Statist.* A6, 813, DOI 10.1080/03610927708827533**（IRLS 实现与 δ 取值表）**+ Huber & Ronchetti 2009 …（§4 效率表）"。同件另两位点：`PHASE2_UPM.md:316`（"δ=1.345 的 IRLS 出处"）、`docs/algorithms/PHASE2_SESSION.md:135`、`实验/additive-sky-seamless/results/evidence_lit.json:111-117`（自报 Crossref 6(9) 813-827 已核、T&F 页 403）。

**关联：一半对、一半 UNPROVEN（按件判，红只落本件）。** (a) "**IRLS 实现**"这半**对**：被引件题名即 "…using iteratively reweighted least-squares"，角色与题名同源，属方法学一手（本批两条第三方途径的转引也都把 H&W 1977 当 IRLS 出处，见下表途径记录）。(b) "**δ 取值表**"这半**未证**：断言要的是"该文给出 95% 高斯效率对应的 δ=1.345 数值表"，这需要读到文内表格；本条取证 5 次用尽——Crossref（题录命中，回包**无** abstract 字段）、WebSearch×3（命中全是把 H&W 1977 当 IRLS 出处的第三方页与统计软件文档，无一条给出 H&W 文内表或其常数表）、MATLAB `robustfit` 文档镜像 `web.mit.edu`/`ece.northwestern.edu` 转存（fetch failed）；**T&F 出版社页本批未由我直接打开**（仓内 `实验/additive-sky-seamless/results/evidence_lit.json:117` 自报该页被 Cloudflare 403 拦截，属仓内自述，不作本批凭据）⇒ **δ=1.345 是否载于 H&W 1977 的表、还是仅载于 Huber 1964 / Huber & Ronchetti 2009 §4 效率表，本批无法判定**。这条子句是高危形态②（伪托表/节号）的候选：**不得**因"存在性已核"就把它当已核。
**若整改**：把 δ=1.345 的数值出处单挂可开表核对的载体（Huber & Ronchetti 2009 §4 已在本行并列），H&W 1977 只保留"IRLS 实现"角色；或补开一次正文表。

**版次/载体：版本对（限定在已核范围内）。** Crossref 唯一记录 = 1977 年 6(9) 813–827，仓内卷页年三项一致，不存在"同名再版内容不同"的对照对象；本批未查预印本/其他载体（预算用于 (b) 项），故"仅此一版"只到 Crossref 粒度。被引角色（IRLS 方法）与版次无关，(b) 项的载体缺口已单独登记，不重复计。

| 201 | DOI 10.1080/03610927708827533 | 已核 | 版本对（Comm. Statist. Theory Methods 6(9), 813–827, 1977；卷页年与仓内一致） | 关联半对："IRLS 实现"对；"**δ 取值表**"子句 **UNPROVEN**（高危形态②候选） | Crossref 题录（无 abstract 字段）；WebSearch×3 仅得第三方转引；MATLAB 文档镜像 fetch failed | δ=1.345 载于该文何处（表号/页）：需正文表；可走 JSTOR/期刊扫描本、或改挂 Huber & Ronchetti 2009 §4 |

## 207 · DOI 10.1086/131977（Stetson 1987, PASP 99, 191, DAOPHOT）—— 核验态：已核，关联对

**存在性：已核。** Crossref `…/works/10.1086/131977`：title *"DAOPHOT - A computer program for crowded-field stellar photometry"*、*Publications of the Astronomical Society of the Pacific*（PASP）、volume **99**、page **191**、print **1987**、publisher IOP Publishing、作者 `Peter B. Stetson`。IOP 文章页实页读数一致：*"Publications of the Astronomical Society of the Pacific, Volume 99, Number 613, 1987, p. 191"*，DOI 同 ⇒ 号、卷、首叶、年四值齐对，非"真号差末位"。

**位点与实际断言（原文已打开）**：`docs/references/SCIENTIFIC_REFERENCES.md:170` §N.2 学术文献第 **67** 条：

> 67. Stetson, P. B. 1987, PASP 99, 191（DOI 10.1086/131977，见 §I 第 22 条）。用途：**拥挤场 PSF 测光**。

指回项已核：`SCIENTIFIC_REFERENCES.md:70` §I 第 22 条同 DOI，用途写得更宽（"拥挤场 PSF 拟合测光、迭代星表构建、质量代理语义的历史来源"）⇒ **§N.2→§I 的内部指针有效**（条号 22 对得上）。另 `docs/science/PSF.md:190`、`docs/science/STAR_DETECTION.md:165` 两处亦绑同一 DOI 于"拥挤场 PSF 拟合测光 / 质心估计"。

**关联：关联对。** 取到 IOP 页摘要实文（转述级）：软件按序做 raw CCD 预处理 → FIND 建初表 → PHOT 做合成孔径测量 → **密集场需要按帧构建经验点扩散函数** → GROUP 分组 → **NSTAR 以最小二乘 profile 拟合出测光** ⇒ "拥挤场 PSF 测光"这一角色由被引件的**方法环节**（GROUP+NSTAR 的 profile 拟合）直接支撑，不是题名级猜认。
**边界登记**：本仓把同一 DOI 还用于"质心估计（一阶矩/导数零交叉）"（`STAR_DETECTION.md:165`）与"迭代星表构建"（`SCIENTIFIC_REFERENCES.md:70`）——FIND/GROUP 环节支撑迭代星表；**"一阶矩/导数零交叉质心"**这一具体口径本次未在摘要级读数中出现，属另一句断言的另一件事，本批位点（:170）不含它，故不计红，仅登记供后续按句核验。

**版次/载体：版本对。** 1987 年 PASP 99, 191 为唯一出版载体（Crossref 无 arXiv/预印本字段，PASP 该文亦无 arXiv 号），仓内只写起始页 191 与 Crossref/IOP 一致；`实验/absolute-snr/docs/EXP-02-STRUCTURE-CONTAMINATION.md:354` 等处的"191–**222**"末叶本批**未核**（Crossref `page` 只给单页 191），不据此判红。

| 207 | DOI 10.1086/131977 | 已核 | 版本对（PASP 99, 191, 1987；单载体） | 关联对（摘要级 GROUP/NSTAR 拟合环节坐实"拥挤场 PSF 测光"） | Crossref 题录＋IOP 文章页实页元数据与摘要 | —（末叶 222 若要用，需 ADS/IOP 全页范围） |

## 219 · DOI 10.1086/316124（Starck & Murtagh 1998, PASP 110, 193, MRS）—— 核验态：已核，关联半红＋行号漂移

**存在性：已核。** Crossref `…/works/10.1086/316124`：title *"Automatic Noise Estimation from the Multiresolution Support"*、PASP、volume **110**、issue **744**、page **193–199**、print **1998**、IOP Publishing、作者 `J.-L. Starck, F. Murtagh` ⇒ 题名、作者、卷、首叶、年五项与仓内写法逐字一致。

**位点与实际断言（原文已打开；清单行号已漂移）**：批次清单给 `docs/science/NOISE_MODEL.md:370`，基线 `c8f64e9a` 上 `git grep` 实际命中 **:378**（漂 8 行；同件另在 `SCIENTIFIC_REFERENCES.md:124` 与 `:167` 两条列表里各出现一次）：

> **多尺度稳健噪声（MRS/N\*）**：Starck, J.-L. & Murtagh, F. 1998, "Automatic Noise Estimation from the Multiresolution Support", PASP 110, 193（**DOI 10.1086/316124，starlet 小波；PixInsight ImageWeighting §2.4 的 MRS 出处**）；…… **核验状态**：文章级；ACSD 现状**未采用**小波 MRS/N\*，该条只作选型对照。

**关联：一半对、一半未证（按件判，只落本件）。**
(a) **对**——"MRS 一手论文 + PixInsight §2.4 出处"：打开 PixInsight 官方 *New Image Weighting Algorithms*（`https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html`）实页，其 **§2.4** 起句逐字为 *"The multiresolution support noise evaluation algorithm (MRS)[4] is currently the standard noise estimator in PixInsight."*，其文末参考 [4] = *Jean-Luc Starck and Fionn Murtagh (1998). Automatic Noise Estimation from the Multiresolution Support. PASP vol. 110, pp. 193–199* ⇒ 仓内"**§2.4 的 MRS 出处**"这一绑定**双向对上**（节号真、指向真），且页范围 193–199 与 Crossref 一致。
(b) **未证**——"**starlet 小波**"这一修饰：PixInsight 同页把 starlet 归给**另一件**——逐字 *"This algorithm uses the starlet transform[3] (aka à trous wavelet transform)…"*，其中 [3] = Starck, Murtagh & Fadili 2010（Cambridge Univ. Press, DOI 10.1017/CBO9780511730344），**不是** 1998 PASP。⇒ 1998 篇内是否用 à-trous/starlet 实现 MRS，本批未能读到正文（IOP 文章页对该 DOI 走 Radware 验证墙，302 到 `validate.perfdrive.com`；ADS `1998PASP..110..193S/abstract` 回 **405**，按派单纪律"仅 bibcode 者句柄不可核 ⇒ UNPROVEN"）。故本件承担 MRS 角色已证，承担"starlet 小波"这一**方法学标签**未证，属高危形态②的近邻（把后出术语回溯给早期一手文）。
(c) 仓内自带的诚实边界（"文章级；未采用"）与本次结论一致，不额外计红。

**版次/载体：版本对。** 1998 PASP 110(744) 193–199 单载体（无 arXiv 预印本，无同名再版），仓内卷页年与 Crossref 一致；被引角色（MRS 定义）不涉版次漂移。

| 219 | DOI 10.1086/316124 | 已核 | 版本对（PASP 110(744), 193–199, 1998） | 关联半对：MRS/§2.4 绑定**对**（PixInsight 实页 [4] 逐字）；"**starlet 小波**"修饰 **UNPROVEN**（该文正文未取得，PixInsight 把 starlet 归 [3] 2010 书） | Crossref 题录；PixInsight ImageWeighting 实页（§2.4＋参考表 [3][4][5]）；IOP 页 302→Radware 验证墙；ADS /abstract 405；WebSearch×1（无一手命中） | 1998 篇正文 §（MRS 用的具体变换）：需 PDF 全文镜像或 ADS 全文 token；否则把"starlet 小波"改挂 Starck-Murtagh-Fadili 2010 |

## 231 · DOI 10.1086/345491（Anderson & King 2003, PASP 115, 113）—— 核验态：已核，关联红（角色错绑）

**存在性：已核。** Crossref `…/works/10.1086/345491`：title *"An Improved Distortion Solution for the Hubble Space Telescope's WFPC2"*、PASP、volume **115**、issue **803**、page **113–131**、print **2003**、IOP Publishing、作者 `Jay Anderson, Ivan R. King` ⇒ 作者对、**卷 115 首叶 113 对**（仓内 `[F-12]` 写 "2003, PASP 115, 113"）、年对。

**位点与实际断言（原文已打开，行号未漂）**：`实验/absolute-snr/docs/surveys/f-instr-survey.md:35`（"本轮核对发现的引用订正清单"表行）——

> | Anderson & King 2006, PASP 118, 560 | **未核到**（PASP 118,560 是 Branch et al. 的超新星光谱论文） | 用 **Anderson & King 2000, PASP 112, 1360, DOI 10.1086/316632** 与 **2003, PASP 115, 113, DOI 10.1086/345491** `[F-12]`；"ACS ISR 2006-01" 标 `[UNVERIFIED]` |

该行的下游正文 `f-instr-survey.md:169-178`（[F-12] 条目本体，两篇并列于同一标题）把被引内容写成：**通量定义**"以**有效 PSF（ePSF）**——在欠采样条件下由星像**逐像素重建**的经验 PSF——做拟合测光/天测"；**借鉴点**"**欠采样下的 PSF 建模**是 HST 路线最可借鉴的部分"；**不借鉴点**"需要大量星像与迭代对齐"。

**关联：角色错绑（按件判，红只落 2003 件）。** 本件题名所示主题是 **WFPC2 几何畸变改正**（Improved **Distortion** Solution），而 [F-12] 通篇的四栏（通量定义/孔径无关性/借鉴点/不借鉴点）**没有一栏涉及畸变改正**，全部是 ePSF-PSF 建模口径——那与并列的 2000 件题名（"…I. Deriving an Accurate Point-Spread Function"）同源。⇒ 2003 件被派了一个它正文里不做的工作：**并列引用把两篇当一个方法出处**，一旦整改方按 [F-12] 的 ePSF 声称回溯到 2003 件的页/节，就会写出伪托内容（高危形态②）。
**同时给出正面判定**：在 `:35` 这一行的**替代**用途上（用 2000+2003 替代流传的"A&K 2006 ACS ePSF/畸变 ISR"），2003 件恰是**畸变**那一半，替代本身合理——缺陷不在选件，而在 [F-12] 没写它承担什么。**整改口径**：给 2003 件单列一行、角色写"WFPC2 几何畸变解（与 ePSF 建模并列，不是同一结论的出处）"，或从 ePSF 条目里摘出；`[UNVERIFIED]` 的 "ACS ISR 2006-01" 保持不引用（与仓内自报一致）。

**版次/载体：版本对。** 2003 单载体（PASP 115(803), 113–131，Crossref 无预印本字段），仓内"115, 113"落在该载体内；"I./II./III."系列号风险已查——仓内**未**给 2003 件编系列号（只给卷页与 DOI），故无系列错绑；本批未开正文确认 2003 是否自标"II"（该信息不进结论，若要写系列号须补核）。

| 231 | DOI 10.1086/345491 | 已核 | 版本对（PASP 115(803), 113–131, 2003；未编系列号，无系列错绑） | **角色错绑**：被挂角色=欠采样 ePSF 建模（属并列的 2000 件），本件实主题为 WFPC2 畸变改正，仓内四栏无一栏写畸变 | Crossref 题录（题名含 "Distortion Solution"）＋位点 `f-instr-survey.md:35` 与 `:169-178` 原文逐栏比对 | — |

## 213 · DOI 10.1086/133316（Schechter, Mateo & Saha 1993, PASP 105, 1342, DoPHOT）—— 核验态：已核，关联对（但"列而不用"）

**存在性：已核。** Crossref `…/works/10.1086/133316`：title *"DOPHOT, a CCD photometry program: Description and tests"*、PASP、volume **105**、page **1342**、print **1993**、IOP Publishing、作者 `Paul L. Schechter, Mario Mateo, Abhijit Saha` ⇒ 作者、卷、首叶、年四项与仓内一致（仓内写 "DoPHOT"、注册库写 "DOPHOT"＝大小写归一，不计缺陷；issue 号 Crossref 未回，仓内亦未写）。

**位点与实际断言（原文已打开）**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:174`，分组标题「**提取与场景补充**」（§2 文献清单内），行面：

> - [B41] Schechter, P. L., Mateo, M. & Saha, A. 1993, PASP 105, 1342, "**DoPHOT…**" DOI 10.1086/133316 **[S]**

`git grep` 全仓（含 json/csv/py）检索 `Schechter|DoPHOT|DOPHOT|133316`：**只有这一行**（另一处命中是 `实验/photometric-magnitude/.../real_ridge.json` 里的数值巧合，与引用无关）⇒ 本件在本仓**没有任何第二句实质断言**。

**关联：关联对（分组角色层）＋登记"列而不用"缺陷形态。** 题名所示该文是一套 CCD 测光程序的描述与检验 ⇒ 与「提取」分组不矛盾，无 A 文干 B 文的错绑。但该行不挂任何论断（题名被省略号截断、无节号/公式/表号、无"我们据此做了什么"），故 §1.5「孔径测光 vs PSF 测光」等现行结论**并不真的消费这一件**；`[S]` 标签按前言与既有口径**不作凭据**（本批独立核到的是 Crossref 题录）。整改口径：补一句可回溯用途（对应哪条算法/门），或把它从"科学出处"降级为"延伸阅读"。

**版次/载体：未钉版次（缺陷形态如实登记）。** 行面只给"卷＋首叶＋DOI"，题名截断、无页范围（Crossref 亦只回 1342，未回末叶/issue）⇒ 载体唯一定位依赖 DOI 本身；被引**内容**（Description 与 tests 的哪一部分）无任何钉法。**注**：不存在"同名再版内容不同"型漂移（1993 PASP 单出版载体）。

| 213 | DOI 10.1086/133316 | 已核 | 未钉版次（题名省略、无页范围/issue；仅"卷＋首叶＋DOI"） | 关联对（分组角色不矛盾）＋**列而不用**（全仓仅此一行，无实质断言消费） | Crossref 题录＋`git grep` 全仓唯一命中＋位点 :174 分组标题原文 | 末叶/issue：Crossref 未回，需 IOP 页（本批 IOP 走不通，Radware 验证墙） |

## 225 · DOI 10.1086/323894（van Dokkum 2001, PASP 113, 1420, LA-Cosmic）—— 核验态：已核，关联对

**存在性：已核（双源一致）。** Crossref `…/works/10.1086/323894`：title *"Cosmic-Ray Rejection by Laplacian Edge Detection"*、PASP、volume **113**、issue **789**、page **1420–1427**、print **2001**、IOP Publishing、作者 `Pieter G. van Dokkum`。arXiv 官方 API `id_list=astro-ph/0108003` 回包 `<id> = astro-ph/0108003**v1**`、同题、`<arxiv:journal_ref>` = "Publ.Astron.Soc.Pac.113:1420-1427, 2001" ⇒ 两源卷页年题四项一致，预印本**只有 v1**。

**位点与实际断言（原文已打开，三处全读）**：
| 位点 | 挂的那句 |
|---|---|
| `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:218`（代表位点，分组「**分场景定量补充**」） | `- [B80] van Dokkum, P. G. 2001, PASP 113, 1420, DOI 10.1086/323894 [S]`（无角色描述） |
| `docs/algorithms/COSMETIC_ALGORITHMS.md:420` | "**宇宙线剔除（单帧）**：van Dokkum 2001, PASP **113, 1420**（LA Cosmic，DOI 10.1086/323894，**卷页已核**）" |
| `docs/research/CCD_LINEAR_DEFECT_LITERATURE.md:286` | "LA-Cosmic（van Dokkum 2001, PASP 113, 1420, arXiv:astro-ph/0108003）**§2–§3.2**：以 Laplacian 边缘检测＋细结构比 `L⁺/F` 区分宇宙线与欠采样点源——'The critically sampled star has L⁺/F = 0.7…'；默认 `flim = 2`，WFPC2 数据需 `flim ≈ 5`" |

**关联：关联对（角色层与摘要逐字对上）。** 被引件摘要逐字（astro-ph/0108003v1）："Conventional algorithms for rejecting cosmic-rays in **single CCD exposures** rely on the contrast between cosmic-rays and their surroundings, and may produce erroneous results if the PSF is smaller than the largest cosmic-rays. This paper describes a robust algorithm for cosmic-ray rejection, **based on a variation of Laplacian edge detection**. The algorithm identifies cosmic-rays of arbitrary shapes and sizes by the **sharpness of their edges**, and **reliably discriminates between poorly sampled point sources and cosmic-rays**." ⇒ `COSMETIC_ALGORITHMS.md:420` 的"宇宙线剔除（**单帧**）＋Laplacian＋区分欠采样点源"三项逐字坐实；该行自称"卷页已核"由本次双源复核**独立成立**（113, 1420–1427）。
**登记的边界**：`:218` 位点所在分组无任何断言文字，本件在该行只是被**列出**（真实用途在算法侧，已核），故不判红；`CCD_LINEAR_DEFECT_LITERATURE.md:286` 的**§2–§3.2 节号**与 `L⁺/F = 0.7/1.8/21`、`flim = 2`、`flim ≈ 5` 等具体数值**摘要不含**，本批未开正文 ⇒ 属"节号/数值级未证"，列入 UNPROVEN 清单（高危形态②候选，非结论凭据）。

**版次/载体：版本对。** 2001 出版版（PASP 113(789) 1420–1427）与 astro-ph/0108003 **v1** 同体（无 v2/v3、无同名再版），仓内引卷页＋DOI 指向出版版；`研究包`处另引 arXiv 号，两者一致 ⇒ 无版次漂移面。

| 225 | DOI 10.1086/323894（＋astro-ph/0108003v1） | 已核 | 版本对（PASP 113(789), 1420–1427, 2001；预印本仅 v1） | 关联对（"单帧宇宙线剔除/Laplacian/区分欠采样点源"由摘要逐字坐实） | Crossref 题录＋arXiv 官方 API（journal_ref 一致）＋摘要原文；三处位点原文 | §2–§3.2 节号与 L⁺/F、flim 数值：需正文（摘要级不含） |

## 237 · DOI 10.1086/444553（Sirianni et al. 2005, PASP 117, 1049, ACS 测光性能与定标）—— 核验态：已核，关联对

**存在性：已核（双源一致）。** Crossref `…/works/10.1086/444553`：title *"The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys"*、PASP、volume **117**、issue **836**、page **1049–1112**、print **2005**、IOP Publishing、作者首 5 位 `M. Sirianni, M. J. Jee, N. Benítez, J. P. Blakeslee, A. R. Martel`。arXiv 官方 API（作者＋题名检索）命中 `<id> = astro-ph/0507614**v1**`，`<arxiv:journal_ref>` = "Publ.Astron.Soc.Pac.117:1049-1112,2005"，作者首 4 位 `M. Sirianni, M. J. Jee, N. Benitez, J. P. Blakeslee` ⇒ 题、卷、页范围、年完全一致。
**高危形态①（伪号）专项**：仓内 `实验/absolute-snr/docs/surveys/f-instr-survey.md:31` 自报"曾写作 **10.1086/496934**（该 DOI 是 *Clinical Infectious Diseases* 书评）"并已订正为 444553；本批复核支持订正，基线行面（`PHOTOMETRY.md:313`、`:183` [B50]、`[F-24]`:332）均已是 **10.1086/444553** ⇒ 无残留伪号。

**位点与实际断言（原文已打开）**：`docs/science/PHOTOMETRY.md:313` §16 第 **9** 条——

> **9. 光子计数通带（`λ` 因子的文献依据）**：Bessell 1990 …（DOI 10.1086/132749）；Bessell & Murphy 2012 …（DOI 10.1086/664083，photonic passband 与零点）；Fukugita 1996 …；**Sirianni, M., et al. 2005, PASP 117, 1049（DOI 10.1086/444553，端到端系统透过率 × 光谱的工程范例）**。

并引四件按件判：本条只判 Sirianni 件。它在该行的角色被明写为"**工程范例**"，而 `λ` 因子的出处另挂在 Bessell & Murphy 2012 那件上 ⇒ **未**把本件当"λ 因子出处"用（不是角色错绑）。

**关联：关联对。** 被引件摘要逐字（astro-ph/0507614v1）："We present the photometric calibration of the HST Advanced Camera for Surveys (ACS). We give here an overview of the performance and calibration of the 2 CCD cameras, the Wide Field Channel (WFC) and the High Resolution Channel (HRC), and a description of the best techniques for reducing ACS CCD data. **On-orbit observations of spectrophotometric standard stars have been used to revise the pre-launch estimate of the instrument response curves to best match predicted and observed count rates. Synthetic photometry has been used to determine zeropoints for all filters in 3 magnitude systems** and to derive interstellar extinction values for the ACS photometric systems." ⇒ "**仪器响应曲线 × 光谱 → 预测计数/零点**"的工程范例定位与摘要两项一手动作（响应曲线定标＋合成测光求零点）逐字同构；`docs/research/PHOTOMETRY_RESEARCH_PACK.md:80`（P4）把它写作"端到端系统透过率（**QE × 滤光片 × 光学**）× 光谱 → 合成计数的工程范例与逐项预算写法"——**摘要级只到 "instrument response curves"**，未见"三分量相乘"的表述 ⇒ 该三分解子句降为正文级未证（见 UNPROVEN 清单），不影响位点 :313 的角色判定。

**版次/载体：版本对。** 2005 出版版（PASP 117(836) 1049–1112）与预印本 astro-ph/0507614 **v1**（无 v2）同体；仓内三处分别写"1049""1049–1112"，均落在该载体内 ⇒ 无版次漂移面。附：该文是 ACS 测光定标的**主文档**，仓内以 `[CR]`/`[S]` 标签登记不作假（本批已独立复核，标签不作凭据）。

| 237 | DOI 10.1086/444553（＋astro-ph/0507614v1） | 已核 | 版本对（PASP 117(836), 1049–1112, 2005；预印本仅 v1） | 关联对（"响应曲线×光谱→计数/零点"的工程范例角色由摘要逐字坐实）；研究包 :80 的"QE×滤光片×光学"三分解＝正文级 **UNPROVEN** | Crossref 题录＋arXiv 官方作者/题名检索（journal_ref 一致）＋摘要逐字；位点 :313 原文＋`:31` 订正表复核 | 三分量透过率相乘的具体小节/表：需 PASP 正文（IOP 走 Radware 验证墙） |

## 243 · DOI 10.1088/0004-637X/750/2/99（Tonry et al. 2012, ApJ 750, 99, PS1 测光系统）—— 核验态：已核，关联半对＋内容级未证

**存在性：已核。** Crossref `…/works/10.1088/0004-637X/750/2/99`：title *"THE Pan-STARRS1 PHOTOMETRIC SYSTEM"*、container-title *The Astrophysical Journal*、volume **750**、issue **2**、page **99**、print **2012**（issued 2012-04-18）、publisher American Astronomical Society、作者 `J. L. Tonry, C. W. Stubbs, K. R. Lykke, P. Doherty, I. S. Shivvers, W. S. Burgett, K. C. Chambers, K. W. Hodapp, N. Kaiser, R.-P. Kudritzki, E. A. Magnier, J. S. Morgan, P. A. Price, R. J. Wainscoat` ⇒ 仓内"Tonry et al. 2012, ApJ 750, 99"三项对。arXiv 官方 API 题名检索命中 `<id> = 1203.0297**v1**`，题同、作者首 4 位同（Tonry, Stubbs, Lykke, Doherty）⇒ 仓内 `PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:137` [B29] 把 arXiv:1203.0297 配该题名**也是对的**（跨批注记，本批不判那一条）。
**过程登记（不得凭记忆）**：本条一次误按印象试了 `id_list=1109.3163`，官方 API 回包是 Aolita 等的量子关联文 ⇒ 与该题无关，已弃用并改走题名检索；结论只建立在回包一致的读数上。

**位点与实际断言（原文已打开）**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:120`（第 7 簇表行 7.4，四栏全读）——

> | 7.4 | Tonry et al. 2012 (PS1 测光系统), ApJ 750, 99. DOI 10.1088/0004-637X/750/2/99 `[CR]` | 从**仪器响应函数**定义巡天测光系统，含把**滤光片/探测器/大气乘积**当作定义通带的东西 ⇒ 定义 `g_k` 归一化**到什么**时引用；也是『乘性响应是通带加权量，不是标量』的引用 | PS1 具体滤光片集，以及『响应由实测滤光片/探测器曲线完全表征』的假设——本项目 `g_k` 是经验的 | 本项目无实测响应曲线 |

同件另两位点：`docs/science/CONTROL_WEIGHT_SNR.md:244`（"**5σ 点源深度 m5**：Tonry, J. L. et al. 2012, ApJ 750, 99（Pan-STARRS 3π）"）与 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:137` [B29]（只列题名＋arXiv）。

**关联：一半对、一半内容级 UNPROVEN。**
(a) **对**：摘要逐字 "In this paper we present our **determination of the Pan-STARRS photometric system**: gp1, rp1, ip1, zp1, yp1, and wp1 … We **define the Pan-STARRS magnitude system**, and describe in detail our measurement of …" ⇒ "定义巡天测光系统／通带与零点归一化参照"这一角色由被引件自己的句子支撑，位点 :120 的"定义 `g_k` 归一化到什么时引用"用途合法。
(b) **未证**（两处子句）：① :120 的"**从仪器响应函数定义**…把**滤光片/探测器/大气乘积**当作定义通带"——摘要给的相反方向的表述是 "The Pan-STARRS photometric system is **fundamentally based on the HST Calspec spectrophotometric observations**, which in turn are fundamentally based on **models of white dwarf atmospheres**"（即以**标准星观测**定系统），且整段摘要**不含** "instrument response function"、也**不含** "filter × detector × atmosphere" 字样（逐字检查后判 absent）⇒ 该表述可能来自正文，但摘要级**不支持**，不得算已核。② :244 的"**5σ 点源深度 m5**"——摘要中 **无** "m5"、**无** "five-sigma/5 sigma" 字样（逐字检查后判 absent）⇒ 该绑定同样是正文级未证（若 m5 定义在正文某节，须补开 ApJ 正文取证）。
(c) 无"角色错绑"到别的文：本件确实是 PS1 测光系统定义文，问题只在**被挂的具体口径未取得**。

**版次/载体：版本对。** ApJ 750(2) 99（2012-04-18）＋预印本 1203.0297 **仅 v1** ⇒ 无"只有后续版才有"风险；仓内只引 DOI/卷页（不混引 arXiv 号于同一行），载体单一。**须钉的一处**：(b) 两项若整改为"已核"，必须同时写**版次＋小节**（ApJ 出版版的响应函数节，或 arXiv v1 的对应节），否则又会留下式号/节号无载体的老问题（本批新形态，见文末）。

| 243 | DOI 10.1088/0004-637X/750/2/99（arXiv:1203.0297v1） | 已核 | 版本对（ApJ 750(2), 99, 2012；预印本仅 v1；载体单一＝DOI） | 关联半对："定义 PS1 测光系统/星等系统"摘要逐字对；"**仪器响应函数＝滤光片×探测器×大气**"与"**5σ 深度 m5**"两子句 **UNPROVEN**（摘要逐字不含，正文未取得） | Crossref 题录（含 AAS 作者全列）＋arXiv 官方题名检索与摘要逐字（含 response/m5 存在性检查）＋位点 :120 四栏、:244、:137 原文；一次误号 `1109.3163` 经官方 API 判为无关文并弃用 | 需 ApJ 正文 §（PS1 response functions）与 m5 定义式：IOP 文章页走 Radware 验证墙；可走 ADS 全文 token 或 arXiv v1 PDF/HTML |

## 249 · DOI 10.1093/mnras/214.4.575（Irwin 1985, MNRAS 214, 575）—— 核验态：已核，关联对（题名级）＋正文不可得

**存在性：已核。** Crossref `…/works/10.1093/mnras/214.4.575`：title *"Automatic analysis of crowded fields"*、*Monthly Notices of the Royal Astronomical Society*、volume **214**、issue **4**、page **575–604**、print **1985**（issued 亦 1985-6）、Oxford University Press、作者 `M. J. Irwin` ⇒ 仓内 `1985, MNRAS 214, 575` 四项全对，且 **DOI 串里的 `214.4.575` 与注册库卷/期/首叶一致**。
**高危形态①专项**：仓内 `实验/absolute-snr/docs/surveys/frame-snr-survey.md:24` 自报"曾写作 `10.1093/mnras/214.**3**.575` 是错的（Crossref 无此记录）"——该行是**订正表**；本批核对支持其订正方向（正确 DOI = `214.4.575`，页范围 575–604 亦与其 :379 自述一致），即基线上**没有**留下末位错值。

**位点与实际断言（原文已打开）**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:146`，分组标题为「**孔径 vs PSF / 提取方法**」：

> - [B11] Irwin, M. J. 1985, MNRAS 214, 575, "Automatic analysis of crowded fields" DOI 10.1093/mnras/214.4.575 [S]

仓内其它消费点（一并打开）：`docs/research/PHOTOMETRY_RESEARCH_PACK.md:140`（O9 行："**星表引导/拥挤场 PSF 测光**的方法学源头"）、`:152`（"② **PSF 拟合不确定度**（含模型失配、采样不足、拥挤）"）、`docs/science/PHOTOMETRY.md:382`（表行"PSF 拟合不确定度 | Stetson 1987；Irwin 1985；…"）。

**关联：关联对（限"拥挤场自动分析⇒PSF 测光方法族历史出处"这一层）；更细子句降为正文级未证。** 题名 *Automatic analysis of crowded fields* 直接支撑"拥挤场自动测光分析"的方法族归属 ⇒ 位点 :146 的分组角色不矛盾、不属角色错绑。**须登记的边界**：(i) "孔径 vs PSF" 的**孔径**一侧；(ii) `PHOTOMETRY.md:382`／研究包 :152 的 **"PSF 拟合不确定度"** 具体口径——两项本批**未能**读正文证实：MNRAS 1985 全文在 OUP 墙后，且仓内三处已各自如实登记同一事实（`实验/absolute-snr/docs/frame-snr-canon.md:555`"Irwin 1985 正文未取得——只引 IRAF 实现级公式与 Crossref 书目"、`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:328`、`实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md:842`）⇒ 属"存在性已核、内容级未证"，不是伪托。

**版次/载体：版本对。** 1985 年 OUP 出版版单载体（Crossref 无预印本字段；1985 年无 arXiv），DOI 本身即**文章级**句柄（卷.期.首叶编码），非"概念 DOI 漂到最新版"形态；仓内引"214, 575"落在该载体内。

| 249 | DOI 10.1093/mnras/214.4.575 | 已核 | 版本对（MNRAS 214(4), 575–604, 1985；文章级 DOI） | 关联对（题名级：拥挤场自动分析⇒方法族出处）；"孔径"/"PSF 拟合不确定度"子句＝正文级 **UNPROVEN** | Crossref 题录（卷期页年作者）＋位点 :146 与 `PHOTOMETRY.md:382`、研究包 :140/:152 原文；MNRAS 正文未取得（OUP 墙） | 文内是否给出孔径/拟合误差具体式：需 ADS 全文 token 或纸质扫描 |

## 255 · DOI 10.1093/pasj/psx126（Huang et al., PASJ 70, HSC 管线测光性能）—— 核验态：已核，关联半对＋年份/文章号缺陷

**存在性：已核。** Crossref `…/works/10.1093/pasj/psx126`：title *"Characterization and photometric performance of the Hyper Suprime-Cam Software Pipeline"*、*Publications of the Astronomical Society of Japan*、volume **70**、issue **SP1**、article-number **S6**、**published-print 2018**（issued/online **2017-12-22**）、OUP、作者 `S. Huang, A. Leauthaud, R. Murata, J. Bosch, P. Price, R. Lupton, …`。arXiv 官方 API 以题名检索命中 `<id> = 1705.01599**v1**`（published=updated=2017-05-03，无 journal_ref 字段），作者首四位 `Song Huang, Alexie Leauthaud, Ryoma Murata, James Bosch` ⇒ 号、题、作者三项一致。并列的另一件 arXiv:1705.06766v1 = Bosch et al. "The Hyper Suprime-Cam Software Pipeline"（仓内 [B47] 以 **PASJ 70, S5** 引用）⇒ 本件应为 **S6**，两件不互撞。

**位点与实际断言（原文已打开）**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:235`，分组标题「**深度 / 巡天策略**」：

> - [B30] Huang, S., Leauthaud, A., Murata, R., et al. **2017**, PASJ **70**, DOI 10.1093/pasj/psx126 [V]

**关联：一半对、一半无对应内容。** (a) **"深度"这半对**：被引件摘要逐字（arXiv:1705.01599v1）"The Wide layer of the SSP is both wide and deep, **reaching a detection limit of i~26.0 mag**"、"At these depths, it is challenging to achieve accurate, unbiased, and consistent photometry across all five bands"、"For stars, we achieve **1% photometric precision at i~19.0 mag and 6% precision at i~25.0** in the i-band" ⇒ 深度与测光精度是该文一手产出，挂「深度」不属错绑。(b) **"巡天策略"这半无对应内容**：摘要通篇是 SynPipe 注星仿真、管线测光性能与 DR1 注意事项（含混合/blending、恒星错分类、CModel 半光半径低估三条 caveat），**不涉及巡天策略/曝光规划** ⇒ 分组标签多派了这件不做的活（轻度角色外溢）。
(c) **同作者-年撞车必须登记**：`docs/science/CONTROL_WEIGHT_SNR.md:244` 把"**HSC 深度**"绑到另一载体（"Huang, S. et al. 2017, **ApJ 838, 110**（HSC 深度）"），与本件同名作者＋同年 ⇒ 仓内"Huang 2017"两处指两件；本批用 Crossref `query.bibliographic` 检索该文未命中（0 相关），故**那一条句柄本批判不了**，只登记风险：`[B30]` 行不给文章号，读者无法区分两件，易把 ApJ 那件的结论安到 PASJ 这件上。

**版次/载体：版本错（应指 2018, PASJ 70(SP1), S6）＋未钉文章号。** Crossref print 年 = **2018**、issue = SP1、article-number = **S6**；仓内写 "2017, PASJ 70"——**2017 只是 online-first（2017-12-22）**，与 PASJ 卷 70 的出版年不配；PASJ 无页码体例，**只给卷号不给 S6 即无法唯一定位**。整改口径：改 `2018, PASJ 70(SP1), S6`（如坚持 2017 须明写"online 2017-12-22"）。附注：预印本侧只有 **v1**，被引的精度/深度数字在 v1 即有 ⇒ 无"只有 v2 才有"风险；缺陷只在**出版版年与文章号未钉**。

| 255 | DOI 10.1093/pasj/psx126（arXiv:1705.01599v1） | 已核 | **版本错**（应指 2018, PASJ 70(SP1), **S6**；仓内"2017/PASJ 70"＝年漂移＋未钉文章号） | 关联半对：「深度」对（摘要给 i~26.0 极限与 1%/6% 精度）；「巡天策略」无对应内容；与 `CONTROL_WEIGHT_SNR.md:244` 的 "Huang 2017 ApJ 838,110" 撞作者-年 | Crossref 题录（issue/article-number/print 年/issued）＋arXiv 官方题名检索与摘要逐字＋位点 :235 与 `CONTROL_WEIGHT_SNR.md:244` 原文 | ApJ 838,110 究系何文（非本批件）：`query.bibliographic` 途径 0 命中，需按 ApJ DOI 正查或 ADS |

## 261 · DOI 10.1111/j.1365-2966.2007.12550.x（Marinucci et al. 2008, MNRAS 383, 539, needlets）—— 核验态：已核，关联对

**存在性：已核。** Crossref `…/works/10.1111/j.1365-2966.2007.12550.x`：title *"Spherical needlets for cosmic microwave background data analysis"*、MNRAS、volume **383**、issue **2**、page **539–545**、print **2008**、OUP、作者首 6 位 `D. Marinucci, D. Pietrobon, A. Balbi, P. Baldi, P. Cabella, G. Kerkyacharian`。arXiv 官方 API 题名检索命中 `<id> = 0707.0844**v1**`，`<arxiv:journal_ref>` = "Monthly Notices of the Royal Astronomical Society, Volume 383, Issue 2, pp. 539-545, January 2008" ⇒ 双源卷期页年完全一致；仓内 "Marinucci et al. 2008 (needlets), MNRAS 383, 539" 三项对。
**版次命名陷阱专项（高危形态③邻）**：DOI 串内的 `2007`（Wiley 以在线首发年命名 `j.1365-2966.2007.12550.x`）与 print 年 **2008** 并存；仓内以 **2008** 计年并配该 DOI ⇒ **不是年份漂移**（卷 383 = 2008 一致）。整改时**不得**因 DOI 串里的 2007 而改年——这是一处"看似矛盾、实为出版社命名规则"的假红。

**位点与实际断言（原文已打开）**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:110`（第 6 簇表行 6.9，四栏全读）——

> | 6.9 | Marinucci et al. 2008 (needlets), MNRAS 383, 539. DOI 10.1111/j.1365-2966.2007.12550.x `[CR]` | **needlets**——在实空间与谐空间同时局域的球面小波构造 ⇒ GP/P-spline 重建的替代：需要天光/噪声场的**多尺度**表示……也是球面小波在天文学中的可引用先例 | CMB 专用统计机械（角功率谱估计、原初非高斯检验）与统计各向同性假设；本项目的场明显各向异性…… | 本项目场定义在 HEALPix tile 网格上，不是全天 |

**关联：关联对。** (i) 角色"needlets＝球面小波构造"由题名逐词命中（*Spherical needlets*），摘要首两句逐字："We discuss **Spherical Needlets** and their properties. **Needlets are a form of spherical wavelets which do not rely on any tangent plane**"；(ii) 不借鉴栏"CMB 专用统计机械"与题名 *for cosmic microwave background data analysis* 同侧，未夸大；(iii) 行面自限"可引用先例"与该文性质一致。**残留（不进结论）**：仓内"**实空间与谐空间同时局域**"这一具体性质只取到摘要前两句，未取全摘要/正文 ⇒ 若把它当构造定义使用需补一次取证。

**版次/载体：版本对。** 被引内容（needlets 球面小波构造）在 2008 出版版（MNRAS 383(2) 539–545）内；预印本 0707.0844 **仅 v1** ⇒ 无"后续版才有"的风险；仓内未引 arXiv 号，句柄单一（DOI → OUP 出版版）。

| 261 | DOI 10.1111/j.1365-2966.2007.12550.x | 已核 | 版本对（MNRAS 383(2), 539–545, 2008；预印本 0707.0844v1 单版；DOI 串含 2007 非漂移） | 关联对（题名＋摘要首两句坐实 needlets/球面小波与 CMB 限定） | Crossref 题录＋arXiv 官方题名检索（journal_ref 卷期页一致）＋摘要逐字；位点 :110 四栏原文 | "实空间与谐空间同时局域"完整表述：需摘要全文/正文 §2 |

## 267 · DOI 10.2352/ei.2023.35.6.iss-347（Borek 2023, Electronic Imaging 35, 347）—— 核验态：已核，关联性内容级 UNPROVEN

**存在性：已核。** Crossref `…/works/10.2352/ei.2023.35.6.iss-347` 回包（题名逐字符）：**"Implementation of EMVA 1288 Standard Release 4.0 for Characterization of Image Sensors"**，container-title *Electronic Imaging*，volume **35**，issue **6**，page **"347-1-347-6"**，print **2023**（issued 2023-01-16），publisher Society for Imaging Science & Technology（SPIE），作者 `Megan E. Borek`；DOI 句柄解析 302 → `https://library.imaging.org/ei/articles/35/6/ISS-347` ⇒ 句柄可解析、指向同一篇。
**取证过程登记（同一条 DOI 出现过两个题名读数）**：本条首次 Crossref 读出的题名是另一种措辞（"Characterization of Image Sensitivity and Noise Using the EMVA 1288 Standard"），第二、三次（要求逐字符引用 `title` 数组）读数一致为上句 ⇒ 判为**工具摘要化失真**，以逐字符复核的题名与卷期页为准；SPIE 落地页 `library.imaging.org` 只返回 JS/CSS 骨架（无文章数据）；WebSearch 该 DOI 串只命中中文厂商页。

**位点与实际断言（原文已打开）**：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:48`（第 2 簇表行 2.5，**三件并引**：EMVA 1288 标准 `[page]`、Jähne 2010 `[CR]`、**Borek 2023** `[CR]`）——断言栏："转换增益、时间暗噪声、SNR、线性度/PRNU/DSNU 表征的**标准化可复现定义**；声称增益/读出噪声可跨相机比较时应引标准而非只引论文"；边界栏明写"机器视觉工作区间……EMVA 1288 **不覆盖**天文量级暗流散粒噪声、不覆盖天光项、不覆盖亚电子噪声；**不得**把它的 SNR 定义当作我们的 SNR 定义（我们必须含天光）"。按"并引按件判"：本条**只判 Borek 件**，EMVA 标准本体（`[page]`）与 Jähne 2010 属其他批次。

**关联：关联性 UNPROVEN（内容级）——角色层无错绑证据，但不构成"已核"。** 题名所示该文就是 **EMVA 1288 Release 4.0 的实现与图像传感器表征**，与该行"标准＋文档"的并引角色同侧，无"A 文干 B 文"迹象；但该行消费的实质是**逐项定义清单**（转换增益／时间暗噪声／SNR／线性度／PRNU／DSNU 是否确在该文内给出），需正文或摘要——本条 6 次取证用尽 ⇒ 依前言 §2「全文不可得时明确降级为关联性 UNPROVEN」处理，**不得**以题名相关冒充关联已核。

**版次/载体：版本对（钉到卷期页）。** 仓内 "Borek 2023, Electronic Imaging 35, 347-1"＝卷 35、起始页 347-1（注册库页字段 "347-1-347-6"）、2023，三项一致；且**标准版本号 Release 4.0 写在题名内**，不存在"标准版次未定"问题。**载体性质登记**：SPIE *Electronic Imaging* 会议文集条目，**Crossref 有记录**（不同于前言对"会议文集常 400／无记录"的一般告警），故一次 Crossref 即得句柄级证据。

| 267 | DOI 10.2352/ei.2023.35.6.iss-347 | 已核 | 版本对（Electronic Imaging 35(6), 347-1–347-6, 2023；题名内含标准 Release 4.0） | 关联性内容级 **UNPROVEN**（"逐项定义在该文"未读到正文；角色层无错绑证据） | Crossref×3（题名逐字符复核，首读失真）；doi.org→library.imaging.org 302（JS 骨架无正文）；WebSearch×1（仅中文厂商页） | 需 SPIE/著者 PDF 的摘要或表格：走 library.imaging.org 需 JS 渲染（浏览器途径）或 SPIE 会议集 PDF |

<!-- PROGRESS: 14/14 -->

---

## 本批三态计数（14 条）

| 字段 | 已核／对 | 问题档 | UNPROVEN |
|---|---|---|---|
| 存在性 | 14（全部经 Crossref 或 arXiv 官方 API 句柄级复核；#195/#225/#237/#249/#261/#267 另有第二源一致） | 不存在 0 | 0 |
| 关联性 | 关联对 7（#195 #207 #213 #225 #237 #249 #261） | **角色错绑／关联错 3**（#189 XP"外定标"挂内部处理件；#231 A&K 2003 畸变件被派 ePSF 活；#255 "巡天策略"无对应内容） | **内容级 4**（#201 δ 取值表；#219 starlet 小波；#243 响应函数三分解＋m5；#267 EMVA 逐项定义清单） |
| 版次/载体 | 版本对 11 | **未钉版次 2**（#195 逐字核验挂在有 v1/v2 而无版本号的 arXiv 句柄；#213 题名省略＋无页范围/issue） | **版本错 1**（#255 应指 2018, PASJ 70(SP1), S6；仓内写 2017, PASJ 70 且漏文章号） |

## 本批 P1 缺陷清单

| # | 缺陷 | 落点（绝对路径按仓根） | 判据 |
|---|---|---|---|
| P1-1 | **小标题与件的角色错绑**：`XP 外定标（仪器响应模型与定标精度）` 伞下挂 De Angeli A2（其摘要自述 **internal calibration** 与处理/验证链），而仓内 [B38]/[B52] 两处已把 A2/A3 分工写对 ⇒ 同一件在本仓两处口径相反 | `docs/science/PHOTOMETRY.md:311`；对照 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:141,188` | 摘要逐字＋双源题录（#189） |
| P1-2 | **并写成"一件事的两个出处"**：[F-12] 两篇并列，四栏（通量定义/借鉴/不借鉴）只描述 ePSF 那一半，Anderson & King **2003 实主题＝WFPC2 几何畸变改正**，仓内无一栏写畸变 ⇒ 按行判绿、按件判红 | `实验/absolute-snr/docs/surveys/f-instr-survey.md:35,169-178` | Crossref 题名 "An Improved Distortion Solution …"（#231） |
| P1-3 | **年与卷不配＋文章号缺**：`Huang et al. 2017, PASJ 70` 实为 **2018, PASJ 70(SP1), S6**（online 2017-12-22）；PASJ 无页码体例，只给卷号无法唯一定位；并与 `CONTROL_WEIGHT_SNR.md:244` 的 "Huang 2017, ApJ 838, 110" 撞作者-年，两件易混 | `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:235`；对照 `docs/science/CONTROL_WEIGHT_SNR.md:244` | Crossref print/issued 双字段＋article-number（#255） |
| P1-4 | **"δ 取值表"伪托风险（高危形态②）**：把"高斯 95% 渐近效率的 δ=1.345 数值表"归给 Holland & Welsch 1977，6 类途径只核到题录与题名级 IRLS 角色，正文表未取得 ⇒ 该文是否载此表不得知 | `docs/science/PHASE2_UPM.md:137`（另 `:316`、`docs/algorithms/PHASE2_SESSION.md:135`、`docs/algorithms/PLATESOLVE.md:196` 同族措辞） | Crossref＋3 次检索＋2 次镜像失败（#201） |
| P1-5 | **后出术语回溯给早期一手文**：`starlet 小波` 被写进 Starck & Murtagh **1998** 的括注，而 PixInsight 官方页把 starlet（aka à trous）归 [3]＝Starck-Murtagh-Fadili 2010 书、把 MRS 归 [4]＝1998 PASP；1998 正文未取得（IOP 验证墙＋ADS 405） | `docs/science/NOISE_MODEL.md:378`（`SCIENTIFIC_REFERENCES.md:124,167` 同件不带该括注） | PixInsight ImageWeighting 实页逐字＋Crossref（#219） |
| P1-6 | **"逐字核验"声称的载体不可复核**：ASTROMETRY 把 §2.1.1 式(1)/§2.1.4 的逐字核验挂在**无版本号**的 `arXiv:astro-ph/0207407` 上，实测该号有 v1（2002-07-18）与 v2（2002-11-13）两版；本次逐字比对对象是 A&A 出版版 | `docs/science/ASTROMETRY.md:265` | arXiv API 版本字段＋aanda 全文（#195） |
| P1-7 | **内容级口径被 `[CR]` 标签覆盖**：#243 位点写"从仪器响应函数定义…滤光片×探测器×大气乘积"、`CONTROL_WEIGHT_SNR.md:244` 写"5σ 点源深度 m5"，而该文摘要（1203.0297v1 全段）既无 "instrument response function" 也无 "m5/5σ" 字样；`[CR]` 只证明书目，不证明这两句 | `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:120`；`docs/science/CONTROL_WEIGHT_SNR.md:244` | arXiv 摘要逐字＋关键词存在性检查（#243） |
| P1-8 | **列而不用／题名省略**：DoPHOT 行只带分组标题＋`[S]`，题名写成 "DoPHOT…"，全仓无任何一句断言消费该件；同类还有 van Dokkum 2001 在 `:218` 与 Irwin 1985 在 `:146` 的清单行（真实用途在别处已核，但清单行自身不构成可回溯出处） | `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:174`（另 `:218`、`:146`） | `git grep` 全仓唯一命中＋分组标题原文（#213） |
| P1-9 | **批次清单行号漂移**：#219 清单给 `NOISE_MODEL.md:370`，基线 `c8f64e9a` 实际命中 **:378**（漂 8 行）⇒ 清单行号不可当唯一索引，须带标识符串作并行键 | `docs/science/NOISE_MODEL.md` | `git grep -n`（#219） |

## UNPROVEN 清单（附实际走过的途径与各自结果，供下轮换路）

| 条 | 未证事项 | 用过的途径 → 结果 | 下一轮换路建议 |
|---|---|---|---|
| #201 | H&W 1977 内是否载 δ=1.345（95% 效率）表 | Crossref `10.1080/03610927708827533` → 题录命中、**回包无 abstract**；WebSearch×3 → 只命中把 H&W 当 IRLS 出处的第三方与统计软件文档；MATLAB `robustfit` 两处镜像（`ece.northwestern.edu`）→ fetch failed | T&F 正文需浏览器途径或期刊扫描本；或改挂 Huber & Ronchetti 2009 §4 效率表（本行已并列） |
| #219 | 1998 文所用变换是否 starlet/à trous | Crossref → 题录命中；IOP `…/article/10.1086/316124` → **302 → Radware 验证墙**；ADS `1998PASP..110..193S/abstract` → **405**；PixInsight 实页 → 只给该文档自身引用结构（starlet→[3] 2010 书）；WebSearch×1 → 无一手命中 | ADS 全文 token 或 PASP 纸质扫描；否则把 starlet 改挂 Starck-Murtagh-Fadili 2010 |
| #243 | "仪器响应函数＝滤光片×探测器×大气"、"5σ 深度 m5" 两口径在正文何处 | Crossref×2 → 题录命中、**无 abstract 字段**；arXiv `id_list=1109.3163` → **回包是无关的量子关联文**（误按印象试号，弃用）；arXiv 题名检索 → 命中 1203.0297v1，摘要逐字含 response/m5 关键词检查（判 absent） | ApJ 正文（IOP 走 Radware 墙）需浏览器途径／ADS token；引用时补"版次＋小节" |
| #267 | 转换增益／时间暗噪声／SNR／线性度／PRNU／DSNU 是否逐项载于 Borek 2023 | Crossref×3 → 题名逐字符复核（**首读给出另一种题名＝工具摘要化失真**）；`doi.org` → 302 到 `library.imaging.org`；该页 → **只回 JS/CSS 骨架，无正文**；WebSearch×1 → 仅中文厂商页 | SPIE PDF 或 library.imaging 的 JS 渲染（浏览器途径）；EMVA 标准本体页面承担同一断言（非本批） |
| #249 | Irwin 1985 文内是否有孔径/拟合误差的具体式（支撑 `PHOTOMETRY.md:382` 的"PSF 拟合不确定度"） | Crossref×2 → 卷期页年作者命中；正文 → OUP 1985 墙未开（仓内三处自认未取得） | ADS 全文 token 或纸质扫描；否则把 :382 表行降为"方法族出处" |
| #225 | `CCD_LINEAR_DEFECT_LITERATURE.md:286` 的 §2–§3.2 节号与 `L⁺/F=0.7/1.8/21`、`flim=2`、`flim≈5` | arXiv API `astro-ph/0108003v1` → 摘要逐字命中但不含这些数值/节号（2001 年文无 arXiv HTML） | 走 arXiv PDF 正文（乱码风险）或 ADS 全文；本批只判到角色层 |
| #189 | De Angeli A2 的 A&A 出版版是否另设"外定标"节（本条判"角色错绑"的反证可能） | Crossref＋arXiv API＋arXiv abs 摘要（v1，abridged） → 摘要只述内部处理/验证 | 开 `aanda.org/articles/aa/full/…` 出版版目录一节号；若目录含 external calibration 节则本条改判 |
| #255 | 仓内 "Huang 2017, ApJ 838, 110"（另一件）究系何文 | Crossref `query.bibliographic` → **0 命中**（只回 Leauthaud 2020 等无关项） | 按 ApJ DOI 正查（10.3847/…）或 ADS；非本批条目，登记供合并台账时消歧 |

## 新发现的缺陷形态（本批独有，前面批次未登记）

1. **同一件在本仓两处口径相反**（#189）：一处清单已把 A2/A3 分工写对，另一处科学文档把两件混进同一小标题。存在性检查、标识符解析、卷页核对**全绿**，只有"小标题↔件"的角色映射红 ⇒ 门必须能读"分组标题↔件职责"这一层。
2. **两篇并列承担一件事的四栏描述**（#231）：按行判必然绿；按件判才红（2003 件的畸变主题在仓内从未被写成借鉴点）。这类"并写掩盖错绑"与 CIT-06 的"作者首字母伪值"不同族，判据要显式要求"件→栏位"映射。
3. **online-first 年与卷年混用＋文章号缺**（#255）：Crossref 同时有 `published-print=2018` 与 `issued=2017-12-22`，仓内取后者年配前者卷 ⇒ 年-卷不配；PASJ 体例无页码，缺 `S6` 即不可唯一定位。核验必须同时读 print/issued 与 article-number 三字段，只看 `published` 会被这一类骗过。
4. **Wiley DOI 串内年 ≠ 出版年**（#261）：`10.1111/j.1365-2966.**2007**.12550.x` 对应 print **2008**（MNRAS 383(2) 539–545）⇒ 假红源。核验表应固定注记"Wiley/旧 OUP 的 DOI 串年份不判漂移"。
5. **"件选对、口径无载体"**（#243/#237/#201）：被引件确是该主题的一手文，但仓内挂在它身上的**具体口径**（响应函数三分解、m5、δ 取值表、QE×滤光片×光学）在最全文本里逐字不存在 ⇒ 三态必须扩到"内容级"，否则存在性已核会被当成整句已核。
6. **核验工具自身的题名失真**（#267）：同一 DOI 两次读出两个题名（第二个是编造式近义改写）。⇒ 结论必须要求"逐字符引用 `title` 数组"复核，且**首读不得作为凭据**；这是审计途径自身的可靠性缺陷，应写入派单前言。
7. **代表位点无断言**（#213/#225/#249 的清单行）：文献清单行只带分组标题、省略号题名与 `[V]/[S]`，"关联"核验对象在行面上根本不存在 ⇒ 应把这类条目转入"清单卫生"门（题名完整、有角色描述、有消费点）。

## 不敢判的与缺什么才能判

- **不敢判 1**：H&W 1977 是否载 δ=1.345 表（#201）。缺：该文正文表（T&F 正文／期刊扫描本／浏览器途径）。若表在别处（Huber 1964 或 Huber & Ronchetti 2009 §4），本条从"子句 UNPROVEN"升级为"载体错挂"。
- **不敢判 2**：1998 MRS 是否用 starlet/à trous 实现（#219）。缺：PASP 正文。PixInsight 的引用结构（starlet→2010 书、MRS→1998 文）只证明**该文档**的分工，不能反推 1998 文内部。
- **不敢判 3**：Tonry 2012 正文是否把通带写成"滤光片×探测器×大气"、是否定义 m5（#243）。缺：ApJ 正文（IOP 验证墙／ADS 需 token）。摘要级已明确不含这两个词，故不能反向判"仓内写错"，只能判"载体未取得"。
- **不敢判 4**：Borek 2023 是否逐项给 EMVA 1288 的转换增益/暗噪声/SNR/PRNU/DSNU 定义（#267）。缺：SPIE PDF 或可渲染正文。
- **不敢判 5**：De Angeli A2 出版版目录是否含"external calibration"节（#189）。缺：A&A 出版版目录页。**这条最可能反转**：若目录真有该节，P1-1 从"角色错绑"降为"小标题措辞可接受"；若没有，则维持红。本批据摘要判红，并如实登记反转条件。
- **不敢判 6**：Irwin 1985 文内的孔径/误差公式（#249）、van Dokkum 2001 的 §2–§3.2 数值（#225）同上一类，缺正文。

## 覆盖率自报

**14/14 条**（CIT-11 全批）每条给出**存在性／版次与载体／关联**三字段单值判定，无留白、无并列存疑；每条结论行已按派单格式给到。

- 网络取证：**每条均 ≤6 次**（最高 #267 = 6 次；#201 = 5；#243/#255/#219 = 5；其余 ≤4）。Crossref 同轮并发 ≤2，其间出现 1 次 429（`10.1093/pasj/psx126` 首查）⇒ 按纪律退避后改单查复核，读数与后两次一致。
- 途径命中率：Crossref 14/14 全部命中（含 SPIE 会议文集条目也有记录）；arXiv 官方 API 6 次命中（含一次误号被 API 判为无关文，**未**据此下结论）；aanda 出版版全文 1 次（#195 逐字）；PixInsight 官方页 1 次（#219）；IOP 1 次成（#207）、1 次撞 Radware 验证墙（#219）；ADS `/abstract` 1 次 405（#219，按派单标 UNPROVEN）；`doi.org` 解析 1 次（#267，302 到 JS 骨架页）。
- 未凭记忆断定任何存在性；未编造卷页/DOI/URL——唯一一次"按印象试号"（`1109.3163`）在过程登记中公开，并被官方 API 否证后弃用。
- 仓库侧只读：`git grep` 3 轮多模式检索＋`Read` 12 个区段（PHOTOMETRY/ASTROMETRY/PHASE2_UPM/NOISE_MODEL/SCIENTIFIC_REFERENCES/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE/f-instr-survey/frame-snr-survey/REVERSE_VERIFY_BIBLIOGRAPHY 等）；**未改仓库任何文件、未跑构建/ctest/门禁、未写 `整改/out/文献台账.csv`**；git 零写操作（仅 `rev-parse`/`status` 读）。基线核对：`HEAD = c8f64e9ab6b867e4f108ba1e0073a8f2a97cfde9`。
- 输出仅本文件：`独立审计/证据/核验-CIT-11.md`。
