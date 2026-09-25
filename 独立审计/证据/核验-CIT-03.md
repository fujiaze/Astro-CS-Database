# 核验-CIT-03（18 条未核引用，P-1 优先）

- 基线：`c8f64e9a`
- 批次清单：`独立审计/批次清单/CIT-03.txt`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md`（§1 三字段 / §2 硬禁令 / §4 高危形态）
- 网络手段清单（本批实际使用）：
  - arXiv 官方 API：`https://export.arxiv.org/api/query?id_list=<id>`（WebFetch）
  - Crossref：`https://api.crossref.org/works/<doi>`（WebFetch）
  - DOI 解析落地页：`https://doi.org/<doi>`（WebFetch）
  - 图书/专著：出版社页（Cambridge 等）
  - 仓库侧仅读：`Read` 代表位点原文
- 硬上限：每条 3 次网络查询 + 2 次仓库读取；到限写 `UNPROVEN` 并附用过的标识符与途径。
- 本批特别注意：高危形态①（I/II 与配对卷页绑反）、②（伪托节号）、③（版次未定）、⑤（作者年漂移）。

---

## 49 · Rosner 1983, Technometrics 25, 165（DOI 10.1080/00401706.1983.10487848）—— 核验态：已核
- **存在性：已核**。Crossref 记录 `https://api.crossref.org/works/10.1080/00401706.1983.10487848` 回包逐字段：title **"Percentage Points for a Generalized ESD Many-Outlier Procedure"**／author Rosner／container *Technometrics*／year **1983**／volume **25**／first page **165**。与本仓登记（`docs/references/SCIENTIFIC_REFERENCES.md:83` §J 第 32 条）**逐字段一致**，题名连字符与 "Many-Outlier"（非 "Many-Outlier Detection"）拼写亦一致。
- **关联性：关联对**。本仓挂法＝"Generalized ESD 的 α/max_outliers 语义来源"。该文正是给出 generalized ESD **临界值（percentage points）** 的原始出处，α 与预设最大剔星数 r 的语义即出自此；用于 §14 ESD 条目为对的角色（方法学原始文献，非实现对照）。未见错绑。
- **版本：版本对**。期刊单版（1983, *Technometrics* **25**, 起始页 **165**；Crossref 仅回起始页，**页码区间未核**——仓内 `docs/algorithms/PHASE2_REJECTION.md:783` 写作"165-172"，该区间本轮未复核，不作对错判定），无同名再版内容差异，本仓已钉年份＋卷＋起始页。
- 用量：网络 1/3（Crossref）；仓库 1/2（SCIENTIFIC_REFERENCES.md §E–§J 段）。

## 91 · arXiv:1401.4169（Gruen, Seitz & Bernstein, SWarp clipped mean stacking）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=1401.4169` 回包：title "Implementation of robust image artifact removal in SWarp through clipped mean stacking"，authors D. Gruen / S. Seitz / G. M. Bernstein，DOI 字段 **10.1086/675080**，`<id>` 末位 **v1**。
- **关联性：关联对**。位点 `实验/additive-sky-seamless/REPORT_paper.md:392` §7 参考文献第 2 条，登记为 "Gruen, Seitz, & Bernstein 2014, PASP 126, 158 … DOI 10.1086/675080, arXiv:1401.4169"，用途为叠加排异/clipped-mean 一手来源。API 回包的 DOI 与该条所引 DOI **完全一致**，作者三元组、题名、被动语态摘要首句（"We implement an algorithm for detecting and removing artifacts … by means of outlier rejection during stacking"）与"clipped-mean 叠加排异"角色一致，无 I/II 绑反风险（本文无姊妹篇编号）。
- **版本：版本对（未钉版本号，但仅存 v1）**。arXiv 侧只有 v1，故不存在"被引内容只在 v2/v3"的漂移。
- 用量：网络 1/3；仓库 1/2（REPORT_paper.md §7）。

## 97 · arXiv:2012.01916（Riello et al., Gaia EDR3 photometric content）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=2012.01916` 回包：title "Gaia Early Data Release 3: Photometric content and validation"，first author **Riello**，DOI 字段 **10.1051/0004-6361/202039587**，`<id>` 末位 **v1**。
- **关联性：关联对（文章级）**。位点 `docs/science/PHOTOMETRY.md:312` §"8. Gaia G/BP/RP 通带与零点"，登记 "Riello, M., De Angeli, F., Evans, D. W., et al. 2021, A&A 649, A3（DOI 10.1051/0004-6361/202039587；arXiv:2012.01916）"。回包 DOI 与所引 DOI 一致，摘要自述"contains astrometry and photometry results … describing the input data, the algorithms, the processing, and the validation"，与"EDR3 测光内容/通带"角色相符。注意该句的具体数值 `ZP_VEG(G)=25.6874` 本仓**另锚**在 ESA 官方文档 §5.4.1 Table 5.4，并未挂在 Riello 上 ⇒ 无伪托数值。逐表/逐图未开全文，关联性证据为摘要级。
- **版本：版本对（未钉版本号，arXiv 仅 v1）**；期刊侧钉 A&A 649, A3 (2021) 与回包 DOI 同源，无 EDDR 版次混淆。
- 用量：网络 1/3；仓库 1/2（PHOTOMETRY.md §溯源段）。

