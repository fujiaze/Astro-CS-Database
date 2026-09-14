# P-G03 Stage1 HISS 交付包与 Agent 任务包 —— 组取证台账

> 取证范围：G03（Stage1 HISS 交付包与 Agent 包）4 个登记身份。方法：`设计大纲/_evidence/packs/dispatch_groups.json` → 4 份 digest → 包本体（`设计大纲/_evidence/packs/history/a78f5430/…`、`设计大纲/_evidence/packs/history_zip/…`）→ 与 `pack_lineage_table.md` / `pack_commit_link.csv` / `pack_inventory.csv` / `pack_instances.jsonl` 及 git 历史交叉核对。zip 仅只读列举（本环境无 `unzip`，改用 `python3 zipfile` 读取，未解包入库）。全程未修改既有文件、未执行任何改变 git 状态的命令。
> 时间锚点（`git rev-parse` / `git log` 实测）：加入 `78a9121d`（2026-07-31T12:55:14+08:00）；删除 `a78f5430`（2026-07-31T18:29:02+08:00，338 条删除路径）；删除父提交 `59e9e39e`；包自述审计基准 `183558ad`（`…/AstroCS_Stage1_HISS_Delivery/00_README.md` L5，2026-07-30T21:48:40+08:00）。

## 一 组内包清单与身份归并

| # | 身份（台账键） | 实例数与存形态 | 包本体相对路径 | 文件 / 字节 | 首次出现提交 | 最后出现提交 | 是否被删除 | zip sha256 |
|---|---|---|---|---|---|---|---|---|
| 1 | `AstroCS_Stage1_HISS_Delivery` | 1；仅历史复原（无 zip、无 `工程控制/` 解包件、无归档、无 `run/` 影子树） | `设计大纲/_evidence/packs/history/a78f5430/AstroCS_Stage1_HISS_Delivery/` | 51（50 包内文件 + `_PROVENANCE.json`）/ 686461 B | digest 与 `pack_inventory.csv` L44 记 `a78f5430`；git 实测加入 `78a9121d` | 记 `78a9121d`；git 实测末次为 `a78f5430`（删除） | 是，`a78f5430`（该目录下 50 条路径全在删除清单） | 无 zip |
| 2 | `AstroCS_Stage1_HISS_Delivery_2026-07-31` | 1；zip（历史复原） | `设计大纲/_evidence/packs/history_zip/AstroCS_Stage1_HISS_Delivery_2026-07-31.zip` | 50 条目 / 盘上 230987 B（解压 689806 B） | digest `first=None`；git 实测 blob `206d97a3f9e2599d…` 加入于 `78a9121d` | digest 仅 `touch_last=a78f5430`；git 实测 `a78f5430` 树无此路径、`59e9e39e` 树有 | 是，`a78f5430` | `2eceeff476b54ecfd653b37e42e487c1be0f7b008b240eb48ff46d0de3b3e883`（`sha256sum` 与 `git show 78a9121d:…zip` 双向实测相同） |
| 3 | `AstroCS_Stage1_HISS_Agent_Package_2026-07-31` | 1；仅历史复原 | `设计大纲/_evidence/packs/history/a78f5430/_agent_package/AstroCS_Stage1_HISS_Agent_Package_2026-07-31/` | 13（12 + `_PROVENANCE.json`）/ 30645 B | digest `adds_n=1` 指向 `a78f5430`；git 实测 `78a9121d` | git 实测 `a78f5430` | 是，`a78f5430`（12 条 `_agent_package/` 前缀删除） | 无 zip（全库从未存在该名的 zip，见 §八 缺口 12） |
| 4 | `_agent_package` | 1；仅历史复原（#3 的容器目录） | `设计大纲/_evidence/packs/history/a78f5430/_agent_package/` | 14（12 内容 + 2 个 `_PROVENANCE.json`）/ 31227 B；digest 记 13 / 30800 B | 同上（`adds_n=6`） | 同上（`dels` 6 条同一 SHA） | 是，`a78f5430` | 无 zip |

归并判定与证据：
1. **#1 与 #2 是同一交付物的两种存形态**：zip 内 50 个条目路径与复现目录 50 个内容文件路径做双向差集，两侧均无独有路径；10 个文件字节不同，逐文件比较显示差异全部为行尾（zip CRLF 对 目录 LF），按 LF 归一后 10/10 相同。仓库根 `.gitattributes` @`78a9121d` L2 为 `* text=auto eol=lf`。
2. **#3 与 #4 是同一物理包**：`_agent_package/` 下唯一子目录即 `AstroCS_Stage1_HISS_Agent_Package_2026-07-31/`；`…/AstroCS_Stage1_HISS_Agent_Package_2026-07-31/_PROVENANCE.json` 记 `pack_rel_path: _agent_package`、`recovered_files: 12`。两 digest 字节差 155 B 仅由容器层 `_PROVENANCE.json`（582 B）与包层 `_PROVENANCE.json`（427 B）的计入方式不同造成。
3. **同一 zip 在实例账中被记两次**：`pack_instances.jsonl` 内两条记录同为 50 文件 / 689806 B / 同一 sha256，身份分别为 `packs/history_zip`（form `zip`）与 `AstroCS_Stage1_HISS_Delivery_2026-07-31.zip`（form `zip_recovered_from_history`）。
4. **现存形态**：`工程控制/`、`engineering/control/archive/`（含 `2026-09-02_legacy_工程控制_v1.3-to-v6.1/`）、`run/` 三处按名检索四个身份目录均 0 命中（`find`/`git ls-files`），本组只存在于 `设计大纲/_evidence/` 复现层。
5. **包内清单自校验在两形态下结论相反**：`…/AstroCS_Stage1_HISS_Delivery/MANIFEST.sha256`（1 行）声明 `MANIFEST.json` 的哈希为 `d4678fd3ad705e14610666caea2d8d7f6c404daac9e23f1b6299674fb26d8953`。zip 侧实测 48/48 条 sha256 相符、清单自身哈希相符；复现目录侧 `MANIFEST.json` 实测哈希 `63a91a2ceb664f90…`（与清单声明不符），48 条中 9 条与目录内容不符（9 = 10 个行尾差异文件去掉 `MANIFEST.json` 自身）。即解包形态的包内自校验链已断，zip 形态自洽。
6. **规模自述与实测对齐**：`MANIFEST.json` 48 条 `size` 合计 681911 B = 665.9 KiB，与 `memory.md` @`78a9121d` L1451 记的「48 文件, 665.9 KB」和 zip 盘上 230987 B = 225.6 KiB 一致；分组字节 root 38498 / `changed_files` 319612 / `reports` 141245 / `tests` 145677 / `wiki` 36879。复现目录实测 51 文件 / 686461 B 的差异由 `MANIFEST.sha256`、`_PROVENANCE.json` 计入与 LF 尺寸差造成。

