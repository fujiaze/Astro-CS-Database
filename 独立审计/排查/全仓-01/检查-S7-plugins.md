# 检查报告 · S7 切片「docs/plugins 全部」

- 检查对象：`docs/plugins/00_INDEX.md` + `algorithms_phase1/`（8）+ `algorithms_phase2/`（5）+ `algorithms_phase3/`（3）+ `infrastructure/`（7）= 24 篇，逐篇全文读，无跳读。
- 检查面：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚（含 wiring / 死键 / 行锚 / 文献锚）。
- 前置：已读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（已订正并验证项不再报）与 `分歧台账.md` D-01…D-11 终裁；A-* 与 D-* 结论不翻案。
- 结论计数：**红 6 · 黄 5 · 绿 4**。

---

## 一、红级（必须改）

### S7-红-01 ｜①+③｜`docs/plugins/algorithms_phase1/06_photometry.md:46`

- **问题**：仍写「1.166 = SD(MAD)/MAD 正态渐近常数」，A-P1-01 的订正未落地到插件文档。
- **证据**：
  - 本行原文：`| GATE A6 | 1.166 = SD(MAD)/MAD 正态渐近常数 | ...`（:46）。
  - 正本已订正：`docs/science/PHOTOMETRY.md:403/:428` —— 1.166 = √1.361，是 **MAD→σ̂ 估计的标准误（相对 SE）因子**，并明文否定「SD(MAD)/MAD」标签。
  - 待办在册：`独立审计/实验重做/总编对账/五单元成稿简报.md:47` 待订正清单第 1 项 =「06_photometry.md §4.1：1.166 的解释标签（A-P1-01）」——**尚未执行**。
  - 反方核验：全切片 grep「1.166」仅 06 一处带旧标签；02/10/11/13 等无重复，故不是全切片共性表述。
- **建议改法**：按 A-P1-01 已裁决文本（以 `PHOTOMETRY.md:403/:428` 为准）改写该标签；**不改公式本身与 1.166 数值**（公式/常数改动走变更流程）。
- **所属面**：①（并构成与正本的 ③ 冲突）

### S7-红-02 ｜③+④｜`docs/plugins/algorithms_phase1/07_noise_snr.md:138-145`（§4.4）

- **问题**：「逐像素方差接线现状（如实登记）」整段为**过期陈述**，且与 `08_drizzle.md:69-70` 形成直接互指冲突：08 明写该表述「与工作区现状**不符，已作废**」并把「双实现分裂」指向 07 §4.4，而 07 §4.4 仍完整断言旧状态、无订正注、也不存在 08 所指的「双实现分裂」讨论。
- **证据**：
  - 07:138 原文：「生产调度路径（`module_adapters.cpp`）**尚未挂** `variance` 块 ⇒ `hp_drizzle_api.cpp:1024` 读不到 ⇒ `astro_sphere_sink.cpp:306–327` 不产出 variance/ivar 子产品面 ⇒ `p2_integrated.json` 报 `uncertainty_available=false`（原因 `ivar_product_missing_frame_snr_fallback`）」。
  - 代码反证（定案2 已接线）：`lib/infrastructure/scheduler/src/module_adapters.cpp:8156` 挂 `frame,"variance",AIO_BLOCK_FLOAT32`（块构造 :7879-8160），`variance_block_name` :8423，products :8805；`ivar_product_missing_frame_snr_fallback`（:11596）已由无条件分支改为条件分支。
  - 07:139「**接入义务**：把 orchestrator 的逐像素方差接线搬进 scheduler」——该义务已执行，文本仍以未执行口吻书写。
  - 08 侧对照：`08_drizzle.md:69-70`「生产调度路径**已挂** variance 块……旧原因 `ivar_product_missing_frame_snr_fallback` 与工作区现状不符，已作废；双实现分裂见 `07_noise_snr.md` §4.4」——07 §4.4 实际**没有**该内容（138-146 行逐行读过），指针落空。
  - 反方核验（可能的辩解）：07:146 有「行号以符号名核对为准；并发改动会使行号漂移」免责——该免责只覆盖**行号**，不覆盖「尚未挂 / 接入义务未做」这一**状态性断言**，且 08 明文判其作废，故不成立。
