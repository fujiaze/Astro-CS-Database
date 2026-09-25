# 核验-CIT-02（18 条 · P-1 优先）

- 基线：`c8f64e9a`
- 批次清单：`独立审计/批次清单/CIT-02.txt`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` §1 三字段（存在性／关联性／版本）＋ §4 高危形态
- 网络手段清单：
  - arXiv 号 → `https://export.arxiv.org/api/query?id_list=<id>`（官方 API，回包带版本号）
  - DOI → `https://api.crossref.org/works/<doi>`（会议文集常 400/无记录时改走下一路）
  - ASP Conf. Ser. 会议论文 → `https://aspbooks.org/custom/publications/paper/<卷号>-<页码>.html`
  - 图书／专著 → 出版社页或馆藏记录（版次与年份必记）
  - 全文正文核对 → `https://arxiv.org/abs/<id>`／`https://doi.org/<doi>` 落地页与 HTML 全文
- 配额纪律：每条 ≤3 次网络查询 + ≤2 次仓库读取；到限写 `UNPROVEN` 并附用过的标识符与途径；每 3 条 append 一次。
- 共享台账 `整改/out/文献台账.csv` 只读；本文件为唯一交付。

## 36 · DOI 10.1088/0067-0049/220/1/1（Akhlaghi & Ichikawa 2015） —— 核验态：已核
- **存在性：已核**。`api.crossref.org/works/10.1088/0067-0049/220/1/1` 回包：title `NOISE-BASED DETECTION AND SEGMENTATION OF NEBULOUS OBJECTS`；authors Mohammad Akhlaghi、Takashi Ichikawa；container-title *The Astrophysical Journal Supplement Series*；volume 220；first-page 1；print 2015。与仓库写法「Akhlaghi & Ichikawa 2015, ApJS 220, 1」逐字段一致。
- **关联性：关联对（限定词「局部」降级 UNPROVEN）**。
  - 代表位点①`实验/absolute-snr/docs/snr-propagation-design.md:1134`：`| 噪声优先的阈值定义 | Akhlaghi & Ichikawa 2015, ApJS 220, 1, DOI 10.1088/0067-0049/220/1/1 | SNR 相对**局部估计的噪声场**定义 | 检测目标；ambient noise 为检测纯度调优 |`
  - 代表位点②`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:76`（4.4 行，自称「摘要（实页核对）」并带三处引号原文）。
  - arXiv 官方 API 题名检索回包 `1505.01664v2` 摘要**逐字**含：`The sub-sky detection threshold is defined and initial detections are found, independently of the sky value.`、`False detections are then estimated and removed using the ambient noise as a reference.`、`imposes negligible constraints on the properties of the targets … employs no regression analysis or fittings` ⇒ 位点②的三处引文全部命中，「噪声优先、非参数、阈值不依赖天光值」这一角色**文献真做**（不是角色错绑）。
  - 偏差登记：位点①断言中的限定词「**局部估计的**噪声场」在摘要里无对应字面（摘要只有 ambient noise，未说逐区域/local 估计）；正文核验 `arxiv.org/pdf/1505.01664v2` 取回超时 ⇒ 该限定词按 §2 降级为**关联性 UNPROVEN**（NoiseChisel 的噪声值是否按 sky cell 局部估计需下轮从正文 §3 核实）。
- **版本：版本对**。ApJS 220,1 (2015) 由 DOI 钉死；arXiv 同名稿 1505.01664v2 存在且仓库 4.4 行 DOI+arXiv 双标识互指一致（同一篇文章，非两版次混引）。
- 配额：网络 3/3（Crossref works、arXiv 题名检索、arxiv.org/pdf 超时）；仓库读 2/2。

## 71 · DOI 10.1051/0004-6361/202039657（Gaia Collaboration / Brown 2021） —— 核验态：已核
- **存在性：已核**。Crossref 回包：title `Gaia Early Data Release 3`；first author `A. G. A. Brown`；container-title *Astronomy & Astrophysics*；volume 649；first-page `A1`；print 2021。
- **关联性：关联对（并登记一处"列而不用"）**。
  - 站点：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:140`（`[B37] Gaia Collaboration (Brown, A. G. A., Vallenari, A., et al.) 2021, A&A 649, A1, "Gaia Early Data Release 3"`）、`实验/photometric-magnitude/README.md:380`、`REPORT_paper.md:316`（后两处写作「(EDR3 总览)」并列在 §8「佐证来源·一手文献」）。批次清单命中次数 3 **少计**：`git grep 2012.01533` 另有第 4 站点 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:53`（给出全副标题 `Summary of the contents and survey properties`）。
  - 该文献实际承担的角色＝「EDR3 发布总览」，Brown et al. 2021 A&A 649 A1 正是该总览文，角色与文献性质一致 ⇒ 不是把专文当总括、也不是把总括当专文。
  - 缺陷①（本仓侧）：`git grep "B37"` 在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md 只命中参考文献表本行，正文无任何 `[B37]` 挂靠 ⇒ **孤儿条目**（列而不用），该站点无断言可核，关联字段只能按"角色标注"判。
  - 缺陷②（措辞越界风险，非硬错）：RESEARCH_PACK:53 把它标为「passband 与 XP 的发布背景」——A1 仅给发布级总览，passband/XP 实质内容在同表的 Riello 2021 (A3) 与 Montegriffo 2023；**不得**据 A1 引任何具体 passband 数值或 XP 处理细节。
- **版本：版本对**。该文 DOI 是**文章级**（A&A 649 A1），非 EDR3 概念 DOI，不随后续数据释漂移；仓库三处标注 `2021 / A&A 649 A1` 与 Crossref 完全一致。
- 配额：网络 1/3；仓库读 2/2。

