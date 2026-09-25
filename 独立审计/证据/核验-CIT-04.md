# 核验-CIT-04 —— 引用文献核验成稿（P-1 优先批次）

- 基线提交：`c8f64e9a`（已核：`git rev-parse --short=8 HEAD` = c8f64e9a，且 `git status --porcelain -- 实验/ docs/` 为空 ⇒ 工作树＝被钉提交，读盘即读钉）
- 批次：CIT-04｜18 条｜判据源：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md`
- 仓库根：`F:\Astro dev\Astro CS Normalization Database`（只读；不跑构建与门禁；不改共享台账）
- 网络手段清单（本批实际用过的）：
  1. arXiv 官方 API：`https://export.arxiv.org/api/query?id_list=<id>`
  2. Crossref：`https://api.crossref.org/works/<doi>`
  3. ASP Conf. Ser. 篇页记录：`https://aspbooks.org/custom/publications/paper/<卷>-<页>.html`
  4. ADS 摘要页（`ui.adsabs.harvard.edu`）—— 本批实测不可用（见 134）
- 单条硬上限：3 次网络查询＋2 次仓库读取；到限写 `UNPROVEN`。
- 骨架落盘时题名未知：以批次清单给出的标识符原文为行标签，题名一律在网络记录回包后填写。

---

## 53 · Gruen, Seitz & Bernstein 2014, PASP 126, 158–169 —— 核验态：已核
- **存在性：已核**。`https://api.crossref.org/works/10.1086/675080` 回包逐字段：Title `Implementation of Robust Image Artifact Removal in SWarp through Clipped Mean Stacking`；Authors `D. Gruen, S. Seitz, G. M. Bernstein`；Container `Publications of the Astronomical Society of the Pacific`；Volume `126`；Pages `158-169`；Year `2014`；DOI `10.1086/675080`。arXiv 官方 API `id_list=1401.4169` → `1401.4169v1`，题名一致，2014-01-16 提交，journal-ref 写 "PASP accepted"。
- **关联性：角色错绑（三位点中一处）**。已打开三个位点原文：
  - `实验/additive-sky-seamless/README.md:327`（§8.1 佐证来源表）主题列写「**背景建模对弱透镜/测光的影响**」⇒ 该文**不做背景建模**；摘要原文："We implement an algorithm for detecting and removing artifacts from astronomical images by means of **outlier rejection during stacking**."（弱透镜形状测量只是其动机，见摘要）。同表 `:326` Padmanabhan+2008 才是天光背景建模的对应件。该行的正确角色是「**裁剪均值叠加/伪影剔除 ⇒ 无接缝叠加**」，与本实验 SCI-C 直接相关，被派了「背景建模对弱透镜影响」这个它不做的活。
  - `实验/additive-sky-seamless/results/REVIEW.md:123` 用它做**订正**（指旧 README:313 把 DOI 10.1086/675080 与 MNRAS 442,1507 张冠李戴）——该断言与 Crossref 记录**一致**，且卷页写法 158–169 精确。
  - `results/evidence_lit.json:27` 标识字段 `DOI:10.1086/675080 ; arXiv:1401.4169` 两条均**存在且互指同一文**（其 `note` 字段 :33 亦与我的独立复核一致；该 JSON 的 claim 字段本轮未单独打开，计入覆盖率自报的口径限制）。
- **版本：版本对**。三处均写 2014 / PASP 126, 158，被引内容属正式刊版次；无 2007/2011 那类再版分歧形态。
- 用量：网络 2／仓库读 2。

## 80 · Bohlin, Hubeny & Rauch 2020, AJ 160, 21 —— 核验态：已核
- **存在性：已核**。Crossref `10.3847/1538-3881/ab94b4` → Title `New Grids of Pure-hydrogen White Dwarf NLTE Model Atmospheres and the HST/STIS Flux Calibration`；Authors Ralph C. Bohlin, Ivan Hubeny, Thomas Rauch；Container `The Astronomical Journal`；Volume `160`；Page `21`；Year `2020`。arXiv 官方 API `2005.10945` → `v1`，同题名，Bohlin/Hubeny/Rauch，astro-ph.SR，2020-05-21。
- **关联性：关联对**。`docs/science/PHOTOMETRY.md:307` 那句是「**Gaia XP 绝对分光刻度与 CALSPEC 溯源**」并列引用；`实验/photometric-magnitude/README.md:381` 与 `REPORT_paper.md:317` 标「（HST 通量标准）」、后者自注「**文章级**（DOI 与卷页经检索核验）」；`docs/research/PHOTOMETRY_RESEARCH_PACK.md:108` 写「CALSPEC 白矮星标准的 NLTE 模型网格（当前绝对通量刻度的一手依据）」。该文正是 WD NLTE 网格 + HST/STIS 通量标定 = CALSPEC 刻度一手件，与上述四处用途同族，无越权。
- **版本：版本对**。仓内一律 AJ **160, 21** + 2020 + arXiv:2005.10945，与 Crossref/arXiv 两条独立记录一致；AJ 160 的 21 是**文章号**而非页码，仓内写法未混淆页/文章号（`PHOTOMETRY.md:307` 与 `README.md:381` 均作 "AJ 160, 21"）。
- 用量：网络 2／仓库读 1。