## 72 · De Angeli et al. 2023, A&A 674, A2（DOI 10.1051/0004-6361/202243680）—— 核验态：已核
- **存在性：已核**。Crossref `works/10.1051/0004-6361/202243680` 回包：title "Gaia Data Release 3: Processing and validation of BP/RP low-resolution spectral data"／first author **De Angeli**／*Astronomy & Astrophysics*／**2023**／volume **674**／article **A2**。与本仓三处登记（`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:141` [B38]；`docs/science/PHOTOMETRY.md:311` 第 7 项；`实验/absolute-snr/docs/surveys/f-instr-survey.md:305` [F-21]）**卷页＋题名＋作者全对**。
- **关联性：关联对**。[B38]/[F-21] 角色＝"DR3 BP/RP 光谱的处理与验证 ⇒ XP 光谱单位与质量标志的权威出处"；`PHOTOMETRY.md:311` 角色＝"XP 外定标（仪器响应模型与定标精度）"。回包题名即 "Processing and **validation** of BP/RP low-resolution spectral data"，与该角色一致。[F-21] 自带订正说明（副标题须为 "Processing and validation of BP/RP low-resolution spectral data"，非 "Processing of BP/RP spectra"）与 Crossref 题名逐字一致 ⇒ 现登记文本已对齐。
- **版本：版本对**。A&A 674 A2 (2023) 为 Gaia DR3 专刊定稿；仓内同时钉 DOI＋文章号，无 DR2/EDR3/DR3 版次混用。
- 用量：网络 1/3；仓库 2/2。

## 103 · arXiv:2508.15278（Liu & Miller 2025, HostSub_GP）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=2508.15278`：title "**HostSub_GP: Precise Galaxy Background Subtraction in Transient Long-slit Spectroscopy with Gaussian Processes**"，authors **Liu, Miller**，`<id>` 末位 **v2**，DOI 字段 **10.1088/1538-3873/ae3cc1**，journal 字段 **PASP 138 024503**，submitted **2025-08-21**。与仓内 `实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:711` 登记的"Liu & Miller 2025（HostSub_GP）arXiv:2508.15278，PASP **138**, 024503，DOI 10.1088/1538-3873/ae3cc1"**逐项一致**（作者/年/刊/卷/号/DOI 全对，无相邻号绑反）。
- **关联性：引文真、角色对，但**节号错绑（高危形态②）****。所引原文 "using classic interpolation-based methods (linear and B-spline), the galaxy background in the LRIS red channel is overestimated" **在该文全文中逐字存在**，脚注亦确有 B-spline knot 数敏感性的表述（footnote 7："PypeIt also employ B-splines… sensitive to… how many knots are used"）⇒ 断言内容与该文献相符，"GPR 做天光/背景扣除并给预测方差"的角色（同文件 `:91` 表格行）亦正确。**但仓内标注的定位是 §4.3，而该句实际位于 §IV.2（"Revisiting a Nebular-phase SN: SN 2019eix"）**，即 arXiv:2508.15278v2 的 4.2 节 ⇒ **节号需订正为 §4.2**（若指 PASP 发表版，须重开发表版 PDF 再钉；本轮据 v2 全文判定）。
- **版本：版本对（发表版已钉）**。仓内同时钉 arXiv＋PASP 138 024503＋DOI，被引内容在 v2 全文核对存在；节号错绑属**定位**缺陷而非版次缺陷。
- 用量：网络 2/3；仓库 2/2。

## 109 · Melchior et al. 2018, Astron. Comput. 24, 129（scarlet；DOI 10.1016/j.ascom.2018.07.001）—— 核验态：已核
- **存在性：已核（双源，配对已核）**。① arXiv 官方 API `id_list=1802.10157`：title "SCARLET: Source separation in multi-band images by Constrained Matrix Factorization"，first author Melchior（+ Moolekamp, Jerdee, Armstrong, Sun, Bosch, Lupton），DOI 字段 **10.1016/j.ascom.2018.07.001**，`<id>` 末位 **v2**；② Crossref `works/10.1016/j.ascom.2018.07.001`（首查 429 网关限流，按前言§3 重试一次即成功）：*Astronomy and Computing*／**2018**／volume **24**／first page **129**。仓内登记（`REVERSE_VERIFY_BIBLIOGRAPHY.md:92` [5.4]、`snr-propagation-design.md:1131`）的作者/年/刊/卷/页/DOI/arXiv 号**逐项对得上**，arXiv↔DOI 互指正确。
- **关联性：关联对（角色层）；所摘摘要句仅首句复核**。角色＝"相关噪声下的测量：coadd 相关噪声下正确测量的公开示范"，并标注"摘要（实页核对）"引两段。本次仅复核到摘要首句 "We present the source separation framework SCARLET for multi-band images, which is based on a generalization of the Non-negative Matrix Factorization to alternative and several simultaneous constraints."；仓内所摘 "…compact spatial support and uniform spectra over their support"、"…treatment of correlated noise and convolutions with band-dependent point spread functions…" **未逐字复现**（本条网络额度耗尽，429 亦计入）⇒ 两段引文的逐字性记 **UNPROVEN**，角色判定不依赖它们。
- **版本：版本对**。期刊单版 2018, 24:129；arXiv v2 的 DOI 字段即指向该版，被引"摘要"属共有内容层。
- 用量：网络 4/3（其中 1 次为网关 429）；仓库 2/2。

