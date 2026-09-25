# 核验-CIT-10（14 条未核引用，P-1）

- 基线：`c8f64e9a`｜批次：CIT-10（清单 `独立审计/批次清单/CIT-10.txt`）｜角色：只读审计节点的文献核验路
- 仓库：`F:\Astro dev\Astro CS Normalization Database`（只读：`Read` / `git grep`；零 git 写、零构建、零 `eng/**` 执行、不写仓库内任何文件）
- 网络手段清单（按派单 §3 与批次指令）：
  1. `https://api.crossref.org/works/<doi>`（DOI 首选；并发 ≤2；回包含标题/作者/容器/卷页/年份）
  2. `https://export.arxiv.org/api/query?id_list=<id>`（arXiv 官方，给版本号）
  3. 出版社自有页（A&A / ApJ-JSTOR / IOP / Biometrika-OUP / IEEE / IMS / T&F）
  4. `https://aspbooks.org/custom/publications/paper/<卷号>-<页码>.html`（ASPC 途径）
  5. ADS 仅给 bibcode ⇒ 该条标 UNPROVEN（无 token 途径）
  6. 正文取证走 HTML 途径（不下载 PDF）
- 三字段判据：存在性（已核／不存在／UNPROVEN）／版次与载体（同一篇预印本 vs 期刊版分开钉；式号/表号/小节号级引用必须同时钉版次＋载体）／关联（打开断言原文对照该文实际内容：关联对／关联错／角色错绑）
- 取证上限：单条网络取证 ≤6 次（派单前言的 3 次为下限纪律，超出即记 UNPROVEN）；查不到写 UNPROVEN ＋走过的途径。
- 结论行格式：`| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |`
- 成节顺序按**取证完成先后**（非序号序）；检索以每条结论行的"序号"为准。`254` 行含两件（Bosch ＋ Morganson），拆为 `254a/254b` 分判。

## 188 · DOI 10.1051/0004-6361/202141249（XP 内定标）—— 核验态：已核

断言原文（`docs/science/PHOTOMETRY.md:311` §14a 第 7 条）："**XP 外定标（仪器响应模型与定标精度）**：… Carrasco, J. M., et al. 2021, A&A 652, A86（DOI 10.1051/0004-6361/202141249，XP 内定标）"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 188 | 10.1051/0004-6361/202141249 | 已核 | 版本对：A&A 出版版 2021, **652** A86（Crossref 记录卷页/年份与仓引字字相符；引用为文章级，未涉式号/表号，无需再钉载体） | 关联对：Crossref 题名 *"Internal calibration of Gaia BP/RP low-resolution spectra"*，BP/RP 即 XP，"内定标"三字与题名同义；作者串首作者 J. M. Carrasco 与仓写 "Carrasco, J. M., et al." 一致 | api.crossref.org/works/10.1051/0004-6361/202141249 → A&L 容器、vol 652、page A86、2021、9 作者 | — |

**注（非缺陷，登记为口径观察）**：该条目挂在"**XP 外定标**"的粗体小标题之下，但自身已就地标注"XP 内定标"，题名核对表明内定标才是对的——即该条把与前两条（Montegriffo A3 / De Angeli A2 外定标）**不同性质**的一篇放进了同组，且已在括号内自我划界。属分组标题与条目性质不完全一致，不是错引。

## 194 · DOI 10.1051/0004-6361:20021326（Greisen & Calabretta Paper I）—— 核验态：已核

断言原文：`docs/science/ASTROMETRY.md:257`（文章级定位声明）＋ `:265`（式号级定位）："Greisen, E. W. & Calabretta, M. R. 2002, A&A 395, 1061（Paper I；DOI 10.1051/0004-6361:20021326，arXiv:astro-ph/0207407 逐字核验）§2.1.1 式(1) q_i=Σ_j m_ij(p_j−r_j)（r_j=CRPIX_j）与 §2.1.4（整数像素号=像素中心，首像素 0.5→1.5）"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 194 | 10.1051/0004-6361:20021326 | 已核 | 版本对，且**载体已钉死**：Colon 式 DOI 为 A&A 2002 旧式登记，Crossref 回包即 *"Representations of world coordinates in FITS"* A&A **395**, 1061 (2002)；仓内 `:265` 的 §2.1.1／式(1)／§2.1.4 三处指认在**A&A 出版版全文 HTML**（`aanda.org/articles/aa/full/2002/45/aah3859/`）逐条命中 ⇒ 式号不依赖预印本编号，无 Paper II 那类"换载体式号漂移"暴露面 | 关联对：出版版 §2.1.1 标题 "Basic formalism"，其式(1) 逐字为 `q_i = Σ_{j=1}^{N} m_{ij}(p_j − r_j)`；§2.1.4 "Additional points" 逐字含 "integer pixel numbers refer to the center of the pixel in each axis" 与 "the first pixel runs from pixel number 0.5 to pixel number 1.5 on every axis" —— 与仓内 §5a 的 1-based/像素中心桥接口径完全同义 | Crossref（题名/卷页/年）＋ A&A 全文 HTML 两次定点取文（式 (1) 与 §2.1.4 原文） | — |

