# T06 · 第 1 轮订正 · 子代理 C · detail 正文车道

写作用域：`docs/detail/` 根级、`docs/detail/anchors/**`、`docs/detail/infrastructure/**`。
未越界触碰 `docs/detail/registry/**`、`docs/GLOSSARY.md`、`docs/README.md`、
`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md`、`docs/engineering/**`、`docs/science/**`、
任何代码与配置。未执行任何 git 写操作。

---

## 1. 逐条处置表

### 1.1 A 组 · 坏路径（真解析器逐条核实）

| 编号 | 位置 | 问题 | 处置 | 改前逐字 | 改后逐字 | 依据 |
|---|---|---|---|---|---|---|
| A1 | `docs/detail/PHASE3_DETAILED_DESIGN.md:131` | 派单称 `phase_config_export.schema.json#/$defs/export_crop` 路径不存在 | **否决 · 保留原样**（验证器误报） | `eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop`） | 同左（未改一字） | `ls eng/contracts/schemas/` 有 `phase_config_export.schema.json`；`python3 -c "import json;print(list(json.load(open('eng/contracts/schemas/phase_config_export.schema.json'))['$defs']))"` 输出含 `export_crop`。脚本 `resolve()` 的 fragment 正则 `[A-Za-z0-9_.\-]+` 不吃 `/`，故 `/$defs/...` 整段落进文件名比对 ⇒ 结构性误报。JSON Pointer `#/$defs/export_crop` 是合法且真实存在的指针。 |
| A2 | `docs/detail/common.md:23` | 引 `docs/engineering/STANDARDS_REGISTRY.md`（不存在） | 已改 | 全仓唯一一套（单源纪律，见 `docs/engineering/STANDARDS_REGISTRY.md`） | 全仓唯一一套（依赖单源纪律，正本 = `docs/engineering/standards/DEPENDENCY.md`） | `test -e docs/engineering/STANDARDS_REGISTRY.md` → MISS；`docs/engineering/standards/DEPENDENCY.md:10` 逐字写「healpix_drizzle 依赖共享 HEALPix 核心，实现单源 = `lib/algorithms/shared/healpix/healpix_core.cpp`」，正是该页要引的「单源纪律」。 |
| A3 | `docs/detail/infrastructure/22_gaia_xpsd_client.md:13` | 同 A2 的坏路径 | 已改 | > XPSD 本地编码合同：docs/engineering/STANDARDS_REGISTRY.md（catalog 行） | > XPSD 本地编码合同：docs/science/algorithms/GAIA_QUERY.md「DR3SP 光谱量化解码」一节 | XPSD 本地编码（`flux_min`/`flux_mul` 量化解码）的真实正本在 `docs/science/algorithms/GAIA_QUERY.md` 的「DR3SP 光谱量化解码」一节（`grep -n "flux_min\|量化解码" docs/science/algorithms/GAIA_QUERY.md` → 命中）。星表行语义已由该页「数据正本」行指向 `DATA_SEMANTICS.md`「星表行语义」，不重复立第二份。 |
| A4 | `docs/detail/infrastructure/23_hips_browser.md:42` | 引 `eng/.../OPTIMIZATION.md`（省略号路径不可解析） | 已改 | （`eng/.../OPTIMIZATION.md` 的 STF 不重采样条款） | （`docs/engineering/standards/OPTIMIZATION.md` 的「STF 变化不重新采样」条款） | `test -e docs/engineering/standards/OPTIMIZATION.md` → OK；该文件逐字含「browser：screen→sky→HEALPix 每像素映射；STF 变化不重新采样」。 |
| A5 | `docs/detail/infrastructure/19_runtime.md:142` | 引 `lib/infrastructure/runtime/**`（目录不存在） | 已改 | 在代码侧的实体是 `lib/infrastructure/scheduler/src/{pipeline,`…`artifact,artifact_store}.cpp` 与 `lib/infrastructure/runtime/**`（docs/engineering/architecture/MODULE_MAP.md `id=runtime`） | 在代码侧的实体是 `lib/infrastructure/scheduler/src/pipeline.cpp`、`lib/infrastructure/scheduler/src/artifact.cpp`、`lib/infrastructure/scheduler/src/artifact_store.cpp`；三阶段产品交换的落盘面在 `lib/infrastructure/aio/runtime/artifact_store/`（`docs/engineering/architecture/MODULE_MAP.md`「三阶段产品交换」行） | `test -e lib/infrastructure/runtime` → MISS；`ls lib/infrastructure/scheduler/src/pipeline.cpp lib/infrastructure/scheduler/src/artifact.cpp lib/infrastructure/scheduler/src/artifact_store.cpp` → 三件俱在；`find lib/infrastructure/aio/runtime` → `artifact_store/`；`MODULE_MAP.md` 内 `grep -n "id=runtime"` → 0 命中（「runtime / io 平台单元」行的落点是 `lib/infrastructure/scheduler`、`lib/infrastructure/aio/io`）。**登记项见 §3-①**：MODULE_MAP 没有 `id=runtime` 条目，原文的三重指认（目录 + MODULE_MAP 条目）都落空。 |
| A6 | `docs/detail/infrastructure/19_runtime.md:19, 81` | 引裸文件名 `configs/stage1.schema.json` | 已改（两处） | `configs/stage1.schema.json` | `lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json` | `test -e lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json` → OK（`docs/detail/common.md:54` 早已用同一全路径，两页现已一致）。 |
| A7 | `docs/detail/infrastructure/18_cli.md:13` | 引 glob `eng/contracts/schemas/phase_config*.schema.json` | 已改 | `eng/contracts/schemas/phase_config*.schema.json` | `eng/contracts/schemas/phase_config_normalize.schema.json`、`eng/contracts/schemas/phase_config_mosaic.schema.json`、`eng/contracts/schemas/phase_config_export.schema.json` | `ls eng/contracts/schemas/phase_config*` → 恰三件，逐字展开。 |
| A8 | `docs/detail/infrastructure/17_aio.md:174` | 引花括号路径 `lib/infrastructure/aio/{include, src}/` | 已改 | `lib/infrastructure/aio/{include, src}/`。 | `lib/infrastructure/aio/include/` 与 `lib/infrastructure/aio/src/`。 | `ls -d lib/infrastructure/aio/include lib/infrastructure/aio/src` → 两目录俱在。 |
| A9 | `docs/detail/infrastructure/22_gaia_xpsd_client.md:17, 136` | 引花括号路径 `gaia_client.{h,c}` | 已改（两处） | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.{h,c}` | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.h` 与 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` | `test -e` 两文件俱 OK（与 `docs/engineering/governance/TRACEABILITY.md:214` 的登记 `SRC-CAT-GAIA-001 @ …gaia_client.c::…` 一致）。 |
| A10 | `docs/detail/common.md:62` | 引 `lib/algorithms/shared/healpix/tests`（目录不存在） | 已改（保留真内容、登记缺口） | `lib/algorithms/shared/healpix/tests`（Hipsgen oracle 对照、`query_disc` 保守性）、`astro_scalar` 分发；frame_id 与 product 的一致性由 … 保证。 | `lib/algorithms/shared/` 下当前**不承载共址测试目录**：HEALPix 几何的 Hipsgen oracle 对照、`query_disc` 保守性与 `astro_scalar` 分发的验证落点未在本库登记（待补，见「Known limitations」）；frame_id 与 product 的一致性由 … 保证。**并在「Known limitations」新增一条**：`- 本库无共址测试目录，上述三项验证的落点未在本库登记。` | `find lib/algorithms/shared -maxdepth 3` 输出无 `tests/`；`ls -d lib/algorithms/*/tests lib/algorithms/*/*/tests` → 2>/dev/null 无输出。三项验证的**意图是真的**，只是落点查不到，按红线 3 保留其名目并如实登记，不编造落点。 |
| A11 | `docs/detail/anchors/ANCHOR_CONTRACT.md:110` | 验证器报 `docs/science/detection/STAR_DETECTION.md",` 不存在 | **否决 · 保留原样**（误报） | 正例 JSON 里的 `"path": "docs/science/detection/STAR_DETECTION.md",` | 同左（未改一字） | `test -e docs/science/detection/STAR_DETECTION.md` → OK。脚本 `raw.rstrip(".,;:")` 不剥引号，JSON 示例尾部的 `",` 被吃进文件名 ⇒ 结构性误报。 |
| A12 | `docs/detail/anchors/ANCHOR_CONTRACT.md:117` | 派单问：这行用了行号，是否是故意演示负例？ | **否决审稿 · 保留原样** | 负例 F（行号混入）：登记面写 "docs/science/psf/PSF.md:7" 或锚里带 "line" 字段 -> D5 / D1 判红 | 同左（未改一字） | 已读上下文：本行位于「正例 / 负例」代码围栏块内，是 **判红清单的负例本体**，逐字写「登记面写 `文件:行` ⇒ D5/D1 判红」。它是演示判红的**标本**，删掉它等于删掉判据本身。另：`docs/science/psf/PSF.md` 实存，行号 `:7` 只是标本载荷。 |
| A13 | `docs/detail/LOG_AND_ERROR_SYSTEM.md:73` | 派单问：`run/task/node/...` 会被误读为路径？ | 已改（消歧义，未删字段名） | 单行结构化日志事件（run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic + 可选 error/progress/value） | 单行结构化日志事件（必含字段名：`run` / `task` / `node` / `module` / `phase` / `commit` / `host` / `level` / `event` / `units` / `elapsed` / `diagnostic`；可选字段名：`error` / `progress` / `value`） | 人工判读结论：原文确是**字段名列表**不是路径，但斜杠连写形似路径，机器扫描必误判（验证器已误判为 `run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic` 路径）。改法：加「字段名」标签 + 反引号逐个包裹，字段集一字未删。 |
| A14 | `docs/detail/LOG_AND_ERROR_SYSTEM.md:102` | 派单问：`run/<task>/logs/` 会被误读？ | 已改（消歧义） | （`run/<task>/logs/`） | （仓库 `run/` 目录下按任务名分的 `logs/` 子目录） | 人工判读：`<task>` 是占位符，但形态仍像可解析路径。改为自然语言描述，语义不变。 |
| A15 | `docs/detail/infrastructure/21_observability.md:107,113,138` | 验证器报 `file.h::symbol` 三处不存在 | **否决 · 保留原样**（误报） | `lib/infrastructure/cli/resource_gate.h::gate_enforcement` 等三处 | 同左（未改一字） | 逐个 `grep`：三文件俱存在且三个符号逐字命中（`gate_enforcement` ∈ `resource_gate.h`；`missing_required_extension_v1` ∈ `protocol.h`；`evaluate_frozen_gate` / `resolve_allocated_capacity` ∈ `run_monitored.py`）。`::` 是**符号锚**，正是 AGENTS 与本单红线 4 要求的「改成符号名」形态，脚本 `PATH_RE` 不识别 `::` 才误报。 |

### 1.2 B 组 · 跨正本口径冲突

| 编号 | 位置 | 问题 | 处置 | 改前逐字 | 改后逐字 | 依据 |
|---|---|---|---|---|---|---|
| B1 | `docs/detail/UNIFIED_MODEL.md:43` + 文末 | `[18]` 是**悬空引用**（全文无参考文献表）。派单指向 `docs/engineering/UNIFIED_OBJECTS.md` 的编号表取号 | 已改（另立真实参考文献表） | 方法学对标 PixInsight PSFSNR（…），但不逐字套用其功率比式[18] `(Σf)²/σ_n²`，… ／ 通量型口径 `F_ref/σ_F`（Horne 1986） | 方法学对标 PixInsight PSFSNR[2]（…），但不逐字套用其功率比式 `(Σf)²/σ_n²`，… ／ 通量型口径 `F_ref/σ_F`（Horne 1986[1]）＋ **文末新增「参考文献」章**，列 [1] Horne 1986（PASP 98:609, DOI 10.1086/131801）、[2] Conejero/Radice/Sartori, *New Image Weighting Algorithms in PixInsight*（https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html ） | 先核上游是否有可继承的编号：`grep -n "PixInsight\|PSFSNR" docs/engineering/UNIFIED_OBJECTS.md` → **exit 1，零命中**；该文件也无参考文献表。⇒ 无编号可继承，只能在 detail 侧立自己的文献表（AGENTS §5 允许「文末列参考文献」）。[1] 书目经 Crossref API 逐字核对（`https://api.crossref.org/works/10.1086/131801` → title / volume 98 / page 609 / 1986 / Horne K. 全对）。[2] 已抓原文（见 §5）。**无悬空引用残留**：全仓 `grep -n "\[[0-9][0-9]*\]" docs/detail/UNIFIED_MODEL.md` 只剩本表两条。 |
| B2 | `docs/detail/UNIFIED_MODEL.md:43/:45/:48` + `:62`；`PHASE2_DETAILED_DESIGN.md:21`；`PHASE1_DETAILED_DESIGN.md:186` | `F_ref` 口径**欠定义**：只写「逐帧参考通量」，读者无法分辨它与代码禁止的「逐帧检出通量中位数」。 | 已改（**补定义，不改结论**） | `sky_samples` 行：`F_ref` = 该帧逐帧参考通量，与 `frame_snr`/`sparse_snr_layer` 同口径 ／ `sparse_snr_layer` 行：同逐帧参考通量 `F_ref` | `sky_samples` 行：`F_ref` = 该帧的参考通量，口径见下文「参考通量基准」 ／ `sparse_snr_layer` 行：同参考通量 `F_ref`（逐帧取「冻结参考星等 `m_ref` 在本帧的仪器通量」，见下文「参考通量基准」）／ **新增「### 2.1 参考通量基准（`F_ref` 的口径）」子节**，写死 `F_ref,k = 10^(−0.4·(m_ref − ZP_k))`、`ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)`、`m_ref` 默认 6.0 且随产品落盘、帧间独立、配对性只约束**同帧**、**禁止**以本帧检出通量中位数回退、缺参即 fail-closed、跨帧比对限同一 `m_ref` 档 ／ PHASE2：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，冻结参考星等档 `m_ref` 在本帧的仪器通量；该档随产品落盘 | 见下方「推导」。 |
| B3 | `docs/detail/UNIFIED_MODEL.md:15` | 节名 `## 2. 数据对象（各自具名）` 只覆盖「对象定义」，未覆盖同一节实际还承载的「字段歧义消解」（`weight/value/mask/snr` 各字段各自具名、模糊字段名禁令、canonical 13 对象的口径） | 已改（节名同时覆盖两件事） | `## 2. 数据对象（各自具名）` | `## 2. 数据对象与字段歧义消解` | 另两篇给出的两个候选名（「13 个对象 → canonical schema → schema ID」见 `docs/engineering/UNIFIED_OBJECTS.md` 标题与「## 2. 13 个对象 → canonical schema → schema ID（对照表）」；「weight/value/scale/sigma/snr 歧义映射」见 `docs/engineering/data/ARTIFACTS.md`）**各自只覆盖本节的一半**。本节正文确实同时承载两件事：`:37–56` 是 13 对象定义 + 可否作权重，`:56` canonical 13 名单，`:58` `psfsw_robust_weight` 非现行对象，`:62` 「`weight/value/mask/snr` 各字段各自具名，一个字段承载一个含义」。故取两者并集的自然语言名。**给另两篇的确切新节名见 §4-①**。 |
| B4 | `docs/detail/UNIFIED_MODEL.md:3`；`docs/detail/00_INDEX.md:3` | 上游抬头用章/节号跨文档指向 | 已改（两处） | `> 上游：ACSD_DESIGN.md §3.1（数据对象）、§3.2（三类配置）` ／ `> 上游：docs/ACSD_DESIGN.md §0（文档权威与索引）、§8（软件架构）` | `> 上游：《ACSD 最高设计》的「数据对象与配置」一章（统一数据对象集、三类配置）` ／ `> 上游：《ACSD 最高设计》的「文档权威与索引」与「软件架构」两章` | 目标章名逐字核自 `grep -n "^#" docs/ACSD_DESIGN.md`：`## 3. 数据对象与配置`、`### 3.1 数据对象`、`### 3.2 三类配置`、`## 0. 文档权威与索引`、`## 8. 软件架构` 全部实存。 |
| B5 | `docs/detail/UNIFIED_MODEL.md:46` | 引 `实验/absolute-snr` | 已改（补全到可打开的文件） | 完整适用域图谱见 `实验/absolute-snr`。 | 完整适用域图谱见 `实验/absolute-snr/README.md`。 | `ls 实验/absolute-snr` → `README.md` 实存。 |
| B6 | 派单「`README.md:3` 的上游抬头」 | 派单称 detail 根级 `README.md:3` 有 `§3.1/§3.2` 上游抬头 | **不成立 · 未改** | `docs/detail/README.md:3` 全文 = 「二级细节文档：模块工作细节、数据对象、接口落地与阶段详细设计，由 `science/` 与 `engineering/` 一级正本推理产出，直接指导开发。…」，**无 `§`、无上游抬头行** | 同左（未改一字） | `cat docs/detail/README.md` 全文三行。同车道另外三份 README（`docs/detail/infrastructure/README.md`、`anchors/README.md`、`registry/README.md`）亦无 `§`。派单该条应是把 `00_INDEX.md:3` 重复列了一次；`00_INDEX.md:3` 已按 B4 改。 |

