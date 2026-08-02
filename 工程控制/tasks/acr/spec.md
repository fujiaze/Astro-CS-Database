# ACR 底层支线实现规格（spec.md）

**支线**: AstroCompute Runtime（ACR）
**分支**: `feature/astrocompute-runtime`（从 `origin/main` 8f50519 创建）
**worktree**: `run/worktrees/acr/`（沙箱内隔离工作树，主仓库工作区不动）
**控制包版本**: AstroCS_ACR_Branch_Control_Package_V1.2_2026-08-02
**测试框架**: GoogleTest 1.15.2
**GPU 真实验证**: 启用 CUDA 11.8 backend（本机 RTX 3060 Ti，驱动 595.79）
**日期**: 2026-08-02

---

## 1. 目标与范围

### 1.1 本支线目标
开发一个可独立构建、测试、未来接入的底层计算运行时 ACR。本支线**严禁**修改 AstroCS 现有任何算法实现、调用链、PipelineFrame 数据语义、Stage1/Stage2 流程、OpenMP 代码。

### 1.2 本次会话范围
完成 Phase A–H（审计 → 公共API/CPU baseline → 硬件发现/ISA → GPU backend → Qualification/路由 → CPU+GPU 混合 → 95% 资源控制 → 16 项经典实验+故障注入）。

**Phase I（合并 main）本次不做**：全部 Phase 验收通过后，由用户二次授权方可执行合并。

### 1.3 禁止修改路径（path guard 红线）
以下目录在 `feature/astrocompute-runtime` 分支上**只读**：
- `lib/astro_image_io/`、`lib/calibration/`、`lib/data_pipeline/`、`lib/dynamic_psf/`
- `lib/gaia_xpsd_client/`、`lib/healpix_db/`、`lib/orchestrator/`
- `lib/photometric_calib/`、`lib/plate_solve/`、`lib/snr_estimator/`、`lib/star_detector/`
- `AGENTS.md`、`AstroCS.wiki/`（worktree 内）、`tools/astro_toolkit.py`、`tools/gen_audit_pack.py`、`tools/vq-commit.ps1`
- 任何 `*.exe`、`*.dll`、`build/`、`testdata/`（只读原始数据）

允许修改/新增：
- `lib/acr/`（ACR 全部源码、测试、工具、文档）
- `工程控制/tasks/acr/`（本三件套 + ADR + 证据）
- `工程控制/evidence/acr/`（实验结果、构建日志、path guard 报告）
- `run/worktrees/acr/build/`（worktree 内构建产物，已被 .gitignore）

---

## 2. 隔离环境

### 2.1 worktree
```
F:\Astro dev\Astro CS Normalization Database\             # 主仓库 (experiment/fast-drizzle-r06, 不动)
└── run\worktrees\acr\                                     # worktree (feature/astrocompute-runtime)
    ├── lib\acr\                                            # ACR 开发目录
    ├── 工程控制\tasks\acr\                                 # spec 三件套
    ├── 工程控制\evidence\acr\                              # 实验证据
    └── build\acr\                                          # 构建产物（gitignored）
```
- `run/*` 已在主仓库 `.gitignore`，worktree 目录不污染主仓库 status
- worktree 内 `.gitignore` 继承自主分支（main），同样忽略 `build/`、`run/*` 等
- worktree 共享主仓库 `.git`，push 时用 PowerShell

### 2.2 base commit
- `origin/main` HEAD: `8f5051946e9ea824ceefa6a90a071de7cad31a98`
- `feature/astrocompute-runtime` 跟踪 `origin/main`

---

## 3. ACR 目录结构