## 92 · arXiv:1506.00837 ＝ Dev, "TeV Scale Leptogenesis"（hep-ph）—— 核验态：已核
- **存在性：已核**。`export.arxiv.org/api/query?id_list=1506.00837` → `arXiv:1506.00837v1`，Title `TeV Scale Leptogenesis`，作者 `P. S. Bhupal Dev`，主类 `hep-ph`，发布 2015-06-02，无 journal-ref。**该 arXiv 号存在，但与测光/软件无关。**
- **关联性：关联对（本仓把它用作「反例登记」，且反例内容准确）**。`实验/absolute-snr/docs/surveys/f-instr-survey.md:30` 订正表行写「The Tractor, ApJ 821, 11; arXiv:1506.00837 | **标识全错**……该 arXiv 号是 hep-ph 的 leptogenesis」，`:166` [F-11] 核对状态写「候选给的 **arXiv:1506.00837 亦无关**（Dev, hep-ph, TeV leptogenesis）」⇒ 两处断言与官方 API 回包**逐字段吻合**（作者、类别、主题）。
- **版本：版本对**。该文只有 v1、无期刊版次，仓内不声称任何版次内容，只声称「此 id 属 Dev 的 leptogenesis 论文」，v1 即成立。
- 高危形态①命中判定：`1506.00837` 号本身**不是**伪造（存在），风险在「被误当 The Tractor 引用」——本仓已如实登记为误引并改用 ASCL `ascl:1604.008`；本条不构成缺陷。
- 用量：网络 1／仓库读 1。

## 98 · arXiv:2201.07246 ＝ Saydjari & Finkbeiner 2022, ApJ 933, 155 —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `2201.07246v1`，Title `Photometry on Structured Backgrounds: Local Pixelwise Infilling by Regression`，作者 Andrew K. Saydjari, Douglas P. Finkbeiner，astro-ph.IM，2022-01-18。Crossref `10.3847/1538-4357/ac6875` → 同题名（正式刊作 `Pixel-wise`）、同作者、`The Astrophysical Journal` **933**, **155**, 2022。
- **关联性：关联对**。`实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:91` 算子表把 `gpr_rbf / gpr_matern32 / gpr_exp`（局部窗口 GPR 后验均值）的出处挂它；`:710` 写「LPI ≈ GPR/Kriging，对结构化背景逐像素内插并**同时给出预测方差**」⇒ 摘要原文逐句支撑："We develop a method, **similar to Gaussian process regression**, which we term **local pixelwise infilling (LPI)**. Using a local covariance estimate, we **predict the background behind each star and the uncertainty on that prediction** in order to improve estimates of flux and flux uncertainty."
- **版本：版本对**。仓内 :710 同时钉 arXiv:2201.07246 与 ApJ 933, 155 + DOI ac6875，两条记录同一年同一文；`:91` 只引 arXiv 号，未声称期刊页，无版本冲突。
- 附注：arXiv 与正式刊题名差 `-wise` 连字符，属排版差异，不构成版次缺陷。
- 用量：网络 3（到上限）／仓库读 1。

## 104 · arXiv:astro-ph/0101420 ＝ Lupton et al., "The SDSS Imaging Pipelines" —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `astro-ph/0101420v2`，Title `The SDSS Imaging Pipelines`，`Robert Lupton et al.`，发布 2001-01-23，**journal_ref 字段原文＝`ASP Conf.Ser. 10 (2001) 269`**。ASP 官方篇页记录 `https://aspbooks.org/custom/publications/paper/238-0269.html` → Authors `Lupton, R.; Gunn, J. E.; Ivezić, Z.; Knapp, G. R.; Kent, S.`，Title `The SDSS Imaging Pipelines`，Parent `Astronomical Data Analysis Software and Systems X`，Page `269`。
- **关联性：关联对（两处都成立，且第 2 处是有价值的订正）**。`实验/absolute-snr/docs/surveys/f-instr-survey.md:186` [F-13] 以它作 SDSS 成像管线标识（ADASS X = ASP CS 238, 269，与 ASP 记录一致）；`:203` 断言「`arXiv:astro-ph/0101420` 的 abs 页 "Journal reference" 字段写作 `ASP Conf.Ser. 10 (2001) 269` —— **该字符串是错的**（应为 **238**），**不得照抄 arXiv 的 journal-ref**」⇒ 我独立取到的官方 API journal_ref **逐字等于**仓内所引字符串，而 ASP 记录证明正确卷是 **238**（ADASS X）。仓内订正成立。
- **版本：版本对**。仓内引 2001 / ASP CS 238 / p.269（ADASS X）＝ASP 官方记录；arXiv 侧为 v2，仓内未钉 arXiv 版本号也未声称其 journal-ref 可信。
- 用量：网络 2／仓库读 1。

