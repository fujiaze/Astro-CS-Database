# ADR-002: oneTBB v2022.0.0 CPU 任务执行运行时

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | oneTBB v2022.0.0（commit `v2022.0.0`） |
| 许可证 | Apache-2.0 |
| 平台 | Windows / Linux |
| 形态 | 编译型库（非 header-only） |

## 状态

Accepted。oneTBB 作为 ACR CPU 端任务执行运行时，承担 worker pool、工作窃取、`task_arena` 隔离与并发限制职责。GPU 任务调度不归 oneTBB。

## 背景

ACR 在 CPU 侧需要执行大量异构任务：标定微基准、路由计算、profile 序列化、ISA 探测。这些任务长短不一、嵌套存在，且需要在 95% CPU 占用节流模式下与 GPU 任务并存。直接使用 `std::thread` 缺乏工作窃取，会导致负载不均；自研线程池违反"复用成熟库"原则；OpenMP 虽在仓库现有模块中已使用，但其并行区域语义不利于 arena 隔离与精细并发控制。

`global_control` 可设并发上限，但需明确：**并发上限 ≠ CPU 利用率%**。占用率节流由 ACR 上层监测 hwloc/OS 计数器后调整 `global_control`，而非 oneTBB 直接报告利用率。

oneTBB 主版本间存在 ABI break，必须钉死 v2022.0.0。

## 决策

1. 引入 oneTBB v2022.0.0 作为 ACR CPU 任务执行运行时。
2. 建立单一受控的 ACR arena（`tbb::task_arena`），所有 ACR CPU 任务经此调度。
3. 使用 `tbb::global_control` 限制最大并发，按节流策略动态调整。
4. 与仓库现有 OpenMP 模块**并存**：ACR 不修改既有 OpenMP 代码，OpenMP 不进入 ACR arena。
5. oneTBB 类型不得出现在 ACR 公共 API 签名中。

## 理由

- 工作窃取天然适配长短任务混排，避免长任务阻塞短任务队列。
- `task_arena` 提供隔离边界，ACR arena 与外部 OpenMP 区域互不干扰。
- `global_control` 提供运行时并发调整能力，无需停线程、无需重建池。
- v2022.0.0 在 MSYS2/Windows 与 Linux 下均稳定，文档与示例充分。

## 集成边界

- **职责内**：CPU worker pool、工作窃取、`task_arena` 隔离、并发上限动态调整（通过 `global_control`）。
- **职责外**：GPU 任务调度（由 alpaka 队列负责）、ISA 发现（由 ADR-004 cpu_features 负责）、CPU 利用率%上报（由 ACR 监测层基于 hwloc/OS 计数器计算，oneTBB 只是被调节对象）。
- **API 边界**：`tbb::task_arena`、`tbb::task_group`、`tbb::global_control` 不得出现在公共头文件接口签名。
- **共存边界**：ACR 编译单元启用 oneTBB；既有 OpenMP 模块保持原状；同一编译单元禁止混用 oneTBB 与 OpenMP pragma。
- **节流边界**：95% 占用节流由 ACR 监测层决策，oneTBB 仅作为执行器接受 `global_control` 参数。

## 替代方案

1. **`std::thread` + 自管队列**：
   - 未采用：无工作窃取，长任务会阻塞整个队列；嵌套并行语义弱。
2. **自研线程池**：
   - 未采用：违反复用原则；工作窃取、取消、异常传播需自行实现，验证成本高。
3. **OpenMP（统一为单一并行栈）**：
   - 未采用：仓库现有模块已用 OpenMP，强制迁移风险大；OpenMP 的 parallel region 与 `task_arena` 隔离语义相比偏弱，ACR 选 oneTBB 是为 arena 隔离更优。
4. **Taskflow**：
   - 未采用：依赖图模型适合静态 DAG，ACR 任务动态生成更适合工作窃取。

## 未采用原因

`std::thread` 与自研线程池在工程严谨性上不达标；OpenMP 统一方案对既有代码侵入过大且 arena 隔离弱；Taskflow 模型与 ACR 动态任务流不匹配。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| 多线程 range | 并行 for over range | 与串行结果一致，耗时随核心数下降 |
| nested | arena 内嵌套 task_group | 无死锁，深度 ≥3 正常返回 |
| 取消 | task_group::cancel | 取消信号在 ≤10ms 内传播到所有 worker |
| 异常传播 | 子任务抛异常 | 异常被父任务捕获，arena 不崩溃 |
| 95% 节流 | `global_control` 动态调整 | CPU 利用率维持在 93%–97% 区间 ≥30s |
| 长短任务混排 | 长 1s + 短 10ms 并发 | 短任务平均等待 < 50ms，无饥饿 |

实验日志写入 `run/logs/acr/onetbb/<YYYYMMDD>/`。