```
lib/acr/
├── CMakeLists.txt                 # 独立 CMake 入口，FetchContent 拉依赖
├── README.md                      # 模块说明
├── memory.md                      # 模块开发日志（按 user_rules）
├── include/astro/compute/
│   └── acr.hpp                    # 公共 API（控制包提供骨架，扩充完整）
├── api/                           # 公共 API 实现（parallel_for/tiles/reduce/batch, BufferView, Event）
├── core/                          # runtime singleton（lazy init）、error、status
├── buffers/                       # BufferView/Buffer/DeviceBuffer、residency、view/subview
├── topology/                      # hwloc 拓扑、cpu_features ISA、设备指纹
├── qualification/                 # Benchmark driver、固定路由生成、profile schema
├── routing/                       # StaticRouteResolver（只读 profile）
├── scheduler/                     # Work-Conserving Dispatcher
├── utilization/                   # 95% 软占用控制器
├── backends/
│   ├── cpu/                       # oneTBB arena + baseline/SSE/AVX/AVX2/AVX-512 variants
│   ├── alpaka/                    # portable kernel adapter
│   ├── cuda/                      # CUDA plugin（RTX 3060 Ti 真实验证）
│   ├── hip/                       # optional（无硬件，编译验证 only，标记 SKIPPED）
│   ├── sycl/                      # optional（无硬件，编译验证 only，标记 SKIPPED）
│   └── starpu_optional/           # PoC/ADR only
├── diagnostics/                   # 日志（spdlog）、诊断、设备报告
├── tests/
│   ├── unit/                      # 公共 API 单测（GoogleTest）
│   ├── classic/                   # E01–E16 经典实验
│   └── fault/                     # 故障注入
├── tools/
│   ├── acr_benchmark/             # acr-benchmark --profile quick|standard|full
│   ├── acr_status/                # acr-status
│   ├── acr_report/                # acr-report
│   └── acr_invalidate/            # acr-invalidate
├── docs/
│   ├── ADR-001-alpaka.md
│   ├── ADR-002-oneTBB.md
│   ├── ADR-003-hwloc.md
│   ├── ADR-004-cpu_features.md
│   ├── ADR-005-google-benchmark.md
│   ├── ADR-006-googletest.md
│   ├── ADR-007-starpu-optional.md
│   ├── ADR-008-cmake-fetchcontent.md
│   ├── ADR-009-cpu-only-build-gate.md
│   ├── dependency-lock.json       # 第三方依赖锁定（控制包 §11 要求）
│   └── forbidden-paths.md         # 禁止修改路径清单（Phase A 产出）
├── schemas/
│   ├── compute_config.example.yaml
│   ├── route_profile.example.json
│   └── route_profile.schema.json
├── examples/
│   ├── minimal_parallel_for.cpp   # 控制包提供
│   └── legacy_chunk_adapter.cpp   # 控制包提供
└── ci/
    └── build_matrix.md            # 构建矩阵说明
```

---

## 4. 依赖清单（CMake FetchContent，固定 tag）

| 库 | 版本 | 许可证 | 用途 | 编译进二进制 |
|---|---|---|---|---|
| alpaka | 1.2.0 | MPL-2.0 | 异构 kernel 抽象（CPU/CUDA/HIP/SYCL 单源） | header-only |
| oneTBB | 2022.0.0 | Apache-2.0 | CPU 任务执行、工作窃取、arena | shared lib |
| hwloc | 2.11.2 | BSD-3-Clause | CPU/NUMA/PCI 拓扑、绑定 | shared lib |
| google/cpu_features | 0.9.0 | Apache-2.0 | CPU ISA 安全门禁 | static lib |
| Google Benchmark | 1.9.1 | Apache-2.0 | Qualification 微基准框架 | static lib |
| GoogleTest | 1.15.2 | BSD-3-Clause | 单测框架（控制包 ADR 选择） | static lib |
| CLI11 | 2.5.0 | BSD-3-Clause | acr-* 工具 CLI | header-only |
| nlohmann/json | 3.11.3 | MIT | 指纹/profile/路由 JSON | header-only |
| spdlog | 1.15.0 | MIT | 日志 | header-only |
| fmt | 11.0.2 | MIT | 格式化（spdlog 依赖） | header-only |
| CUDA Toolkit | 11.8（系统已装） | NVIDIA EULA | CUDA backend 真实验证 | 系统库 |
| StarPU | 评估 only | LGPL-2.1 | ADR PoC，不强制 | 不引入 |