### 1.3 C 组 · 机械锚与历史叙事（写作用域内逐条处理）

处置口径（三档，逐条判读后定）：

- **改**：指向**本仓**文档、且不带章节标题的 `§N` / 「见 §N」/「第 N 章第 N 节」机械跳转锚 → 改成点名章节标题的自然语言。C1–C17 共 17 组替换；**`§` 出现总数由 255 处降到 46 处（净减 209 处）**，复跑命令见 §7.2 末行。
- **保留（`「X」一节` 形态，127 处）**：带引号的章节**标题**引用（如「错误对象与退出码映射」一节）。它携带的是标题而非编号，不属「机械跳转锚」，且是全仓一级正本通行的引用形态。一处未动。
- **保留（外部标准/文献节号，改后残留 34 行）**：`IVOA HiPS 1.0 §4.1/§5.1`、`WD-HiPS-2.0 §4.3.2`、`B&A96 §3/§4/§5/§6`、`Stetson 1987 §II`、`Triggs & Sdika 2006 §2`、`Kron 手册 v2.3 §6.4` 等。外部标准的章节号无法改名，删掉即丢失可核性；残留行已逐行确认全为外部目标。

| 编号 | 位置 | 改前逐字 | 改后逐字 |
|---|---|---|---|
| C1 | `docs/detail/anchors/ANCHOR_CONTRACT.md:3-4` | `> 上游：docs/ACSD_DESIGN.md §0（文档权威与索引）、§8.5（模块与 ABI）` / `> 状态面：AGENTS.md 第 8 节（SubAgent 无 git 写权限）与` | `> 上游：《ACSD 最高设计》的「文档权威与索引」一章与「软件架构」一章（模块与 ABI）` / `> 状态面：AGENTS.md「角色与协作」一章（子代理无 git 写权限）与` |
| C2 | `ANCHOR_CONTRACT.md:16,18,32,36,67,123,145-146,150` | `一律用内容锚（§4）…`；`只能逐条登记（§3）`；`**#N 是行号锚，§N 是章节引用**…引用章节一律写 §条 并写全路径`；`或在 §3 登记为已知漂移`；`归一化指纹（§4.2）`；`由 §5 的审查判据`；`一律按 §4 内容锚…仓内代码不再被文档以行号定位`；`**引用 Markdown 章节**写 §N，不要写 #N` | 全部改为点名章节标题（「内容锚（content anchor）」一章 /「外部锚的登记面」一章 /「归一化与指纹（唯一算法）」一节 /「审查判据」一章）；并把**该文件自己**的规则同步改掉：**`#N` 是行号锚，章节引用是章节名**，两者互不代用；**引用 Markdown 章节写章节标题，不要写行锚 `#N`**；「仓内代码**不**由文档以行号定位」。**这是实质订正**：该文原第 32 行要求「引用章节一律写 `§条`」，与其自身第 3/16/36/67/123/145 行的 `§N` 写法互相违背，现已自洽。 |
| C3 | `docs/detail/common.md:3-5,32,34,58` | `> 上游：docs/ACSD_DESIGN.md §8.5（…）、§3.3（…）、§3.1（…）`；`DATA_SEMANTICS.md §3.1`；`docs/ACSD_DESIGN.md §3.3`；`DATA_SEMANTICS.md §3.7` | 改为《ACSD 最高设计》的「软件架构」一章与「数据对象与配置」一章；`DATA_SEMANTICS.md`「HEALPix 索引与 tile 布局」一节；「精度归属」一节；`DATA_SEMANTICS.md`「溯源最小集」一节 |
| C4 | `docs/detail/infrastructure/17_aio.md:36-37,133` | `最高设计 docs/ACSD_DESIGN.md §8.4（…）、§10（…）、§9（…）`；`（最高设计 §7.3）` | 《ACSD 最高设计》的「软件架构」一章（顶层结构：aio 模块位）、「I/O 与原子产品」一章、「CPU 后端与资源」一章；《ACSD 最高设计》的「命令行合同」一章 |
| C5 | `docs/detail/infrastructure/18_cli.md:11,24,26` | `最高设计 ACSD_DESIGN.md §7（CLI 合同）、§4.5（运行前预检）、§7.3（…）`；`命令树（唯一）见最高设计 §7.1`；`见最高设计 §4.5` | 《ACSD 最高设计》的「命令行合同」一章、「运行前预检」一节、「错误传播与日志」一节；「命令行合同」一章；「运行前预检」一节 |
| C6 | `docs/detail/infrastructure/19_runtime.md`（14 处） | `> 上游：ACSD_DESIGN.md §8.1…§8.4`；`最高设计 ACSD_DESIGN.md §8.4…§9`；`见 21_observability.md §8`；`（最高设计 §7.1）`；`合同 §6`；`最高设计 §3/4/5`；`最高设计 §8.2、§9`；`最高设计 §4.5`（×4）；`见 21_observability §8`；`退出码见 §7.2`；`最高设计 §6.2`；`docs/detail/00_INDEX.md §2 第 2 列`；`最高设计 §7.3`（×3）；`顶层设计 §8.4` | 全部改为《ACSD 最高设计》的章名（「命令行合同」「软件架构」「CPU 后端与资源」「运行前预检」「数据对象与配置」「normalize」「mosaic」「export」）与本仓文档的章节标题；`合同 §6` → 指向 `LOG_AND_ERROR.md`「显式降级登记要件」一节。**改后 `grep -n "§" 19_runtime.md` → 0 命中。** |
| C7 | `docs/detail/infrastructure/20_benchmark.md:3,12` | `> 上游：ACSD_DESIGN.md §9（CPU 后端与资源）` | `> 上游：《ACSD 最高设计》的「CPU 后端与资源」一章` |
| C8 | `docs/detail/infrastructure/21_observability.md:3,12,58,123,107` | `> 上游：ACSD_DESIGN.md §7.3…§8.4`；`最高设计 ACSD_DESIGN.md §7.2…§9…§10`；`ACSD_DESIGN.md §0`；`§8.3 的 ④⑤⑥`；`ACSD_DESIGN.md §4.5` | 全部改为章名；`§8.3` → 「判据表」一节 |
| C9 | `docs/detail/infrastructure/22_gaia_xpsd_client.md:3-5,120,129` | `> 上游：docs/ACSD_DESIGN.md §8.4、§8.5、§3.3、§8.4、§9`；`GAIA_QUERY.md §2.9/§4/§5/§资源`；`DATA_SEMANTICS.md §4.3`；`GAIA_QUERY.md §5`；`GAIA_QUERY.md §资源` | 改为《ACSD 最高设计》的「软件架构」/「数据对象与配置」/「CPU 后端与资源」三章；`GAIA_QUERY.md` 的「汇总表的『缓存』行、并发模型、测试设计、资源安排各段」「测试设计一节」「资源安排一节」；`DATA_SEMANTICS.md`「星表行语义」 |
| C10 | `docs/detail/infrastructure/23_hips_browser.md:3-4,26,39,88,93` | `> 上游：docs/ACSD_DESIGN.md §8.4、§8.5、§1.3、§6.3`；`最高设计 ACSD_DESIGN.md §1.3、§8.4`；`最高设计 §6.3`；`docs/ACSD_DESIGN.md §12.4`；`最高设计 §1.3` | 改为《ACSD 最高设计》的「软件架构」/「项目定位」/「export：投影导出」/「验证层级与四层验收」各章 |
| C11 | `docs/detail/PHASE1_DETAILED_DESIGN.md`（9 组 17 处） | `> 上游：ACSD_DESIGN.md §4`；`上位：docs/ACSD_DESIGN.md（§0…§1…§2…§4…§12）`；`ACSD_DESIGN.md §4.2`（×2）；`细化见 §3.6`；`该合同 §6 D1–D3`；`LOG_AND_ERROR_SYSTEM.md §10`；`ACSD_DESIGN.md §4.4`；`ACSD_DESIGN.md §7.2`；`STAR_DETECTION.md §3.1`；`ACSD_DESIGN.md §2、§3.1` | 改为《ACSD 最高设计》normalize 一章的「节点流程」「输出合同」一节与「命令行合同」一章的「机器输出与退出码」一节；`LOG_AND_ERROR.md`「显式降级登记要件」一节；`STAR_DETECTION.md`「O3 检测阈值」一节 |
| C12 | `docs/detail/PHASE2_DETAILED_DESIGN.md`（10 组 12 处） | `> 上游：ACSD_DESIGN.md §5`；`DATA_SEMANTICS.md §3.4`；`PHASE1_DETAILED_DESIGN.md §7.1`；`ACSD_DESIGN.md §5.3/§5.4/§5.5/§2、§3.1`；`PHASE2_UPM.md §14a`；`§2 的三条 SNR 重建口径`；`docs/ACSD_DESIGN.md §12.4` | 改为《ACSD 最高设计》的「mosaic」章、「信噪比重建与逆方差叠加」「天光平面（UPM）」「逐像素排异」「核心科学方法：五个创新点」「数据对象」各节；`DATA_SEMANTICS.md`「量纲与逐像素语义」一节；「本章的三条 SNR 重建口径」 |
| C13 | `docs/detail/PHASE3_DETAILED_DESIGN.md`（13 组 17 处） | `> 上游：ACSD_DESIGN.md §6`（2 处）；`ACSD_DESIGN.md §6.3 输入语义守卫 / §6.4 硬约束`；`DATA_SEMANTICS.md §3.4/§3.6`；`ACSD_DESIGN.md §6.3 投影算法`；`docs/ACSD_DESIGN.md §12.4`（×3）；`UNIFIED_MODEL.md §3`；`继承 §2 的 TAN 冻结域`；`设计允许的几何（§8）`（×2）；`DATA_SEMANTICS.md §4.5`（×2）；`DATA_SEMANTICS.md §4.5 规则项` | 改为《ACSD 最高设计》的「export：投影导出」「投影算法」「FITS 产品」「验证层级与四层验收」各章/节；`DATA_SEMANTICS.md`「状态与失败语义」「量纲与二次律传播」各段；「WCS 计划」一章；「导出裁剪范围（crop）」一章；`UNIFIED_MODEL.md`「三类配置严格分离」一章 |
| C14 | `docs/detail/PRODUCT_STORAGE_FORM.md`（14 组 16 处） | `> 上游：ACSD_DESIGN.md §10、§4.4/§5/§6、附录 B`；`（§5）`；`（§7）`；`（§6）`；`见 §10.1`；`哈希口径见 §8`；`（§9.2）`；`判据正文见 §9.1/§9.2`；`（见 §9.3）`/`（§9.3）`（×2）；`日志合同 §2`；`（§8.2）`；`（§5.2）`；`DATA_SEMANTICS.md §3.2`；`按 §12.4 冻结` | 全部改为本文章节标题（「索引：块粒度，两层」「写路径」「部分覆盖与查询语义」「输入：Phase1 的形态切换键」「properties 与哈希口径」「哈希口径」「包围盒 TRIM（可选形态，默认不启用）」「文件系统打洞（默认启用，裸形态）」「与归档形态的关系（不变）」「数据集级覆盖索引」「溯源最小集」）与《ACSD 最高设计》的章名 |
| C15 | `docs/detail/STAR_DETECTION_IMPL_DESIGN.md`（18 组 21 处） | `判据级合同见 §2`；`见 §8-1 存疑`；`承载（§2）`；`见 §6.3`（×2）；`见 §5.11`；`见 §5.14`；`DAOFIND 门备置 §6.3`；`含 §6.1 CLEAN 门与 §6.3/§6.4`；`§5.11 全链`；`角标写法见 §1.2`；`按 §8 登记`；`按 §8 逐条登记`；`（§5 逐项给规格）`；`§6 的谱系化构造`；`（§8-5…）`；`（§3-3…）`（×3）；`（§4 条款）`（×5）；`（§3-2）`；`（§6.4）`；`（§4 重标定）`；`登记 §8`；`§3 输出合同`；`§8-2`；`§5.3 的…`；`§5.12 的…` | 全部改为本文章节标题（「行为等价合同（判据级）」「诚实边界与存疑清单」「DAOFIND roundness / sharpness 门」「O11 多星去重」「O14 孔径测光」「CLEAN 门（B&A96 §5）」「Kron 自适应孔径」「逐算子规格（O1–O16）」「谱系化构造与切换合同（备而未启）」「三条冻结合同定案」「实现边界」「证据边界」「O3 检测阈值」「O12 椭圆高斯 LM 拟合」）。**未改**的是外部文献节号与 `ALG §2`/`ALG §11.4`（跨文档，见 §4-②）。 |
| C16 | `docs/detail/merged_TROUBLESHOOTING.md`（4 处） | `> 上游：docs/ACSD_DESIGN.md §7.2、§7.3、`；`AGENTS.md 第 5 节`；`按 §3 的通用定位顺序`；`再回到 §4 的表` | 《ACSD 最高设计》的「命令行合同」一章；AGENTS.md「文档规范」一节；「通用定位顺序」一节；「症状主表」一节 |
| C17 | `docs/detail/LOG_AND_ERROR_SYSTEM.md`（余 6 处） | `> 上游：ACSD_DESIGN.md §7.3、§10`；`ACSD_DESIGN.md §7.3`；`最高设计 §10`；`合同 §4`（×2）；`21_observability.md §5`；`合同 §6 D1–D3`；`ACSD_DESIGN.md §4.4` | 改为章名/章节标题：「命令行合同」「I/O 与原子产品」「运行前预检」三章；`LOG_AND_ERROR.md`「错误对象与退出码映射」「显式降级登记要件」两节；`21_observability.md`「配置项」一章 |
| C18 | 历史叙事词全作用域扫 | `已删`/`旧版`/`作废`/`曾经`/`此前`/`轮次名+曾` | **零命中，零改动**。`grep -nE "此前|曾经|已作废|历史上|旧版|已删|曾[经是叫过改用]" docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md` → 仅 1 条命中且**不是历史叙事**：`PHASE1_DETAILED_DESIGN.md:56` 的「轮次数是求解器实现细节，不是流程语义」——「次数」是迭代次数名词，与时间叙事无关，按红线 4「数学/实现条件不判为历史叙事」保留。 |
| C19 | `不再` 逐条判读 | `LOG_AND_ERROR_SYSTEM.md:207`「**中止运行**：不再处理后续帧」；`17_aio.md:121`「调用方不再拥有 `aio_alloc` 的 buffer」；`ANCHOR_CONTRACT.md` 的「仓内代码不再被文档以行号定位」 | 前两条按红线 4「行为口径里的『不再回读』要改」改为状态直述：**「中止运行**：不处理后续帧」**、**「成功接管后调用方不持有 `aio_alloc` 的 buffer」**；第三条已随 C2 改为「仓内代码**不**由文档以行号定位」。 |
| C20 | 日期 / 版本号 / 流水号 / commit / 元信息块 | — | **零命中，零改动**。`grep -nE "[0-9]{4}-[0-9]{2}-[0-9]{2}|[0-9]{4} 年 [0-9]+ 月" …` → none；`grep -nE "^(状态|版本|日期|作者|ID)\s*:" …` → none。`R2` 唯一命中是 `LOG_AND_ERROR_SYSTEM.md:191` 的判据编号「R2 降级显式」——是**在册判据 ID**（判据表内编号），非流水号。`V1a/V1b`（`PHASE3_DETAILED_DESIGN.md` 判据名）、`V5`（`STAR_DETECTION_ALGORITHMS.md` 章名）同属在册命名。`commit` 唯一命中是 `LOG_AND_ERROR_SYSTEM.md:73/113/144` 的**日志字段名**（运行现场的构建 SHA），不是文档的 git 叙事。 |
| C22 | `docs/detail/PRODUCT_STORAGE_FORM.md:249` | 缺一个开引号，`manifest.json` 路径的 code span 未闭合（全文件反引号 413 个，奇数）| 已改（一字之补） | `<output_dir>/manifest.json` 新增 `storage` 段（加性） | `` `<output_dir>/manifest.json` `` 新增 `storage` 段（加性） | 该缺陷**非本车道引入**：逐字比对 `git -c core.quotepath=false show HEAD:docs/detail/PRODUCT_STORAGE_FORM.md` 同行为同一串（缺开引号）。按红线 3 只补回引号，正文一字未删。改后全文件反引号 414 个（偶数），是写作用域内**唯一**一个反引号奇偶失衡的文件（复跑：`for f in docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md; do echo "$(grep -o '`' $f | wc -l) $f"; done`）。 |
| C21 | 本仓源码行号 | — | **零命中，零改动**。`grep -nE "[A-Za-z0-9_/.-]+\.(md\|cpp\|c\|h\|hpp\|json\|py)\:[0-9]+" …` 在 detail 正文只命中两处：`ANCHOR_CONTRACT.md:16/117`，都是**规则/负例标本**（C2、C-A12 已处置）。`STAR_DETECTION_IMPL_DESIGN.md:252/253/254/256/283/287` 的 `src/refine.c:97` 等是**上游开源实现**的取证位置，见 §4-③。 |

