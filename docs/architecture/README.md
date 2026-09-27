# architecture

本目录是软件架构层：把最高设计 §8 落实为可执行的结构规则，覆盖总览、执行与线程、I/O 与错误、CPU 后端与性能模型，并附机读清单。

## 职责边界

- 放：架构总览与分域设计文档、架构合同、ISA/CPU 后端台账、机读清单（API/执行/调用路径清单）。
- 不放：科学公式（在 docs/science/）；接口合同正文（在 docs/api/、docs/interfaces/）；模块代码说明（在 docs/modules/）；子域细节（在 abi/、cpu/、observability/ 三个子目录）。

## 内容

- `ARCHITECTURE.md` —— 架构总览：单一 CLI 入口与三阶段调度器结构。
- `PIPELINE.md` —— 管线结构（阶段间落盘交换）。
- `MODULE_MAP.md` —— 模块地图与依赖边界。
- `PHASE3_MODULE_ARCH.md` —— export 阶段模块架构（reader → WCS → resampler → writer）。
- `DATA_FLOW.md` / `BUILD_GRAPH.md` / `DEPENDENCY_RULES.md` —— 数据流、构建图与依赖规则。
- `EXECUTION_MODEL.md` / `THREADING_MODEL.md` / `THREAD_BUDGET_ARCH.md` —— 执行与生命周期模型、线程模型、全局线程预算。
- `execution_options_contract.md` —— 全局 worker 预算合同（ExecutionOptions）。
- `OWNERSHIP_AND_LIFETIME.md` / `ASYNC_IO_CONTRACT.md` —— 所有权与生命周期、有界异步 I/O 合同。
- `IO_AND_ATOMICITY.md` / `ERROR_MODEL.md` —— I/O 与原子性、错误模型。
- `CACHE_POLICY.md` / `COMPATIBILITY_POLICY.md` —— 缓存与兼容性策略。
- `CPU_BACKEND_ARCH.md` / `ISA_VARIANTS.md` / `ISA_BIT_MANIP_VARIANTS.md` —— CPU 后端架构与 ISA 变体台账。
- `PERFORMANCE_MODEL.md` —— 性能模型。

## 机读清单与子目录

- `api_inventory.csv` / `execution_inventory.csv` —— API 面与执行面的机读清单。
- `production_call_paths_stage1.csv` / `production_call_paths_stage2.csv` / `PRODUCTION_EXECUTION_INVENTORY.csv` —— 生产调用路径与执行清单。
- `doc_symbol_namespaces.json` —— 文档符号命名空间登记。
- `abi/`、`cpu/`、`observability/` —— 安全加载器、CPU 能力与后端、可观测性三个子域。

## 上游

上游：docs/ASTROCS_DESIGN.md §8（软件架构）、§8.1（顶层结构）、§8.4（模块与 ABI）、§9（CPU 后端与资源）。