## 二 包内文件地图

### 交付包 `AstroCS_Stage1_HISS_Delivery`（50 内容文件；`MANIFEST.json` 48 条 = 50 减 `MANIFEST.json` 与 `MANIFEST.sha256`，实测无缺项无多项）

| 顶层分区 | 文件数 | 清单字节 | 内容与职责（指针） |
|---|---|---|---|
| 根 | 6 | 38498 | `00_README.md`（141 行；L1 标题、L3 交付日期、L4 上游任务包名、L5 审计基准 commit、§1 交付内容、§3 已知问题、§5 明确未运行、§6 交付结构、§7 声明）；`APPLY_IN_PLACE.md`（156 行；§1 基准表含远端 `https://github.com/fujiaze/Astro-CS-Database.git` 与 `main`、§2 方式 A patch / 方式 B 复制、§3 删除、§4 构建、§5 测试、§8 验证清单 7 项）；`DELETE_LIST.txt`（23 行）；`MANIFEST.json`；`MANIFEST.sha256`；`git_diff.patch`（9 个 diff 头） |
| `changed_files/` | 15 | 319612 | 保持仓库相对路径的源码副本：`lib/astro_image_io/`（`include/hiss_format.h`、`src/hiss_codec.cpp`、`src/hiss_common.cpp`、`src/hiss_reader.cpp`、`src/hiss_writer.cpp`、`build.ps1`、`Makefile`、`memory.md`）、`lib/calibration/`（`include/astro_calibration.h`、`src/dark_optimizer.cpp`、`build.ps1`）、`lib/healpix_db/healpix_drizzle/`（`drizzle_engine.cpp`、`drizzle_engine.h`、`memory.md`）+ 根 `memory.md`。以基准 `183558ad` 判定：新增 6（`hiss_format.h`、4 个 `hiss_*.cpp`、`dark_optimizer.cpp`）、修改 9 |
| `wiki/` | 16 | 36879 | 10 页无废止标头（`Stage1-Scope-and-Pipeline.md`、`Stage1-Calibration.md`、`Stage1-Photometry-and-SNR.md`、`Stage1-HEALPix-Drizzle.md`、`HISS-Container-and-Tiles.md`、`HISS-Metadata.md`、`Stage1-Decision-Status.md`、`Stage1-Agent-Execution.md`、`Home.md`、`_Sidebar.md`，即 8 标准页 + 2 导航页）+ 6 页首行含 `SUPERSEDED — DO NOT IMPLEMENT`（`HISS-格式规范.md`、`Stage1-待确认事项.md`、`Stage1-范围与架构.md`、`Stage1-开发治理.md`、`Stage1-校准规范.md`、`Stage1-Drizzle规范.md`） |
| `tests/` | 4 | 145677 | `hiss_benchmark.cpp`（L1502-1511 以 `generate_dataset` 造 `large_full`/`center_80`/`edge_30`/`sparse_5`，随机源 `std::mt19937`）、`hiss_correctness_test.cpp`、`hiss_writer_smoke.cpp`、`test_output.txt`（300 行；L293-298 汇总块） |
| `reports/` | 9 | 141245 | `repository_audit.md`（162 行，Phase 0；L4 审计主体、L12 仓库根、L55-56 Wiki 现状、L90 冲突关键词命中 2 文件、L130 计划替换项）、`implementation_summary.md`（211 行，§1-§7）、`correctness_report.md`（305 行，TEST 01-21）、`performance_profile.md`（159 行）、`decision_queue.md`（108 行，DQ-001…007 + L96 状态汇总 + L108 声明）、`experiments/`（`summary.md` 239 行 L20/52/75/106/149/174/198 为七节、L222 总结；`environment.md` 42 行；`raw_results.csv` 121 行；`raw_results.json`） |

缺失分区（客观登记，均实测为 0）：本包**无** `tasks/`、无 `checklists/`、无 `contracts/`、无 `templates/`、无 `schemas/`、无 `scripts/`、无 `tools/`、无 `agent/`、无 `evidence/`、无任何台账 CSV；`pack_instances.jsonl` 的 `tasks_files`、`scripts`、`schemas`、`templates` 四字段对本组两包实例均为空数组。入口命名口径为 `00_README.md`，非 `00_READ_FIRST.md`。

### Agent 任务包 `AstroCS_Stage1_HISS_Agent_Package_2026-07-31`（12 文件，全部为规格与入口，无代码）