### 4.1 dependency-lock.json
按控制包 `05_OPEN_SOURCE_REUSE_PLAN.md §11` 要求，每个依赖记录：项目名、官方仓库、精确 tag/commit、SHA-256、SPDX 许可证、用途、是否编译、是否可选、平台/后端、本地补丁、升级风险。

### 4.2 FetchContent 策略
- 所有依赖通过 `FetchContent_Declare` + `FetchContent_MakeAvailable` 拉取
- `GIT_TAG` 固定到 release tag（不跟踪 main/develop）
- `GIT_SHALLOW TRUE`（只拉取指定 tag 的浅克隆）
- 首次构建联网下载到 `build/_deps/`（gitignored，不入仓）
- CPU-only 构建不依赖任何 GPU SDK：`ACR_BUILD_CUDA=OFF` 时不拉取 CUDA-related 依赖
- alpaka 的 `ALPAKA_ACC_GPU_CUDA_ENABLE` 由 `ACR_BUILD_CUDA` 控制

---

## 5. 构建系统

### 5.1 CMake 选项（lib/acr/CMakeLists.txt）
```cmake
option(ACR_BUILD_TESTS        "Build ACR unit tests"              ON)
option(ACR_BUILD_BENCHMARK    "Build acr-benchmark tool"          ON)
option(ACR_BUILD_TOOLS        "Build acr-status/report/invalidate" ON)
option(ACR_BUILD_EXAMPLES     "Build standalone examples"         ON)
option(ACR_BUILD_CUDA         "Enable CUDA backend (requires CUDA SDK)" OFF)
option(ACR_BUILD_HIP          "Enable HIP backend (requires ROCm)"  OFF)
option(ACR_BUILD_SYCL         "Enable SYCL backend (requires oneAPI/DPC++)" OFF)
option(ACR_ENABLE_STARPU      "Enable StarPU PoC (not for production)" OFF)
option(ACR_FETCHCONTENT_FULL  "Fetch all deps via FetchContent"   ON)
```
- 默认 `ACR_BUILD_CUDA=OFF`：保证 CPU-only 构建不依赖 GPU SDK
- 本机验证时显式 `-DACR_BUILD_CUDA=ON` 启用 RTX 3060 Ti
- 所有第三方 target namespaced（如 `alpaka::alpaka`、`TBB::tbb`），避免与未来 main 依赖冲突

### 5.2 构建命令
```powershell
# CPU-only（默认）
cmake -S lib/acr -B build/acr -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/acr -j

# 启用 CUDA（本机真实验证）
cmake -S lib/acr -B build/acr-cuda -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DACR_BUILD_CUDA=ON
cmake --build build/acr-cuda -j
```
- 工具链：MSYS2 MinGW64 g++ 16.1.0（C++20，alpaka 要求）
- CUDA：nvcc 11.8（CUDA 11.8 自带）
- 构建产物隔离在 worktree 的 `build/acr{,-cuda}/`，不污染主仓库

### 5.3 通过 astro_toolkit.py 编排
多步构建（configure + build + test + sha256 + 落盘日志）打包成 JSON 配置，用 `run_orchestrator` step 执行 cmake，避免逐步 RunCommand 触发沙箱确认。

---

## 6. 公共 API（实现规格）

控制包 `03_PUBLIC_API_SPEC.md` 提供 `acr.hpp` 骨架。本支线扩充完整实现：

### 6.1 核心 API
```cpp
namespace astro::compute {
  // 已在骨架：parallel_for, parallel_for_2d, parallel_tiles, run_for
  // 新增实现：
  template<class T, class MapKernel, class ReduceOp, class... Args>
  T parallel_reduce(KernelId, Range1D, T identity, MapKernel, ReduceOp, Args&&...);

  template<class ItemKernel, class... Args>
  Event parallel_batch(KernelId, std::size_t item_count, ItemKernel, Args&&...);

  template<class T, class Kernel, class Op, class... Args>
  Event parallel_scan(KernelId, ...);

  template<class RangeKernel, class... Args>
  Event parallel_chunks(KernelId, Range1D, RangeKernel, Args&&...);

  template<class T> class Buffer;          // 拥有式
  template<class T> class BufferView;       // 非拥有式
  class Event;                              // 异步同步原语
}
```

