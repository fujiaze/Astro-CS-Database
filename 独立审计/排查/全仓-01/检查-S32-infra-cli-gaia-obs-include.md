# 检查-S32 ｜ lib/infrastructure（cli＋gaia＋observability）＋ lib/include ｜ 对抗性只读复查

- **切片**：S32；**方法面**：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚（死配置键 / 静默旁路 / 文档说有代码没接）。
- **范围（本片实读/实扫）**：
  - `lib/infrastructure/cli/`：command_tree.h、parser.cpp（parse_args / session_keys / block_keys / session_blocks_errors / validate_config_full / validate_cpu_profile）、commands.cpp（分派、cmd_session1/2/3_run、run_phase*_block、SessionOp）、subcommand.h、session_commands.h、protocol.h、jsonl.h、v6_mode_gate.h、v6_runtime_contract.h、disk_gate.h、resource_gate.h、runtime_client.{h,cpp}、exit_codes.h、main.cpp、README、CMakeLists、module.yaml、normalize/mosaic/export 头（锚扫描全覆盖）；
  - `lib/infrastructure/gaia_xpsd_client/`：README、module.yaml、CMakeLists、src/module_entry.c（1061 行）、src/gaia_client.h 导出面、integration JSON、tests 登记面；
  - `lib/infrastructure/observability/`：PENDING.md、logging/{README, schema}、probes/README；
  - `lib/include/astrocs/`：abi/status_codes.h、abi/module_api_v1.h、abi/host_api_v1.h、common_abi_v1.h、core/contracts.h（ErrorDomain）。
- **对照权威**：docs/ASTROCS_DESIGN.md（L1 现行编号）、分歧台账 D-01…D-11（含 D-08/D-09、§9.73 A44、§9.74 裁决 10）、检查-修复验证.md PASS 表、docs/api/{CLI_PROTOCOL_V1, COMMON_ABI_V1, PHASE1/2/3_API_V1}、docs/contracts/{CONFIG_CONTRACT, LOG_AND_ERROR_CONTRACT}、docs/development/CONFIG_SCHEMA、docs/architecture/ERROR_MODEL、docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT、eng/ci/ledgers/{log_system_ledger, dead_config_keys}、eng/ci/checks.json。
- **避让（在册/他片已报，本片不重复计数，仅交叉引用）**：S1 域外-1（L1 侧旧编号锚）、S11 红4/黄13/黄15/黄16（CONFIG_SCHEMA fence 死键与注释）、S12 黄1/黄2/黄4 与红1..红4、CR-83 Finding#1、log_system_ledger LSW-014 / F-EXIT-MAP、dead_config_keys 台账、ERROR_MODEL 台账项 AUD-101-DB-04、检查-修复验证.md PASS 表 28 项。
- **计数**：🔴 红 5 ｜ 🟡 黄 8 ｜ 🟢 绿 5。

---

## 一、红（必须改）

### 红1 ｜ 红 ｜ lib/infrastructure/cli/v6_mode_gate.h:36-37、:78-90 ｜ 配置腿从 `--config` 取文档，而 `--config` 不是任何命令可解析的旗标 ⇒ `have_doc` 恒 false，A44「weight_mode 任何形态出现即具名拒绝」的配置腿在**全部可达路径**永不执行 ｜ 面④②

- **问题描述**：`mode_gate(p, phase, ev)` 的文档腿只读 `p.values["--config"]`；真实用法中配置由 `--json <path>` 携带，而 `--config` 既不在 `command_tree.h` 的 `value_flags()`（:56-75），也不在任何命令的 `allowed`（:91-110）。解析器 `parse_args`（parser.cpp:148）对白名单外旗标 `parse_fail("unknown flag ...") → rc=2`，故 `p.values["--config"]` 在所有可达路径为空 ⇒ `have_doc` 恒 false ⇒ `doc.contains("weight_mode")` 分支（:78-90，含 `emit_route(..., "config.weight_mode", ...)` 与 rc=2）是**不可达死代码**；头注与块注仍声明该 fail-closed 行为。
- **证据（对照来源＋反方核验）**：
  - 正：`command_tree.h:56-75 value_flags()`、`:91-110 commands()` 全表无 `--config`；`parser.cpp:148` 未知旗标即 2；调用点 `commands.cpp:1360`（phase2）、`:1652`（phase3）均在 `need_value(p,"--json")` 之前先跑 `mode_gate`。
  - 反方核验①：即使把该腿改读 `--json`，**平铺**形态顶层 `weight_mode` 仍会被 `validate_config_full`（parser.cpp:590-599，session_keys 不含 weight_mode）按未知键 **rc=3** 拦下——但那是**未知键**报错，不是 A44 声明的具名拒绝（rc=2）与 `v6_mode_route`（source=config.weight_mode）事件；该事件的 config.source 分支零产出。
  - 反方核验②（在册面）：`eng/ci/ledgers/log_system_ledger.json:181-191 LSW-014` 只登记「门控文档**解析失败** ⇒ have_doc=false 静默降级」，未覆盖「路径本身永不可达」；`独立审计/证据/通读-CR-83.md:78-98` 只比注释与代码语义，且其后果段写「若配置中出现 weight_mode 会被拒绝」——按本条该前提不成立。
  - 反方核验③：`--mode`/`--export-mode` 旗标腿（:88-93）仍可达，问题仅限 config.source 腿与随附事件。