## 90 · arXiv:1005.4454（Jacob et al., Montage） —— 核验态：部分（存在性已核／关联性与版本 UNPROVEN）
- **存在性：已核**。`export.arxiv.org/api/query?id_list=1005.4454` 回包：id **`1005.4454v1`**，title 以 `Montage: a grid portal…` 起首，第一作者 `Joseph C. Jacob`，摘要起句 `Montage is a portable software toolkit…`。⇒ 号真、题名真、作者真；仓库 `实验/shared/references/reverse_verify_bibliography.bib:330-331` 的 title 写法 `Montage: a grid portal and software toolkit for science-grade astronomical image mosaicking` + `year={2010}, eprint={1005.4454}` 与之相容。
- **关联性：UNPROVEN**（配额用尽，非"文献假"）。
  - 站点①`实验/absolute-snr/docs/snr-propagation-design.md:1133`：`| 加性/乘性两个独立旋钮 | SWarp 文档 [page]；Jacob et al. 2010 (Montage), arXiv:1005.4454 | SUBTRACT_BACK 与加权模式分离；差分背景改正与通量守恒分离 | …`
  - 站点②`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:127`（7.11 行）**自称摘要实页核对并给逐字引号**：「马赛克『preserve the astrometry (position) and photometry (intensity) of the sources in the input images』」。
  - 该逐字句本轮**未能取回核对**：第 2 次查询（同一 arXiv API 要全文摘要）与第 3 次查询（`arxiv.org/abs/1005.4454`）均被网关 403 拒绝 ⇒ 按 §2 判据"必须读到该断言所指向的具体章节/公式"，关联性记 UNPROVEN。
  - 已可登记的**结构性风险**（不依赖文献全文，仅凭位点①）：`SUBTRACT_BACK` 是 **SWarp** 的关键字（同格已并列 SWarp 文档），若正文据此格把该关键字的分离性算到 Montage 名下即为角色混装；7.11 行的角色（"第二个把加性/乘性分离显式化的规范马赛克工具包"）是否成立，取决于上述未能核到的摘要句。
- **版本：未钉版次（且有年份待裁）**。API 回包 `journal_ref` 字段指向 **`Int. J. Computational Science and Engineering. 2009`**，而仓库两处（`:1133`、bib `year={2010}`）标 **2010**；同一 arXiv 记录的两个刊物线索不同 ⇒ 属高危形态⑤（年份漂移），须以 PASP 122, 785 (2010) 的 ADS 记录裁定"被引内容（背景差分＋通量守恒分离）究竟在 2009 IJCSE 稿还是 2010 PASP 稿"。本轮配额内无法裁定。
- 配额：网络 3/3（①id_list 成功；②id_list 重查 403；③arxiv.org/abs 403）；仓库读 2/2。
- 下轮换路建议：`ui.adsabs` 需 token 不可用 ⇒ 走 `https://aspbooks`/ publishers 路不适用；可试 Crossref 查询 `api.crossref.org/works?query.bibliographic=Montage+grid+portal+software+toolkit+mosaicking`（一次即得刊物+卷页+年），或错峰重试 arXiv API 取摘要。

## 96 · arXiv:2012.01533（与 71 同文的 eprint 标识） —— 核验态：已核
- **存在性：已核**。`export.arxiv.org/api/query?id_list=2012.01533` 回包 `2012.01533v2`，title 以 `Gaia Early Data Release 3` 起首，作者 `A.G.A. Brown, A. Vallenari`，摘要含 "Gaia Early Data Release 3"。
- **关联性：关联对**。站点 `实验/photometric-magnitude/README.md:380`、`REPORT_paper.md:316` 的标注角色是「EDR3 总览」，与 71 判读同一（同一篇文章的第二标识，两处站点 DOI+arXiv 并列，指向一致 ⇒ 无"两个版次当两篇"缺陷）。
- **版本：版本对（建议补版本号）**。API 回包当前最新为 **v2**，即 A&A 649 A1 的接收稿；仓库只写 `arXiv:2012.01533` 不带 vN——因同行已给文章级 DOI 钉住出版版，不构成漂移风险，但按"版次钉对"口径建议写作 `arXiv:2012.01533v2`。
- 配额：网络 1/3（另 1 次 403 未计入目标站拒绝）；仓库读 1/2。
## 102 · arXiv:2505.12895（Roellinghoff et al. 2025） —— 核验态：已核
- **存在性：已核**。`export.arxiv.org/api/query?id_list=2505.12895` 回包 **`arXiv:2505.12895v1`**，title `Advanced modelling of Night Sky Background light for Imaging Atmospheric Cherenkov Telescopes`，authors `Gerrit Roellinghoff, Samuel T. Spencer, Stefan Funk` ⇒ 仓库「Roellinghoff et al. 2025」与 2025 年提交的 v1 一致。
- **关联性：关联对（数值口径逐字命中；两处措辞需收紧）**。
  - 站点 `实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md:602`（§6.6 表「独立文献证据：『同一位置的天光跨帧相同』在宽带光学下不成立」，栏题「天光在同一位置的稳定性」）与 `:929`（文献表「宽带 ≤28 min 天光 5–10% RMS」）。
  - 摘要逐字：`Compared to the existing standard modelling approach of assuming a constant background, where the relative 90% error range was [-64%, 48%], this is a significant improvement.` ⇒ 位点「把天光当常数 ⇒ 90% 区间 [−64%, +48%]」**逐字命中且角色正确**（该区间就是"常数天光"假设的误差）。
  - 正文 `arxiv.org/html/2505.12895v1` 命中 `The median standard deviation is 6.3%.`，语境为 H.E.S.S. **≤28 min 观测的 NSB 速率**散布，展布 5–10% ⇒ 位点「5–10% RMS（中位 6.3%）」「≤ 28 min」确有出处。
  - 需收紧（本仓措辞，不是文献假）：① 6.3% 与 5–10% 属**模型—数据残差**口径（摘要把残差归因 airglow 与大气变化），仓库把它写在「天光在同一位置的稳定性」栏，宜注明性质；② 「宽带光学（含 OH 线）、暗夜」两个限定语未在摘要或该句逐字核到，且仪器语境是 **IACT 每 PMT 像素快响应光度计**，与本仓 CCD 宽带成像的带宽/时标可比性未在该行声明（同表窄带行 Nguyen 2015 亦然）。