| 文件 | 行数 | 结构（`##` 节数 / 锚点行） |
|---|---|---|
| `00_AGENT_START.txt` | 1 | 机器入口提示，单行 |
| `START_HERE.md` | 49 | 5 节：§任务性质 L3、§单次执行原则 L7-17、§绝对禁止 L19-27（7 条）、§必须按顺序执行 L29-37（7 步）、§首次输出要求 L39-49（5 项） |
| `01_EXECUTION_ORDER.md` | 80 | 7 节 Phase：L3 审计、L14 冻结文档、L35 接口契约、L55 实现、L61 C++ 实验、L72 测试与性能、L78 精简交付 |
| `02_FROZEN_STAGE1_HISS_SPEC.md` | 288 | 18 节：L5 范围、L33 校准模式、L72 Flat、L84 PlateSolve、L94 自动 NSIDE、L107 ordering、L114 Gaia signal、L132 累计通量、L147 pixfrac、L162 support、L176 自适应 Tile、L194 占用编码、L204 独立子块、L216 HISS 容器、L232 目录必需字段、L248 元数据、L277 SNR 控制点、L286 当前不讨论 Stage2 |
| `03_WIKI_UPDATE_REQUIREMENTS.md` | 79 | 4 节：§目标、§必须新增或重写的页面（8 页，L9-57）、§旧内容处理 L59-64（4 条）、§Wiki 自检 L66-79（10 个冲突词） |
| `04_IMPLEMENTATION_REQUIREMENTS.md` | 89 | 7 节（总体要求 4 条、统一接口 9 条、Stage1 修改含校准 4 / PlateSolve 2 / Gaia 2 / Drizzle 6、Writer 8 条、Reader 5 条、Browser 7 条、未决参数 3 条） |
| `05_CPP_EXPERIMENT_PLAN.md` | 129 | 6 节：§原则、§1 待实验事项（1.1 codec 17 条 / 1.2 阈值 4 / 1.3 checksum 3 / 1.4 对齐 3）、§2 数据集覆盖 7 条、§3 测量指标 12 条、§4 输出（4 文件 + 源码 + 命令）、§5 决策边界 L119-129 |
| `06_TEST_AND_ACCEPTANCE.md` | 88 | 7 节必测清单（13 / 11 / 5 / 5 / 13 / 3 / 4 条） |
| `07_FINAL_DELIVERY_STRUCTURE.md` | 95 | 4 节：§总原则、§必须内容（`00_README` 4 项、`APPLY_IN_PLACE` 6 项、`decision_queue` 5 项）、§禁止打包 12 项、§ZIP体积控制 |
| `08_DECISION_QUEUE_TEMPLATE.md` | 58 | 7 节 DQ（L5/15/22/29/36/44/52），每项 7 字段，状态行 L13/20/27/34/42/50/58 |
| `09_DELIVERY_REVIEW_CHECKLIST.md` | 50 | 6 组 31 个 `- [ ]`（范围 4 / Wiki 3 / Stage1 8 / HISS 9 / 实验 4 / 交付 3） |
| `MANIFEST.json` | — | 11 条（不含自身与 `_PROVENANCE.json`），11/11 sha256 与复现目录实测相符；字节合计 28570 B |

命名口径：`NN_UPPER_SNAKE.md` 顺序号 01-09 + 无编号 `START_HERE.md` + `00_AGENT_START.txt`；**无** `00_READ_FIRST.md`、无 `OWNER_BINDINGS.md`、无 `templates/`、无 `schemas/`、无 `tasks/`。

## 三 任务结构

1. **无任务台账、无任务号**：两包均无 `TASK_LEDGER.csv` / `MASTER_TASK_REGISTER.csv`；`pack_lineage_table.md` 行 13/14/16/52 的台账列全为 0；`pack_commit_link.csv`（共 16 行数据）中无 G03 任何身份 → 本组不存在「任务号 ↔ 提交消息」可对接单元。
2. 实际编号族 4 类（均非台账）：
   - **Phase 0-6**：`…/01_EXECUTION_ORDER.md` 七个 `## Phase N`（L3/14/35/55/61/72/78）。交付侧以 `00_README.md` §1.1-§1.5 按 Phase 回填。
   - **DQ-001…DQ-007**：模板 `08_DECISION_QUEUE_TEMPLATE.md` L5/15/22/29/36/44/52；交付侧 `reports/decision_queue.md` 同名 7 节 + L96 汇总表 L100-106；`reports/experiments/summary.md` L20/52/75/106/149/174/198 逐号一节。
   - **文件顺序号 00-09**：任务包 12 文件本身即结构。
   - **TEST 01…21**：仅存在于交付侧 `reports/correctness_report.md`，L18-22 分类表记 01-05 校准 / 06-11 Drizzle / 12-21 HISS 格式。
3. **编号承载于 CSV 留痕**：`reports/experiments/raw_results.csv` 表头含 `dq_id,dataset,candidate,…,verified`，120 数据行按 `dq_id` 分布 20/12/20/32/12/12/12，`dataset` 仅 4 个合成集名（各 30 行），`verified` 列 120/120 为 `true`。
4. **依赖与顺序的表达形式为自然语言条件句，无机器字段**：`START_HERE.md` L29-37（7 步顺序）、`01_EXECUTION_ORDER.md` L33（文档未同步前不得沿用旧接口写实现）、L46-53（接口冻结后方可并行）、`05_CPP_EXPERIMENT_PLAN.md` L5（未冻结事项只能以 C++ 实验产出建议）、`06_TEST_AND_ACCEPTANCE.md` L73-77（测试规模禁止项）。
5. **状态字面量集合（实测计数）**：模板态 `状态：等待确认` x7（`08` L13/20/27/34/42/50/58）；交付态 `状态: 实验完成, 待用户冻结` x7 与汇总表 `待冻结` x7（`decision_queue.md` L100-106）；测试态 `通过` x19 + `通过 (含已知问题记录)` x1（`correctness_report.md` L115）+ `通过 (行为记录)` x1（L138）；清单态 `- [ ]` x31（`09`，交付包内无已回填版本）。上一代字面量 `TODO`/`READY`/`DONE`/`PENDING_USER_DECISION` 与本代治理字面量 `NOT_STARTED`/`IN_PROGRESS`/`PASS`/`FAIL`/`BLOCKED`/`REVIEW_PENDING` 在 G03 两包内 0 命中。
6. **无状态机定义文件**：`06_TEST_AND_ACCEPTANCE.md` §7（L79-88）仅给 4 条验收约束（L88 禁止 Agent 自行宣称用户验收完成），无状态迁移定义、无 schema。