- **建议改法**：文档腿取值旗标改为运行实际使用的 `--json`（或删腿并同步头注与 A44 声明面），并复核 `v6_mode_route` 的 config.source 是否仍属冻结事件面。**涉及 A44 fail-closed 合同声明，本片只登记，落地由 CLI 域主裁决。**

### 红2 ｜ 红 ｜ lib/infrastructure/cli/commands.cpp:1376-1393、:1668-1683 ｜ mosaic/export 的 **blocks 形态运行期不调 `validate_config_full`**，与 subcommand.h:403-407 注释承诺（-force 跳过预检后由运行期校验兜底）相抵 ⇒ `-force` 下多块配置**顶层未知键被静默忽略** ｜ 面②④

- **问题描述**：`cmd_session2_run`/`cmd_session3_run` 的 blocks 分支只跑 `session_blocks_errors`（结构 + 块内键 + 顶层**会话键混用**），没有 `validate_config_full`；块级文档 `bdoc.dump()` 直接下传，节点面看不到文件顶层。而 `-force` 在 `subcommand.h:408-412` 跳过全部预检（预检里唯一跑 `validate_config_full` 的是 `subcommand.h:441`）⇒「mosaic/export 多块 + -force」时，顶层写的 `weight_mode` 或任何不在 `session_keys()/block_keys()` 的键**全程无人读、无人报**。
- **证据（对照来源＋反方核验）**：
  - 正：`commands.cpp:1376-1385` 平铺分支调 `validate_config_full`；`:1386-1393` blocks 分支只调 `session_blocks_errors`；`commands.cpp:1668-1683` export 同构；`validate_config_full` 全部调用点 = commands.cpp:1380/1671/1721/1843/1964 + subcommand.h:441（grep 实测），`run_phase2_block`/`run_phase3_block` 函数体内零调用。
  - 反方核验①（边界）：**不加** `-force` 时 subcommand.h:441 会拦（rc=3）；**平铺**形态两条运行分支都会拦（rc=3）；**normalize** 的 blocks 形态安全——`run_phase1_block` 逐块对整文件调 `validate_config_full`（commands.cpp:1961-1968）。故缺口精确限定在 mosaic/export × blocks × -force。
  - 反方核验②：`session_blocks_errors`（parser.cpp:455-551）的顶层检查只有 `config_has_flat_session_keys`（parser.cpp:436-439，按 session_keys 逐个探测顶层），`weight_mode` 不在该集合 ⇒ 不触发。
  - 反方核验③（合同注释）：subcommand.h:403-407 原文「结构非法/路径缺失由运行期（session_dispatch → cmd_sessionN_run 的 validate_config_full + 节点 validate_config）自然报错」——对 mosaic/export blocks 分支不成立。
  - 在册关联：`dead_config_keys.json` 登记的是「CLI 认识但生产零消费」，本条相反——「文档声明会拒、实际不拒」，台账无此条目。
- **建议改法**：blocks 分支补一次与平铺分支同源的 `validate_config_full`（或让 `session_blocks_errors` 覆盖顶层未知键），使承诺注释与实现一致，并补「-force × blocks 顶层未知键」负例。

### 红3 ｜ 红（在册未修） ｜ lib/infrastructure/cli/runtime_client.cpp:492-502 ｜ 运行时错误域→退出码映射与 `docs/contracts/LOG_AND_ERROR_CONTRACT.md §5` 唯一映射表不一致（RESOURCE→5、CONFIG/BACKEND→default 70） ｜ 面③

