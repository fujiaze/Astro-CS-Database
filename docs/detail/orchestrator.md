# Module: orchestrator

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

## 职责

Phase1 编排：stage1.json 驱动 READ/CALIBRATE/STAR/PSF/PLATESOLVE/
PHOTOMETRIC/NOISE/DRIZZLE/HIPS_WRITE，DllLoader 动态加载模块 DLL。

## 非职责

不实现科学算法。

## Public API

orchestrator.exe <stage1.json>；模块头文件契约。

## Data contract

stage1.json（输入/校准/输出/参数）；运行产物 run/。

## Ownership

模块句柄生命周期由 orchestrator 管理。

## Thread safety

stage 顺序；模块内 OpenMP。

## Errors

模块加载失败/阶段失败 → 显式 exit code + 日志。

## Config

stage1 JSON schema（configs/stage1.schema.json）。

## Science IDs

全链（阶段委托模块）。

## 性能特征

资源监控（resource_monitor）；spill/checkpoint。

## Diagnostics

阶段日志 run/logs/orchestrator/；已知 bug：cpp/ 下嵌套 logs 目录（非阻断）。

## Tests

单帧端到端验证；DLL smoke。

## Source files

lib/infrastructure/pipeline/orchestrator/cpp/。

## 归属与构建（ORCH-001 落位）

- **§7.1 目标家（职责家）= `lib/infrastructure/scheduler/**`**：`ASTROCS_DESIGN.md:656`
  目录树 scheduler/ 行，与本模块职责逐条对应 ——
  注册 = `cpp/src/dll_loader.cpp`（模块动态加载 + 函数指针注册表）；
  资源预算 = `cpp/include/admission_controller.h` + `cpp/include/resource_monitor.h`；
  执行 = `cpp/src/orchestrator.cpp` 的 `run_stage_*` 与阶段表；
  取消 = `request_cancel()` / SIGINT 原子 token（`ASTROCS_CANCELLED`）；
  checkpoint = `cpp/src/checkpoint.cpp`。
  `ASTROCS_DESIGN.md` §8.4 顶层结构里的 pipeline 位（typed DAG、块生命周期、内存/数据管线）在本仓的
  代码实体是 `lib/infrastructure/scheduler/src/{pipeline,artifact,artifact_store}.cpp` 与
  `lib/infrastructure/runtime/**`（MODULE_MAP id=`runtime`），**不是**本模块。
- **物理位 = `lib/infrastructure/pipeline/orchestrator/**`**：该目录承载编排层实现，对应顶层设计
  §8.4 的「infrastructure/pipeline（typed DAG 编排）」位。
- ⇒ **位置与职责分离**：归属一律按职责判定，不按目录名推断 —— 本模块 = 编排层；
  typed DAG 与运行时实体 = `lib/infrastructure/scheduler/**` 与 `lib/infrastructure/runtime/**`。
  检查器、清单与文档的归属判据同此口径。
- **构建 target**：
  `astrocs_infra_orchestrator`（静态库；`cpp/CMakeLists.txt` 声明，根 `CMakeLists.txt`
  经 `add_subdirectory` 注册），vendored json-schema-validator 独立为
  `astrocs_orchestrator_jsv`；入口可执行 `orchestrator_legacy_cli` 为**非产品**
  （不进 install 白名单、不进产品 manifest；`ASTROCS_DESIGN §6.2` 的唯一命令树仍是产品
  `acsd`）。共址测试 6 条 ctest（`cpp/tests/CMakeLists.txt`）：logger 单测、checkpoint
  单测、CLI 集成、`orchestrator_legacy_cli` 冒烟 ×2、可执行级饱和接线门。

## 构建/契约锚点

C++17 (`-std=c++17`, 见 `lib/infrastructure/pipeline/orchestrator/cpp/Makefile:CXXFLAGS`；
正式构建入口 = 根 CMake 的 `astrocs_infra_orchestrator`)；C ABI 经 `DllLoader`
纯 C 调用（`docs/engineering/C_ABI_STANDARD.md`）；退出码与 `docs/engineering/ERROR_MODEL.md`
全集合一致（`eng/tools/docs_machine_consistency.py` 校验）。