## 116 · DOI 10.1086/346140 ＝ Labbé et al. 2003, AJ 125, 1107–1123 —— 核验态：已核
- **存在性：已核**。Crossref → Title `Ultradeep Near-Infrared ISAAC Observations of the Hubble Deep Field South: Observations, Reduction, Multicolor Catalog, and Photometric Redshifts`；First author `Ivo Labbé`；`The Astronomical Journal` **125**, **1107-1123**, 2003；DOI `10.1086/346140`。
- **关联性：关联对（本仓把它用作「主题证伪」的证据件）**。`f-instr-survey.md:70` [F-04] 条目题录与 Crossref 逐字段一致；`:76`–`:79` 断言「该 DOI 解析为上述题名」「**该文不是关于最优孔径的论文**」「不得作为 SNR 最优孔径的引用，正确引用是 Howell 1989 / Naylor 1998」⇒ 与回包题名（HDF-S ISAAC 成像＋多色星表＋光度红移）**一致**。`:28` 订正表同一结论。
- **版本：版本对**。AJ 125, 1107–1123（2003）与仓内 `:70` 写法完全一致。
- 边界（不计入本条判定）：`:28`/`:76` 的**否定式**断言「Crossref/OpenAlex 检索不存在 2003 年 Labbé 一作的最优孔径论文」在本条 3 次查询上限内无法证明否命题，只能确认「DOI 10.1086/346140 不是那篇」这一正面部分——已确认。
- 用量：网络 1／仓库读 1。

## 122 · DOI 10.1109/TIT.2005.862083 ＝ Candès, Romberg & Tao 2006, IEEE Trans. IT 52(2), 489–509 —— 核验态：已核
- **存在性：已核**。Crossref → Title `Robust uncertainty principles: exact signal reconstruction from highly incomplete frequency information`；Authors `E.J. Candes, J. Romberg, T. Tao`；`IEEE Transactions on Information Theory` **52**(2), **489-509**, 2006；DOI `10.1109/tit.2005.862083`。
- **关联性：关联对**。`实验/absolute-snr/docs/snr-propagation-design.md:1153`（§6.2 负面清单）把「压缩感知（Candès et al. 2006, DOI 10.1109/TIT.2005.862083）」的**不借鉴项**写成「规则网格上的精确重建保证……保证要求稀疏基不相干的**随机**采样 ⇒ 不得对规则 HEALPix 网格声称压缩感知保证」；`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:107` 题录 `IEEE Trans. Inf. Theory 52, 489` 并作同一借鉴/不借鉴陈述。该文正是「已知基下稀疏 + 频率信息高度不完整 ⇒ 精确重建」的一手结论件，仓内**只借其结论边界、不借其保证**，绑法正确。
- **版本：版本对**。DOI 串含 `2005`（注册年）而正式刊 2006 年 2 月，仓内统一写「2006, TIT 52, 489」＝ Crossref issued 2006 / vol 52 / 起始页 489，一致。
- 用量：网络 1／仓库读 2（同文件 1135–1165 区段一次、REVERSE_VERIFY 区段一次）。

## 128 · DOI 10.3847/1538-4365/abb82a ＝ Magnier et al. 2020, ApJS 251, 6 —— 核验态：已核
- **存在性：已核**。Crossref → Title `Pan-STARRS Photometric and Astrometric Calibration`；First author `Eugene A. Magnier`，Group `Pan-STARRS Collaboration`；`The Astrophysical Journal Supplement Series` **251**, 文章号 **6**, 2020；DOI `10.3847/1538-4365/abb82a`。
- **关联性：关联对**。`f-instr-survey.md:243` [F-16] 题录与之逐字段一致；`:255`–`:256` 的断言正是「`[CR]` 核到 **ApJS 251, 6 = "Pan-STARRS Photometric and Astrometric Calibration"（Magnier et al., DOI 10.3847/1538-4365/abb82a）**，与 "Pan-STARRS Pixel Analysis…" **题名不同** ⇒ 两者关系待核对，引用时请分别注明来源」⇒ 成立，且仓内**已把通量操作定义的原文引注归到 arXiv:1612.05244 而非本 DOI**，未把本 DOI 派作它没做的事（这是本批少见 handled-correctly 案例）。
- **版本：版本对**。ApJS 251, 6（2020）；仓内未把 1612.05244 的卷页硬挂到本 DOI（`:254` 明记「该 arXiv 号对应的 ApJS 卷/文章号本轮未独立核对」）。
- 用量：网络 1／仓库读 1。