- **问题描述**：`switch (rt_ret.error().domain())` 现文为 `DATA→2`、`SCIENCE_PRECONDITION→4`、`IO→7`、`CANCELLED→9`、`RESOURCE→5`、`default→70`。合同 §5（:97-106）要求 `RESOURCE→10`、`CONFIG→2`、`BACKEND→5`，未列出域才 70。⇒ 非磁盘类资源失败以 **5（BACKEND 语义）** 上报；CONFIG/BACKEND 落 70。
- **证据（对照来源＋反方核验）**：
  - 正：LOG_AND_ERROR_CONTRACT.md:97-106 映射表；`lib/include/astrocs/core/contracts.h:17-37` ErrorDomain 八域与合同枚举逐名一致；`exit_codes.h` RESOURCE=10。
  - 反方核验①：磁盘满另有专门臂 `runtime_client.cpp:474-490`（error_kind==disk_full → 10）⇒ 日常磁盘路径正确，坏的是**非 disk_full 的 RESOURCE** 与 CONFIG/BACKEND 缺分支。
  - 反方核验②（在册面）：`eng/ci/ledgers/log_system_ledger.json:492-501 F-EXIT-MAP` 已逐字登记同一结论（owner=负责人裁决，action=「另派实现任务按 §5 订正映射并补退出码可达矩阵测试」）——本片复测 :493-502 现文如上，**至今未修**。
  - 反方核验③：只在「Runtime 返回带 domain 的错误」路径触发，不改任何科学量。
- **建议改法**：按 F-EXIT-MAP 既定 action 落地（§5 映射 + 退出码可达矩阵测试）；属在册项，本片不重复定案。

### 红4 ｜ 红 ｜ eng/packaging/config/defaults.json:643 ｜ `upm.k_corr` 的 `source` 字段仍写「冻结默认 1.4、定义域 1 ≤ k_corr、:124 记不可接受变化」，与 D-08 终裁后正本 `docs/science/PHASE2_UPM.md:24`（定义域 **1 < k_corr**、公式面两因子 k_gauss×k_geo、1.4 = 实现记录）及 `:296`（§10 所在行）冲突 ｜ 面①③

- **问题描述**：defaults.json 是机器可查的常数登记面（`astrocs.config-defaults/v1`；`TestDefaultsContract` 只校验计数与 source_ref 行存在，不校验语义）。其 `upm.k_corr` 来源串同时携带三处已废止表述：①「冻结默认」（D-08：1.4 = 代码默认/实现记录）；②「定义域 1 ≤ k_corr」（正本：**1 < k_corr**，`k_corr=1` 是**显式拒绝**）；③「:124 记不可接受变化」（现文该主张在 §10，`:296`）。
- **证据（对照来源＋反方核验）**：
  - 正：`defaults.json:638-649`（key/source/note 逐字）；`PHASE2_UPM.md:24` 现文「定义域 **1 < k_corr**（k_corr = 1 ⇔ 忽略相关，显式拒）；公式面 = 两因子 k_gauss(N)×k_geo 几何查表（D-08），代码默认 1.4 为实现记录」；grep `PHASE2_UPM.md`「不可接受变化」唯一命中 `:296`。
  - 反方核验①：`docs/contracts/CONFIG_CONTRACT.md:52` 同一条目**已按 D-08 改述**（含订正注「原『定义 + 冻结默认，:124 记不可接受变化』」）⇒ 修了合同文档、漏了 defaults.json，两处现文互相矛盾。
  - 反方核验②：值本身（1.4）未变、与 `sampler.cpp:83` 实现一致，**不主张改数值**；坏在登记面声明的**定义域与身份**——按登记面执行会允许 `k_corr=1`，而正本把它定义为显式拒。
  - 反方核验③（查重）：`独立审计/排查/全仓-01/` 全目录 grep `upm.k_corr` 零命中；`06_实施/附件/A2_051、A3_OK-054、AUD-402-判读` 只质疑 1.3883→1.4 的余量与 MC 锚，未报本条的「定义域/身份/行锚三处过期」。
- **建议改法**：**只登记**——定义域与冻结身份属 D-08 面，本片不给改写方案；建议由配置合同 owner（CFG-001）按既有变更流程复核该 `source` 串与正本 §24/§10 的一致性，并考虑给 `TestDefaultsContract` 补「source 语义 vs 正本」锚点断言。

### 红5 ｜ 红 ｜ docs/development/CONFIG_SCHEMA.md:13-14（同族 docs/contracts/CONFIG_CONTRACT.md:87-88）｜ 文档仍指示 mosaic/export 用 `config.precision` 键名，而 CLI 白名单无 `precision`、出现即 **unknown key rc=3**；死键台账的退出条件只覆盖模板/schema，未覆盖这两处文档行 ｜ 面③④

