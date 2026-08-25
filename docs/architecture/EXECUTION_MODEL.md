# Execution & Lifetime Model (ARC-EXEC)

> 关联: ARC-EXEC-001..00N  模块: orchestrator/phase2/acr  状态: FROZEN (T303 2026-08-23)

## 1 串/并行分层

| 路径 | 调用线程 | 切分单位 | 最大并发 | 调度器 | 同步点 | 证据 |
|---|---|---|---|---|---|---|
| Stage1 calibrate | calibrator thread | per-tile OpenMP | 16 | OpenMP parallel for | tile barrier | `calibrator.cpp: OpenMP 16` |
| Stage1 drizzle | drizzle worker | per-source-pixel candidate | n_threads | OpenMP + cache | tile merge serial | `drizzle_engine.cpp:1662 reduction` |
| Stage2 sampler | stage2 main | per-control-cell (64 per tile) | n_threads if P2_ENABLE_OPENMP ON else 1 | OpenMP or serial | cell barrier | `sampler.cpp:604, CMakeLists.txt:18 OFF` |
| Stage2 UPM solve | stage2 main | full graph | 1 | serial | — | `upm.cpp Huber IRLS` |
| Stage2 block/reject/integrate | block worker | per-pixel candidate stack | n_threads | OpenMP per-pixel | pixel barrier | `rejection.cpp/integrate.cpp` |
| ACR Dispatcher | acr thread | per-tile chunk (px) | auto | Dispatcher::decide | mixed merge | `acr_kernels.cpp` |

见 `THREADING_MODEL.md` 确定性锚点 ARC-004。

## 2 异步 I/O 与 ACR

| 项 | 模式 | 细节 |
|---|---|---|
| HiPS write | async_io | `aio_hips_writer` 异步刷盘, 事务提交 |
| HiPS read | serial or critical | `aio_read critical(aio_read)` 若 OpenMP 开启则串行化 |
| ACR H2D/D2H | async via CUDA stream | `cuda_bridge_api` H2D>0 in cold Mixed (BDR D gate) |
| Fallback | sync fallback | ivar weight_mode=2 → CPU canonical (ACR-IVAR-001), 无画像→OpenMP fallback |

## 3 锁/原子与 I/O 串行

| 共享 | 原语 | 粒度 |
|---|---|---|
| aio_read | `critical(aio_read)` | whole read if parallel |
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
| GPU buffers | `cuda_buffer` device alloc, residency via ResidencyManager |
| H2D/D2H | per-chunk async stream, timed via bridge loader |
| Fallback | GPU OOM/无画像 → CPU OpenMP per-pixel (science equiv) |

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
| ACR error | Dispatcher 返回码, fallback 兜底 |
| Invalid/UNDERDETERMINED | per-pixel status, 不抛异常 |

## 8 ARC-EXEC 契约 ID 映射

| ID | 覆盖 |
|---|---|
| ARC-EXEC-001 | Stage1 per-tile OpenMP calibrate |
| ARC-EXEC-002 | Stage2 sampler critical(aio_read) |
| ARC-EXEC-003 | Stage2 UPM serial solve |
| ARC-EXEC-004 | Phase2 block/reject/integrate per-pixel parallel |
| ARC-EXEC-005 | ACR Dispatcher mixed H2D/D2H + fallback |
| ARC-EXEC-006 | HiPS async I/O transaction |
| ARC-EXEC-007 | Orchestrator cancel/timeout propagation |

见 `THREADING_MODEL.md`, `IO_AND_ATOMICITY.md`, `ERROR_MODEL.md` 子契约。
