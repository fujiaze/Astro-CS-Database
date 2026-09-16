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


---

## 附录 F　迁移纪律（2026-09-16 事故后确立）

**事故**：ROOT-008 一次性把 `cli/`、`modules/`、`runtime/`、`providers/` 搬走后，才统一改根 `CMakeLists.txt` 引用 ⇒ 工作树在一个窗口期内**无法 `cmake`/`ninja`**，同时阻塞了 SCI-FIX-NOISE / SCI-FIX-WEIGHT / SCI-FIX-PSF 三条线的「构建 + ctest」验收门。

**纪律（即刻生效，适用于一切搬迁/重命名类任务）**：

1. **分批搬迁，每批结束必须可构建**：按目录分批 `git mv` → **立刻**同步引用（CMake/include/脚本相对深度）→ **立刻**跑 `cmake` + `ninja`；一批绿了再搬下一批；
2. **禁止「全部搬完再统一改引用」**：中间态不得超过一次构建周期；
3. **跨任务写域冲突预判**：搬迁任务开跑前，前台必须列出「哪些在跑任务的文件域会被搬走」，并**先通知**那些任务改到新址（本次 GATE-FIX-RES 的 `cli/**` 资源件即属此类，已提前化解）；
4. **搬迁期间其它任务不得跑 `cmake`/`ctest`**（会污染证据或抢 build 目录）；前台负责广播「构建可用/ctest 空闲」两个时刻；
5. **验收门不因阻塞而免除**：被阻塞的任务改用定向验证（直接编译生产源 / 真跑探针）取证，构建恢复后**补跑**并写进自证摘要。


---

## 附录 G　行号锚同步授权（前台裁定，对所有执行线生效）

**背景**：多线反复因「我的改动移动了被文档引用的符号行号」而停下请示（W4-A7、W4-A9、SAT-001、MASK-002 各一次）。