**残留小瑕（P3 级，不算引用缺陷）**：`:265` 把逐字核验的**载体**写成 `arXiv:astro-ph/0207407`，而同一行给出的 DOI 指期刊版。官方 arXiv API 回包：`astro-ph/0207407` 题名 *"Representations of world coordinates in FITS"*、作者 Eric W. Greisen & Mark R. Calabretta、**最新 v2**、`journal_ref = "Astron.Astrophys. 395 (2002) 1061-1076"`、`DOI = 10.1051/0004-6361:20021326` ⇒ **两标识符确指同一篇**、且 arXiv 页自证其期刊版就是被引卷页，故"预印本登记 + 期刊 DOI"在本行不构成矛盾（页码 1061-**1076** 与仓写起页一致）。风险面仍在：仓内式号如改按预印本取，可能与期刊版编号不同（Paper II 已出过此类 P0）。
## 200 · DOI 10.1080/01621459.1993.10476408（MAD 一致性因子）—— 核验态：已核（内容级半档降级）

断言原文（`docs/algorithms/PHASE2_SAMPLER.md:214-221`）："该常数 = 1/Φ⁻¹(3/4) …… 仅在 **f 为高斯** 时相合。出处：Rousseeuw, P. J. & Croux, C. 1993, JASA 88(424), 1273-1283, DOI 10.1080/01621459.1993.10476408（MAD 的一致性因子与有限样本修正）；有限样本修正 b_n 表见 Croux & Rousseeuw 1992, *Computational Statistics*, 411-428"。同件另挂 `docs/science/PHOTOMETRY.md:304`（"稳健性讨论见 Rousseeuw & Croux 1993"）。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 200 | 10.1080/01621459.1993.10476408 | 已核 | 版本对：Crossref 回包 *"Alternatives to the Median Absolute Deviation"*, JASA **88**(424) 1273–1283, **1993** — 与仓引卷/期/起讫页/年**逐项相符**（含期号 424 这种易漏项）；引用为文章级，无式号/表号，无载体漂移面 | 关联对（文章级）：该文题名即以 MAD 为主题对象，被"稳健尺度估计"断言引用不属角色错绑。**内容级降级**：断言把"一致性因子"与"有限样本修正"两件事并列归给 1993，本轮未取到正文（T&F 付费墙，ADS 需 token）证不了 1993 是否给出 MAD 的有限样本修正；而 1/Φ⁻¹(3/4) 这一**恒等式本身**是数学事实、不需该文为据（仓内 `PHOTOMETRY.md:296` 亦已自认"教科书级恒等式，Project-defined 采纳"） | api.crossref.org/works/10.1080/01621459.1993.10476408（一次 429 后重发成功）；tandfonline 正文／ADS ⇒ 未得 |

## 206 · DOI 10.1086/131801（Horne 1986）—— 核验态：已核

断言原文（`实验/absolute-snr/REPORT_paper.md:44` §1.3）："Horne 1986 DOI 10.1086/131801、Zackay & Ofek 2017 DOI 10.3847/1538-4357/836/2/187 与 arXiv:1512.06872/1512.06879 **均核验可解析**"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 206 | 10.1086/131801 | 已核 | 版本对：Crossref ⇒ K. Horne, *"An optimal extraction algorithm for CCD spectroscopy"*, PASP **98**, 609, **1986**（仓写 "Horne, K. 1986, PASP 98, 609"，见 `docs/science/PHOTOMETRY.md:305`）；文章级，无式号 | 关联对（就该位点那句而言）：被挂的断言只是"DOI 可解析"这一存在性声明，Crossref 记录直接满足它，不涉内容转述 | api.crossref.org/works/10.1086/131801 | — |

**范围提示（不判红，登记给后续内容路）**：该文自述域是 **CCD 光谱**的最优提取。同一 DOI 在 `docs/science/PHOTOMETRY.md:305` 被用来支撑"**最优提取（PᵀC⁻¹P 结构）**"这一**结构级**断言（并自带边界句"二者为已知 profile/方差下的最优提取……引用只作统计结构对照"）。PᵀC⁻¹P 的矩阵写法是否出自 Horne 1986 本文，本轮无正文途径（JSTOR/PASP 付费、ADS 405）⇒ 属"证据层级不匹配"风险面，需正文路核（见文末"不敢判的"）。

## 212 · DOI 10.1086/133014（Kjeldsen & Frandsen 1992）—— 核验态：已核