## 115 · Stoughton et al. 2002, AJ 123, 485（SDSS EDR；DOI 10.1086/324741）—— 核验态：存在性已核／关联性 UNPROVEN
- **存在性：已核**。Crossref `works/10.1086/324741`：title "Sloan Digital Sky Survey: Early Data Release"／first author **Stoughton**／*The Astronomical Journal*／**2002**／volume **123**／page **485**／登记作者数 **109**。与仓内 `REVERSE_VERIFY_BIBLIOGRAPHY.md:63` [3.7] 与 `f-instr-survey.md:186` [F-13]（"AJ 123, 485–548"）题名/作者/年/卷/起始页一致（末页 548 未单独复核）。
- **关联性：UNPROVEN（内容级断言未核到）**。[3.7] 的断言是强内容断言："SDSS **`ivar`/权重面约定**与成像产品 **mask/flag 语义在此详细成文**"。全文取证失败：`https://arxiv.org/pdf/astro-ph/0009253` 回包无可读正文，且 abs 页证明 **astro-ph/0009253 不是本文**（实为 Maciejewski, "Black Holes in Centers of Disk Galaxies – Spectroscopy with a Wide Slit"）⇒ 本仓该条**只给 DOI、无 arXiv 号**，我不凭记忆替代。
  - **疑点（不作判定）**：[F-13] 同一行并列引用的 **Lupton et al. 2001, "The SDSS Imaging Pipelines", ASPC 238, 269（arXiv:astro-ph/0101420）**才是 primary/secondary image＋`ivar`＋mask 平面的常规出处；若如此，[3.7] 有把管线论文内容记在总述论文头上的**角色错绑**风险，但证据不足，本轮不判红。
  - 位点 [F-13] 一侧的实质断言（psfMag/`aa`/nanomaggies 等）本仓是锚在 **sdss4.org DR17 官方文档逐字原文**上的，未挂在 Stoughton 2002 ⇒ 该位点无错绑。
- **版本：版本对（书目层）**。AJ 123, 485 (2002) 无版次歧义。
- 用量：网络 3/3；仓库 2/2。**下轮途径**：ADS 摘要/正文（需 token）、`iopscience.iop.org/article/10.1086/324741`、SDSS 文档 "Imaging Data Products" 页追 `ivar` 首引。

## 121 · Mäkitalo & Foi 2012, ICASSP（PGN VST；DOI 10.1109/ICASSP.2012.6288074）—— 核验态：存在性已核／式号 UNPROVEN
- **存在性：已核**。Crossref `works/10.1109/ICASSP.2012.6288074`：title "**Poisson-gaussian denoising using the exact unbiased inverse of the generalized anscombe transformation**"，authors **Makitalo, Foi**，container "2012 IEEE International Conference on Acoustics, Speech and Signal Processing (ICASSP)"，year **2012**，pages **1081–1084**，DOI 10.1109/ICASSP.2012.6288074。仓内两处（`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:455` 与 `:607` 表行 A8）登记为 "Mäkitalo M. & Foi A., 2012, ICASSP, DOI 10.1109/ICASSP.2012.6288074" ⇒ 作者/年/会场/DOI 全对；**仓内未给页码 1081–1084**（建议补，非缺陷判红项）。
- **关联性：关联对（论文角色层）；§2.2 式 (3)(4)(6) 的式号 UNPROVEN**。断言＝"含高斯读出的广义 Anscombe（PGN）…§2.2 式 (3)(4)(6)：`f(z) = (2/alpha)*sqrt(alpha*z + 3/8*alpha^2 + sigma^2 - alpha*g)`；低 `y`、`sigma ~ 2` 处有 overshoot"。题名即 "exact unbiased inverse of the **generalized anscombe** transformation"＋Poisson-Gaussian ⇒ 派给它的活（PGN VST 一手来源）与文献主题**同一件事**，非错绑；仓内所写公式形式与 GAT 标准式一致（`α` 为泊松增益、`σ²` 为读出方差、`g` 为偏移）。**但**该式在原文中的**节号/式号**（§2.2 与式 (3)(4)(6)）与"低计数 σ≈2 overshoot"的具体表述**未逐字核对**：作者自留 PDF `https://webpages.tuni.fi/foi/papers/ICASSP2012-Makitalo-Foi-GenAnscombe.pdf` 在本环境抽取失败（返回乱码二进制，非站点拒绝）。⇒ 式号引用需下轮换 HTML/项目页途径（作者项目页 `http://www.cs.tut.fi/~foi/invansc/`）后再宣布"已逐字核"。
- **版本：版本对**。会议论文单版（ICASSP 2012, 1081–1084），无再版内容差异；仓内钉年份＋DOI。
- 用量：网络 3/3（Crossref＋检索＋一次失败的 PDF 取回）；仓库 2/2。