### 1.4 D 组 · detail 层失效推导

| 编号 | 位置 | 问题 | 处置 | 依据 |
|---|---|---|---|---|
| D1a | `docs/detail/STAR_DETECTION_IMPL_DESIGN.md` | 审稿称 detail 侧复制了「噪声估计器差分方向写错」（文档「相邻行之间」vs 代码「同一行内相邻列」） | **不成立 · detail 侧零改动** | 生产代码 `lib/algorithms/star_detection/src/sdet_api.cpp` 的 `sdet_compute_bgnoise` 内：`row = img + y*width` 锁 y；`for (x = 1; x < width; ++x)` 沿 x；`diff = row[x] - row[x-1]` ⇒ 确为**同一行内相邻列**差分。detail `:100` 写「行内相邻差分 `d(x,y) = I(x+1,y) − I(x,y)`」，是同一差分的下标平移，**方向正确**。`grep -nE "相邻行\|行间差分\|上下行\|纵向差分\|垂直差分" docs/detail/STAR_DETECTION_IMPL_DESIGN.md` → exit 1（零命中）。**写错的是一级正本 `docs/science/detection/STAR_DETECTION.md:32` 与 `:152`**（均在禁区，见 §3-②）。 |
| D1b | 同上 | 审稿称 detail 侧复制了「`bgnoise` 定义漏 `×1/√2`」 | **不成立 · detail 侧零改动** | 代码 `sdet_api.cpp`：`med_std = robust_median(各行 std)`，`return med_std * 0.70710678118654752`。detail `:103` 写 `σ ≈ std(d)/√2`，`:106` 写「跨行取中位再乘 1/√2」，并附 `:107-109` 的推导 ⇒ **detail 侧比一级正本更完整**。**一级正本 `docs/science/detection/STAR_DETECTION.md:50` 全文不含 `0.707/√2/1/√`**（`grep -n "0\.707\|√2\|sqrt(2)\|1/√" …` → exit 1），即正本高估 `bgnoise` 一个 √2 倍（见 §3-③）。 |
| D1c | `docs/detail/STAR_DETECTION_IMPL_DESIGN.md` | 一级正本内部的 self-contradiction 是否波及 detail | **登记** | `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:34-35`（同为一级正本）逐字写对方向 + `×0.70710678118654752`。detail `:109` 引的「ALG §2」指向的正是**对的那一份** ⇒ detail 的引用链通向正确侧。裁定「哪份是 `bgnoise` 定义正本」属一级正本层，见 §3-④。 |
| D2 | `docs/detail/PHASE1_DETAILED_DESIGN.md:160,169` | 审稿称 `m_5` 公式系数应为 `2.5`，并已被点名为「与 DATA_SEMANTICS 同式同系数 2.5，**无需改动**」 | **审稿已撤回 · 本车道复核后保留原样，仅补推导与出处** | 独立复核三重闭环：(a) **代码**：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp` 的 `snr_frame_depth_f64` — `f5 = 5.0 * reference->sigma_f_optimal_adu; *out_m5_mag = zero_point_mag - 2.5 * std::log10(f5);`；(b) **一级正本**：`docs/science/noise_snr/NOISE_SNR.md` `m_5 = ZP - 2.5 * log10( 5 * sigma_F(ref) )`，`docs/science/unified/DATA_SEMANTICS.md` 「5σ 深度按 `F_5 = 5·σ_F(ref)` 换算」；(c) **自推**：`2.5` 是星等定义式 `m = ZP − 2.5·log10 F` 的换算常数（对数以 10 为底时的 `1/ln 10 × 2.5`），**不是深度倍数**；深度倍数是那个独立的 `5`（5σ）。三者逐字一致，无系数可改。已就地补一句说明，防后人再误判：「系数 `2.5` 是星等定义式的换算常数，不是深度倍数」，并补上 `F_5 = 5·σ_F(ref)` 与两处正本节名。 |
| D3 | `docs/detail/PRODUCT_STORAGE_FORM.md` | 与 `docs/engineering/contracts/HIPS_STORAGE_FORM.md`、`docs/ACSD_DESIGN.md`「I/O 与原子产品」一章在 HiPS 两形态与哈希口径上的一致性 | **已核：形态骨架三方一致；detail 侧补 9 处 detail 缺项 / 失实引用**（7 切面 + 不一致清单见下） | 三方独立核对（detail × 工程合同 × 最高设计第十章 + 机器事实源 `eng/contracts/schemas/hips_storage_form.schema.json`） |  |

**D3 七切面核对结果（detail 侧零改动）**

| 切面 | 三方判定 | 逐字依据 |
|---|---|---|
| 形态集合与命名 | **一致** | 最高设计 `ACSD_DESIGN.md`「I/O 与原子产品」一章逐字「HiPS 产品两种形态：裸 `.hips/` 目录与归档 `.hips.zst`（整包 tar + 逐成员 zstd），两形态互斥且同身份（哈希取解压后内容）」；合同有「## 命名与扩展名」「## 归档容器布局」两章；detail `PRODUCT_STORAGE_FORM.md` 的「两种落盘形态」「归档容器布局」段同。 |
| 归档内容合法性 | **一致** | detail「归档形态合法，**前提是解压后得到目录/文件视图**……`properties` 必须与裸形态逐字节一致」；合同 H1「同一产品两形态的 `tree_hash` **必须相同**（身份与打包参数无关）」。 |
| 逐瓦片压缩 | **一致判为不在设计内** | detail「本设计不引入逐瓦片压缩，不引入自定义容器格式」；`hips_tile_format` 词表无对应 token。 |
| **哈希口径** | **一致（本单最重的一条，逐字对齐）** | 三方逐字：合同 `tree_hash` = 解压后 HiPS 内容的**条目三元组数组** `[[path,size,sha256],…]` 按 `(path,size,sha256)` 升序后 `sha256(canonical_json)`，冻结序列化参数 UTF-8 / `ensure_ascii=false` / 分隔符 `(",",":")`；detail 逐字同（另补了执行面 `lib/infrastructure/aio/io/hips_output_store.py`）；最高设计逐字「同身份（哈希取解压后内容）」。「容器指纹 ≠ 产品身份」三条齐备：合同 `archive_sha256` 用途「限于容器面（产品身份 = `tree_hash`）」、detail「`archive_sha256` 是**容器指纹**，不是产品身份」、最高设计「哈希取解压后内容」。 |
| 形态切换键 | **一致** | 合同 `storage_form` = `archive`（默认）\| `bare`，缺省/空串/`null` ⇒ 取默认 + 报一条 `level=warn`，`form_source` = `config`/`default`；detail 逐字同（`archive` 默认 / `bare`，缺省留空取默认并记 warn，`form_source` 两值）。不变式 F0 两边同形。 |
| 形态适用面 | **一致** | 合同「mosaic 固定裸 `<name>.hips/`（服务面）/ export 固定裸 FITS（不压缩、不套壳）」+「该键**只**属 normalize，mosaic / export 出现即 REJECT」；detail「Phase2 固定裸形态（服务面）、Phase3 固定裸 FITS（不套壳）」「Phase2/Phase3 的输入合同**不设**形态键，出现即 REJECT」，并把「为什么不用值域收窄」的**同一条**理由也写全了（键白名单 vs JSON Schema，`archive` 会静默透传成 no-op）。 |
| 体积削减 | **一致** | 合同 T1 打洞（字节不变、仅裸形态、非 fail-closed）、T2 TRIM（改 FITS 结构、**默认不启用**、读端不认则 fail-closed）；detail「9.1 文件系统打洞」「9.2 包围盒 TRIM」+「9.3 与归档形态的关系（不变）」逐条同形，含 T2 改变产品身份需在 `properties` 与 manifest `storage` 段显式区分。 |

**D3 不一致清单 —— detail 侧已改的 9 处（本车道写作用域内）**

| # | 位置 | 问题 | 改后逐字 | 依据 |
|---|---|---|---|---|
| 5-1 | `PRODUCT_STORAGE_FORM.md:161` | **detail 缺项**：`hips_tile_format` 只写「取标准 token 集」，**漏掉合同要求的两档词表**（目录子产品 `snr/` = `tsv`）。全篇 `grep -n "tsv\|snr" docs/detail/PRODUCT_STORAGE_FORM.md` → 零命中 | 「**`hips_tile_format` 分两档**（逐子产品档位表 = `eng/contracts/schemas/hips_storage_form.schema.json` 的 `x-acsd-field-vocabulary.hips_tile_format_two_tiers.by_subproduct`）：- **Image 产品**子产品 `signal` / `support` / `variance` / `ivar` = `fits`；- **HiPS 目录（catalogue）**子产品 `snr/` = `tsv`（VOTable 元数据 + `.tsv` 控制点，承载 SNR 精度锚）。停写 `tsv` 等于撤掉已冻结的科学载体。档位不符或任何非标准 token 一律判红…」 | 合同 A4（`:51`）与错误语义（`:177`）；`python3 -c "import json;print(json.load(open('eng/contracts/schemas/hips_storage_form.schema.json'))['x-acsd-field-vocabulary']['hips_tile_format_two_tiers']['by_subproduct'])"` → `{"signal":"fits","support":"fits","variance":"fits","ivar":"fits","snr":"tsv"}` |
| 5-2 | `:160` | 同源：把 `hips_tile_format=fits` 写成**全产品统一值** | 从括号里删去 `hips_tile_format=fits`，改为「`hips_tile_format` **按子产品取登记档位**，不是全产品统一值，见下条」 | 同 5-1 |
| 5-3 | `:162` | 同源：判据只说「必须是既有读端接受的取值」 | 「`hips_tile_format` 必须**等于该子产品的登记档位**（Image 四层 = `fits`，目录子产品 `snr/` = `tsv`），非登记 token 判红、既有读端以 `UNSUPPORTED` 拒绝」 | 同 5-1 |
| 5-4 | `:18` | **detail 缺项（读侧降级面）**：只写「两形态都由产品级索引伴随，索引是产品的组成部分」，**漏掉合同 N6/N7 的读侧分档**（归档缺索引 fail-closed / 裸形态缺索引按目录枚举重建并记降级）。detail 反而单方收紧了写侧 | 「**写侧**两形态都写产品级索引……**读侧缺失处理按形态分档**：归档形态缺索引 ⇒ 产品不完整，fail-closed（判定只走索引面…）；裸形态允许无索引 ⇒ 读端按目录枚举重建覆盖，并在 provenance 记降级」 | 合同 N6（`:36`）/ N7（`:37`）/ R5（`:151`） |
| 5-5 | `:208` | **detail 缺项（哈希口径的关键后果）**：TRIM 改 FITS 结构 ⇒ **产品身份改变**，必须在 `properties` 与 manifest `storage` 段显式区分；detail 只写「显式声明该形态」，无身份后果 | 追加一条「**对产品身份的影响**：TRIM 改 FITS 结构 ⇒ 启用 TRIM 的产品与未 TRIM 的同源产品**产品身份不同**；两个身份都必须在 `properties` 与运行完成清单 `manifest.json#storage` 段显式具名区分，静默分化判红」 | 合同 T2 身份后果（`:164`）；与 detail 自身「哈希口径」一章同源 |
| 5-6 | `:211` | **引用失实**：补边位模式称「按《ACSD 最高设计》的「验证层级与四层验收」一节冻结为 IEEE NaN」。`grep -c "IEEE" docs/ACSD_DESIGN.md` → **0 命中**；该节只冻结 NaN/Inf/缺失的**位置与语义**一致，不含位型 | 改为「补边位模式 = IEEE NaN；该冻结面的正本 = `docs/engineering/contracts/HIPS_STORAGE_FORM.md`「体积削减（裸形态）」一章 T2 判据①。（最高设计只冻结位置与语义，不含位型）」 | `grep -n "IEEE" docs/ACSD_DESIGN.md` → 0；合同 T2 判据①（`:161`） |
| 5-7 | `:190` | **本车道自身引入的失实指针**：C14 轮把 `DATA_SEMANTICS.md §3.2` 展开成不存在的标题「未覆盖像素一律 invalid（signal=NaN/support=0）」。该字符串全仓**只出现在 detail 自身**；`DATA_SEMANTICS.md` 的真实标题是 `### 3.2 三个基本对象的语义` | 改为「`docs/science/unified/DATA_SEMANTICS.md`「三个基本对象的语义」一节：NaN 是无效值的唯一载体、`invalid` 判据为「NaN 或 `support <= 0`」、无覆盖行的 `signal` 记 NaN 而非 0」 | `grep -n "^### 3.2" docs/science/unified/DATA_SEMANTICS.md` → `### 3.2 三个基本对象的语义`；`grep -rn "signal=NaN/support=0" docs lib eng 实验` → 只命中 detail 自身 |
| 5-8 | `:137` | detail 只写「远低于载体切换阈值」，未给数字 | 补为「远低于载体切换阈值 `10^7` 条（切换载体后字段模型与不变式不变）」 | 合同「载体演进」一节（`:129`） |
| 5-9 | `:16`、`:173` | 两处指针面不完整：Phase2 `.mosaic` 中缀在仓内其他正本**无登记面**；`tree_hash` 执行面单指一个文件，未排除与 scheduler 的带域前缀哈希的歧义 | `:16` 补「该中缀取自 Phase2 产物族命名规则」；`:173` 补「另有 `lib/infrastructure/scheduler/src/canonical_hash.cpp` 的带域前缀 per-file 规范哈希，作用域不同：`tree_hash` 只取前者」 | `grep -rn "\.mosaic\b" docs --include=*.md \| grep -v PRODUCT_STORAGE_FORM` → 空；`ls lib/infrastructure/scheduler/src/canonical_hash.cpp` → 实存 |

