# G08-05 对抗审稿 · 第 1 遍 · 片 TAIL-DOC-001

- **片号**：`TAIL-DOC-001`
- **层**：尾域合并（`docs/` 下层行数 < 2000 的小层）
- **划片依据**：SRS-1 层内 LPT 均衡装箱，严格不跨层
- **基线**：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（实测一致）
- **日期**：2026-10-02
- **纪律**：全程只读；未执行任何 git 写命令；未编译、未跑 ctest/pytest/构建/实验脚本；未读取 `/tmp/acsd_g08/`；未修改任何仓内文件（本交付件除外）。中文路径一律 `git -c core.quotepath=false`。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威版清单） | **4** |
| 实际读完份数 | **4** |
| 成员总行数（清单声明 / 实测 `wc -l`） | **1716 / 1716**（一致） |
| 实际逐行读取行数 | **1716** |
| **覆盖率** | **100%（4/4 份，1716/1716 行）** |

**未读完的部分：无。** 本片 4 份成员文件全部由我本人用 `read` 工具逐行读完（非脚本扫描、非摘要）。分片清单成员：`docs/DOCUMENT_INDEX.yaml`(992) · `docs/ACSD_DESIGN.md`(682) · `docs/GLOSSARY.md`(30) · `docs/README.md`(12)。

**为核证本片而额外打开的原文**（不计覆盖率，属取证）：`docs/engineering/DOCUMENT_GOVERNANCE.md:32-38`、`docs/engineering/UNIFIED_OBJECTS.md:28-47`、`docs/detail/UNIFIED_MODEL.md`、`docs/science/DATA_SEMANTICS.md:40-91`、`docs/science/CONTROL_WEIGHT_SNR.md`、`docs/science/INTEGRATION.md:8-12,147-151`、`docs/science/CALIBRATION.md`、`docs/science/DRIZZLE.md`、`docs/science/ASTROMETRY.md`、`lib/algorithms/calibration/src/cosmetic_corrector.cpp:156-161,228-266`、`lib/infrastructure/scheduler/src/module_adapters.cpp:512-526,927`、`artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`。

**计数口径声明**：本片不涉判据计数。凡出现「24 个术语」「7 个被覆盖」「49 个条目」「181 条索引」处，分别为**去重术语单元格**、**附录A∩词典交集**、**台账 distinct 条目 ID**、**索引 `- path:` 条目**这一层口径，已就地注明。

---

## 2. 本片判定：**需修（含 3 条阻断级）**

本片是全仓权威顶点与其两份附属登记面。红队姿态下我预期这里问题最多；实测结果相反的一半恰恰是**指针层极干净**——但语义层与登记层存在 3 处会误导实现的硬缺陷。

### 最重的 3 条

**① 阻断 · `docs/GLOSSARY.md:12` 的 `variance` 行同时被上位权威与它自己指定的锚点推翻。**
词典写「**无覆盖像素=0**」。而 (a) 权威顶点 `docs/ACSD_DESIGN.md:550` 写「无覆盖、无数据用 NaN，**不用 0** 或 Inf 冒充无效」；(b) 该行自己声明的锚点 `docs/science/DATA_SEMANTICS.md` §4a 三态表 `:82` 写「**无覆盖** ⇒ `variance=NaN`、`:82` `ivar=NaN`」，且 `:88` 明写「**凡与本表不一致的表述一律以本表为准**」。即：自称「唯一术语权威」的词典，被更高一层和被它自己点名的正本**双向推翻**，且推翻它的那句话自带「以我为准」的裁决条款。实现者照词典写 `0` 即同时违反最高设计与数据正本。

**② 阻断 · `docs/DOCUMENT_INDEX.yaml:425-426, :443` 指向一个不存在的机器门禁，且是「恒真门」的极端形态。**
`:426` notes 断言「双向对应由 `eng/tools/contract_doc_sync.py` 机器校验（含 `--self-test` 红绿双向）」；`:425` 与 `:443` 又把该文件列为 `downstream`「**机器扫描出的**活动引用方」。实测 `git -c core.quotepath=false ls-files | grep contract_doc_sync` → **零命中**，磁盘亦无。而本索引 `:6` 自订「path **必须真实存在，悬空条目即缺陷**」、`:19-20` 自订「悬空指针…**按缺陷处置**」——自订规则被自身违反。这不只是悬空指针：一个**声称存在却根本没有的校验器**在结构上不可能翻红，是「判据读不到真实对象」的退化终局（连对象都不存在）。

