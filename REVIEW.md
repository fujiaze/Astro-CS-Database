# REVIEW.md — AstroCS 项目负责人审查入口（L0）

> 目标产品：**0.11.0-alpha.2**（根 `VERSION`，GOV-003 唯一源；
> 生成串 `0.11.0-alpha.2+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）
> 建立：GOV-004（SA-GOV-01），基线时点 `caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（历史值）。
> **本轮状态收敛：DOC-CONV-001**（cprun run `Rmtxvlrtfa66eb7` rev23 / dispatch
> `7c0b15abaee56640`），收敛基线 **BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`**
> （执行时 HEAD = main = origin/main 三 SHA 一致）。本文所有执行级证据均来自该 BASE
> 的实测（命令与 rc 见 `run/docconv001/logs/`）。
> 项目负责人只需阅读本文件及 `docs/owner/` 5 份顶层文档，底层文档/源码/日志通过
> 证据 ID 与文件锚追溯，不要求逐文件阅读。

## 1. 一句话结论

```text
Alpha 架构收敛已完成主体：合同面（冻结宪章/文档边界/版本单源/ABI v1/数据产物/
Runtime 图/工具链 preset/DLL schema/FITS 流接口/内核标准注册表）冻结在位；
三 Phase 节点化（每 DAG 节点唯一真实模块 operation）、Phase3 四投影 registry、
RT 唯一 executor + 实测资源门、MOD 科学模块安装面、CLI validate/plan/inspect
薄命令面均已落地（源码 + 当前提交实测绿）；遗留 run --phases 连跑已删除。
尚未完成：Windows 发布执行面（MSVC/DLL 安装树复验）与真实数据（BASS/32R/接缝）
验收，以及 healpix_interp4 / Phase3 流式 FITS 接入。
当前状态：NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）。
```

## 2. 顶层文档（点击审阅，负责人 L0 直达全部权威）

| 文档 | 内容 |
|---|---|
| [SCIENCE_OVERVIEW](docs/owner/SCIENCE_OVERVIEW.md) | 科学权威源汇总、Phase1/2/3 逐项状态、诚实缺口 |
| [PIPELINE_OVERVIEW](docs/owner/PIPELINE_OVERVIEW.md) | 三 Phase 隔离模型、各 Phase 内部链与节点绑定、跨 Phase 磁盘交换 |
| [ARCHITECTURE_OVERVIEW](docs/owner/ARCHITECTURE_OVERVIEW.md) | Windows 优先、ACR dormant、唯一 Runtime、DLL 边界、依赖方向 |
| [RELEASE_STATUS](docs/owner/RELEASE_STATUS.md) | **状态词阶梯（§0 唯一口径）**、冻结面清单、未完成清单、发布 Gate 口径 |
| [CHANGE_REVIEW](docs/owner/CHANGE_REVIEW.md) | 本轮集成变化、验证、已知限制 |

> 历史旧轮次顶层文档（`docs/archive/review/*`，GOV-002 归档）为非规范，不作当前权威；
> `docs/review/*` 为 L0 治理评审层（ACTIVE_INFORMATIVE），状态口径一律以
> `docs/owner/` 与本文为准。

## 3. 状态词阶梯（DOC-CONV-001 统一口径，权威定义见 RELEASE_STATUS §0）

| 状态词 | 语义 | 判据（当前提交内可核） |
|---|---|---|
| `CONTRACT_READY` | 权威文档/合同/schema 冻结在位；文档/合同类对象的**终态** | 权威文档 + module.yaml/registry 条目 + 机器检查器 rc=0 |
| `IMPLEMENTED` | 生产（或注册测试面）源码在位，且**当前提交内实际执行通过** | 文件锚 + 命令 + rc=0（ctest/pytest 实测） |
| `INSTALLED` | 除 IMPLEMENTED 外，已进入构建安装树 + 产品清单，并可被 CLI/loader 实际发现 | install_layout + product manifest + `modules list/verify` 实测 |
| `VERIFIED` | 除 INSTALLED 外，已在正式平台（Windows x64）与真实数据上通过验收 | Fatduck/真实数据证据（当前无此项） |
| `NOT_IMPLEMENTED` | 能力不在当前基线（符号/路径不存在） | 全域 grep 零命中 |
| `NOT_VERIFIED` | 能力可能存在但当前提交未复跑执行验收 | 无当前提交证据 |
| `DEFERRED` | 任务图/负责人裁决明确不在本轮范围 | 控制包任务图条目 |
| `DORMANT` | 保留源码但不进生产构建/加载/发布 | CMake/preset 排除 + 隔离测试 |
| `FAIL` | 已执行但不符合要求 | 执行证据 + 不符合项 |

> 与 `docs/standards/STANDARDS_REGISTRY.md` §1.4 的**标准符合性**取值域
> （CONFORMANT/PARTIAL/NON_CONFORMANT/PROJECT_DEFINED）是两个独立轴：
> 前者描述"交付到什么程度"，后者描述"与外部标准条款的关系"，不互相替代。

## 4. 当前进度（Gate 与波次口径）

- 当前唯一 ACTIVE 控制包 = **ASTROCS-CONSTITUTION-ALIGNMENT-V1**
  （`工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/`，基线 789c5b6c）；
  其 GOV-001 已完成（宪章 FROZEN，ASTROCS-CONSTITUTION-001，四项负责人裁决 §18）。
- 本轮执行态：cprun run **`Rmtxvlrtfa66eb7` rev23**（V2 谱系 47 任务并入后按
  `06_V2_PACK_DESIGN_20260911.md` 六条线组织）；活动状态唯一登记源 =
  `工程控制/ACTIVITY_STATE.md`，任务状态唯一源 = 该包 `TASK_LEDGER.csv`
  （foreground 逐任务机器验收；Git 主历史只含前台集成提交）。
- 已并入 main 的收口提交（本轮收敛依赖）：`GOV-001` 宪章冻结 `d8c821db`、
  `STD-REG-001` 标准注册表 `fb7f232a`、`CI-BASELINE-001` `778fe98e`、
  `CI-001B` `a67bbc83`、`CI-REG-002` `d3097319`。
- 旧控制包线均已归档（V7 = ARCHIVED_DISARMED 于 42/140；V6.1/V8.1 =
  ARCHIVED_SUPERSEDED；V1.3–V6.1 已入 `engineering/control/archive/`），
  不得视为旧包延续执行。

## 5. 组件状态（状态词按 §3；证据列均指 BASE=`da3c4b4a`）

| 组件/面 | 状态 | 说明与证据（文件锚 / 命令 / rc） |
|---|---|---|
| 冻结宪章 / 工程约束 / 文档边界 / 版本单源 | `CONTRACT_READY` | `ASTROCS_PROJECT_CONSTITUTION.md` FROZEN（`d8c821db`，§18 四项裁决）+ `docs/DOCUMENT_INDEX.yaml` + 根 `VERSION`；doccheck 三检查器 + 标准注册表检查器本提交 rc=0 |
| 内核标准注册表（六域符合性 + 偏差指针） | `CONTRACT_READY` | `docs/standards/STANDARDS_REGISTRY.md` + `docs/standards/checks/check_standards_registry.py`（STANDARDS_REGISTRY_PASS，STD-REG-001 `fb7f232a`） |
| C ABI v1 / DLL 边界合同 | `CONTRACT_READY` | `include/astrocs/abi/*.h`（ABI-001）、`contracts/config/module_dll_contract.schema.json`（ARC-001）、`runtime/module_loader/secure_loader.h`（ABI-003 安全 loader） |
| 数据产物 / 三阶段交换 / 不确定度合同 | `CONTRACT_READY` | DATA-001/002 + DATA-UNC-001（`99713034`）+ `contracts/data/*` |
| **三 Phase 节点化（§F.1 每 DAG 节点唯一真实模块 operation）** | **`IMPLEMENTED`** | `lib/core/src/module_adapters.cpp`:4257 Phase1 八节点 / :4282 Phase2 七节点 / :4309 Phase3 五节点，全部经 `make_p1/p2/p3_node_module` 绑定唯一真实 operation（不再委托同一 `*_session_run`）；任务 P1-001 `9e09941a`、P2-001 `439f9f20`、P3-002 `1a56ffb7`；ctest `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty` 本提交实测 4/4 PASS |
| **RT 唯一 executor + 实测资源门** | **`IMPLEMENTED`** | `lib/core/src/executor_runtime.h` + `module_adapters.cpp`:138/:3777（P3 行带经 `rt::shared_work_executor` 租约）；`tools/monitoring/run_monitored.py` `evaluate_frozen_gate()` 按 §10.5/§18.2 冻结阈值；ctest `rt001_unique_executor` 本提交实测 PASS（RT-001 `91440c16`） |
| **Phase3 四投影 registry（TAN/SIN/CAR/AIT）** | **`IMPLEMENTED`** | `lib/phase3_proj/p3_projection.h` + `p3_projection.cpp`:267-273（registry v1 恰四行，`:299` 版本断言）；ALG 唯一权威 `docs/algorithms/PHASE3_PROJ_IMPL.md` §15；ctest `p3_projection_units`/`p3_projection_fault` 本提交实测 2/2 PASS；CI 显式检查项 `CTEST-P3-PROJECTION-UNITS/FAULT`（P3-001 `9953f103`）。**未 INSTALLED**：`lib/phase3_proj/module.yaml`:79-80 `module_status: CONTRACT_READY` / `entrypoint: MISSING`，生产会话 WCS 路径仍 TAN-only（`lib/phase3_session/p3_wcs.cpp`:36），DLL 挂载归 P3-PROJ-INT |
| **MOD 科学模块安装面 + 产品清单** | **`INSTALLED`** | `cmake/install_layout.cmake`:104-105 五科学模块入 `modules/`；`packaging/astrocs.product.json` units=10；`tests/abi/mod001_install_load_check.py` 本提交实测 **64/64 PASS**（含 sha256 篡改/module_id 错配/root 越界/文件缺失/删 DLL 五类负向注入必败）+ `build/cli/astrocs modules list --json` units=10 PASS（MOD-001 `59fdeab3`；p1_noise 摘出见 `f74fc20f`） |
| **CLI 薄命令面 validate/plan/inspect** | **`INSTALLED`** | `cli/parser.cpp` kRules 登记 phase1/2/3 × validate/plan/inspect 九条（+ 三条 run）；`build/cli/astrocs --help` 本提交实测列出全部；`tests/cli/test_cli001_vpi.py` 本提交实测 **15/15 PASS**（CLI-001 `026717fd`） |
| 三 Phase 隔离（独立命令；无进程内连跑） | `IMPLEMENTED` | kRules 已无 `run`/`graph`；本提交实测 `astrocs run --phases 1,2,3` → rc=2 `unknown command 'run'`（CLI-002 删除，`a6c39cc1` 收口）；跨 Phase 仅磁盘交换（DATA-002） |
| Phase1/2/3 科学链实现源码 | `IMPLEMENTED` | `lib/phase1_session`、`lib/phase2_session`、`lib/phase3_session` + 各模块库；Phase3 不确定度传播与扩展 HDU（`9662afa8`）经 `p3002_uncertainty` 实测 PASS |
| Phase3 `healpix_interp4` 四点插值 | `NOT_IMPLEMENTED` | `lib/`、`cli/`、`include/`、`runtime/` 全域 grep `interp4` 零实现符号（仅 `lib/phase3_rsmp/module.yaml` 未支持特性清单提及）；当前采样为 nearest/bilinear |
| Phase3 流式 FITS 输出接入 | `NOT_IMPLEMENTED` | IO-001 流式接口在位（`runtime/io/fits_core.c`），但 Phase3 writer 走 CFITSIO 原子写（`lib/phase3_session/p3_output.cpp`），未见流式写接线 |
| Windows 发布执行面（MSVC/32R/真实数据/DLL 安装树复验） | `NOT_VERIFIED` | Fatduck 侧执行（`FATDUCK_ACCESS.md`）；CI 侧 WIN-* 检查项已注册（CI-001/CI-001B），本提交未复验 |
| 真实数据（BASS/32R/接缝）最终验收 | `NOT_VERIFIED` | `docs/RELEASE_STATUS.md`：FINAL_REAL_DATA_VALIDATION=PENDING；REAL-000 `9f6b72b5` 已完成数据审计/索引 v1.2/确定性匹配计划（`IMPLEMENTED`，见 `artifacts/realdata/` 与 tests/realdata） |
| ACR | `DORMANT` | 根 `CMakeLists.txt`:17 `ASTROCS_ENABLE_ACR` 默认 OFF，生产 target 不链 `lib/acr`；保留源码与隔离测试，不加载不发布 |
| L0 负责人入口（本文件 + `docs/owner/`） | `CONTRACT_READY` | GOV-004 建立，DOC-CONV-001 收敛；`tools/check_l0_docs.py` 本提交实测 DOC-002_PASS（5 链接完整） |

## 6. 关键结论（每条可回溯）

- 产品版本单源 `0.11.0-alpha.2`（根 `VERSION`，GOV-003）；CLI `--version` 由生成链
  输出 `0.11.0-alpha.2+g<commit>`（生成链见 `docs/governance/VERSION_NAMESPACES.md`）。
- 科学定义=算法=接口=代码=测试全链闭合是目标（宪章）；本收敛为文档任务，
  `scientific_change=false`，未改任何公式/容差/冻结门/负责人裁决。
- 三 Phase 隔离是产品模型：独立命令 `phase1/2/3 run`（另有 validate/plan/inspect）；
  **遗留 `run --phases 1,2,3` 已删除**（实测 rc=2），跨 Phase 仅磁盘交换（DATA-002）。
- **每 DAG 节点唯一真实模块 operation（§F.1）已在三 Phase 达成**（P1 八节点 /
  P2 七节点 / P3 五节点），节点绑定表唯一源
  `runtime/pipeline/module_ports.registry.json`，本提交以 4 个节点化 ctest 实测复核。
- Phase3 投影：冻结四投影 **TAN/SIN/CAR/AIT**（宪章 §18.1）已在 registry v1 实现并
  通过独立 Oracle 与故障注入；生产会话路径与 DLL 挂载尚未切换到 registry
  （`entrypoint: MISSING`），**不得表述为已安装/已验证**。
- 科学模块安装面（Linux `.so` 技术预览）已达 `INSTALLED`：五科学模块 + noop 入安装树，
  产品清单 10 units，安全 loader 按签名清单加载（负向注入全部 fail-closed）。
- Windows x64 仍是正式平台：合同面冻结，**发布执行面未验收**（`NOT_VERIFIED`），
  不宣称"Windows 已交付"。
- ACR dormant：生产构建默认排除（preset ACR=OFF、根 CMake 不链 `lib/acr`），不加载不发布。
- 机器验证入口：`tools/doccheck/check_doc_index.py`（GOV-002）、
  `check_engineering_constraints.py`（GOV-001）、`check_version_namespaces.py`（GOV-003）、
  `docs/standards/checks/check_standards_registry.py`（STD-REG-001）、
  `tools/check_l0_docs.py`（DOC-002）；本轮全部 PASS（日志 `run/docconv001/logs/`）。

## 7. 待负责人/前台决策事项（节选）

1. Phase3 四投影的生产切换（registry→会话/DLL 挂载、`lib/phase3_proj/module.yaml`
   `entrypoint` 由 MISSING 升级）排期（P3-PROJ-INT 域）。
2. `healpix_interp4` 与 Phase3 流式 FITS 接入是否列入本轮范围（当前无实现）；
3. Windows DLL 化安装树复验与 32R/真实数据验收资源（Fatduck 执行面）；
4. 科学模块 `module.yaml` 是否随 P3-002/P1-001/P2-001 节点化统一登记
   `node_operations`（本任务写域外，见 `lib/*/README` 与 module.yaml 归属域）。
5. 文档侧机器锚：跨 L0 文档状态词一致性目前**无 CI 检查项**（本任务写域外，
   建议由 tools/+ci/ 域任务补 checker 与 `ci/checks.json` 显式项）。

## 8. 发布口径

本 Agent 无权宣布发布（宪章 §1.2/§H）；当前不满足 READY_FOR_OWNER_REVIEW 门槛，
如实标注 **NOT_READY_FOR_RELEASE**。

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_run: Rmtxvlrtfa66eb7 (rev23)
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540
