# 架构总览（Architecture Overview）

> 文档 ID：DOC-GOV-OWNER-ARCHITECTURE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003）
> 建立基线：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004，历史值）
> 收敛基线：DOC-CONV-001，BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`
> 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §B/C/D/F、`docs/contracts/ARCH-001.md`、
> `docs/architecture/*.md`、`docs/api/*`、`contracts/config/module_dll_contract.schema.json`。
> 状态词约定同 `docs/owner/RELEASE_STATUS.md` §0（DOC-CONV-001 唯一口径）。

## 1. 平台与发布形态（验收项：Windows 优先）

约束 §B 冻结：

1. **正式开发/客户端/发布平台 = Windows x64**；兼容下限 Windows 10 22H2 x64
   （build 19045），Windows 11 x64 为主验证环境。
2. Linux amd64 仅作常在线控制、静态分析、轻量编译、小合成实验节点；
   不得为 Linux 便利反向塑造 Windows 架构（§B.3）。
3. Windows 用户只面对 `astrocs.exe`；运行时、I/O、科学模块和 CPU provider
   作为 **DLL** 随包交付（§B.4）。
4. Linux 可产出同源 `astrocs` + `.so` 技术预览用于轻验证；Linux 性能不作为
   Windows 发布性能结论（§B.5）。
5. HiPS Browser 不注册为 CLI 插件、不进入本轮科学 DLL 列表（§B.6）；未来 GUI
   经稳定 CLI/JSON/退出码/产品文件调用（§B.7）。

### 现状核实

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| Windows 发布工具链 preset 冻结（VS17 2022 x64 / v143 14.44.35207 / SDK 26100 / CMake 3.31.12 / ACR OFF） | PASS | `CMakePresets.json`（BLD-001）+ `packaging/schemas/preset-contract.json` + `cmake/toolchain/verify_toolchain.py`（合同冻结） |
| 唯一根 CMake 产品构建图（唯一 `project(astrocs)`、唯一 `add_executable(astrocs)`、无 GLOB） | PASS | `CMakeLists.txt`（BLD-002）静态可核 |
| DLL 边界合同冻结（module_dll_contract schema：instances/forbidden/negative） | PASS | `contracts/config/module_dll_contract.schema.json`（ARC-001） || C ABI v1 冻结（4 纯 C 头：status_codes/host_api/module_api/artifact_api；`ACS_ABI_VERSION_V1=1u`） | PASS | `include/astrocs/abi/*.h`（ABI-001）+ `include/astrocs/common_abi_v1.h` |
| **科学模块安装面（Linux 技术预览：`modules/` + 产品清单 + 安全 loader）** | **`INSTALLED`** | `cmake/install_layout.cmake`:104-105（五科学模块 SHARED 入安装树 + `$ORIGIN` RPATH）；`packaging/astrocs.product.json` units=10；`tests/abi/mod001_install_load_check.py` 本提交实测 **64/64 PASS**（含 5 类负向注入必败）+ `astrocs modules list/verify` units=10 PASS（MOD-001 `59fdeab3`） |
| **Windows DLL 化发布安装树（astrocs.exe + runtime/io/模块/provider DLL）Windows 侧复验** | **`NOT_VERIFIED`** | 目标安装树定义于控制包 03 §4；Windows 侧未产出/复验 → 不宣称 Windows 已交付 |
| **Windows 上 MSVC 编译/测试/32R/真实数据验收** | **`NOT_VERIFIED`** | Fatduck 侧执行（约束 §E.5；FATDUCK_ACCESS.md）；CI WIN-* 检查项已注册（CI-001/CI-001B），本提交未复验 |
| Windows 兼容下限（Win10 22H2）验证 | `NOT_VERIFIED` | 同上 |
| Linux 同源 `astrocs` + `.so` 技术预览构建 | `IMPLEMENTED` | 本提交根 `ninja -C build` 实测 rc=0（全量 28 步）+ 安装树加载验证 64/64 PASS |

> Windows 优先是**目标与验收导向**：合同面（preset/ABI/DLL schema）已冻结
> （`CONTRACT_READY`）；Linux 技术预览安装面已 `INSTALLED`；
> Windows 执行面（MSVC 构建、Windows 安装树、32R/真实数据）未完成 →
> `NOT_VERIFIED`，不写"Windows 已实现/已交付"。

## 2. ACR 状态（验收项：ACR dormant）

约束 §C.1：ACR 是正式发布后的 CPU/GPU 异构更新；本轮**保留源码和隔离测试**，
但生产构建、加载、路由、benchmark、发布包**均不得依赖或包含 ACR/CUDA**。
当前唯一生产计算后端是纯 CPU（§C.2）。

| 项 | 状态 | 依据（当前提交内可核） |
|---|---|---|
| 根 CMake `ASTROCS_ENABLE_ACR` 默认 OFF 且不 add_subdirectory(lib/acr) | PASS | `CMakeLists.txt`：option 默认 OFF；仅 `tests/unit` add_subdirectory；message 显示 ACR 状态 |
| Windows preset 强制 `ASTROCS_ENABLE_ACR=OFF` | PASS | `CMakePresets.json` base-msvc/linux-control cacheVariables |
| CLI 链接图不含 ACR（`astrocs` target 链接表无 lib/acr 源） | PASS | `CMakeLists.txt` add_executable 链接清单静态可核（无 acr target） |
| `lib/acr` 保留（源码 + 独立 CMake + tests/qualification/ci） | PASS | `lib/acr/` 目录与 CMakeLists 在位（SA-ACR-13 域） |
| lib/phase2 的 `acr_kernels.cpp`/`cuda_bridge_stub.cpp` 编译进生产模块 | PASS（如实记录） | 根 CMake astrocs_phase2 源含 `cuda_bridge_stub.cpp`（stub）；`lib/phase2/src/acr_kernels.cpp` 不在根 CMake 源表（LEG-004 注释）；约束禁止 ACR/CUDA 进入生产构建，stub 是否存在运行时 CUDA 依赖属他人路径审计项，本文不裁定 |
| ACR 不进入 benchmark/发布包 | PASS（合同）+ NOT_VERIFIED（执行） | 合同面由约束 §C.1 与 preset 保证；benchmark 不含 ACR 的执行证据未在当前提交复跑 |

结论：**ACR = DORMANT**（保留源码/隔离测试；生产构建默认排除；不加载不发布）。
生产构建不含 ACR/CUDA 的机器级验证（如符号/link 检查）属 ACR-00x/QA 域。

## 3. 运行时与资源（RT-001 / 约束 §D）

| 项 | 状态 | 依据 |
|---|---|---|
| 唯一生产 Runtime（`astrocs_core`）：load_pipeline→run→cancel→inspect | PASS（源码在位） | `include/astrocs/core/runtime.h`、`lib/core/src/runtime.cpp`、`cli/runtime_client.cpp` 静态可核 |
| 类型化运行图合同（typed DAG schema/validator/registry） | PASS（合同冻结） | `runtime/pipeline/typed_dag.py` + `typed_dag.schema.json` + `module_ports.registry.json`（RT-001，pytest 24/24 已在集成提交验收） |
| 模块注册表（register_phase_modules：P1/P2/P3 模块族 + factory） | `IMPLEMENTED` | `lib/core/src/module_adapters.cpp`:4257/:4282/:4309 —— P1 八节点 / P2 七节点 / P3 五节点各经 `make_p1/p2/p3_node_module` 绑定**唯一真实 operation**（约束 §F.1 已达成）；ctest `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty` 本提交实测 4/4 PASS |
| 唯一 executor + 实测资源门（RT-001） | `IMPLEMENTED` | `lib/core/src/executor_runtime.h`（进程唯一池注册点）+ `module_adapters.cpp`:3777-3793（P3 行带经租约提交）+ `tools/monitoring/run_monitored.py` `evaluate_frozen_gate()`；ctest `rt001_unique_executor` 本提交实测 PASS（`91440c16`） |
| ThreadBudget/ThreadLease 合同 | PASS（合同冻结） | `include/astrocs/core/context.h`、RT-001.md §2.4；RT-002 budget 测试在 tests/unit |
| 模块不得私建永久线程池/硬编码核数 | `CONTRACT_READY`（合同）+ `NOT_VERIFIED`（全域扫描） | 约束 §D.3；RT-001 已把 Phase3 行带自建线程池改为唯一 executor 提交（实测）；其余模块的全域静态扫描属 W5/LNX 域，未在本提交复跑 |
| 遗留 drizzle/calibration OpenMP pragma 与 `aio_pipeline` 5-stage 调度 | 保留中（如实） | 根 CMake 注释：遗留模块 omp pragma target-local 编译；ARCH-001.md §7 登记 `aio_pipeline_engine` 越权编排为已知现状差距（LEG-003 迁移），不宣称已删除 |

## 4. 依赖方向与边界（ARCH-001 §3）

冻结依赖方向：`cli → runtime → registry → modules → cpu_backend`；
`io → data_contracts`；`io ⇏ runtime`；`io ⇏ modules`；禁反向依赖与
`file(GLOB)` 隐式塞目录。

现状：
- 根 CMake 显式 target 依赖图（BLD-002）使构建边可静态核；`astrocs_io` 依赖
  `astrocs_core`（CMake 注释：IO-001 定 core <- io 依赖方向）——该方向与
  ARCH-001 §3（io 不依赖 runtime/core）存在表述差异，属架构域待审项，本文如实记录。
- DLL 边界、C ABI、所有权规则以 `docs/standards/{C_ABI_STANDARD,API_STANDARD,
  CONCURRENCY_STANDARD,ERROR_HANDLING_STANDARD}.md` 与 `docs/api/COMMON_ABI_V1.md` 为准
  （合同冻结 PASS）。

## 5. 关键契约文件索引（负责人可直达）

| 契约 | 路径 | 状态 |
|---|---|---|
| 工程约束（根权威） | `AstroCS_ENGINEERING_CONSTRAINTS.md` | ACTIVE_NORMATIVE |
| Runtime/模块公共合同 | `docs/contracts/RT-001.md` | ACTIVE_NORMATIVE |
| 架构职责边界 | `docs/contracts/ARCH-001.md` | ACTIVE_NORMATIVE |
| 数据语义/产物 | `docs/contracts/DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md` | ACTIVE_NORMATIVE |
| CLI 协议 | `docs/api/CLI_PROTOCOL_V1.md` | ACTIVE_NORMATIVE |
| Phase API | `docs/api/PHASE{1,2,3}_API_V1.md` | ACTIVE_NORMATIVE |
| 版本命名空间 | `docs/governance/VERSION_NAMESPACES.md` | ACTIVE_NORMATIVE |
| DLL 边界 schema | `contracts/config/module_dll_contract.schema.json` | 机器 schema |

## 6. 架构状态汇总

```text
Windows 优先目标/合同:      CONTRACT_READY（preset + ACR OFF + 唯一根 CMake + ABI v1 + DLL schema）
Linux 技术预览安装面:       INSTALLED（5 科学模块 + noop / 10 units / 安全 loader 64/64 实测）
Windows 发布执行面:          NOT_VERIFIED（Windows 安装树/MSVC/32R/真实数据未在本提交验证）
ACR:                        DORMANT（保留源码隔离测试；生产构建排除；不加载不发布）
唯一 Runtime/typed DAG:     IMPLEMENTED（合同 + 源码 + 节点化/executor ctest 实测）
每节点唯一 operation（§F.1）: IMPLEMENTED（P1 8 / P2 7 / P3 5 节点，实测）
遗留 run --phases 连跑:      已删除 IMPLEMENTED（实测 rc=2 unknown command）
旧 aio_pipeline 越权编排:    保留中（ARCH-001 §7 登记，LEG-003 迁移；不宣称已删除）
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540