**③ 阻断 · `docs/ACSD_DESIGN.md:131` 是**裸从句式伪引**，且与同文件 `:182` 自相矛盾。**
`:131` 写「可作科学叠加权重的对象只有两个（**§3.1**）：…广义最小二乘组合权重 `surface_gls`」，把该断言归给 §3.1。但 §3.1 的 13 对象表 `:179` **不含 `surface_gls`**；`surface_gls` 在全篇 682 行中**只出现在 `:131` 这一处**（`grep -n surface_gls docs/ACSD_DESIGN.md` 单行命中）。更硬的是同文件 `:182`：「全链**没有**「权重模式」这一可选概念」——与两个一级正本正面冲突：`docs/science/DATA_SEMANTICS.md:3252`「生产科学模式 `point_information` / `surface_gls`｜**配置显式选择**」、`docs/engineering/PUBLIC_API.md:2027`「有效来源…（**显式声明**，切换只经配置）」。按 AGENTS.md §3「文档冲突以更高一层为准」，遵 `:182` 的实现者将失去 `FZ-MODE-PRODUCTION` 退役词拒绝机制的全部依据。
**该条正是纪律点名的「主项裸从句」形态：主项无任何引号，只扫成对引号的审计器会 100% 漏掉它。**

---

## 3. 逐文件清单