### 6.2 设计约束
- 公共头**不暴露** `tbb::*`、`alpaka::*`、`starpu_*`、`cuda*`、`hip*`、`sycl::*` 类型
- 所有 backend 类型只出现在 `.cpp`/内部头，通过 PIMPL 或 type-erased 接口隔离
- ACR_KERNEL 宏映射到 backend host/device 注解（alpaka `ALPAKA_FN_ACC`/`ALPAKA_FN_HOST`）
- 默认 FP32，允许 IEEE 754 正常末位差异；特殊 kernel 声明 FP64/FP64 accumulator/deterministic merge

### 6.3 Lazy initialization
- ACR runtime 全局 singleton，首次调用 `parallel_for` 等 API 时初始化
- 普通 AstroCS 启动不初始化 ACR、不探测 GPU、不输出警告
- 未标定时纯 CPU + 非阻断警告（仅在 ACR API 被调用时）

---

## 7. Phase 实现顺序与验收

### Phase A：仓库和依赖审计（主 Agent）
**任务**：
- 确认 worktree、构建系统、平台、测试框架（已完成）
- astro_toolkit.py 自检（已完成，ok:true）
- 识别允许新增目录与 CMake 接入点（已识别 lib/acr/）
- 生成 `docs/forbidden-paths.md` 禁止修改路径清单
- 为 alpaka/oneTBB/hwloc/cpu_features/Google Benchmark/StarPU/GoogleTest/CMake FetchContent 写 ADR
- 生成 `docs/dependency-lock.json` 依赖锁定

**验收**：审计报告、禁止路径清单、8 个 ADR、依赖矩阵、base commit 记录。

### Phase B：公共 API 和 CPU baseline（主 Agent）
**任务**：
- `BufferView`/`Buffer`/shape/stride/access mode/ownership
- `parallel_for`/`parallel_for_2d`/`parallel_tiles`/`parallel_reduce`/`parallel_batch`/`parallel_scan`/`parallel_chunks`/`run_for`
- `Event`（wait/ready）、取消、错误语义
- oneTBB CPU runtime（task_arena、worker pool、工作窃取）
- baseline scalar backend
- standalone examples（minimal_parallel_for、legacy_chunk_adapter）
- CPU-only CMake target + GoogleTest 单测

**验收**：Windows CPU 构建；API 单测通过；path guard 证明无算法目录修改；现有主线测试不受影响。

### Phase C：硬件发现、NUMA 和 ISA（subcoding agent，可与 D 部分并行）
**任务**：
- hwloc topology（package/core/PU/cache/NUMA/PCI）
- cpu_features（SSE/AVX/AVX2/FMA/AVX-512 子集）
- baseline/SSE/AVX/AVX2/AVX-512 函数版本 + 安全门禁
- 设备报告（hardware.json 指纹）
- 线程/NUMA 亲和性接口

**验收**：不支持 ISA 永不执行；baseline 始终可用；拓扑缺失降级；经典 CPU 实验结果与 Phase B 一致。

### Phase D：GPU portable backend（subcoding agent，与 C 部分并行）
**任务**：
- alpaka adapter（统一 kernel 编译为 CPU/CUDA）
- CUDA backend 真实验证（RTX 3060 Ti）
- backend feature gates（`ACR_BUILD_CUDA`）
- device/queue/buffer/copy
- 数据驻留和 event
- kernel 异常传播
- 无 GPU/驱动错误时降级到 CPU

**验收**：同一经典 kernel 源在 CPU 和 RTX 3060 Ti 上正确运行；CPU-only 不依赖 CUDA SDK；无伪造结果。

### Phase E：Qualification 和固定路由（主 Agent，依赖 B/C/D）
**任务**：
- `acr-benchmark --profile quick|standard|full` 独立工具
- `acr-status`/`acr-report`/`acr-invalidate`
- 空载提示
- 经典实验矩阵（E01-E16）
- 硬件指纹 schema + `dependency-lock.json`
- 固定路线生成（routes.json 只读）
- missing/stale/corrupt 处理
- 路由档案运行时只读