- **问题描述**：`CONFIG_SCHEMA.md:13-14`「三命令 phase_config 的精度必须显式（normalize = 块级 drizzle.precision_mode；mosaic/export = 位深键 / `config.precision` 键名）」；`CONFIG_CONTRACT.md:87`「mosaic/export 用 `config.precision`（值域 {fp32,fp64}；模板填 fp64）」、`:88`「三命令的精度键名不同（`precision` / `drizzle.precision_mode`）」。而 `parser.cpp:362-365` 明写「合同旧键 `precision` 由死键台账登记，**CLI 面拒绝**」，session_keys 全表无 `precision`，`validate_config_full` 未知键 → INPUT(3)。
- **证据（对照来源＋反方核验）**：
  - 正：`parser.cpp:272-387` session_keys 全表（阶段二/三精度键 = `bitpix`，:362-365 注释逐字）；`session_commands.h:234-235`「**不新造 precision(fp32/fp64) 同义键**」、`:296-303` export 行「合同 precision 键被拒后用户找不到精度载体 ⇒ 补 `bitpix` 字段说明」；`dead_config_keys.json:80-94 dead_config_key:precision`：「precision 作为 CLI 键**拒绝**（unknown key ⇒ rc=3）…解除路径 = FIX-207 把模板/schema 的 precision 改为各阶段实际精度键」。
  - 反方核验①（台账覆盖面）：该条 `exit_condition` 只点名 `eng/packaging/config/templates/*.json` 与两份 phase_config schema，**不含** CONFIG_SCHEMA/CONFIG_CONTRACT 这两处「让读者去写 precision」的正文；且实测两份模板文件 grep `"precision"` **已 0 命中**（模板侧已改）⇒ 台账退出条件半闭合，文档侧是新的裸奔面。
  - 反方核验②（可达性）：用户照 CONFIG_CONTRACT §3「精度显式声明」在 mosaic/export 配置写 `"precision":"fp64"` ⇒ 顶层未知键 ⇒ rc=3，且报错串不指向精度键名。
  - 反方核验③（查重）：`独立审计/排查/全仓-01/` grep `config.precision` **零命中**；S11 已报的是 fence 内 `upm_weight_source`（其红4）/`patch_radius_leaf`（黄15）/`acr_route` 注释（黄16），**未含 :13-14**。
- **建议改法**：把两处正文的 mosaic/export 精度键名改为阶段实际键（bitpix/-32|-64，与 docs/ASTROCS_DESIGN §3.3:256、parser.cpp:362-365、dead_config_key:precision.consumed_as 同口径），或在 FIX-207 退出条件中显式追加这两处文档行；本条只登记冲突、不替合同 owner 定稿。

---

## 二、黄（建议改）

### 黄1 ｜ 黄 ｜ docs/api/COMMON_ABI_V1.md:32-35 ｜ §2 `acs_status` 代码块缺 `ACS_ERR_INTERNAL=70`，与它所描述的头 `lib/include/astrocs/common_abi_v1.h:66` 不一致 ｜ 面③

- **证据**：文档块枚举止于 `ACS_ERR_SELFTEST=9`（:34）；头 `common_abi_v1.h:60-66` 含 `ACS_ERR_INTERNAL = 70`（注「等价 CLI 退出码 70 语义」）。
- **反方核验**：`abi/status_codes.h:28` 对「ABI 层新增 `ACS_ERR_EXCEPTION=10`、legacy 不产生该值、二者不得同 TU 混用」有明文分层说明 ⇒ 两头之间**无数值冲突**（0–9 与 70 两侧同值），缺的只是文档块一枚；PHASE1/2/3_API 的错误码引用也未越界。
- **建议改法**：COMMON_ABI_V1 §2 补 70，并注明 EXCEPTION=10 只属 abi 层（status_codes.h）。

### 黄2 ｜ 黄 ｜ lib/infrastructure/cli/command_tree.h:143-146 与 commands.cpp:2283 ｜ help/golden 写 `acsd doctor [--json]`（方括号=可选），实现强制 `--json`，裸 `acsd doctor` → `parse_fail("doctor requires --json")` = rc 2 ｜ 面②③

- **证据**：`help_usage()` 对 doctor 固定返回 `"acsd doctor [--json]"`，且该行是 help golden（command_tree.h:72-73 注「help 文本 golden = docs/api/CLI_PROTOCOL_V1.md §1 逐行一致」）；`commands.cpp:2282-2284` 无 `--json` 即 parse_fail。
- **反方核验**：`parser.cpp:57-58`「允许裸用（不取值）的旗标：…doctor 的 --json」说的是**不取值**（旗标而非路径），与是否**必填**是两件事；S1-L1:54-64 已从 L1 侧讨论 doctor `--json` 的单 JSON 文档语义，**未覆盖**必选/可选这处；排查目录 grep 无同题条目。
- **建议改法**：若 doctor 只服务机器消费，help 行改 `acsd doctor --json` 并同步 CLI_PROTOCOL §1 golden；若确应可选则去掉强制。二选一，由 CLI 域主定。

