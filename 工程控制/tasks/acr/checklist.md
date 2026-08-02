# ACR 底层支线验收检查表（checklist.md）

**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`
**控制包**: AstroCS_ACR_Branch_Control_Package_V1.2_2026-08-02
**本次范围**: Phase A–H（Phase I 合并 main 等用户授权）

---

## 0. 隔离与环境

- [x] 已创建 `feature/astrocompute-runtime` 分支（从 `origin/main` 8f50519）
- [x] 已记录 base commit `8f5051946e9ea824ceefa6a90a071de7cad31a98`
- [x] worktree 隔离在 `run/worktrees/acr/`（主仓库工作区不动）
- [x] 已检查并自检 `astro_toolkit.py`（ok:true，git_status + list_dir 通过）
- [x] 已建立算法目录禁止修改清单（见 spec.md §1.3）
- [ ] path guard 脚本就位并在每次 commit 前运行
- [ ] 没有删除或注释现有 OpenMP
- [ ] 普通 AstroCS CLI 行为未改变（合并后验证）

---

## Phase A：仓库和依赖审计

- [ ] `lib/acr/docs/forbidden-paths.md` 禁止修改路径清单产出
- [ ] ADR-001 alpaka（用途、版本、PoC、集成边界）
- [ ] ADR-002 oneTBB（CPU runtime、arena、与现有 OpenMP 共存）
- [ ] ADR-003 hwloc（拓扑、NUMA、降级策略）
- [ ] ADR-004 google/cpu_features（ISA 门禁、AVX-512 子集精确匹配）
- [ ] ADR-005 Google Benchmark（Qualification 框架、不替代单测）
- [ ] ADR-006 GoogleTest（测试框架选择，控制包 ADR 授权）
- [ ] ADR-007 StarPU 可选评估（不强制，固定路由优先）
- [ ] ADR-008 CMake FetchContent（依赖拉取策略、版本锁定）
- [ ] ADR-009 CPU-only 构建门禁（不依赖 GPU SDK）
- [ ] `lib/acr/docs/dependency-lock.json` 依赖锁定（项目名/仓库/tag/SHA-256/SPDX/用途/编译/可选/平台/补丁/升级风险）
- [ ] 依赖选择矩阵（dependency selection matrix）
- [ ] 审计报告（仓库结构/构建系统/平台/CI/测试框架/依赖方式）
- [ ] Phase A 原子提交 `docs(acr): freeze bottom-only scope and dependency ADRs`

---

## Phase B：公共 API 和 CPU baseline

- [ ] `include/astro/compute/acr.hpp` 公共 API 完整（parallel_for/for_2d/tiles/reduce/batch/scan/chunks/run_for + Buffer/BufferView/Event/NumericPolicy/ExecutionHints）
- [ ] 公共头不暴露 `tbb::`/`alpaka::`/`starpu_*`/`cuda*`/`hip*`/`sycl::` 类型
- [ ] `BufferView` shape/stride/access mode/ownership/subview
- [ ] `Event` wait/ready/cancel/error 语义
- [ ] oneTBB CPU runtime（task_arena、worker pool、工作窃取）
- [ ] baseline scalar backend
- [ ] `parallel_for`/`parallel_for_2d`/`parallel_tiles`/`parallel_reduce`/`parallel_batch`/`parallel_scan`/`parallel_chunks` 实现
- [ ] standalone examples（`examples/minimal_parallel_for.cpp`、`examples/legacy_chunk_adapter.cpp`）
- [ ] CPU-only CMake target 构建通过（`ACR_BUILD_CUDA=OFF`）
- [ ] GoogleTest 单测：空范围/单元素/非整 Tile/极小大范围/shape-stride/只读写/host-device 生命周期/同步事件取消/异常错误码/别名/越界
- [ ] path guard 证明无算法目录修改
- [ ] 现有主线测试不受影响
- [ ] Phase B 原子提交 `feat(acr): add public API and CPU baseline runtime`

---

## Phase C：硬件发现、NUMA 和 ISA

- [ ] hwloc topology（package/core/PU/cache/NUMA/PCI）
- [ ] cpu_features ISA 检测（SSE/AVX/AVX2/FMA/AVX-512 子集精确）
- [ ] baseline/SSE/AVX/AVX2/AVX-512 函数版本
- [ ] 安全门禁（不支持 ISA 永不执行）
- [ ] 设备报告 `hardware.json` 指纹（CPU vendor/model/stepping/ISA/cache/NUMA/GPU UUID/PCI/driver/compiler/build）
- [ ] 线程/NUMA 亲和性接口
- [ ] 拓扑缺失降级（不拒绝 CPU baseline）
- [ ] 经典 CPU 实验结果与 Phase B 一致
- [ ] Phase C 原子提交 `feat(acr): add topology and ISA discovery`

---

## Phase D：GPU portable backend

- [ ] alpaka adapter（统一 kernel 编译为 CPU/CUDA）
- [ ] CUDA backend 真实验证（RTX 3060 Ti，CUDA 11.8）
- [ ] backend feature gates（`ACR_BUILD_CUDA`）
- [ ] device/queue/buffer/copy
- [ ] 数据驻留和 event
- [ ] kernel 异常传播
- [ ] 无 GPU/驱动错误时降级到 CPU
- [ ] 同一经典 kernel 源在 CPU 和 RTX 3060 Ti 上正确运行
- [ ] CPU-only 构建不依赖 CUDA SDK（`ACR_BUILD_CUDA=OFF` 时无 CUDA 引用）
- [ ] 无真实硬件不得虚报（HIP/SYCL 无硬件标记 SKIPPED）
- [ ] Phase D 原子提交 `feat(acr): add portable accelerator backend`

---

## Phase E：Qualification 和固定路由

- [ ] `acr-benchmark --profile quick|standard|full` 独立工具
- [ ] `acr-status`/`acr-report`/`acr-invalidate` 独立工具
- [ ] 空载提示（不替用户判断、不检测/阻止/关闭其他应用）
- [ ] 经典实验矩阵（E01-E16，按 17_CLASSIC_EXPERIMENT_SUITE.md）
- [ ] 硬件指纹 schema（`schemas/route_profile.schema.json`）
- [ ] `dependency-lock.json` 指纹部分
- [ ] 固定路线生成（routes.json 只读）
- [ ] missing/stale/corrupt 处理（纯 CPU + 非阻断警告）
- [ ] 路由档案运行时只读（不在线学习/回写）
- [ ] 测试矩阵裁剪（CPU ISA/线程代表点/精度/设备组合/尺寸桶）
- [ ] 计时方法（预热 3 次/正式 9 次或累计 1 秒/GPU device event/H2D-D2H 分离/median-p95-MAD）
- [ ] 正确性先于性能（未通过不得进入路由候选）
- [ ] 可重复生成 profile；中断可恢复或明确作废
- [ ] Phase E 原子提交 `feat(acr): add qualification and immutable routes`

---

## Phase F：CPU+GPU 混合与工作保持

- [ ] Range/Tile 拆分（不重叠 chunk）
- [ ] CPU+单 GPU
- [ ] CPU+多 GPU（本机单 GPU，多 GPU 部分 SKIPPED 注明）
- [ ] 局部 reduction 合并
- [ ] 队列感知（finish = queue_wait + transfer + compute + merge）
- [ ] 首选设备忙时使用空闲合格设备
- [ ] 失败任务回退（不重放已执行中的 chunk）
- [ ] 数据驻留成本（写入后失效其他副本）
- [ ] 经典 mixed 实验覆盖完整不重复（E13）
- [ ] coverage bitmap 每个工作项恰好一次
- [ ] 输出与单设备 reference 一致
- [ ] 固定路由比例可执行
- [ ] 设备忙碌测试通过
- [ ] Phase F 原子提交 `feat(acr): add mixed CPU GPU scheduler`

---

## Phase G：95% 资源控制

- [ ] CPU 利用率软目标（所有 worker 可参与，批次/队列/优先级/让步）
- [ ] GPU 队列/批次软目标
- [ ] RAM/VRAM 容量限制（`limit = min(total*ratio, total-fixed_reserve)`）
- [ ] I/O 预算（有界队列）
- [ ] 配置热读取边界（明确支持/不支持）
- [ ] 所有 CPU 线程可参与（95% 不等于少一个线程）
- [ ] 记录实际利用率（平均/p95/控制误差/worker 参与）
- [ ] 目标点 50/80/95/100% 验证（E14）
- [ ] 系统保持可响应（取消/状态查询）
- [ ] 不满足时记录而非伪报
- [ ] 控制器不修改 kernel 性能估计/路由 profile/Qualification 原始数据
- [ ] Phase G 原子提交 `feat(acr): add utilization controller`

---

## Phase H：经典可靠性实验和持续测试

- [ ] E01 Memory Copy/Read/Write/Triad
- [ ] E02 AXPY/FMA
- [ ] E03 Dot/Reduction Family
- [ ] E04 Tiled Matrix Transpose
- [ ] E05 2D Convolution
- [ ] E06 Bilinear Affine Resampling
- [ ] E07 Histogram 256 bins
- [ ] E08 Prefix Scan
- [ ] E09 Gather/Scatter
- [ ] E10 Branch Divergence (Mandelbrot)
- [ ] E11 GEMM 成熟库适配（cuBLAS/oneMKL）
- [ ] E12 FFT Round-trip（cuFFT/FFTW）
- [ ] E13 CPU+GPU Mixed Partition（0/25/50/75/100%）
- [ ] E14 Resource Utilization Controller
- [ ] E15 Failure and Fallback
- [ ] E16 Concurrency/Cancellation/Lifetime
- [ ] 每实验输出 JSON case 级报告（experiment_id/case_id/seed/backend/device/precision/size/correct/max_abs/max_rel/rmse/median_ms/p95_ms/status/reason）
- [ ] ASan/UBSan 通过
- [ ] TSan CPU 路径通过
- [ ] 30 秒持续候选路线
- [ ] 进程连续启动 100 次
- [ ] profile 重载
- [ ] 内存泄漏检查
- [ ] race/deadlock 检查
- [ ] 必选实验全部 PASS；可选实验明确 SKIPPED 原因
- [ ] Phase H 原子提交 `test(acr): add classic experiment suite and fault injection`
- [ ] 收尾文档提交 `docs(acr): add merge evidence and dormant integration report`（为 Phase I 准备）

---

## 开源复用

- [ ] alpaka ADR + PoC（验证编译 CPU/CUDA）
- [ ] oneTBB 集成（与现有 OpenMP 共存验证）
- [ ] hwloc 集成和降级
- [ ] cpu_features ISA 门禁
- [ ] Google Benchmark 集成
- [ ] GoogleTest 集成
- [ ] CLI11/nlohmann-json/spdlog-fmt 复用
- [ ] StarPU 可选评估完成（ADR，不强制）
- [ ] dependency-lock.json 完整
- [ ] SPDX 许可证和 NOTICE 清单

---

## 底层能力

- [ ] 公共 API 完整
- [ ] CPU baseline
- [ ] CPU ISA versions（SSE/AVX/AVX2/AVX-512 实际子集）
- [ ] CUDA 真实 backend 通过（RTX 3060 Ti）
- [ ] CPU-only 无 GPU SDK 构建
- [ ] buffer/queue/event/residency
- [ ] Qualification 和 immutable routes
- [ ] missing/stale/corrupt handling
- [ ] CPU+GPU mixed
- [ ] multi-GPU 可用时验证（本机单 GPU，SKIPPED 注明）
- [ ] 95% 软占用且所有线程可参与
- [ ] device failure fallback

---

## 合并前门禁（Phase I，等用户授权后执行）

- [ ] 已同步最新 main
- [ ] 合并预演无未解决冲突
- [ ] 主线测试通过
- [ ] 普通启动无 ACR 初始化/警告
- [ ] feature 分支已推送
- [ ] 已使用 `--no-ff` 或符合仓库政策的可追溯合并
- [ ] 合并后再次测试通过
- [ ] ACR 在 main 中保持 dormant
- [ ] 后续算法集成明确另开分支

---

## 交付（Phase I 时）

- [ ] Control Package
- [ ] merge 后完整 Source Snapshot
- [ ] Evidence Package
- [ ] Merge Report
- [ ] manifest 和 SHA-256
- [ ] ZIP 完整性验证