**D3 附带发现（登记，均在禁区，本车道未动）**：

| 编号 | 落点 | 问题 |
|---|---|---|
| 3-⑨ | `docs/engineering/contracts/HIPS_STORAGE_FORM.md` 的「体积削减（裸形态）」一节（另见 `:3`、`:156`、`:165`） | 该节两处逐字写「机制与推导见 `docs/detail/infrastructure/` 的产品存储形态设计 「形态核对与负例」一节」。但 `docs/detail/infrastructure/` 下**没有**产品存储形态设计文档（该目录只有 17–23 七张基建卡 + README），而「形态核对与负例」是**该合同自身**的一节（`:179`）。⇒ **悬空反向指针 + 跨层反向定义**。真实落点是 `docs/detail/PRODUCT_STORAGE_FORM.md` 的「体积削减：稀疏打洞（默认）与包围盒 TRIM（可选形态）」一章（schema `x-acsd-design` 也指向该文件）。**建议**：把合同的三处指针改指 detail 的该章，或在 detail 增一节接住。 |
| 3-⑩ | 最高设计 `docs/ACSD_DESIGN.md` | schema 自带的逐层词表要求最高设计含五词：`"layer": "top-design", "file": "docs/ACSD_DESIGN.md", "must_contain": ["storage_form","archive","bare",".hips.index.json","coverage.index.json"]`；实测 `docs/ACSD_DESIGN.md` **五词命中 0/5**（`for t in storage_form archive bare .hips.index.json coverage.index.json; do grep -c -- "$t" docs/ACSD_DESIGN.md; done`）。按合同 `:194`「缺登记词…判红」，**最高设计层按现行词表应判红**。 |
| 3-⑪ | 合同 `:161` 与 detail `:188`/`:212` | TRIM 依据的**归因分歧**：detail 归 IVOA `WD-HiPS-2.0-20260501 §4.3.2`（工作草案）；合同 `:161` 把 `TRIM1/TRIM2/ONAXIS1/ONAXIS2` 归到参考文献 `[2]` = FITS 标准 4.0（`grep -n "WD-HiPS\|4\.3\.2" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → **零命中**）。⇒ 无法判定一致，**需一手标准原文裁决**（仓内无 FITS 4.0 与该草案正文）。登记 UNRESOLVED。 |
| 3-⑫ | schema `frame_storage.index_path` 的 description vs 合同 N7 | schema 逐字「**两形态都必须有**」（写侧），合同 N7「裸形态**允许**无产品级索引」（读侧容忍）。二者不必然矛盾，但需在任一侧写明「写必产 / 读可降级」。detail 侧已按此口径写全（5-4）。 |
| 3-⑬ | schema `:13` 声明的检查器 `CHK-HIPS-STORAGE-FORM` | `grep -rln "CHK-HIPS-STORAGE-FORM" eng docs lib` 只命中 schema 自身与 `eng/contracts/data/unified_object_registry.json`，**实现位置未核到**。需补充。 |

| 编号 | 落点 | 问题 |
|---|---|---|
| 3-⑨ | `docs/engineering/contracts/HIPS_STORAGE_FORM.md` 的「体积削减（裸形态）」一节 | 该节两处逐字写「机制与推导见 `docs/detail/infrastructure/` 的产品存储形态设计 「形态核对与负例」一节」。但 `docs/detail/infrastructure/` 下**没有**产品存储形态设计文档（该目录只有 17–23 七张基建卡 + README），而「形态核对与负例」是**该合同自身**的一节。⇒ **悬空反向指针**。真实落点应是 `docs/detail/PRODUCT_STORAGE_FORM.md` 的「体积削减：稀疏打洞（默认）与包围盒 TRIM（可选形态）」一章。 |
| 3-⑩ | 合同的「体积削减」一节 vs detail 的同名章 | 合同逐字「signal/variance/ivar **三层**的收益口径另计」，detail 逐字「signal / support / variance / ivar **四层**都尝试」。schema `hips_storage_form.schema.json` 里 signal/support/variance/ivar 四名俱在。两句不必然矛盾（合同说的是**收益口径分列的三层**，detail 说的是**打洞尝试的四层**），但字面不同，读者会读成矛盾。**需 owner 裁定统一措辞**。 |

---

## 2. 你否决的条目（保留审稿/派单原判不抹除）

| 条目 | 原判 | 我的处置 | 理由（可核） |
|---|---|---|---|
| `PHASE3_DETAILED_DESIGN.md:131` 的 `phase_config_export.schema.json#/$defs/export_crop` | 「该路径不存在」 | **保留原样，不动一字** | 文件实存；`$defs.export_crop` 实存（`python3 -c "import json;print(list(json.load(open('eng/contracts/schemas/phase_config_export.schema.json'))['$defs']))"` → 列表含 `export_crop`）。JSON Pointer `#/$defs/export_crop` 合法。脚本 `resolve()` 的 fragment 字符类不含 `/`，是把 `/$defs/export_crop` 整段当文件名去 `os.path.exists` ⇒ **脚本缺陷，非文档缺陷**。 |
| `ANCHOR_CONTRACT.md:117` 的 `docs/science/psf/PSF.md:7` | 「本节自己在示例里用了行号」 | **保留原样，不动一字** | 读上下文后判定：这是「正例 / 负例」围栏块里的**负例 F 本体**，逐字功能就是演示「登记面写 `文件:行` ⇒ D5/D1 判红」。它是被判红的标本，不是正例。删它 = 删判据。 |
| `21_observability.md:107/113/138` 的 `file::symbol` | 验证器报「不存在」 | **保留原样，不动一字** | 三文件实存、三符号逐字命中。`::` 是符号锚，正是 AGENTS 与红线 4 要求的形态；脚本 `PATH_RE` 不识别 `::` ⇒ 误报。 |
| `common.md:23` / `22_gaia:13` 的 `docs/engineering/STANDARDS_REGISTRY.md`（catalog 行） | 「改指真实路径」 | **已改，但改指对象与派单预设不同** | 派单猜测在 `docs/engineering/governance/` 或 `docs/engineering/standards/` 找「标准登记册」。实测 `docs/engineering/` 全树 44 份 md 中**没有任何文件叫 STANDARDS_REGISTRY**，也没有任何一份承载「标准登记册」这一实体：`docs/engineering/standards/` 是八项标准的目录，`docs/engineering/governance/` 是治理正本目录（README 自述：文档治理规范 / TRACEABILITY / DUAL_LINE / UNRESOLVED）。所以我按**这两处各自真正要引的内容**分别改指（单源纪律 → `standards/DEPENDENCY.md`；XPSD 编码 → `GAIA_QUERY.md` 的量化解码一节），而不是硬造一个「登记册」落点。 |
| B 组 2 的前提「生产代码明文禁止逐帧 `F_ref`」 | 「代码禁止逐帧 `F_ref`，detail 写『逐帧参考通量』⇒ 冲突」 | **部分否决：结论保留、定义补齐** | 代码禁止的是**逐帧检出通量中位数回退**当 `F_ref`，不是「逐帧 `F_ref`」这个作用域。铁证三处：(a) `snr_frame_science.cpp` 注释逐字「逐帧检出通量中位数**回退**会丢掉帧间标度因子 `a_f²`」——禁的是回退；(b) `astro_sphere_sink.cpp` 注释逐字「此时逐帧 flux_adu 是**有意**逐帧的（`F_ref,k = 10^(-0.4(m_ref-ZP_k))`），**不是缺陷**」；(c) `module_adapters.cpp` 注释逐字「配对性定理只要求**同一帧内** SNR 与 F_ref 同源，**不要求跨帧相等**……跨帧一致性降级为**报告字段**，不再是门」。一级正本 `NOISE_SNR.md` 也逐字把「逐帧参考通量」称作「**本链的生产口径**」。⇒ **detail 原句是对的**（红线 3：不得为消问题而删真内容），我做的是把「`F_ref` 到底是什么」补成可核定义，而不是删掉「逐帧」。见 §3-⑤。 |