- **建议改法**：07 §4.4 状态段按代码现状改写（已挂 variance 块、fallback 转条件），并补上 08 所指的「双实现分裂」事实，或改 08:70 的指针；两篇同一次提交内一致。
- **所属面**：③（④ 状态核验）

### S7-红-03 ｜④+③｜`07_noise_snr.md:97,:194,:217` + `algorithms_phase2/13_integration.md:20-24,:42,:8(:94)`

- **问题**：三路径 SNR 重建与 `snr_path_effective` 落盘**以现状口吻书写**，但生产调度路径未接入稀疏层、该字段全代码零实现；文档无任何缺口登记（与 07 §4.4 那种「如实登记」体例相反）。
- **证据**：
  - 代码：`module_adapters.cpp:11534` —— `in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面`（明文）。
  - `grep '"snr_path"' lib/` 仅 `cli/parser.cpp:340` 与 `cli/session_commands.h:230`（CLI 键表）；`sparse_reconstruct` 在 `lib/` 仅命中 `session_commands.h`，scheduler/integration 0 命中。
  - `snr_path_effective` 在 `lib/` **0 命中**（全仓 93 处命中全在 docs/ledger/实验/独立审计）。
  - 稀疏层生产者：`"snr_model"` 块仅由旧 `pipeline/orchestrator/cpp/src/orchestrator.cpp:4627` 写出；scheduler 0 命中；`sparse_snr` 在 scheduler 与 `lib/algorithms/noise_snr` 均 0 命中。
  - 机器台账在役：`eng/ci/ledgers/dead_config_keys.json:34-45` `dead_config_key:snr_path`——「**生产科学消费点仍为零**……本条**保留**，防止把「CLI 认识」误当成「已消费」」；`design_clauses.json:155-161` exit_condition 同步登记 `snr_path_effective` 未落地。
  - 文档侧：13:23「按帧级执行并**显式记录实际路径**（`snr_path_effective=frame_reconstruct` + 计数）」；13:42「路径由配置显式选择（默认 `sparse_reconstruct`），是 Phase2 **标准行为**」；13:8/:94 把「实际路径被显式记录」列为测试项；07:97/:194 同类断言。
  - 反方核验：v6 库层确有 `reconstruction_operator` 冻结词表与 `SparseReconstruction`（`lib/algorithms/integration/v6/src/weight_chain.cpp:144-148,:384-401`）——**库层是真**；但生产调度面喂入的是 `nullptr`，故「生产标准行为」的口吻仍不成立；独立审计 `02_科学/SNR链路核验报告.md:214`（DEV-04）与 `实验/absolute-snr/results/b6_gates_audit.json:136`（H4_scope_note：「lib/ 实现侧尚无该状态机」）已从另一面登记同一缺口 —— **插件文档是唯一未登记缺口的一面**。
- **建议改法**：13 §3/§4.0/§8 与 07 §4.2/§5 改为「目标行为 + 现状未接线（引用 dead_config_key:snr_path 台账）」的登记式写法，或补实现后删注；**不改默认值 `sparse_reconstruct` 与冻结词表**（默认值属冻结面，改动走变更流程）。
- **所属面**：④（③ 跨文档：文档 ↔ 台账 ↔ 代码）

### S7-红-04 ｜③｜`docs/plugins/00_INDEX.md:104-112`（§6）

