# 执行与生命周期模型

> 上游：ASTROCS_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）

本文件给出各执行路径的串/并行分层、锁与原子原语、取消与超时语义、内存驻留与错误传播。生产执行层与隔离面分列：**ACR / CUDA / GPU 面不接入生产**（最高设计 §1.4：ACR 源码保留为隔离实验、生产不可达），**HiPS 浏览器属工具分类（非发布）**（最高设计 §1.4：GUI 非目标，不进产品清单），**Phase1 编排并入 CLI 的 pipeline driver、无独立进程**（最高设计 §8.1）。发布与性能结论只引用生产层条目。

## 1 串/并行分层

| 路径 | 切分单位 | 最大并发 | 调度器 | 同步点 |
|---|---|---|---|---|
| Stage1 calibrate | per-pixel（单层 `n = w*h` 循环） | Runtime lease | OpenMP parallel for（static） | 无 barrier、无 tile 级同步点 |
| Stage1 drizzle | per-source-pixel candidate（确定性 stripe 分片） | Runtime lease | OpenMP + cache | 按 stripe 索引升序左折叠归约 |
| Stage2 sampler | per-control-cell | Runtime lease | std::thread pool | cell barrier |
| Stage2 UPM solve | full graph；per-obs 权重行 | 主迭代 1；权重 = lease | 主迭代串行；权重 per-call 池 | — |
| Stage2 block/reject/integrate | per-tile（动态认领） | Runtime lease | p2_parallel_for（std::thread） | 无 barrier，跨任务零浮点归约 |

调用线程与证据锚：

- **Stage1 calibrate**：调用线程 = calibrator thread。证据 = `lib/algorithms/calibration/src/calibrator.cpp`
  （`:94` 平场归一、`:126` 兼容式、`:134` 标准式；`calibrate_d` 的 `:170`/`:178` 同构）。
- **Stage1 drizzle**：调用线程 = drizzle worker。证据 = `drizzle_engine.cpp` `:1923`（主并行区）、
  `:1702`（`merge_tile_map_into`）、`:2117-2142`（stripe 序归约）。

- **Stage2 sampler**：调用线程 = stage2 worker pool；`cfg.cpu_workers = budget.max_workers`。
  证据 = `sampler.cpp` `:924-954`（`next_c.fetch_add(1)` 动态取 cell）。
- **Stage2 UPM solve**：调用线程 = stage2 main（IRLS 主迭代）+ per-call std::thread 池（`compute_raw` /
  per-obs 权重，登记在 `THREAD_BUDGET_ARCH.md`）。证据 = `upm.cpp` `:605`（`compute_raw`）、`:747`
  （per-obs 独立 w 计算）、`:620/:751/:794/:916/:2143`（per-call 池 ×5，`cworkers = cfg.cpu_workers`）；
  M/C 更新主体串行（`cg_solve_frame` `:676` 起）。
- **Stage2 block/reject/integrate**：调用线程 = block worker（std::thread 池）。证据 =
  `module_adapters.cpp` `:9045`（`p2_parallel_for` 定义）、`:10983`（reject 调用点）、`:11778`
  （integrate 调用点）；`rejection.cpp`/`integrate.cpp` 内 `#pragma omp` 计数 = 0。

确定性锚点见 `THREADING_MODEL.md`。ACR 异构分块面不接入生产（最高设计 §1.4），不在本表列行。

## 2 异步 I/O

| 项 | 模式 | 细节 |
|---|---|---|
| HiPS write | 原子提交（`aio_hips_writer`） | 同目录临时文件 → 内容写出 → CHECKSUM 校验 → `fsync` → 打洞（可选）→ 原子 `rename` → 父目录 `fsync`；同目标并发写为 last-writer-wins（单写者前提，见 `IO_AND_ATOMICITY.md`） |
| HiPS read | 并发只读（无进程级锁） | 读路径无进程级共享可变状态；`fitsfile*` 线程私有、不跨线程转移；并发安全由 cfitsio `_REENTRANT` 构建保证（详见本节末） |
| Fallback | sync fallback | 生产 fallback **只有一条**：无 cpu_profile → baseline 后端 + 动态 worker（保守合法，见 `CPU_BACKEND_ARCH.md` §6） |