## 四 门禁与验收要求

前提事实：G03 两包内**不含任何机器门禁实现**（无 `tools/validate_pack.py`、无 `ci/`、无 `checklists/GATE_*`、无脚本，见 §二 缺失分区）；门禁全部为文本条款 + 两份清单文件。

Agent 任务包侧（要求源）：
1. `06_TEST_AND_ACCEPTANCE.md` 七节必测项：§1 科学正确性 13 条、§2 HISS 格式 11 条、§3 CLI 5 条、§4 Browser 5 条、§5 性能剖析 13 条、§6 测试规模 3 条（含禁跑 710 帧全量回归）、§7 验收状态 4 条（L88）。
2. `07_FINAL_DELIVERY_STRUCTURE.md`：交付结构模板（L9-34 目录树，与交付包 `00_README.md` §6 L110-127 的树逐行对应）、「必须内容」分项（`00_README` 4 项 / `APPLY_IN_PLACE` 6 项 / `reports/decision_queue.md` 5 项）、「禁止打包」12 项（L78-91，含 `.git/`、构建产物、大数据、全量回归产物）、「ZIP体积控制」（L93-95）。
3. `09_DELIVERY_REVIEW_CHECKLIST.md`：6 组 31 复选项（范围 4 / Wiki 3 / Stage1 8 / HISS 9 / 实验 4 / 交付 3），交付前自检门。
4. `03_WIKI_UPDATE_REQUIREMENTS.md`：8 个必新增或重写页面（L9-57）；旧内容处理 4 条（L59-64，要求加废止标头并删除「每阶段等待用户确认」类规则）；提交前冲突词自检 10 项（L66-79）。
5. `05_CPP_EXPERIMENT_PLAN.md`：§4（L108-117）强制交付 `reports/experiments/` 四文件（交付侧实测四件齐备）；§5（L119-129）要求报告可写三类结论但必须同时附一句未冻结声明，并写「等待用户与主审助手确认」。
6. `START_HERE.md`：§绝对禁止 7 条（L19-27：不得新建仓库、不得进入 Stage2、不得跑 710 帧全量回归、不得以 Python 替代 C++、不得把未冻结结论写成默认值、不得 HISS v1/v2 双路线、不得整仓交付）；§单次执行原则（L7-17）允许中断情形仅 3 种（科学定义冲突、权限或数据缺失、不可恢复失败）。

交付包侧（可机检项）：
7. 清单完整性门：`MANIFEST.json` 48 条 `path/size/sha256` + `MANIFEST.sha256` 单行；两形态校验结果分叉（zip 48/48 通过，复现目录 39/48 通过且 `MANIFEST.sha256` 失配），见 §一 判定 5。
8. 应用验证门：`APPLY_IN_PLACE.md` §2 方式 A 要求先 `git apply --check`（L24-26）；命令级超时与终止条件见 §4.2 L68（120 秒）、§4.3 L77（120 秒）、§4.4 L88（180 秒）、§5.1 L107（60 秒编译 + 30 秒运行）；§8 验证清单 7 项。
9. 删除门：`DELETE_LIST.txt` L1-23 审计结论为「暂无必须删除的文件」，另以 4 条理由给出后续建议删除项（`lib/astro_image_io/python/hiss_v2.py`、`hiss_v2_inspector.py`、`hiss_v2_visualizer.py`，三者 @`78a9121d` 均存在）。
10. 完成顺序门：Phase 0→6（`01_EXECUTION_ORDER.md`），交付侧按 Phase 回填。
11. 豁免（waiver）与放行：两包内 `waiver|豁免` grep **0 命中** → 本组未定义任何放行通道，也未定义机器门失败后的处理口径。
12. 决策权口径：`08_DECISION_QUEUE_TEMPLATE.md` L3（不得由 Agent 直接冻结）、`05_CPP_EXPERIMENT_PLAN.md` L129、`06_TEST_AND_ACCEPTANCE.md` L88、`reports/decision_queue.md` L108（需用户确认后才能写入冻结规范并更新两个 HISS Wiki 页）。

## 五 设计意图（转述）

以下均为包内文本转述，逐条给指针，不含本台账的判断。