### 黄3 ｜ 黄 ｜ lib/infrastructure/gaia_xpsd_client/README.md:4-9、:19、:66、:136、:145、:154 ｜ 状态与测试面三处陈述与仓内现状相反（entrypoint「零调用 ⇒ NOT_IMPLEMENTED / 迁移未开始」、「当前无可执行测试 / 可执行测试未建立」） ｜ 面③②

- **证据**：`src/module_entry.c` 1061 行，`:1038-1048` 静态 vtable `g_gaia_api` 装配 `gaia_describe/validate_config/plan/create/execute/inspect/request_cancel/destroy` 八个实函数，`:1052-1060` 唯一导出 `astrocs_module_query_v1` 返回该 vtable；`eng/ci/checks.json:3119-3152` 登记 **UT-GAIA-ZLIB**（`waivable:false`，unittest discover `lib/infrastructure/gaia_xpsd_client/tests`）；`eng/tests/unit/CMakeLists.txt:1769-1773` 登记 `test_gaia_race.c`/`test_spec_collector_ownership.c` 两个 add_executable；模块自身 `CMakeLists.txt:77` 也引用 `tests/test_gaia_zlib_configure_contract.py` 作回归门。
- **反方核验**：①「函数体零调用」对 `astrocs_module_query_v1` 字面成立（体只返回 vtable，:1056-1060），但由此推出「模块化迁移未开始 / NOT_IMPLEMENTED / 无可执行测试」与上面的 adapter、三处测试登记面直接冲突；②`eng/ci/prod_wiring_baseline.json:91-92` 把两个 C 测试标为 `unbuildable_test`（基线在册）⇒ 正确表述应是「C 两件不可构建」而非「无可执行测试」；③`module.yaml:1-12` 自记 entrypoint 由 CAT-GAIA-IMPL commit babe752d 建立、nm -D 实证。
- **建议改法**：README 状态注记按检查器实际口径重写，区分「query 函数体无调用」与「adapter 未实现」，并把 §8/§9-8 的测试陈述改为「C 两件在基线 unbuildable、python 门 UT-GAIA-ZLIB 非豁免在册」。

### 黄4 ｜ 黄 ｜ lib/infrastructure/gaia_xpsd_client/README.md:38 vs :98-101 ｜ 同一文档两套目录键名：§3 输入 port `catalog.xpsd_dir`，§5.1 配置 schema 却写 `catalog.data_dir` ｜ 面②④

- **证据**：全仓 grep：`xpsd_dir` 18 处（module.yaml:28、integration JSON:39/:65-69、module_entry.c:29、eng/tests/unit/gaia_integration_test.c:539/:576）；`catalog.data_dir`/`catalog.db_type` **仅 README:98-99 两处**。
- **反方核验**：README §5.1 自陈「迁移合同；现状为 C 参数直传，无配置文件」⇒ 键未接线本身已如实登记；但同文两键名指向同一目录，将来接线必错一侧——C 形参名是 `data_dir`（gaia_client.h:82-83），`module_entry.c:29` 又写「catalog_dir 为数据集 port (catalog.xpsd_dir)」，三处命名混用。
- **建议改法**：README §5.1 键名与 port 对齐，或显式说明「config 键 ≠ port 名」及其理由。

### 黄5 ｜ 黄 ｜ docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md:29、:36、:167-168、:3 ｜ ①同节先声明「不复制其字段表」却复制了运行事件流 kind 枚举，且只列 5/10；②引用旧编号与不存在的 §7.1a ｜ 面②③④

- **证据**：:29 事件枚举 `progress/resource/artifact/backend/final`——`lib/infrastructure/cli/protocol.h` 的 `registered_event_kinds_v1()` 实为 **10 类**（`jsonl_event_v1.schema.json` kind 枚举逐字 10 项，脚本核对一致）；:35-36 同段自述「运行事件流的字段名/枚举/顺序键唯一以 protocol.h+jsonl.h 为源；本合同**不复制**其字段表」。旧锚：:29/:36「最高设计 §6.3」、:167「依据 docs/ASTROCS_DESIGN.md §6.3」（§6.3 现为投影算法）、:168「§7.1a」（L1 无此小节）、:3「§8.1（顶层结构）」（顶层结构 = §8.4，§8.1 是总原则）。
- **反方核验**：:65 phase 枚举与 schema `phase` 枚举（phase1/phase2/phase3/runtime/monitoring/cli/""）**逐字相同** ✓；§2.2 的 15 个 required 与 schema required **完全一致**（脚本比对）✓；故问题只在运行事件流行与上游锚，字段表本身干净。
- **建议改法**：枚举行标「节选」或直接指向 protocol.h；旧编号锚随全仓 §6→§7 重排同步。