- **问题**：索引把科学核心写成「**三个**创新点」，与最高设计「五个创新点」直接冲突；P5 章节号错引；实验单元数量与目录清单与 §12.3 不符。
- **证据**：
  - 00_INDEX:104 标题「## 6. **三个**创新点与实验单元」；:106「科学核心是**三个**紧密相连的创新点（最高设计 **§2**）」；:110 把加性天光标为「（**§2.3**）」；:112「**三个**创新点各自是一个独立实验单元（最高设计 **§12.3**）」只列 3 个目录。
  - 最高设计反证：`docs/ASTROCS_DESIGN.md:120`「ACSD 的科学核心是**一条科学链上的五个创新点**」；`:140/:150/:208/:223/:234` = §2.1 P1 / §2.2 P2 / **§2.3 = P3**（算子·平面到 HEALPix）/ §2.4 P4 / **§2.5 = P5**（加性天光与无接缝）；`:771`「**五个**创新点各立一个自包含实验单元」。
  - 即 00_INDEX:110 的「加性天光（§2.3）」实际应为 §2.5 —— 引的是 P3 的节号。
  - 目录实况：`实验/` 下 8 个目录（photometric-magnitude / absolute-snr / additive-sky-seamless / **healpix-polar** / **dense-snr-reconstruct** / cone-search-constants / m42-realdata / shared），索引只列前 3 个，P3 产品级单元与 P4 单元缺席。
  - 自相违背：00_INDEX:99 维护规则「插件文档与最高设计保持一致；**冲突以最高设计为准并修订本文档**」。
  - 反方核验：本切片其余 23 篇均无「三个创新点」表述（grep「创新点」仅 00_INDEX 命中）；13:24 等按 P1/P5 语义书写无冲突 —— 故该错仅在索引一篇，不属设计本身歧义。
- **建议改法**：§6 改为五创新点（或改为「与本插件集直接相关的三组实验单元」并注明省略 P3/P4 的理由），P5 节号改 §2.5，实验单元清单补 `healpix-polar`、`dense-snr-reconstruct`（或标注归属）。
- **所属面**：③

### S7-红-05 ｜④+③｜`docs/plugins/infrastructure/22_gaia_xpsd_client.md:1-2,:7-8,:42-43,:50,:63`

- **问题**：把模块职责写成「查询外部服务 / 管理网络 / 在线查询 / 超时」，与它自己引用的最高设计 §3.3、模块自身 README 与代码**三方冲突**。
- **证据**：
  - 文档：:1「查询 Gaia（及 XPSD 等**外部服务**）星表数据，**管理网络**」；:7「外部服务不可用时降级策略显式」；:8「**网络不可用** + 无缓存 → 明确失败（可降级到离线星表，但需显式配置）」；:42「合并为**一次外部请求**」；:50「离线星表输入与**在线查询**等价合同」；:63 `timeout` 单位 s「**网络超时**」。
  - 上位权威（该文档自己在 :12 引用的 §3.3）：`docs/ASTROCS_DESIGN.md:266` ——「星表只解析**本地**星表文件，**离线、零网络**；科学身份不依赖网络」。
  - 模块正本 README：`lib/infrastructure/gaia_xpsd_client/README.md` §2 —— 不负责「**网络访问（结构性零网络）**；数据目录整理/下载」；§3 ——「模块不写输入文件、**不联网**」，输入端口 = `catalog.xpsd_dir` 本地目录。
  - 代码：`grep -ri 'http|curl|socket|network|online|网络' lib/infrastructure/gaia_xpsd_client/` 仅命中 memory.md 里的上游仓库 URL —— 实现是本地 XPSD 解析 + mmap + 锥形搜索，**零网络符号**。
  - 反方核验：若把「网络」读成「未来能力」，则与 :1 职责栏（现行职责）、:7/:8 错误分支（现行处置面）、:63 配置键（现役键表）三处**现在时**表述不相容；且与 §3.3 的冻结结论方向相反。
- **建议改法**：按 §3.3 + README 改为「本地 XPSD 星表解析与锥形搜索、结构性零网络；离线/在线等价合同留作未来能力须显式标注未实现」；:63 `timeout` 键同步删除或标「未实现」。
- **所属面**：④（并构成 ③：与其引用的上位文档冲突）

### S7-红-06 ｜④｜`22_gaia_xpsd_client.md:26-28,:42-44,:50,:54-63`（缓存模型与配置键）