**裁定**：
1. **行号锚同步属机械同步，不构成科学内容修改**——它是 ENGINEERING_SPEC §8「锚存活」义务的一部分，**不触发**「禁改 docs/science/**、docs/algorithms/** 科学内容」的红线。
2. **责任在改动方**：谁改动了被文档内联引用的文件（含仅增删行），谁必须在**同一批**内把该文档的行号引用与锚号同步到实测值，并复跑锚点门至 rc=0。
3. **只许改数字**：同步时只允许改行号/锚号；若发现文档**表述与实现语义不符**（不只是行号漂移），**停下报告**，由前台按 SCIENCE_CORRECTNESS 走科学订正流程（claim 登记）。
4. **外部引用写法**：代码/文档注释里引用**仓外**文件时禁止写成 file.ext:NNN（锚点检查器会当仓内锚解析而报 no-such-file），一律写成 file.ext LNNN（例：isrTask.py L431-442）。
5. **锚点门是批次门的组成部分**：任何批次交付前必须四门齐全——cmake rc=0 + ninja 0 FAILED + ctest ≥ 基线且 0 失败 + 锚点门 rc=0。

---

## 附录 H　构建隔离 + 门的分层（前台裁定，对所有执行线生效）

**背景**：多线同时往共享 build/ 跑 cmake/ninja/ctest，造成三类事故：① 抢目录、互相污染证据；② 负载敏感测试假红（已确认 4 条：p1drz_merge_pipeline_lock、rt001_unique_executor、p1cos_performance、p1hips_performance）；③ 迁移中间态下构建不可用而全线停摆。

**裁定**：
1. **构建隔离（硬）**：执行线一律使用自己 ID 下的独立 build 目录（run/PROJECT-GOVERNANCE-01/<ID>/build/），**禁止**再对共享 build/ 跑 cmake/ninja/ctest；共享 build/ 只由**前台在提交点**用于四门复跑。
2. **门的分层（硬）**：
   - **逐线批次门** = cmake rc=0（自己的 build 目录）+ ninja 0 FAILED + ctest ≥基线且 0 失败 + **本域无新增锚违规**（自己新增/改动的锚逐条存活：文件存在且**被追踪**、行号在界、符号逐字命中）；
   - **整门锚点 rc=0** = **提交点门**，由前台在空闲窗口统一复跑；并行迁移期整门翻转属预期，不作为单线批次门；
   - **负载敏感测试**必须在**独占**窗口复跑 ≥3 次定性，并登记到负载敏感清单；确认后其工作目录按实例唯一化。
3. **中间态纪律（重申附录 F）**：一批 = 一个可构建状态；**签名/结构变更与本批调用点同批**；**删除与引用改绑同批**（顺序：先改引用，再删对象）；未追踪旧副本必须先快照到 run/PROJECT-GOVERNANCE-01/<ID>/preserve/ 再清理，并列出清单。
4. **提交窗口**：前台提交前广播冻结窗口；窗口内执行线不得改工作树、不得跑 cmake/ctest。

### H.5　断言活性与注入有效性（补充，2026-09-17 多例事故后追加）

**背景（四条已实证的同类事故）**：
1. **退化注入**：`kernel_link_wired_without_registration` 在接线后变成 no-op（写的 key 已存在且同值）⇒ 自检会因「期望红却绿」失败（W5-CPU-001 发现）；
2. **空断言（vacuous assertion）**：产物 glob 只写 `calibrated_*.fts` 而夹具写 `.fits` ⇒「不留半成品」断言在合成模式下**从未看见任何产物**、恒真（UNIT-001 自纠，被新增正例暴露）；
3. **门的存在性依赖 configure 次数/缓存**：`p1noise_abi_layout` 的 `add_test` 先用 `${P1NOISE_PYTHON3}`、`find_program` 后定义 ⇒ 全新目录恒红、缓存目录恒绿（W4-A9 用翻转实验证明）；
4. **负载敏感假红**：并行构建/多线共跑时 `*_performance`/`dispersion_ok`/`lock` 类断言随机红（已登记 6 条）。

**硬性要求**：
1. **断言活性**：任何断言都必须能回答「它看见了什么」。**空匹配/零命中 = 假绿**，必须显式判失败（例：匹配到的产物数 > 0；regex 命中数 > 0；被测目标存在）。
2. **品类前缀而非单一后缀**：涉及产物时用品类前缀/多后缀集（`calibrated_*`、`*.[fF][iI][tT][sS]?`）或直接以 manifest 为准，禁止只写一个后缀。
3. **注入有效性**：每个故障注入必须证明「改了什么、为何因此必然判红」；注入后若目标已不存在/已同值 ⇒ 必须**失败**，不得静默通过。
4. **门的可存在性**：`add_test` 用到的变量必须在**其之前**定义；缺依赖必须 `FATAL_ERROR`，禁止静默退化为裸脚本路径。CI 已增设 `CMakeLists use-before-define` 常设门（基线 0 命中）。
5. **提交点复跑必须在全新 build 目录**进行（不复用缓存 `build/`），以捕获第 3 类问题。

### H.6　烧毁式基线的三条硬约束（LEDGER-DOC 以实证发现）

适用场景：**渐进式收敛**的门（缺口已知且只应减少），既不能恒红挂 CI（会让人对红灯脱敏），也不能没有门。

1. **基线只许下降**：门语义为「每一类缺口 ≤ 基线值」；基线值只许在**落地批**里手改下调，且必须在自证摘要写「本批下调 X→Y，依据=落地了哪几条」。缺口归零后**删除基线文件**，门转**全量强制**。
2. **故障注入必须绕过基线**（实测）：若负例也吃基线，`drop-page-module` 等会被「未超基线」掩盖成 rc=0 ⇒ **假绿**。故自检模式**一律全量强制、忽略基线**。
3. **判据活性必须精确到「期望错误类 ∧ 点名对象」**：仅要求输出里出现某个名字不够——首版 `dup-alias` 注入是**死匹配**，其点名命中来自无关的 `MODULE_WITHOUT_PAGE` 行 ⇒ 假绿。活性判据 = 期望错误类成立 **且** 点名对象逐字命中；空匹配判 `DEAD`。