1. **原地演进、只交差异**：任务性质为在现有仓库就地审计、删除、更新 Wiki、修改实现并交付差异文件（`START_HERE.md` L3-5 与 §绝对禁止 L21/L27）；交付侧实现为 `changed_files/` + `git_diff.patch` + `APPLY_IN_PLACE.md` 两方式（§2）。
2. **单次连续执行、取消人工检查点**：用户按 Agent 调用次数计费，要求一次连续完成 Phase 0-6，仅 3 种情形可中断（`START_HERE.md` L9-15）；`03_WIKI_UPDATE_REQUIREMENTS.md` L59-64 进一步要求从旧文档删除「每阶段等待用户确认」类规则。
3. **科学语义归用户，冻结权在「用户与主审助手」**：`02_FROZEN_STAGE1_HISS_SPEC.md` L3（本文件内容均已由用户确认，Agent 不得改写科学语义）、`05_CPP_EXPERIMENT_PLAN.md` L129、`08_DECISION_QUEUE_TEMPLATE.md` L3、`06_TEST_AND_ACCEPTANCE.md` L88。
4. **单格式路线**：禁 HISS v1/v2 双路线（`START_HERE.md` §绝对禁止），规范侧为「当前只维护一个 AstroCS 1.0 目标 HISS 格式」（`02` §14 L216 起）与「不使用 Footer、Checkpoint、断点续写」（同节 L219-224，见 `wiki/HISS-Container-and-Tiles.md` 对应实现）；旧 Python `hiss_v2` 路线仅作迁移参考（`reports/repository_audit.md` §3、`DELETE_LIST.txt` 建议项）。
5. **范围封闭**：`02` §1（L5）+ §18（L286）不讨论 Stage2；Wiki 侧同步为唯一权威知识库、唯一主线 Stage1、Stage2 停止开发（`wiki/Home.md` L1-11）。
6. **文档先行**：Phase 1 先冻结文档再写实现（`01` L14-33），Wiki 页名与规范节名一一对应（`03` §必须新增或重写的页面）。
7. **负责人裁决与用户指令痕迹的分布**：四个身份内**不存在** `OWNER_BINDINGS.md`、`04_OWNER_DECISIONS.md`、`07_FRONT_DESK_RULINGS.md` 或同类裁决文件（§二 清单）。裁决痕迹只有两种形式：(a) 包内措辞（`02` L3、`05` L129、`08` L3、`06` L88、`decision_queue.md` L108）；(b) 包外仓库侧留痕——`memory.md` @`78a9121d` L1435-1455（记任务包名、审计基准 commit、Phase 0-6、交付包规模、未决 7 项、已知问题 2 项）、`b4bcf8e7`（2026-07-31T15:17:53，正文记载依据用户审查反馈新增 Stage1/HISS 修复规范文档）、`a78f5430`（负责人整理根目录并删除本组四对象的提交，正文逐条列出去向）。是否另有负责人书面裁决：未证实。
8. **废止声明的分布**：组内唯一废止语义在 Wiki 层（`wiki/Home.md` L43-53 逐页取代映射 + `03` L59-64 标头要求）。**组内没有任何文件声明废止上一代控制包**：`engineering_authoritative`/`Authoritative`/`v2.0`/`GATE` 在 `_agent_package` 全目录 0 命中。

## 六 代际差异表

对照对象：上一组（G02）末包 `AstroCS_Stage1_Wiki_Freeze_2026-07-30`（`设计大纲/_evidence/packs/history/a78f5430/_wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/`，23 内容文件 + `_PROVENANCE.json`，53118 B）；更早一代 `engineering_authoritative`（177 文件）与 `_v2_pack/AstroCS_Authoritative_Development_Pack_v2.0`（75 文件），行数据取 `pack_lineage_table.md` 行 11-12 与 `pack_instances.jsonl`。

| 维度 | `engineering_authoritative` / `…_v2.0`（上一代） | `AstroCS_Stage1_Wiki_Freeze_2026-07-30`（上一组末包） | G03 Agent 任务包 | G03 交付包 |
|---|---|---|---|---|
| 文件数 | 177 / 75 | 23 | 12 | 50（复现目录 51） |
| 入口命名 | `00_READ_FIRST.md` | `README.md` | `00_AGENT_START.txt` + `START_HERE.md`（无 `00_READ_FIRST.md`） | `00_README.md` |
| 任务结构 | `tasks/` 30 个（号族 `A-001…I-003`）+ `docs/` 9（`00…08`） | 无 `tasks/`（README + `wiki/` 20 + `tools/Push-Wiki.ps1`） | 无 `tasks/`（Phase 0-6 散文式） | 无 `tasks/`（DQ / TEST 号只在报告内） |
| 台账 | `control/MASTER_TASK_REGISTER.csv` 30 行（v2.0：表头 `task_id,gate,title,dependencies,status`，29 TODO + 1 READY；authoritative：DONE 28 / PENDING_USER_DECISION 1 / TODO 1） | 无 | 无（台账行 0） | 无 |
| 门禁 | `checklists/` 9 个 `GATE_A…I` + `contracts/` 6 + `tools/validate_pack.py` | 仅推送脚本，无门 | `06` 七节必测项 + `09` 31 复选项 + `07` 结构门（无脚本） | `MANIFEST.json`/`MANIFEST.sha256` + `APPLY_IN_PLACE.md` §4/§5/§8（含超时数值） |
| 状态字面量 | `TODO` / `READY` / `DONE` / `PENDING_USER_DECISION` | 无状态列 | `状态：等待确认` x7 | `待冻结` x7、`实验完成, 待用户冻结` x7、`通过` x21（含 2 条限定态） |
| 完整性清单 | `MANIFEST.md` + `SHA256SUMS.txt` | `MANIFEST.json` 14 条（未覆盖树内 8 页：`HISS-Container-and-Tiles.md`、`HISS-Metadata.md`、`Stage1-Agent-Execution.md`、`Stage1-Calibration.md`、`Stage1-Decision-Status.md`、`Stage1-HEALPix-Drizzle.md`、`Stage1-Photometry-and-SNR.md`、`Stage1-Scope-and-Pipeline.md`） | `MANIFEST.json` 11 条（无 `.sha256`），11/11 通过 | `MANIFEST.json` 48 条 + `MANIFEST.sha256`（zip 通过 / 复现目录失配） |
| 交付物形态 | 文档 + 台账 + 校验器 | Wiki 快照 + 推送脚本 | 仅规格文档（无代码、无 patch） | 差异代码 + patch + 报告 + 测试输出（本组唯一含实现物的身份） |
| 执行模型 | 逐任务提交、逐门验收 | 人工推送 Wiki | 单次连续执行 Phase 0-6 | 交付 + 就地应用（方式 A / 方式 B） |
| 行尾 | git 侧 LF | git 侧 LF | 复现目录 LF | zip CRLF 对 目录 LF（10 文件差异；`.gitattributes`@`78a9121d` L2 要求 `* text=auto eol=lf`） |
| Stage2 / 710 口径 | 无此约束表述 | 无 | 绝对禁止（`START_HERE.md` L19-27）+ `02` §18（L286） | 明确未运行（`00_README.md` §5，含未跑 710 帧、未运行 Stage2、未用真实天文数据） |