**验收**：可重复生成 profile；中断可恢复或明确作废；正式运行不修改 profile；无 profile 纯 CPU + 警告。

### Phase F：CPU+GPU 混合与工作保持（主 Agent，依赖 E）
**任务**：
- Range/Tile 拆分（不重叠 chunk）
- CPU+单 GPU、CPU+多 GPU
- 局部 reduction 合并
- 队列感知（finish = queue_wait + transfer + compute + merge）
- 首选设备忙时使用空闲合格设备
- 失败任务回退（不重放已执行中的 chunk）
- 数据驻留成本

**验收**：经典 mixed 实验覆盖完整不重复；结果正确；固定路由比例可执行；设备忙碌测试通过。

### Phase G：95% 资源控制（主 Agent，依赖 F）
**任务**：
- CPU 利用率软目标（所有 worker 可参与，通过批次/队列/优先级/让步控制）
- GPU 队列/批次软目标
- RAM/VRAM 容量限制（`limit = min(total*ratio, total-fixed_reserve)`）
- I/O 预算
- 配置热读取边界
- 记录实际利用率

**验收**：目标可调；95% 不等于少一个线程；系统保持可响应；不满足时记录而非伪报。

### Phase H：经典可靠性实验和持续测试（主 Agent + subcoding agents，依赖 G）
**任务**：
- 完成 `17_CLASSIC_EXPERIMENT_SUITE.md` 全部 E01-E16 必选实验
- sanitizer（ASan/UBSan/TSan）
- 故障注入（E15）
- 30 秒持续路线
- 多次进程重启
- profile 重载
- 内存泄漏和竞态测试
- CPU/GPU 结果报告（JSON case 级）

**验收**：必选实验全部通过；可选实验明确 SKIPPED 原因；无未解释崩溃/泄漏/越界/错误路由。

### Phase I：主线无副作用合并（**本次不做**，等用户授权）
全部 Phase A-H 验收通过后，由用户二次授权，按 `18_MAIN_MERGE_AND_DORMANT_INTEGRATION.md` 执行。

---

## 8. 经典实验套件（E01-E16）

按控制包 `17_CLASSIC_EXPERIMENT_SUITE.md` 完整实现。固定 seed：`0xA57C5AC20260802`。

| ID | 实验 | 验证能力 | 主设备 |
|---|---|---|---|
| E01 | Memory Copy/Read/Write/Triad | Map、连续内存、ISA、线程、GPU 基础、驻留 | CPU+CUDA |
| E02 | AXPY/FMA | 算术吞吐、FMA、单源 kernel | CPU+CUDA |
| E03 | Dot/Reduction Family | parallel_reduce、局部归约、CPU+GPU 合并 | CPU+CUDA |
| E04 | Tiled Matrix Transpose | 2D range、Tile、shared mem、边缘 | CPU+CUDA |
| E05 | 2D Convolution | Tile halo、二维拆分、mixed ownership | CPU+CUDA+Mixed |
| E06 | Bilinear Affine Resampling | 坐标计算、gather、2D Tile | CPU+CUDA |
| E07 | Histogram 256 bins | 原子竞争、局部副本、整数归约 | CPU+CUDA+Mixed |
| E08 | Prefix Scan | 依赖模式、专用库适配（CUB/oneDPL） | CPU+CUDA |
| E09 | Gather/Scatter | 不规则内存、稀疏度、atomic、输入验证 | CPU+CUDA |
| E10 | Branch Divergence (Mandelbrot) | GPU warp divergence、工作量不均 | CPU+CUDA |
| E11 | GEMM 成熟库适配 | vendor library handle 隔离、cuBLAS/oneMKL | CPU+CUDA |
| E12 | FFT Round-trip | 专用库、cuFFT/FFTW、计划缓存 | CPU+CUDA |
| E13 | CPU+GPU Mixed Partition | 0/25/50/75/100% 拆分、coverage bitmap | CPU+CUDA+Mixed |
| E14 | Resource Utilization Controller | 50/80/95/100% 目标点、worker 参与 | CPU+CUDA |
| E15 | Failure and Fallback | backend 缺失/profile corrupt/device lost | CPU（注入） |
| E16 | Concurrency/Cancellation/Lifetime | 多调用者、取消、Buffer 生命周期、100 次重启 | CPU+CUDA |