**HiPS 读路径线程模型**：每次调用各自 open→read→close，句柄只活在调用栈帧内；
`FptrTable` 与错误栈由 cfitsio 自带 `Fitsio_Lock` 保护，`READONLY` 打开走
`fits_already_open` 直接返回、不复用句柄（`cfitsio/cfileio.c` `:1544`）。
机器判据 = `eng/tools/quality/contracts/check_execution_contracts.py` 的
`EXEC-AIO-READ-NO-GLOBAL-LOCK`。

## 3 锁 / 原子与 I/O 串行

| 共享 | 原语 | 粒度 |
|---|---|---|
| aio_read 读路径 | **无进程级互斥量**（读路径不存在进程级临界区） | 每次调用独立句柄，句柄线程私有、不跨线程转移；剩余串行化点（诊断/写面）统一走计数式 `aio::CfitsioLockGuard`（等待进 `resource_timeseries.csv` 的 `lock_wait_ns`） |
| rejected_* | `atomic` | per-sample |
| Drizzle counters | `atomic`；浮点归约 = thread-local/per-stripe scratch 累加 + stripe 索引升序左折叠（无 OpenMP reduction 子句） | per-tile |
| Dense cache | `mutex` | per-write |
| Memory budget | `atomic` counters | per-alloc |

## 4 Future / Callback 与取消 / 超时

| 项 | 语义 |
|---|---|
| Orchestrator cancel | atomic flag `CANCELLED=9`（用户取消或超时；唯一源 `lib/infrastructure/cli/exit_codes.h`），流水线中断检查点 |
| Stage2 signal | handler 设置取消标志，当前 block 完成即退 |
| Timeout | stage 配置 timeout_ms，超时返 `CANCELLED=9`（唯一源不设独立超时码，取消与超时同码） |
| Exception 传播 | C ABI 边界捕获转返回码，无异常跨 DLL |

## 5 CPU 内存驻留与回退

| 项 | 语义 |
|---|---|
| CPU buffers | `BufferBinding` caller-owned，`free` via aio_hio_free |
| Fallback | **生产 fallback = 无 cpu_profile → baseline 后端 + 动态 worker**（见 `CPU_BACKEND_ARCH.md` §6） |

## 6 确定性与嵌套并行限制

| 约束 | 规则 |
|---|---|
| 浮点求和顺序 | 按输入索引固定顺序，reduction 文档化（见 `THREADING_MODEL.md`） |
| 输入顺序 | frame_id/cell/pixel 索引固定，不依赖线程调度 |
| 嵌套并行 | 外层已并行则内层串行 |

## 7 错误 / 异常传播

| 错误 | 传播 |
|---|---|
| C ABI 返回码 | 0=OK，非 0=失败，err 缓冲仅日志 |
| Invalid / UNDERDETERMINED | per-pixel status，不抛异常 |

## 8 执行合同面映射

本文件的执行合同面按执行路径标识，覆盖如下；生产错误面唯一源 = `lib/infrastructure/cli/exit_codes.h`（表见 `ERROR_MODEL.md`）。

| 合同面 | 覆盖 |
|---|---|
| ARC-EXEC-001 | Stage1 calibrate：per-pixel OpenMP（`#pragma omp parallel for schedule(static)`，共享 P2_ENABLE_OPENMP 构建开关；线程数由 Runtime lease 注入） |
| ARC-EXEC-002 | Stage2 sampler 并发只读（无进程级锁；句柄单线程私有、不跨线程转移；Runtime lease 定 worker 数） |
| ARC-EXEC-003 | Stage2 UPM solve：IRLS 主迭代串行；compute_raw/per-obs 权重按 Runtime lease 并行（per-call std::thread 池 ×5，登记在 `THREAD_BUDGET_ARCH.md`） |
| ARC-EXEC-004 | Phase2 block/reject/integrate：p2_parallel_for（std::thread）tile 级并行、原子计数动态认领，无 barrier，跨任务零浮点归约；tile 内逐像素候选栈串行处理 |
| ARC-EXEC-006 | HiPS 写事务：临时文件 → 内容 → CHECKSUM 校验 → `fsync` → 打洞（可选）→ 原子 `rename` → 父目录 `fsync`；取消/失败不落正式产品 |
| ARC-EXEC-007 | Orchestrator cancel/timeout propagation |