---

## 3. 需代码侧 / 一级正本侧订正的问题（均在禁区，本车道只登记）

| 编号 | 落点 | 问题 | 证据 | 影响 | 建议处置（交前台裁定） |
|---|---|---|---|---|---|
| 3-① | `docs/engineering/architecture/MODULE_MAP.md` + `docs/detail/infrastructure/19_runtime.md:143` | MODULE_MAP **没有 `id=runtime` 条目**。`grep -n "runtime" MODULE_MAP.md` 只命中「runtime / io 平台单元 \| `lib/infrastructure/scheduler`、`lib/infrastructure/aio/io`」一行。detail 原文却按 `id=runtime` 反查 `lib/infrastructure/runtime/**`，而该目录不存在。 | 见上 | detail 侧已按 MODULE_MAP 的「三阶段产品交换」行改指 `lib/infrastructure/aio/runtime/artifact_store/`（该行指的就是 `lib/infrastructure/aio/runtime/artifact_store`）。但 MODULE_MAP 缺 `runtime` 模块条目这一事实本身未修 | 请工程正本车道确认：是补一条 `id=runtime` 条目，还是确认「runtime 只作为路径词、不作模块名」（detail `19_runtime.md` 第 8 行明写「`runtime` 不是模块名（禁第二名字）」——**两者当前互相矛盾**） |
| 3-② | `docs/science/detection/STAR_DETECTION.md:32`、`:152` | 噪声估计器的差分方向写错：正本写「**相邻行之间**的差分」，生产代码 `sdet_api.cpp` 的 `sdet_compute_bgnoise` 是**同一行内相邻列**差分（`row[x] - row[x-1]`，行指针锁 y）。 | `sed -n '707,725p' lib/algorithms/star_detection/src/sdet_api.cpp` | 纯描述性错误。**数值影响为零**：同一 σ 场下换到 y 轴差分，`std(d)/√2` 不变，`bgnoise` 与阈值均不变。但它会让读者误以为跨行增益差不参与；且正本自己给的理由（「信号缓变时只剩噪声」）在「相邻行」方向下**不成立**（沿 y 的线性斜坡经相邻行差分仍是斜坡差 g，变成常数台阶），文字与理由互相打架 | 请 science 正本车道改为「行内相邻列一阶差分」，并同步 `:152` 的表格行 |
| 3-③ | `docs/science/detection/STAR_DETECTION.md:50` | `bgnoise` 定义漏 `×1/√2`。正本逐字「`bgnoise` 是未平滑原图的行差分背景噪声均方根」，即 `std(d) = √2·σ`；全文不含 `0.707/√2/1/√`（`grep -n "0\.707\|√2\|sqrt(2)\|1/√" …` → exit 1）。生产代码 `sdet_api.cpp` 是 `med_std * 0.70710678118654752`。 | 同上 | 正本相对代码高估 `bgnoise` 约 √2 倍 ⇒ `median(img) + 5.0·bgnoise` 的阈值相对实现虚高约 √2 倍。detail `:103/:106` 写的是**对的**（含因子与推导），所以这是「正本低、detail 高」的空档 | 请 science 正本车道补 ÷√2（或直接改引 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:34-36`） |
| 3-④ | `docs/science/detection/STAR_DETECTION.md` vs `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` | 一级正本层**自相矛盾**：前者方向错 + 漏因子，后者方向与因子全对。按 AGENTS「同一主题只有一份正本」，需裁定以哪份为 `bgnoise` 定义正本。 | `sed -n '34,36p' docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` | detail `:109` 引的「ALG §2」指向**对的那一份** ⇒ detail 引用链目前通向正确侧，但正本层的不一致会让「以更高一层为准」的裁决失去唯一落点 | 请前台裁定正本归属后，两份对齐 |
| 3-⑤ | `docs/ACSD_DESIGN.md:152`（最高设计，禁区） | 派单称此处「逐帧参考通量」与代码冲突。**复核后判定：实质一致，欠定义**。`ACSD_DESIGN.md:152` 逐字「帧级信噪比与稀疏层是两个独立对象，共用同一物理定义与逐帧参考通量」——「逐帧参考通量」是一级正本 `NOISE_SNR.md:271` 自称的「本链的生产口径」，与代码一致。问题在于**该词没定义**：既没写 `F_ref,k = 10^(−0.4(m_ref − ZP_k))`，也没写「禁止以本帧检出通量中位数替代」，读者会误读成「逐帧实测通量」。 | `sed -n '152p' docs/ACSD_DESIGN.md`；`sed -n '104,114p' docs/ACSD_DESIGN.md`；`sed -n '270,276p' docs/science/noise_snr/NOISE_SNR.md` | detail 侧已补完整定义（B2）。最高设计与 science 侧仍是欠定义 | 请前台把本车道 B2 新增的「参考通量基准」定义**上提到 ACSD_DESIGN.md 与 `NOISE_SNR.md`**，否则 detail 反向定义上游、违反权威链方向 |
| 3-⑥ | `docs/engineering/UNIFIED_OBJECTS.md:1`、`:119` 等 | 两处**错误路径** `docs/detail/common/UNIFIED_MODEL`（真实是 `docs/detail/UNIFIED_MODEL.md`）。`UNIFIED_OBJECTS.md:119` 亦写 `docs/detail/infrastructure/gaia_xpsd_client`（真实带 `.md`）。 | `ls docs/detail/common 2>/dev/null` → 不存在 | detail 侧**不能改**（在 `docs/engineering/**` 禁区） | 请工程正本车道改为 `docs/detail/UNIFIED_MODEL.md` 与 `docs/detail/infrastructure/22_gaia_xpsd_client.md` |
| 3-⑦ | `docs/detail/registry/**`（子代理 B 的面） | `registry/acsd.phase1.noise-snr.md` 等卡内仍有 `lib/.../{a,b}` 花括号路径（同 A8/A9 形态）。派单点名「`registry/...` 里的花括号路径」但明示不由我改。 | 同 A8/A9 | 同 A8/A9 | 请 B 车道按同一口径展开 |
| 3-⑧ | `docs/detail/README.md`、`docs/detail/infrastructure/README.md`、`docs/detail/anchors/README.md` | 三份 README 的上游抬头用「由 `science/` 与 `engineering/` 一级正本推理产出」这类**路径词**指代，未点名具体文件与标题（派单 B4-3 只点了根 README）。 | — | 不构成坏路径 | **保留**。AGENTS §5「每个文件夹内放一个极简 README，用一两句说明该文件夹放什么」对 README 有豁免意图；改长反而违反体例。若前台要求统一形态，请裁定 README 是否整体豁免。 |

---

## 4. 需权威补充才能定的问题

| 编号 | 需补充什么 | 为什么我不能自己定 |
|---|---|---|
| 4-① | **另两篇应改指的「确切新节名」已给出，但路径在别人写面。** B3 后 `docs/detail/UNIFIED_MODEL.md` 的节名是 **`## 2. 数据对象与字段歧义消解`**。`docs/engineering/UNIFIED_OBJECTS.md` 与 `docs/engineering/data/ARTIFACTS.md` 当前各叫一个**不同的、且只覆盖一半**的名字（「13 个对象 → canonical schema → schema ID」/「weight/value/scale/sigma/snr 歧义映射」）。二者都应改指同一新节名 **`「2. 数据对象与字段歧义消解」`**（含二级正本文件名）。**路径订正同批**：`docs/detail/common/UNIFIED_MODEL` → `docs/detail/UNIFIED_MODEL.md`。均在 `docs/engineering/**` 禁区，我不改。 | 权威链：detail 由一级正本推理产出；改一级正本指/detail 的名字要一级正本车道动。 |
| 4-② | ~~**已关闭。**~~ 跨文档机械锚 `ALG §2`（7 处）/ `ALG §11.4`（8 处）/ `ALG §3`（1 处）已在本车道全部改为点名 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` 的章节标题（「离散公式」一章 /「TEST-STAR-DESIGN-001 冻结测试设计」一节 /「伪代码」一章），章名逐字核自 `grep -n "^#" docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md`。**仅剩** `「X」一节` 形态（115 处，属体例问题）与外部标准/文献节号（34 行，外部目标不可改名）两类登记。 | — |
| 4-③ | **上游标准/文献原文的逐字核对。** (a) PixInsight 页的公式以图片渲染，抓取到的文本里公式被丢弃，**`(Σf)²/σ_n²` 的上下标字面形态未能逐字核对**（页面正文只给到结构：2.6 节标题「The PSF SNR Robust Estimator of Signal-To-Noise Ratio」+ 逐字「PSF SNR is a realization of the signal-to-noise ratio formulation based on the **ratio of powers** paradigm」+ 2.5 节逐字「the sum of PSF flux estimates provides a measurement of the total signal in the evaluated image」）。我在 `UNIFIED_MODEL.md` 里**保留了原式并标注 [2]**，未改其字面，但把「不能逐字核对」登记在此——**需人工目检原文图片确认**。(b) `docs/detail/PRODUCT_STORAGE_FORM.md` 引的 `WD-HiPS-2.0-20260501 §4.3.2`（**工作草案**）、合同 `:161` 归因的 `FITS 标准 4.0`、与 Hipsgen 手册「not standardized by the IVOA … currently only recognised by Aladin Desktop」原句，我**三份正文均未取得**；且合同与 detail 对 TRIM 依据的归因不同（IVOA 草案 vs FITS 4.0），**必须以一手原文裁决**（§3-⑪）。 | 禁止编造。核不到就写「需补充」。 |
| 4-④ | **`lib/algorithms/shared/` 的共址测试落点。** 派单只让我「核该目录是否存在」（不存在）。我未找到 Hipsgen oracle 对照 / `query_disc` 保守性 / `astro_scalar` 分发这三项验证的现落点（`find lib/algorithms/shared -maxdepth 3` 无 `tests/`；`ls -d lib/algorithms/*/tests lib/algorithms/*/*/tests` 无输出；`ls eng/tests` 只有 `conformance` 与 `validation`，无 `unit`）。已在 detail 里如实登记为待补，**但落点本身需工程正本或前台指定**。 | 同上。 |

---

## 5. 文献核对

### 5.1 核到原文的

| 文献 | 用在哪 | 核对方式与结果 |
|---|---|---|
| Horne K. 1986, *An optimal extraction algorithm for CCD spectroscopy*, PASP 98: 609–617, DOI 10.1086/131801 | `UNIFIED_MODEL.md` 新增参考文献 [1] | **Crossref API 逐字核对**：`https://api.crossref.org/works/10.1086/131801` → `"title":["An optimal extraction algorithm for CCD spectroscopy"]`、`"volume":"98"`、`"page":"609"`、`"container-title":["Publications of the Astronomical Society of the Pacific"]`、`"author":[{"given":"K.","family":"Horne"}]`、`published-print 1986-6`。全部与写入逐字一致。 |
| Conejero J., Radice E. L., Sartori R. *New Image Weighting Algorithms in PixInsight*. PixInsight Reference Documentation. https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html | `UNIFIED_MODEL.md` 新增参考文献 [2] | **抓取原文**（HTTP 200）。逐字核到：作者行「By Juan Conejero, Edoardo Luca Radice and Roberto Sartori (Pleiades Astrophoto, S.L.)」；2.6 节标题「The PSF SNR Robust Estimator of Signal-To-Noise Ratio」；「PSF SNR is a realization of the signal-to-noise ratio formulation based on the ratio of powers paradigm」；2.5 节「the sum of PSF flux estimates provides a measurement of the total signal in the evaluated image」。**结构核对通过，公式字面形态核对不到**（公式以图片渲染，见 §4-③a）。 |
| 本仓一级正本对上述两条的著录 | — | `docs/science/noise_snr/NOISE_SNR.md` [1] Horne、[15] PixInsight —— 与我在 detail 侧立的 [1][2] **同源同物**（`[15]` 的 URL 逐字相同）。 |