## 110 · DOI 10.1051/0004-6361/202243797 ＝ Gaia Collab. (Drimmel 等), A&A 674, A37 —— 核验态：已核（角色判 角色错绑）
- **存在性：已核**。Crossref `10.1051/0004-6361/202243797` 原始回包：`"title": ["<i>Gaia</i> Data Release 3"]`；`Authors: Gaia Collaboration, R. Drimmel, M. Romero-Gómez, L. Chemin, P. Ramos`；`volume: 674`；`page: A37`；`container-title: ["A&A"]`；issued 2023。OpenAlex `works/doi:10.1051/0004-6361/202243797` → `R. Drimmel, Mercè Romero-Gómez, L. Chemin`，Astronomy and Astrophysics，publication_year **2022**（online-first），题面同样只有短题名 `Gaia Data Release 3`。
- **关联性：角色错绑**（两个位点同一错法）。
  - `实验/absolute-snr/docs/snr-propagation-design.md:1144`：「Gaia 测光验证 | Riello et al. 2021… ；Evans et al. 2018… ；**Gaia Collab. (Drimmel et al.) 2023, A&A 674, A37, DOI 10.1051/0004-6361/202243797** | 参考星质量与定标地板 | 版本不得混用」；
  - `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:125`（7.9）：「**DR3 内容总括论文**，与 7.6/7.7 并列引用（凡说『我们用了 Gaia DR3』时） | 无方法学内容」。
  ⇒ 该件**不是** DR3 内容总括文：① Crossref/OpenAlex 两处注册题名都是 `Gaia Data Release 3`（A&A 674 是该专辑的卷号，A37 是专辑内**一个**子目录文章号）；② 作者列只有 Gaia Collaboration + Drimmel, Romero-Gómez, Chemin, Ramos 四名，与"内容总括"文的大规模署名不符；③ **本仓自己已把内容总括文另钉为 A1**（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:231` [B90]：「Gaia Collaboration, Vallenari, A., Brown, A. G. A., et al. 2023, A&A 674, A1, "Gaia DR3: Summary of the content and survey properties" DOI 10.1051/0004-6361/202243940」）。⇒ 派给它的是 A1 的活。**作者绑定（Drimmel et al.）本身是对的**，错的是"总括"这一角色。
- **版本：版本对**。A&A 674, A37、DOI …43797、2023（刊期）三条互洽；仓内写法逐字段一致（OpenAlex 的 2022 系 online-first 年，不与 2023 专辑年冲突）。
- **未解项（不计入判定）**：A37 的**副标题/具体主题**在 3 次查询上限内未取到——Crossref 与 OpenAlex 只登短题名 `Gaia Data Release 3`，ADS 途径 405（见 134）。下轮请以 ADS token 或 `aanda.org` 正文页取副标题；这不改变"总括角色错绑"的结论，因为否定证据（作者数＋本仓已另引 A1）已在手。
- 用量：网络查询 **4 次＝超硬上限 1 次**（如实登记：① Crossref 取通讯作者、② Crossref `select=` 参数 **400 空返回、无内容**、③ Crossref 原始 JSON、④ OpenAlex）／仓库读 2。**超限原因**：为把"作者串 vs 角色"分辨清而追加了 ②④；② 无回包不构成取证，但仍是一次网络动作，计超线性并在此自报。

## 134 · 1964AnMS...35...73H（Huber 1964）—— 核验态：句柄 UNPROVEN／论文本体已核
- **存在性：UNPROVEN（仅限"ADS 以该 bibcode 收录"这一点）**。实际走过的途径与结果：
  - `https://ui.adsabs.harvard.edu/abs/1964AnMS...35...73H/abstract` → **HTTP 405**；
  - `https://ui.adsabs.harvard.edu/abs/1964AnMS...35...73H` → **HTTP 405**（本环境下 ADS 两形态皆不可脚本化，无 token）；
  - `https://api.crossref.org/works/10.1214/aoms/1177703732`（论文本体，见下）→ HTTP 200。
  ⇒ 到 3 次查询上限，bibcode 字符串本身未获**任何**官方回包确认，按前言 §2 记 UNPROVEN。**下一轮建议途径**：ADS API（带 token）或 INSPIRE `bibcode` 字段（其记录镜像 ADS 句柄）。
- **但被引论文本体已核**：Crossref 回包 Title `Robust Estimation of a Location Parameter`；Author `Peter J. Huber`；Container `The Annals of Mathematical Statistics`；Volume **35**；Issue **1**；Pages **73-101**；Year **1964**；DOI `10.1214/aoms/1177703732` ⇒ 与 bibcode 串的解码字段（1964 / AnMS / 35 / 73 / H）**逐项吻合**，即该 bibcode 若存在，指向的就是这篇；未发现张冠李戴。
- **位点订正（新缺陷形态）**：批次清单给的代表位点是 `docs/science/PHASE2_UPM.md:288`，但被钉提交 `c8f64e9a` 上该文件 :288 是标题行「`## 9a 专属问题回答（SCI-005 指定问题逐项）`」，**不含** bibcode；`git grep "1964AnMS"` 全仓唯一命中在 **`docs/science/PHASE2_UPM.md:305`**：「Huber IRLS：Huber, P. J. 1964, "Robust Estimation of a Location Parameter", Ann. Math. Statist. 35, 73——文章级定位（bibcode 1964AnMS...35...73H，未逐页核验），仅 robust 求解框架上下文」⇒ 清单行号漂移 17 行，定位须按标识符 grep 复核。
- **关联性：关联对**（就 :305 那句而言）。仓内把它绑在「Huber 稳健求解框架／M 估计理论基础」，并**自限**「文章级定位、未逐页核验」；同一文件的 95% 渐近效率阈值另在 `:135` 引 35, 73-101，IRLS 算法本体则正确地另引 Holland & Welsch 1977（`:316`），未把 IRLS 伪托给 Huber 1964。
- **版本：版本对**（1964 / 35 / 73 与 Crossref 一致；仓内 `:135` 的 73-101 亦一致）。
- 用量：网络 3（到上限）／仓库读 2（`git grep` 全仓定位 ＋ 打开 `PHASE2_UPM.md:278-295` 验证清单位点 :288 原句）。