断言原文（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:223`）："- [B85] Kjeldsen, H. & Frandsen, S. 1992, PASP 104, 413, DOI 10.1086/133014 [S]"（"分场景定量补充"节）。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 212 | 10.1086/133014 | 已核 | 版本对：Crossref ⇒ *"High-precision time-resolved CCD photometry"*, H. Kjeldsen & S. Frandsen, PASP **104**, 413, **1992** — 作者/卷/页/年与仓引全符；文章级无式号 | **无断言可绑（列而不用）**：`git grep -n "B85"` 与 `git grep -n Kjeldsen` 全仓各只 1 命中，即本清单行自身；正文从未挂靠，故"关联对/错"无对象可判。行尾 `[S]` 标签按本仓已定口径不作已核凭据 | api.crossref.org/works/10.1086/133014 ＋ 仓内 `git grep`（两次读取，命中数=1） | — |

**P3（登记）**：`[B85]` 属已判出的"列而不用"形态在本批的新实例——清单计数把它算作"命中 1 次"，但它**从未被用作证据**；同族 `[B74]–[B86]` 一片都在"分场景定量补充"里只列不挂。

## 218 · DOI 10.1086/301513（York et al. 2000 SDSS）—— 核验态：已核（内容级降级）

断言原文（`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:62` §3.6）："York et al. 2000 (SDSS), AJ 120, 1579. DOI 10.1086/301513 `[CR]` | 借鉴点：现代『先校准帧、再组合、携带逐像素不确定度』架构的历史起点 | 不借鉴点：drift-scan 专用几何与 2000 年代定标方式（已被 ubercal 取代）"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 218 | 10.1086/301513 | 已核 | 版本对：Crossref ⇒ *"The Sloan Digital Sky Survey: Technical Summary"*, D. G. York et al., **AJ 120**, 1579, **2000**（仓写 AJ 120, 1579 相符；容器是 AJ 而非 PASP，仓内未写错）；文章级无式号 | 关联对（就该断言的措辞档位而言）：引用被派的是"**历史起点**"这一史位角色，SDSS 技术总括担此角色不算错绑。**内容级降级**：该句里"携带逐像素不确定度"是**数据产品级**细节，本轮未取正文（AAS/IOP 墙），且同一档案 §3.7 已把"ivar/权重面成文"归给 Stoughton et al. 2002（AJ 123, 485）——即该细节的正主在仓内另有其件；把产品级细节挂在总括文上属已登记的"证据层级不匹配"形态的轻度实例 | api.crossref.org/works/10.1086/301513 |

## 224 · DOI 10.1086/323387（Everett & Howell 2001）—— 核验态：已核

断言原文（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:217`）："- [B79] Everett, M. E. & Howell, S. B. 2001, PASP 113, 1428, DOI 10.1086/323387 [S]"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 224 | 10.1086/323387 | 已核 | 版本对：Crossref ⇒ *"A Technique for Ultrahigh-Precision CCD Photometry"*, M. E. Everett & S. B. Howell, PASP **113**, 1428, **2001** — 作者/卷/页/年逐项相符；文章级无式号 | **无断言可绑（列而不用）**：与 `[B85]` 同形，本行是清单行本身，`[S]` 标签按本仓口径不作凭据；该行位于"分场景定量补充"列表，正文无挂靠 ⇒ 关联字段只能判"无对象"，不判对/错 | api.crossref.org/works/10.1086/323387 |

## 230 · DOI 10.1086/345491（Anderson & King 2003）—— 核验态：已核 ＋ 仓内自纠已双查证实

断言原文（`实验/absolute-snr/docs/surveys/f-instr-survey.md:169,175-178`）："[F-12] Anderson, J. & King, I. R. (2000)… PASP 112, 1360. DOI 10.1086/316632；**(2003) PASP 115, 113. DOI 10.1086/345491** `[CR]`"；核对状态："任务书候选『Anderson & King 2006, PASP 118, 560』**未核到** —— PASP 118, 560 实为 Branch et al. 2006（超新星光谱，DOI 10.1086/502778）…"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 230 | 10.1086/345491 | 已核 | 版本对：Crossref ⇒ *"An Improved Distortion Solution for the Hubble Space Telescope's WFPC2"*, Jay Anderson & Ivan R. King, PASP **115**, 113, **2003**（卷页年与仓写全符；作者串与缩写 I. R. 也对得上）；文章级无式号 | 关联对（有精度损失，登记 P3）：[F-12] 条目主体角色是"ePSF 拟合测光/天测、欠采样 PSF 建模"，2003 这一件的**实际主题是 WFPC2 几何畸变改正**，不是 ePSF；两篇并写而未分角色，读者会把"欠采样 PSF 建模"的功劳一并记到 2003 头上。畸变改正支撑"天测"半边尚可，但**建议就地括注各件角色**（本仓已有"同一行并列多件、各件角色不同须落到件"的口径） | api.crossref.org/works/10.1086/345491 ＋ api.crossref.org/works/10.1086/502778 | — |

