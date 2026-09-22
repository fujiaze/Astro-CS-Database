# 发布状态（Release Status）

> 上游：ASTROCS_DESIGN.md §12.5（状态阶梯）、§13（版本与发布权）

> 最终发布裁定只属项目负责人（`ASTROCS_DESIGN.md` §12；原引「宪章 §1.2/§H」已废止），本 Agent 至多声明
> READY_FOR_OWNER_REVIEW，不替代批准。

## 0. 状态词阶梯（`ASTROCS_DESIGN.md` §12.5 的操作层判据）

> 本节展开 `ASTROCS_DESIGN.md` §12.5（状态阶梯）的**操作层判据**；其它登记表/映射表
> 只引用 §12.5 与本表，不得复述或改写状态词清单。

| 状态词 | 语义 | 判据（当前提交内可核） |
|---|---|---|
| `CONTRACT_READY` | 权威文档/合同/schema 冻结在位；**文档与合同类对象以此为终态** | 权威文档 + `module.yaml`/registry 条目 + 机器检查器 rc=0 |
| `IMPLEMENTED` | 生产（或注册测试面）源码在位，且**当前提交内实际执行通过** | 文件锚 + 命令 + rc=0（ctest/pytest 实测） |
| `INSTALLED` | 除 IMPLEMENTED 外，已进入构建安装树 + 产品清单，并可被 CLI/loader 实际发现 | `eng/cmake/install_layout.cmake` + `eng/packaging/astrocs.product.json` + `modules list/verify` 实测 |
| `VERIFIED` | 除 INSTALLED 外，已在正式平台（Windows x64）与真实数据上通过验收 | Fatduck/真实数据证据（**当前无此项**） |
| `NOT_IMPLEMENTED` | 能力不在当前基线（符号/路径不存在） | 全域 grep 零命中 |
| `NOT_VERIFIED` | 能力可能存在但当前提交未复跑执行验收 | 无当前提交证据 |
| `DEFERRED` | 任务图/负责人裁决明确不在本轮范围 | 控制包任务图条目 |
| `DORMANT` | 保留源码但**不**进生产构建/加载/发布 | CMake/preset 排除 + 隔离测试 |
| `FAIL` | 已执行但不符合要求 | 执行证据 + 不符合项 |

> 与 `docs/standards/STANDARDS_REGISTRY.md` §1.4 的**标准符合性**取值域
> （`CONFORMANT`/`PARTIAL`/`NON_CONFORMANT`/`PROJECT_DEFINED`）是两个独立轴：
> 本表描述"交付到什么程度"，注册表描述"与外部标准条款的关系"，不互相替代。
> 历史口径 `PASS`（合同冻结/源码在位/执行验收三级）自 DOC-CONV-001 起由本表取代，
> 各文档统一改用本表状态词。

## 1. 一句话结论

```text
架构收敛主体已完成：合同面（权威文档集[原冻结宪章已删除]/版本单源/文档边界/ABI v1/数据产物/Runtime 图/
工具链 preset/DLL schema/FITS 流接口/内核标准注册表）冻结在位；三 Phase 节点化、
Phase3 四投影 registry、RT 唯一 executor + 实测资源门、MOD 科学模块安装面、
CLI validate/plan/inspect 薄命令面均已落地（当前提交实测绿）；遗留 run --phases
连跑已删除。未完成：Windows 发布执行面与真实数据验收、healpix_interp4、
Phase3 流式 FITS 接入 → 当前状态 NOT_READY_FOR_RELEASE，而非 READY_FOR_OWNER_REVIEW。
```

## 1a. 发布结论（当前）

~~~text
发布结论:            NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）
真实数据面:          NOT_VERIFIED（FINAL_REAL_DATA_VALIDATION 未达成）
Windows x64 复验面:  NOT_VERIFIED（VERIFIED 要求正式平台 + 真实数据验收通过）
ACR（CPU/GPU 异构）: DORMANT（不进生产构建/加载/路由/发布）
psf_snr_power:       DEFERRED（生产拒绝）
~~~

- 逐面状态与证据锚以本文（§0 词表 + 分面表）与 `docs/modules/MODULE_MAP.yaml` 为准。
- **合成测试或历史可用节点不等于真实数据 / Windows `VERIFIED`**（`ASTROCS_DESIGN.md` §12.5 末条）。
- 最终发布决定只属项目负责人；Agent 至多声明 `READY_FOR_OWNER_REVIEW`（`ASTROCS_DESIGN.md` §12）。Alpha 前程序/代码/产物内不存在版本信息（§12）；根 `VERSION` 仅为内部助记符，不进入程序与发布产物（`ENGINEERING_SPEC.md` §7）。

## 2. 版本与发布面

- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.2`，内容一行 `MAJOR.MINOR.PATCH-alpha.N`（禁 stable/rc/beta）。
- **版本命名空间**（生命周期独立，禁止跨命名空间借用号段）：

| 命名空间 | 权威定义点 | 当前值 | 递增规则 / 生命周期 |
|---|---|---|---|
| product | 仓库根 `VERSION`（唯一事实源） | `0.11.0-alpha.2` | `MAJOR.MINOR.PATCH` 只由负责人指令变更，`alpha.N` 只在外部审核通过后提升 |
| module | 各模块 `module.yaml` 的 `module_version` | 逐模块独立（以 manifest 为准） | 模块接口/产物变更时由模块 owner 递增，与产品版本无关 |
| ABI | `lib/include/astrocs/common_abi_v1.h` 的 `ACS_ABI_VERSION_V1` 与 CLI 暴露的 `abi_version` | ABI v1（头常量 `1u`） | 任何破坏二进制兼容的变更必须递增 |
| data-schema | `eng/contracts/data/artifact_types.registry.json` 的 `schema_version` 与各 schema 文件的 `$schema` 版本 | type_id schema_version = 1 | 数据产品结构变更时按 registry 递增 |
| doc-revision | 治理/规范文档自身的修订号 | 逐文档独立 | 文档内容修订时递增；与产品版本无换算关系 |
| history（历史工程轮次） | 只允许出现在 `git log` 与 `docs/**/v6/**`（产品族设计档案） | — | 不携带可递增的"当前值"；禁止把旧轮次数字冒充当前产品状态 |

- 非产品版本的数字三元组不得当作产品版本，也不得反向手抄：FITS 4.0（格式规范）、HiPS 1.0/1.4（IVOA 格式版本）、DatabaseVersion（Gaia 库标识）、`schema_version` / ABI v1（见上表定义点）、外部组件版本（CFITSIO 4.6.4、gcc/cmake 等）、`X.Y.Z` / `MAJOR.MINOR.PATCH` 占位表述。
- **生成链**（CMake/CLI/打包从根 `VERSION` 派生，禁止手抄字面量）：根 `CMakeLists.txt` 的 `file(READ …/VERSION)` + `git rev-parse HEAD` → `ASTROCS_VERSION_STRING`（`X.Y.Z-alpha.N+g<sha>`）→ `configure_file` 生成 `lib/infrastructure/cli/version_generated.h`；`eng/tools/gen_version.py --json` 输出 version/prerelease/commit/dirty/build_id/abi_version/cli_schema_version 合同对象；CLI `--version[ --json]` 与 doctor/hardware/verify 共用同一生成串。
- 生成串形态：clean main 为 `0.11.0-alpha.2+g<commit12>`，dirty 工作树追加 `.dirty`。
- **机器检查入口**：`eng/tools/doccheck/check_version_namespaces.py`（唯一源格式 + 生成链 + 允许路径扫描 + 反误报断言 + 伪造版本必须判红）；既有版本扫描 `eng/tools/check_version_consistency.py`。
- Windows 正式发布候选：**未产生**（`NOT_VERIFIED`）。DLL 化安装树的 **Linux 技术预览安装面已 `INSTALLED`**（五科学模块 + noop 入 `modules/`，产品清单 10 units，安全 loader 实测 64/64 PASS），Windows 侧复验未执行。
- 已知他人路径遗留：`docs/VERSIONING.md`、CMake `project(... VERSION)` 字面量、若干 eng/tests/tools 硬编码旧版本号 —— 由版本检查器 `known_legacy_reported` 输出登记（见检查器 `out_of_scope` 列表）。

## 3. 冻结与落地面清单（状态词见 §0；证据指 BASE=`da3c4b4a`）

| 面 | 状态 | 主要依据（文件锚 / 命令 / rc） |
|---|---|---|
| 最高设计 | `CONTRACT_READY` | `ASTROCS_DESIGN.md`（§0 权威链，唯一最高权威） |
| 最高设计 ↔ 工程规范边界 | `CONTRACT_READY` | `ASTROCS_DESIGN.md` §0 权威链 + `ENGINEERING_SPEC.md`；旧 `AstroCS_ENGINEERING_CONSTRAINTS.md` 与 `eng/tools/doccheck/check_engineering_constraints.py` 已随 ROOT-007/RETIRE-001 退役（历史条目） |
| 文档边界/索引 | `CONTRACT_READY` | `docs/DOCUMENT_INDEX.yaml`（DOC-001 收敛：新文档集补登 + 旧体系移出活动区）；`eng/tools/doccheck/check_doc_index.py --strict` 残留 1 项 `control_archive_dir_readme`（绑定 ROOT-007 已删除的 `engineering/control/archive/**`），登记 CI-001 迁移 |
| 内核标准注册表 | `CONTRACT_READY` | `docs/standards/STANDARDS_REGISTRY.md` + `docs/standards/checks/check_standards_registry.py` → STANDARDS_REGISTRY_PASS（STD-REG-001 `fb7f232a`） |
| 版本单源 | `CONTRACT_READY` | `VERSION` + 本文 §2；检查器 rc=0（GOV-003） |
| C ABI v1 / DLL 边界 / 安全 loader 合同 | `CONTRACT_READY` | `lib/include/astrocs/abi/*.h`（ABI-001）、`eng/contracts/config/module_dll_contract.schema.json`（ARC-001）、`runtime/module_loader/secure_loader.h`（ABI-003） |
| 类型化产物 / 三阶段交换 / 不确定度合同 | `CONTRACT_READY` | DATA-001/002 + DATA-UNC-001（`99713034`）+ `eng/contracts/data/*` |
| Runtime 类型化运行图 + 节点绑定表 | `IMPLEMENTED` | `runtime/pipeline/typed_dag.py` + `module_ports.registry.json`；节点绑定经 ctest 节点化用例复核 |
| 三 Phase 节点化（`ASTROCS_DESIGN.md` §3.2/§4.2/§5.2 每节点唯一真实 operation；原引「宪章 §F.1」已废止） | `IMPLEMENTED` | `lib/infrastructure/scheduler/src/module_adapters.cpp`:4257/:4282/:4309（P1 8 / P2 7 / P3 5 节点）；ctest `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty` 4/4 PASS |
| RT 唯一 executor + 实测资源门（`ASTROCS_DESIGN.md` §8/§10 + `ENGINEERING_SPEC.md` §10；原引「宪章 §10.4/§10.5/§18.2」已废止） | `IMPLEMENTED` | `lib/infrastructure/scheduler/src/executor_runtime.h`、`module_adapters.cpp`:3777-3793、`eng/tools/monitoring/run_monitored.py:evaluate_frozen_gate()`；ctest `rt001_unique_executor` PASS（RT-001 `91440c16`） |
| Phase3 四投影 registry（TAN/SIN/CAR/AIT） | `IMPLEMENTED` | `lib/algorithms/projection/p3_projection.{h,cpp}`:267-273（registry v1 恰四行）；ctest `p3_projection_units`/`p3_projection_fault` 2/2 PASS；CI `CTEST-P3-PROJECTION-UNITS/FAULT` |
| MOD 科学模块安装面 + 产品清单 | `INSTALLED` | `eng/cmake/install_layout.cmake`:104-105；`eng/packaging/astrocs.product.json` units=10；`eng/tests/abi/mod001_install_load_check.py` 64/64 PASS（MOD-001 `59fdeab3`；`f74fc20f` 摘出 p1_noise） |
| CLI 薄命令面（validate/plan/inspect） | `INSTALLED` | `cli/parser.cpp` kRules（9 条新命令）；`build/cli/astrocs --help` 实测；`eng/tests/cli/test_cli001_vpi.py` 15/15 PASS（CLI-001 `026717fd`） |
| 三 Phase 隔离（三个独立命令，一次调用只驱动一个阶段） | `IMPLEMENTED` | `normalize`/`mosaic`/`export` 各拉起本阶段调度器（`ASTROCS_DESIGN.md` §1.2/§8.1）；`run --phases 1,2,3` → rc=2 `unknown command 'run'`（CLI-002）；DATA-002 磁盘交换合同冻结 |
| 结构化日志合同 | `CONTRACT_READY` | LOG-001（schema/JSONL 契约） |
| Windows 工具链 preset | `CONTRACT_READY` | BLD-001 + `eng/packaging/schemas/preset-contract.json` |
| 唯一根 CMake 构建图 | `IMPLEMENTED` | BLD-002；根 `ninja -C build` 本提交实测 rc=0（全量 28 步） |
| FITS 流式接口 | `IMPLEMENTED` | IO-001（接口 + 实现 + 契约测试）；**未接入 Phase3 writer**（见 §4） |
| L0 负责人入口 | `CONTRACT_READY` | `docs/owner/*` + `docs/DOCUMENT_INDEX.yaml`（根 `REVIEW.md` 已由 ROOT-007 删除，旧轮次评审副本已由 CLEAN-402 删除）；`eng/tools/check_l0_docs.py` 现行绑定 `docs/owner/**`（DOC-001 后实测 rc=0，GAP-001/GAP-016 关闭）|

> 本表"实测"级证据（IMPLEMENTED/INSTALLED）全部来自 BASE=`da3c4b4a` 的命令日志
> `run/docconv001/logs/{focused_rebuild_test.log,mod001_install_check.log,cli001_vpi.log}`；
> 未在本提交复跑的面一律不写 IMPLEMENTED/INSTALLED。

## 4. 未完成 / 未验证面（`NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED`）

| 面 | 状态 | 说明 |
|---|---|---|
| Windows DLL 化发布安装树（Windows 侧复验） | `NOT_VERIFIED` | Linux 技术预览安装面已达 INSTALLED；Windows 侧未产出/复验 |
| Windows MSVC 编译 + 测试（Win10 22H2 下限 / Win11 主验证） | `NOT_VERIFIED` | Fatduck 侧执行，未完成；CI WIN-* 检查项已注册（CI-001/CI-001B） |
| 真实数据（BASS/32R/接缝）最终验收 | `NOT_VERIFIED` | 本文 §1a：FINAL_REAL_DATA_VALIDATION=PENDING；REAL-000 `9f6b72b5` 数据审计/索引 v1.2/确定性匹配计划已 IMPLEMENTED |
| 32R 单线程重计算禁令的执行证据 | `NOT_VERIFIED` | 资源门实现面已 IMPLEMENTED（RT-001）；正式平台执行证据属 Windows 域 |
| Phase3 四投影 DLL 挂载/生产会话切换 | `NOT_IMPLEMENTED` | registry 实现已 IMPLEMENTED，但 `lib/algorithms/projection/module.yaml`:79-80 `entrypoint: MISSING`，生产 WCS 路径仍 TAN-only（`lib/algorithms/projection/p3_wcs.cpp`，W4-A9 批次 1 迁入） |
| Phase3 `healpix_interp4` | `NOT_IMPLEMENTED` | lib/cli/include/runtime 全域无 `interp4` 实现符号；当前 nearest/bilinear |
| Phase3 流式 FITS 输出接入 | `NOT_IMPLEMENTED` | IO-001 接口在位；Phase3 writer 走 CFITSIO 原子写（`lib/algorithms/fits_output/p3_output.cpp`） |
| 顶层占位 descriptor `astrocs.phase3.resample` | `DEFERRED` | P2 模板复制残留，归 P3-RSMP-INT（`lib/algorithms/resample/README.md` 登记） |
| 旧 `aio_pipeline_engine` 越权编排 | 保留中（`DEFERRED`） | `lib/infrastructure/aio/src/aio_pipeline_engine.cpp` 仍在位，ARCH-001 §7 登记为已知现状差距（LEG-003 迁移）；不宣称已删除 |
| ACR | `DORMANT` | 根 `CMakeLists.txt`:17 ACR 默认 OFF；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA |

## 5. 发布 Gate 口径（参考控制包 02_GATES_AND_EXECUTION.md / 03_AUDIT_PACKAGE_SPEC.md）

- 当前执行态：cprun run `Rmtxvlrtfa66eb7` rev23；门禁链
  G-STD→G-SCI→G-CODE→G-CI-L1→G-REAL-L2→G-FAT-L3→G-RELEASE。
- 达到 `READY_FOR_OWNER_REVIEW` 仍需：Windows 正式验证（G-FAT-L3）、
  真实数据终验（G-REAL-L2）、文档质量收敛、独立终审（G-RELEASE）；**当前不满足**，
  如实标注 `NOT_READY_FOR_RELEASE`。
- 资源门口径（负责人既定原则，CI-001/CI-REPAIR-001 裁决）：构建/打包/单测为
  **非重计算面**，冻结阈值语义针对重计算区间；重计算面必须显式请求
  `--gate-required` 并附判定证据，缺失即 fail-closed（原引「宪章 §10.5/§18.2」已废止；现行 = `ASTROCS_DESIGN.md` §8 + `eng/contracts/resource_gate_v1.json`；阈值不被放宽）。

## 6. 状态汇总

```text
冻结/合同面:    CONTRACT_READY（权威文档集/索引/版本/ABI/DLL schema/数据合同/标准注册表）
节点化与运行时: IMPLEMENTED（三 Phase 节点化 `ASTROCS_DESIGN.md` §3.2/§4.2/§5.2、RT 唯一 executor + 资源门）
Phase3 投影:    IMPLEMENTED（TAN/SIN/CAR/AIT registry v1 + Oracle + 故障注入）
安装面:         INSTALLED（Linux 技术预览：5 科学模块 + noop / 10 units / 安全 loader）
CLI 命令面:     INSTALLED（phase1/2/3 × validate|plan|inspect；run --phases 已删除）
发布执行面:     NOT_VERIFIED（Windows MSVC/DLL 安装树复验/32R）
真实数据面:     NOT_VERIFIED（FINAL_REAL_DATA_VALIDATION=PENDING）
科学扩展面:     NOT_IMPLEMENTED（healpix_interp4、Phase3 流式 FITS 接入）
架构遗留:       保留中（aio_pipeline_engine / 顶层 phase3.resample 占位 descriptor）
发布结论:       NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_run: Rmtxvlrtfa66eb7 (rev23)
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540