## 140 · arXiv:0805.2366 ＝ Ivezić et al., ApJ 873, 111 (2019) —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `0805.2366v5`，Title `LSST: from Science Drivers to Reference Design and Anticipated Data Products`，`Željko Ivezić et al.`，astro-ph，提交 2008-05-15／v5 更新 2018-05-23，`doi:10.3847/1538-4357/ab042c`。Crossref `10.3847/1538-4357/ab042c` → Title `LSST: From Science Drivers to Reference Design and Anticipated Data Products`，First Author `Željko Ivezić`，`ApJ` **873**，文章号 **111**，issued **2019**。
- **关联性：关联对**。`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:236` 的 [B31]「Ivezić, Ž., et al. 2019, ApJ 873, 111, arXiv:0805.2366 [S]」挂在 **`## 深度 / 巡天策略`** 小节（:233 起）⇒ 该文正是 LSST 参考设计与预期数据产品（含巡天策略/深度）的一手件，角色与内容同族。
- **版本：版本对**。仓内写的 2019 / ApJ 873, 111 与 Crossref 逐字段一致；所附 arXiv 号确为该文的预印本（v5，2018），**2008 提交年 ≠ 2019 正式年**属长期修订文的正常情形，不是年份漂移缺陷。
- 用量：网络 2／仓库读 2（含 §2 图例与清单区段各一次）。

## 146 · arXiv:1203.0297 ＝ Tonry et al., "The Pan-STARRS1 Photometric System" (2012) —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `1203.0297v1`，Title `The Pan-STARRS1 Photometric System`，first author `J. L. Tonry`，发布 2012-02-29，`journal_ref: ApJ`，`doi: 10.1088/0004-637X/750/2/99`。
- **关联性：关联对**。`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:137` 的 [B29]「Tonry, J. L., Stubbs, C. W., Lykke, K. R., et al. 2012, "The Pan-STARRS1 Photometric System" arXiv:1203.0297 [V]」在 **`**定标 / 合成测光 / 绝对通量**`** 小节（:125）之下，并被 §1.1（:15）作 `ZP + c·(color) + s(x,y)` 零点/色项惯例的并列引件之一；`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:120`（7.4）进一步写「Tonry et al. 2012 (PS1 测光系统), ApJ 750, 99. DOI 10.1088/0004-637X/750/2/99 | 从仪器响应函数定义巡天测光系统…」⇒ 卷页/DOI 与官方 API 的 doi 字段**一致**，角色（定义测光系统/通带与零点）与该文相符。
- **版本：版本对**。2012 提交、正式刊 ApJ 750, 99（2012）；[B29] 只钉 arXiv+年、7.4 钉期刊卷页，两处互不矛盾。
- 用量：网络 1／仓库读 2。

## 152 · arXiv:1802.10157 ＝ Melchior et al., scarlet, A&Comp 24, 129–142 —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `1802.10157v2`，Title `SCARLET: Source separation in multi-band images by Constrained Matrix Factorization`，first author `Peter Melchior`，2018-02-27。Crossref `10.1016/j.ascom.2018.07.001` → Title `scarlet: Source separation in multi-band images by Constrained Matrix Factorization`；Authors `P. Melchior, F. Moolekamp, M. Jerdee, R. Armstrong, A.-L. Sun, J. Bosch, R. Lupton`；Container `Astronomy and Computing`；Volume **24**；Pages **129-142**；Year **2018**。
- **关联性：关联对（含两处逐字摘录，全部命中）**。`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:92`（5.4）引摘要两句：「describes the observed scene as a mixture of components with compact spatial support and uniform spectra over their support」与「derive[s] the treatment of correlated noise and convolutions with band-dependent point spread functions, rendering our approach applicable to coadded images observed under variable seeing conditions」⇒ 与官方 API 回包的 v2 摘要**逐字一致**（仓内 `derive[s]` 是明示的括注变形，非改写）。其派给的角色是「**相关噪声处理**是可引用的、在 coadd 相关噪声下正确测量的公开示范」——正是摘要那句所做的事，不是越权。
- **版本：版本对**。A&Comp 24, 129（2018）＋ DOI ＋ arXiv 三项与两条官方记录一致；仓内未声称节号/式号（该条只摘摘要），无伪托节号风险。
- 用量：网络 2／仓库读 1。

