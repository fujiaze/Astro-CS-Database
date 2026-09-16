# 派发提示词（DISPATCH）

> 用途：把本控制包的任务分给**其他 Agent** 执行。提示词是复制粘贴即用的；每条都已把环境、纪律、动作、回报要求写全，Agent 不需要看本次对话。
> 本文件只是**提示词模板**，不代表任何任务已开始或通过。

## 0. 使用说明（给负责人）

- 派发前必须满足：`任务状态 = NOT_STARTED` 且**依赖任务已 PASS**（见 `TASK_LIST.md`）。
- 一次只派一个任务文件域内的活；同组（S6-P1 / S6-P2 / S6-P3 / S6-C / S6-D）可并行，其余按依赖串行。
- 每个 Agent 干完只交自证材料，**不 commit、不 push**；由你的调度员 Agent 复跑验收门后原子提交。
- 回报不合格（缺命令/退出码/证据路径）一律打回重做，不接受口头结论。

## 1. 通用派发提示词（所有任务共用，替换 <TASK-ID>）

```text
你是 AstroCS 项目的执行 Agent。本次只做一件事：执行控制包任务 <TASK-ID>。

仓库根：/workspace/Astro CS Database （路径含空格，命令务必加引号；不要换工作目录另建 clone）。

第一步（不读完不许动手）：
1. 读 工程控制/PROJECT-GOVERNANCE-01/tasks/<TASK-ID>.md 全文；
2. 按该文件「权威依据」逐条打开对应文档章节读原文；
3. 读 AGENTS.md、ENGINEERING_SPEC.md、CONTROL_PACK_SPEC.md 中与本次相关的条款；
4. 用一段话复述：本任务要改什么、改到哪、怎么算通过、什么绝对不能碰。

第二步（动手）：
5. 严格只改该任务「改动范围 → 允许改」列出的文件域；「禁止改」列的绝不触碰；
6. 按该文件「步骤」逐条实施，每条都要留下命令与输出；
7. 所有外部命令加 timeout，日志写入 run/PROJECT-GOVERNANCE-01/<TASK-ID>/logs/；
8. 逐条跑「验收门」；任何一条不过，必须先修到过；修不过就停下来登记 BLOCKED 并说明卡在哪一条、缺什么。

硬禁令（违反即回退）：
- 不 commit、不 push、不建分支、不 stash/reset/clean/checkout；不动工作区里与本任务无关的预存修改与未跟踪文件；
- 不改 docs/science/** 与 docs/algorithms/** 的公式、阈值、推导；不改默认容差与冻结 SCI/ALG；
- 不用 facade、空实现、no-op、注释掉测试来让门变绿；不放宽或删除检查项；
- 不用「环境问题/工具问题」解释失败；必须给可复现命令与输出。

第三步（回报，按此格式，缺项即不合格）：
- 任务ID 与状态：DONE / BLOCKED；
- 改动文件清单（逐个路径 + 一句话说明）；
- 逐条验收门：门的内容 → 命令 → 退出码 → 关键输出片段（不超过 5 行）；
- 未决项 / BLOCKED 原因（若无写「无」）；
- 你**没有**做但认为必须做的事（不要顺手做，只报告）。
```

## 2. 派发顺序与并行建议