**自纠双查结果（按派单硬条款）**：仓内自记"PASP 118, 560 不是 Anderson & King 2006，而是 Branch et al. 2006（超新星光谱，DOI 10.1086/502778）"——
Crossref 回包：10.1086/502778 = *"Comparative Direct Analysis of Type Ia Supernova Spectra. II. Maximum Light"*, David Branch, L. C. Dang, …, PASP **118**, 560, **2006** ⇒ 卷、页、年、题材四项全中。
两侧都查过：**替代件真、被否的候选件假**。**结论：这一条不是缺陷，是本仓自纠正确的例子**（与已登记的 DES DR2 `2101.02242→2101.05765` 正例同族，可作为"核对机制在跑对的方向"的证据）。
**同件旁注**：`[F-12]` 的另一标识符 **10.1086/316632（Anderson & King 2000, PASP 112, 1360）本批未查**（属它自己那一行），下一轮若判 [F-12] 整件需补此号。

## 236 · DOI 10.1086/444553（Sirianni et al. 2005 ACS）—— 核验态：已核（内容级 UNPROVEN）

断言原文：`实验/absolute-snr/docs/surveys/f-instr-survey.md:332` [F-24]"Sirianni, M. et al. (2005). The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys. PASP 117, 1049–1112. **DOI 10.1086/444553** `[CR]`… 明确区分**能量积分**与**光子计数**两种合成通量约定… **孔径改正（encircled energy / growth curve）**、CTE 改正、AB 零点"；同件另挂 `docs/science/PHOTOMETRY.md:313` 第 9 条"（DOI 10.1086/444553，端到端系统透过率 × 光谱的工程范例）"，该条标题是"**光子计数通带（`λ` 因子的文献依据）**"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 236 | 10.1086/444553 | 已核 | 版本对：Crossref ⇒ 题名逐字相符（含所有格写法差异不在判定面），PASP **117**, 1049（起页与仓写一致）, **2005**, 14 作者且首位 M. Sirianni ⇒ 仓写"Sirianni, M. et al."成立；引用为文章级，但**仓写在 f-instr-survey 处未给年份 2005 之外的版次区分**，无需 | 关联对（题名可证的那半）＋**内容级 UNPROVEN**（"能量积分 vs 光子计数"约定、`λ` 因子写法）：题名与"测光性能与定标"角色同域，孔径改正/CTE/AB 零点属其自然内容；但"`λ` 因子的文献依据"是**公式级**断言，本轮拿不到正文 ⇒ 不能按标题相关就判关联对 | api.crossref.org/works/10.1086/444553（元数据） | ①`iopscience.iop.org/article/1538-3873/117/836/1049` ⇒ 302 跳 RadWare 反爬验证页（Bot Manager），未得正文/摘要；②arXiv 官方 API `search_query=ti:"photometric performance and calibration … advanced camera for surveys"` ⇒ **NO_RESULTS**（无预印本可作替代载体）；③ADS 全文需 token，按派单不作途径。⇒ 缺口＝正文级取证（出版商权限或 ADS token） |

**格式旁注（非引用缺陷）**：清单里该行标识符带 `**`（`DOI 10.1086/444553**`）是仓内 Markdown 粗体起止符与行尾粘连，**不是**DOI 的一部分；抓取时按去 `**` 的规范键解析即命中。

## 248 · DOI 10.1093/biomet/36.3-4.458（Plackett 1949）—— 核验态：已核

断言原文（`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:38` §1.10）："Plackett 1949, Biometrika 36, 458. DOI 10.1093/biomet/36.3-4.458 `[CR]` | 最小二乘/最优组合史的简短同行评审记述，供审稿人追问逆方差加权出处时引用 | **不得作为最优性的证明（它是历史注记）**"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 248 | 10.1093/biomet/36.3-4.458 | 已核 | 版本对：Crossref ⇒ R. L. Plackett, *"A historical note on the method of least squares"*, Biometrika **36** 期 **3-4**, 458, **1949** — DOI 内卷/期/页三段与回包逐项自洽（本批少见"期号也钉住"的一条） | 关联对：仓内派的角色是"**历史注记**、且显式禁止当作最优性证明"，与题名 "A historical note on the method of least squares" **同义**——连自我限界都被题名证实；未涉该文节号/表号 ⇒ 无载体漂移面 | api.crossref.org/works/10.1093/biomet/36.3-4.458 | — |

## 260 · DOI 10.1109/TIT.2006.885507（Candès & Tao 2006）—— 核验态：已核