### 3.1 `docs/README.md`（12 行）— 判定：**通过**
- 读了什么：全文 12 行。
- 看到什么：`:3` 上游声明 `ACSD_DESIGN.md §0（文档权威与索引）、§0.1（文档写法）`；`:10` 声明「`GLOSSARY.md` 为术语唯一权威；`DOCUMENT_INDEX.yaml` 是全文档集的唯一索引地图，**与文件树一致**」。
- 判定：`:3` 两个上游节**逐字命中**（`ACSD_DESIGN.md:5` = `## 0. 文档权威与索引`；`:33` = `### 0.1 文档写法`）。`:10` 的「与文件树一致」经我独立双向对账后**部分为真**：181 条索引 path 全部存在（0 断链），13 个未登记文件全部是被规则 2 明令豁免的目录招牌 README——但**规则 4 `:21-22` 的措辞「docs/** **全部** .md 与 .yaml」本身无条件，与该豁免不自洽**（见 §4 #7）。`:5` 的「GLOSSARY 为术语唯一权威」与实测 7/24 覆盖率冲突（见 §4 #11）。

### 3.2 `docs/GLOSSARY.md`（30 行）— 判定：**需修（本片问题最密的一份）**
- 读了什么：全文 30 行（21 条术语行 + 抬头 + 表头）。
- 看到什么：`:3` 上游 `ACSD_DESIGN.md 附录 A（术语）` 真实存在（`:670` = `## 附录 A. 术语`，逐字）。全表 21 条术语，每条带「单位/极性/域」「legacy alias」「权威锚点」四列。锚点分两类：节号锚（§4a/§2a/§3a 等）与**唯一一处源码行锚** `:20`。
- 判定：**6 条锚点破口**（我逐条独立复核，见 §4 #1-#6、#9）。文件级 0 悬空、节号级 0 缺失——我实测被引 7 个文件全部存在、被引节号（`§4a`/`§4`/`§2`/`§1`/`§5`/`§3`/`§2a`/`§3a`）全部真实存在。**坏的全是「节号对、内容反」**。`term` 行数实测 21（`awk -F'|'` 计得 22 含表头行，去表头 = 21）。

### 3.3 `docs/ACSD_DESIGN.md`（682 行）— 判定：**需修（含 1 条阻断）**
- 读了什么：全文 682 行，13 章 + 附录 A/B。
- 看到什么：权威链顶点。`:5` §0、`:33` §0.1、`:670` 附录 A、`:676` 附录 B 均真实存在。13 处「下级索引」块共 **171 个路径指针，我实测 171/171 全部存在，0 悬空**。文档内 `§N.M` 自引用全部指向真实章节。
- 判定：**语义层 1 条阻断（`:131`）+ 若干须修**（`:672` 附录 A 指针断裂、`:485-512` §8.4 结构树漏 3 个生产目录、`:179` vs 一级正本的 canonical 覆盖率）。文档写法（AGENTS.md §5）实测**零日期、零版本号、零 commit、零历史叙事词、零元信息块**——这一项是本片最干净的部分，我如实记录为通过。

### 3.4 `docs/DOCUMENT_INDEX.yaml`（992 行）— 判定：**需修（含 1 条阻断）**
- 读了什么：全文 992 行（头部规则 1-6 + `doc_index` + 181 条 `active` 条目 + 空 `archived:`）。
- 看到什么：`:32` `schema_rev: "3"`；`:35` 起 181 条条目；`:992` `archived:` 区段为空（YAML 解析为 `null`，非 `[]`——不违反规则 1 真空集平凡满足，但消费方需防 null）。规则 2/4/5/6 四处互相牵制的覆盖面定义。
- 判定：**1 条阻断（悬空机器门禁）+ 4 条须修**（`:989` 计数错、`:629` 行锚失效、`:26-27` 规则 6 自相矛盾、`downstream` 层已陈旧）。**但必须如实记录：181 条 `path` 全部存在、0 悬空、0 重号；13 个孤儿文件全部是规则 2 显式豁免的目录招牌 README；账目闭合 174 + 13 = 187。** 索引在「文件是否存在」这一层是本片乃至全仓最干净的面。

---

## 4. 发现清单

### 阻断（3）

**#1 `docs/GLOSSARY.md:12` `variance` 行：`无覆盖像素=0` 与两处上位权威正面冲突**
- 位置：`docs/GLOSSARY.md:12`
- 现状：`…无覆盖像素=0`，锚点列写 `docs/science/DATA_SEMANTICS.md §4a`
- 应为：`无覆盖像素=NaN`（`:82` 三态表）；锚点行的 k² 立场亦应改挂 `docs/science/DRIZZLE.md:218`（§9a）而非 §4a
- 证据：`docs/ACSD_DESIGN.md:550`「无覆盖、无数据用 NaN，**不用 0** 或 Inf 冒充无效」；`docs/science/DATA_SEMANTICS.md:82`「**无覆盖** | `area<=0` 或 `area` 非有限 | **`NaN`** | **`NaN`**」；`:88`「**凡与本表不一致的表述一律以本表为准**」；`:48`「…给出**同一个** `c_jp` 与**逐位相同**的 `variance_p`（`pixfrac²` 在分子分母相消）」——直接反驳词典「裸写 `/D_p²` 漏 k²」
- 附：同行的量纲也不一致（词典 `信号单位²(ADU²)` vs `DATA_SEMANTICS.md:53` `ADU^2/sr^2`）

**#2 `docs/DOCUMENT_INDEX.yaml:425-426, :443` 悬空机器门禁**
- 位置：`docs/DOCUMENT_INDEX.yaml:425`、`:426`、`:443`
- 现状：notes 声称 `eng/tools/contract_doc_sync.py` 执行「含 `--self-test` 红绿双向」的机器校验；另两处把它列为机器扫描出的 downstream 活动引用方
- 应为：要么补实现并接线，要么改写该保证性声明为「未接线」
- 证据：`git -c core.quotepath=false ls-files | grep -i contract_doc_sync` → **零命中**；`test -e eng/tools/contract_doc_sync.py` → 不存在。索引 `:6`「必须真实存在，悬空条目即缺陷」、`:19-20`「悬空指针…按缺陷处置」自订规则被自身违反。放大面：`eng/contracts/data/unified_object_registry.json` 对 4-5 个 CONTRACT-501 schema 重复同一句保证；`docs/engineering/UNRESOLVED_REGISTER.md:3124-3127` 已列其为「无自动执行面」
- 口径声明：本条的「downstream token 已陈旧」一面，前三轮 `审稿-R2-S2-文档索引与登记面.md:125`（M-08）**已记录**；**新增的是 notes 里那句「机器校验 + 红绿双向自测」的保证性断言，以及它被 registry json 复制的放大面**。

**#3 `docs/ACSD_DESIGN.md:131` 裸从句伪引 + 与 `:182` 自相矛盾**
- 位置：`docs/ACSD_DESIGN.md:131`（归因 `:179`、`:182`）
- 现状：「可作科学叠加权重的对象只有两个（§3.1）：…`surface_gls`，`x_hat = (AᵀC⁻¹A)⁻¹AᵀC⁻¹d`」
- 应为：或补 `surface_gls` 进 §3.1 并撤 `:182` 的「无权重模式」，或撤 `:131`
- 证据：`grep -n surface_gls docs/ACSD_DESIGN.md` → **仅 `:131` 一行命中**；`:179` 13 对象表无此项；`:182`「全链**没有**「权重模式」这一可选概念」；对立面 `docs/science/DATA_SEMANTICS.md:3252`、`docs/engineering/PUBLIC_API.md:2027`

### 须修（10）

**#4** `docs/GLOSSARY.md:15` `frame_quality_weight = support×snr_v²` — 被引节 `CONTROL_WEIGHT_SNR.md §2a` 不含该式；同文件 `:130` 明令「**不存在权重枚举、不存在 support×snr² 通道**」；`:134`/`:153`/`:168`/`:175` 明令禁止「snr=1.0 伪装 unknown」。**仓内 `UNRESOLVED_REGISTER.md:697-699` 已登记为「待裁决」且至今未闭环。**

**#5** `docs/GLOSSARY.md:14` `pixel_weight` — 该词在 `docs/science/DATA_SEMANTICS.md` 全文 **0 命中**（我实测），且非 13 个 canonical 对象之一（`UNIFIED_OBJECTS.md:28-47` 逐行核对无此项）。词典给出无正本承载的术语。

**#6** `docs/DOCUMENT_INDEX.yaml:989` 计数错 — `duty` 写「逐条 **48** 项」，实际 **49** 项。**我独立复算**（不采信子代理）：`LIMITATIONS.md` 的编号条目 ID 去重后为 `1..13, 15, 16a, 16b, 17..49`，共 **49** 个 distinct ID（无 14，16 拆 a/b）。

**#7** `docs/DOCUMENT_INDEX.yaml:629` 行锚失效 — `duty` 写「唯一源=`module_adapters.cpp:512-526` descriptor」。实测该区间是 `p1_angular_sep_deg` 角距函数（`asin`/`atan2`）；真实 descriptor 在 `lib/infrastructure/scheduler/src/module_adapters.cpp:927`（`d.module_id = "acsd.phase1.wcs-platesolve";`）。属「代码改了、归档没重跑」家族。

**#8** `docs/DOCUMENT_INDEX.yaml:26-27` 规则 6 自相矛盾 — 头部写「机器源（.json/.csv/.py 等非人读件）**一律不登记**」，却在 `:49`、`:54` 登记了 `eng/contracts/ledgers/ledger_schema.py` 与 `eng/tools/source_scan.py`。其权威 `docs/engineering/DOCUMENT_GOVERNANCE.md:38` 明文：「机器源…**不因跨域而自动取得登记资格**：只有落进第 2 类、且有人读正本对它有直接指针时才登记…反过来…**不得一边声明某类件不登记、一边把它们登记在册**」。同索引的 `coverage` 字段 `:34` 措辞其实是对的（只说「不因跨域而自动取得登记资格」）——**不一致出在头部规则 6，不在 coverage**。

**#9** `docs/GLOSSARY.md:20` `bad_mask` 源码行锚指错 — 锚 `lib/algorithms/calibration/src/cosmetic_corrector.cpp#158` 实为分节横幅注释 `// ======================== 插值修复 ========================`（`:161` 才是 `interpolate_pixels` 签名）。极性主张本身**正确**（1=坏点）：代码证据 `:152` `cold_mask[i] = (bias[i] < threshold) ? 1 : 0`、`:167` `if (!bad_mask[i]) { out[i] = data[i]; }`、`:258` `all_bad[i] = hot_mask[i] || cold_mask[i]`。真正写着「1=坏像素，0=好像素」的是 `lib/algorithms/calibration/cpp/cosmetic_corrector.h:18`。

**#10** `docs/GLOSSARY.md:26` `stacking` 锚点自否认 — 词典称「跨帧样本组合的唯一实现是**排异积分**…做**排异**与加权求和」，锚 `docs/science/INTEGRATION.md`。但该文件 `:10` 非目标明写「**不决定候选资格/排异（SCI-REJ）**」，`:149` 写「`accepted` 为**排异层产物（SCI-REJ）**」。加权求和半边成立（`:9` 目的），排异半边被自身锚点否定。

**#11** `GLOSSARY` 对附录 A 的覆盖缺口 — `docs/ACSD_DESIGN.md:672` 列 24 个术语并称「定义见 `docs/detail/` 与 `docs/GLOSSARY.md`」。我实测交集仅 **7 个**（signal、variance、ivar、support、coverage、projection、stacking），**17 个在词典无条目**，含 13 个 canonical 对象中的 8 个（source_snr、depth_m5、frame_snr、point_information、sparse_snr_layer、validity、rejection、provenance）与 `control_ivar`。`control_ivar` 在 `UNIFIED_OBJECTS.md` 中 **0 命中**（我实测），不是数据对象，却是 Phase2 生产科学权重唯一来源（`DATA_SEMANTICS.md:2056`「本域科学权重唯一来源=control_ivar」，docs/ 内 97 处引用），读者会误读为第 14 个对象。`运行 manifest` 在词典与 `docs/detail/` **各 0 命中**（我实测）。

**#12** `docs/DOCUMENT_INDEX.yaml:21-22` 规则 4 措辞与规则 2 不自洽 — 规则 4 无条件称覆盖「docs/** **全部** .md 与 .yaml」，而实际 13 个 README 不在索引内；豁免只写在规则 2 与 `:34` 的散文字段里，规则 4 本身无限定语。

**#13** `downstream` 层已陈旧且计数不可复现 — 34 处引用指向 10 个 HEAD 已不存在的文件（`ACCEPTANCE_SPEC.md` ×15、`ENGINEERING_SPEC.md` ×10、`contract_doc_sync.py`、`文档现状审计台账.csv`、`检查-S6-algorithmsB.md`、`问题台账.md`、`bad_id_format.json`、`config_consistency_check.py`、`DEPENDENCIES.md`、`CONTROL_PACK_SPEC.md`）。另 `:76/:81/:86/:91/:102` 的「等 462/276/353/366/36 处」在「文件数×出现次数 × 含/不含声明排除项」四种组合下无一复现。

### 建议（5）

**#14** 8 处下级索引以「另 N 份」截断（`ACSD_DESIGN.md:241,277,389,439,526,541,554,640`），合计 55 份未具名，且未指向 `DOCUMENT_INDEX.yaml` 作为查证去处。**前三轮已记（`RR01:62` M-1、`R2-S2:126` M-09），本遍复核成立，计入分母而非新增。**
**#15** `ACSD_DESIGN.md:485-512` §8.4 顶层结构树列 `lib/` 下 4 项（algorithms/infrastructure/include/third_party），实况 7 项，漏 `lib/phase1_session/`、`lib/phase2_session/`、`lib/phase3_session/` 三个生产目录。
**#16** `ACSD_DESIGN.md:672` 附录 A 是纯指针存根（自身不含定义），溯源链在此退化一层。
**#17** `DOCUMENT_INDEX.yaml:202/208/219` 的「`p3_output.cpp` 370 行」「`p3_wcs.cpp` 165 行」等口径歧义——若读作「文件共 N 行」则与实况（1270/593 行）不符，若读作「已锚定 N 行」则不可证伪。
**#18** 词典与正本双向互引：`docs/science/ASTROMETRY.md:44` 反向引词典 `ra_dec`、`docs/science/DRIZZLE.md:216` 反向引 `surface_brightness`。治理隐患。

---

## 5. 我主动构造的反例

**CE-1（推翻成功）——「词典能拿到 `variance` 的权威定义」**
- 构造：假设 `GLOSSARY.md:12` 的锚点 `DATA_SEMANTICS.md §4a` 确实承载该行全部断言。取该节最硬的一条可判定位——「无覆盖像素」的取值——回源逐字比对。
- 期望推翻：GLOSSARY 自称「唯一术语权威」且每条带权威锚点，故该行应可回源闭环。
- 结果：**推翻成功**。`:82` 写无覆盖 = `NaN`；词典写 0。`:88` 更自带「以我为准」裁决句。且词典的 k² 立场被 `:48`「逐位相同、pixfrac² 相消」正面反驳。⇒ 阻断 #1。

**CE-2（推翻成功）——「索引覆盖 = 与文件树一致」**
- 构造：按 `ACSD_DESIGN.md:28` 的「未登记或登记不存在均为缺陷」，做双向对账：索引 181 条 path 逐条 `-e` 测试 + `git ls-files docs/` 全量取差集。
- 期望推翻：找出登记不存在（悬空）或未登记（孤儿）。
- 结果：**部分推翻**。悬空 **0** 条（181/181 存在）；孤儿 **13** 条——但全是规则 2 明令豁免的目录招牌 README，故**不推翻治理结论**。真正推翻了**规则 4 `:21-22` 的措辞**（自称「全部」却无条件），以及 `:989` 的 48→49 计数。⇒ 须修 #6、#12；「与文件树一致」判 **PARTIALLY TRUE**。

**CE-3（推翻成功）——「§3.1 是权威顶点，`:131` 归因给它就等于有据」**
- 构造：`surface_gls` 是 `:131` 断言的两个科学叠加权重之一。对全篇 682 行做 `grep -n surface_gls`。
- 期望推翻：若该词在别处（尤其 §3.1 `:179` 的 13 对象表）被定义，则 `:131` 的归因成立。
- 结果：**推翻成功**。全篇仅 `:131` 单行命中；§3.1 无此对象；`:182` 反述「无权重模式」，与两个一级正本冲突。⇒ 阻断 #3。

**CE-4（推翻失败，如实记录）——「`docs/science/DRIZZLE.md` 是悬空锚点」**
- 构造：我从分片清单 `DOC-SCI-001` 成员表（23 份）里**看不到** `DRIZZLE.md`，而 GLOSSARY 与 ACSD_DESIGN 共引它 4 次，据此推定文件缺失。
- 期望推翻：证实悬空锚点。
- 结果：**推翻失败**。`docs/science/DRIZZLE.md` 真实存在（255 行，18 个 `##` 章节，§3 `:29`/§5 `:47` 均在），且未列入 DOC-SCI-001 是因为我当时只读了清单的前半段。**这是我的初始误判，据实记录。**

**CE-5（推翻成功）——「机器门禁声称存在即等于存在」**
- 构造：`:426` 的 notes 断言 `contract_doc_sync.py` 执行红绿双向自测。若该文件存在，则「双向对应已被机器校验」为真。
- 期望推翻：反证校验器缺失。
- 结果：**推翻成功**。`git ls-files | grep contract_doc_sync` 零命中。这同时是**恒真门体检双向性**的实例：一个「永远绿」的判据——因为它没有对象。⇒ 阻断 #2。

---

## 6. 盲复算

**方法**：先在不读前三轮审稿件的前提下独立取证完成 §3–§5，再回读 `审稿-RR*.md`/`审稿-R2-*.md`/`审稿-R3-*.md` 检索我的每条发现，最后逐条标注「一致 / 偏松 / 偏严 / 新」。

| 我的发现 | 前三轮是否已记 | 比对结论 |
|---|---|---|
| #1 GLOSSARY:12 `无覆盖=0` vs NaN | **未记该语义冲突**。`R3-T2:233`（S6）只提 `:12` 单位 ADU² 与 `:29` 的面分叉，**未触及 0-vs-NaN** | **偏严（新）**——我这条是新的，且比前轮更硬（引入了 `:88` 自带裁决句） |
| #2 contract_doc_sync 悬空门禁 | `R2-S2:125`（M-08）**已记** downstream token 含 `contract_doc_sync.py` 已不存在 | **一致但我更严**——前轮止于 downstream token；我把 notes 的「机器校验+红绿自测」保证句与 registry json 放大面算作新增 |
| #3 ACSD_DESIGN:131 裸从句伪引 | 前三轮 `grep surface_gls` **零命中** | **偏严（新）** |
| #4 frame_quality_weight | `R2-S1:99`（M1c）**逐字已记**「改为 `quality_weight = snr_v`，锚点改指 §4」 | **一致**——非新增 |
| #5 pixel_weight | `RR09:90`（4-2）、`总账:216`（R09 S-13）**已记** | **一致**——非新增 |
| #6 `:989` 48→49 | 前三轮未记 | **偏严（新）** |
| #7 `:629` 行锚失效 | 前三轮未记 | **偏严（新）** |
| #8 规则 6 自相矛盾 | `R2-S2` 覆盖了 DOCUMENT_GOVERNANCE 拓扑与 downstream，**未覆盖规则 6 措辞 vs 实践的自相矛盾** | **偏严（新）** |
| #11 词典 7/24 覆盖 | `R2-S1:98`（M1b）、`R3-T2:206`（S5）、`RR09:173` **均已记**（口径为「4/13」「0/13 核心符号」） | **一致**——我补的是附录A 24→7 的**可数口径**与 `control_ivar`/`运行 manifest` 两处具体缺口 |
| #14 「另 N 份」截断 | `RR01:62`（M-1）、`R2-S2:126`（M-09）**已记** | **一致**——非新增 |
| 指针层 0 悬空 | 与前轮一致（`R2-S2` M-08 方向相同但结论更悲观） | **一致，且我比前轮乐观**——前轮把 downstream 陈旧外推为索引整体失信；我实测 181/181 path 全部存在，区分开了「path 层可信」与「downstream 层不可信」两层 |

**盲复算总判：一致偏严。** 11 条比对的发现中 4 条为新、5 条与前轮一致、0 条被前轮更严地命中、**0 条属我虚增**。我把 3 条前轮已记的发现（#4/#5/#14）明确标为「非新增」并仍列入清单——因为「一遍 = 同一片完整重读」，重复确认本身就是这一遍的产出，但**不得冒充实新**。

---

## 7. 子代理派发记录

派发 **5 个**，全部只读（明确禁止改文件、禁止 git 写、禁止编译/测试、禁止读 `/tmp/acsd_g08/`、禁止把 `topic_save` 当作写文件通道）。

| # | 主题 | 做法 | 我的复核结果 |
|---|---|---|---|
| S1 | GLOSSARY 锚点全量审计 | 24 锚点逐条回源 | **采纳 5 条**（#1 部分、#4、#5、#9、#10 前身）。**修正其一处**：其称 `:158` 是「函数签名」类内容，我初读也这么以为，**实测 `:158` 是分节横幅注释、`:161` 才是签名**——以我的复核为准，结论不变（该行不含极性定义） |
| S2 | DOCUMENT_INDEX 双向对账 | 索引↔文件系统 | **全盘采纳**且与我独立复算**逐项吻合**（181/0 悬空、13 孤儿、豁免 4 条）。其 `:989` 48→49 我**独立复算得 49**，采信。其 `archived:` 为 null 的补充我采信并写入 §3.4 |
| S3 | ACSD_DESIGN 声明 vs 代码现实 | 逐条对文件树/注册表/CLI | **采纳** #15（§8.4 漏 3 个 `lib/phase*_session/` 目录）、`control_ivar` 非 canonical 对象。**否决其 F-1**（见下） |
| S4 | 伪引与引用审计（含裸从句） | 明确要求**不扫成对引号**、专扫裸从句 | **采纳其全部 7 条失败**中的 F1/F3/F4/F7/F8（对应我的 #1/#5/#3/#2/#7）。其「9 条失败中 4 条是无引号裸从句，若只扫配对引号一条都抓不到」的方法论结论与我的阻断 #3 独立吻合 |
| S5 | GLOSSARY 盲复算（S1 的独立重复） | 同 S1，互不可见 | **作为盲复算的第二样本**：锚点单元格数报 22 vs S1 的 24、破口 5 vs 6。差异全部定位到 `projection`（仅 S1 报）与 `stacking`（仅 S5 报）两条；**我独立复核后两条均成立**，故破口数取 **6**；单元格数按我 `awk` 实测取 **21 条术语行**（22 含表头） |

### 明确否决 / 降级的子代理结论

1. **否决 S3 的 F-1（阻断级「phase1/2/3 禁令被代码违反」）→ 降级为不成立。**
   理由：`:415` 原文是「phase1/2/3 仅为内部指代，**不出现在命令与算法目录命名中**」。实测的 `acsd.phase1.calibration` 是 **module_id**（模块标识串），既非命令名、亦非算法目录名；`lib/phase1_session/` 在 `lib/` 根而不在 `lib/algorithms/` 下。按句子的**字面辖域**它并未被违反。把「模块标识符」与「目录命名」混为一谈会把一条可辩护的禁令误判成阻断。**该条不成立，不计入发现清单。**
2. **降级 S3 的 F-4 中「§3.1 的 13 对象被推翻」一半。** 其依据是 `UNIFIED_OBJECTS.md:123` 的「mapped = 4」。我未独立复核该 `:123` 行，且该判据涉及 canonical 映射覆盖率的口径之争，超出本片成员范围，**记为待前台裁决，不计本片发现**。
3. **降级 S3 的 F-10 / F-11（实验树日期与任务号、`lib/algorithms/rejection/module.yaml` 三套身份）。** 二者均**不在本片 4 份成员文件内**，属其他片，按「不许换焦点」纪律**不计入本片**。
4. **修正我自己的一处初判。** 我在 CE 之前口头称 `:158` 是 `interpolate_pixels` 签名行，实测为横幅注释（`:161` 才是签名）。已在 §3.2 与发现 #9 更正，不保留错误表述。
5. **记录一次我自己的误判。** CE-4：`docs/science/DRIZZLE.md` 悬空之疑来自我只读了分片清单前半段，**证伪失败**，如实记录。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD          # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
wc -l docs/DOCUMENT_INDEX.yaml docs/ACSD_DESIGN.md docs/GLOSSARY.md docs/README.md   # 期望 992 682 30 12 = 1716

# #1 阻断：无覆盖 0 vs NaN
sed -n '12p' docs/GLOSSARY.md | cut -c1-200
sed -n '550p' docs/ACSD_DESIGN.md
sed -n '82p;88p' docs/science/DATA_SEMANTICS.md

# #2 阻断：悬空机器门禁
test -e eng/tools/contract_doc_sync.py && echo EXISTS || echo ">>> DOES NOT EXIST <<<"
git -c core.quotepath=false ls-files | grep -i contract_doc_sync || echo "  NONE tracked"
sed -n '6p;19,20p;425,426p;443p' docs/DOCUMENT_INDEX.yaml

# #3 阻断：裸从句伪引 + 自相矛盾
sed -n '131p;179p;182p' docs/ACSD_DESIGN.md
grep -n 'surface_gls' docs/ACSD_DESIGN.md          # 期望仅 131 行命中
grep -n 'surface_gls' docs/science/DATA_SEMANTICS.md docs/engineering/PUBLIC_API.md | head

# #4/#5：GLOSSARY 锚点内容被否定
grep -n '不存在权重枚举' docs/science/CONTROL_WEIGHT_SNR.md
grep -c 'pixel_weight' docs/science/DATA_SEMANTICS.md   # 期望 0

# #6 计数：48 vs 49（独立复算）
sed -n '989p' docs/DOCUMENT_INDEX.yaml
grep -oE '^\s*\|?\s*\*{0,2}([0-9]{1,2}[a-b]?)\.{0,2}\*{0,2}\s' \
  artifacts/evidence/known-limitations-ledger/LIMITATIONS.md \
  | grep -oE '[0-9]{1,2}[a-b]?' | sort -u | wc -l          # 期望 49

# #7 行锚失效
sed -n '629p' docs/DOCUMENT_INDEX.yaml
sed -n '512,516p' lib/infrastructure/scheduler/src/module_adapters.cpp   # 角距函数，非 descriptor
grep -n 'acsd.phase1.wcs-platesolve' lib/infrastructure/scheduler/src/module_adapters.cpp | head -1   # 期望 927

# #8 规则 6 自相矛盾
sed -n '25,27p' docs/DOCUMENT_INDEX.yaml
sed -n '38p' docs/engineering/DOCUMENT_GOVERNANCE.md
grep -n 'ledger_schema.py\|source_scan.py' docs/DOCUMENT_INDEX.yaml | head

# #9 bad_mask 行锚
sed -n '158p;161p' lib/algorithms/calibration/src/cosmetic_corrector.cpp
sed -n '258p' lib/algorithms/calibration/src/cosmetic_corrector.cpp
sed -n '18p' lib/algorithms/calibration/cpp/cosmetic_corrector.h

# #10 stacking 锚点自否认
sed -n '26p' docs/GLOSSARY.md
sed -n '10p;149p' docs/science/INTEGRATION.md

# #11 词典覆盖 7/24
for t in signal variance ivar source_snr depth_m5 frame_snr point_information sparse_snr_layer \
         support coverage validity rejection provenance UPM PSF HiPS HEALPix WCS projection \
         stacking drizzle control_ivar; do
  grep -qE "^\| \`?$t\`? \|" docs/GLOSSARY.md && echo "IN  $t" || echo "OUT $t"
done | grep -c IN        # 期望 7
grep -c 'control_ivar' docs/engineering/UNIFIED_OBJECTS.md   # 期望 0
grep -rc '运行 manifest' docs/GLOSSARY.md docs/detail/ 2>/dev/null   # 期望 0 0

# #12 覆盖面双向对账
grep -oE '^\s+- path: "[^"]+"' docs/DOCUMENT_INDEX.yaml | sed 's/.*path: "//; s/"$//' | sort -u > /tmp/i.txt
while read -r p; do [ -e "$p" ] || echo "DANGLING $p"; done < /tmp/i.txt   # 期望无输出
git -c core.quotepath=false ls-files docs/ | grep -E '\.(md|yaml)$' | sort > /tmp/f.txt
comm -23 /tmp/f.txt /tmp/i.txt                                          # 期望 13 行且全为 README.md

# #15 §8.4 树漏列
ls -d lib/*/ | tr '\n' ' '; echo        # 期望 7 项
sed -n '485,500p' docs/ACSD_DESIGN.md    # 树只列 4 项