- **问题**：缓存模型描述（瓦片键、磁盘缓存、全 run 单例、查询合并、8 个配置键）与实现不符——属「文档说有、代码没接」。
- **证据**：
  - 缓存键：文档 :26「天区**瓦片**（固定 HEALPix/天区网格）……重叠视场通过**瓦片包含关系**复用」；实现 `gaia_client.h:2` ——「Cache key（**精确匹配**）: ra/dec/radius/mag_low/mag_high (double 逐位) + db_type/file_count + version=GAIA_CACHE_VERSION(2)……**不做量化舍入，命中即同一查询精确重复**」→ 无瓦片量化、无瓦片包含复用。
  - 两级缓存：文档 :27「进程内只读共享缓存（LRU 字节预算）+ **磁盘缓存**（跨运行复用，落安装/缓存目录）」；实现只有两层**内存**缓存：`gaia_client.c:200-218` `BLOCK_CACHE_CAPACITY=8192`（解压块）与 `QUERY_CACHE_CAPACITY=64`（查询结果 LRU）；`grep 'fopen|disk_cache|cache_dir' gaia_client.c` 仅 `/proc/meminfo` → **无磁盘缓存**。
  - 单例：文档 :28「**全 run 单例客户端**：多帧、多 worker 共享同一缓存」、:68「多帧共享单例客户端」；实现创建点在操作函数内 —— `module_adapters.cpp:3829`（`p1_op_star_psf_impl` 内）与 :4868，各 `gaia_client_create(gcfg.gaia_dir…)`，**未见 run 级单例/跨节点共享的实现**。
  - 查询合并：:42-43 的在途去重与预取 —— `gaia_client.c` 查询由 `cache_lock` 互斥**串行**执行（`gaia_client.h:4`「查询串行+缓存互斥」），无在途请求合并/预取实现。
  - 配置键：`cache_dir` / `mem_cache_budget_mb` / `tile_scheme` / `prefetch_neighbors` / `query_limit` / `catalog_version` / `timeout` 在 `lib/` **同名字面量 0 命中**；`eng/packaging/config/config_registry.json:1803-1841` 仅登记为 `registration: plugin_doc`（文档级登记），非代码读取点。
  - 反方核验：模块 README §2 承认「两级缓存」——但其语义 = 块缓存 + 查询缓存（均内存，与 gaia_client.c 一致），**不能**支撑「磁盘跨运行 + 瓦片键 + 单例 + 合并」；LRU 字节预算仅在块级 `block_budget` 存在，与 `mem_cache_budget_mb` 配置键不是同一载体。
- **建议改法**：缓存/配置各条按 `gaia_client.h/.c` 实测改写（精确匹配两级内存 LRU、无磁盘缓存、按调用点创建、无合并/预取），或把这些条目标成「目标规格 · 未实现」并挂到实现任务；键表要么删、要么补实现。
- **所属面**：④

---

## 二、黄级（建议改）

### S7-黄-01 ｜②｜`docs/plugins/infrastructure/18_cli.md:24`

- **问题**：「命令树（唯一）见最高设计 **§6.2**」与本文档 :3 自己写的「上游：…§7.1（命令树）」自相矛盾，且指向错误。
- **证据**：`docs/ASTROCS_DESIGN.md` 标题实测：`:467` =「6.2 流程」（export 章内）、`:493` =「**7.1 命令树（唯一）**」；`20_benchmark.md:12` 亦写「§7.1（命令树：benchmark）」，`14/15/16` 的「§6.2（export 流程）」证实 6.2 = 流程。反方核验：不是编号体系差异——同一文档内 :3 与 :24 两处指同一对象却不同节号。
- **建议改法**：:24 改为 §7.1。
- **所属面**：②（附带 ③ 引用错位）

### S7-黄-02 ｜④｜`docs/plugins/algorithms_phase1/03_star_detection.md:26,:12`

- **问题**：阈值行锚漂移（引文行不含阈值语句）；:12 的冻结正本行锚与阈值定义位置不齐。
- **证据**：`03:26` 锚 `sdet_api.cpp:1782-1792` ——实测 :1779 才是 `T threshold = img_median + T(5.0) * bgnoise;`，:1771 注释、:1772 bgnoise、:1761 blur；:1782-1792 实为计时器/日志/阶段4 注释。同型漂移上轮只修了 `GATES_AND_TOLERANCES.md:40-41`（PASS 表在册），插件文档未同步。反方核验：锚点文件与函数存在（`lib/algorithms/star_detection/src/sdet_api.cpp`），属行号漂移非幻觉。
- **建议改法**：锚改 :1761-1779（或改符号锚），:12 同步核对 SCI/SDA 行号。
- **所属面**：④

### S7-黄-03 ｜④｜基建四篇配置表：`17_aio.md:37-39`、`19_runtime.md:77-79`、`21_observability.md:36-37`、`22_…:57-63`

