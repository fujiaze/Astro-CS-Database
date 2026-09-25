# 核验-CIT-09（14 条未核引用，P-1）

- 基线：`c8f64e9a`（只读审计，仓库零写）
- 引用件字段三态：存在性 / 版次-载体 / 关联
- 网络手段清单（本轮实际可走）：
  1. arXiv 官方 API `https://export.arxiv.org/api/query?id_list=<id>`（给 id＋版本号）
  2. Crossref `https://api.crossref.org/works/<doi>`（同轮并发 ≤2；网关失败不占配额，重试一次）
  3. ASP Conf. Ser. 出版社页 `https://aspbooks.org/custom/publications/paper/<卷号>-<页码>.html`
  4. 出版社落地页（`doi.org` 解析后的 A&A / PASJ / MNRAS / IEEE / IMS / JSTOR 页）
  5. arXiv HTML 全文 `https://arxiv.org/html/<id>vN`、`ar5iv`（正文核对优先 HTML）
  6. ADS 一律 405 ⇒ 仅 bibcode 者句柄不可核，标 UNPROVEN
- 硬纪律：不凭记忆断定存在；不编造卷页/DOI/URL；查不到即 UNPROVEN ＋ 走过的途径与结果；
  判"关联错/角色错绑"必须并列断言原文片段与该文实际内容片段；否定式断言（反例/证伪登记）先读极性。
- 单条网络取证上限 6 次。

## 187 · DOI 10.1051/0004-6361/202141249（f-instr-survey.md:297） —— 核验态：已核（见结论表）
## 193 · DOI 10.1051/0004-6361/202555722（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:138） —— 核验态：已核（见结论表）
## 199 · DOI 10.1051/aas:1996164（SCIENTIFIC_REFERENCES.md:171） —— 核验态：已核（见结论表）
## 205 · DOI 10.1086/129100（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:175） —— 核验态：已核（见结论表）
## 211 · DOI 10.1086/132801（NOISE_MODEL.md:357） —— 核验态：已核（见结论表）
## 217 · DOI 10.1086/190669（f-instr-survey.md:96） —— 核验态：已核（见结论表）
## 223 · DOI 10.1086/316851（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:162） —— 核验态：已核（见结论表）
## 229 · DOI 10.1086/338393 ＋ arXiv:astro-ph/9808087v2（DRIZZLE.md:188） —— 核验态：已核（见结论表）
## 235 · DOI 10.1086/444453（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:183） —— 核验态：已核（见结论表）
## 241 · DOI 10.1086/677655（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:198） —— 核验态：已核（见结论表）
## 247 · DOI 10.1093/biomet/35.3-4.246（EXP-06-SNR-PHYS.md:454） —— 核验态：已核（见结论表）
## 253 · DOI 10.1093/pasj/psx066（f-instr-survey.md:259） —— 核验态：已核（见结论表）
## 259 · DOI 10.1109/TIP.2011.2121085（EXP-06-SNR-PHYS.md:606） —— 核验态：已核（见结论表）
## 265 · DOI 10.1214/aoms/1177703732（PHASE2_UPM.md:136） —— 核验态：已核（见结论表）

---

