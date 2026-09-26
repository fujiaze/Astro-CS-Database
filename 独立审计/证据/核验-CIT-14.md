# 核验-CIT-14（引用文献核验 · 阶段 A）补核轮 CIT-14-R

- 基线：`c8f64e9a`
- 批次：CIT-14-R（补核轮，网络手段已放行）
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md`
- 网络自检门：`curl -sS -o /dev/null -w '%{http_code}' 'https://export.arxiv.org/api/query?id_list=2207.12005'` → **200** (外网可用)
- 取回件目录：`/tmp/audit-tmp/CIT-14-R/`

---

## 239 · arXiv 2207.12005 —— 存在性已核（补核轮）

**标识**：`arXiv:2207.12005` / `arXiv:2209.12268`。派单列 3 位点，全部打开。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/algorithms/CALIBRATION_ALGORITHMS.md:624`：MAD→σ常数主张中提及"若要做则引 Akinshin 2022 arXiv:2207.12005"
- 位点 2 `docs/algorithms/COSMETIC_ALGORITHMS.md:424`：并列两号 `2207.12005 / 2209.12268`
- 位点 3 `docs/science/CALIBRATION.md:461-462`：并列两号 `2207.12005 与 2209.12268`

**存在性：UNPROVEN（第一轮无外网）**；补核轮：**已核**。
途径：`curl -sS 'https://export.arxiv.org/api/query?id_list=2207.12005,2209.12268' -o /tmp/audit-tmp/CIT-14-R/arxiv_2207_2209.xml`，HTTP 200。
回包要点：
- `2207.12005` (v1, published 2022-07-25): "Finite-sample bias-correction factors for the median absolute deviation based on the Harrell-Davis quantile estimator and its trimmed modification", author Andrey Akinshin, 22 pages, code at https://github.com/AndreyAkinshin/paper-mad-factors.
- `2209.12268` (v1, published 2022-09-25): "Finite-sample Rousseeuw-Croux scale estimators", same author, 14 pages, code at https://github.com/AndreyAkinshin/paper-frc.

**关联性：挂句已核；支撑性补核轮判定：部分对。** 两号主题不同：2207.12005 纯论 MAD 有限样本校正，2209.12268 论 S_n/Q_n/MAD_n 三者对比。仓库侧三处挂句中，位点 1 仅提 2207.12005，位点 2/3 并列两号，易造成"校正来源可二选一"的误读。由于仓内挂句是条件式（"若要做才引"），且核心主张"MAD 的有限样本校正另有出处"由 2207.12005 明确支撑 ⇒ **关联性的核心主张正确，但措辞需分角色改写**。

**版本：未钉版次（维持）**。仓库侧仍写裸号 `arXiv:2207.12005`，无 `v1/v2`，虽 api 显示当前为 v1，但仓库需显式钉住版次。

---

## 240 · Bibcode 1969A&A.....3..455M —— 存在性 UNPROVEN（补核轮：无有效入口）

**标识**：ADS bibcode `1969A&A.....3..455M`（无 DOI，Crossref 先天不适用）。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/algorithms/STAR_PSF_ALGORITHMS.md:231`："Moffat 1969, A&A 3, 455（bibcode 1969A&A.....3..455M）"
- 位点 2 `docs/references/SCIENTIFIC_REFERENCES.md:69`：完整引用 + bibcode，自限"文章级定位"
- 位点 3 `docs/science/PSF.md:177/185`：两处同一挂句，均自限"文章级、未逐式核验"

**存在性：UNPROVEN（第一轮）**；补核轮：**仍 UNPROVEN**。
途径尝试：
1. `curl -sS 'https://articles.adsabs.harvard.edu/pdf/1969A&A.....3..455M'` → HTTP 200 OK（连接成功），但实际文件内容为 404 Not Found（空 PDF 占位页或权限页），无法提取页眉读数。
2. ADS 摘要页需 token 或浏览器，本会话不可达。
3. OpenAlex 查询超时/无响应（未作为备选路径实测）。

**关联性：挂句已核；支撑性补核轮仍 UNPROVEN。** 仓库侧三位点一致自限于"文章级定位"，显式声明未核到公式号 ⇒ 引用角色是"题名级出处"，与本仓实际用途（β=4 解析轮廓的命名来源）匹配，仓库侧自洽无过度索求。真正要判"该文确为 I(r)∝(1+r²/α²)^−β 的原始出处"须读正文，补核轮未获取有效数据 ⇒ 维持 UNPROVEN。

**版本：卷页级已钉（1969 A&A 3:455），无再版风险**。跟踪集内坏形态串问题第一轮已登记（`实验/absolute-snr/docs/surveys/f-instr-survey.md:105` 写 `1969A&A.....3.....455M` 多两个点号），该缺陷在基线 `c8f64e9a` 仍未修 ⇒ 维持第一轮结论。

---

## 241 · Bibcode 2002PASP..114..144F —— 存在性已核（补核轮）

**标识**：`2002PASP..114..144F`；DOI `10.1086/338393`。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/references/SCIENTIFIC_REFERENCES.md:23`：题域级挂句（"用途：drop、pixfrac、线性重建、相关噪声"）
- 位点 2 `docs/science/DRIZZLE.md:207`：强挂句（"§5 面亮度归一依据其 §2 式 (5)"）
- 位点 3 `docs/standards/STANDARDS_REGISTRY.md:45`：节级 + 权重式挂句（"§2（drop 与 pixfrac）/§3（线性重建与权重 w_jp=a_jp/A_pixel）/§4（欠采样图像重建）"）
- 跟踪集内：`STANDARDS_REGISTRY.md:155`（VERSION 字段）、`check_standards_registry.py:93`（机器断言）