- **问题**：配置键在代码中无同名字面量读取点，文档却按「现役配置项 + 默认值」列。
- **证据（逐一 grep `lib/` 字面量 "key"）**：`schedule_policy`(locality_first/balanced) 0 命中；`cache_budget_mb` 0；`memory_limit` 0；`log_keep` 0；`sampling`（作配置键）0；`cache_mb` 0；`fsync` 0；22 的 `cache_dir`/`tile_scheme`/`prefetch_neighbors`/`query_limit`/`catalog_version`/`timeout` 0（并入红-06 佐证）。对照：同表里的 `workers`/`block`/`kernels` 在 `lib/infrastructure/benchmark/backend_host/*.cpp` 有读取（:201-202,:197）—— 说明「同名读取点」是可核验判据，上列键不满足。注册面：`config_registry.json:1788-1841` 这些键 `registration=plugin_doc`（仅文档登记），无 consumed_by。
- **反方核验**：不存在以变量为键的动态读取迹象（这些键均非动态构造名）；`workers/block` 有读取证明本仓库确用字面量键。
- **建议改法**：表头或每行标注「规格键 · 未接实现」，或补实现后再以现状书写；涉及**默认值/容差面**一律只登记、不擅改（如 `schedule_policy` 默认 locality_first）。
- **所属面**：④

### S7-黄-04 ｜④｜`docs/plugins/infrastructure/23_hips_browser.md:7,:33-34`

- **问题**：① `color_map` 默认 `inferno` 在实现里零命中；② `stretch` 默认 `sqrt` 与实现取值 `asinh` 不同；③ :7「**未来** GUI 可视化组件」与已建成 `healpix_browser_qt`/`browser_cli` 时态矛盾。
- **证据**：`grep -rn 'inferno' lib/` → 0；`grep -rn 'color_map' lib/` → 0（全仓唯一命中即本文 :33）；`lib/infrastructure/hips_browser/healpix_browser_qt/app/browser_cli.cpp:389` `sky.set_stretch("asinh", true);`（core 侧 `display_tone` 支持 sqrt/log/asinh，:276-280）。反方核验/前情：独立审计 `独立审计/证据/AUD-101-DB-04.md:290-292` 已**登记**同一组问题（含时态矛盾），`检查-修复验证.md` PASS 表未收录其订正 ⇒ 文档侧**仍未改**，本切片不构成重复翻案，而是补一次在册未闭环项。
- **建议改法**：色表/拉伸默认值改按实现（或补实现），:7 时态改为现行描述。
- **所属面**：④

### S7-黄-05 ｜④｜`docs/plugins/algorithms_phase2/11_upm.md:142`

- **问题**：行锚 `stage2_common.h:24` 不是常量行；「`P2_SMOOTHING_LAMBDA_AUTO`=0.1 **仅在 auto 路径生效**」只覆盖调度器路径，漏了 stage2 工具的 struct 默认。
- **证据**：`lib/algorithms/coverage/include/astro/phase2/stage2_common.h:24` 实为注释（「负责人裁决 GAP_AUDIT §9.39 A5…」），常量 `P2_SMOOTHING_LAMBDA_AUTO = 0.1` 在 **:30**；`stage2_common.h:56-58` 注释与初值把该常量设为 `P2Stage2Config` 的 **struct 默认**（缺键即 0.1，且注明「struct 默认 == parser 默认 == CONFIG_SCHEMA「auto」解析值」，`CONFIG_SCHEMA.md:33` `smoothing(auto→0.1)`）。调度器路径则确按文档：`module_adapters.cpp:9599`「缺键保持 upm.h:75 的编译期默认 0.0」+ :9613 auto→0.1；代码侧已把两默认冲突登记上呈（module_adapters:9600-9602、stage2_common.h:56-57、`PHASE2_UPM_IMPL.md:382` 冻结面 0.0）。反方核验：`upm.h:75` 引文逐字命中 ✓（该行无问题）。
- **建议改法**：行锚 :24→:30；「仅在 auto 路径生效」补一句双路径差异（**不改任何默认值**——默认值属冻结面，冲突已登记，只登记不擅改）。
- **所属面**：④

---

## 三、绿级（可不改）