## 158 · arXiv:2206.01007（测光定标方法综述）—— 核验态：已核（作者串有误）
- **存在性：已核**。arXiv 官方 API → `2206.01007v1`，Title `Photometric calibration methods for wide-field photometric surveys`；**Authors: Bowen Huang, Kai Xiao, Haibo Yuan**；primary `astro-ph.IM`；Published 2022-06-02；Journal 字段 `Scientia Sinica Physica, Mechanica & Astronomica (Chinese)`；摘要首句 "Uniform and accurate photometric calibration plays an important role in the current and next-generation wide-field imaging surveys."
- **关联性：关联对（主题支撑成立）**。`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:194` 的 [B59] 挂在 **`**定标 / Gaia XP / 绝对通量（续）**`**（:186）下，写作「Huang, Y., Xiao, K. & Yuan, H. 2022 (photometric calibration review) arXiv:2206.01007 [S]」⇒ 该文**确为**宽视场测光定标方法综述，主题与位点一致。
- **订正（书目串与官方记录不符）**：仓内第一作者写 **Huang, Y.**，官方回包第一作者是 **Bowen Huang（Huang, B.）**。本仓 §2（`:123`）对 [S] 的自述是「**以同样方式核对**」，这一处被证伪 ⇒ 属高危形态⑤「作者年组合」变体（首字母错），非主题错绑。
- **版本：版本对**（就 arXiv:2206.01007v1 / 2022 而言）。**附注**：正式刊为《中国科学》系列（中文），仓内未钉其卷页；引用若需权威版次，应补该刊卷页或明确声明引 arXiv 版。
- 用量：网络 2（同一 id 查两次，两次回包逐字段一致）／仓库读 2（清单区段 ＋ §2 图例区段）。

## 164 · arXiv:2406.06850 ＝ Castander et al., PAU 窄带测光定标 —— 核验态：已核
- **存在性：已核**。arXiv 官方 API → `2406.06850v1`，Title `The PAU Survey: Photometric Calibration of Narrow Band Images`；first author `F. J. Castander`，共 23 位作者；primary `astro-ph.IM`；Published 2024-06-10；Journal 字段 `MNRAS (accepted)`；无 DOI 字段。
- **关联性：关联对**。`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:196` 的 [B61]「Castander, F. J., et al. 2024 (PAU 窄带) arXiv:2406.06850 [S]」挂在 **`**定标 / Gaia XP / 绝对通量（续）**`** 下；摘要首句（官方回包）"The Physics of the Accelerating Universe (PAU) camera is an optical narrow band and broad band imaging instrument mounted at the prime focus of the William Herschel Telescope." ⇒ 题名即为窄带像测光定标，"PAU 窄带"标签与年份 2024 均与记录一致。
- **版本：版本对（且未越钉）**。仓内只钉 arXiv＋2024，未声称 MNRAS 卷页；官方回包的 journal_ref 恰为 `MNRAS (accepted)`（尚未给卷页）⇒ 不存在"只有 v2/v3 才有被引内容"或"卷页编造"风险。
- 用量：网络 1／仓库读 1。