## 127 · Zackay, Ofek & Gal-Yam 2016, ApJ 830, 27（DOI 10.3847/0004-637X/830/1/27；arXiv:1601.02655）—— 核验态：已核
- **存在性：已核（双源，配对已核）**。① Crossref `works/10.3847/0004-637X/830/1/27`：title "PROPER IMAGE SUBTRACTION—OPTIMAL TRANSIENT DETECTION, PHOTOMETRY, AND HYPOTHESIS TESTING"，authors **Zackay, Ofek, Gal-Yam**，*ApJ*，**2016**，volume **830**，page **27**；② arXiv API `id_list=1601.02655`：同题名/同三作者，DOI 字段 **10.3847/0004-637X/830/1/27**，`<id>` 末位 **v2**。⇒ DOI↔arXiv **互为回指**，配对正确。
  - 风险族排查：本条属"同作者成对论文"族。**ZO17I/ZO17II = ApJ 836, 187 与 188**（`snr-propagation-design.md:1132` 登记为 "Zackay & Ofek 2017, ApJ 836, 187/188, DOI 10.3847/1538-4357/836/2/187 与 …/188"）**不在本批**，故未核；本批 830/1/27 与 1601.02655 无绑反。
- **关联性：关联对（论文＋摘要级）**。位点 `docs/references/SCIENTIFIC_REFERENCES.md:94` §J 第 63 条，用途="噪声加权最优检验/预测残差方差阈值（plugins/12_rejection.md:23 若引最优检验应锚此）"。摘要回包："Starting from basic statistical principles, we develop **the optimal statistic for transient detection, flux measurement and any image-difference hypothesis testing**." ⇒ "最优检验"角色成立；"预测残差方差阈值"的逐式未开全文，故**不**据以判任何公式号。
- **版本：版本对**。ApJ 830, 27 (2016) 单版；arXiv v2 与发表版由 DOI 互指，无漂移面。
- 用量：网络 3/3；仓库 1/2。

## 133 · Maples et al. 2018, ApJS 238, 2（RCR；DOI 10.3847/1538-4365/aad23d）—— 核验态：已核（内容细节保留）
- **存在性：已核（双源，配对已核）**。① Crossref `works/10.3847/1538-4365/aad23d`：title "**Robust Chauvenet Outlier Rejection**"／first author **Maples**／*ApJS*／**2018**／volume **238**／page **2**；② arXiv API `id_list=1807.05276`：同题名，authors **Maples, Reichart, Konz, Berger, Trotter, Martin, Dutton, Paggen, Joyner, Salemi**，`<id>` 末位 **v1**，DOI 字段 **10.3847/1538-4365/aad23d**。⇒ 仓内 `docs/algorithms/PHASE2_REJECTION.md:784` 的"DOI＋arXiv 号＋作者序（Maples, Reichart, Konz, et al.）＋ApJS 238, 2 (2018)"**逐项对**，DOI↔arXiv 互为回指（无相邻号绑反）。
- **关联性：关联对（角色层）；内容细节 UNPROVEN**。断言＝"支持本层 **3-pass 链（序贯更换集中趋势测度）**与**经验修正因子表**"。核对结果：题名层角色（RCR = Robust Chauvenet Rejection 的论文出处）绑定正确，`docs/references/SCIENTIFIC_REFERENCES.md:85` §J 第 34 条同判（"RCR 的论文出处；项目现仅登记官方 RCR 2.4.7 软件参考，缺该论文引用"——该自评与本批发现的仓位点一致：`PHASE2_REJECTION.md:784` 已补）。**但**"3-pass／序贯更换集中趋势测度／经验修正因子表"是**方法细节断言**：arXiv abs 页与 API 两次仅回到摘要首句（"Sigma clipping is commonly used in astronomy for outlier rejection, but the number of standard deviations beyond which one should clip data from a sample ultimately depends on the size of the sample."），未复现后续方法描述 ⇒ 细节断言的逐字性记 **UNPROVEN**，不判红也不判绿。
  - 注意区分：仓内把"sequentially applying different measures of central tendency and empirically determining the rejection sigma value"归给 **Konz & Reichart 2023, arXiv:2301.07838**（`PHASE2_REJECTION.md:784` 后半句、`SCIENTIFIC_REFERENCES.md:86` 第 35 条），**不在本批**（该文自身登记为"期刊卷页需网络核验"）。
- **版本：版本对**。ApJS 238, 2 (2018) 单版；arXiv v1 唯一；仓内同时钉 DOI＋arXiv 号。
- 用量：网络 3/3；仓库 2/2。