### 5.2 核对不到的（分列，不以印象填）

| 项 | 状态 | 处置 |
|---|---|---|
| PixInsight PSFSNR 公式 `(Σf)²/σ_n²` 的上下标字面形态 | **需补充** —— 该页公式以图片渲染，文本抽取里公式被丢弃 | 原式保留在 `UNIFIED_MODEL.md` 并挂 [2]；字面形态登记待人工目检（§4-③a） |
| `WD-HiPS-2.0-20260501 §4.3.2` 工作草案正文 | **需补充** —— 未取得草案正文 | `PRODUCT_STORAGE_FORM.md` 保留该引用（本车道未删）；**且合同归因与之不同**，登记 UNRESOLVED（§3-⑪） |
| `FITS 标准 4.0` 正文（合同 `:161` 把 TRIM 关键字归因于此） | **需补充** —— 未取得该标准正文 | 同上 |
| Hipsgen 手册 TRIM 章节原句 | **需补充** —— 未取得手册原文 | 同上 |
| Hipsgen 手册「not standardized by the IVOA … currently only recognised by Aladin Desktop」 | **需补充** —— 未取得该手册原文 | 同上 |
| `lib/algorithms/shared/` 共址测试的三项验证落点 | **需补充** —— 仓内查无 | detail 如实登记为待补，未编造落点（§4-④、A10） |
| B3 两篇正本的确切新节名 | 节名已由我给出（`「2. 数据对象与字段歧义消解」`），但**改动需一级正本车道执行** | 登记（§4-①） |

