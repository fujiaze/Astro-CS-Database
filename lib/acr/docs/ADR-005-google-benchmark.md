# ADR-005: Google Benchmark v1.9.1 Qualification 测量框架

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | Google Benchmark v1.9.1（commit `v1.9.1`） |
| 许可证 | Apache-2.0 |
| 平台 | Windows / Linux |
| 形态 | 编译型库 |

## 状态

Accepted。Google Benchmark 作为 ACR Qualification 阶段的微基准测量框架，承担单设备微基准驱动、参数化、warm-up、重复、JSON 输出与统计汇总职责。ACR 在其之上扩展 GPU event 计时、CPU+GPU 联合、温态持续等能力。

## 背景

ACR 需要为每个 kernel × 后端 × 问题规模组合产出可重复的性能数据，作为离线路由标定的输入。测量框架必须支持：参数化（problem size sweep）、warm-up（避免冷启动偏差）、重复（统计稳定性）、统计输出（median / p95 / MAD）、结构化 JSON（供下游 router 解析）。

Google Benchmark 原生覆盖 CPU 侧需求，但 ACR 还需要：GPU event 计时（避免 CPU-GPU 同步偏差）、CPU+GPU 联合测量、温态持续测量、空载提示（检测系统抖动）、路由生成（基于测量结果产出路由表）、profile 指纹、中断恢复。这些由 ACR 在 Google Benchmark 之上扩展实现。

**Google Benchmark 不替代单元测试**：单元测试由 ADR-006 GoogleTest 承担，Benchmark 只负责性能测量与统计。

## 决策

1. 引入 Google Benchmark v1.9.1 作为 ACR Qualification 测量框架。
2. 使用原生能力：`BENCHMARK` 宏、`Range`/`DenseRange` 参数化、`Iterations`/`Repetitions`、`UseRealTime`、`JSON` 输出。
3. ACR 扩展（在 Benchmark 之外实现，不修改 upstream）：
   - 空载提示（idle probe，检测系统背景负载）
   - GPU event 计时包装器（CUDA/HIP event，对齐 alpaka 队列）
   - CPU+GPU 联合测量（同步等待与重叠两种模式）
   - 温态持续测量（long-running，检测热降频）
   - 路由生成（基于测量 JSON 产出路由表）
   - profile 指纹（合并 CPU/ISA/GPU 拓扑与测量结果）
   - 中断恢复（benchmark 中断后已完成的测量可落盘）
4. 统计口径：median / p95 / MAD 三者必出，写入 JSON。
5. 不替代单元测试。

## 理由

- Google Benchmark 与 ADR-006 GoogleTest 同生态，构建链与 CI 集成一致。
- 原生 JSON 输出格式稳定，便于下游 router 解析。
- 参数化与重复机制成熟，避免手写计时循环的常见陷阱（编译器优化、时钟精度、warm-up）。
- Apache-2.0 许可证宽松。
- MSYS2 有预编译包，Windows 构建无障碍。

## 集成边界

- **职责内**：单设备微基准驱动、参数化、warm-up、重复、JSON 输出、median/p95/MAD 统计。
- **职责外**：单元测试（由 GoogleTest 承担）、GPU event 计时的底层实现（ACR 扩展，非 upstream 功能）、路由决策（由 ACR router 基于 JSON 解析）。
- **扩展边界**：ACR 扩展能力以独立文件实现，不修改 Google Benchmark upstream 源码，便于升级。
- **API 边界**：`benchmark::State` 等类型不得出现在 ACR 公共 API 签名中。
- **构建边界**：Google Benchmark 仅在 `ACR_BUILD_BENCHMARKS=ON` 时编译，默认 ON；CPU-only 构建不引用 CUDA event 扩展。

## 替代方案

1. **手写计时循环（`std::chrono`）**：
   - 未采用：易受编译器优化（死代码消除）、时钟精度、warm-up 缺失影响；统计口径需自行实现，不可靠。
2. **Catch2 `BENCHMARK` 宏**：
   - 未采用：功能弱于 Google Benchmark（参数化、重复、JSON 输出均不如）；且 ADR-006 已选定 GoogleTest，引入 Catch2 会造成双测试框架（被控制包禁止）。
3. **perf / VTune 等外部 profiler**：
   - 未采用：不可在 CI 中自动化驱动；输出格式非结构化 JSON；平台限制（perf 仅 Linux）。

## 未采用原因

手写计时在工程严谨性上不达标；Catch2 BENCHMARK 与 ADR-006 冲突且功能弱；外部 profiler 无法满足 CI 自动化与结构化输出需求。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| 经典实验矩阵可驱动 | kernel × 后端 × 问题规模 sweep | 矩阵完整运行，无中断 |
| JSON 输出 | 落盘 benchmark 结果 | 含 median/p95/MAD，schema 校验通过 |
| 可重复 | 同配置重复 5 次 | median 相对偏差 < 5% |
| 空载提示 | 检测背景负载 | 空载 CPU 占用 > 10% 时标记可疑 |
| GPU event 计时 | CUDA 后端 kernel 计时 | 与 `nvprof` 偏差 < 3% |
| 中断恢复 | 中途中断后重启 | 已完成测量保留，仅重跑未完成项 |

实验日志与 JSON 写入 `run/logs/acr/benchmark/<YYYYMMDD>/`。