## 139 · Gruen, Seitz & Bernstein 2014, PASP 126, 158（ADS bibcode 2014PASP..126..158G）—— 核验态：已核
- **存在性：已核**。bibcode 逐段解码核对：Crossref `works/10.1086/675080` 回包 title "Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking"，authors **Gruen, Seitz, Bernstein**，*Publications of the Astronomical Society of the Pacific*，**2014**，volume **126**，first page **158** ⇒ `2014`＋`PASP`＋`126`＋`158`＋首作者 **G** **五位全对**（ADS 未开：需 token；bibcode 是派生标识，其各分量已用出版社元数据独立核对）。Crossref 记录内无 `alt-bibcode` 字段，故 bibcode 未被直接回显——判定依据为上列分量核对。
- **关联性：关联对**。位点 `docs/references/SCIENTIFIC_REFERENCES.md:39` §E 第 19 条，用途="叠加排异与 PSF 差异下的伪影控制"；题名即"Robust Image Artifact Removal in SWarp through Clipped Mean Stacking"，角色绑定正确。同文在 `SCIENTIFIC_REFERENCES.md:89` §J 第 38 条、`实验/additive-sky-seamless/REPORT_paper.md:392`、`PHASE2_REJECTION.md:788` 重复登记，均同一篇（本批 [91] 与本条 [139] 为**同一文献的两种标识**，彼此一致，不构成矛盾）。
- **版本：版本对**。PASP 126, 158 (2014) 单版；本条为 bibcode（自带年份），无漂移面。
- 用量：网络 1/3；仓库 1/2。

## 145 · arXiv:1201.2208（Schlafly et al., PS1 1.5 年测光校准）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=1201.2208`：title "Photometric Calibration of the First 1.5 Years of the Pan-STARRS1 Survey"，authors **Schlafly, Finkbeiner, Juric**（et al.），`<id>` 末位 **v2**，**无 journal-ref 字段**。与 [B28]（`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:136`）登记的题名/作者/2012 **逐字一致**。
- **关联性：关联对**。[B28] 角色＝PS1 测光校准一手来源；同文在 `REVERSE_VERIFY_BIBLIOGRAPHY.md:118` [7.2] 以发表版出现（ApJ 756, 158），角色"从重叠观测同时拟合响应与零点 ⇒ UPM 的 `g_k` 拟合先例"与题名所指一致。
- **版本：未钉版次（缺陷）**。[B28] 只给 arXiv 号、无卷页/DOI，而该号存在 **v1/v2**，API 无 journal-ref ⇒ 概念级引用漂到 v2。同仓 [7.2] 已给发表版锚点（`ApJ 756, 158, DOI 10.1088/0004-637X/756/2/158`，**该 DOI 本轮未独立复核**，不作存在性证据），建议 [B28] 补齐同一锚点。
- 用量：网络 1/3；仓库 1/2。

## 151 · arXiv:1801.03181（DES DR1）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=1801.03181`：title "**The Dark Energy Survey Data Release 1**"，authors **Abbott & Abdalla**（et al.），`<id>` 末位 **v3**，DOI 字段 **10.3847/1538-4365/aae9f0**，摘要首句 "We describe the first public data release of the Dark Energy Survey, DES DR1…"。
  - **相邻号绑反排查（高危形态①，本条命中风险族）**：同项目同期存在 **arXiv:1801.03177 = Morganson et al., DES 图像处理管线**（PASP 130, 074501；仓内 `REVERSE_VERIFY_BIBLIOGRAPHY.md:57` [3.1] 与 `REPORT_paper.md:401` 引的正是它）。本仓位点写的是 "DES DM（**DR1** arXiv:1801.03181；DR2 arXiv:2101.05765）" ⇒ **标签 DR1 与号 1801.03181 对得上**，未与 03177（管线文）绑反。
- **关联性：关联对（含节号核对）**。位点 `实验/absolute-snr/docs/surveys/frame-snr-survey.md:123` [A4]：断言 "DES 不定义、也不发布 SNR 标量，用 SExtractor 的 FLUXERR/MAGERR；用户经 Pogson 微分反推 `δm = −(2.5/ln10)(δF/F)`（**DR1 §4.4.2 逐字**）"。全文核对（ar5iv 渲染的最新版正文）：**§4.4.2 存在，标题 "Magnitude limit at fixed signal-to-noise"**，且该节确有 δm–δF/F 的微分关系式 ⇒ 节号与内容对得上。**逐字性保留一点**：回包渲染为 `δm = -2.5 ln10 δF/F` 形态，与仓内转写 `−(2.5/ln10)(δF/F)` 之间是 LaTeX 渲染歧义（2.5/ln10≈1.0857 为物理正确形式），**不据此判红**。
- **版本：未钉版次（缺陷）**。[A4] 仅给 arXiv 号（现存 v1–v3），未给 ApJS 卷页/DOI；本轮由官方 API 回包取得 DOI **10.3847/1538-4365/aae9f0** 可补锚点。被引的 §4.4.2 内容在最新版已核对存在 ⇒ 无"仅旧版才有"风险，但概念级解析会随版本漂移。
- 用量：网络 2/3；仓库 1/2。