### 黄6 ｜ 黄 ｜ docs/contracts/LOG_AND_ERROR_CONTRACT.md:82-93 ｜ §5 定义的唯一错误对象 `error_report`（9 字段）全仓零机器落地 ｜ 面④

- **证据**：全仓 grep 字面 `"error_report"` **0 命中**；`lib/` 内 6 处非零命中全在 `scheduler/src/module_adapters.cpp` 注释（:5330/:6201/:6285/:6344/:7639/:8390，「error_report 形状/口径」）；CLI 侧错误收敛面是 jsonl final 事件 `{exit_code,status,run_manifest,summary}`（jsonl.h:219-231）+ 节点 manifest 散字段（error_domain/error_status/error）。
- **反方核验**：§5 自身把域映射偏差登记进 `log_system_ledger#findings`（说明实现面另有其形）⇒ 不是「完全没实现错误报告」，而是**文档命名的对象没有同名机器形态**，消费方按 §5 找 `error_report` 会找不到；枚举与退出码本身一致（见已查无问题面 1）。
- **建议改法**：或在 §5 注明「该对象当前以 final 事件 + manifest 散字段承载，尚无同名 JSON 对象」，或补机器落地；本条登记不改码。

### 黄7 ｜ 黄 ｜ lib/infrastructure/cli/**（75 处 / 16 文件）｜ 源码注释、README、module.yaml、CMakeLists 大面积引用 docs/ASTROCS_DESIGN **旧编号** §6.1/§6.2/§6.3（现 §7.1/§7.2/§7.3）与 §3.5（现 §4.5） ｜ 面④