断言原文（`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:108` §6.7）："Candès & Tao 2006, IEEE Trans. Inf. Theory 52, 5406. DOI 10.1109/TIT.2006.885507 `[CR]` | RIP / 近最优恢复结果，使压缩感知保证对噪声与近似稀疏稳健；与 6.6 并列引用以陈述稀疏重建论证的**边界** | 同 6.6，随机投影假设"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 260 | 10.1109/TIT.2006.885507 | 已核 | 版本对：Crossref ⇒ *"Near-Optimal Signal Recovery From Random Projections: Universal Encoding Strategies?"*, Emmanuel J. Candes & Terence Tao, IEEE Trans. Inf. Theory **52**(12), 5406, **2006**；仓写"Candès"带音符 vs 回包 "Candes" 属转写差异不判错；文章级无式号 | 关联对：断言派的角色（**RIP**／近最优恢复／随机投影假设）与题名自述的 "Random Projections / Universal Encoding Strategies" 直接对应；仓内**只**用它立"边界"、与 6.6 并列，未派它做二维空间 GP 的活 ⇒ 无角色错绑 | api.crossref.org/works/10.1109/TIT.2006.885507 | — |

## 254 · DOI 10.1093/pasj/psx080 ＋ 同行 Morganson 件 —— 核验态：已核（两件分判）

断言原文（`实验/absolute-snr/docs/snr-propagation-design.md:1145`）："| 巡天管线架构 | Bosch et al. 2018, PASJ 70, S5, DOI 10.1093/pasj/psx080；Morganson et al. 2018, PASP 130, 074501, DOI 10.1088/1538-3873/aab4ef | detrending + coadd 一体架构；方差面一等产品 | LSST-DM 数据模型；DES 的「先定标后 coadd」次序 |"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 254a | 10.1093/pasj/psx080 | 已核 | **版次细节钉住**：Crossref ⇒ *"The Hyper Suprime-Cam software pipeline"*, Bosch, J.; Armstrong, R.; **Bickerton, S.**; Furusawa, H.; Ikeda, H. 等，PASJ **70**, 文章号 **S5**；`published-print = 2018`、`published-online = 2017-10-12` ⇒ 仓写 **2018** 取的是**印刷卷次年**，与只读 `published` 字段（2017，在线先发表）不同——本行的"年"因此**不是**漂移，而是两字段之别；引用未涉式号/表号，载体无需再钉 | 关联对：HSC 管线文担"detrending + coadd 一体架构"角色正对（该文即 HSC 管线总述） | api.crossref.org/works/10.1093/pasj/psx080（含 print/online 两日期二次取） | — |
| 254b | 10.1088/1538-3873/aab4ef（同行并列件） | 已核 | 版本对：Crossref ⇒ *"The Dark Energy Survey Image Processing Pipeline"*, E. Morganson 等，PASP **130**(989), **074501**；print 2018-07-01 / online 2018-05-17 ⇒ 仓写 "Morganson et al. 2018, PASP 130, 074501" 逐项相符 | 关联对：DES 成像管线文担"方差/权重面作为一等产品 + 先定标后 coadd 次序"角色，与该文的记述范围一致；仓内还把"DES 的先定标后 coadd"列入**不借鉴**，方向也未错绑 | api.crossref.org/works/10.1088/1538-3873/aab4ef | — |