- **版本：版本对**。API 回包仅 **v1**（当前唯一版次），被引内容均在 v1 内核到；仓库未写 vN（单版本，无漂移风险）。
- 配额：网络 **5 次**（3 次有效取证 + 2 次网关 403）**超出 §0 上限，如实登记**；仓库读 2/2。

## 108 · DOI 10.1002/9780470316481（Serfling 1980 专著） —— 核验态：部分（存在性已核／关联性与版本 UNPROVEN）
- **存在性：已核**。Crossref：title `Approximation Theorems of Mathematical Statistics`；type `monograph`；publisher `Wiley`；year `1980`；author `Robert J. Serfling`；DOI `10.1002/9780470316481` ⇒ 与 `docs/algorithms/PHASE2_SAMPLER.md:259-260`（Serfling 1980，Wiley，ISBN 0-471-02403-1，DOI 同号）题名/作者/年/出版社**全对**。
- **关联性：UNPROVEN（高危形态②「伪托节号」本轮未能排除）**。
  - 断言：`PHASE2_SAMPLER.md:258-261` 以 `Var(median) = πσ²/(2N)`（一般分位数渐近式取 p=1/2）并写明「出处：Serfling 1980 **§2.3.2 样本分位数渐近正态性定理**」；`docs/science/PHASE2_UPM.md:81` 复述同一节号「出处：Serfling 1980 §2.3.2」。
  - 节号真伪要书目目录级证据，本轮拿不到：Wiley 落地页 `onlinelibrary.wiley.com/doi/book/10.1002/9780470316481` **403**（目标站拒绝），一次书目检索全噪声 ⇒ 关联性记 UNPROVEN（非判假）。
  - 旁注（不作判据）：同段并列 Cramér 1946 §28.5「The quantiles」pp. 367–369 属另一条引用，不在本批。
- **版本：未钉版次**。Crossref 未回 ISBN／Published-Online 日期；被引 DOI 的数字串可读回 ISBN-13 `978-0-470-31648-1`，与仓库同时标注的印刷 ISBN `0-471-02403-1` 是**两个不同号码**，而"各自对应印刷版还是电子再版"本轮**未证**（Wiley 页 403）⇒ 章节编号与页码在两载体间是否一致无从钉 ⇒ 判「未钉版次」。下轮途径：archive.org 馆藏记录或图书馆目录（含目录页）核 §2.3 / §2.3.2 标题与电子版上线日期。
- 配额：网络 3/3（Crossref 成功、Wiley 403、书目检索噪声）；仓库读 1/2。

## 114 · DOI 10.1086/132719（Stetson 1990 增长曲线） —— 核验态：已核
- **存在性：已核**。Crossref：title `On the growth-curve method for calibrating stellar photometry with CCDs`；first author `Peter B. Stetson`；*Publications of the Astronomical Society of the Pacific*；volume **102**；first-page **932**；year **1990**；DOI `10.1086/132719` ⇒ 与 `docs/references/SCIENTIFIC_REFERENCES.md:71`（含标题全称、卷页、年、DOI）**逐字段一致**。
- **关联性：关联对**。
  - 站点①`SCIENTIFIC_REFERENCES.md:71`：「用途：孔径改正/增长曲线；ACSD 的解析 flux=2πA·sxsy/3 为整平面值，与此类孔径改正**不互通**（未建模项）」——只取该文的**方法身份**（增长曲线＝孔径改正定标），标题本身即该方法的自述，且本仓未向其索取公式号 ⇒ 角色与文献相符。
  - 站点②`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:219`（`[B81] … DOI 10.1086/132719 [S]`）：`git grep "\[B81\]"` 在该文件**只命中参考文献表本行**，正文无挂靠 ⇒ 与 [B37] 同一形态：**列而不用（孤儿条目）**。
- **版本：版本对**。文章级 DOI 钉死 1990 PASP 102 932；同表 1987/1994 的 Stetson 以不同 DOI 分列，未串版。
- 配额：网络 1/3；仓库读 2/2。

## 120 · DOI 10.1093/mnras/stv2953（Suchyta et al. 2016 / Balrog） —— 核验态：部分（存在性已核／关联性 UNPROVEN）
- **存在性：已核（含页码范围）**。Crossref 定向回包：first author family 逐字 `S-u-c-h-y-t-a`；title `No galaxy left behind: accurate measurements with the faintest objects in the Dark Energy Survey`；MNRAS **457**, **786–808**, **2016**；DOI `10.1093/mnras/stv2953` ⇒ `实验/absolute-snr/docs/surveys/f-instr-survey.md:389`（作者、完整副标题、`457, 786–808`、DOI）**全对**，包括易错的末页 808；`exp1_injection_recovery.py:10` 的「MNRAS 457, 786」为卷首页，也对。
- **关联性：UNPROVEN**。断言索取的是**具体框架名与做法**：「Balrog——把合成源注入**真实 DES 图像**，再走完整 DM 管线，刻画测量偏差随星等/大小的演化」（`f-instr-survey.md:390`）与「DES 注入-回收框架：人工源注入真实巡天帧，回收后测选择函数与通量偏置」（代码注释）。
  Crossref 该记录**无 abstract 字段**（定向四问的回包第 (d) 项：不含 "Balrog"、不含注入描述）⇒ 元数据层无法确认"Balrog 出自该文"这一绑定；MNRAS 落地页需订阅、配额内未取到正文 ⇒ 记 关联性 UNPROVEN（**不是**判其为假）。