- **证据（分布实测）**：grep `§6.1|§6.2|§6.3|§3.5` 于 `lib/infrastructure/` 共 84 处，剔除 scheduler(9)、orchestrator(1) 后本域 cli/** = **75 处**：subcommand.h 19、command_tree.h 15、commands.cpp 12、disk_gate.h 5、module.yaml 3、session_commands.h 3、export/mosaic/normalize 头各 2、resource_gate.h 2、jsonl.h 2、README.md 2、parser.cpp 2、runtime_client.h 1、CMakeLists.txt 1、main.cpp 1。代表锚：`command_tree.h:1/:8/:36-41/:88`、`disk_gate.h:4/:6/:213/:242`、`main.cpp:3`、`README.md:7/:13`、`session_commands.h:104/:391`、`module.yaml:3/:28`。
- **反方核验**：①现行编号已实测——L1 §7.1 命令树、§7.2 配置/事件/退出码（含 exit 10 行）、§7.3 错误传播、§4.5 运行前预检 ⇒ 这批引用里**数值/语义对、章节号错**（`runtime_client.cpp:470-471` 写 §7.2 是少数写对的）；②同族在 L1 侧已由 **S1-L1:122-127 域外-1** 登记并显式移交「代码侧修」——本片即该移交的落地登记，不另计为新族；③`grep '21_observability §8.4'` 与 `PHASE3_DETAILED_DESIGN §8.2/§8.4` 属**合法**引用，已从计数剔除。
- **建议改法**：按句义逐处机械重锚（§6.1→§7.1；§6.2→§7.1/§7.2 按「命令树/模板与机器输出」句义；§6.3→§7.2 或 §7.3；§3.5→§4.5），**禁止整串替换**；与 S1 域外一并收口。

### 黄8 ｜ 黄 ｜ eng/ci/ledgers/log_system_ledger.json:503-510 ｜ `F-PHASE-ENUM` 登记的「文档 phase 枚举被写坏」与现状不符（条目已过期） ｜ 面②

- **证据**：条目称 `STRUCTURED_LOGGING_CONTRACT.md §2.2` 的 phase 枚举被写成 `phase1/phase2/phase3/lib/infrastructure/observability/monitoring/cli/""`；实测该文档 :65 现文为 `phase1/phase2/phase3/runtime/monitoring/cli/""`，与 schema 枚举**逐字相同** ⇒ 该 finding 声称的不一致已不存在。
- **反方核验**：条目后半句「phase1/2/3 是最高设计 §7.1 明令不出现在代码目录的内部指代」本片未在 L1 §7.1（现为命令树）核到原文 ⇒ **不主张整条删除**，只登记「evidence 与现状不符、需复核」。
- **建议改法**：由 CHK-LOG-SYS owner 复核该条 evidence；台账条目只减不增，过期条目复核后移除。

---

## 三、绿（可不改）

### 绿1 ｜ 绿 ｜ lib/infrastructure/observability/PENDING.md:3 ｜ 引「docs/ASTROCS_DESIGN §7.1（顶层结构唯一）」——现行 §7.1 是命令树，顶层结构 = §8.4 ｜ 面④

- **证据**：PENDING.md:3/:13 两处以 §7.1 承载「顶层结构」；L1 现行 §7.1 = 命令树、§8.4 = 顶层结构。
- **反方核验**：grep `lib/infrastructure/observability` 对 `§6.x/§3.5` **零命中**，本域仅此一旧锚；同文 :14 已自陈「本目录缺 README/module.yaml 而 MODULE_MAP 声明 readme 不解析（登记不改）」属自陈登记项。
- **建议改法**：随全仓重排顺手改锚号即可。

### 绿2 ｜ 绿 ｜ lib/infrastructure/gaia_xpsd_client/README.md:94-103 ｜ §5.1 配置键（catalog.data_dir/db_type/ra/dec/radius_deg/mag_*）为「迁移合同」，现状 C 参数直传、无配置文件 ｜ 面④

- **反方核验**：README 自陈未接线，非「文档说有、代码偷接」；键名冲突另立黄4。属如实登记，无需改。

### 绿3 ｜ 绿 ｜ lib/infrastructure/observability/logging/README.md:12 ｜ 相对路径 `../../docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md` 从本文件出发不可解析（需 `../../../../docs/…`） ｜ 面④

- **反方核验**：按仓根解读该路径成立（文件实存）⇒ 只是相对/根路径口径混写，不影响可达性结论。

### 绿4 ｜ 绿 ｜ lib/infrastructure/cli/commands.cpp:2404-2413 ｜ `session_dispatch` 的 `SessionOp::Validate/Plan/Inspect` 三分支零调用者（subcommand.h:411/:467 只传 `Run`） ｜ 面④

- **反方核验**：全 grep 仅这两处传入 Run ⇒ 三分支不可达、无副作用、不改变任何可达行为，属预留死分支。建议清理或注明「预留」。

### 绿5 ｜ 绿 ｜ lib/infrastructure/cli/parser.cpp:681（声明 cli_common.h:125）｜ `validate_cpu_profile` 全域零调用 ｜ 面④

- **反方核验**：grep `validate_cpu_profile lib/` 仅命中声明(:125) 与定义(:681)；`--cpu-profile` 的实际消费在 commands.cpp（读文件 + `cli_memory_budget_percent` 异常兜底，LSW-001 在册）而非本函数。S12 红3 已在 docs/api 侧登记 cpu-profile 合同问题，本条是**代码侧死函数**的对偶登记，不与 S12 重复计数。

---

## 四、已查无问题面（含抽样方法）

1. **退出码唯一源 ↔ 三份权威（①③）**：`lib/infrastructure/cli/exit_codes.h` 全 21 行读，11 个码（0/2/3/4/5/6/7/8/9/10/70）与 `docs/api/CLI_PROTOCOL_V1.md §2`、`docs/architecture/ERROR_MODEL.md` 退出表逐行逐值一致；`LOG_AND_ERROR_CONTRACT §5` 的 ErrorDomain 八名与 `lib/include/astrocs/core/contracts.h:17-37` 逐名相同。**方法**：三表逐行 diff + 枚举源码逐名比对（唯一偏差即 runtime_client 映射，已立红3）。
2. **命令树 ↔ help golden ↔ CLI_PROTOCOL §1（②③）**：`command_tree.h:89-164` 全读，命令集/顺序/旗标与 §1 行逐行一致；`--events-jsonl` 别名在 :93/:96/:99（文档锚成立）。**方法**：help_text() 生成逻辑逐条走查 + §1 对照（S12 黄1 的 commands.cpp:2115 锚漂移在其片，本片不重复）。
3. **JSONL 事件 schema ↔ 协议正本（②③）**：`protocol.h` 的 10 required 字段、10 kind、kExt 扩展集与 `eng/contracts/schemas/jsonl_event_v1.schema.json` 脚本逐字比对**全部相等**（schema 的 `x-astrocs-event-kind-registry` 反向指认 protocol.h 为 implementation_authority）；`final.exit_code` 受 `is_frozen_exit_code_v1` 冻结域校验（protocol.h:31-35/:142-143）。**方法**：python 解析两份 JSON 做集合差。
4. **配置三形态错误码 ↔ MANIFEST_VERIFY_V1 §1 / CLI_PROTOCOL §7.1-7.2（②③）**：`validate_config_full`（parser.cpp:559-678）三形态判定、output_dir 规则（V1→3、平铺→2、blocks→2、退役逐帧→3）与 MANIFEST_VERIFY §1 表逐条一致；`session_blocks_errors` 结构/块内键/顶层混用分支与退出码注释自洽。**方法**：函数体全读 + 两份文档表逐行对照。
5. **LOG-001 字段面（②③）**：`log_event_v1.schema.json` 的 15 required / event 9 枚举 / phase 7 枚举 ↔ `STRUCTURED_LOGGING_CONTRACT §2.2` 表**逐字一致**（含 seq≥1、单行 4096 与 §6 同口径）。**方法**：schema 解析 + 表格逐格比对（异常只剩黄5 的运行事件流 kind 行）。
6. **v6_runtime_contract 与 docs/contracts（③）**：`evaluate_heavy_run` 返回 `record_only_pending_owner_signoff`、`hard_fail=false`、`kDeterminismContractId=FZ-RUNTIME-DETERMINISM`——与「§9.74 裁决 10：一般性资源超限门只留磁盘」的 L1 现行条款、`resource_gate.h:4/:252` 退役说明同向；`docs/contracts/` 内 grep `v6_mode_gate`/`RUNTIME-CI-001` 无矛盾条文。**方法**：头文件关键谓词 + docs/contracts 全量 grep。
7. **ABI 面逐表（③）**：`abi/status_codes.h`（0–10 + DOMAIN 0–7 + 70）、`common_abi_v1.h`（0–9 + 70）、`abi/module_api_v1.h` vtable 八函数与生命周期序（query→describe→validate_config→plan→create→execute*→inspect→request_cancel→destroy）、`host_api_v1.h` 服务表，与 `COMMON_ABI_V1 §2-§5`、`PHASE1/2/3_API_V1` 的 acs_status/函数签名/拒绝清单一致；`p1_session_create`/`p2_session_create`/`p3_session_create` 实现与文档签名逐字命中（p1_session.cpp:159、p2_session.cpp:58、p3_session.cpp:52）。**方法**：头全文读 + 文档签名 grep 反查（异常只剩黄1）。
8. **PHASE1/2/3 错误码枚举（①③）**：PHASE3_API 拒绝清单的 `ACS_ERR_UNSUPPORTED/PARAM/IO`、PHASE2:51 的 rc→acs_status 映射均落在 status_codes.h 枚举域内，无越界值；D-01…D-11 终裁在本域未发现被翻案的表述；非退化判据相关处未见「恒真门」。
9. **资源/磁盘门口径（①）**：`disk_gate.h`（唯一 rc=10 臂）、`resource_gate.h`（Enforced 已退役注）、`commands.cpp:297-298` 运行期磁盘臂与 L1 §7.2「10 = 磁盘写满 / 写盘失败」、§9.74 裁决 10 同向；无一般性内存/CPU 门残留（commands.cpp:814/:1086、runtime_client.h:25 同注）。
10. **死配置键三方专项（④，本片执行 + 引 S11 复核）**：① `CONFIG_SCHEMA.md` fence ↔ `stage2_common.cpp` 解析键——`acr_route`(:468)、`memory_limit_mb`、`target_order`、`snr_weight_mode`(:164) 等**全部有消费者**（S11 面④②：17 键全命中，死键仅其红4/黄15，本片复测一致）；② `defaults.json` 59 键 ↔ 全仓消费者——**S11 实测 0 死键**，本片对 `upm.k_corr` 的核查只涉来源串语义（红4）不涉接线；③ `filters.json` 有消费者（S11 同项）；④ CLI `session_keys()` ↔ 死键台账——`snr_path/rotation_deg/crpix_px/storage_form/coverage_index/algorithm_rejection_method/reject` 等均在 `dead_config_keys.json` 具名登记（含 exit_condition 与 owner），非静默；模板侧 `precision/center_deg/s_out_deg` 已实测 0 命中（FIX-207 模板半程已落地）。
11. **observability 其余（②④）**：`probes/README` 示例旗标 `-y` 是 `command_tree.h:39` 登记旗标 ✓；monitoring 自陈「windows_pdh_etw 采集未实现」为如实登记；logging 边界节（运行事件流 vs LOG-001 双流不冒充、键名不混用）与 `jsonl.h:3-6` 同口径 ✓。
12. **重复劳动避让核查（④）**：对 `独立审计/排查/全仓-01/` 全目录 grep `upm.k_corr`、`config.precision`、`STRUCTURED_LOGGING`、`ACS_ERR_INTERNAL`、`noop_entrypoint`、`doctor --json`——除已引的 S1/S11/S12 条目外零命中；PASS 表 28 项已核对未重报；S12 专属（CLI_PROTOCOL:44 锚、resource_recorder 锚、PHASE3 variance、RELEASE_STATUS）本片一律避让。