**本件旁证（与已登记缺陷对得上）**：psx080 的作者串里第三位是 **`Bickerton, S.`**，而 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:176` 写的是 `Bickerton, J. W.`（已判为"首字母凭空来"）⇒ 本版次回包再次独立支持那条判红，并给出一条可回写的正确写法（Bickerton, S.）。

## 242 · DOI 10.1088/0004-6256/147/6/127（Bohlin 2014，非 "et al."）—— 核验态：已核，**作者数判红**

断言原文（`docs/science/PHOTOMETRY.md:307` §14a 第 4 条）："**Gaia XP 绝对分光刻度与 CALSPEC 溯源**：…Bohlin, R. C., Hubeny, I. & Rauch, T. 2020, AJ 160, 21（DOI 10.3847/1538-3881/ab94b4）；**Bohlin et al. 2014, AJ 147, 127（DOI 10.1088/0004-6256/147/6/127）**；Bessell, M. & Murphy, S. 2012, PASP 124, 140…"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 242 | 10.1088/0004-6256/147/6/127 | 已核 | 版本对：Crossref ⇒ *"Hubble Space Telescope CALSPEC Flux Standards: Sirius (and Vega)"*, The Astronomical Journal **147**, 127, **2014**（卷/页/年与仓写符）；**但该记录 `author[]` 只有一条：`R. C. Bohlin`（given name `Ralph`）＝单作者**。arXiv 官方 API 独立第二证：`1403.6861v1` 题名 *"HST CALSPEC Flux Standards: Sirius (and Vega)"*，**Full Author List = R. C. Bohlin 一人**，v1，无 journal_ref ⇒ 预印本与期刊版**同一作者数**，不存在"预印本多作者"的解释余地 | 关联对（角色层）：仓内派它的角色是"**CALSPEC 溯源**"，题名即 CALSPEC Flux Standards，正对；同条并写的 Bohlin+2020（3 作者）角色为另一件 ⇒ 各件角色不误。**唯作者写法错**：见下方 D1 | api.crossref.org/works/10.1088/0004-6256/147/6/127（两次：元数据＋作者数组原文）＋ arXiv 官方 API `search_query=ti:"CALSPEC flux standards"` |

**D1（本批唯一判红的元数据错）**：`docs/science/PHOTOMETRY.md:307` 的 "Bohlin et al. 2014" 应为 "**Bohlin, R. C. 2014**"（单作者）。
证据两侧一致：Crossref `author` 数组长度 1；arXiv:1403.6861v1 作者串 1 人。
形态定位：与已登记的"Bickerton, J. W. 首字母凭空来""Huang, Y. 实为 Bowen Huang"同族，但**方向相反**——不是缩写错，而是把单作者文用 `et al.` 扩写成多作者；最可能的成因是同一行紧邻的 Bohlin, Hubeny & Rauch 2020 为三作者，被顺手同化。
**可机械防**：凡写作 `X et al.` 的条目，比对 Crossref `author[]` 长度是否 ≥2 ⇒ 本批即由该一条暴露。

## 266 · DOI 10.1214/aoms/1177703732（Huber 1964）—— 核验态：已核（数值级降级）

断言原文（`docs/algorithms/PLATESOLVE.md:193-197`）："Huber 1.345：**有文献依据**——Huber (1964) `ψ_k` 族在 `k = 1.345` 处对 Gaussian 的渐近效率为 **95%**（Huber, P. J. 1964, Ann. Math. Statist. 35, 73, DOI 10.1214/aoms/1177703732；效率表见 Holland & Welsch 1977, Comm. Statist. Theor. Meth. 6, 813, DOI 10.1080/03610927708827533 §2）"。

| 序号 | 标识符 | 存在性 | 版次/载体 | 关联 | 依据 | 若 UNPROVEN：走过的途径 |
|---|---|---|---|---|---|---|
| 266 | 10.1214/aoms/1177703732 | 已核 | 版本对：Crossref ⇒ Peter J. Huber, *"Robust Estimation of a Location Parameter"*, Ann. Math. Statist. **35**(1), 73, **1964**（期号 1 也在册）；载体 = IMS/Project Euclid 记录（回包 `resource.primary.URL = projecteuclid.org/euclid.aoms/1177703732`，Crossref **无摘要字段**）；仓写卷/页/年全符 | 关联对（主题层）：`ψ_k` 族与"位置参数的稳健估计"正是本文主题，题名可直证；**"k=1.345 ⇒ 95%"这一数值对本文未取到正文**（Euclid 该文只有 PDF、无 HTML 全文，本批按派单只走 HTML ⇒ 记内容级 UNPROVEN）。减轻因素：仓内**已把"效率表"就地转指另一件** Holland & Welsch 1977（我核其元数据全对：Comm. Statist. Theory Meth. **6**(9), 813, 1977, *"Robust regression using iteratively reweighted least-squares"*），即数值表并未硬挂在 Huber 上；但其 `§2` 的**节号级**指认同样未取正文 ⇒ 属"式号/节号要钉版次＋载体"的未完成面 | api.crossref.org/works/10.1214/aoms/1177703732（元数据＋resource/link 字段两次）＋ projecteuclid.org 正文页 1 次（返回反爬错误页）＋ api.crossref.org/works/10.1080/03610927708827533（并列件） |

<!-- PROGRESS: 14/14 -->

## 本批三态计数

- **存在性**：`已核 14`／`不存在 0`／`UNPROVEN 0`（14 行 = 15 件，`254` 行两件均存在）。
- **版次与载体**：`版本对 14 行（15 件）`／`版本错 0`／`未钉版次 0`；其中 **式号级引用 1 件已钉到载体并命中**（`194` 的 §2.1.1 式(1)、§2.1.4 两条逐字，在 A&A 出版版 HTML 上证实），另有 **2 件节号/表级指认未取正文**（`266` 的 Huber 数值对与 H&W `§2`、`236` 的 `λ` 因子约定）⇒ 登记为内容级降级，不改判"版本错"。
- **关联**：`关联对 12 行`／`无断言可绑（列而不用）2 行（212、224）`／`关联错 0`／`角色错绑 0`。
- **元数据写法判红 1 件**：`242` 的作者数（`et al.` 用于单作者文）。

## 本批 P1 缺陷清单

| 编号 | 档 | 位置 | 缺陷 | 修法（不代拟全文） |
|---|---|---|---|---|
| D1 | 判红·元数据 | `docs/science/PHOTOMETRY.md:307` | "Bohlin et al. 2014, AJ 147, 127"——该文**单作者**（Crossref `author[]` 长度 1 ＋ arXiv:1403.6861v1 同证） | 改 `Bohlin, R. C. 2014`；台账加"`X et al.` ⇒ 作者数组须 ≥2"的机械门 |
| D2 | 判红·引用有效性 | `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:217,223`（[B79]、[B85]） | **"列而不用"**：全仓 `git grep` 各只 1 命中且命中即清单行本身，正文从不挂靠 ⇒ 批次清单"命中次数 1"是虚高，[S] 标签不自证已核 | 台账区分"列出"与"用过"；或把它们挂到具体断言句上 |
| D3 | 偏差·证据层级 | `实验/absolute-snr/docs/surveys/f-instr-survey.md:332` ＋ `docs/science/PHOTOMETRY.md:313` 第 9 条（Sirianni 2005） | 用一篇**无预印本、正文在本环境不可达**的 2005 PASP，去支撑"**λ 因子的文献依据**"这一**公式级**断言；元数据全对，但公式级背书缺正文证据 | 就地标注"正文未核（仅元数据核）"，或改由可达载体（官方手册/有 HTML 全文的后续文）承担公式级依据 |
| D4 | 精度损失 | `实验/absolute-snr/docs/surveys/f-instr-survey.md:169`（[F-12]） | 2000（ePSF）与 2003（**WFPC2 畸变改正**）两件并写、未分角色，条目主体讲的是"欠采样 PSF 建模" | 各件后括注自身主题；本仓已有"并引多件要落到件"的口径 |
| D5 | 精度损失 | `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:62`（York 2000） | 数据产品级细节"携带逐像素不确定度"挂在**巡天总括**文上；同档案 §3.7 已把该产品细节正主指给 Stoughton et al. 2002 | 把该半句改挂正件，或在 York 句里去产品级措辞（"历史起点"的史位角色本身可保留） |
| D6 | 风险面登记 | `docs/science/ASTROMETRY.md:265`（Greisen & Calabretta） | 载体混写：逐字核验记在 `arXiv:astro-ph/0207407`，标识符给期刊 DOI。本轮证实两标识符**确指同一篇**（arXiv 回包 journal_ref = A&A 395, 1061-1076 ＝ 同一 DOI），且式号/节号在**期刊版 HTML** 上命中 ⇒ **当前不是缺陷**；风险在"若有人改按预印本取式号"（Paper II 已因此出过 P0） | 把"逐字核验"的载体统一写成期刊版＋卷页，预印本号降为附注 |
| N1 | **正例·非缺陷** | `实验/absolute-snr/docs/surveys/f-instr-survey.md:176-178` | 仓内自纠"PASP 118, 560 实为 Branch et al. 2006（超新星光谱，DOI 10.1086/502778）"——**双查两侧均证实**：345491 确为 Anderson & King 2003 PASP 115, 113；502778 确为 Branch et al. 2006 PASP 118, 560 *"Comparative Direct Analysis of Type Ia Supernova Spectra. II. Maximum Light"* | **不是缺陷，是自纠正确的例子**（与 DES DR2 `2101.02242→2101.05765` 同族）。保留原登记不动 |
| N2 | **正例·非缺陷** | `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:38`（Plackett 1949） | 仓内给该件的自我限界（"历史注记、不得当最优性证明"）被题名 *"A historical note on the method of least squares"* 直接证实 | 正面样本：角色＋限界双对 |
| N3 | **正例·非缺陷** | `snr-propagation-design.md:1145` / `REVERSE_VERIFY_BIBLIOGRAPHY.md:59`（Bosch 2018） | Crossref `published` 回 **2017**（online 2017-10-12）而 `published-print` = **2018**；仓写 2018 取印刷卷次年，**正确** | 不改条文；但见"新形态" B——台账取年字段必须固定用 `published-print`，否则会把正确条目判成假红 |

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

| 件 | 未证的到底是 | 用过的标识符／途径 | 下一步可换的路 |
|---|---|---|---|
| 236 Sirianni+2005 | "正文里明确区分能量积分 vs 光子计数"、`λ` 因子写法 | `10.1086/444553`（Crossref ✓元数据）；IOP 摘要页 ⇒ 302 RadWare Bot Manager；arXiv 官方 API 题名检索 ⇒ NO_RESULTS（无预印本） | ADS token 取 facsimile／出版社权限／馆际；或改由有 HTML 全文的后续综述承担该约定 |
| 266 Huber+1964 | "k=1.345 处 95% 渐近效率"是否**写在本文** | `10.1214/aoms/1177703732`（Crossref ✓＋resource 指向 Euclid；无摘要字段）；`projecteuclid.org/euclid.aoms/1177703732` ⇒ 反爬错误页；Euclid 该文仅 PDF（本批禁 PDF） | PDF→HTML 转换途径（需负责人放行"正文走 HTML"之外的取法）；或把该数值改钉到可达载体（如 Huber & Ronchetti 2009 书，仓内已登记其 ISBN） |
| 266 并列件 H&W 1977 | "`§2` 有效率表"的节号指认 | `10.1080/03610927708827533`（Crossref ✓：6(9) 813, 1977, 题名相符）；T&F 正文墙 | 同上；或引该书/手册级载体 |
| 200 R&C 1993 | 1993 文是否给 **MAD 的**有限样本修正（仓内括注把"一致性因子与有限样本修正"并列归给 1993） | `10.1080/01621459.1993.10476408`（Crossref ✓ 88(424) 1273-1283, 1993）；T&F 墙 | 需正文；b_n 表的正主仓内另指 Croux & Rousseeuw 1992（本批未查 `10.1007/978-3-662-26811-7_58`） |
| 206 Horne 1986 | "最优提取的 **PᵀC⁻¹P 结构**"是否出自本文（另一挂点 `PHOTOMETRY.md:305` 的内容级断言） | `10.1086/131801`（Crossref ✓ 题名 "…for CCD spectroscopy"）；PASP/JSTOR 墙；ADS 需 token | 正文途径；本批挂点只声明"DOI 可解析"，故未判红 |
| 218 York 2000 | 总括文内是否有"逐像素不确定度"字样 | `10.1086/301513`（Crossref ✓ AJ 120 1579, 2000） | 需正文（同 D5） |
| 230 旁件 | `10.1086/316632`（Anderson & King 2000, PASP 112, 1360）本批未查 | — | 下一轮补一号 Crossref 即可判 [F-12] 整件 |

## 不敢判的与缺什么才能判

1. **凡"公式级/数值级/节号级"断言（236 的 `λ` 因子、266 的 1.345 与 H&W `§2`、200 的 MAD b_n、206 的 PᵀC⁻¹P、218 的逐像素不确定度）本批一律不判对**——缺的是**正文取证权限**：ADS token、或 AAS/IOP 与 T&F 的订阅、或放行 PDF→HTML 这一条途径（派单 §3 规定"正文走 HTML 途径"，而 IMS/Euclid 与 AAS 旧刊的正文**只有 PDF**，IOP 页面在本环境一律 302 到反爬）。这三者任一到位，上面 6 项一轮内可全部收口。
2. **`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` 的 [V]/[S] 标签**：本批 `[B79]/[B85]` 两件的标签含义（"以同样方式核对"）与该文件 87 个标签中 37 个"只出现一次"的实测一起，说明标签**不自证**；但"该档案整体元数据是否被逐一核过"属整档重跑，不是一条批次能判的——需要**台账级重跑**（该文件已登记过两处作者伪值，D1 是第三处）。
3. **"列而不用"判档的边界**：`212/224` 我判"无断言可绑"而不是"关联错"——因为它们**没有**被挂到任何断言句上；若要按"关联错"计，需先定"清单行本身是否构成断言"的口径（本仓现有口径未见成文）。缺的就这一句口径。
4. **年份字段取法**：`psx080`（print 2018 / online 2017）证明 Crossref 的 `published` 与 `published-print` 会把同一篇的年分开——若台账取错字段，会把仓内**正确**写法判红。本批未见成文规定取哪个字段，建议由前台定一条（我按 print 卷次年判，与仓内一致）。

## 覆盖率自报

- 分配 14 条 ⇒ **已核 14/14 = 100%**（无一条留在未核态；无一条 `存在性 UNPROVEN`）。
- 三字段齐备率：存在性 14/14；版次与载体 14/14（其中 1 件含 print/online 双日期说明）；关联 14/14（12 判档 + 2 判"无断言可绑"）。
- 内容级（正文）取证成功 **1 件**（`194` 的 A&A 出版版式号/节号命中），受阻 6 件 ⇒ 已逐件登记途径与缺口。
- 网络查询用量：单条最高 4 次（`266`），全部 ≤6 上限；`429`（Crossref 并发）出现 2 次，均按派单"网关/限流不占配额"重发成功，未影响结论。

## 新发现的缺陷形态（本批独有、前面没登记过）

- **A｜`et al.` 把单作者文扩写成多作者**（D1，`Bohlin et al. 2014`）。与已登记的"首字母凭空来"（`Bickerton, J. W.`、`Huang, Y.`）同族但**方向相反**：卷/页/年/DOI 四项全对，只有作者数错；成因线索是同段紧邻的三作者姊妹文被同化。**可机械判**：`X et al.` 者比对 Crossref `author[]` 长度 ≥2（本批即双源证实：Crossref 与 arXiv v1 一致）。
- **B｜同一 DOI 记录里 print 年 ≠ online 年，会把正确条目判成假红**（N3，`psx080` 2018 vs 2017-10-12）。这是**台账侧**缺陷形态而非仓内缺陷：判"年份漂移"前必须固定取 `published-print`（卷次年），否则核验机制自身造红。
- **C｜载体天然不可达的一档应显式登记**：AAS/IOP 系旧刊（PASP/AJ 2005 及更早）正文在本环境**恒不可达**（Bot Manager／仅 PDF／ADS 需 token），且**无预印本**（arXiv 题名检索 NO_RESULTS 已实测）⇒ 这类件**只能**核到元数据层。台账需要一个第三态"仅元数据可核"，否则会持续出现两种偏差：要么把公式级背书默认为已核（偏松），要么整件判 UNPROVEN（偏严，把已证的存在性也丢了）。