**容差**：
- FP32 通用：`abs(a-b) <= 1e-5 + 5e-5 * max(abs(a),abs(b))`
- FP64 通用：`abs(a-b) <= 1e-12 + 1e-11 * max(abs(a),abs(b))`
- 整数实验：exact

**报告格式**：每个 case 输出 JSON（experiment_id/case_id/seed/backend/device/precision/size/correct/max_abs/max_rel/rmse/median_ms/p95_ms/status/reason），不得只输出汇总表。

---

## 9. 提交规则

### 9.1 原子提交（按控制包 §4）
1. `docs(acr): freeze bottom-only scope and dependency ADRs`（Phase A）
2. `build(acr): add isolated optional targets and dependency gates`（Phase A 收尾）
3. `feat(acr): add public API and CPU baseline runtime`（Phase B）
4. `feat(acr): add topology and ISA discovery`（Phase C）
5. `feat(acr): add portable accelerator backend`（Phase D）
6. `feat(acr): add qualification and immutable routes`（Phase E）
7. `feat(acr): add mixed CPU GPU scheduler`（Phase F）
8. `feat(acr): add utilization controller`（Phase G）
9. `test(acr): add classic experiment suite and fault injection`（Phase H）
10. `docs(acr): add merge evidence and dormant integration report`（Phase H 收尾，为 Phase I 准备）

### 9.2 path guard（每次 commit 前）
```powershell
# 检查 diff 是否越界
git diff --name-only HEAD | findstr /V "^lib/acr/ ^工程控制/tasks/acr/ ^工程控制/evidence/acr/"
# 若有输出 = 越界，立即停止并回退
```
每次 commit 前运行，发现越界立即 `git reset HEAD~1` 并报告。

### 9.3 commit 工具
- 通过 `astro_toolkit.py` 的 `git_add` + `git_commit` + `git_push` + `git_log` step 打包执行
- commit message 用 `-F` 文件参数（避免长 message 触发扫描超时）
- 临时 JSON/msg 文件命名 `tools/_acr_<purpose>.json` / `tools/_acr_<purpose>.txt`，使用后删除
- 每个 Phase 完成后 push 一次（PowerShell，非 WSL）

---

## 10. 并行化策略（按 user_rules）

### 10.1 依赖关系
```
Phase A (审计/ADR/forbidden-paths)
    └─ Phase B (公共 API + CPU baseline)  ← 后续所有 Phase 的基础
         ├─ Phase C (硬件/ISA)  ┐
         └─ Phase D (GPU backend)┘ 可并行
              └─ Phase E (Qualification/路由)  依赖 B/C/D
                   └─ Phase F (混合调度)  依赖 E
                        └─ Phase G (95% 控制)  依赖 F
                             └─ Phase H (经典实验)  依赖 G
                                  ├─ E01-E04 可并行（基础 Map/Reduce）
                                  ├─ E05-E07 可并行（Tile/Stencil/Atomic）
                                  ├─ E08-E10 可并行（Scan/Gather/Branch）
                                  ├─ E11-E12 可并行（库适配）
                                  ├─ E13-E14 可并行（Mixed/Utilization）
                                  └─ E15-E16 可并行（Fault/Lifetime）
```

### 10.2 subcoding agent 分配
- Phase A、B：主 Agent（建立基础，确定 API 形态）
- Phase C、D：并行 2 个 subcoding agent（硬件发现 vs GPU backend，接口由 B 确定）
- Phase E、F、G：主 Agent（依赖链紧密，需集成调试）
- Phase H：分组并行 subcoding agent（按 10.1 分组），主 Agent 集成验收
- 每个 subcoding agent 任务必须：输入输出明确、可独立验收、不修改同一文件