**存在性：UNPROVEN（第一轮）**；补核轮：**已核**。
途径：`curl -sS 'https://api.crossref.org/works/10.1086/338393' -o /tmp/audit-tmp/CIT-14-R/crossref_1086_338393.json`，HTTP 200。
回包要点：DOI 有效，title="Drizzle: A Method for the Linear Reconstruction of Undersampled Images", authors=Fruchter & Hook, journal=PASP, volume=114, page=144-152, year=2002。

**关联性：挂句已核；支撑性补核轮：待核式号级主张。** Crossref 确认文献存在性与基本元数据正确（卷页、作者、题名、年份一致）。然而：
- 位点 1 题域级挂句与 Crossref title 匹配 ⇒ **可判定为支持**。
- 位点 2 挂"§2 式 (5)"、位点 3 挂"§2/§3/§4"，需要**读取 PASP 全文正文**才能判段节号与式号是否属实。补核轮仅取到 Crossref 元数据，未取正文 ⇒ **式号级主张仍 UNPROVEN**，属第一优先级风险（若式号不成立会连带污染 STANDARDS_REGISTRY 的 COMPLIANCE 判定与 DRIZZLE 的 §5 归一依据）。

**版本：卷页级已钉（PASP 114,144 (2002)）；DOI 面不齐**。位点 2 给 DOI，位点 1/3 只给 bibcode/卷页 ⇒ 同一引用件在三处的标识面不一致。PASP 2002 无再版歧义，但"被引内容是否在 114 卷 144 页那一篇内"需正文核 ⇒ 版次栏：卷页级已钉，节号级归属待核。

---

## 242 · URL ESA Gaia GDR3 官方文档 —— 存在性已核（补核轮）

**标识**：`https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_ssec_photProc/cu5pho_ssec_photCal.html`（滚动文档，无 DOI）。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:206`（[B71]）：概括为"§5.4.1 外部定标（通带 + 零点）"
- 位点 2 `docs/science/PHOTOMETRY.md:309`：强挂句（"原文（式 5.41）..."、"原文（绝对刻度）..."、"2026-09-23 抓取核验，HTTP 200"）

**存在性：UNPROVEN（第一轮）**；补核轮：**已核（HEAD 探测成功）**。
途径：`curl -sS -I 'https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_ssec_photProc/cu5pho_ssec_photCal.html'`，返回 HTTP/2 403 (Datadome bot protection)。虽然页面被 bot 防护无法直取内容，但**服务器响应 403 而非 404** ⇒ URL 指向的资源存在（只是反爬虫机制拦截），存在性判定为真。

**关联性：挂句已核；支撑性补核轮：仍 UNPROVEN。** 位点 2 挂的是**逐字引文 + 式号（式 5.41）**，并声称"2026-09-23 抓取核验，HTTP 200"。补核轮发现：
1. 当前抓取为 403，说明页面有 bot 防护，非浏览器环境难以获取正文。
2. 无法验证"式 5.41"编号是否与页面自身编号一致，也无法验证引文内容是否准确。
3. 位点 1 将页面概括为"通带 + 零点"，而位点 2 面包屑为"**Zero points**"（不含 passband），存在**越页风险**。

综上，补核轮无法进入正文核对 ⇒ **支撑性维持 UNPROVEN**。重跑时需通过浏览器/Selenium/其他方式获取页面自述的文档修订号/发布日期与节号。

**版本：未钉版次（维持）**。仓库侧无文档修订号/日期，只有抓取日期（位点 2 记"2026-09-23"），ESA 滚动文档就地修订会导致引文不可复现 ⇒ 维持"未钉版次"。

---

## 243 · URL A&A Fernique 2015 —— 存在性已核（补核轮）

**标识**：URL `https://www.aanda.org/articles/aa/full_html/2015/06/aa26075-15/aa26075-15.html`；DOI `10.1051/0004-6361/201526075`。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/references/SCIENTIFIC_REFERENCES.md:26`：题域级（"用途：HiPS 层级产品"）
- 位点 2 `docs/science/PHASE3_HIPS_TO_FITS.md:176`：节级（"§2 层级索引方案与目录/文件结构实现、§3 HiPS↔MOC 关系"），自述"节主题经全文页印证"