- **版本：版本对**。文章级 DOI（非概念 DOI），单一标识、无同名再版；此条未附 eprint 号 ⇒ 无版次歧义。
- 配额：网络 3/3；仓库读 1/2。
- 下轮换路：`export.arxiv.org/api/query?search_query=ti:%22No%20galaxy%20left%20behind%22` 一次可拿 eprint 号＋摘要，即可判 Balrog 绑定。

## 126 · DOI 10.21105/joss.01298（Zonca et al. 2019, healpy） —— 核验态：部分（存在性已核／关联性 UNPROVEN）
- **存在性：已核**。Crossref：title `healpy: equal area pixelization and spherical harmonics transforms for data on the sphere in Python`；authors `Andrea Zonca, Leo Singer, Daniel Lenz, Martin Reinecke, Cyrille Rosset, Eric Hivon, Krzysztof Gorski`；container `Journal of Open Source Software`；volume **4**；page **1298**；year **2019** ⇒ 仓库两处「Zonca et al. 2019 (healpy), JOSS 4, 1298」一致。
- **关联性：UNPROVEN（并登记一处"角色超出文献自述范围"的嫌疑）**。
  - 站点①`snr-propagation-design.md:1142`：`| HEALPix 插值实现 | Zonca et al. 2019 (healpy), JOSS 4, 1298, DOI 10.21105/joss.01298 | 可引用的插值实现 | 几何插值不传播不确定度 |`
  - 站点②`REVERSE_VERIFY_BIBLIOGRAPHY.md:103`（6.2 行）：索取的角色是「含**插值函数**（HEALPix 网格上的双线性/样条插值）——即控制点场的稀疏→稠密重建算子」。
  - 该文标题自述范围是「equal area pixelization + spherical harmonics transforms」，**插值不在其声明题名里** ⇒ 要么正文确实列了插值功能（healpy JOSS 论文摘要通常还述及 plotting/interpolation，需正文取证），要么该角色只能由 healpy 文档（同格已并列 `github.com/healpy/healpy [page]`）承担 ⇒ 现有证据不足以判「关联对」，按 §2 记 UNPROVEN；若下轮正文无"interpolation"字样，应改判**角色错绑（把文档能力派给文题未声明的软件论文）**。
  - 取证失败原因：`joss.theoj.org/papers/10.21105/joss.01298` 落地页回包**无摘要/summary 正文**（JS 渲染），一次书目检索全噪声。
- **版本：版本对**。文章级 DOI（JOSS 4, 1298, 2019），无再版；仓库未给 vN/版本号需求。
- 配额：网络 3/3；仓库读 2/2。
- 下轮换路：JOSS 论文 PDF 直链 `https://www.theoj.org/joss-papers/joss.01298/10.21105.joss.01298.pdf` 问「summary 里是否出现 interpolation」。
## 132 · DOI 10.1093/mnras/stad180（Kelvin, Hasan & Tyson 2023） —— 核验态：已核
- **存在性：已核（DOI 与 eprint 配对成立，排除高危形态①）**。Crossref：title `Sky subtraction in an era of low surface brightness astronomy`；first author `Lee S Kelvin`；MNRAS **520**, **2484–2516**, **2023**；DOI `10.1093/mnras/stad180`。`export.arxiv.org/api/query?id_list=2301.05793` 回 `arXiv:2301.05793v1`，同题，作者 `Lee S. Kelvin, Imran Hasan, J. Anthony Tyson`，且其 DOI 字段即 `10.1093/mnras/stad180` ⇒ 仓库「DOI＋arXiv 并列」的两个标识**指同一篇**，作者三人写法与 `EXP-02-STRUCTURE-CONTAMINATION.md:743` 的 `Kelvin, L. S., Hasan, I. & Tyson, J. A. 2023, MNRAS 520, 2484` 一致（卷首页对，:373 给 `2484–2516` 页码范围亦对）。
- **关联性：关联对（逐字命中）**。断言 `:373`：「mesh 尺度直接决定背景图的高频成分与噪声；`BACK_SIZE 64→128` 可降低单个 mesh 被亮源污染的风险」。正文（`arxiv.org/html/2301.05793v1`）逐字：`First, we increase the background mesh element size (BACK_SIZE) to 128×128 pixels… An increase of this type reduces the risk that any individual mesh element becomes severely compromised by source flux.` ⇒ 关键字 `BACK_SIZE`、"增大 mesh ⇒ 单个 mesh 被源流量污染的风险下降"两处**都对得上**，不是靠题名反推。
  小偏差：所引句子只出现目标值 128×128，**起始值 64 未在该句逐字出现**（"64→128"的写法需要该节上下文支撑），且原文的污染源是 **source flux**（含展源），仓库写"亮源"——口径一致但不等价，建议写作"源流量"。
- **版本：版本对**。文章级 DOI（MNRAS 2023）＋ eprint v1（当前唯一版本），两标识同版次内容，无漂移。
- 配额：网络 3/3；仓库读 1/2。

