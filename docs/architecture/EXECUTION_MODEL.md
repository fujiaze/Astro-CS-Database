# Execution & Lifetime Model (ARC-EXEC)

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> 关联: ARC-EXEC-001..00N  模块: phase2/acr  状态: FROZEN

> ⚠ **休眠面不得写成生产执行层**：
> 本文件的 **ACR / CUDA / GPU 行与 §2/§5 的 H2D/D2H、GPU buffer、GPU fallback 全部标
> `DORMANT`**（保留源码与隔离测试，**不进生产构建/加载/路由/benchmark/发布**，
> 最高设计 §8/§1.3）；**浏览器（Qt）标「工具分类（非发布）」**（最高设计 §7.1/§10.1：
> HiPS Browser 不进产品 manifest）；**orchestrator 标「历史保留」**（最高设计 §7.1：
> 接入后删除）。上述三类**均不是生产执行层**，任何发布/性能结论不得引用其行。

## 1 串/并行分层

| 路径 | 调用线程 | 切分单位 | 最大并发 | 调度器 | 同步点 | 证据 |
|---|---|---|---|---|---|---|
| Stage1 calibrate | calibrator thread | per-tile OpenMP | 16 | OpenMP parallel for | tile barrier | `calibrator.cpp: OpenMP 16` |
| Stage1 drizzle | drizzle worker | per-source-pixel candidate | n_threads | OpenMP + cache | tile merge serial | `drizzle_engine.cpp:1662 reduction` |
| Stage2 sampler | stage2 worker pool | per-control-cell (64 per tile) | budget.max_workers（Runtime lease；1 = 串行 reference） | std::thread pool（无 OpenMP 条件） | cell barrier | `sampler.cpp:924-954`（`cfg.cpu_workers = budget.max_workers`，`next_c.fetch_add(1)` 动态取 cell） |
| Stage2 UPM solve | stage2 main | full graph | 1 | serial | — | `upm.cpp Huber IRLS` |
| Stage2 block/reject/integrate | block worker | per-pixel candidate stack | n_threads | OpenMP per-pixel | pixel barrier | `rejection.cpp/integrate.cpp` |
| ~~ACR Dispatcher~~ **DORMANT** | — | — | — | — | — | 保留源码与隔离测试，**不进生产**（最高设计 §8）；原行：acr thread / per-tile chunk (px) / auto / Dispatcher::decide / mixed merge / `acr_kernels.cpp` |

见 `THREADING_MODEL.md` 确定性锚点 ARC-004。

## 2 异步 I/O 与 ACR

| 项 | 模式 | 细节 |
|---|---|---|
| HiPS write | async_io | `aio_hips_writer` 异步刷盘, 事务提交；合同见 [ASYNC_IO_CONTRACT.md](ASYNC_IO_CONTRACT.md) |
| HiPS read | 并发只读（无进程级锁） | **读路径线程模型**：读路径无进程级共享可变状态；每个 `fitsfile*` 为单线程私有、生命周期不跨线程转移（每次调用各自 open→read→close，句柄只在该调用栈帧）；并发安全由 cfitsio `_REENTRANT` 构建保证（`FptrTable`/错误栈由 cfitsio 自带 `Fitsio_Lock` 保护，`READONLY` 打开 `fits_already_open` 直接返回、不复用句柄，`cfileio.c:1544`）。机器判据 `check_execution_contracts.py::EXEC-AIO-READ-NO-GLOBAL-LOCK` |
| ~~ACR H2D/D2H~~ **DORMANT** | — | 保留源码与隔离测试，**不进生产**；原行：async via CUDA stream / `cuda_bridge_api` H2D>0 in cold Mixed (BDR D gate) |
| Fallback | sync fallback | 生产 fallback **只有一条**：无 cpu_profile → baseline 后端 + 动态 worker（保守合法，最高设计 §8） |

## 3 锁/原子与 I/O 串行