---

## 6. 索引变更与真解析器验证输出

### 6.1 索引变更

| 文件 | 变更 | 依据 |
|---|---|---|
| `docs/detail/00_INDEX.md` | `registry/ 生产模块登记正本（26 张卡 + README）` → `（25 张卡 + README）`；`## 3. \`registry/\` 模块卡（26）` → `（25）` | `ls docs/detail/registry/*.md | grep -v README | wc -l` → **25**；00_INDEX 自己的表恰好列了 11 + 9 + 5 = 25 条，与目录实存逐名一致 ⇒ 「26」是笔误。**注意：registry 目录由子代理 B 车道负责，我未动其任何文件。** |
| `docs/detail/common.md:71` | 「以 `lib/algorithms/shared/` 为根的 7 件：`Makefile`、…」→「…7 件：`dirent_win.h`、…」 | `ls -a lib/algorithms/shared` → 无 `Makefile`，有 `dirent_win.h`。件数仍为 7（源文件 7 件），构成换了。 |
| 其余 | **无索引变更** | `docs/DOCUMENT_INDEX.yaml`、`docs/README.md`、`docs/GLOSSARY.md` 都在禁区；且本车道未新增/删除/改名任何文档文件。 |

### 6.2 真解析器输出（改前 / 改后）

命令（两轮同一条）：

```bash
python3 /tmp/verify_anchors.py docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md
```

**改前：合计 19 处**

```
=== docs/detail/00_INDEX.md : 0 处 ===
=== docs/detail/common.md : 2 处 ===
  L23    FILE-MISS  docs/engineering/STANDARDS_REGISTRY.md
  L62    FILE-MISS  lib/algorithms/shared/healpix/tests
=== docs/detail/LOG_AND_ERROR_SYSTEM.md : 2 处 ===
  L73    FILE-MISS  run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic
  L102   FILE-MISS  run/<task>/logs/
=== docs/detail/merged_TROUBLESHOOTING.md : 0 处 ===
=== docs/detail/PHASE1_DETAILED_DESIGN.md : 0 处 ===
=== docs/detail/PHASE2_DETAILED_DESIGN.md : 0 处 ===
=== docs/detail/PHASE3_DETAILED_DESIGN.md : 1 处 ===
  L131   FILE-MISS  eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop
=== docs/detail/PRODUCT_STORAGE_FORM.md : 0 处 ===
=== docs/detail/README.md : 0 处 ===
=== docs/detail/STAR_DETECTION_IMPL_DESIGN.md : 0 处 ===
=== docs/detail/UNIFIED_MODEL.md : 0 处 ===
=== docs/detail/anchors/ANCHOR_CONTRACT.md : 2 处 ===
  L110   FILE-MISS  docs/science/detection/STAR_DETECTION.md",
  L117   FILE-MISS  docs/science/psf/PSF.md:7"
=== docs/detail/anchors/README.md : 0 处 ===
=== docs/detail/infrastructure/17_aio.md : 1 处 ===
  L174   FILE-MISS  lib/infrastructure/aio/{include,
=== docs/detail/infrastructure/18_cli.md : 1 处 ===
  L13    FILE-MISS  eng/contracts/schemas/phase_config
=== docs/detail/infrastructure/19_runtime.md : 3 处 ===
  L21    FILE-MISS  eng/contracts/schemas/run_
  L141   FILE-MISS  lib/infrastructure/scheduler/src/{pipeline,
  L142   FILE-MISS  lib/infrastructure/runtime/
=== docs/detail/infrastructure/20_benchmark.md : 0 处 ===
=== docs/detail/infrastructure/21_observability.md : 3 处 ===
  L107   FILE-MISS  lib/infrastructure/cli/resource_gate.h::gate_enforcement
  L113   FILE-MISS  lib/infrastructure/cli/protocol.h::missing_required_extension_v1
  L138   FILE-MISS  eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate
=== docs/detail/infrastructure/22_gaia_xpsd_client.md : 3 处 ===
  L13    FILE-MISS  docs/engineering/STANDARDS_REGISTRY.md
  L17    FILE-MISS  lib/infrastructure/gaia_xpsd_client/src/gaia_client.{h,c}
  L136   FILE-MISS  lib/infrastructure/gaia_xpsd_client/src/gaia_client.{h,c}
=== docs/detail/infrastructure/23_hips_browser.md : 1 处 ===
  L42    FILE-MISS  eng/.../OPTIMIZATION.md
=== docs/detail/infrastructure/README.md : 0 处 ===

合计 19
```

**改后：合计 7 处**

```
=== docs/detail/00_INDEX.md : 0 处 ===
=== docs/detail/common.md : 0 处 ===
=== docs/detail/LOG_AND_ERROR_SYSTEM.md : 0 处 ===
=== docs/detail/merged_TROUBLESHOOTING.md : 0 处 ===
=== docs/detail/PHASE1_DETAILED_DESIGN.md : 0 处 ===
=== docs/detail/PHASE2_DETAILED_DESIGN.md : 0 处 ===
=== docs/detail/PHASE3_DETAILED_DESIGN.md : 1 处 ===
  L131   FILE-MISS  eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop
=== docs/detail/PRODUCT_STORAGE_FORM.md : 0 处 ===
=== docs/detail/README.md : 0 处 ===
=== docs/detail/STAR_DETECTION_IMPL_DESIGN.md : 0 处 ===
=== docs/detail/UNIFIED_MODEL.md : 1 处 ===
  L111   FILE-MISS  docs/ImageWeighting/ImageWeighting.html
=== docs/detail/anchors/ANCHOR_CONTRACT.md : 2 处 ===
  L110   FILE-MISS  docs/science/detection/STAR_DETECTION.md",
  L117   FILE-MISS  docs/science/psf/PSF.md:7"
=== docs/detail/anchors/README.md : 0 处 ===
=== docs/detail/infrastructure/17_aio.md : 0 处 ===
=== docs/detail/infrastructure/18_cli.md : 0 处 ===
=== docs/detail/infrastructure/19_runtime.md : 0 处 ===
=== docs/detail/infrastructure/20_benchmark.md : 0 处 ===
=== docs/detail/infrastructure/21_observability.md : 3 处 ===
  L107   FILE-MISS  lib/infrastructure/cli/resource_gate.h::gate_enforcement
  L113   FILE-MISS  lib/infrastructure/cli/protocol.h::missing_required_extension_v1
  L138   FILE-MISS  eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate
=== docs/detail/infrastructure/22_gaia_xpsd_client.md : 0 处 ===
=== docs/detail/infrastructure/23_hips_browser.md : 0 处 ===
=== docs/detail/infrastructure/README.md : 0 处 ===

合计 7
```

**19 → 7，净消 12 处。改后残留 7 处逐条判读，7 处全部是验证器误报，文档侧无需再改：**

| 残留 | 误报机制 | 人工核实 |
|---|---|---|
| `PHASE3:131` | 脚本 fragment 字符类不含 `/` | 文件实存 + `$defs.export_crop` 实存 |
| `UNIFIED_MODEL:111` | URL 被 `PATH_RE` 当成仓内路径 | `https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html` 是外部永久链接，本车道新加的 [2] |
| `ANCHOR_CONTRACT:110` | `rstrip(".,;:")` 不剥引号，JSON 示例尾 `",` 进文件名 | `docs/science/detection/STAR_DETECTION.md` 实存 |
| `ANCHOR_CONTRACT:117` | 同上 + 标本载荷 | 故意负例；`docs/science/psf/PSF.md` 实存 |
| `21_observability:107/113/138` ×3 | `PATH_RE` 不识别 `::` | 三文件实存、三符号逐字命中 |

**19 − 12 = 7；7 处真缺陷之外的 12 处（A2、A3、A4、A5 两处、A6 两处、A7、A8、A9 两处、A10、A13、A14）全部落地。**

### 6.3 `§` 机械锚口径（裁决请求）

**可核计数（改后）**：

| 指标 | 值 | 复跑命令 |
|---|---|---|
| 写作用域内 `§` 出现总数（改前 → 改后） | **255 → 46**（净减 209） | `grep -o "§" docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md \| wc -l` |
| `「X」一节` 形态（一处未动） | **127** | `grep -oE "「[^」]*」一节" …` |
| 非「X」一节形态的**行**数（改后残留） | **34 行** | `for f in …; do grep -n "§" "$f" \| grep -vE "「[^」]*」一节"; done` |
| 残留分布 | `STAR_DETECTION_IMPL_DESIGN.md` 32 行、`PRODUCT_STORAGE_FORM.md` 2 行；其余写作用域文件 **0 行** | 同上逐文件计数 |
| 残留 34 行的性质 | **全部是外部文献/标准节号**：`§II`/`§II.E`/`§II.F`/`§II.2`（Stetson 1987）、`§3`/`§4`/`§4.1`/`§4.3`/`§5`/`§6`（B&A96）、`§2`（Triggs & Sdika 2006；Nielsen 1999）、`§6.4`（Kron 手册 v2.3）、`§4.1` 与 `§4.3.2`（IVOA HiPS 1.0 / WD-HiPS-2.0） | `grep -n "§" docs/detail/STAR_DETECTION_IMPL_DESIGN.md \| grep -vE "「[^」]*」一节"` 逐行人工判读 |