代际结论（可核对部分）：G03 相对上一代把「机器门 + 台账 + 任务号」整体替换为「顺序 Phase + 人工复选清单 + 报告内编号」，清单载体由 `MANIFEST.md` + `SHA256SUMS.txt` 改为 `MANIFEST.json`，并首次在同一交付物上同时留下 zip 与解包目录两种形态——而两形态的行尾差异恰好使包内自校验只在 zip 侧成立（§一 判定 1 与 5）。

## 七 执行留痕三方核对

对接前提：本组台账任务号数为 0 → 无 `pack_commit_link.csv` 行，「任务号 ↔ 提交消息」命中率不可计算。下表以包内编号与包内断言为「账」，以仓库文件、`evidence/` 目录与提交消息为「据」。

| 编号 / 断言 | 包内出处 | 仓库与留痕侧出处 | 分类 |
|---|---|---|---|
| DQ-001…DQ-007 号族 | `08_DECISION_QUEUE_TEMPLATE.md` L5-52；`reports/decision_queue.md` L100-106；`reports/experiments/summary.md` L20-222；`raw_results.csv` 120 行 | 提交消息 `78a9121d`（Phase 4 记 DQ-001~DQ-007 全部完成、仅推荐不冻结）、`5c19ceeb`（WP-I-2 完成七组实验）、`8f505194`（逐号给出 DQ-001…007b 推荐）；全库 `git log --grep` 计数 DQ-001 x4、DQ-002~006 各 x2、DQ-007 x5 | 一致（7/7 命中，且三个提交均在同日 12:55-22:14 窗口） |
| `changed_files/` 15 件 = 已落地实现 | §二 清单 | `git show 78a9121d:同路径` 逐件 sha256：14 件与包内一致；根 `memory.md` 不一致（包 `e51364b95e78…` / 1645 行，仓库 `ae0566b46426…` / 1665 行，仓库多出约 20 行交付记录 @L1435-1455）；该包内版本在 59 个已提交 `memory.md` 版本中 0 命中 | 一致 14 / 差异 1（该 1 件为「有账无据」） |
| `git_diff.patch` 与基准 commit 耦合 | `APPLY_IN_PLACE.md` §2（L24-26）+ `00_README.md` L5 声明基准 `183558ad` | patch 的 9 个 diff 头 = `78a9121d` 的 9 个修改文件；LF 形态在 `183558ad` 工作区（临时目录，未动仓库）`git apply --check` 通过、`git apply` 后 9 件哈希与包内 `changed_files/` 对应件一致；zip 内 CRLF 形态 `git apply --check` 失败 | 一致（但仅解包形态可复现，zip 形态不可直接应用） |
| `tests/` 4 件与仓库测试 | `tests/` 清单 | @`78a9121d` 与包内同哈希（`hiss_correctness_test.cpp` `f52a18269371…`、`test_output.txt` `0b520cde1cb1…`）；@`59e9e39e` 起测试源码变为 `3139756e1bd7…`（HEAD `615e1838a812…`），而 `test_output.txt` 在 `78a9121d`/`59e9e39e`/`a78f5430`/HEAD 四代恒为 `0b520cde1cb1…` | 一致（但包内测试源码是 `59e9e39e` 修复前版本，测试输出未随之更新） |
| 「21/21 通过」断言 | `00_README.md` L34-36；`correctness_report.md` L11-17（总数 21 / 通过 21 / 失败 0 / 通过率 100% / 退出码 0） | 同包 `tests/test_output.txt` L293-298 汇总块实测为 `总计: 21 / 通过: 0 / 失败: 0 / 跳过: 0 / 实际通过: 21 / 21`（通过、失败两计数器为 0）；解释性修复记录在包外 `59e9e39e` 正文第 4 条（计数器从未递增） | 包内自相矛盾（账与包内据不一致，解释据留在包外） |
| 实验数据性质（真实 / 合成） | `00_README.md` L108（测试使用合成数据）、`decision_queue.md` L108、`experiments/summary.md` L224、`tests/hiss_benchmark.cpp` L1502-1511 | 包外 `5c19ceeb` 新增 `lib/astro_image_io/tests/results/dq001…dq007_*.csv`（首列 `fits_label`，样本名 `Galaxy_Center_panel3_Red` 等真实 FITS）与 `performance_report.md`（标题记为真实数据 C++ 实验）；包内 `raw_results.csv` 仅 4 个合成 dataset 名 | 有账无据（同族编号的真实数据留痕不在包内，两版并存） |
| 任务级 `evidence/` 留痕 | 包内无 `evidence/` | 仓库 `evidence/` 仅 `ciqa`、`refactor`、`v6_1_rework`、`v8_1_ci_control` 四个子目录；全库 `find -iname *dq00*` 仅命中 `lib/astro_image_io/tests/results/` | 有账无据（0 个包内编号有 `evidence/<号>/` 级留痕） |
| WP-A…I 与「步骤 1-17」体系 | 包内 `WP-` 0 命中 | `docs/stage1_fix/tasks.md`（`b4bcf8e7` 加入、`6e9dc1b8` 于 2026-08-04 删除）定义 WP-A…I 与步骤 1-17；提交 `1defe271`/`498a08de`/`e434d3d3`/`134beafc`/`0a5eefbc`/`91021c35`/`d49edb68`/`5c19ceeb` 以 WP 号为标题 | 有据无账（相对本组编号体系；该体系是本组交付之后 2.5 小时另立的旁支） |
| `DELETE_LIST.txt` 建议删除项 | `DELETE_LIST.txt` L9-14 | 三件 Python 原型 @`183558ad`、@`78a9121d` 存在；@`a78f5430` 与 @`HEAD` 不存在；删除动作发生在包外 `8653bd54`（2026-08-03，`refactor(r10)` Python 生产层清理） | 一致（建议成立，执行在包外、晚于包生命周期 3 天） |
| 包规模自述 | `00_README.md` §6 结构声明 + `MANIFEST.json` 48 条 | 48 条 `size` 合计 681911 B = 665.9 KiB；zip 盘上 230987 B = 225.6 KiB；`memory.md`@`78a9121d` L1451 同口径；`07_FINAL_DELIVERY_STRUCTURE.md` 的 ZIP 体积控制条款与之对应 | 一致 |
| Wiki 页数口径 | `00_README.md` L12（重写 8 个标准页面）；`repository_audit.md` L56（写「8 个页面」而随后列出 12 个页名）、L130（重写为 8 个标准页面） | 包内 `wiki/` 实测 16（10 无废止标头 + 6 有废止标头）；`memory.md`@`78a9121d` L1441 记为「10 标准页面 + 6 SUPERSEDED 标注，见 `_wiki_freeze/`」 | 计数口径不一致（8 / 10 / 12 / 16 四种值并存，未证实何者为准） |
| Wiki 页内引用可解析性 | `wiki/Home.md` L49-54、`wiki/_Sidebar.md` L19-24 | 被引页面 `Stage1-CLI接口.md`、`Stage1-浏览器检查.md`、`Stage1-验收与性能分析.md`、`Wiki-本地推送说明.md` 不在本包 `wiki/`，仅存在于同复原根的 `_wiki_freeze/…/wiki/`；`Home.md` L49 另指向 `engineering_authoritative/contracts/CLI_CONTRACT.md`，该路径 @`a78f5430` 被删、当前工作区 `find` 0 命中 | 有账无据（包内断链 4 页 + 1 外部路径） |
| 交付包与任务包的结构对应 | `00_README.md` L4（上游任务包名）；`07_FINAL_DELIVERY_STRUCTURE.md` L9-34（结构模板） | 交付包顶层十项与模板逐一对应；`reports/experiments/` 四件与 `05` §4 强制项一致；`09` 31 复选项在交付包内无回填件 | 一致（结构层面）/ 复选项未回填 |
| DQ 推荐取值（包内 对 包外后续） | `decision_queue.md` L101（support 用 LZ4 或 Zstd）、L104（FULL>80% / BITMAP 20-80% / SPARSE<20%）、L106（子块对齐 64 字节） | 包外 `8f505194`（22:14:49）正文对同族编号给出 DQ-002 推荐 RAW、DQ-005 推荐 5%、DQ-007a 推荐不 padding | 不一致（同族编号两版推荐并存；包内声明其推荐基于合成数据） |