# 通过项复跑（防偏倚）
test -e docs/science/DRIZZLE.md && echo "DRIZZLE.md EXISTS"   # CE-4 证伪成功
grep '^| [a-z_]' docs/GLOSSARY.md | grep -vc '^| term |'   # 21 条术语行（裸 grep -c 会多算表头 term 得 22）
grep -nE '^## [0-9]+|^## 附录' docs/ACSD_DESIGN.md | head -20 # §0/§0.1/附录A/B 均在
```

---

## 9. 遗留与移交

- **待前台裁决（超出本片或需负责人）**：#3 的 `surface_gls` 取舍（动最高设计）；#4 已在 `UNRESOLVED_REGISTER.md:697` 挂「待裁决」，**本遍确认它仍未闭环**；#6/#7 涉及 `artifacts/` 与 `lib/` 侧的事实，改动落在本片之外。
- **我与子代理的**争议裁定（供负责人推翻）**：§7 否决项 1（`:415`「phase1/2/3 不出现在命令与算法目录命名中」未被违反）。子代理按「`module_id` 含 phase 标识」判该禁令已不可执行；我按句子字面辖域（**命令名** + **`lib/algorithms/` 下的目录名**）判其未被违反——`acsd.phase1.*` 是模块标识串不是命令或算法目录名，`lib/phase1_session/` 也不在 `lib/algorithms/` 下。**此为口径之争，不是我掌握的事实优势；若负责人认为该禁令本意涵盖模块标识符，则应改写 `:415` 措辞使其可执行，而非按现状判为缺陷。**
- **方法学留痕**：审 `DOCUMENT_INDEX.yaml` 时只应取 `- path:` 条目。子代理自陈最初扫全文产生 12 条假阳性（把 `downstream:` 散文里的文件名误判为悬空登记）；本遍的对账命令同样只取 `- path:` 行，故 §3.4 的「181/181 存在、0 悬空」不含此类假阳性。
- **本片不建议施工**：审稿人只登记不改。本片 13 条发现中，#1/#2/#3 为阻断，但**任何一条的修复都会牵动其他片的文件**（如 #2 的 registry json、`docs/science/*`），应由前台统一原子提交。
- **覆盖率自评**：4/4 份、1716/1716 行、100% 逐行读完，无脚本替代阅读，无未读部分。