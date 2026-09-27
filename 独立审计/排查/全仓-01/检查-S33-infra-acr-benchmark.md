# 检查 S33 — lib/infrastructure acr 核心＋benchmark（全仓对抗性静态审计）

- 切片：`lib/infrastructure/acr/`（api/、backends/、core/、cost/、diagnostics/、include/、profile/、routing/、scheduler/、topology/、utilization/、ci/、README、tools/、qualification/、tests/ 中与文档断言相关处）＋ `lib/infrastructure/benchmark/`（backend_host/ 全部、cpu/ 三 provider、capability_detect、schema）。
- 方法：四面逐项（①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚）；所有 file:line 锚**逐条实开**；检索词均先 `grep -v run/` 排除快照副本。
- 前置排除：`独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表已核（28/30 PASS，残留 3 项 k_shape 记号 + UPM_SOLVER.md:26 死锚，均属科学面、不在本切片）；D-01…D-11 为实验裁决，本切片代码未见与之相悖处。
- 防重复：对既有切片报告（S1…S23）就 `acsd benchmark cpu / ACK-ACR / acr README / profile_store / CPU-007 / 自动读取 / selection_rules / memory_budget_percent / kernel_v1|kernel_v2|W5-CPU-001 / cpu_routing.cpp:216` 十组关键词反查：
  - `memory_budget_percent` 仅 S23 检查过"默认 95 唯一数值源"一处并明确写下"配置死键……未出现"（检查-S23-修复包⑤接线.md:200、:202，其范围是修复包⑤五件）；本报告 R1 是**生产者/合同侧**死键，S23 未覆盖，不重复。
  - `kernel_v1|kernel_v2|W5-CPU-001` 命中 S23 的是 UT-CONFIG/接线负例面；本报告 Y4 是 CONFIG_CONTRACT §5 正文**现时态**与同文件 §表自相矛盾，S8/S23 均未登记（下文 Y4 有对照）。
  - 其余八组关键词在既有报告 0 命中。

---

## 一、问题清单

### 红（2 条）

---

#### S33-R1【红】`host.memory_budget_percent` 是配置死键：唯一权威声明的"profile 覆盖"路径结构性不可能，且被冻结 schema 明令禁止

- **所属面**：④ 幻觉与锚（配置死键 + schema 与文档漂移），连带 ③
- **文件:行**
  - `eng/packaging/config/runtime_resources.json:18`：「运行期可经机器绑定 profile（--cpu-profile 指向的 astrocs.cpu-profile/v2 的 **host.memory_budget_percent**）覆盖；profile 未声明或取值非法时用本文件的默认值」；`:19` 同句再述。
  - `lib/infrastructure/cli/commands.cpp:126-129`（注释「比例 = 机器绑定 profile 的 host.memory_budget_percent 优先」）与 `commands.cpp:144-149`（唯一读点）。
  - **生产者**：`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp:565-576`（host 对象只写 arch/vendor/family/model/stepping/os_abi/features/xcr0/logical_available/quota_signature 共 10 键；`:662-663` verify 必查键表同为这 10 键）。
  - **合同**：`eng/contracts/schemas/cpu_profile.schema.json:422-436`（$defs.profile_v2.properties.host，`additionalProperties:false`，properties 仅上述 10 键）；legacy 分支 host `schema:210-241` 同样 `additionalProperties:false`。
- **证据（反核）**
  1. 全仓 `grep -rn memory_budget_percent`（排除 run/、索引、审计件）：**生产者零命中**——唯一写此键的代码不存在；命中全部是消费侧（commands.cpp、runtime_client、scheduler runtime.cpp:126）与登记文本。
  2. 全文 `grep memory_budget_percent eng/contracts/schemas/cpu_profile.schema.json` = **0**；host 两分支均 `additionalProperties:false` ⇒ 合法 profile 携带该键会被合同**判红**（`eng/tools/validate_cpu_profile.py:41-70` 按顶层/host additionalProperties 执行）。
  3. 时序反核：即便手工往 profile 里塞该键（绕过合同），也需要"benchmark 写入"这一唯一合法写者（schema `x-astrocs-writer=benchmark`，CONFIG_CONTRACT.md:134）——而 benchmark 生成路径不写它 ⇒ 文档承诺的覆盖在**正常链路里永远回落默认 95**，且回落由 `commands.cpp:148` 归零后不可观测。
  4. 旁证：`独立审计/证据/通读-CR-70.md`（同链路前一轮证据件）只核了"回落可观测性"与"95 无推导"（:266-284），**未发现**生产者不写 + schema 禁止这两层；S23:202 更明确写过"配置死键……未出现"（范围=修复包⑤）。故本条为新发现。
- **建议改法**（登记不代改）：二选一并同批改四处——① 让 profile_gen_v2 在 host 里写 `memory_budget_percent`（从 runtime_resources.json 生成头取默认，禁止第二个数值源），schema host properties 增列该键（integer 1..100），并补负例 fixture；② 承认覆盖不存在：删 runtime_resources.json:18-19 与 commands.cpp:126-129 的覆盖表述，改为"仅默认值，profile 不参与"。任一方案都必须同步 `eng/packaging/config/config_registry.json` 与通读-CR-70 的登记。

---

#### S33-R2【红】最高设计「benchmark 产出的 cpu_profile 运行时自动读取」整条消费链零接线：校验、建议、路由、存储四层全部只有测试调用方

- **所属面**：④ 幻觉与锚（文档说存在、代码没接），连带 ②
- **文件:行（声称侧）**
  - `docs/ASTROCS_DESIGN.md:511`「benchmark ……生成/更新安装目录 cpu_profile（**自动读取**）」；`docs/ASTROCS_DESIGN.md:518`「输出 profile 到安装目录，**运行时自动读取**……profile 缺失时按保守参数运行，显示经过但不阻塞」（最高权威）。
  - `docs/api/CLI_PROTOCOL_V1.md:31`「`benchmark` 生成/更新**安装目录** cpu_profile（后续运行自动读取）」。
  - `lib/infrastructure/cli/commands.cpp:2344`「机器绑定配置，运行时自动读取」。
- **文件:行（实际侧，逐条反核）**
  1. **无自动读取**：`commands.cpp:2386` 写 `install_dir/cpu_profile.json`；全仓没有任何代码读 `<exe_dir>/cpu_profile.json`（grep `cpu_profile.json` 仅命中生产者 profile_gen*/profile_store/CLI 写点与测试）。运行期唯一带 profile 的入口是可选旗标 `--cpu-profile`，其唯一消费是 `commands.cpp:130-152` 的 memory_budget_percent（恰是 R1 的死键）→ 即 **flag 给了 profile，运行行为也只差一个恒回落的比例**。
  2. **校验层未接线**：`lib/infrastructure/cli/parser.cpp:681-706` `validate_cpu_profile`（CPU-004 结构+机器一致性，失败拟退 exit 5）**零调用方**（全仓 grep：仅定义处 + `cli_common.h:125` 声明）。
  3. **建议层未接线**：`worker_advisor.cpp`（profile 行→workers/block、heavy 拒 worker=1、fallback 链完整）调用方只有 `eng/tests/unit/cpu008_worker_advisor_test.cpp`。
  4. **路由层未接线**：`cpu_routing.h:117/:130/:145` 的 `decide_kernel_v1` / `build_route_table_v1` / `route_kernel_from_profile` 生产零调用（命中仅 `eng/tests/cpu/dispatch/*`）；生产侧唯一调用是 `parser.cpp:697` 的 `validate_profile_v2_for_machine`——而它所在的 `validate_cpu_profile` 自身无人调。
  5. **存储层未接线**：`profile_store.h`（CPU-007 原子写→校验→rename、失效改名 `.rejected-utc`、"半写结构性不可见"）的 `save_profile_atomic_v1`/`load_profile_checked_v1`/`default_profile_path_v1` 生产零调用（命中仅 `eng/tests/unit/cpu007_profile_store_test.cpp`）；生产写盘是 `commands.cpp:2387-2394` 裸 `ofstream trunc`（无 verify_profile_v2、无身份校验、无 fsync、无原子 rename——崩溃窗口直接留下半写 profile，恰是 profile_store.h:20-27 自称"结构性保证"要消灭的形态）。
  6. **旁证**：run manifest `cpu_profile_path/cpu_profile_sha256` 恒为 `nullptr`（`commands.cpp:351-352`，全仓无第二处赋值），与 `eng/ci/fixtures/run_manifest/real_manifest_sample.json:34-35` 恒 null 一致——若真"自动读取"，这两个 provenance 字段不可能恒空。
- **反核（避免误判）**
  - `backend_loader`（doctor `commands.cpp:2316-2325`）与 `generate_profile_v2`（`commands.cpp:2375`）确有生产调用——所以**不是**"整条 benchmark 链没接"，而是**"产出→消费"这最后一跳没接**；
  - 生产执行当前不读 profile 也**不构成铁律违反**（生产路径未见硬编码线程/ISA/block，见"已查无问题面①"），风险在"最高设计条款失效 + 用户按文档预期得到假保证"。
- **建议改法**（登记不代改）：要么接线（run 入口读 `<exe_dir>/cpu_profile.json` → `load_profile_checked_v1` → 缺失/失效按 V8-CPU-002 保守回落并打"经过"事实行，同时给 manifest 两字段填值），要么降级 docs/ASTROCS_DESIGN.md:511/:518 与 CLI_PROTOCOL:31 的"自动读取"为"当前未接线（登记为已知缺口）"。按 AGENTS §1.1，此项属顶层合同表述，需负责人裁决方向。

---

### 黄（7 条）

---

#### S33-Y1【黄】`lib/infrastructure/acr/README.md` 目录树块 6 处与实际不符 + 2 条死链，且与同文件“历史文档已清理”的自述互相矛盾

- **所属面**：④ + ②
- **文件:行**：`lib/infrastructure/acr/README.md:41`、`:44-65`、`:93-94`。
- **证据（逐条实开）**
  | README 行 | 断言 | 实际 |
  |---|---|---|
  | :41 | 「详见 `工程控制/tasks/acr/spec.md`」 | `工程控制/tasks` 目录**不存在**（工程控制/ 仅 RELEASE-05） |
  | :30 | 「ACR 实验入口（`eng/tools/qualification/scheduler`）」 | `eng/tools/qualification` **不存在** |
  | :48 | `lib/include/astro/compute/` | 实为 `include/astro/compute/` |
  | :51 | `buffers/` 目录 | 不存在（BufferView/Buffer 在 `include/astro/compute/acr.hpp`） |
  | :53 | `backends/{cpu,alpaka,cuda,hip,sycl,starpu_optional}/` | 实为 `backends/{classic,cpu,cuda}/`——列了 4 个不存在的，漏了 classic |
  | :59 | `eng/tests/{unit,classic,fault}/` | 实为 `tests/{unit,classic,fault,integration,sanitizer}/`（前缀错 + 漏 integration/sanitizer） |
  | :60 | `eng/tools/{acr_benchmark,acr_status,acr_report,acr_invalidate}/` | 实为 `tools/{acr_benchmark,acr_classic_runner,acr_report,acr_status}/`——`acr_invalidate` 不存在、`acr_classic_runner` 未登记 |
  | :62 | `schemas/ # route_profile schema` | `schemas/` 只有 compute_config.example/evidence_manifest/hardware_profile/task_descriptor 四件，**无任何 route_profile schema**（route profile 的机器面只是 C++ `validate_route_profile_v2` 注册子集 {schema,version,generated_by}，`routing/route_profile_v2.cpp:528`） |
  | :64 | `eng/ci/path_guard.ps1` | 实为 `ci/path_guard.ps1` |
- **行文矛盾**：`:93-94` 自述「历史 ACR 任务文档（spec/checklist/tasks……）已随治理工件清理删除」——与 `:41` 仍把 `工程控制/tasks/acr/spec.md` 当活链接、`:30` 仍把 eng/tools/qualification 当现存在，构成同一文件内旧说法/新说法并存。
- **反核**：README 的休眠边界五条（:17-30）与机器事实一致（根 CMake 无 add_subdirectory、ASTROCS_ENABLE_ACR=OFF、install/manifest 零 ACR）——错的只是目录树/链接层。
- **建议改法**：目录树块按 `find lib/infrastructure/acr -maxdepth 2` 重生成；`:41`/`:30` 两处改指现行权威（`docs/science/ACR_EQUIVALENCE.md`、`docs/science/algorithms/ACR_EQUIVALENCE.md`）或删句；schemas 行改为「schemas/ ……（route_profile 机器面在 routing/route_profile_v2.cpp，无独立 schema 文件）」。

---

#### S33-Y2【黄】休眠守卫 `check_acr_dormant.py` 的 ACK-ACR-005 文档断言失实，且"产品 target 源表排除"这条它自己声称核过的判据**根本没实现**（自检也无对应负例）

- **所属面**：② + ④
- **文件:行**
  - `lib/infrastructure/acr/ci/check_acr_dormant.py:29-31`（docstring：「the phase2 legacy stub **cuda_bridge_stub.cpp is excluded from the root product target list** (BLD-002 source whitelist)」）与 `:64-66`（LEGACY_ACR_SOURCES 注释同句）。
  - `lib/infrastructure/acr/ci/ACR_DORMANT_GUARD.md:10`（ACK-ACR-005 逐字复述）。
- **证据**
  1. **断言与事实相反**：根 `CMakeLists.txt:623-633` 的产品 target `astrocs_phase2` 源表**包含** `lib/algorithms/coverage/src/cuda_bridge_stub.cpp`（:633，注释「phase2 生产模块; ACR 源不在此列 — LEG-004」）。真实情况是"stub 在产品 target 内、ACR loader 不在"，与 docstring 写的"stub 被排除"相反。
  2. **所声称的检查未实现**：ACK-ACR-005 实际实现（`check_acr_dormant.py:153-178`）只对 `PROD_TREES`（:56-66，**不含 `lib/algorithms/**`**）做标记串扫描，从不读任何 target 源表；`LEGACY_ACR_SOURCES` 的按名跳过（:165-166）对不在 PROD_TREES 里的文件是不可达分支。
  3. **自检缺位**：`selftest` 七案例（:244-289：add_subdir/no_guard/preset_on/install_acr/prod_include/registry_open/manifest_acr）没有一例注入"ACR 源被加进产品 target"，即守卫对它自称冻结的边界**不可证伪**。
  4. **反核（为何只定黄不定红）**：边界本身成立——生产 target 只编 stub（恒返回 bridge 不可用，`cuda_bridge_api.hpp` 为契约头），真实 `cuda_bridge_loader.cpp` 仅在 coverage 兼容 `phase2` 测试 target（Windows）里编译；install/manifest/registry 三面 grep 零 ACR；`docs/owner/ARCHITECTURE_OVERVIEW.md:54` 对同一事实的表述是**准确**的（如实登记 stub 在根源表、acr_kernels.cpp 不在）。故缺陷集中在"守卫文档的失实断言 + 未实现判据"，不是生产泄漏。
- **建议改法**：docstring/GUARD 的 ACK-ACR-005 第二句改为真实口径（「产品 target 只含非功能 stub cuda_bridge_stub.cpp；ACR loader/registry 源不在根源表」），并在 checker 里补一条真正的判据（扫描根 CMakeLists astrocs_phase2 源表禁止出现 `lib/infrastructure/acr/**` 与 `acr_kernels.cpp`）+ 对应 selftest 负例；`PROD_TREES` 增列 `lib/algorithms` 或明确声明不含并把注释改真。

---

#### S33-Y3【黄】三处面向用户的提示与 `bench_report` 头注仍指向已删除命令 `acsd benchmark cpu`；而 bench_report 层自称"实现该命令"却无任何生产调用方

- **所属面**：② + ③ + ④
- **文件:行**
  - `lib/infrastructure/benchmark/backend_host/cpu_routing.cpp:138`：`stale_reason = "quota_signature mismatch (rerun 'acsd benchmark cpu')"`。
  - `lib/infrastructure/benchmark/backend_host/profile_store.cpp:298-300`：missing 警告 `"Run 'acsd benchmark cpu' to generate one."`。
  - `lib/infrastructure/benchmark/backend_host/bench_report.h:1/:4`：「实现 `acsd benchmark cpu --suite quick|release --output profile.json`」。
- **权威对照（反核）**
  1. 命令树唯一登记是 `lib/infrastructure/cli/command_tree.h:110` `{"benchmark", true, {}}`——**无子命令 cpu、allowed 旗标为空**；`docs/api/CLI_PROTOCOL_V1.md:8-9` 明文「`benchmark cpu` ……不在命令面上，调用返回 rc=2」，`:20` 命令树只有 `acsd benchmark`。
  2. 实际行为：`parser.cpp:126-137` 在 "benchmark" 定案后，`cpu` 走 `parser.cpp:147` `unexpected positional argument 'cpu'`（附带：`parser.cpp:134-135` 注释声称这类拼写报 "unknown command"，也与实现不符——cli 域附带观察，不计本切片条目）。`--suite/--output` 也会被 `parser.cpp:148` `unknown flag` 拒绝。
  3. 层次断言落空：`generate_benchmark_report` / `verify_benchmark_report` 的全仓调用方只有 `eng/tests/unit/cpu006_bench_report_test.cpp`；生产 `acsd benchmark`（`commands.cpp:2343-2397`）直接调 `generate_profile_v2` 自行落盘，**不经过** bench_report 层。即 `bench_report.h:4` "实现 acsd benchmark cpu --suite ……"两条（命令、旗标、报告产物）都无对应命令面。
- **建议改法**：`bench_report.h:1-4` 改为真实定位（「CPU-006 报告聚合层，当前由 cpu006 单测驱动；命令面为 `acsd benchmark`（无旗标），profile 落盘走 commands.cpp」）；`cpu_routing.cpp:138`、`profile_store.cpp:300` 提示串改 `'acsd benchmark'`；`parser.cpp:134-135` 注释把 "benchmark cpu → unknown command" 改为真实错误词（cli 域，随批）。

---

#### S33-Y4【黄】`CONFIG_CONTRACT.md` §5 把「kernel 接线缺口」写成**现时态**，与同文件 §5 表格「已闭合」、schema 实测、登记册三方矛盾（同一文档内旧/新结论并存）

- **所属面**：② + ③
- **文件:行**
  - `docs/contracts/CONFIG_CONTRACT.md:138`（§5 正文 bullet）：「**kernel 接线缺口**……`$defs.kernel_v1`/`kernel_v2` **被 0 处 `$ref`**……workers/block/provider/self_test_sha256 等约束已定义但**未被 schema 施加**；`config_registry.cpu_profile_kernel_link` **登记该状态并 pin**」。
  - 同文件 `:193`（§5 问题表行）：「**已闭合（W5-CPU-001，2026-09-17）**：……refs=1/1；登记册 `cpu_profile_kernel_link.status=LINKED`，门 CFG002-10」。
- **证据（反核三方）**
  1. schema 实测：`eng/contracts/schemas/cpu_profile.schema.json:25`（x-astrocs-notes）「两处 refs=1/1（登记 …#cpu_profile_kernel_link，门 CFG002-10）」；`$defs.kernel_v2` 由 `profile_v2.properties.kernels.additionalProperties $ref`（$defs 区），`grep '\$ref' cpu_profile.schema.json` 中 kernel 两处各 1。
  2. 登记册：`eng/packaging/config/config_registry.json#cpu_profile_kernel_link.status = LINKED`。
  3. ⇒ :138 的"0 处/$ref 未施加/登记该状态"三句全部是**接线前**的旧结论，却以现在时写在正文 bullet（表格行 :193 已更新）——读者按正文会得出与登记册相反的状态。
  4. 与邻片边界：S8 黄-7 覆盖的是 UT-CONFIG 门与 checks 计数；S23 覆盖接线负例本身——**正文旧句**均未被登记（已对既有报告 grep `kernel_v1|kernel_v2|W5-CPU-001` 反查）。
- **建议改法**：把 :138 整段改写为"历史（已闭合）"并指向 :193，或直接删 bullet；同批核对该段引用的 `cpu_profile_kernel_link` 描述词（"登记该状态并 pin"→"登记 LINKED 并 pin refs 计数"）。

---

#### S33-Y5【黄】冻结 schema `cpu_profile.schema.json` 描述字段内的 3 个生产者锚全部漂移，且与 CONFIG_CONTRACT 的同款正确锚互相打架

- **所属面**：④ + ③
- **文件:行（schema 侧，错） vs 文档侧（对）**
  | schema 锚 | 声称 | 实测 |
  |---|---|---|
  | `cpu_profile.schema.json:459`、`:691` | `hardware_inspect.cpp:255-261` 只写 windows/linux | 实为 `hardware_inspect.cpp:259-265`（:258 注释、:259 `nlohmann::json os`、:261 "windows"）——同 claim 在 `CONFIG_CONTRACT.md:137` 写的就是 :259-265（对） |
  | `cpu_profile.schema.json:459`、`:691` | `profile_gen_v2.cpp:568` 缺 os 回落 linux | `os_abi` 在 `profile_gen_v2.cpp:571`；:568 是 `{"family", ...}`——CONFIG_CONTRACT:137 写 :571（对） |
  | `cpu_profile.schema.json:471` | 「v2 生产写 std::to_string(xcr0)（profile_gen_v2.cpp:570）」 | 实为 `profile_gen_v2.cpp:573`；:570 是 `{"stepping", ...}` |
- **反核**：仅描述文本漂移，**约束本体正确**（host/os_abi enum/xcr0 pattern 与生产者一致，负例 `cpu_profile_v2_bad_os_abi.json` 存在）；影响是审计者按 schema 描述开锚会开错行、且两份权威给同一事实两组行号。
- **建议改法**：三处描述锚改到 :259-265 / :571 / :573（与 CONFIG_CONTRACT:137 对齐），并在 CFG002 锚登记（config_registry 的 anchor 条目）同步。

---

#### S33-Y6【黄】acr＋benchmark 源码注释中 **42+ 处**引用已随治理清理删除的规格/计划文档，权威链断锚（系统性）

- **所属面**：④
- **证据（实测计数与代表锚，全部实开）**
  - `grep -rn '_SPEC\.md|_TASKS\.md|号规范' lib/infrastructure/acr` = **42 命中**；`find . -name '*RESOURCE_CONTROL_SPEC*' -o -name '*QUALIFICATION_BENCHMARK_SPEC*' -o -name '*STATIC_ROUTING*' -o -name '*P0_REMEDIATION*'` = **0**（这些文件在全仓不存在）。
  - 代表锚：`acr/profile/profile_reader.hpp:4`（06_QUALIFICATION_BENCHMARK_SPEC §15 + 07_STATIC_ROUTING §9）；`acr/routing/route_profile_v2.hpp:6`（03/04 号规范）、`:145/:152/:188/:226`（BDR Reviewed 08 计划 A/B/C/H）；`acr/routing/benchmark_route_estimator.cpp:340/:546`（08 计划 E/F）；`acr/qualification/profile_schema.hpp:4`（04_QUALIFICATION_SPEC / 06_STATIC_ROUTING_SPEC）；`acr/tests/classic/classic_common.hpp:2`（09_PHASE_H_CLASSIC_EXPERIMENTS_SPEC）。
  - benchmark 侧同病：`backend_host/profile_store.h:2`（V7.1 04_CPU_RESOURCE_TASKS.md CPU-007）、`:7`（V8.1 03_P0_REMEDIATION_TASKS V8-CPU-002）、`cpu_routing.h:2`（15_CPU_PROVIDER_AND_RESOURCE_STANDARD §2/§3）、`bench_report.h:1`（V7.1 §"benchmark cpu 命令"）。
- **反核**：README:93 承认历史任务文档已清理——即"文档说有、文件没有"是**已知事实**，但 42+ 处注释仍以现行语气引用它们，阅读者无法沿注释核到任何条款；这与 S10 登记的 THREADING_MODEL 死锚同类、但**不同件不同锚**（S10: 红R7 是 THREADING_MODEL:124）。
- **建议改法**：不逐条改注释，改为在 `lib/infrastructure/acr/README.md` 与 `lib/infrastructure/benchmark/backend_host/` 各加一段「注释内 03–17 号规范/V7.1/V8.1 均已清理，语义现行权威 = docs/science|algorithms ACR_EQUIVALENCE + ENGINEERING_SPEC + 本文件头注」的总注；对含**冻结语义**的引用（route_profile_v2.hpp:6「冻结语义（03/04 号规范）」、profile_store.h CPU-007 验收句）逐条改指现行权威，避免"冻结条款查无出处"。

---

#### S33-Y7【黄】`bench_report` 在产物里自报 `selection_rules`（avx512/worker 收益门 0.03），但本层聚合实现既不施加这两条、也不声明自己实际使用的平手/离散度规则——同一次运行的 report 与 profile 可能给出不同 provider

- **所属面**：②（并触及 ① 的"判据自述与实现一致性"），连带 ④
- **文件:行**
  - 声明：`bench_report.cpp:219-232`，`measurement.selection_rules = {oracle_gate, avx512_min_gain_rel:0.03, worker_min_gain_rel:0.03, heavy_min_workers:2}`。
  - 实现：`aggregate_benchmark_kernels` `bench_report.cpp:97-157`——候选只按 `oracle_pass` 过滤（:111），胜者 = min median（:132-133），`rel<1e-2` 平手走保守 provider/少 worker（:138-146）；离散度仅记录 `dispersion_ok`（:152-153），**只进阈值门**（`:164-176`，与 `bench_report.h:39-42` 表述一致）。
  - 0.03 门真实所在：`profile_gen_v2.cpp:496-514`（avx512 相对 avx2 收益 <3% → 退 avx2，引擎层）、`:515-537`（worker 收益 <3% → 保持 1；heavy 强制 ≥2）。
- **证据（反核链）**
  1. 本层 `aggregate` 全函数无 0.03、无 avx512-vs-avx2 比较、无 worker 收益比较 ⇒ 声明中两条 0.03 规则**不适用于产生本报告胜者的这一层**；
  2. 本层实际规则（best=min median、tie<1e-2→provider_rank、dispersion 进 thresholds、oracle-only）**未出现在 selection_rules**；
  3. `verify_benchmark_report`（:347-355）校验 warmup/samples/outlier_policy，**不校验 selection_rules**——篡改该块仍 verify PASS；
  4. 结果性反核（代码路径级，未运行）：若某 kernel avx512 同 workers 下只快 1%，引擎 `profile_gen_v2` 按 :508 把 profile 的 provider 降为 avx2，而同一 bundle 的 `aggregate`（:132）按 min median 仍记 avx512 ⇒ 同一次运行的 `cpu-profile/v2` 与 `benchmark-report/v1` 可对同一 (kernel,size) 报不同 provider，且 report 自称施加了 0.03 门。
- **建议改法**：selection_rules 拆成 `engine_rules`（0.03 两条，注明由 profile_gen_v2.cpp:508/:522 施加）与 `report_aggregation_rules`（oracle-only、best=min median、tie<1e-2→conservative rank、mad_median_max=0.35 只进 thresholds），并把两组都纳入 `verify_benchmark_report` 必查；或让 aggregate 直接复用引擎同一 winner 函数以消除双实现。

---

### 绿（5 条，低危/备查级）

---

#### S33-G1【绿】CONFIG_CONTRACT 身份校验锚 `cpu_routing.cpp:216-300` 漂移（实为 :219-310），尾段 benchmark_binary_sha256 校验落在锚外

- **所属面**：④。证据：`docs/contracts/CONFIG_CONTRACT.md:133` 声称 `check_profile_identity_v1 (cpu_routing.cpp:216-300)`；实测函数定义在 `cpu_routing.cpp:219`、结束于 `:310`（下一函数 `profile_kernel_benchmark_valid` 在 :312），且 `CONFIG_CONTRACT.md:229` 自己引用的 `cpu_routing.cpp:271-275` 在新锚内、在旧锚外。反核：函数体与文档列举的绑定字段逐项一致（vendor/family/model/stepping/xcr0/os_abi/features/benchmark_binary_sha256 全查），**仅行号漂移，无语义缺口**。
- **建议**：锚改 :219-310。

#### S33-G2【绿】RUN_GRAPH_CONTRACT §3 metrics 清单漏 `artifact_publish_total`，§5 schema 有

- **所属面**：②。证据：`docs/architecture/observability/RUN_GRAPH_CONTRACT.md:50`（§3 列 node_count/module_call_total/scheduler_concurrency_max/…）不含 `artifact_publish_total`，而 `:82`（§5 schema）`"metrics": {"node_count","module_call_total","artifact_publish_total",…}` 含。反核：字段定义与生产者一致，仅 §3 枚举漏项；RESOURCE/STRUCTURED 两合同归 S10/S12，不重复。
- **建议**：§3 清单补 `artifact_publish_total`。

#### S33-G3【绿】`device_executor.cpp` 注释宏名前后不一致（ACR_CUDA_BRIDGE vs ACR_WITH_BRIDGE_LOADER）

- **所属面**：②。证据：`acr/scheduler/device_executor.cpp:18` 注释写「宏 `ACR_CUDA_BRIDGE`」，`:25` 实际条件编译宏是 `ACR_WITH_BRIDGE_LOADER`。反核：以 `:25` 实现为准（未定义→weak no-op，休眠默认安全），仅注释错名。
- **建议**：注释改 `ACR_WITH_BRIDGE_LOADER`。

#### S33-G4【绿】dispatcher worker 槽位上限字面量 16——反核后**不构成**铁律违反，登记备查

- **所属面**：①（铁律扫描项）。证据：`acr/scheduler/dispatcher.cpp:471-476` `worker_slots = min(max_possible_blocks, max(1, min(16, hardware_concurrency())))`。
- **反核（为何不是红）**：① ACR 属 DORMANT（THREADING_MODEL.md:12-14 删除线声明休眠，根构建不含 acr——见"已查无问题面③"），不处于生产路径；② 这是"上限钳制"而非固定线程数（随 hardware_concurrency 与块数收缩），且 `:471-472` 给出工程理由（避免按最大块数创建空转 worker）；③ 生产侧同位对照（`worker_advisor.cpp:200-245`）完全 profile 驱动。
- **建议**：若未来 ACR 转生产，把 16 提为 profile/config 键；当前仅登记。

#### S33-G5【绿】BENCHMARK_STANDARD「输出到 run/ 或 reports/」与最高设计「profile 落安装目录」字面冲突（建议补例外句）

- **所属面**：③。证据：`docs/standards/BENCHMARK_STANDARD.md:6`「输出到 run/ 或 reports/」；`docs/ASTROCS_DESIGN.md:518`「benchmark 输出 profile 到**安装目录**」；实现 `commands.cpp:2386` 落 install_dir。反核：按五方权威链，DESIGN 更高且对此工件明文指定，实现无错；冲突在标准文件缺例外。另核对同文件其余三条：Release-only 由 CI 结构建面承担（`eng/ci/checks.json:39` BUILD-GCC-RELEASE、:164 WIN-BUILD-RELEASE；切片内无 NDEBUG/CMAKE_BUILD_TYPE 断言）——已足够，不算缺口；variance 由 7 样本+MAD+p05/p95 承担（bench_report.h:36-42）；fast/reference 成对由 oracle 双路（bench_harness.h:27 独立 scalar 参考）承担。
- **建议**：BENCHMARK_STANDARD 第 3 条补「cpu_profile.json 例外：按 DESIGN §7.1 落安装目录」。

---

## 二、已查无问题面（按 ①②③④ 四面分别交代覆盖）

### ① 科学性（vs D-01…D-11、非退化负例）
- **覆盖**：D 系 11 条裁决全部为实验/科学量（外半径、k_corr、idw、depth 等），与 acr/benchmark 代码域无交集；本切片全文未出现这些量的任何实现或常数（grep 科学关键词 + 逐文件过读）。
- **查无问题的实测点**：测量链冻结常数自洽（warmup=3/samples=7/MAD≤0.35：`bench_report.cpp:219-231`、`:347-355`、`:164-176` 三处同值）；0.03 门在引擎层真实施加（profile_gen_v2.cpp:508/:522）；**非退化负例实存**——`verify_benchmark_report` 对"无合格候选"强制 baseline+fallback_reason+workers=0+median=0（:384-394，可红）、对 fallback 缺原因/伪造测量值逐条拒（:389-394）；`verify_profile_v2` 必填/类型缺失即拒（profile_gen_v2.cpp:648-710）；`worker_advisor` heavy 拒 worker=1 有 floor2 与 chain 留痕（:219-241）。未发现恒真判据。**登记类问题（科学公式/容差/冻结定义）**：0 条。
- **边界**：0.35 MAD 门只在 report 阈值层、不进 profile 生成——未见任何文档把它声称为 profile 门槛，故不判冲突（bench_report.h:39 自述与实现一致）。

### ② 行文逻辑（断链、文内自相矛盾、订正新旧、UNRESOLVED 当结论）
- **覆盖文件**：acr README、check_acr_dormant/ACR_DORMANT_GUARD、acr 各模块头注（api/backends/core/cost/diagnostics/include/profile/routing/scheduler/topology/utilization 全部过读）、benchmark backend_host 全部头注与 bench_report/profile_gen/profile_gen_v2/profile_store/cpu_routing/host_services/worker_advisor/bench_harness、cpu/ 三 provider 头注。
- **本面问题**：Y1（README 内新旧矛盾）、Y3（注释/提示与命令面矛盾）、Y4（CONFIG_CONTRACT 现时态 vs 已闭合）、Y7（自述规则 vs 实现）、G2、G3。
- **查无问题**：未发现"以 UNRESOLVED/待定当结论"的段落；profile_store/bench_report/cpu_routing 的层次合同（不复制校验链）与其代码真实依赖一致（均单点复用 verify_profile_v2/check_profile_identity_v1）。

### ③ 跨文档冲突（五方权威链 docs/ASTROCS_DESIGN → docs/contracts|architecture → ENGINEERING_SPEC → 实现/登记册）
- **逐对核过的链**：docs/ASTROCS_DESIGN:511/:518 ↔ CLI_PROTOCOL_V1:31 ↔ command_tree.h:110 ↔ commands.cpp:2343-2397（→ R2、Y3）；runtime_resources.json:18-19 ↔ schema ↔ profile_gen_v2（→ R1）；CONFIG_CONTRACT:133/:137/:138/:193 ↔ schema 描述 ↔ 登记册（→ Y4、Y5、G1）；BENCHMARK_STANDARD ↔ DESIGN §7.1（→ G5）；ARCHITECTURE_OVERVIEW:54 ↔ ACR_DORMANT_GUARD:10 ↔ check_acr_dormant docstring ↔ 根 CMakeLists:633（→ Y2）。
- **复用的邻片权威裁决（不重开）**：RESOURCE_MONITORING §3.0（S10 红R5）、STRUCTURED_LOGGING kind 枚举（S10 黄Y3）、ISA_VARIANTS:53/:69（S10）、THREADING_MODEL:124 死锚（S10 红R7）、EXECUTION_MODEL（S10 黄Y4）、CONFIG_CONTRACT 计数/UT-CONFIG（S8 黄-7）、docs/api MANIFEST_VERIFY cpu-profile 合同（S12 红-3）、resource_recorder.h 锚漂移（S12 黄-2）。本切片对此四类只做引用不重复登记。
- **D 系与 PASS 表**：见报告头部"前置排除"；无一处本切片发现与之冲突。

### ④ 幻觉与锚（file:line 逐条实开；"说有没接"、死配置键、schema 漂移；文献 DOI/arXiv）
- **锚核验量**：本报告 14 条问题共引用 **50+ 处** file:line 锚，全部逐一 `read/sed` 实开，无一处凭印象；其中 7 处是"开完发现漂移"的反例锚（Y5×3、G1、Y1×2、Y4）。开锚过程中同步核对的"对锚"（CONFIG_CONTRACT:137 两锚、:229、schema:25、ARCHITECTURE_OVERVIEW:54、CLI_PROTOCOL:8/:20/:31、docs/ASTROCS_DESIGN:511/:518、checks.json:39/:164、isa_sites.json product-avx2/avx512）均实开。
- **"说有没接"专项**：R2（自动读取 + 4 层零调用方，逐层 grep 反核）、R1（死键）、Y3（命令面/报告层）、Y2（守卫未实现判据）、Y6（42+ 死规格锚）。
- **文献 DOI/arXiv**：对 `lib/infrastructure/acr` 与 `lib/infrastructure/benchmark` 全部 .cpp/.h/.hpp/.md 做 `doi|arxiv|et al.|10\.` 引用样式扫描 = **0 条**命中（唯一近似命中是 bench_harness.h:27 的 "expected_ref" 术语）⇒ 本切片无外部题录需要 web 核验；方法在此声明以备复核。
- **第三方边界**：nanoflann/cfitsio/nlohmann 只做接口级观察，未评其实现（cfitsio -msse2 站点已由 isa_sites.json third-party-cfitsio-sse2 登记，status=DORMANT，合规）。

### 附：域外附带观察（不计入本切片计数，供 cli 切片参考）
- `lib/infrastructure/cli/parser.cpp:700` 同样给出 `rerun 'acsd benchmark cpu'`（不存在的命令），`:134-135` 注释声称 "benchmark cpu → unknown command" 而实现报 "unexpected positional argument"；`commands.cpp:2344` 注释"运行时自动读取"与实现不符；`cli_common.h:26` "v2 profile 无独立 schema 文档"已被 `cpu_profile.schema.json` 推翻。三处均为 cli 域文件。

---

## 三、汇总

| 严重级 | 条数 | 编号 |
|---|---|---|
| 红 | **2** | S33-R1、S33-R2 |
| 黄 | **7** | S33-Y1…Y7 |
| 绿 | **5** | S33-G1…G5 |
| 合计 | **14** | |
