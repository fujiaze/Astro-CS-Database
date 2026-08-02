# ACR 底层支线任务拆分（tasks.md）

**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`
**本次范围**: Phase A–H

任务拆分用于并行分配给 subcoding agent。每个任务标注：依赖、输入、输出、验收、可并行性。

---

## Phase A：仓库和依赖审计（主 Agent，串行）

### A1 仓库审计报告
- **依赖**: 无
- **输入**: 控制包、仓库现状
- **输出**: `lib/acr/docs/audit-report.md`（仓库结构/构建系统/平台/CI/测试框架/依赖方式/base commit）
- **验收**: 报告完整，记录 worktree 路径与 base commit
- **状态**: 大部分已完成（本 spec §0、§2 已记录）

### A2 禁止修改路径清单
- **依赖**: A1
- **输入**: 仓库 lib/ 现有模块列表
- **输出**: `lib/acr/docs/forbidden-paths.md`（11 个现有模块 + AGENTS.md + AstroCS.wiki + tools/astro_toolkit.py 等）
- **验收**: 清单覆盖所有现有算法目录 + path guard 脚本引用此清单

### A3 path guard 脚本
- **依赖**: A2
- **输入**: forbidden-paths.md
- **输出**: `lib/acr/ci/path_guard.ps1`（检查 `git diff --name-only` 是否越界，越界则非零退出）
- **验收**: 在 worktree 内故意 touch 一个 lib/astro_image_io/ 文件，脚本能检出

### A4 ADR 集合
- **依赖**: A1
- **输入**: 控制包 05_OPEN_SOURCE_REUSE_PLAN.md
- **输出**: `lib/acr/docs/ADR-001` ~ `ADR-009`（9 个 ADR）
- **验收**: 每个 ADR 说明用途、版本、PoC、集成边界、替代方案、未采用原因
- **可并行**: ADR 之间独立，可并行 subcoding agent 撰写

### A5 dependency-lock.json
- **依赖**: A4
- **输入**: ADR 确定的版本
- **输出**: `lib/acr/docs/dependency-lock.json`（控制包 §11 字段）
- **验收**: JSON schema 校验通过

### A6 lib/acr/CMakeLists.txt 骨架
- **依赖**: A4、A5
- **输入**: spec §5 CMake 选项
- **输出**: `lib/acr/CMakeLists.txt`（options + FetchContent 骨架，CPU-only 默认可构建空目标）
- **验收**: `cmake -S lib/acr -B build/acr` configure 成功（无 GPU SDK）

### A7 Phase A 提交
- **依赖**: A1-A6
- **执行**: `docs(acr): freeze bottom-only scope and dependency ADRs` + `build(acr): add isolated optional targets and dependency gates`

---

## Phase B：公共 API 和 CPU baseline（主 Agent，串行）

### B1 公共头 acr.hpp 完整化
- **依赖**: A7
- **输入**: 控制包 include/astro/compute/acr.hpp 骨架 + 03_PUBLIC_API_SPEC.md
- **输出**: `lib/acr/include/astro/compute/acr.hpp`（parallel_for/for_2d/tiles/reduce/batch/scan/chunks/run_for + Buffer/BufferView/Event/NumericPolicy/ExecutionHints/Range1D/Extent2D/TileShape）
- **验收**: 公共头不包含任何第三方类型

### B2 Buffer/BufferView/Event 实现
- **依赖**: B1
- **输入**: acr.hpp
- **输出**: `lib/acr/buffers/`、`lib/acr/api/event.cpp`、`lib/acr/api/buffer.cpp`
- **验收**: shape/stride/ownership/subview/host-device 生命周期单测通过

### B3 oneTBB CPU runtime
- **依赖**: B2
- **输入**: ADR-002
- **输出**: `lib/acr/core/runtime.cpp`（task_arena、worker pool、lazy singleton）
- **验收**: 与现有 OpenMP 共存（编译现有模块不冲突）

### B4 parallel_for/for_2d/tiles 实现
- **依赖**: B3
- **输入**: B1、B2
- **输出**: `lib/acr/api/parallel_for.cpp` 等
- **验收**: GoogleTest 单测通过（空范围/单元素/非整 Tile/边缘）

### B5 parallel_reduce/batch/scan/chunks 实现
- **依赖**: B4
- **输入**: B1
- **输出**: `lib/acr/api/parallel_reduce.cpp` 等
- **验收**: 局部归约合并正确；scan 使用 CUB/oneDPL 适配（不自研生产级）

### B6 baseline scalar backend
- **依赖**: B4
- **输入**: B3
- **输出**: `lib/acr/backends/cpu/baseline.cpp`
- **验收**: CPU-only 构建运行 E01/E02/E04 小尺寸正确

### B7 standalone examples
- **依赖**: B6
- **输入**: 控制包 examples/
- **输出**: `lib/acr/examples/minimal_parallel_for.cpp`、`legacy_chunk_adapter.cpp`
- **验收**: 编译运行通过

### B8 GoogleTest 单测
- **依赖**: B4-B6
- **输入**: 控制包 12_TEST_VALIDATION_MATRIX.md §2
- **输出**: `lib/acr/tests/unit/`（test_api.cpp、test_buffer.cpp、test_event.cpp 等）
- **验收**: 全部通过

### B9 Phase B 提交
- **依赖**: B1-B8
- **执行**: `feat(acr): add public API and CPU baseline runtime`

---

## Phase C：硬件发现、NUMA、ISA（subcoding agent，与 D 并行）

### C1 hwloc topology
- **依赖**: B9
- **输入**: ADR-003
- **输出**: `lib/acr/topology/hwloc_topo.cpp`（package/core/PU/cache/NUMA/PCI 枚举）
- **验收**: 本机拓扑序列化 JSON 输出

### C2 cpu_features ISA 检测
- **依赖**: B9
- **输入**: ADR-004
- **输出**: `lib/acr/topology/cpu_features.cpp`（SSE/AVX/AVX2/FMA/AVX-512 子集精确）
- **验收**: 本机 ISA 报告正确（RTX 3060 Ti 主机 CPU）

### C3 ISA 函数版本 + 安全门禁
- **依赖**: C1、C2
- **输入**: B6 baseline
- **输出**: `lib/acr/backends/cpu/isa/`（sse.cpp、avx.cpp、avx2.cpp、avx512.cpp）+ 门禁逻辑
- **验收**: 不支持 ISA 永不执行；baseline 始终可用

### C4 线程/NUMA 亲和性
- **依赖**: C1
- **输入**: hwloc
- **输出**: `lib/acr/topology/affinity.cpp`
- **验收**: NUMA 本地/远端内存测试

### C5 设备报告 + 指纹
- **依赖**: C1、C2
- **输出**: `lib/acr/diagnostics/hardware_report.cpp`（hardware.json schema）
- **验收**: 完整指纹（CPU/ISA/cache/NUMA/GPU/PCI/driver/compiler/build）

### C6 拓扑缺失降级
- **依赖**: C1
- **输出**: 降级路径（无 hwloc 时纯 CPU baseline）
- **验收**: 模拟无 hwloc 仍可运行

### C7 Phase C 单测 + 经典 CPU 实验一致性
- **依赖**: C1-C6
- **验收**: 经典 CPU 实验与 Phase B 一致

### C8 Phase C 提交
- **执行**: `feat(acr): add topology and ISA discovery`

---

## Phase D：GPU portable backend（subcoding agent，与 C 并行）

### D1 alpaka adapter
- **依赖**: B9
- **输入**: ADR-001
- **输出**: `lib/acr/backends/alpaka/adapter.cpp`（统一 kernel 编译为 CPU/CUDA）
- **验收**: 同一 kernel 源编译为 CPU 和 CUDA

### D2 CUDA backend
- **依赖**: D1
- **输入**: ADR-001、本机 CUDA 11.8 + RTX 3060 Ti
- **输出**: `lib/acr/backends/cuda/`（device/queue/buffer/copy）
- **验收**: RTX 3060 Ti 上 AXPY/Triad 真实运行

### D3 数据驻留和 event
- **依赖**: D2
- **输出**: `lib/acr/backends/cuda/residency.cpp`
- **验收**: H2D/D2H 分离计时

### D4 kernel 异常传播
- **依赖**: D2
- **输出**: 异常捕获 + 转换为 ACR error code
- **验收**: kernel launch 失败时主进程不崩溃

### D5 无 GPU/驱动错误降级
- **依赖**: D2
- **输出**: `ACR_BUILD_CUDA=OFF` 时无 CUDA 引用；运行时 CUDA 不可用时回退 CPU
- **验收**: CPU-only 构建无 CUDA SDK 依赖

### D6 Phase D 提交
- **执行**: `feat(acr): add portable accelerator backend`

---

## Phase E：Qualification 和固定路由（主 Agent，依赖 B/C/D）

### E1 acr-benchmark 工具
- **依赖**: C8、D6
- **输入**: ADR-005、控制包 06_QUALIFICATION_BENCHMARK_SPEC.md
- **输出**: `lib/acr/tools/acr_benchmark/`（--profile quick|standard|full + 空载提示）
- **验收**: quick profile 在本机运行通过

### E2 acr-status/report/invalidate
- **依赖**: E1
- **输出**: `lib/acr/tools/acr_status/`、`acr_report/`、`acr_invalidate/`
- **验收**: CLI11 子命令完整

### E3 测试矩阵裁剪
- **依赖**: E1
- **输入**: 控制包 06 §4
- **输出**: ISA/线程代表点/精度/设备组合/尺寸桶裁剪逻辑
- **验收**: 矩阵不爆炸，覆盖关键组合

### E4 计时方法
- **依赖**: E1
- **输出**: 预热 3 次/正式 9 次或累计 1 秒/GPU device event/H2D-D2H 分离/median-p95-MAD
- **验收**: 输出格式符合 17 §18

### E5 硬件指纹 schema
- **依赖**: C5
- **输出**: `lib/acr/schemas/route_profile.schema.json`
- **验收**: schema 严格校验

### E6 固定路线生成
- **依赖**: E1-E5
- **输出**: `routes.json` 只读生成 + 运行时只读解析
- **验收**: 正式运行不修改 profile

### E7 missing/stale/corrupt 处理
- **依赖**: E6
- **输出**: 纯 CPU + 非阻断警告路径
- **验收**: 无 profile 时纯 CPU 运行 + 警告

### E8 Phase E 提交
- **执行**: `feat(acr): add qualification and immutable routes`

---

## Phase F：CPU+GPU 混合与工作保持（主 Agent，依赖 E）

### F1 Range/Tile 拆分
- **依赖**: E8
- **输出**: `lib/acr/scheduler/partition.cpp`（不重叠 chunk）
- **验收**: coverage bitmap 完整

### F2 CPU+GPU 调度
- **依赖**: F1
- **输出**: `lib/acr/scheduler/dispatcher.cpp`（finish 估算、工作保持）
- **验收**: 首选设备忙时使用空闲合格设备

### F3 局部 reduction 合并
- **依赖**: F2
- **输出**: 设备局部结果合并
- **验收**: E03 mixed 通过

### F4 失败回退
- **依赖**: F2
- **输出**: 失败任务回退（不重放已执行中的 chunk）
- **验收**: E15 故障注入通过

### F5 数据驻留
- **依赖**: F2
- **输出**: 写入后失效其他副本
- **验收**: 同步只发生在依赖需要时

### F6 Phase F 提交
- **执行**: `feat(acr): add mixed CPU GPU scheduler`

---

## Phase G：95% 资源控制（主 Agent，依赖 F）

### G1 CPU 软占用
- **依赖**: F6
- **输出**: `lib/acr/utilization/cpu_controller.cpp`（批次/队列/优先级/让步，100-500ms 窗口）
- **验收**: 所有 worker 可参与

### G2 GPU 软占用
- **依赖**: F6
- **输出**: `lib/acr/utilization/gpu_controller.cpp`（queue depth/batch/提交节奏）
- **验收**: 短 batch 优先

### G3 RAM/VRAM 限制
- **依赖**: F6
- **输出**: `lib/acr/utilization/memory_limit.cpp`（`limit = min(total*ratio, total-fixed_reserve)`）
- **验收**: 达上限时按序：停止提交→缩小 batch→释放 cache→选低内存路线→回退 CPU

### G4 I/O 预算
- **依赖**: F6
- **输出**: `lib/acr/utilization/io_budget.cpp`（有界队列）
- **验收**: 日志/结果写入不拖垮计算

### G5 配置热读取边界
- **依赖**: G1-G4
- **输出**: 明确支持/不支持热更新
- **验收**: 用户改目标后可继续使用最近路由

### G6 Phase G 提交
- **执行**: `feat(acr): add utilization controller`

---

## Phase H：经典实验和持续测试（主 Agent + subcoding agents，依赖 G）

### H1 E01-E04 基础实验组（并行 subcoding agent）
- **依赖**: G6
- **输出**: `lib/acr/tests/classic/e01_memory.cpp`、`e02_axpy.cpp`、`e03_reduce.cpp`、`e04_transpose.cpp`
- **验收**: CPU+CUDA 通过，JSON case 报告
- **可并行**: 4 个实验独立

### H2 E05-E07 Tile/Stencil/Atomic 组（并行 subcoding agent）
- **依赖**: G6
- **输出**: `e05_convolution.cpp`、`e06_resampling.cpp`、`e07_histogram.cpp`
- **验收**: CPU+CUDA+Mixed 通过

### H3 E08-E10 Scan/Gather/Branch 组（并行 subcoding agent）
- **依赖**: G6
- **输出**: `e08_scan.cpp`、`e09_gather_scatter.cpp`、`e10_mandelbrot.cpp`
- **验收**: CUB/oneDPL 适配

### H4 E11-E12 库适配组（并行 subcoding agent）
- **依赖**: G6
- **输出**: `e11_gemm.cpp`（cuBLAS/oneMKL）、`e12_fft.cpp`（cuFFT/FFTW）
- **验收**: vendor handle 隔离

### H5 E13-E14 Mixed/Utilization 组（并行 subcoding agent）
- **依赖**: G6、F8
- **输出**: `e13_mixed.cpp`、`e14_utilization.cpp`
- **验收**: 0/25/50/75/100% 拆分 + 50/80/95/100% 目标点

### H6 E15-E16 Fault/Lifetime 组（并行 subcoding agent）
- **依赖**: G6
- **输出**: `e15_failure.cpp`、`e16_concurrency.cpp`
- **验收**: 故障注入 + 100 次重启 + 30 秒持续

### H7 sanitizer
- **依赖**: H1-H6
- **输出**: ASan/UBSan/TSan 构建配置 + 运行
- **验收**: 无泄漏/竞态/use-after-free

### H8 Evidence Package 草稿
- **依赖**: H1-H7
- **输出**: `工程控制/evidence/acr/`（构建日志/测试日志/经典实验结果/sanitizer/path guard/未通过 SKIPPED 原因）
- **验收**: 完整可审

### H9 Phase H 提交
- **执行**: `test(acr): add classic experiment suite and fault injection` + `docs(acr): add merge evidence and dormant integration report`

---

## Phase I：主线无副作用合并（**本次不做**，等用户授权）

### I1 同步 main
### I2 合并预演
### I3 path guard 终检
### I4 CPU-only + 现有主线 + ACR classic + 无副作用测试
### I5 --no-ff 合并
### I6 推送 main
### I7 Merge Report + Source Snapshot + Evidence Package

---

## 任务并行化时间线

```
T1: A1-A7（主 Agent，串行）
T2: B1-B9（主 Agent，串行，建立 API 基础）
T3 并行:
    - C1-C8（subcoding agent 1）
    - D1-D6（subcoding agent 2）
T4: E1-E8（主 Agent，依赖 T3）
T5: F1-F6（主 Agent）
T6: G1-G6（主 Agent）
T7 并行（4 组 subcoding agent）:
    - H1 E01-E04
    - H2 E05-E07
    - H3 E08-E10
    - H4 E11-E12
T8 并行（2 组 subcoding agent）:
    - H5 E13-E14
    - H6 E15-E16
T9: H7-H9（主 Agent，集成 + sanitizer + evidence）
```

每个 subcoding agent 任务规格：输入（API/接口/控制包章节）、输出（具体文件）、验收（单测通过 + path guard）、不修改同一文件。