## 八 缺口与不确定

1. **无任务台账 → 无提交对接面**：4 身份台账行 0（`pack_lineage_table.md` 行 13/14/16/52），`pack_commit_link.csv` 无本组行，任务号与提交消息的命中率不可计算（替代核对见 §七）。
2. **digest 与 inventory 的提交方向与 git 时序相反**：`pack_inventory.csv` L44/L43/L62 与三份 digest 记 first=`a78f5430`、last=`78a9121d`，而实测加入 12:55（`78a9121d`）、删除 18:29（`a78f5430`）。zip 身份记 `first/last=None`、仅 `touch_last=a78f5430`，实测其 blob 在 `78a9121d` 存在、`a78f5430` 不存在。
3. **计数不对应**：`dels` 对 #1/#4 各为同一 SHA 重复 6 次、`adds_n=6`（#3 为 1），与文件数 50 / 12 无映射关系 → 台账的「删除次数 / 新增次数」不能当文件数使用。
4. **同一 zip 被记两个身份**：`pack_instances.jsonl` 两条同 SHA（50 文件 / 689806 B）记录，身份键分别 `packs/history_zip` 与 `AstroCS_Stage1_HISS_Delivery_2026-07-31.zip`，form 分别为 `zip` 与 `zip_recovered_from_history` → 按实例累加会重复计数。
5. **入口槽位被错填**：`digest/AstroCS_Stage1_HISS_Delivery_2026-07-31.md` 的「00_READ_FIRST 开头」槽位内容是 `MANIFEST.json` 的 JSON 片段（且带 CRLF），并非任何 README；G03 两包均无 `00_READ_FIRST.md`（入口为 `00_README.md` / `START_HERE.md`）→ digest 的入口推断规则对本组不适用。
6. **provenance 记法与实际取回点不一致**：`history_zip/AstroCS_Stage1_HISS_Delivery_2026-07-31.zip.provenance.json` 的 `recovered_from_rev` 写作 `a78f5430f9ad…`（无 `^`），而该 zip 在 `a78f5430` 树中不存在、在 `59e9e39e`（`a78f5430^`）中存在（`git cat-file -e` 双向验证）；目录身份的 provenance 则正确记 `a78f5430…^`。
7. **解包形态自校验失效**：`MANIFEST.sha256` 指向 zip 版 `MANIFEST.json` 哈希 `d4678fd3…`，复现目录实测 `63a91a2c…`，且 48 条中 9 条内容不符（§一 判定 5）。哪一形态为权威：包内与仓库均无记载 → 未证实。
8. **容器身份与子身份的文件数与字节冲突**：`digest/_agent_package.md` 记 13 文件 / 30800 B，磁盘容器实测 14 文件 / 31227 B；`pack_inventory.csv` L62 亦记 13 / 30800；两个 provenance 均记 `recovered_files: 12`。
9. **无豁免与失败处理口径**：两包内 `waiver|豁免` 0 命中，且无 CI / 校验器 → 包内自校验失败（如缺口 7、§七 第 5 行）时无既定处置路径。
10. **测试留痕自相矛盾**：`tests/test_output.txt` L293-298 汇总块 `通过: 0 / 失败: 0` 与同包 `correctness_report.md` L11-17 的 `通过数 21` 并存；解释性修复记录（`59e9e39e` 17:46）与修复后的测试源码均不在包内（包内件停留在 `78a9121d` 12:55 版本）。
11. **两版 DQ 结论并存且取值不同**：包内 `decision_queue.md` L101/104/106 推荐 support 用 LZ4 或 Zstd、阈值 20% 至 80%、子块对齐 64 字节；包外 `8f505194` 正文对同族编号给出 support 推荐 RAW、阈值 5%、不 padding。包内数据源为 `tests/hiss_benchmark.cpp` 合成集，包外为真实 FITS 集。哪一版代表后续冻结输入：未证实（包内自述仅为推荐）。
12. **同批声明的第三个根目录 zip 不在本组也不在任何 digest**：`a78f5430` 正文声明保留 `AstroCS_Stage1_Fix_Review_2026-07-31.zip`，但该路径在全库 `git log` 0 命中、在 `a78f5430` 树中不存在 → 是否曾入库：未证实。另两个根目录 zip 中 `AstroCS_Delivery_20260729.zip` 归 G01。
13. **Agent 任务包无 zip 形态**：全库 `--diff-filter=A` 检索 2026-07-24 至 08-05 期间入库的 zip 仅 4 个（`AstroCS_Delivery_20260729.zip`、`AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0.zip`、`AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip`、`AstroCS_Stage1_HISS_Delivery_2026-07-31.zip`），任务包从未以 zip 入库 → 任务包是否有过对外发放形态：未证实。
14. **Wiki 页无法与真实 Wiki 仓核对**：`AstroCS.wiki/` 在 `.gitignore` L122 被忽略且当前工作区 0 文件，`78a9121d` 树内亦无该目录、且无 `.gitmodules`（`git show 78a9121d:.gitmodules` 报路径不存在）→ 包内 16 页是否已同步至远端 Wiki：未证实（`repository_audit.md` L55 仅声明远端已启用）。
15. **审计执行环境不可复核**：`repository_audit.md` L4 记审计主体为 `TRAE AI Agent (GLM-5.2)`、L12 记仓库根为 `f:\Astro dev\Astro CS Normalization Database`（`APPLY_IN_PLACE.md` §1 同值），该 Windows 副本在本仓库无痕迹；而基准 commit `183558ad`、远端与 `main` 分支在本仓库均可核 → 审计环境事实未证实，审计基准可核。