1. **G-01 ｜②｜`08_drizzle.md:53-55`**：:53 的 bullet 插入表格中间，:55 `| sparse_snr_layer |…` 成为无表头孤儿行，渲染断表。内容本身正确（与 13/07 一致），建议只整理版式。
2. **G-02 ｜③｜`00_INDEX.md:53`**：「投影 registry（**内置** TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）」未带 4/8 现状；`14_projection.md:27` 与 `p3_projection_registry.h:78-84`（STG/MOL/CEA/ZEA = `kNotImplemented`）一致且已如实登记，索引行可补「已实现 4/8」。
3. **G-03 ｜①｜`01_calibration.md:27,:33`**：方差传播式 `{V(r)+V(b)+α²[V(d)+V(b)]+y²V(f)}/f²` 为独立性近似（丢共享 master 协方差、`V(b)` 双计 2αV(b)）——已作为 **OI-02** 登记于 `docs/contracts/DATA_SEMANTICS.md:3402`，且与 `PHASE1_DETAILED_DESIGN.md:93/:103` 逐字同式（跨文档一致）。**属科学公式面：只登记、不提改法**（改须走变更流程）。
4. **G-04 ｜④｜`05_platesolve.md:37 tweak_order(tan)`**：`lib/` 同名读取 0 命中 —— 但该问题已由 `artifacts/evidence/governance-01/research/DOC-SCI-001_插件文档与科学权威冲突裁决.md:410` 裁决登记（A+U，处置 = U 命名区分 + SCI-WCS 线补 tweak 模型条款），**不重复立项**，此处仅作已知在册项归档。

---

## 四、已查无问题面（按面覆盖 + 抽样方法）

### 面①科学性（全查，无遗漏）

- **覆盖**：01-16 全 16 篇科学文档全文读；逐条对照 `分歧台账.md` D-01…D-11 终裁与 §2 A-* 裁决：1.166（A-P1-01 → 红-01）、`k_corr = k_gauss × k_geo` 两因子（D-08，10:40 ✓）、`r_eff = n_free` 与 dof=`n_obs−r_eff`（A-P5-01，10:110/:117、11 §4.7 带订正注 ✓）、w∝SNR² 几何限定 +18%（A-P5-06，11:79-80 ✓）、`λ = τ·mean(diag)`（A-P5-08 ✓）、×5.07→峰值/长尺度≈8（A-P5-11，11:189 订正 ✓）、Andrae 2010/Aitken 1935 DOI（A-P5-04/05：Aitken 为全切片唯一 DOI，11:77，先前轮 PASS 不复核）、排异三档（12 §9 ↔ `REJECTION.md:275/:367-368` 逐档同值 ✓）、σ_sky 单次读噪口径与 +12.8%~+34.0% 实测（07 §4.2a ↔ b2_noise_terms ✓）、σ̂×1.2533/1.4826/1.4142（06 ↔ PHOTOMETRY ✓）、13 背景平面 3/6 系数与 7/3 标度（09 ↔ SKY_BACKGROUND ✓）。
- **未翻案面**：D/A 结论一律不复核、不重算；非退化/恒真门表述（07:106 判据局限自述、12 能红能绿）已如实声明，未发现把 UNRESOLVED 写成结论的科学行。
- **抽样法**：全文读（16/16）+ 公式常数行定向 grep（`1.166|1.2533|1.4826|1.4142|k_corr|r_eff|5.07|18%`）逐处回正本核对。

### 面②行文逻辑（全查）

- **覆盖**：24/24 全文读。命中 = 红-02（07↔08 互指作废、指针落空）、黄-01（18 内部节号矛盾）、绿-01（08 断表）。订正注 `<!-- 订正: ... -->` 抽验：07:195（D-05 配置化＋运行日志）、11 §4.7（A-P5-01）、10:48/:110/:117（D-08/A-P5-01）均在位且与正本一致；无「以 TODO 口吻当结论」的行。
- **抽样法**：每篇读小节标题序列（8 节模板完整性，`00_INDEX:71-83` 模板 vs 各篇实际：24/24 齐）+ 互指词 grep（`已作废|不符|另见|正本=`）人工展开。

### 面③跨文档冲突（全查）