子契约见 `THREADING_MODEL.md`、`IO_AND_ATOMICITY.md`、`ERROR_MODEL.md`。

## 9 编排策略承接

调度器按下列策略编排，内存占用永不越界（最高设计 §8.3）；各策略的现行落点：

| 策略 | 现行落点（详见下） |
|---|---|
| **静态预算** | 标定常数 + 准入预算面 |
| **探针校正** | 探针面 + 探针驱动标定 |
| **异步并行** | 多帧并行 + I/O 与计算重叠 |
| **可中断排队** | 内存闸门限帧在飞数 |
| **可丢弃重跑** | 原子单元不落盘 + 重跑换目录 |

- **静态预算**：从输入数据（帧尺寸与类型、配置、模块声明）静态估算每模块的内存与 CPU 需求，
  作调度决策输入；标定常数落点 = `PERFORMANCE_MODEL.md` §1.2（`kP1FrameBytesPerPixel`、
  `kP1FrameMemSafetyFrac`、内存预算百分比），准入预算面 = `eng/packaging/config/runtime_resources.json`。
- **探针校正**：每节点墙钟 / 排队等待 / 块生命周期 / RSS / I/O / worker 均衡 / 缓存命中探针
  随事件流落盘（探针面见 `observability/RESOURCE_MONITORING_CONTRACT.md`）；用实测校正静态模型，
  探针驱动的标定旋钮 = `PERFORMANCE_MODEL.md` 与 `THREADING_MODEL.md` 的轴分配。

- **异步并行**：预算充裕时异步启动独立工作流——多帧并行（`p1_parallel_for` 原子认领分批，
  同时最多 `cap` 帧在飞）、I/O 预取与计算重叠（`ASYNC_IO_CONTRACT.md`）；
  异步只用于能隐藏延迟的 I/O、预取与压缩。
- **可中断排队**：预测后续工作流将产生内存膨胀或多线程峰值时，中断优先级低的工作流，
  让其在内存中等待、按序排队进入；现行形态 = 内存闸门 `p1_memory_cap` 限制帧在飞数
  （`THREADING_MODEL.md`「并行轴分配」），准入边界优先于并行宽度。
- **可丢弃重跑**：内存仍不足时丢弃进度最低的工作流并释放其占用，该工作流随后重新开始；
  现行形态 = 失败或取消的产物不落盘（原子单元不落盘），重跑 = 新运行目录 + 新 manifest
  （`ARCHITECTURE.md` §5）。

**不变量**：数值结果与并发度无关——1/N worker 数值等价，判据是事前冻结的浮点容差；归约顺序冻结是达成手段，不是判据本身（最高设计 §8.3）。

## 10 在役生产 / CI 面

- `lib/infrastructure/cli/runtime_contract.h`：由 `lib/infrastructure/cli/commands.cpp` include 并**编入产品 `acsd`** ⇒ **在役生产**。
- `lib/infrastructure/cli/mode_gate.h`：同链 include ⇒ **在役生产**。
- `lib/infrastructure/scheduler/budget.py`：由 `eng/tools/quality/check_runtime_closure.py` 调用其 `selftest`、`eng/tools/quality/runtime_oracle.py` 锚定 ⇒ **在役 CI 面**。
- 上述三者与隔离面（ACR / CUDA / GPU、Qt 浏览器）**分属不同类别**，各列一张表。