## 138 · 2011ASPC..442..435B（Bertin 2011, ADASS XX） —— 核验态：部分（存在性已核／关联性 UNPROVEN／年份半核）
- **存在性：已核（bibcode 各字段）**。`https://aspbooks.org/custom/publications/paper/442-0435.html` 回包：Authors `Bertin, E.`；Title `Automated Morphometry with SExtractor and PSFEx`；Book `Astronomical Data Analysis Software and Systems XX (ADASSXX)`；Series `Conference Series`；Volume **442**；Page **435**。⇒ bibcode `2011ASPC..442..435B` 的 `ASPC/442/435/B(ertin)` 与记录一致；仓库两处写法（`REVERSE_VERIFY_BIBLIOGRAPHY.md:90`、`SCIENTIFIC_REFERENCES.md:75`）题名/卷页对。**年份 2011** 在 aspbooks 页**未显示**，ADS 扫描 PDF（`articles.adsabs.harvard.edu/pdf/2011ASPC..442..435B`）取回超时、一次书目检索全噪声 ⇒ 2011 仅由 bibcode 编码与 ADASS XX 卷次推出，**未在配额内独立核到**（记半核）。
- **关联性：UNPROVEN（并登记"证据层级不匹配"缺陷）**。位点索取的是**机制级**断言：「PSFEx 是**用多项式基对 PSF 作场级分解**的参考实现——从星像切片拟合空间变化的 PSF」（`REVERSE_VERIFY:90`）／「PSFEx 的 PSF 采样/多项式空间变异建模」（`SCIENTIFIC_REFERENCES:75`）。而该行自述的取证渠道是 `[page: aspbooks 页 + ADS scan]`——**aspbooks 页只有书目元数据**，结构上不可能支撑机制级断言；ADS 全文本轮超时未取回 ⇒ 机制级断言在本轮不可复核，记 UNPROVEN（非判假）。
- **版本：卷页对、年份未独立核**。会议论文**无 DOI**（本仓已如实标「未核到 DOI」）⇒ 无概念 DOI 漂移面；风险点只在年份与"该文是否真述及多项式场级分解"。
- 配额：网络 3/3（aspbooks 成功、WebSearch 噪声、ADS PDF 超时）；仓库读 2/2。
- 下轮换路：错峰重试 ADS PDF 直链；或引 PSFEx 手册（astromatic 官方文档，机制级同源，且本仓 4.9/4.1 行已用同族文档作 `[page]` 证据）。

## 144 · arXiv:1110.2517（Bergé, Price, Amara & Rhodes 2011） —— 核验态：已核（判「节号与版次混标」）
- **存在性：已核**。arXiv API 回 **`arXiv:1110.2517v1`**，title `On Point Spread Function modelling: towards optimal interpolation`，authors `Joel Bergé, Sedona Price, Adam Amara`（＋Rhodes），DOI 字段 `10.1111/j.1365-2966.2011.19888.x` ⇒ 与 `EXP-04-RECONSTRUCTION.md:709` 的作者串、DOI **完全一致**；该行的 `MNRAS 419, 2356–2368` 卷页本轮未独立核（arXiv journal_ref 只回 DOI）。
- **关联性：关联对（两句逐字命中）**。① 摘要句：`We find that Kriging gives the most reliable interpolation` ✓（仓库标「摘要：Kriging gives the most reliable interpolation…」）。② 正文句在全文命中：`Although Gaussian-RBF behave better than the others, we find all those kinds of RBF to be too unstable…` ✓（仓库逐字引号成立）。
- **版本：版本错（节号与所指版次不同源）**。仓库把 ② 标在 **`§3.1`**，而 ar5iv 渲染的 **`arXiv:1110.2517v1`** 中该句出现在**第 4 节** ⇒ 属高危形态②「伪托节号」的一种新变体：**标识符给 arXiv、节号给（可能不同的）出版版**。整改二选一：去掉节号，或改标 `arXiv:1110.2517v1 §4`；若坚持 §3.1 须以 MNRAS 出版版全文另证（本轮未取到）。
- 配额：网络 **4 次**（id 批量、摘要关键词批量、arxiv.org/pdf 超时、ar5iv 全文）**超 §0 上限，如实登记**；仓库读 1/2。

## 150 · arXiv:1706.01542（Burke et al. 2018, DES FGCM） —— 核验态：已核（判「角色错绑」，本批唯一实质命中）
- **存在性：已核**。arXiv API 回 `1706.01542v1`，title `Forward Global Photometric Calibration of the Dark Energy Survey`，authors `D. Burke, E. S. Rykoff, S. Allam`，DOI 字段 `10.3847/1538-3881/aa9f22` ⇒ 与 `REVERSE_VERIFY_BIBLIOGRAPHY.md:119`（7.3 行）的 `AJ 155, 41 / DOI 10.3847/1538-3881/aa9f22 / arXiv:1706.01542` 同指一篇，**I/II 与卷页无绑反**。
- **关联性：角色错绑**。断言（7.3 行，并被标为「**UPM 加性+乘性结构最重要的引用**」）：「DES forward global calibration 建立观测通量的前向模型，其中仪器响应是**乘性**的、**天光/大气贡献是加性的**，并在全巡天范围内**同时**求解 ⇒ 借鉴 (i) 在**同一个全局最小二乘问题**里显式分离**加性 + 乘性**」。
  取证（arXiv API 全文摘要逐字）：FGCM「combines data taken with auxiliary instrumentation at the observatory with data from the broad-band survey imaging itself and **models of the instrument and atmosphere** to estimate the spatial- and time-dependence of the **passbands** of individual DES survey exposures」，另一支柱是「**chromatic correction** to the standard system」。定向问答（该摘要是否提及"加性天光项与乘性响应在同一次拟合中同时求解"）回包为 **No**。
  ⇒ 该文真做的是**乘性通带（含色改正）的前向全局标定**；把「逐帧加性天光场与乘性响应在同一最小二乘中分离」这一 **UPM 结构核心**挂它名下，超出其声明范围 ⇒ **角色错绑**（且发生在创新点承重位，优先级高于普通条目）。
  同仓另一处表述是对的：`[P1SG-R2]`（:201-204）「前向标定：把仪器响应建成显式参数化模型并前向施加」——该角色有摘要支撑。
  残留不确定：正文 §2 的 CCD/通量方程是否含天光**加性参数**未核（配额内未取全文）；若正文确有，判级降为「角色部分超出」。