- **覆盖**：00_INDEX↔docs/ASTROCS_DESIGN（红-04）；07↔08（红-02）；06↔`PHOTOMETRY.md`（红-01）；22↔设计 §3.3↔模块 README（红-05/06）；01↔PHASE1_DETAILED_DESIGN 与 DATA_SEMANTICS（绿-03 一致）；12↔REJECTION（一致）；11↔PHASE2_UPM_IMPL（一致，双默认冲突已由代码登记 → 黄-05）；10↔PHASE2_UPM（`k_corr`/`B0`/SP-0/2048× 逐处一致）；14↔PHASE3_PROJ_IMPL（§15 存在 ✓）；16↔DATA_SEMANTICS §31.1a（`ADU/sr` = `flux_sum/covered_area`，DATA_SEMANTICS:30/:451/:476 逐字一致 ✓）；17↔HIPS_STORAGE_FORM_CONTRACT（§7/§10 字段表一致 ✓）；19↔21（G-RES-01 指针、exit 10 语义一致 ✓）。
- **抽样法**：每篇「权威依据」节的每个文件指针 + 同名正本逐条对读；数字型结论全 grep；权威链方向（设计→docs→实验→审计→代码）只判一致性、不重裁。

### 面④幻觉与锚（全查）

- **指针**：`docs/{algorithms,science,design,contracts}/*.md` 类引用 **24 个唯一目标，0 悬空**（脚本化 test -f）。
- **行锚**：切片内 `file:LINE` 型锚共 **8 个唯一**，逐一开行：`upm.h:75` ✓ 逐字、`monitor.h:189` ✓、`resource_recorder.h:158` ✓（io_wait_pct 归一）、`hp_drizzle_api.cpp:1024` ✓（方差传播行，但承载的**状态断言**已过期 → 红-02）、`stage2_common.h:24` ✗（黄-05）、`sdet_api.cpp:1782` ✗（黄-02）、`astro_sphere_sink.cpp:306`/`orchestrator.cpp:4694` 归入红-02 的状态性陈述（:146 有行号免责注，不单列）。
- **代码符号/接线**：`kForbiddenWeightSourceTokens`(coverage.cpp:391) ✓、`FZ-WEIGHT-SINGLE-PATH/FZ-MODE-RETIRED/FZ-FIELD-WEIGHTMODE/FZ-P3-BUNIT-QUADRATIC`（v6_clause_registry ✓）、算子冻结词表 4 个 ✓（weight_chain.cpp:144-148）、`variance_audit_required_fields()`(variance_plane_policy.h:116) ✓、`sparse_recon_operator_for_source()`(weight_chain.cpp:182) ✓、`SNR_SIGMA_SKY_UNSPECIFIED`(snr_estimator.h:318) ✓、退出码 11 枚举与 `exit_codes.h` 逐行一致 ✓、`aio_sparse_punch.h` 唯一实现与 `write_fits_atomic`(:539) 内 fsync(:580)→打洞(:594)→rename(:614+) 顺序与 17:27 描述一致 ✓、`storage_form` 默认 archive（parser.cpp:288 + session_commands.h:168-176）与 17:25 一致 ✓、投影 4/8 实现状态与代码一致 ✓。
- **文献锚**：全切片仅 1 个 DOI（11:77 Aitken 1935）——先前轮已 PASS，本次不复核；无 arXiv/URL 外链（grep `doi|arXiv|https?://` = 1 命中）。
- **死键专项**：`snr_path`/`snr_path_effective`（红-03）、基建/浏览器键（黄-03/黄-04）、`tweak_order`（绿-04 已裁决在册）、`sparse_snr_spacing_px`=64（07:215 ↔ defaults.json:691-699 一致，非死键 ✓）。
- **抽样法**：锚点用「文档正则抽出 → 逐个 sed 行内容」全量核验（8/8，非抽样）；配置键 25 个全部对 `lib/` 做字面量 grep（全量）；文档指针 24/24 全量 test -f。

**未查（如实登记）**：① 构建/ctest/CI 门未执行（只读纪律）；② `实验/` 数值复算未做（属实验面切片）；③ 07:139 `orchestrator.cpp:4694-4839` 区段内部逐行核对未做（该段随红-02 一并改写时再核）。