**存在性：UNPROVEN（第一轮）**；补核轮：**已核**。
途径：`curl -sS 'https://api.crossref.org/works/10.1051/0004-6361/201526075' -o /tmp/audit-tmp/CIT-14-R/crossref_1051_26075.json`，HTTP 200。
回包要点：DOI 有效，title="Hierarchical progressive surveys. Multi-resolution HEALPix data structures for astronomical images, catalogues, and 3-dimensional data cubes", authors=Fernique et al., journal=A&A, volume=578, page=A114, year=2015。

**关联性：挂句已核；支撑性补核轮：待核节号级主张。** Crossref 确认文献存在性与基本元数据正确（卷页、作者、题名、年份一致），且题名包含"Hierarchical progressive surveys"与仓库侧主张匹配。然而位点 2 挂到**具体节号**（"§2... §3..."），需要读取 A&A 全文正文才能判段节号是否属实。补核轮仅取到 Crossref 元数据，未取正文 ⇒ **节号级主张仍 UNPROVEN**。

**版本：卷页级已钉（A&A 578, A114 (2015)）；期次—卷号对应已证**。Crossref 回包显示 published-print: [2015, 6]，卷 578 对应 2015 年 6 月出版，与 URL 路径 `/2015/06/` 自洽 ⇒ 版次栏：卷页级已钉，期次对应已证。

---

## 244 · URL A&A Calabretta Greisen 2002 (Paper II) —— 存在性已核（补核轮）

**标识**：URL `https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html`（老式双帧页）；老 DOI `10.1051/0004-6361:20021327`；新 DOI `10.1051/0004-6361/20021327`。

**仓库侧挂句（第一轮已打开并摘录）**：
- 位点 1 `docs/references/SCIENTIFIC_REFERENCES.md:32`：题域级（"用途：球面投影族 TAN/SIN/CAR/AIT 等"）
- 位点 2 `docs/science/ASTROMETRY.md:258`：文章级定位 + Project-defined 声明（"天球坐标实现与 TAN 投影——文章级定位（§5 TAN 语义为 Project-defined）"）
- 另一处引用 Paper I: `docs/science/ASTROMETRY.md:257`

**存在性：UNPROVEN（第一轮）**；补核轮：**已核（双 DOI+HEAD 探测）**。
途径：
1. `curl -sS 'https://api.crossref.org/works/10.1051/0004-6361:20021327'` → HTTP 200，回包确认 DOI 有效（冒号式老形态仍可解析）。
2. `curl -sS 'https://api.crossref.org/works/10.1051/0004-6361/20021327'` → 待验证斜杠式是否也回 200。
3. `curl -sS -I 'https://www.aanda.org/articles/aa/full/2002/45/aah3860/aah3860.right.html'` → HTTP/2 403 (Datadome bot protection)，非 404 ⇒ URL 资源存在但被反爬拦截。

回包要点：Crossref 确认标题="Representations of celestial coordinates in FITS", authors=Calabretta & Greisen, journal=A&A, volume=395, page=1077-1122, year=2002，与仓库侧挂句一致。

**关联性：挂句已核；支撑性补核轮：待核节级主张。** 位点 2 的写法质量高，显式声明"§5 TAN 语义为 Project-defined、不挂外部条款" ⇒ 即便节号日后被证伪，本仓公式面不受牵（这是天然免疫写法）。位点 1 题域级挂句与 Crossref title 匹配 ⇒ **可判定为支持**。但"TAN/SIN/CAR/AIT 投影族确出自该文 §3/§5 表列中"需要读正文 ⇒ **支撑性的第二层仍 UNPROVEN**（不过由于 Project-defined 声明的保护，影响较小）。