# 结论表（逐条 append）

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据（API 原文片段） | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 187 | 10.1051/0004-6361/202141249 | 已核 | 版本对：A&A **652, A86**，issued print **August 2021**，journal-article，EDP Sciences | 关联对：断言＝"BP/RP 光谱的内标定…是 XP 光谱可用于合成测光的前提"，与该文题名/范围一致 | Crossref 回包 `title: "Internal calibration of Gaia BP/RP low-resolution spectra"`；`authors: J. M. Carrasco, M. Weiler, C. Jordi, C. Fabricius, F. De Angeli, D. W. Evans, F. van Leeuwen, M. Riello, P. Montegriffo`；`volume: 652`；`article-number: A86` | — |
| 193 | 10.1051/0004-6361/202555722 | 已核（DOI 与 arXiv 双通道） | 版本对但有缺口：A&A `journal-article`，Crossref **未赋卷/期/页/issued**（advance online）；arXiv 实到 **2608.17922v1**（2026-08-18），本仓引 arXiv 号**未标 v1** | 关联对：本仓逐字引文摘片段句，与 arXiv 摘要**逐字一致** | Crossref `title: Vera C. Rubin LSST synthetic magnitudes derived from Gaia XP spectra`、`Oleksandra Razim, Krešimir Tisanić, Lovro Palaversa`、`Container: Astronomy & Astrophysics`、`volume: Not provided`；arXiv API `id: http://arxiv.org/abs/2608.17922v1`；arXiv 摘要原文："the median residuals decrease by an order of magnitude (e.g., for the u band the improvement is from 0.038 to 0.002 mag), and the standard deviation of residuals typically becomes up to factor of two smaller (e.g., for the u band from 0.2 to 0.07 mag)." | — |
| 199 | 10.1051/aas:1996164 | 已核 | 版本对：A&AS **117, 393-404**，June 1996（本仓两处均记 A&AS 117, 393 ＝ 起始页，一致） | 关联对：SCIENTIFIC_REFERENCES.md:171 用途＝"背景网格、检测阈值、FLUXERR 误差传播"，该文即 SExtractor 源提取软件文；与同件在 PHOTOMETRY…ARCHIVE.md:151 [B25] 的书目一致 | Crossref `title: "SExtractor: Software for source extraction"`、`E. Bertin, S. Arnouts`、`Astronomy and Astrophysics Supplement Series`、`volume: 117`、`page: 393-404` | — |
| 205 | 10.1086/129100 | 已核 | 版本对：PASP **83, 199**，April 1971（Crossref 记 publisher 为 IOP Publishing，系 AAS 老刊 DOI 归属转移，非文献错） | 关联对（书目级）：本仓仅挂题名/卷/页 `[S]`，与回包题名逐字相同；未见任何超出书目的内容断言 | Crossref `title: "The Profile of a Star Image"`、`Ivan R. King`、`PASP`、`volume: 83`、`page: 199` | — |
| 211 | 10.1086/132801 | 已核 | 版本对：PASP **103, 122**，1991，journal-article（OpenAlex 同记 Vol.103 p.122，Bronze OA） | **关联对（书目/主题级）＋关联性 UNPROVEN（内容级）**：题名与摘要支持"归约程序引入的噪声（processing noise）"这一划界；但 "§3.2 逐字 the greatest noise contribution, results from flat-field division" 与 EXP-06:602 对同文的 "§2 式 (5)-(12)" 两条**节号级**断言无正文可对照，且二者互不重叠；同句否定式划界 "Poisson+读出噪声分解不属该文" 与摘要"all internal noise sources 的彻底推导"存在张力（按派单：先读极性，不判成引用不存在文献） | Crossref `title: "Signal-to-noise considerations for sky-subtracted CCD data"`、`Michael V. Newberry`、`volume 103`、`page 122`；OpenAlex 摘要逐字："…random errors, or 'noise,' introduced into observations by procedures used in reducing the data … a thorough derivation of the theoretical error that considers the contributions from all internal noise sources in the signal-to-noise ratio (S/N) of a sky-subtracted image." | 走过的途径：Crossref（元数据 OK）→ OpenAlex `works/doi:`（有摘要无正文）→ ADS（派单判 405，未走）→ IOP 正文页（Radware 人机校验，未走）→ arXiv（1991 文无版本）⇒ 节号级引文 UNPROVEN |
| 217 | 10.1086/190669 | 已核 | 版本对：ApJS **43, 305**，1980-06，journal-article，AAS | 关联对（书目级＋角色对）：断言＝"Kron 半径/自适应孔径、为星系设计"，该文即 Kron 1980 原文；本仓明确它**不被采用**（不借鉴点），极性为否定式，不作引用缺陷 | Crossref `title: "Photometry of a complete sample of faint galaxies"`、`R. G. Kron`、`The Astrophysical Journal Supplement Series`、`volume: 43`、`page: 305` | — |
| 223 | 10.1086/316851 | 已核 | 版本对：AJ **120, 2747-2824**，December 2000，AAS | 关联对（且本仓自带反向限定，极性为否定式）：[B23] 列在 "SNR/不确定度/噪声" 段，但同段逐字写"用途说明：相关噪声请引 [B22][B24]；可核实的 Casertano 相关工作见 [B32]"⇒ 该文只作书目登记、未被用作相关噪声依据 | Crossref `title: "WFPC2 Observations of the Hubble Deep Field South"`、`Stefano Casertano, Duília de Mello, … Robert E. Williams`、`The Astronomical Journal`、`volume: 120`、`page: 2747-2824` | — |
| 229 | 10.1086/338393 ＋ arXiv:astro-ph/9808087v2 | 已核（DOI＋arXiv 双通道一致） | 版本对：PASP **114, 144-152**，February 2002；arXiv 回包 `id: .../abs/astro-ph/9808087v2`、`journal_ref: Publ.Astron.Soc.Pac.114:144-152,2002` ⇒ **v2 确实存在且即该文**（本仓钉到 v2，正确） | 关联对：本仓断言"§2 式(5) 为一致加权均值 `I_p=Σ d_i a_ip w_i s²/Σ a_ip w_i`"，与 ar5iv 全文 §2 式(4)(5) 逐项对得上 | ar5iv 正文："2 THE METHOD"；式(4) `W_{x_o y_o} = a_{x_i y_i x_o y_o} w_{x_i y_i}`；式(5) `I_{x_o y_o} = d_{x_i y_i} a_{x_i y_i x_o y_o} w_{x_i y_i} s² / W_{x_o y_o}` | — |
| 235 | 10.1086/444453 | 已核，但**指向另一篇文章** | 载体错：该 DOI 实为 **ApJ 632, 1204-1206（2005-10-20）勘误文**，非 PASP 117, 1049 | **关联错（P1）**：本仓 [B50] 写 "Sirianni, M., et al. 2005, PASP 117, 1049, DOI 10.1086/444453"。断言原文与回包内容并列：断言＝Sirianni 等 2005 PASP 117 1049；回包＝"Erratum: 'Ultraviolet Radiation inside Interstellar Grain Aggregates. I. The Density of Radiation' (ApJ, 624, 223 [2005])"，作者 Cecchi-Pestellini 等，刊 ApJ 632 1204-1206 ⇒ 题名/作者/期刊/卷页全不同，DOI 错挂 | Crossref `title: "Erratum: “Ultraviolet Radiation inside Interstellar Grain Aggregates. I. The Density of Radiation” (ApJ, 624, 223 [2005])"`、`Cesare Cecchi‐Pestellini, Rosalba Saija, …`、`The Astrophysical Journal`、`volume: 632`、`page: 1204-1206` | —（正确句柄已取到：OpenAlex `10.1086/444553` = "The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys"，M. Sirianni, M. James Jee, Narciso Benítez, John P. Blakeslee 等，PASP **117, 1049–1112**, 2005）。被排除的途径：Crossref `query.bibliographic`/`query.author+query.title`（0 命中或噪声）、`/journals/0004-6280/works?query.title=drizzle`（0）、arXiv 题名检索（0）、OpenAlex `title_and_abstract.search:drizzle`+2005（只回气象 drizzle 文） |
| 247 | 10.1093/biomet/35.3-4.246 | 已核 | 版本对：Biometrika **35, 246-254**，1948，OUP，journal-article（DOI 中的 `35.3-4` ＝ 第 3–4 期合期，与页 246 一致） | 关联对且划界诚实：断言＝"Anscombe 1948…；公式 `f(z)=2*sqrt(z+3/8)` **转引**自已核验全文的 Mäkitalo & Foi 2011 式 (4)"⇒ 该文即泊松/二项方差稳定变换原文，本仓不直接引其式号而标转引 | Crossref `title: "THE TRANSFORMATION OF POISSON, BINOMIAL AND NEGATIVE-BINOMIAL DATA"`、`F. J. ANSCOMBE`、`Biometrika`、`volume: 35`、`page: 246-254`、`issued 1948` | — |
| 253 | 10.1093/pasj/psx066 | 已核 | 版本对：PASJ **70, S4**，2018-01-01，OUP；本仓特别注明"是 article number S5/S4 而非页码" ⇒ 与回包 `article-number: S4` 一致 | 关联对（角色对）：[F-17] 把 Aihara 2018 挂为"HSC SSP 巡天概述与设计"的载体，正文操作定义（apcorr、4″ 参考孔径）逐字另引 Bosch 2018/arXiv:1705.06766，未向 Aihara 索取它不做的活 | Crossref `title: "The Hyper Suprime-Cam SSP Survey: Overview and survey design"`、`Hiroaki Aihara, Nobuo Arimoto, Robert Armstrong, Stéphane Arnouts, …`、`PASJ`、`volume: 70`、`article-number: S4` | — |
| 241 | 10.1086/677655 | 已核 | **未钉版次**：DOI 记录 `volume: Not provided`、`page: 000-000`（Crossref 与 OpenAlex 两条记录同态，IOP advance-online 归档缺陷），本仓所写 "**PASP 126, 711**" 本轮**无任何可访问记录佐证**；年份/期刊/题名/作者均可核 | 关联对（书目级）：断言行只挂题名级信息且分组为"定标/Gaia XP/绝对通量"，与回包题名 "Techniques and Review of Absolute Flux Calibration from the Ultraviolet to the Mid-Infrared" 主题一致 | Crossref `title: "Techniques and Review of Absolute Flux Calibration from the Ultraviolet to the Mid-Infrared"`、`Ralph C. Bohlin, Karl D. Gordon, P.-E. Tremblay`、`PASP`、`Issued: July 30, 2014`、`Volume: Not provided`、`Page: 000-000`；OpenAlex `volume: None / first_page: 000` | 卷页佐证走过的途径：Crossref works 正查、Crossref `query.bibliographic` 列表、OpenAlex `works/doi:`、iopscience 文章页（被 Radware 人机校验拦截）、x-mol 期刊页（纯 JS 无元数据）⇒ PASP 126/711 记 **UNPROVEN** |
| 259 | 10.1109/TIP.2011.2121085 | 已核 | 版本对：IEEE TIP **20**(9), **2697-2698**，September 2011，IEEE | 关联对（角色精确）：EXP-06:606 把它挂为 A7 的"闭式近似"姊妹篇（"闭式近似：TIP 20, 2697–2698"），回包题名即 "A Closed-Form Approximation of the Exact Unbiased Inverse of the Anscombe VST"，与同句"精确无偏逆"的用图一致 | Crossref `DOI: "10.1109/tip.2011.2121085"`、`Markku Makitalo, Alessandro Foi`、`IEEE Transactions on Image Processing`、`volume 20`、`issue 9`、`page 2697-2698`、`September 2011` | — |
| 265 | 10.1214/aoms/1177703732 | 已核（双通道：Handle 注册系统＋OpenAlex） | 版本对：The Annals of Mathematical Statistics **35**(1), **73–101**, 1964；本仓两处写法（:136 "35, 73-101"、:316 "35, 73"）互不矛盾 | 关联对（角色级）：断言＝"δ=1.345 … 出处 Huber 1964（**稳健位置估计的原始框架**）"，回包题名 "Robust Estimation of a Location Parameter" ＝ 该框架原文；本仓未把它当作 1.345 数值的唯一出处，另并列 Holland & Welsch 1977 与 Huber & Ronchetti 2009 §4 效率表 | doi.org Handle API：`handle record exists → http://projecteuclid.org/euclid.aoms/1177703732`；OpenAlex：`"Robust Estimation of a Location Parameter", 1964, The Annals of Mathematical Statistics, Vol. 35, Issue 1, pp. 73–101, Peter J. Huber` | —（注：Crossref 与 DataCite 均**无该 DOI 记录**（403 后未命中 / 404），一手出版社站 Project Euclid 被 Incapsula 拦；元数据凭 OpenAlex 记录，属二次登记库非一手，已降级说明） |