## 170 · arXiv:astro-ph/0504244 ＝ Stritzinger et al., PASP 117, 810–822 (2005) —— 核验态：已核（题名被增写）
- **存在性：已核**。arXiv 官方 API → `astro-ph/0504244v2`，Title `An Atlas of Spectrophotometric Landolt Standard Stars`，first author `Maximilian Stritzinger`，published 2005-04-11，`journal_ref: 10.1086/431468`，`doi: 10.1086/431468`。Crossref `10.1086/431468` → Title `An Atlas of Spectrophotometric Landolt Standard Stars`；Authors `Maximilian Stritzinger, Nicholas B. Suntzeff, Mario Hamuy, Peter Challis, Ricardo Demarco, Lisa Germany, A. M. Soderberg`；Container `Publications of the Astronomical Society of the Pacific`；Volume **117**；Pages **810-822**；Year **2005**。
- **关联性：关联对（主题）**。`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:229` 的 [B88] 挂在 **`**孔径改正 / 通带颜色项 / Gaia XP 追加**`**（:226）下；该文是**分光测光标准星**星表（Landolt 星的光谱能量分布），作通带/颜色项与绝对通量参考成立；DOI 10.1086/431468 与 arXiv 号互指同一文（官方 API 的 doi 字段即该 DOI）。
- **订正（题名与两处官方记录不符）**：仓内引号内题名为「An Atlas of Spectrophotometric **Landmark/**Landolt Standard Stars」，而 Crossref 与 arXiv 的正式题名**都**是「An Atlas of Spectrophotometric Landolt Standard Stars」，**无 "Landmark/"** ⇒ 引号式逐字引用被增词。另 [B88] 未给 PASP 117, 810–822 卷页（仅靠 DOI 钉版，可接受但建议补）。
- **版本：版本对**（2005；DOI `10.1086/431468`＝PASP 117, 810-822, 2005；arXiv 侧 v2）。
- 用量：网络 2／仓库读 1。

## 176 · DOI 10.1017/S0370164600014346 ＝ Aitken, Proc. Roy. Soc. Edinburgh 55, 42–48 —— 核验态：已核
- **存在性：已核**。Crossref → Title `IV.—On Least Squares and Linear Combination of Observations`；Author `A. C. Aitken`；Container `Proceedings of the Royal Society of Edinburgh`；Volume **55**；Pages **42-48**；issued **1936**；DOI `10.1017/s0370164600014346`。
- **关联性：关联对**。`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:447`（§9.2 权重最优性）把它绑在「**逆方差最优**」的出处并列位（与 Horne 1986 §II.B 式 (7)(8)(9)、Zackay & Ofek 2017 §2.2 并列），并**自标**「（题名级核验）」⇒ Aitken 论"最小二乘与观测值的线性组合"（即加权线性组合的最小方差/效率性质）与该角色匹配；仓内没有为它伪托式号（只引卷页），诚实边界已写。
- **版本：版本对**。仓内写「Aitken **1935/36**, Proc. Roy. Soc. Edinburgh **55, 42–48**, DOI 10.1017/S0370164600014346」＝ Crossref 的 vol 55 / pp 42–48 / issued 1936；`1935/36` 是该卷的**会期年**写法（卷 55 跨 1935–1936），不构成年份漂移；DOI 已把版次钉死，无同名再版分歧。
- 附注：题名带分卷标号「IV.—」（爱丁堡皇家学会论文集的部次记号），仓内未录题名原文，故无逐字引用风险。
- 用量：网络 1／仓库读 1。

## 182 · DOI 10.1051/0004-6361/201832756 ＝ Evans et al., "Gaia Data Release 2", A&A 616, A4 —— 核验态：已核
- **存在性：已核**。Crossref → Title `Gaia Data Release 2`；First Author `D. W. Evans`；Container `Astronomy & Astrophysics`；Volume **616**；Page **A4**；issued **2018**；DOI `10.1051/0004-6361/201832756`。
- **关联性：关联对**。`实验/absolute-snr/docs/snr-propagation-design.md:1144` 作「Evans et al. 2018, A&A 616, A4, DOI 10.1051/0004-6361/201832756」并与 Riello 2021（EDR3）并列，末列警示「**版本不得混用**」；`REVERSE_VERIFY_BIBLIOGRAPHY.md:124`（7.8）给它的作用是「**若使用 DR2 而非 EDR3/DR3 作参考星表时的版本专用引用。引用实际使用的版本** | 不得在同一句里混用 DR2 与 EDR3/DR3 的定标陈述」⇒ 该文确为 **DR2** 星表文（注册题名即 `Gaia Data Release 2`），角色（版本专用引用）与内容一致；仓内亦明示本项目当前用 DR3（7.8 末列），未把它当 DR3/EDR3 用。
- **版本：版本对**。A&A 616, A4（2018）＝ DR2 专辑卷，DOI 与卷页一致；**注意同一段落（:1144）里另一件（entry 110，A37）的角色是错的**，本条只判 Evans/DR2 这件。
- 用量：网络 1／仓库读 2。

---

## 本批三态计数（18 条）

| 字段 | 已核／对 | 错 | UNPROVEN | 明细 |
|---|---|---|---|---|
| 存在性 | **17** | 不存在 **0** | **1**（134：ADS bibcode 句柄；论文本体已核） | 已核＝53 80 92 98 104 110 116 122 128 140 146 152 158 164 170 176 182 |
| 关联性 | 关联对 **16** | 关联错 **0** · **角色错绑 2** | — | 角色错绑＝53（`README.md:327` 主题列派"背景建模对弱透镜影响"）、110（把 A&A 674 A37 当"DR3 内容总括论文"，两处位点同错） |
| 版本 | 版本对 **18** | 版本错 **0** | 未钉版次 **0** | 无「只有 v2/v3 才有被引内容」「同名再版项不同」形态；134 的版本判定基于 Crossref 记录（1964/35/73-101），不依赖未证的 ADS 句柄 |

**元数据串缺陷（不影响三态，须回写台账）**：170 题名多写 "Landmark/"；158 第一作者首字母 `Huang, Y.` ≠ 官方记录 `Bowen Huang`；110 作者绑定正确但角色绑错；134 清单代表位点行号漂移（:288 → 实为 :305）。

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

1. **134 · `1964AnMS...35...73H`**：句柄本身未获任何官方回包。用过：ADS `ui.adsabs.harvard.edu/abs/1964AnMS...35...73H/abstract`（405）、`…/abs/1964AnMS...35...73H`（405）、Crossref `10.1214/aoms/1177703732`（200，只证论文）。下轮建议：**ADS API 带 token**，或 INSPIRE `https://inspirehep.net/api/literature?sort=most-cited&fields=bibcode,doi,title&q=doi:10.1214%2Faoms%2F1177703732`（镜像 ADS 句柄）。
2. **110 · A&A 674 A37 的副标题/确切主题**：Crossref 与 OpenAlex 均只登注册短题名 `Gaia Data Release 3`，ADS 405，3 次到限。下轮建议：`aanda.org` 正文页（或 DOI 解析落地页浏览器途径）取副标题。**注**：本条"总括角色错绑"的结论已成立，不受此项未解影响。
3. 边界声明（不计 UNPROVEN）：116 的**否定式**断言「不存在 2003 年 Labbé 一作的最优孔径论文」无法以有限查询证明否命题；正面部分（DOI 10.1086/346140 ＝ HDF-S/ISAAC 文）已逐字段核。

## 新发现的缺陷形态（本批独有，前面批次未登记过的）

- **㊀ 批次清单代表位点的行号漂移**：134 在钉提交上实命中 `:305`，清单写 `:288`（该行是 `## 9a` 标题）。⇒ 清单行号不可作定位凭据，判定前须按标识符 `git grep` 复核；若其他批次也按行号取证，可能整批读错断言。
- **㊁ "一行三引、逐件角色不一" 的绑法**：`snr-propagation-design.md:1144` 同一行并列 Riello 2021／Evans 2018／Drimmel 2023 三件，其中两件（110、182）指向同一位点行——若按"行"而非按"件"判，就会把对的与错的混成一个结论。⇒ 判据须显式落到「该行内这件被派的角色」，逐件出裁。
- **㊂ 书目串的"引号内题名"被增词**（170 "Landmark/Landolt"）：文、DOI、arXiv、年份全对，唯**引号式逐字题名**多出 `Landmark/`。此前登记的形态多在卷页/年份/标识符层面，这类"题名内部插词"是新面。
- **㊃ 作者姓名首字母错＋自述核验等级被证伪**（158 `Huang, Y.` vs `Bowen Huang`）：`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:123` 明写 [S] ＝「以同样方式核对」，而该处第一作者缩写与官方回包不符 ⇒ 该文件的 [V]/[S] 标签**不构成已核凭据**，须逐条重跑元数据。
- **㊄ 佐证来源表的"主题列"与文献所做的事错位**（53 `README.md:327`）：DOI/卷页/题名三件全对，但主题列写"背景建模对弱透镜/测光的影响"，该文做的是叠加期伪影剔除；**同表相邻行**（Padmanabhan+2008）才是背景建模件。⇒ 这类缺陷在"引文↔标识"层面查不出，只在"引文↔主题标签"层面才现形。
- **㊅ ADS 途径在本环境对脚本一律 405**（两种 URL 形态皆试）：⇒ 凡以 ADS bibcode 为**唯一**标识的条目（本批即 134），存在性天然无法在三查内核句柄；清单类批次应事前标注"仅 bibcode、无 DOI"的条目并预配 token 途径，否则整批只能落 UNPROVEN。
- **本批无 Janesick 家族件**：CIT-04 的 18 条标识符中**不含**探测器噪声／系统增益类文献（Janesick 2001 只出现在 `PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:170` [B40]，不属本批），故前言 §4 警示的 `+1/12` vs `g²/12` 版次之争在本批无适用对象——已逐条确认，非未查。顺带登记（**不属本批判定范围**，供持有 [B40] 的批次参考）：该行的钉法是 `Janesick 2001, "Scientific Charge-Coupled Devices", SPIE Press, DOI 10.1117/3.374903`，只钉了 2001 初版＋SPIE Press＋DOI，未涉 2007 卷或 2011 再版，故若其被引内容是量化噪声项，须由该批确认所引式在哪一版次出现。

## 覆盖率自报

- **18/18 条**全部给了三态判定，无遗留"未核"行：`覆盖率自报 = 18/18 = 100%`。
- 其中存在性带 UNPROVEN 1 条（134，仅 ADS 句柄；论文本体已核）；关联性带角色错绑 2 条（53、110）；版本 0 错。
- **用量实况（逐条相加，不美化）**：网络查询合计 **32 次**（上限 3×18＝54）；单条分布 53:2、80:2、92:1、98:3、104:2、110:**4（超限 1，已在该条自报）**、116:1、122:1、128:1、134:3、140:2、146:1、152:2、158:2、164:1、170:2、176:1、182:1。到上限 2 条（98、134），超限 1 条（110）。
- 仓库读取：对 18 条的**归因计数 27 次**，全部 ≤2 合规；实际发起的仓库读操作只有 **13 次**（同一文件区段一次取证服务多条，如 `f-instr-survey.md:20-300` 一次覆盖 92/104/116/128，`PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:128-243` 一次覆盖 140/146/158/164/170，`snr-propagation-design.md:1135-1165` 一次覆盖 110/122/182）。
- 口径限制如实登记：`实验/additive-sky-seamless/results/evidence_lit.json` 的 claim 字段未单独打开（该条 2 次仓库读取用于 `README.md` 表区与全仓 grep），故 53 的关联性判定基于 `README.md:327`（错）与 `REVIEW.md:123`（对）两处原文，第三处只核到 `identifier`＋`note` 字段。
- 取证基线核对：开工即核 `git rev-parse --short=8 HEAD` = **c8f64e9a**，且 `git status --porcelain -- 实验/ docs/` 为空 ⇒ 所有引用位点原文都取自被钉提交，非本机残留快照。
- 未跑构建、未跑门禁、未改仓库、未写共享台账 `整改/out/文献台账.csv`（结论只落本文件，由前台回写）。

<!-- PROGRESS: 18/18 -->