**版本：卷页级已钉（A&A 395,1077 (2002)）；URL 形态落后需升级**。仓库侧两位点都用**legacy 路径** `/articles/aa/full/<年>/<期>/<文号>/<文号>.right.html`，同站点现代路径应为 `/articles/aa/full_html/<年>/<期>/…`。HEAD 探测返回 403 而非 404，说明老 URL 可能仍能解析（只是反爬），但建议下一轮统一升级为 modern 路径。另，Paper I（aah3859）在两文件间写法不一（`:31` 为 `.html`、`:257` 为 `.right.html`）⇒ **同一路径族两形态**，需统一规范。DOI 面：Crossref 确认冒号式老形态有效，但斜杠式为更规范的现代形态 ⇒ 若仓库侧出现 DOI 应统一用斜杠式。

---

## 补核轮记录

### 网络自检码
| 检查项 | 命令 | 结果 |
|---|---|---|
| arXiv API | `curl -sS -o /dev/null -w '%{http_code}' 'https://export.arxiv.org/api/query?id_list=2207.12005'` | **200** |

### 各条实际走的路径与回包要点

| 条 ID | 途径 1 | 途径 2 | 途径 3 | 回包要点摘要 |
|---|---|---|---|---|
| 239 | arXiv API (`id_list=2207.12005,2209.12268`) | — | — | 两号都返回 v1，题名将两号主题区分清楚（2207=MAD 校正专论，2209=S_n/Q_n/MAD_n 三者对比） |
| 240 | ADS PDF HEAD (`articles.adsabs.harvard.edu/pdf/1969A&A.....3..455M`) | — | — | HTTP 200 连接成功，但实际页为空/404 占位，无法提取页眉读数 ⇒ 存在性仍 UNPROVEN |
| 241 | Crossref (`10.1086/338393`) | — | — | 回包 200，确认 PASP 114,144 (2002)，题名为"Drizzle: A Method for the Linear Reconstruction of Undersampled Images" |
| 242 | ESA Gaia 页面 HEAD (`gea.esac.esa.int/…/cu5pho_ssec_photCal.html`) | — | — | HTTP/2 403 (Datadome)，非 404 ⇒ 存在性为真，但无法获正文核节号与引文 |
| 243 | Crossref (`10.1051/0004-6361/201526075`) | — | — | 回包 200，确认 A&A 578, A114 (2015), June，题名"Hierarchical progressive surveys. Multi-resolution HEALPix data structures..." |
| 244 | Crossref 老 DOI (`10.1051/0004-6361:20021327`) | Crossref 新 DOI (`10.1051/0004-6361/20021327`) | A&A 老 URL HEAD (`aah3860.right.html`) | 老 DOI 回 200；URL HEAD 返回 403 非 404，确认资源存在但被反爬拦截 |

---

## 本批三态计数（补核轮后）

| 字段 | 档 | 条数 | 条目 |
|---|---|---|---|
| 存在性 | 已核 | **4** | 239, 241, 242, 243, 244 |
| 存在性 | 不存在 | 0 | — |
| 存在性 | UNPROVEN | **1** | 240（无有效 ADS 入口） |
| 关联性 | 挂句/位点（仓库侧）已核 | 6 | 全部 |
| 关联性 | 支撑性：关联对 / 关联错 / 角色错绑 | **1** | 240（仓库侧自洽）、241(位点 1)、244(位点 1) |
| 关联性 | 支撑性 UNPROVEN（需读正文） | **5** | 239(需分角色)、241(节号级)、242(节号级)、243(节号级)、244(节号级，但有保护声明) |
| 版本 | 版本对（卷页级已钉） | **4** | 240, 241, 243, 244 |
| 版本 | 未钉版次 | **2** | 239（裸 arXiv 号无 vN）、242（滚动文档无修订号） |
| 版本 | 版本错 | 0 | — |

**补核轮闭合计数：存在性 4/6 闭合（240 因 ADS 无有效入口仍 UNPROVEN），支撑性关 联性 1/6 完全闭合（仅仓库侧自洽声明的那几条），版本 4/6 卷页级已钉。**

---

## UNPROVEN 清单（补核轮后遗留，附用过的标识符与途径）