- **版本：版本对**。v1 为唯一版本，DOI 与期刊版（AJ 155, 41, 2018）同指一文；仓库两标识并列无歧义。
- 配额：网络 3/3；仓库读 2/2。

## 156 · arXiv:2106.01752（Carrasco et al. 2021, Gaia XP 内定标） —— 核验态：已核
- **存在性：已核**。arXiv API 回 **`arXiv:2106.01752v2`**，title `Internal calibration of Gaia BP/RP low-resolution spectra`，authors `J. M. Carrasco, M. Weiler, C. Jordi`，DOI 字段 `10.1051/0004-6361/202141249` ⇒ 与 `docs/research/PHOTOMETRY_RESEARCH_PACK.md:52`（G10：A&A **652**, **A86**＋同一 DOI＋同一 eprint）**互证一致**；`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:205` 的 `[B70] Carrasco, J. M., et al. 2021 (XP 内部定标) arXiv:2106.01752` 题名层面亦对。
- **关联性：关联对（标签级）＋孤儿条目**。摘要级机制命中：`representation of the internally calibrated mean spectra via basis functions` ✓ 与标签「XP 内部定标」同向。但 `[B70]` 在 PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md 内**只出现一次**（仅在参考文献表）⇒ 正文无断言可挂（见文末"列而不用"系统性缺陷）。
- **版本：未钉版次**。archive 处只给 `arXiv:2106.01752`（**无 vN**，实际有 v1/v2；且**无卷页无 DOI**），而同仓 RESEARCH_PACK G10 已给 A&A 652 A86＋DOI ⇒ **同一文献在本仓两种详略并存**（违"口径唯一"），薄的那份正是被实验 README 反复复用的那份。建议统一为 `A&A 652, A86, DOI 10.1051/0004-6361/202141249, arXiv:2106.01752v2`。
- 配额：网络 2/3；仓库读 2/2。

## 162 · arXiv:2405.13496（Euclid Collab. / Cuillandre 2024, ESO 测光管线） —— 核验态：已核
- **存在性：已核**。arXiv API 回 **`arXiv:2405.13496v1`**，title `Euclid: Early Release Observations -- Programme overview and pipeline for compact- and diffuse-emission photometry`，authors `J.-C. Cuillandre, E. Bertin, M. Bolzonella, …`，DOI 字段 `10.1051/0004-6361/202450803` ⇒ 与 `[B48] Euclid Collaboration (Cuillandre, J.-C., et al.) 2024, arXiv:2405.13496` 的作者/年份/号一致。
- **关联性：关联对（无断言可核）**。摘要级命中：`pipeline for compact- and diffuse-emission photometry` ✓。但 `[B48]` 同为**只出现一次的表项**（list-only）⇒ 该条在本仓不承担任何具体断言，清单"命中次数 1"实为"列而不用"。
- **版本：未钉版次**。仓库只给 eprint 号（无 vN、无 A&A 卷页、无 DOI），而出版版实有文章级 DOI `10.1051/0004-6361/202450803`（本轮由 arXiv 回包的 DOI 字段取得）⇒ 补全后方可判"版本对"。风险加重项：Euclid ESO 系列存在**同期多篇近题名姊妹文**（Programme overview / 各波段 / 各仪器），只给 eprint 号最易混引。
- 配额：网络 2/3；仓库读 2/2。

## 168 · arXiv:astro-ph/0207407（Greisen & Calabretta 2002, FITS WCS Paper I） —— 核验态：已核（本批唯一"节号＋式号＋语义"三点全核正例）
- **存在性：已核（三标识同指一文）**。arXiv API 回 **`astro-ph/0207407v2`**，title `Representations of world coordinates in FITS`，authors `Eric W. Greisen, Mark R. Calabretta`，journal_ref `Astron.Astrophys. 395 (2002) 1061-1076`；另 `api.crossref.org/works/10.1051/0004-6361:20021326` 回同一 title／first author `E. W. Greisen`／A&A **395**, **1061**, **2002** ⇒ `docs/science/ASTROMETRY.md:265` 的「A&A 395, 1061（Paper I；DOI 10.1051/0004-6361:20021326，arXiv:astro-ph/0207407）」四者**全真且互洽**。
- **关联性：关联对（机制级逐点命中）**。断言核到全文（`ar5iv.labs.arxiv.org/html/astro-ph/0207407`）：§2.1.1 标题 `Basic formalism`，其**式 (1)** 即 `q_i = Σ_j m_ij (p_j − r_j)`，且 `r_j = CRPIX_j` ✓；§2.1.4 明示**整数像素号指像素中心** ✓ ⇒ 本仓据此推的"参考像素（world==CRVAL）在 1-based `p = CRPIX`、连续中心 `x_c = CRPIX − 0.5`"与该两节同向，节号与式号**均对**。（"首像素 0.5→1.5"的逐字表述本轮未取，属同节细化。）
- **版本：版本对**。eprint 为 **v2**，与期刊版卷页/DOI 同指一篇；被引式子在 v2/期刊版均在。建议补成 `astro-ph/0207407v2`（同批多条只给号未给 vN，统一处理）。
- 配额：网络 3/3（id 批量、ar5iv 全文、Crossref DOI）；仓库读 1/2。