- **本车道已改**：指向**本仓**文档、不带章节标题的机械锚（C1–C17）。跨文档的 `ALG §2`（7 处）/ `ALG §11.4`（8 处）/ `ALG §3`（1 处）也已全部改为点名 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` 的章节标题（「离散公式」一章 /「TEST-STAR-DESIGN-001 冻结测试设计」一节 /「伪代码」一章）。
- **保留 1**：`「X」一节` 形态 **127 处** —— 携带章节标题，非机械编号，是一级正本通行的引用形态。
- **保留 2**：外部标准/文献节号 **34 行** —— 外部标准的章节号不可改名，删则失可核性；逐行已确认全为外部目标。
- **保留 3**：无。原先登记待裁的跨文档 `ALG §` 16 处已在本车道消化完毕，**§4-② 相应关闭**（`「X」一节` 与外部节号两类属体例问题，不属本车道处置面）。

**口径冲突（请前台裁定）**：AGENTS.md 第 5 节写「不使用『见 §几』式的机械跳转锚」，但 `docs/engineering/governance/DOCUMENT_GOVERNANCE.md`「写法检查（人读清单）」一节的封闭词表只有五类（日期 / 流水号与工作项 / commit 与散列 / 元信息块 / 历史叙事），**`§N` 不在其中**，且该节明写「本表只有这一处。`docs/` 下的其他文档不复述这张表，只引用本节」。而 `docs/engineering/UNIFIED_OBJECTS.md:117` 等一级正本自身也大量使用 `§14`/`§15`。⇒ 「`§N` 是否算写法缺陷」在两份权威件之间没有一致答案。**在裁定前我按「带章节标题的保留、纯编号跳转的改」执行，不做全仓一刀切。**

---

## 7. 自证段

### 7.1 写作用域自证

```
$ git -c core.quotepath=false status --porcelain -- docs/detail
 M docs/detail/00_INDEX.md                        ← 本车道
 M docs/detail/LOG_AND_ERROR_SYSTEM.md            ← 本车道
 M docs/detail/PHASE1_DETAILED_DESIGN.md          ← 本车道
 M docs/detail/PHASE2_DETAILED_DESIGN.md          ← 本车道
 M docs/detail/PHASE3_DETAILED_DESIGN.md          ← 本车道
 M docs/detail/PRODUCT_STORAGE_FORM.md            ← 本车道
 M docs/detail/STAR_DETECTION_IMPL_DESIGN.md      ← 本车道
 M docs/detail/UNIFIED_MODEL.md                   ← 本车道
 M docs/detail/anchors/ANCHOR_CONTRACT.md         ← 本车道
 M docs/detail/common.md                          ← 本车道
 M docs/detail/infrastructure/17_aio.md           ← 本车道
 M docs/detail/infrastructure/18_cli.md           ← 本车道
 M docs/detail/infrastructure/19_runtime.md       ← 本车道
 M docs/detail/infrastructure/20_benchmark.md     ← 本车道
 M docs/detail/infrastructure/21_observability.md ← 本车道
 M docs/detail/infrastructure/22_gaia_xpsd_client.md ← 本车道
 M docs/detail/infrastructure/23_hips_browser.md  ← 本车道
 M docs/detail/merged_TROUBLESHOOTING.md          ← 本车道
 M docs/detail/registry/acsd.phase1.noise-snr.md  ← **子代理 B 的面，本车道未动**
```

- 本车道**未触碰**：`docs/detail/registry/**`、`docs/GLOSSARY.md`、`docs/README.md`、`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md`、`docs/engineering/**`、`docs/science/**`、任何代码与配置。
- **全仓 `git status` 里这些文件的 `M` 来自并行车道，不是本车道**：本车道 `git diff --stat` 的 18 个文件全部落在授权写作用域内；`docs/detail/registry/` 下 10 个 `M` 由子代理 B 造成，`docs/GLOSSARY.md` / `docs/README.md` / `docs/DOCUMENT_INDEX.yaml` / `docs/engineering/**` / `docs/science/**` 的 `M` 由各正本车道造成。
- 本车道**修了自己上一轮引入的一处失实**（§1.4-D3 的 5-7）：C14 轮把 `DATA_SEMANTICS.md §3.2` 展开成仓内不存在的标题「未覆盖像素一律 invalid（signal=NaN/support=0）」，已在 D3 轮自查中改为真实标题「三个基本对象的语义」。登记在此，不抹除。
- **零 git 写操作**：全程只用 `git -c core.quotepath=false status --porcelain`（只读）。
- **零新增/删除/改名文件**：只改既有 18 份 detail 文档的内容。

### 7.2 方法学自证

| 断言 | 复跑命令 |
|---|---|
| 验证器前后计数 | `python3 /tmp/verify_anchors.py docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md` → 改前 19、改后 7 |
| 坏路径确实不存在 | `test -e docs/engineering/STANDARDS_REGISTRY.md; test -e lib/infrastructure/runtime; test -e lib/algorithms/shared/healpix/tests; test -e configs/stage1.schema.json; test -e eng/...` → 全 MISS |
| 改指目标确实存在 | `test -e docs/engineering/standards/OPTIMIZATION.md`、`test -e lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json`、`test -e lib/infrastructure/scheduler/src/pipeline.cpp` 等 → 全 OK |
| 花括号路径已展开 | `ls -d lib/infrastructure/aio/include lib/infrastructure/aio/src lib/infrastructure/gaia_xpsd_client/src/gaia_client.h lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` → 全 OK |
| glob 已展开 | `ls eng/contracts/schemas/phase_config*` → 恰 3 件 |
| registry 卡数 | `ls docs/detail/registry/*.md | grep -v README | wc -l` → 25 |
| `m_5` 系数三重对齐 | `sed -n '252,267p' lib/algorithms/noise_snr/cpp/src/snr_science.cpp`；`grep -n "m_5 = ZP" docs/science/noise_snr/NOISE_SNR.md`；`grep -n "F_5 = 5" docs/science/unified/DATA_SEMANTICS.md` |
| 差分方向 | `sed -n '707,725p;750,757p' lib/algorithms/star_detection/src/sdet_api.cpp`；`grep -nE "相邻行\|行间差分\|上下行\|纵向差分\|垂直差分" docs/detail/STAR_DETECTION_IMPL_DESIGN.md` → exit 1 |
| bgnoise 因子 | `grep -n "0\.70710678118654752" lib/algorithms/star_detection/src/sdet_api.cpp`；`grep -n "0\.707\|√2\|sqrt(2)\|1/√" docs/science/detection/STAR_DETECTION.md` → exit 1 |
| `F_ref` 口径（代码侧） | `sed -n '163,181p' lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp`；`sed -n '7024,7050p;7128,7152p;12145,12167p' lib/infrastructure/scheduler/src/module_adapters.cpp`；`sed -n '405,412p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` |
| `F_ref` 口径（science 侧） | `sed -n '179,193p' docs/science/unified/DATA_SEMANTICS.md`；`sed -n '268,276p' docs/science/noise_snr/NOISE_SNR.md`；`sed -n '330,375p' eng/contracts/schemas/unified/frame_snr.schema.json` |
| 文献书目 | `https://api.crossref.org/works/10.1086/131801`；`https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html` |
| 剩余 `§` 口径 | `for f in docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md; do grep -n "§" "$f" \| grep -vE "「[^」]*」一节"; done`（改后仅 34 行，全为外部文献/标准节号） |
| 反引号奇偶 | `for f in …; do echo "$(grep -o '\`' $f \| wc -l) $f"; done` → 改后仅 `PRODUCT_STORAGE_FORM.md` 为奇数（413），已补回开引号变 414 |
| D3 三方一致性 | `sed -n '453,464p' docs/ACSD_DESIGN.md`；`sed -n '131,143p;154,190p;196,215p;270,282p' docs/engineering/contracts/HIPS_STORAGE_FORM.md`；`sed -n '156,180p;219,255p' docs/detail/PRODUCT_STORAGE_FORM.md` |
| D3 schema 两档词表 | `python3 -c "import json;print(json.load(open('eng/contracts/schemas/hips_storage_form.schema.json'))['x-acsd-field-vocabulary']['hips_tile_format_two_tiers']['by_subproduct'])"` → `{"signal":"fits","support":"fits","variance":"fits","ivar":"fits","snr":"tsv"}` |
| D3 最高设计词表缺失 | `for t in storage_form archive bare .hips.index.json coverage.index.json; do echo "$t $(grep -c -- \"$t\" docs/ACSD_DESIGN.md)"; done` → 五词全 0（§3-⑩） |
| D3 合同悬空指针 | `grep -n "docs/detail/infrastructure/" docs/engineering/contracts/HIPS_STORAGE_FORM.md` + `ls docs/detail/infrastructure/` → 无产品存储形态设计件（§3-⑨） |
| D3 TRIM 归因分歧 | `grep -n "WD-HiPS\|4\.3\.2" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → 零命中；`sed -n '161p;288p' …` 归 FITS 4.0（§3-⑪） |
| D3 IEEE 归因核实 | `grep -c "IEEE" docs/ACSD_DESIGN.md` → 0（§5-6） |
| D3 §3.2 标题核实 | `grep -n "^### 3.2" docs/science/unified/DATA_SEMANTICS.md` → `### 3.2 三个基本对象的语义`；`grep -rn "signal=NaN/support=0" docs lib eng 实验` → 只命中 detail 自身（§5-7） |
| 合同悬空反向指针 | `grep -n "docs/detail/infrastructure/" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → 命中「体积削减（裸形态）」一节两处；`ls docs/detail/infrastructure/` → 无产品存储形态设计件 |
| 历史叙事 / 日期 / 元信息块 | `grep -nE "此前\|曾经\|已作废\|历史上\|旧版\|已删" …`；`grep -nE "[0-9]{4}-[0-9]{2}-[0-9]{2}" …`；`grep -nE "^(状态\|版本\|日期\|作者\|ID)\s*:" …` → 三者均零命中（除 C18 已说明的非叙事命中） |
| 本仓源码行号 | `grep -nE "[A-Za-z0-9_/.-]+\.(md\|cpp\|c\|h\|hpp\|json\|py)\:[0-9]+" …` → 只剩 `ANCHOR_CONTRACT.md` 的规则/负例标本 |
| 无悬空引用 | `grep -no "\[[0-9][0-9]*\]" docs/detail/UNIFIED_MODEL.md` → 只剩新立参考文献表的 `[1]`/`[2]`；全文另有文末「参考文献」章与之闭合 |

### 7.3 未完成 / 未覆盖的自陈

1. **`§N` 未做全仓清零**（保留项只剩两类：`「X」一节` 形态 127 处 + 外部标准/文献节号 34 行，精确计数见 §6.3；跨文档 `ALG §` 16 处已消化）。这是**有意的降级**，不是遗漏：逐处改写需先核实每个目标章节标题，其中还混着外部标准节号与一级正本自身的编号用法，须等前台对 §6.3 的口径冲突给出裁定。
2. **D3（`PRODUCT_STORAGE_FORM.md` 三方一致性）已核完**：形态骨架三方一致；detail 侧补了 9 处（5-1…5-9），其中 **5-7 是本车道上一轮自己引入的失实指针**（把 `DATA_SEMANTICS.md §3.2` 展开成不存在的标题），已在本轮自查中改正。**未与 `WD-HiPS-2.0` 草案 / FITS 标准 4.0 原文对齐**（两份正文仓内均无），且合同与 detail 对 TRIM 依据的归因不同 ⇒ 登记 UNRESOLVED（§3-⑪、§4-③b）。
3. **PixInsight 公式字面形态未逐字核对**（公式以图片渲染），已在 §4-③a 登记。
4. **未做 git 历史取证**（`git log -L` / `git blame`）：派单未要求，且本车道核心事实已由文件与代码原文闭环；红线 1 亦限定只读 git。
5. **`docs/detail/README.md` 的 B4-3 条被判不成立**（该文件本就没有上游抬头），已记入 §1.2-B6，**未凭空造一条抬头填进去**。