### 10.3 主 Agent 职责
- 接口定义（API、Buffer、Event、Kernel Registry）
- path guard 执行
- 集成测试
- 验收（按 checklist.md）
- commit 编排（astro_toolkit.py）

---

## 11. 测试与验收

### 11.1 构建矩阵
- Windows + MSYS2 MinGW64 g++ 16.1.0，CPU-only（默认）
- Windows + MSYS2 MinGW64 + CUDA 11.8，`ACR_BUILD_CUDA=ON`（本机真实验证）
- Debug/Release
- 无 GPU SDK 时 CPU-only 必须构建成功

### 11.2 验收门禁
按 `checklist.md` 逐项检查。关键门禁：
- path guard 证明算法目录零修改
- CPU-only 构建无 GPU SDK 依赖
- CUDA backend 真实通过（RTX 3060 Ti，不伪造）
- 经典实验 E01-E16 全部 PASS 或 SKIPPED（注明原因）
- sanitizer 无泄漏/竞态/use-after-free
- 普通启动不初始化 ACR（无副作用）

---

## 12. 交付（Phase I 时执行，本次不交付）

按控制包 `13_DELIVERY_PACKAGE_RULES.md`：
- A. Control Package（本控制包 + ADR + dependency-lock + API/schema/benchmark 规范）
- B. Complete Source Snapshot（merge 后 main 完整源码）
- C. Evidence Package（构建/测试/经典实验/sanitizer/path guard/回归）
- D. Merge Report

本次 Phase A-H 完成后，生成 Evidence Package 草稿供用户审核，确认后方可进入 Phase I。

---

## 13. 风险与缓解

| 风险 | 缓解 |
|---|---|
| R1 alpaka 工具链差异 | 公共 API 隔离、feature gate、CPU-only 永远可用、版本锁定 |
| R2 第三方 ABI 冲突 | namespaced CMake target、dependency-lock、优先复用仓库已有（本仓库无第三方，全 FetchContent） |
| R3 CMake 合并冲突 | ACR 子目录独立 CMake，不引入顶层 CMakeLists.txt |
| R4 合并后副作用 | lazy initialization、普通启动不创建 runtime singleton |
| R5 CPU-only 被 GPU SDK 绑死 | `ACR_BUILD_CUDA` 默认 OFF、CI 无 GPU SDK 构建门禁 |
| R6 自动向量化不足 | 编译器向量化报告、多 ISA build、经典实验验证 |
| R7 固定路由环境变化失效 | 完整指纹、stale 警告、用户自主重新 benchmark |
| R8 95% 被误解为少线程 | 软利用率目标、所有线程可参与、记录实际值 |
| R9 CPU+GPU 切分错误 | coverage bitmap、整数 exact 实验、故障注入 |
| R10 Benchmark 测假象 | 分离 kernel/transfer/resident、预热、多轮、原始结果保留 |
| R11 StarPU 过度引入 | 只做 ADR/PoC、不强制、固定路由语义优先 |
| R12 支线越界改算法 | path guard、禁止路径清单、提交审查、diff 证明零算法改动 |

---

## 14. 决策记录（关键）

1. worktree 隔离在 `run/worktrees/acr/`（沙箱限制，主仓库工作区不动）
2. ACR 在 `lib/acr/` 内独立 CMake，不引入顶层 CMakeLists.txt
3. 依赖全部 CMake FetchContent 拉取，固定 tag，不入仓
4. 测试框架选 GoogleTest 1.15.2（控制包 ADR 选择）
5. CUDA backend 启用真实验证（RTX 3060 Ti，本机 CUDA 11.8）
6. 本次范围 Phase A-H，Phase I 合并 main 等用户二次授权
7. 公共 API 不暴露第三方类型，PIMPL/type-erased 隔离
8. ACR lazy initialization，合并后普通 AstroCS 运行无副作用
9. 路由离线固化，正式运行不在线学习
10. 95% 软占用，所有线程可参与
11. 默认 FP32，允许正常末位差异
12. CPU-only 构建不依赖任何 GPU SDK