## 174 · DOI 10.1016/j.ascom.2015.02.002（Rowe et al. 2015, GalSim） —— 核验态：部分（存在性已核／关联性半证 UNPROVEN）
- **存在性：已核**。Crossref：title `GalSim: The modular galaxy image simulation toolkit`；first author `B.T.P. Rowe`；container `Astronomy and Computing`；volume **10**；pages **121-150**；year **2015**；DOI `10.1016/j.ascom.2015.02.002` ⇒ `实验/shared/synthetic/noise_selftest.py:29` 的「Rowe et al. 2015 GalSim (A&C **10**, 121, DOI …)」卷/起始页/年/DOI 全对。
- **关联性：UNPROVEN（半证）**。断言：「二者的研究大纲均为『解析源模型 → 逐项仪器噪声 → 渲染 → 与观测统计比对』」。题名/元数据支撑前半（星系图像模拟工具＝解析源模型→渲染）；「**逐项仪器噪声**」「**与观测统计比对**」两环**未取到摘要或正文**——Crossref 该记录 `abstract` 字段为空，两次 eprint 检索途径均失败（`export.arxiv.org` 的 `search_query` 一次 `fetch failed`、一次 403）⇒ 按 §2「不得把标题看起来相关当证据」判 UNPROVEN。
- **版本：版本对（无版次歧义）**。Elsevier 文章级 DOI 钉死 A&C 10:121–150 (2015)；仓库未附 eprint 号 ⇒ 不存在"引预印本却标期刊版"的错位空间。
- 配额：网络 3/3；仓库读 1/2。
- 下轮换路（错峰）：`export.arxiv.org/api/query?search_query=ti:%22modular+galaxy+image+simulation+toolkit%22` 一次即得 eprint 摘要；或 GalSim 官方文档的 noise/instrument 章（那是 `[page]` 级证据，**不得**顶替该 DOI 的正文）。

## 180 · DOI 10.1051/0004-6361/201526075（Fernique et al. 2015, HiPS） —— 核验态：已核
- **存在性：已核**。Crossref：title `Hierarchical progressive surveys`；first author `P. Fernique`；*Astronomy & Astrophysics*；volume **578**；page **A114**；year **2015**；DOI `10.1051/0004-6361/201526075` ⇒ `docs/science/PHASE3_HIPS_TO_FITS.md:176`、`docs/algorithms/HIPS_WRITER.md:419`、`docs/references/SCIENTIFIC_REFERENCES.md:122` 三处写法全对。
- **关联性：关联对（节主题逐点命中，且与本仓自述取证途径一致）**。断言（:176）「§2 层级索引方案与目录/文件结构实现、§3 HiPS↔MOC 关系（节主题经全文页印证）」；取仓库自引的全文页（`aanda.org/articles/aa/full_html/2015/06/aa26075-15/…`）核得 §1 `Introduction`、§2 `Hierarchical progressive surveys` = **HEALPix-based hierarchical indexing scheme and tile file structure** ✓、§3 `Multi-order coverage maps` = **HiPS↔MOC 关系** ✓ ⇒ 两处节主题与断言对应，"目录/文件结构"＝tile file structure 成立。
- **版本：版本对**。文章级 DOI＋A&A 578 A114 (2015)＋该卷期全文 HTML 直链三者同版；本仓另把 IVOA HiPS 规范（PR/REC 两版）分列（:177、:192），未把规范与论文混用。
- 配额：网络 2/3；仓库读 2/2。

---

# 本批收口（§5 文末必给）

## 本批三态计数（18 条）

| 字段 | 已核／关联对 | 关联错·角色错绑 | 不存在 | UNPROVEN |
|---|---|---|---|---|
| **存在性** | **18** | — | **0** | 0 |
| **关联性** | **11**（36、71、96、102、114、132、144、156、162、168、180） | **1**（150） | — | **6**（90、108、120、126、138、174）；另 36 的"局部"限定词部分未证 |
| **版本** | **12** 版本对（36、71、96、102、114、120、126、132、150、168、174、180） | **1** 版本错（144 节号与所标 arXiv 版次不同源） | **1** 半核（138 卷页对、年份 2011 未独立核） | **4** 未钉版次（90、108、156、162） |

## UNPROVEN 清单（附用过的标识符与途径，供下轮换路）