| 波次 | 任务 | 并行 | 说明 |
|---|---|---|---|
| W0 | BASE-001 | 单独 | 冻结基线与预存改动边界，必须先做完；它产出的预存清单是所有后续任务判断「越界」的依据 |
| W1 | GOV-001、DOC-001 | 可并行 | 两者文件域已分开（GOV 动根治理入口与检查器，DOC 动文档）；若都改 README.md，则改为串行 |
| W2 | DATA-001 | 单独 | 合同是后续所有任务的输入，不要与其它任务并行改 contracts/** |
| W3 | CFG-001、MOD-001 | 可并行 | CFG 动 config/** 与配置 schema；MOD 动检查器与映射表 |
| W4 | ARCH-001 | 单独 | 大规模目录迁移，工作区写入必须串行 |
| W5 | AIO-001、RT-001、CI-001 | AIO 与 RT 可并行；CI-001 与 GOV-001/MOD-001 的检查器改动需串行 | 三者都会动检查器/注册表，动手前先确认文件域不重叠 |
| W6 | CLI-001、P1-001、P2-001、P3-001、CPU-001、OBS-001 | 六者可并行 | 文件域互斥；但根 CMakeLists.txt 谁都不许动 |
| W7 | CLI-002、CLI-003、P1-002、P2-002、P3-002 | 可并行 | 同样不许动根 CMakeLists.txt 与中央 registry |
| W8 | INT-001 | 单独 | 唯一允许改根 CMake/中央 registry 的任务，必须等 W6/W7 全 PASS |
| W9 | QA-001、PKG-001 | 可并行 | QA 动 .github/** 与 ci/** 绑定；PKG 动 packaging/** 与安装规则 |
| W10 | REAL-001 | 单独 | 需锁定最终 SHA；期间禁止任何提交 |
| W11 | FINAL-001 | 单独 | 独立总审计，只写报告与台账 |

## 3. 常用补充提示词

### 3.1 只审计不改（当你不确定某任务是否值得做时）
```text
你是 AstroCS 的只读审计 Agent。只做取证，不改任何文件。
核对 GAP_AUDIT.md 中 <GAP-ID 列表> 的「权威依据」条款号是否真实存在、内容是否支持结论，
以及「当前证据」是否与仓库现状一致（给文件+行号或命令输出）。
输出：逐条判定 CONFIRMED / PARTIAL / WRONG / UNRESOLVED + 关键证据 + 建议修正措辞。
禁止 git 写操作，禁止修改仓库内任何文件。
```

### 3.2 复核（给验证 Agent）
```text
你是 AstroCS 的独立验证 Agent。对任务 <TASK-ID> 的交付做独立复跑：
1. 打开 工程控制/PROJECT-GOVERNANCE-01/tasks/<TASK-ID>.md，逐条执行「验收门」命令，记录退出码与输出；
2. 不要相信执行 Agent 的自述，一律自己跑；
3. 检查 git diff 是否越出「改动范围」；越界即判 FAIL 并列出越界文件；
4. 特别检查是否有 facade/no-op、被注释掉的测试、被放宽的检查项、被删除的负例；
5. 输出：逐门 PASS/FAIL + 命令 + 退出码 + 结论；任何一门 FAIL 则总体 FAIL。
你没有 git 写权限。
```

### 3.3 修 BLOCKED（给负责人/调度员）
```text
任务 <TASK-ID> 登记为 BLOCKED，原因是：<粘贴原因>。
请只做一件事：判断这是「文档冲突」「科学歧义」「权限/数据/环境缺失」还是「任务拆分不当」。
- 若属文档冲突或科学歧义：给出应裁决的条款对与两种口径，交由负责人决定，不要替负责人选；
- 若属缺失：给出最小补齐方案（缺什么、谁能提供、补上后哪条门会过）；
- 若属拆分不当：给出该任务的原子化拆分建议（每个子任务的文件域与验收门）。
只输出判断与建议，不改代码。
```

## 4. 派发时的常见错误（务必避免）

- 一次派多个任务给同一个 Agent：会造成文件域交叉与越界改动；
- 让执行 Agent 自己 commit：SubAgent 零 git 写权限是硬规则（CONTROL_PACK_SPEC §6.2）；
- 跳过 BASE-001 直接改代码：之后无法区分「预存改动」与「任务改动」，验收会失去意义；
- 让 ARCH-001 与其它任务并行：目录级 rename 期间任何并行写入都会互相踩；
- 允许执行 Agent 改根 CMakeLists.txt：只有 INT-001 可以。

---

## 附录 B　问题扫描 → 任务派发矩阵与波次（前台 2026-09-16 生成，验证后派发）

**数据来源**：26 片 PSV 聚合 → `reports/PROJECT-GOVERNANCE-01/root-scan/DISPATCH_MATRIX.md`；逐任务条目清单 → `reports/PROJECT-GOVERNANCE-01/root-scan/by_owner/<任务名>.txt`（37 个文件）。

**前台对合并表的独立验证**（接手轮交付物）：`问题扫描/REBASE_TABLE.md` = **785 数据行 / 785 唯一 ID / 0 重复 / 与 26 片 ID 对称差 0 / 第 5、6 列 785/785 非空**；四态 = OPEN 727 / RESOLVED 42 / VOID 13 / UNVERIFIABLE 3。

**待对账 1 条**：P0 中「OPEN 72（本矩阵实测）vs 73（P0_RECHECK.md 自述）」差 1 条 —— 已登记，要求接手轮/下一轮对账时给出唯一口径（不影响派发，因派发按 ID 逐条而非按计数）。

### B.1 覆盖缺口（无任务卡）

- 归属值 37 种，其中 **9 种共 47 条无对应任务卡**，全部为 `NEXT-PACK:NP-*` 变体（`NP-DC-02` 15 / `NP-01` 8 / `NP-DC-01` 8 / `NP-02` 4 / `NP-DC1` 4 / `NP-GAIA-01` 3 / `NP-03` 2 / `NP-04` 2 / `NP-DC-03` 1）；
- **其中 P0-OPEN = 0**（即：未映射项不含 P0），故不阻塞当前波次；
- 处置：编号需先归一（`NP-DC-02`/`NP-DC1`/`NP-DC-01` 疑为同一族），再由下一轮控制包统一立项（已登记为待办，不在本波强行派发）。

### B.2 派发波次

| 波次 | 任务（条目数 / P0-OPEN） | 状态 | 依据 |
|---|---|---|---|
| **W1（已派）** | ROOT-001..007、TEST-GREEN-001、DATA-001、MOD-001、CLI-001、ARCH-001、CFG-001、RETIRE-001、CI-001(1) | 已交付/在跑 | 本文件前文 |
| **W2（本轮派发）** | **DOC-001**（93 / 4）、**GOV-001**（28 / 6） | 已派发 | 文档与治理域，与 `lib/**` 迁移不冲突 |
| **W2（在跑）** | TEST-CLI-SYNC（41 文件）、CI-001 第二轮（94 / 12）、W1 调度（CFG-001 收口、验证 Agent） | 在跑 | — |
| **W3（等 ARCH-001 停稳）** | **P1-001**（112 / 16）、**P1-002**（42 / 2）、**P2-001**（42 / 3）、**P2-002**（21 / 4）、**P3-001**（18 / 4）、**P3-002**（24 / ?）、**AIO-001**（32 / 4）、**RT-001**（26 / ?）、**OBS-001**（25 / ?）、**MOD-002**（引用刷新） | 待派 | 全部落在 `lib/**`（ARCH 迁移域），并发会与 `git mv` 冲突 |
| **W4（与 W3 并行，不依赖 lib/）** | **CLI-002**（export 缺陷，GAP-034）、**CI-002**（三项守卫 + 工作流切换，依赖 QA-001）、**CFG-002**（配置登记遗留）、**PKG-001**（20 / 3）、**QA-001**（20 / ?）、**CPU-001**（16 / ?）、**CLI-003**（7）、**INT-001**、**REAL-001**、**FINAL-001** | 待派 | 与 `lib/**` 迁移无文件域交集（PKG/QA/CPU 需在迁移停稳后复跑构建类门） |

### B.3 派发纪律（本轮确立）

1. 每条派发**必须携带该任务的 `by_owner/<任务名>.txt` 清单**（禁止「按卡泛做」而不逐条对账）；
2. 执行行**先对清单做当前树复检**（合并表结论取证于分片时点，此后 ROOT-007/TEST-GREEN-001/CLI-001/DATA-001/MOD-001/RETIRE-001 已落地并推送，状态可能已被解决）；
3. 处置表**计数必须与清单条目数一致**（少一条即视为未收口）；
4. 一切以第 6 列「命令 + 逐字输出」为准，**不得采信账本 fix_state/verified_state**。


---

## 附录 C　当前写域占用与在跑任务（前台实时维护 · 交接必读）

> 继任前台请**先读本附录**再决定派发：下表是「谁现在正在写哪个域」。占用的域**不得**再派第二个写者。

### C.1 在跑任务与写域（2026-09-16 18:40 快照）

| 任务 | 状态 | 独占写域 | 备注 |
|---|---|---|---|
| **ARCH-001** | 在跑（~90%） | `lib/**` + 根 `CMakeLists.txt`/`tests/**/CMakeLists.txt`/`cmake/**` 的**机械路径替换** + `docs/algorithms/**` 锚点与 `anchor_contract.json` | **`ARCH-CLEAR` 信号前，任何任务不得写 `lib/**`**；W3 整波（P1/P2/P3/AIO/RT/OBS/MOD-002）都在等它 |
| **CI-001（第二轮）** | 在跑 | `ci/checks.json`、`ci/run_checks.py`、`ci/tests/**`、`ci/id_migration_map.json`、`tools/check_api_docs.py` 的 `_repo()` 严格化 + GAP-027 P0/P1 fail-closed | 另占 `.github/**`（工作流切换本身归 CI-002）；**他人一行不得改 `ci/checks.json`** |
| **DOC-001** | 在跑 | `docs/**`（不含 `docs/algorithms/**` 的推导与 `docs/science/**` 的公式）、根活动文档、`docs/DOCUMENT_INDEX.yaml`；含 `docs/api/CLI_PROTOCOL_V1.md §7` | 与 ARCH 的锚点域在 `docs/algorithms/**` **重叠**——DOC-001 只允许改该目录的索引/链接/状态标注 |
| **GOV-001** | 在跑 | 根治理文档、`tools/check_agents_gov.py`、`tools/doccheck/check_engineering_constraints.py` | 注册项元数据只登记不改（`ci/**` 归 CI-001） |
| **W1 调度线** | 在跑 | 无独立写域（调度 + 验证 + 台账维护） | 维护 `ACCEPTANCE.md`/`TASK_LIST.md` 状态列 |
| **前台（我）** | 在跑 | `工程控制/**`、`reports/PROJECT-GOVERNANCE-01/**`（证据）、提交与推送 | 只做调度/独立复跑/原子提交 |

### C.2 冻结中（未提交、勿动）

| 文件 | 归属 | 状态 |
|---|---|---|
| `config/**`、`ENGINEERING_SPEC.md §7`、`tests/quality/test_root_cleanliness.py:76`、`ci/root_manifest.json` | **CFG-001**（已完成，待提交） | 等「CFG-001 独立验证 PASS + ARCH-CLEAR」后由前台整文件提交（`ci/root_manifest.json` 含 RETIRE-001 的 7 条清理，属**共享提交**，消息须写明两处归属） |
| `lib/**` 的 1114 条 git mv（已入索引） | **ARCH-001** | 尚未成为独立任务提交；前台提交**必须带显式 pathspec**（`git mv` 会写索引，历史上有过连带提交事故） |

### C.3 提交纪律（前台铁律，继任者请沿用）

1. **一律 `git commit -- <显式路径>`**，并在提交前核对 `git diff --cached --name-only`；**禁止**裸 `git commit`（ARCH 的暂存 rename 会被连带提交）；
2. 每次提交后 `git push`，再 `git fetch` 核对 `HEAD = main = origin/main`；
3. 归属更正一律走**新提交 + 台账注记**，禁止 amend/重写已推送历史；
4. 共享文件（多任务都改到）**不做 hunk 级拆分**，整文件提交 + 消息写明各处归属。

### C.4 下一批派发顺序（依赖就绪判定）

1. **`ARCH-CLEAR` 一到** → 派 W3：`P1-001`（112 条 / 16 P0-OPEN，最高优先）、`P1-002`、`P2-001`、`P2-002`、`P3-001`、`P3-002`、`AIO-001`、`RT-001`、`OBS-001`、`MOD-002`；
2. **不等 ARCH** 可派：`CLI-002`（export 缺陷 GAP-034，P0）、`CI-002`（依赖 QA-001）、`CFG-002`、`TEST-CLI-SYNC-2`（依赖 CLI-002）；
3. 每条派发**必须携带** `reports/PROJECT-GOVERNANCE-01/root-scan/by_owner/<任务>.txt` 清单，并要求执行行先做当前树复检（合并表状态取证于 15:00 前后，其后多个任务已落地）；
4. 未映射的 47 条（`NEXT-PACK:NP-*`，编号需归一，**无 P0**）由下一轮控制包统一立项。


---

## 附录 D　提交归属更正（前台记账 · 不改历史）

### D.1 `eaf32aad`（ARCH-001 迁移）**连带提交了在途线文件**

- **原因**：前台对 `lib/**`/`docs/**`/`tests/**` 做整树 `git add -A` + pathspec 提交时，**P1-001（在跑）与 GOV-001（在跑）正在同域写入**，其未提交改动被一并纳入；
- **被连带的具体项**：
  - P1-001 的 9 个 comment-hygiene 文件（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.{cpp,h}`、`spherical_overlap.cpp`、`drizzle/module_entry.cpp`、`calibration/master_generator.cpp`、`calibration/cosmetic_corrector.cpp`、`platesolve/cpp/ipv/src/ipv_wcs.cpp`、`ipv_types.h`、`ipv_robust_refine.cpp`）；
  - GOV-001 的 2 个归档副本（`docs/archive/HANDOVER.md`、`docs/archive/VISUAL_CHECK_README.md`）；
- **处置**：内容合法、**归属挂错**、**不改历史**；两条线各自的自证摘要为准据台账；ACCEPTANCE 里对应行注明「共享提交」；
- **流程修正（即刻生效）**：
  1. **禁止**在他线在跑时对整棵树（`lib`/`docs`/`tests`）做 `git add -A`；提交前必须先 `git status` 列出**在跑线的在途文件**并用 pathspec `:(exclude)` 排除；
  2. 前台每次提交前声明「本次将排除的在途文件」；
  3. 已发生两次同类事故（`23acb453` 的 `git mv` 连带、`eaf32aad` 的整树连带）——第三次视为流程失控，须暂停派发做写域冻结。


---

## 附录 E　W3 域授权修正：**按对象授权**（前台 2026-09-16，包结构缺陷修正）

### E.1 缺陷

P2-002、OBS-001、P3-001 三条线均报「**卡内文件域不覆盖它自己的验收对象**」：
- P2-002 卡授权 `lib/algorithms/{sampling,integration,rejection,upm}/**` ⇒ 这些目录现存**只有 README/module.yaml/memory.md，零源码**；生产源在 `lib/algorithms/coverage/src/`；卡里写的 `tests/mosaic/**` **树内不存在**；
- OBS-001 卡授权 `lib/infrastructure/observability/**`（当前只有 PENDING.md）⇒ 其门禁阈值测试在 `tests/unit/mon00*_gate_test.cpp`、exit 10 门在 `cli/**`；
- P3-001 卡授权 `tests/export/**` ⇒ **树内不存在**（投影测试实为 `tests/unit/p3_projection_test.cpp` 与 `tests/backend/test_p3_projection_oracle.py`）。

**根因**：扫描的「归属」按**旧账本子系统**划分，落到控制包的**目录域**就错位。这是包的结构缺陷，不是执行行的问题。

### E.2 修正原则（即刻生效）

1. **按对象授权**：对象在哪就授权到哪，**不再**要求对象落在卡里写死的目录；
2. 授权**逐文件/逐模块列明**（由前台在派发消息里给出），不等于整目录放开；
3. **同一文件同时只允许一个写者**；跨任务共享文件的串行顺序由前台指定；
4. **科学语义仍只读**（`docs/science/**`、`docs/algorithms/**` 公式与锚、SCI/ALG 冻结定义）——需要改 ⇒ 升级为 `OWNER_DECISIONS.md` 项，不得在代码里绕过；
5. 卡的「允许改」与实际树不符时，**由前台在卡内追加订正段**（不改历史），执行行按订正段执行。

### E.3 本波具体授权

| 任务 | 授权（逐对象） | 串行/前置 |
|---|---|---|
| **OBS-001 → 拆两段** | **OBS-A**：`lib/infrastructure/observability/**` + `tests/monitoring/**`；**OBS-B**：`cli/**` + `tools/**`（除 `ci/**`） | OBS-A 可立即开工；OBS-B 与 CLI-002 串行（同改 `cli/`）；`ci/checks.json` 的注册面等 CI-002/CI-003 窗口 |
| **P2-002** | `lib/algorithms/coverage/{src,include,tests}/**` + 新建 `lib/algorithms/coverage/tests/**` 内的 Oracle | **P2-001 先**（同目录）；`weight_mode=2` 语义裁决（D-7）后才动分支 |
| **P3-001** | `lib/algorithms/projection/**` + `tests/unit/p3_projection_test.cpp` + `tests/backend/test_p3_projection_oracle.py` | D-1/D-2 裁决后才可改冻结公式（裁决前只许补独立 Oracle 与登记） |
| **P1-002 / P3-002 / AIO-001 / RT-001** | 各自清单里对象所在的 `lib/algorithms/*` 或 `lib/infrastructure/*` 模块（逐条列明） | 同模块内串行 |
| **MOD-002** | `docs/modules/**`、`tools/quality/check_module_map.py`、`tests/quality/test_module_map.py` | 无（已开工） |

### E.4 卡面订正（追加段，不改历史）

- `tasks/P2-002.md`：`tests/mosaic/**` 不存在 ⇒ 订正为 `lib/algorithms/coverage/tests/**`；`lib/algorithms/{sampling,integration,rejection,upm}/**` 无源码 ⇒ 订正为「对象在 `lib/algorithms/coverage/src/**`，与 P2-001 串行」；
- `tasks/P3-001.md`：`tests/export/**` 不存在 ⇒ 订正为 `tests/unit/p3_projection_test.cpp` + `tests/backend/test_p3_projection_oracle.py`；
- `tasks/OBS-001.md`：拆 OBS-A/OBS-B 两段并改写允许域（见 E.3）。