| 共享 | 原语 | 粒度 |
|---|---|---|
| aio_read 读路径 | **无进程级互斥量**（读路径不存在进程级临界区） | 每次调用独立句柄，句柄线程私有、不跨线程转移；剩余串行化点（诊断/写面）统一走计数式 `aio::CfitsioLockGuard`（等待进 `resource_timeseries.csv` 的 `lock_wait_ns`） |
| rejected_* | `atomic` | per-sample |
| Drizzle counters | `atomic` / `reduction` | per-tile |
| Dense cache | `mutex` | per-write |
| Memory budget | `atomic` counters | per-alloc |

## 4 Future/Callback 与取消/超时

| 项 | 语义 |
|---|---|
| Orchestrator cancel | atomic flag `CANCELLED=10`, 流水线中断检查点 |
| Stage2 signal | handler 设置取消标志, 当前 block 完成即退 |
| Timeout | stage 配置 timeout_ms, 超时返 `TIMEOUT=9` |
| Exception传播 | C ABI 边界捕获转返回码, 无异常跨 DLL |

## 5 CPU/GPU 内存驻留与回退

| 项 | 语义 |
|---|---|
| CPU buffers | `BufferBinding` caller-owned, `free` via aio_hio_free |
| ~~GPU buffers~~ **DORMANT** | 保留源码与隔离测试，**不进生产**（最高设计 §8）；原行：`cuda_buffer` device alloc, residency via ResidencyManager |
| ~~H2D/D2H~~ **DORMANT** | 同上；原行：per-chunk async stream, timed via bridge loader |
| Fallback | **生产 fallback = 无 cpu_profile → baseline 后端 + 动态 worker**（最高设计 §8）。~~原「GPU OOM/无画像 → CPU OpenMP per-pixel」面 DORMANT~~ |

## 6 确定性与嵌套并行限制

| 约束 | 规则 |
|---|---|
| 浮点求和顺序 | 按输入索引固定顺序, reduction文档化 (THREADING_MODEL ARC-004) |
| 输入顺序 | frame_id/cell/pixel 索引固定, 不依赖线程调度 |
| 嵌套并行 | 禁止 (外层已并行则内层串行) |

## 7 错误/异常传播

| 错误 | 传播 |
|---|---|
| C ABI 返回码 | 0=OK 非0=失败, err缓冲仅日志 |
| ~~ACR error~~ **DORMANT** | 保留源码与隔离测试，**不进生产**；生产错误面见 `docs/architecture/ERROR_MODEL.md`（唯一源 `lib/infrastructure/cli/exit_codes.h`） |
| Invalid/UNDERDETERMINED | per-pixel status, 不抛异常 |

## 8 ARC-EXEC 契约 ID 映射

| ID | 覆盖 |
|---|---|
| ARC-EXEC-001 | Stage1 per-tile OpenMP calibrate |
| ARC-EXEC-002 | Stage2 sampler 并发只读（无进程级锁；句柄单线程私有、不跨线程转移；Runtime lease 定 worker 数） |
| ARC-EXEC-003 | Stage2 UPM serial solve |
| ARC-EXEC-004 | Phase2 block/reject/integrate per-pixel parallel |
| ~~ARC-EXEC-005~~ **DORMANT** | ACR Dispatcher mixed H2D/D2H + fallback —— **休眠，不进生产**（最高设计 §8） |
| ARC-EXEC-006 | HiPS async I/O transaction |
| ARC-EXEC-007 | Orchestrator cancel/timeout propagation |

见 `THREADING_MODEL.md`, `IO_AND_ATOMICITY.md`, `ERROR_MODEL.md` 子契约。

---

## 在役生产/CI 面（不得标为 DORMANT）

- `lib/infrastructure/cli/v6_runtime_contract.h`：由 `lib/infrastructure/cli/commands.cpp` include 并**编入产品 `acsd`** ⇒ **在役生产**。
- `lib/infrastructure/cli/v6_mode_gate.h`：同链 include ⇒ **在役生产**。
- `lib/infrastructure/scheduler/v6_budget.py`：由 `eng/tools/v6/check_v6_runtime_closure.py` 调用其 `selftest`、`eng/tools/v6/v6_runtime_oracle.py` 锚定 ⇒ **在役 CI 面**。
- 上述三者与本节开头的 `DORMANT` 面（ACR/CUDA/GPU、Qt 浏览器、orchestrator）**分属不同类别**，不得混列。