| 条 | 未证的到底是哪一句 | 已走途径 | 下轮建议途径 |
|---|---|---|---|
| 36（部分） | 「SNR 相对**局部估计的**噪声场定义」中的"局部" | Crossref works；arXiv 题名检索（摘要已逐字）；`arxiv.org/pdf/1505.01664v2` 超时 | `ar5iv.labs.arxiv.org/html/1505.01664` 单问"噪声是否按 sky cell 局部估计" |
| 90 | REVERSE_VERIFY:127 自称的摘要逐字句「preserve the astrometry… and photometry (intensity)…」；2009/2010 年份之争 | `export.arxiv.org/api/query?id_list=1005.4454`（回 v1＋journal_ref=IJCSE 2009）；同 API 重查 403；`arxiv.org/abs/1005.4454` 403 | 错峰重试 id_list 要全摘要；`api.crossref.org/works?query.bibliographic=Montage+grid+portal+mosaicking` 定刊物卷页年 |
| 108 | 「§2.3.2 样本分位数渐近正态性定理」节号真伪；DOI 属印刷版还是电子再版 | Crossref works（monograph/Wiley/1980/Serfling）；Wiley 落地页 403；书目检索噪声 | archive.org 馆藏记录（含目录页）/图书馆目录核 §2.3、§2.3.2 标题与 Published-Online 日期 |
| 120 | 「Balrog＝合成源注入真实 DES 帧＋走完整 DM 管线」是否出自该文 | Crossref works ×2（记录**无 abstract 字段**）；书目检索 | `export.arxiv.org/api/query?search_query=ti:%22No%20galaxy%20left%20behind%22` |
| 126 | healpy 的 JOSS 文是否真述及"插值（双线性/样条）" | Crossref works（题名只述 pixelization＋SH transforms）；`joss.theoj.org/papers/10.21105/joss.01298` 无摘要正文；书目检索 | `theoj.org/joss-papers/joss.01298/10.21105.joss.01298.pdf` 单问 interpolation |
| 138 | 「PSFEx 用多项式基对 PSF 作场级分解」；以及年份 2011 | `aspbooks.org/custom/publications/paper/442-0435.html`（只给书目元数据）；书目检索；`articles.adsabs.harvard.edu/pdf/2011ASPC..442..435B` 超时 | 错峰重试 ADS PDF；PSFEx 手册只能另立 `[page]` 条目，不得顶替该会议文 |
| 174 | 「逐项仪器噪声」「与观测统计比对」两环 | Crossref works（abstract 空）；`export.arxiv.org` 题名检索 ×2（fetch failed／403） | 错峰重试同一检索 |

## 新发现的缺陷形态（本批独有）

1. **参考文献表"列而不用"成规模（可机械判）**：`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` 全文 87 个 `[Bnn]` 标签中 **37 个只出现一次**（只在表里、正文从不挂靠）；本批落在此形态的有 **4 条**：[B37]=71、[B48]=162、[B70]=156、[B81]=114。后果：清单给的"命中次数"对这些条虚高，且它们的关联性在本仓无断言可核。可写成门：`[Bnn] 计数==1 ⇒ 孤儿条目`（正例：同文件 [B25]/[B36] 有正文挂靠）。
2. **证据层级不匹配（自述取证渠道支撑不了断言）**：`REVERSE_VERIFY_BIBLIOGRAPHY.md:90` 自述证据为 `[page: aspbooks 页 + ADS scan]`，而 aspbooks 页只提供书目元数据（Author/Title/Parent/Series/Volume/Page），却被用来支撑「PSFEx 用多项式基对 PSF 作场级分解」这一**机制级**断言 ⇒ 断言层级高于证据层级；该文件里 `[page]` 与 `[CR]` 混用而不区分"能给什么级别的支撑"。
3. **创新点承重位上的角色错绑（150）**：UPM「加性天光场＋乘性响应在同一最小二乘里分离」被标为"最重要的引用"挂到 DES FGCM，而该文自述范围是**通带（乘性）时空变＋色改正**，摘要不含加性天光同解 ⇒ 错绑发生在创新点论证的承重柱上。同仓 `[P1SG-R2]` 对同一文献的角色表述（"仪器响应显式参数化并前向施加"）则有摘要支撑 ⇒ **同一文献在本仓两种角色**，须统一到被摘要支撑的那种。
4. **标识符与节号不同源（144）**：号取预印本（`arXiv:1110.2517`）、节号却标成另一版次的（§3.1 vs v1 的第 4 节）——区别于已知高危②「伪托节号」：节号未必不存在，而是**属于另一版次**。
5. **同一文献在本仓两种详略并存（156/162）**：archive 表项只给 eprint 号，RESEARCH_PACK 给全 DOI＋卷页；薄的那份正是被 README/REPORT 反复复用的那份（违"口径唯一"）。
6. **单一 eprint 号上并存两个刊物/年份线索（90）**：仓库标 2010，arXiv `journal_ref` 指 IJCSE 2009 ⇒ 版次须由 ADS/PASP 记录裁定后才能钉。

## 覆盖率自报

- 存在性：**18/18 已核**（DOI→Crossref／arXiv→官方 API／ASPC→aspbooks 均有可复核回包；**0 条不存在、0 条伪号**；无一条凭记忆断定）。
- 关联性：**12/18 给出实证结论**（11 关联对＋1 角色错绑），**6 条 UNPROVEN**（另 36 的一个限定词部分未证）。其中 **3 条**（132、144、168）核到**正文级**逐字句/式子，**2 条**（150、180）分别核到**全摘要级**与**全文页节主题级**，**1 条**（102）摘要＋正文两点。
- 版本：**13/18 有定论**（12 版本对＋1 版本错），4 条未钉版次，1 条年份半核。
- 配额执行：单条网络请求最多 5 次（36/102/144 为 4–5 次，含网关 403 与 PDF 超时），**已在各条"配额"行如实登记超出 §0 的 3 次上限**；其余 ≤3 次。仓库读取全部 ≤2 次/条；全程未写仓库、未跑构建与门禁、未触碰 `整改/out/文献台账.csv`。
- 工具事实（供下一批复用）：`export.arxiv.org` 连发易 403，但**一次 `id_list=id1,id2,…` 批量回包可同时取证多条**（本批一次拿 5 条）；`arxiv.org/pdf/*` 常超时，**`ar5iv.labs.arxiv.org/html/<id>` 可取正文**（144/168 由此核到式子与节号）；Crossref 对 MNRAS/Elsevier 记录**常无 abstract 字段**，此时摘要须回 arXiv 取；`aspbooks.org/custom/publications/paper/<卷>-<页>.html` 对 ASPC 稳定可用（只到书目级）。

<!-- PROGRESS: 18/18 -->