## 157 · arXiv:2107.03403 —— 核验态：存在性已核／关联性**角色错绑**
- **存在性：已核（但该号不是仓内所指的文献）**。arXiv 官方 API `id_list=2107.03403`：title "**The GOGREEN Survey: Evidence of an excess of quiescent disks in clusters at 1.0<z<1.4**"，first author **Chan**（+ Wilson, Balogh, Rudnick, van der Burg, Muzzin, … 共 24 人），`<id>` 末位 **v1**，DOI 字段 **10.3847/1538-4357/ac1117**；摘要首二句为星系形状/轴比测量（HST 近红外成像 + Sérsic 拟合）。⇒ **该 arXiv 号真实存在，属星系形态学论文**。
- **关联性：角色错绑（本批最重缺陷）**。位点 `实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md:845` 第 10 项："**中位数方差**的规范教科书一手来源 —— 未能打开；支撑 = 渐近公式自含推导 + LSST 代码 + [arXiv:2107.03403]"。`git grep -n "2107.03403"` 全仓**仅此 1 处命中**，即该号唯一用途就是给"Var(median)≈πσ²/(2N)"一类的中位数方差结论做支撑。GOGREEN 星系盘/轴比分析与该断言**无任何关系**，属把不相干文献派作统计结论的支撑。
  - 订正方向：**不是**改卷页，而是**撤下或换成真正讨论中位数/稳健尺度估计渐近方差的文献**。本轮**未能**在同一 arXiv 号族内定位到"作者本意"的那篇，故**不猜替代号**（避免以新编造覆盖旧编造）。同主题的仓内另一处（`SCIENTIFIC_REFERENCES.md:108` [65] Kendall & Stuart, *The Advanced Theory of Statistics* Vol.1）是该结论的教科书定位，未钉卷页。
- **版本：不适用／未钉版次**。所引内容不在所指版次里（v1 全文主题为星系形状），故"版本"无从谈对；本条正确判语是**指向错文**。
- 用量：网络 1/3；仓库 2/2（Read 位点原文＋git grep 全仓命中集）。

## 163 · arXiv:2406.03310（Rodrigo et al. 2024, SVO FPS）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=2406.03310`：title "**Photometric segregation of dwarf and giant FGK stars using the SVO Filter Profile Service and photometric tools**"，first author **Rodrigo**（+ Cruz, Aguilar, Aller, Solano, … 共 21 人），`<id>` 末位 **v4**，DOI 字段 **10.1051/0004-6361/202449998**，journal-ref **A&A 689, A93 (2024)**。与 [B62]（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:197`）"Rodrigo, C., et al. 2024 (SVO FPS) arXiv:2406.03310 [S]"的作者/年份/编号一致。
- **关联性：关联对（弱标签，需加限定）**。该条只出现在档案"定标 / Gaia XP / 绝对通量（续）"书目池，括注写作 "(SVO FPS)"。回包题名确含 "using the **SVO Filter Profile Service**"，故括注不算错；**但**该文是**用** SVO FPS 做 FGK 矮/巨星测光分离的应用论文，**不是** SVO 滤光片库本体的描述性文献——若后续把它当"通带曲线来源/物镜库总述"引用即为**角色升格**，现登记无此断言 ⇒ 判关联对，附标签歧义提示。
- **版本：未钉版次（缺陷）**。[B62] 只给 arXiv 号（现存 **v1–v4**），未给 A&A 689 A93 与 DOI ⇒ 概念级引用漂到 v4。建议补 `A&A 689, A93 (2024); DOI 10.1051/0004-6361/202449998`（二者由官方 API 回包核对，非凭记忆）。
- 用量：网络 1/3；仓库 1/2。

## 169 · arXiv:astro-ph/0212362（Blakeslee et al., ACS 自动处理流水线）—— 核验态：已核
- **存在性：已核**。arXiv 官方 API `id_list=astro-ph/0212362`：title "**An Automatic Image Reduction Pipeline for the Advanced Camera for Surveys**"，authors **Blakeslee, Anderson**（et al.），`<id>` 末位 **v1**，journal-ref 字段："To appear in the proceedings of **Astronomical Data Analysis Software and Systems XII**, October 2002, Baltimore, MD"。与 [B45]（同档 `:178`）"Blakeslee, J. P., et al. 2003, ASP Conf. Ser. 295 (ADASS XII), arXiv:astro-ph/0212362 [S]"一致。
- **关联性：关联对**。[B45] 括注"APSIS = ACS GTO 自动处理流水线"，摘要首句逐字为 "We have written an automatic image processing pipeline for the Advanced Camera for Surveys (ACS) Guaranteed Time Observation (GTO) program." ⇒ 角色（HST/ACS 自动归约流水线先例，用于 drizzle/欠采样测光池）绑定正确。
- **版本：版本对**。journal-ref 只到"ADASS XII 会议"层，未回显 ASPC 卷号 295 ⇒ 仓内"ASPC 295 (2003)"与会议同名同期（ADASS XII 文集），本轮由 journal-ref 核对到会议层，卷号 295 **未逐字复核**（如需可走 `aspbooks.org/custom/publications/paper/295-<页>.html`，但仓内未给页码 ⇒ 该卷页为**未钉页**状态）。arXiv 仅 v1，无版本漂移。
- 用量：网络 1/3；仓库 1/2。