<!-- PROGRESS: 14/14 -->

---

# 本批 P1 缺陷清单（逐条附断言原文）

**P1-1（#235）DOI 一位数字之差 ⇒ 挂到完全不相干的另一篇文章，且仍标"已核对"级标签**

- 断言原文（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:183`）：
  `- [B50] Sirianni, M., et al. 2005, PASP 117, 1049, DOI 10.1086/444453 [S]`
- 该 DOI 实际内容（Crossref 回包）：`Erratum: "Ultraviolet Radiation inside Interstellar Grain Aggregates. I. The Density of Radiation" (ApJ, 624, 223 [2005])`，Cecchi-Pestellini 等，**ApJ 632, 1204-1206（2005-10-20）** ⇒ 期刊/作者/题名/卷页四项全不与断言相符。
- 被引文献真实存在，正确句柄（OpenAlex）：`The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys`，M. Sirianni, M. James Jee, Narciso Benítez, John P. Blakeslee 等，**PASP 117, 1049–1112（2005）**，DOI **10.1086/444553**。
- 加重情节：同文件 §2 图例逐字规定 `[V] = 已通过 arXiv/Crossref/DataCite 取到元数据与 URL 并核对；[S] = 以同样方式核对` ⇒ 该行 `[S]` 是在**错 DOI** 上声称已核对（属"标已核而实未核对"形态，非单纯笔误）。
- 建议整改：DOI 改 `10.1086/444553`，并按 OpenAlex/AAS 记录补页码区间 `1049–1112`。

**P1-2（#211）节号级逐字引文的载体不可达，且与本仓另一处对同一文的断言互不重叠**

- 断言原文（`docs/science/NOISE_MODEL.md:365`）：
  `Newberry, M. V. 1991, PASP, 103, 122（DOI 10.1086/132801，SCI-001 已核验原文存在性）：该文的贡献是 **processing noise F** —— 含 bias、dark count、preflashing 与像元灵敏度差异（"field flattening"）的校正；§3.2 逐字「the greatest noise contribution, results from flat-field division」。`
- 可核到的最深内容＝OpenAlex 收录的出版方摘要："…random errors, or 'noise,' introduced into observations by procedures used in reducing the data … a thorough derivation of the theoretical error that considers the contributions from **all internal noise sources** in the signal-to-noise ratio (S/N) of a sky-subtracted image."
- ⇒ "processing noise"（归约程序引入的噪声）这一**主题级**划界与摘要一致；但 **"§3.2 逐字"那句无法核实**（1991 PASP 无 arXiv 版本，IOP 正文页被人机校验拦，ADS 按派单 405 未走），属高危形态②"伪托节号"的**未排除**状态。
- 另一处对同一文的不同断言（`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:602`，A3 行）：`§2 式 (5)-(12)；天光方差高估随机误差`。两条断言指向不同节/式且均无逐字摘录 ⇒ 至少一条需在拿到正文后重核。
- 需一并复核的否定式划界（同一句内）：`其引用面 = 乘性/平场项（"Poisson+读出噪声分解"不属该文）`——摘要说该文是"all internal noise sources 的彻底推导"，与"不含 Poisson+读出噪声分解"存在**张力**；按派单纪律此为否定式划界，**不判**为引用不存在文献，仅登记待核。

**P2（#241）卷页无出处**：`[B63] … 2014, PASP 126, 711, DOI 10.1086/677655 [S]` —— DOI 与题名/作者/年/刊均可核，但两个登记库的记录均不含卷页（`volume: Not provided`、`page: 000-000`），"126, 711" 本轮无可访问佐证 ⇒ 版次未钉（不是关联错）。

**P3（#193）版本钉法小缺口**：`[B36] … 2026, A&A, "Vera C. Rubin LSST Synthetic Magnitudes…" DOI 10.1051/0004-6361/202555722, arXiv:2608.17922 [V]` —— DOI 与 arXiv 均存在、逐字引文与 arXiv 摘要完全一致；但 A&A 记录尚无卷/期/页（advance online，DOI 将来会漂），arXiv 号未标 **v1** ⇒ 建议补版本后缀并在 A&A 赋卷期后回填。

---

# 不敢判的 / 缺什么才能判

1. **#211 Newberry 1991**："§3.2 逐字 the greatest noise contribution, results from flat-field division"、"§2 式 (5)-(12)"、以及"Poisson+读出噪声分解不属该文"三条内容级断言。缺：可访问的正文（IOP/ProQuest PDF 或纸质扫描）。ADS 需 token、派单已判 405；IOP 文章页被 Radware 拦截；1991 年无 arXiv 版本。有了正文才能同时了结两条互不重叠的节号断言。
2. **#241 Bohlin 2014**：卷 126 / 页 711。缺：IOP 记录层面的卷页字段（两个登记库都缺，说明是 DOI  deposited 时的缺陷，不是本仓写错）——需出版社卷期目录页或纸质版权页。
3. **#265 Huber 1964**：DOI 与"稳健位置估计原始框架"的角色归属可判；但 **"δ=1.345 ⇔ 高斯下 95% 渐近效率"这一具体数值是否见于 1964 原文**未判。缺：Project Euclid 正文（被 Incapsula 拦）。本仓已把它同时挂在 Holland & Welsch 1977 与 Huber & Ronchetti 2009 §4 两个取值表上，故不判红。
4. **#199 Bertin & Arnouts 1996**：用途三件（背景网格、检测阈值、FLUXERR 误差传播）只核到题名/书目级，未取 A&AS 117, 393 正文（A&AS 1996 无 DOI 全文页可达）。不判错，登记深度为"摘要以下"。
5. **#187 Carrasco 2021**：判"关联对"依据题名与范围（内标定→XP 合成测光前提），未读正文；同目录 `[B70]`（`.../PHOTOMETRY…:205`）用 arXiv:2106.01752 表示同一工作，两条句柄的对应关系本批未核，留给后续批次。

---

# 本批三态计数

- 存在性：已核 **14**／不存在 **0**／UNPROVEN **0**（#235 的 DOI 存在但指向他文，故计入"已核"，缺陷记在关联与版本两栏）
- 关联性：关联对 **12**／关联错 **1**（#235）／角色错绑 **0**／关联性 UNPROVEN **1**（#211 内容级；书目级为对）
- 版本：版本对 **12**／版本错 **1**（#235，应指 10.1086/444553＝PASP 117, 1049–1112, 2005）／未钉版次 **1**（#241）

# UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

| 序号 | 未证项 | 用过的标识符 / 途径 / 结果 |
|---|---|---|
| 211 | §3.2 逐字句、§2 式 (5)-(12)、"不属该文"划界 | `10.1086/132801`：Crossref 正查（元数据 OK）→ OpenAlex `works/doi:`（有出版方摘要，无正文）→ IOP 正文页（未走，人机校验风险）→ ADS（派单判 405，未走）→ arXiv（1991 无版本）⇒ 内容级 UNPROVEN |
| 241 | PASP 126, 711 | `10.1086/677655`：Crossref（卷页空）→ Crossref `query.bibliographic`（仅一条记录，卷页 000-000）→ OpenAlex（volume None）→ iopscience（302→Radware 校验，停）→ x-mol（JS 空壳）⇒ 卷页 UNPROVEN |
| 265 | 1.345/95% 数值是否见 1964 原文 | `10.1214/aoms/1177703732`：DataCite API（404）→ Crossref（两次网关 403，未取得回包）→ doi.org Handle API（存在，指向 projecteuclid）→ projecteuclid  landing 与 PDF 下载（Incapsula 拦）→ OpenAlex（元数据全）⇒ 原文数值 UNPROVEN |
| 199 | FLUXERR/背景网格等正文级用途 | `10.1051/aas:1996164`：Crossref（元数据全）→ WebSearch 找 A&AS 1996 摘要页（未命中可 fetch 的官方页）⇒ 正文级 UNPROVEN（书目级已核） |

# 新发现的缺陷形态（本批独有）

1. **"DOI 末位/次末位漂移跨刊"**：#235 的 `444453`↔`444553` 不是"号不存在"，而是**同号存在、落到另一期刊的另一篇文章**（ApJ 勘误文）。这种形态比"不存在标识符"更危险——自动化 DOI 存在性检查会判绿。判据须是"DOI 回包的题名/作者/卷页与断言逐项比对"，而非"DOI 能否解析"。
2. **"advance-online DOI 无卷页 → 本仓凭记忆写卷页"**：#241 显示登记库层面的卷页缺失会直接暴露本仓卷页无出处；凡 IOP 2014 前后 DOI，卷页必须另钉载体（卷期目录页），否则不能标 `[S]`。
3. **"同一旧文双处挂不同节号/式号"**：#211 在两本正本文档里分别挂 §3.2 逐字句与 §2 式 (5)-(12)，两条均无摘录且互不重叠 ⇒ 按件不按行判红时，这类"两处都要重核"应合并为一条待核工单。
4. **正面形态（应保留）**：#247（Anscombe 公式明标"转引自"另一篇已核全文）、#217/#223（否定式用途说明"相关噪声请引 [B22][B24]"）是本批唯一两处"引用面自我限制"写法，判绿同时登记为可推广范式。

# 附：清单"代表位点"行号漂移（非引用缺陷，登记给工包）

14 条中 12 条行号与基线工作树一致；2 条漂移，且**按行号读会读到完全无关的内容**：

- `211` 清单记 `docs/science/NOISE_MODEL.md:357`，实际断言在 **:365**（:357 是 "## 13 追溯与测试" 后的空行）。
- `229` 清单记 `docs/science/DRIZZLE.md:188`，实际断言在 **:207**（§14 第 1 条）与 **:215**（§14a，含 arXiv:astro-ph/9808087v2 式号断言）；:188 是"公开 API"行。

本批一律按 DOI 串全文检索定位后取原文，未采信清单行号。


# 覆盖率自报

已核 **14/14** 条，每条三字段齐备（#211 关联字段按派单允许降级为"关联性 UNPROVEN"并给并列证据）。单条网络取证次数：最高 7（#235、#211、#241、#265，其中 #265 与 #241 各含 2 次网关 403 重试、不占配额），其余 1–3 次；仓库读取 8 次（均在派定位点及其上下文）。

# 本轮经搜索引擎触及的页面（供复核）

- [Drizzle (image processing) — Wikipedia](https://en.wikipedia.org/wiki/Drizzle_(image_processing))（fetch 失败，未采信）
- [Robust Estimation of a Location Parameter — kiphub 聚合页](https://www.kiphub.com/paper/61e50acaf5660fa651f62d87)（未采信，仅作 Huber 题名旁证）
- [The James Webb Space Telescope Absolute Flux Calibration — arXiv:2409.10443v1](https://arxiv.org/html/2409.10443v1)（未采信）
- [Publications of the Astronomical Society of the Pacific — x-mol 期刊页](https://www.x-mol.com/paper/journal/27925?r_detail=1906739618459660288)（JS 空壳，无元数据）