---

**与上一组 G02 的衔接**：G02 末包为 `AstroCS_Stage1_Wiki_Freeze_2026-07-30`（`设计大纲/_evidence/packs/history/a78f5430/_wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/`），与本组三个物理对象同为 `78a9121d` 加入、同为 `a78f5430` 删除（`git log --diff-filter=A -- _wiki_freeze` 唯一命中 `78a9121d`），二者在仓库生命周期上完全同批。内容关系是**承接 + 部分取代**而非整体废止：本组交付包 `wiki/` 的 16 页与 Wiki_Freeze `wiki/` 20 页中的同名 16 页，在 LF 归一后逐页 sha256 相同（16/16 无差异），Wiki_Freeze 独有 4 页（`Stage1-CLI接口.md`、`Stage1-浏览器检查.md`、`Stage1-验收与性能分析.md`、`Wiki-本地推送说明.md`）未被交付包携带，而交付包 `wiki/Home.md` L49-54 与 `wiki/_Sidebar.md` L19-24 仍在引用它们（§七 倒数第 3 行）；被取代范围仅 6 个中文页（`wiki/Home.md` L43-53 的逐页取代映射 + `03_WIKI_UPDATE_REQUIREMENTS.md` L59-64 的标头要求，交付包内 6 页首行实测含 `SUPERSEDED — DO NOT IMPLEMENT`）。交付包 `00_README.md` L4 把自己的上游明确写为 `AstroCS_Stage1_HISS_Agent_Package_2026-07-31` 而不是 Wiki_Freeze，且 `repository_audit.md` L55/L90/L130 把 `_wiki_freeze/` 当作 Phase 0 审计对象与计划替换对象。另两处跨组事实：本组的 8 个英文标准页恰好是 Wiki_Freeze 自身 `MANIFEST.json`（14 条）未覆盖的那 8 页（§六 表），即本组任务把标准页写进了上一组的冻结目录而未回写其清单；交付包 `wiki/Home.md` L49 指向的 `engineering_authoritative/contracts/CLI_CONTRACT.md` 属更早一代（G02）包，该路径在 `a78f5430` 被删且当前 `find` 0 命中 → 两代之间无书面继承或废止声明，是否有意取代：未证实。

**重叠身份**：按 `设计大纲/_evidence/packs/dispatch_groups.json` 全量比对，G03 的 4 个身份均未出现在其他组的身份列表 → 组间无身份重叠。组内存在两处重叠：(a) `AstroCS_Stage1_HISS_Delivery` 与 `AstroCS_Stage1_HISS_Delivery_2026-07-31` 为同一交付物的目录 / zip 两形态（路径集合双向差集为空，10 文件仅行尾差异，§一 判定 1），并在 `pack_instances.jsonl` 层再被拆成 `packs/history_zip` 与 `…_2026-07-31.zip` 两条同 SHA 记录（§八 缺口 4）；(b) `_agent_package` 与 `AstroCS_Stage1_HISS_Agent_Package_2026-07-31` 为容器目录与包目录的父子重叠（12 个内容文件同一，§一 判定 2）。物理共存关系：G03 复现目录与 G02 的 `_wiki_freeze/` 子树同处复原根 `设计大纲/_evidence/packs/history/a78f5430/` 之下，G03 的 zip 与 G01 的 `AstroCS_Delivery_20260729.zip` 同处 `设计大纲/_evidence/packs/history_zip/`。digest 与本体的核对结果：文件集合、zip sha256、`recovered_files` 计数三项一致；首次与最后提交方向、`dels` 与 `adds_n` 计数、zip 形态 git 事件、容器身份文件数与字节四项不一致（§八 缺口 2/3/5/8）。