## 175 · Howell, *Handbook of CCD Astronomy*（DOI 10.1017/cbo9780511807909, Ch. 4）—— 核验态：存在性已核／节号 UNPROVEN
- **存在性：已核**。Crossref `works/10.1017/cbo9780511807909`：book title "**Handbook of CCD Astronomy**"／author **Steve B. Howell**／publisher **Cambridge University Press**／year **2006**／**Edition 2**／DOI 10.1017/cbo9780511807909。与仓内 `实验/absolute-snr/docs/snr-propagation-design.md:1115` 的 "Howell 2006, CUP, DOI 10.1017/cbo9780511807909（Ch. 4，等式号未核）"**一致**。
- **关联性：关联对（角色层）；"Ch. 4" 定位 UNPROVEN**。角色＝"CCD 方程的项结构（源/天光/暗流/读出/平场）"，即把 Howell 手册用作 CCD 信噪比方程的教科书级出处——该用途与该书的实际内容域相符（且仓内已在"不借鉴"列写明"孔径框架；**方程号未核不得写**"，自我约束正确，**无伪托式号**）。唯 **"Ch. 4" 这一节号未获出版社侧核对**：`doi.org` 302 跳 `cambridge.org/core` 后页面为 JS 壳、无目录；两次书目检索命中的均是非权威中转页（doc88/wenku 等），不足以定节号 ⇒ 记 UNPROVEN。
- **版本：版本对（本批唯一"同名再版"高危点，已核住）**。Crossref 明确该 DOI 指向 **2nd edition (2006)**，与仓内标注年份 2006 一致 ⇒ 不存在"引 2 版内容却钉 1 版（2000）"或反向的版次错绑（这正是前言 §1.3 举的量化噪声项 `+1/12` vs `g²/12` 一族的失效模式）。若后续要在正文写具体方程项，须回到 2 版对应章节逐字核（本仓现禁令已覆盖此风险）。
- 用量：网络 5/3（其中 2 次为检索噪声、1 次为 doi.org 302 无内容——超支如实登记，不作证据用）；仓库 1/2。

## 181 · Evans et al. 2018, A&A 616, A4（Gaia DR2 photometric content；DOI 10.1051/0004-6361/201832756）—— 核验态：已核
- **存在性：已核**。Crossref `works/10.1051/0004-6361/201832756`：title "Gaia Data Release 2: Photometric content and validation"／first author **Evans**／*A&A*／**2018**／volume **616**／page **A4**。与 `REVERSE_VERIFY_BIBLIOGRAPHY.md:124` [7.8] "Evans et al. 2018, A&A 616, A4. DOI 10.1051/0004-6361/201832756" **完全一致**（仓内未写副标题 ⇒ 无副标题错写面）。
- **关联性：关联对**。角色＝"若使用 **DR2** 而非 EDR3/DR3 作参考星表时的版本专用引用；不得在同一句混用 DR2 与 EDR3/DR3 的定标陈述"。回包题名正是 DR2 测光内容/验证文，与同段 [7.7] Riello 2021（EDR3, A&A 649 A3）并列分工明确 ⇒ 版本层角色绑定正确。
- **版本：版本对**。DOI 明确落在 DR2 专刊 A&A 616 A4 (2018)。
- 用量：网络 1/3；仓库 1/2（同一次读表覆盖 [3.7]/[5.4]/[7.8]）。

---

## 本批三态计数（18 条）

| 字段 | 计数 |
|---|---|
| 存在性 | 已核 **18** ／ 不存在 **0** ／ UNPROVEN **0** |
| 关联性 | 关联对 **15**（49、72、91、97、109、121、127、133、139、145、151、163、169、175、181，其中 109/121/133/175 附"引文逐字性／式号／节号／方法细节"保留）／关联对但**节号错绑 1**（103）／**角色错绑 1**（157）／关联性 UNPROVEN **1**（115） |
| 版本 | 版本对 **14**（49、72、91、97、103、109、115、121、127、133、139、169、175、181）／**未钉版次 3**（145、151、163）／不适用 **1**（157，指向错文故无版次可谈） |

### 判红两条（按严重度排序）

1. **#157 `arXiv:2107.03403` = 角色错绑**（本批最重）。该号真实存在，是 Chan et al., *The GOGREEN Survey: Evidence of an excess of quiescent disks in clusters at 1.0<z<1.4*（ApJ, DOI 10.3847/1538-4357/ac1117），与"中位数方差"毫无关系；全仓仅此一处命中（`实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md:845` 用它给 `Var(median)` 类结论做支撑）。修法是**撤下该支撑件**、不是改卷页；本轮**未**指定替代号（不猜）。
2. **#103 `arXiv:2508.15278` 节号错绑**。引文逐字真（"…linear and B-spline… galaxy background in the LRIS red channel is overestimated"＋footnote 7 的 knot 敏感性），但在 v2 全文中位于 **§IV.2**"Revisiting a Nebular-phase SN: SN 2019eix"，仓内标的是 **§4.3**（`实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:711`）。

### 建议补锚点（非判红，属"未钉版次"缺陷）