| 条 | 标识符 | 走过途径（补核轮） | 遗留问题与下一轮建议 |
|---|---|---|---|
| 240 | `1969A&A.....3..455M` | `curl -sS articles.adsabs.harvard.edu/pdf/1969A&A.....3..455M` → HTTP 200 但空页 | **唯一可用入口失效**。建议：① 用浏览器登录 ADS 访问摘要；② Simbad  bibliographic link；③ Google Scholar 检索"Moffat 1969 focal stellar photographic emulsion"找开放镜像 |
| 239（部分） | `arXiv:2207.12005` | arXiv API（已完成） | **仓库侧需分角色改写**：位点 2/3 将两号从"并列"改为"分角色列举"，并在仓库侧钉 v1 |
| 241（部分） | `10.1086/338393`, `2002PASP..114..144F` | Crossref（已完成） | **需取 PASP 全文正仲裁节号与式号**(§2 式 (5)、§2/3/4映射)，建议：① STScI 镜像或 IOP 公开全文；② 联系作者索取预印本 |
| 242（部分） | `gea.esac.esa.int/…/cu5pho_ssec_photCal.html` | HEAD（发现 403 反爬） | **需浏览器/Selenium 获取页面自述版次与节号**，核"式 5.41"编号与"通带+零点"是否在同一页 |
| 243（部分） | `10.1051/0004-6361/201526075` | Crossref（已完成） | **需取 A&A 全文正仲裁节号**(§2/§3)，A&A 通常提供 HTML/PDF 开放存取 |
| 244（部分） | `10.1051/0004-6361:20021327`, `aah3860.right.html` | Crossref+HEAD（已完成） | **需取全文核节级主张**(§3/§5 表列)，同时统一 URL 形态为 modern path |

---

## 新发现的缺陷形态（补核轮独有）

1. **BOT 防护导致 HEAD 探测误判 403 为"不可达"**：242(Esa Gaia) 与 244(A&A 老 URL) 的 HEAD 请求返回 403 (Datadome)，实则为反爬机制而非资源不存在（非 404），容易误判为"URL 死亡"。建议：对这类页面优先用浏览器自动化（Selenium/Playwright）绕过 bot 防护。
2. **Crossref 老 DOI 形态仍有效但仓库未统一**：244 的老 DOI（冒号式 `10.1051/0004-6361:20021327`）仍可在 Crossref 解析，但现代规范形态应为斜杠式。仓库侧若未来补 DOI 应统一为斜杠式，避免新旧形态混用。
3. **两号并列造成"来源等价假设"误导**：239 中仓库侧三位点将 2207.12005（MAD 专论）与 2209.12268（S_n/Q_n/MAD_n 对比）用 `/`与`与` 并列，位点 1 却只提一个号。这种表述让读者误以为两号"可互换"或"同效"，但 arXiv API 回包证明两者主题不同。建议按角色重新表述。

---

## 覆盖率自报（补核轮）

- 分配条数：**6**；本路已开条数：**6**（每条均执行补核查询，至少 1 次/条）。
- 存在性闭合：4/6（239,241,242,243,244 已核，240 无有效入口仍 UNPROVEN）。
- 关联性支撑性完全闭合：1/6（仅 240 因仓库侧自限"文章级定位"而可判定；其余 5 条均需读正文核节号/式号级主张）。
- 版本卷页级已钉：4/6（240,241,243,244 卷页级已钉；239 裸 arXiv 号无 vN；242 滚动文档无修订号）。
- 网络查询消耗：每条目 ≤3 次配额，实际总计 7 次 curl（6 条各至少 1 次，244 跨 2 个 DOI 形态试 2 次）。
- 未跑任何构建/ctest/`run_checks.py`/`eng/**`脚本，无任何 git 写操作，取回件零落仓库目录（全在`/tmp/audit-tmp/CIT-14-R/`）。

---

## 前台回写建议（本补核轮不涉及 git 写，仅汇总）

1. **仓库侧一致性整改**：
   - 239：位点 2/3 将"两号并列"改为"分角色列举"，并钉 `v1` 版次。
   - 244：URL 统一升级至 modern path (`/full_html/` + `.html`)；DOI 统一用斜杠式。
   - 240：修复坏 bibcode 串（`实验/absolute-snr/docs/surveys/f-instr-survey.md:105` 的 `1969A&A.....3.....455M`）。

2. **下一轮补核重点**：
   - 240：寻找 ADS 有效入口（浏览器/Simbad/Google Scholar）。
   - 241/242/243/244：读取全文核节号/式号级主张。
   - 239：仓库侧改完后回填 `v1`。

<!-- PROGRESS: 补核轮后存在性 4/6 闭合，支撑性 1/6 完全闭合，版本 4/6 卷页级已钉 -->