- [B28] `arXiv:1201.2208`（v1/v2 存在、无 journal-ref）⇒ 补同仓 [7.2] 已用的发表版锚点；
- [B62] `arXiv:2406.03310`（现 v4）⇒ 补 `A&A 689, A93 (2024); DOI 10.1051/0004-6361/202449998`（由官方 API 回包取得）；
- [A4] `arXiv:1801.03181`（现 v3）⇒ 补 `DOI 10.3847/1538-4365/aae9f0`（同上）；
- #121 ICASSP 条目 ⇒ 补页码 1081–1084（Crossref 回包）。

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

| 条 | 未证项 | 已用标识符／途径 | 下轮建议途径 |
|---|---|---|---|
| 115 | "SDSS `ivar`/mask 语义**在 EDR 论文中详细成文**" | DOI 10.1086/324741（Crossref ✓ 书目层）；`arxiv.org/pdf/astro-ph/0009253`（乱码）＋abs 页（**证伪**：该号是 Maciejewski 黑洞文） | ADS 需 token；`iopscience.iop.org/article/10.1086/324741`；或改锚 Lupton et al. 2001 ASPC 238, 269（arXiv:astro-ph/0101420，可开 HTML） |
| 109 | 摘要两段引文的逐字性 | arXiv API（仅回首句）、Crossref（无 abstract 回显，首查 429） | ScienceDirect 摘要页 / arXiv abs HTML 全文摘要 |
| 121 | §2.2 与式 (3)(4)(6) 的式号、σ≈2 overshoot 表述 | Crossref ✓；作者自留 PDF `webpages.tuni.fi/foi/papers/ICASSP2012-Makitalo-Foi-GenAnscombe.pdf`（本环境 PDF 抽取失败） | 作者项目页 `www.cs.tut.fi/~foi/invansc/`（HTML）／IEEE Xplore 预览 |
| 133 | 3-pass 链、序贯集中趋势测度、经验修正因子表 | arXiv API 与 abs 页两次仅回摘要首句；Crossref 无 abstract | ApJS 全文（IOP）／Elsevier／RCR 软件文档 |
| 175 | "Ch. 4" 节号 | Crossref ✓（含 Edition 2）；doi.org 302 → cambridge.org/core（JS 壳无目录）；2 次检索命中均为非权威中转页 | Google Books 目录预览／馆藏记录（世界猫） |
| 139 | ADS 端 bibcode 回显 | Crossref 记录**无** `alt-bibcode` 字段；未走 ADS（需 token） | 如需 ADS 层确认：带 token 的 `ui.adsabs.harvard.edu/abs/2014PASP..126..158G` |

## 新发现的缺陷形态（本批独有）

1. **"真文献派错活"形态**（#157）：号、DOI、卷页全真，但与所支撑的统计结论无关。比"文献不存在"更隐蔽——书目自动核对（只核 DOI 可达性）永远查不出，必须**打开断言句**比对（本批靠此抓出）。
2. **引文逐字真而节号漂一层**（#103，§4.3 ↔ 实际 §IV.2）：罗马/阿拉伯数字层级易被人工反推，属前言 §4② 的一个新变体。
3. **书目池括注升格**（#163 "(SVO FPS)"）：把"使用某工具的应用论文"登记成"工具本体文献"，一旦下游按括注引用即成角色错绑——建议在池内括注补"用 SVO FPS 做 FGK 矮/巨星分离"限定。
4. **半钉版次**（#145/#151/#163）：只给 arXiv 号、不给期刊卷页/DOI，而该文有 v1–v4 ⇒ 概念级解析漂移。
5. **流程性发现（与文献无关但影响取证）**：本环境 **PDF 取回 2/2 次均为乱码二进制**（#115、#121），故凡需读正文的条目应**先走 HTML 途径**（`arxiv.org/html/<id>vN` 新文、`ar5iv.labs.arxiv.org/html/<id>` 旧文——#151 即由此一次成功核到 §4.4.2）；建议把该优先级写进派单 §3 手段表。
6. **网关 429 吃掉取证额度**（#109）：`api.crossref.org` 同轮 3 并发即 429，重试成功但仍计额度 ⇒ 建议派单明确"网关类失败（429/403/302 无内容）不计入 3 次额度"，并把 Crossref 并发限到 2。

## 覆盖率自报

- **18/18 = 100%** 条数覆盖，全部逐条给出三字段（无"未核"遗留）。
- 存在性 18/18 判定；关联性 17/18 判定（1 条按前言 §2 降级为 UNPROVEN：#115）；版本 17/18 判定（#157 不适用）。
- 关联性中 **4 条**（109、121、133、175）为"角色对＋细节未逐字核"，已按 §2 明写降级范围，未以"标题相关"充当证据。
- 网络查询合计 **33 次**／额度 54（超额的三条已各自如实登记：#109 含 1 次网关 429、#175 含 2 次检索噪声＋1 次 302 无内容）；仓库读取 **18 次**／额度 36；**未修改** `整改/out/文献台账.csv`、未改仓库任何文件、未跑构建与门禁。

<!-- PROGRESS: 18/18 -->
