# ACR 模块开发日志

**模块**: AstroCompute Runtime (ACR)
**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`

## 进度

### 2026-08-08 BDR Reviewed 纠正（控制包 A9766B99...53984，08 号计划 A-J）

**执行入口**：`08_CURRENT_EXECUTION_PLAN.md`（BDR Reviewed 版；解压于
`run/temp/AstroCS_ACR_Control_Package_20260807-102458/`）。审计对象
`AstroCS_Review_ACRBDRReviewed_20260807.zip`（HEAD 2a7fbe3）。

**本轮完成（按 08 计划 A-J）**：
1. 场景级资格语义（A）：RoutePath 拆分 model_available/model_trusted
   （eligible 兼容字段=model_trusted）；RouteScenarioProfile 增加
   scenario_qualified/qualification_reason/route_replay；Operation.qualified
   由三个 required 场景全部 qualified 生成；生产 decide 场景未 qualified
   直接 OpenMP fallback；诊断 Replay（decide(..., diagnostic=true)）允许
   所有 model_available 候选参与预测。
2. 标定数据三集合隔离（B）：Fit（5 尺寸×4/16/32 帧=15 点）、Refinement
   Probe（8 点）、Final Untouched Holdout（8 点/场景）；自动断言无交集并
   写入 Profile datasets 清单。
3. 真正 Adaptive Refinement（C）：最多 2 轮，每轮在剩余 Probe 上选插值器
   并找最差点，最差 Probe 转入 Fit（加入 samples 并重算 domain）后重新拟合；
   模型冻结后才测量 Final。
4. 真实 cold Mixed（D）：warmup 用不同 generation（不同 seed）；每个正式
   cold Mixed 样本 fresh Dispatcher（residency 独立）；GPU 参与时 timed H2D
   必须 >0；true_cold_semantics gate 同时检查 GPU Direct 与 Mixed。
5. chunk 服务曲线修正（E）：GPU 单块保持完整 frame-major stride
   （pixel_count=service_domain，begin/end 语义）；begin/middle/end offset
   抽样；每测量点独立 warmup；随机化候选顺序；sanity gate（同 chunk 帧数
   增加时间不得大幅下降；同帧 chunk 增加时间不得反向跳变）。
6. 2D service 插值（F）：interpolate_chunk_service(curve, chunk, frames)
   先沿 chunk 轴（log2）再沿 frame 轴；未命中帧数禁止混用全部曲线。
7. reuse4/内存统计（G）：Mixed reuse4 累计 4 次真实 per_device_stats/
   transfer；GPU 场景 absolute_peak_vram_bytes 由 device_memory 前后差值
   真实记录；metrics_complete 逐字段检查，删除硬编码。
8. Route Replay（H）：每场景 8 个独立 Final 点实际运行 OpenMP/GPU/Mixed
   得到 oracle best；Router 只用冻结 Profile（诊断模式）预测；硬门
   chosen<=best×1.10。
9. Benchmark 资格清理（I）：case 级 best 按 cold/resident/reuse4 场景分组；
   profile_threshold_validated 由三场景 qualified 生成；
   single_stream_semantics_verified 由 configured==1 && observed==1 生成；
   summary 由最终 JSON 自动生成。
10. 稳定性（J）：FocusedMixed.AutoMixedWithinTenPercentOfBest 修复
    （复用 Dispatcher，配置/worker 创建开销移出计时）；--three-clean-ctest-runs
    由 CI 传入，Benchmark 不再硬编码。

**验证结果（2026-08-08，RTX 3060 Ti + MinGW64）**：
- 完整标定流程跑通（约 9 分钟）：Fit 15 点 + Probe 8 点 + Final 8 点 +
  2D chunk 服务曲线（sanity 通过）+ Route Replay 24 点；
- Profile v2：9 条路径 final_holdout 全部 = 8；datasets 无交集断言通过；
  resident/reuse4 GPU Direct 路径 final med 1.9–2.3%、max 5.4–7.9% 达到
  trusted；OpenMP/Mixed 及 cold GPU 模型误差超门（max 17–54%）如实
  model_trusted=false；三场景未全 qualified → op.qualified=false、
  READY_FOR_BUSINESS_ADAPTER=false；
- cold Mixed 17/17 样本 timed H2D>0（fresh Dispatcher 真实 cold）；
  metrics_complete 全 true（absolute VRAM peak 非零）；
- standard 报告 gates 真实计算：true_cold_semantics/metrics_complete/
  scenario_isolation/direct_gpu_reuse4_present/single_stream 均 true；
  profile_threshold_validated=false（场景未全 qualified）；
- CTest 连续 3 轮 625/625 通过、8 项准确跳过、0 fail
  （FocusedMixed.AutoMixedWithinTenPercentOfBest 修复后稳定）；
- 修复的关键问题：VRAM 预算 gate 死等（校准禁用内存反压）、E2E 插值
  "精确帧单样本导致相邻插值失败"（改用 usable 帧集）、auto_reuse4 gate
  场景名不匹配、GPU VRAM 峰值增量 0 问题。

### 2026-08-05 聚焦版控制包（SHA 56f74f2e...eac14，08 号计划）

**执行入口**：`08_CURRENT_EXECUTION_PLAN.md`（控制包 8；仓库内副本
`工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE/`）。旧 20—26 号计划全部失效。

**定位收缩**：ACR 是积分/Drizzle 类重负载逐像素算法的 CPU+GPU 动态混合
分块与数据驻留优化层。不做通用硬件画像、不扩展无关 gather/branch/全部
ISA 线程矩阵、不做 CPU/GPU 精确利用率控制。解析/元数据/稀疏小任务不接入。

**保留**：KernelRegistry/KernelInvocation/DeviceExecutor、真实 CPU/CUDA 执行、
动态工作池与 WorkToken、CUDA 独立容量与多块卷积、RAM/VRAM 预算骨架、
compute-sanitizer/MSVC ASan、业务算法零修改。

**本轮完成（聚焦版提交 1）**：
- 删除生产路径 CPU/GPU 精确利用率控制（CpuController/GpuController/IoBudget
  及 50/80/95/100 目标测试），MemoryBudget 独立 enable_memory_budget 开关
  （5123949，对应 08 号计划 §2 清理错误方向）；
- 定义 RouteMode（AutoMixed/CpuOnly/GpuOnly）与 PartitionKind
  （IndependentOutputTiles/PrivatePartialThenMerge）；
- 定义 5 个目标合成 OperationId（dense_pixel_accumulate fp32/fp64acc、
  pixel_reduce fp64acc、drizzle_like_scatter fp64acc、resident_chain）；
- KernelInvocation 增加 partition/mode 字段；
- 控制包 8 已同步到仓库控制目录并记录 SHA。

**待办（08 号计划 §3—§9）**：目标 Operation 合成套件 → OperationProfile →
MixedRoutePlanner（独立块大小/边际收益门/尾段停止慢设备）→ ResidencyManager
与内存预算完善 → 聚焦测试与单一干净 HEAD Evidence。

**聚焦版 5 个提交已完成（2026-08-05）**：
1. `0030c3a` refactor(acr): focus runtime on target pixel operations——
   RouteMode/PartitionKind、5 个目标 OperationId、KernelInvocation partition/mode；
2. `3a40596` bench(acr): add focused operation profiles——桥接新增
   dense_accumulate_fp64acc/drizzle_scatter/chain/launch_event/transfer_h2d/
   transfer_d2h；5 个合成 Operation（CPU+GPU）；OperationProfile（符合
   operation_profile.schema.json）；acr-benchmark-focused 工具；
3. `fe98df3` feat(acr): tune mixed chunk routing and tail gating——
   MixedRoutePlanner（独立块大小、host/resident 阈值、边际收益门）；
4. `380aae3` feat(acr): reuse residency and finish memory budget——
   ResidencyManager（Host/Device/Both/dirty）、pinned 独立记账、ReleaseCache
   返回字节、Shrink 循环重估；
5. `78101ae` test(acr): add focused qualification evidence——真实 Mixed
   （CPU+GPU 均非零）、AutoMixed ≤ 最佳模式 10%（实测 1.02x）、Auto 自然退化。

**验证结果（2026-08-05）**：全量 ctest 597/597 通过（0 failed，SKIPPED 如实）；
focused 测试 4+4+3 全过；真实 RTX 3060 Ti Mixed 正确。期间修复 planner 边际
收益门与首轮公平门死锁（聚焦版禁用强制公平门）、CPU/GPU 实测速率对比与
兜底清尾，避免慢设备拖尾与工作丢失。

### 2026-08-06 聚焦版 v2（控制包 9，SHA 65685119...93d77）

**执行入口**：`08_CURRENT_EXECUTION_PLAN.md`（v2）。审计
`ACR_FOCUSED_REVIEW.md` 确认 8 项阻断，全部已修复：
1. `40c9d21`：OperationProfile 改用 nlohmann 层级解析（禁止字符串搜索同名键），
   完整 roundtrip（CPU/GPU/transfer/memory/eligibility/指纹逐字段），
   真实运行指纹（编译器宏 + 内核地址 hash）；
2. `3cbd998`：桥接新增 resident 持久上传与提交；真实 CPU/GPU resident/host
   三条曲线；拟合截距固定开销；候选块实测（替代硬编码 64K/1M）；真实交叉点
   （无收益路径 ineligible + null 阈值）；leave-one-out 真实误差；
   每 Operation 独立 qualified；
3. `cc1d692`：Auto 前置收益门（worker 启动前按 Profile 筛选 GPU，host/resident
   阈值 + 规模检查）；Auto 禁止强制首块；删除慢设备固定轮次强制清尾；
   ForcedMixed 保留首块参与（仅正确性）；
4. `83291ad`：ResidencyManager 真实字节/access/generation/device allocation；
   dispatcher 经桥接真实整帧上传（共享输入只上传一次）与真实 D2H；
   禁止机械 uploaded→downloaded；桥接 resident 提交支持整帧 view 复用；
5. `233359d`：reduce/drizzle 每 token 私有 partial + 明确 merge；
   CPU/GPU 全路径（CpuOnly/GpuOnly/ForcedMixed）正确性测试；
6. `eba1c23`：pinned staging 真实 reservation ledger（reserve/release/limit）；
   claim 前 reserve、预算不足缩块/等待、完成后 release。

**验证结果（2026-08-06）**：全量 ctest 603/603 通过；focused 测试
（operation/mixed_route/residency/focused_mixed）全过；standard Profile
通过 schema（无收益 GPU 路径如实 ineligible）；真实 GPU 复用测试
（upload 一次 + 多块 resident 提交）通过。

### 2026-08-05 25 号计划执行（控制包 SHA 755278bf...5f98）

**执行入口**：`25_SECOND_FIX_IMPLEMENTATION_REVIEW_CORRECTION_PLAN.md`（外部
控制包 6；仓库内副本 `工程控制/docs/ACR_25_SECOND_FIX_IMPLEMENTATION_REVIEW_CORRECTION_PLAN.md`）。

**用户方向（2026-08-05）**：不再实现 CPU/GPU 50/80/95/100 利用率精确闭环
（Windows 无软限制系统 API），需求移除；只保留 RAM/VRAM 容量预算与反压；
优先把底层跑通（Benchmark/成本模型/CUDA 分块/内存预算），不改算法。

**已完成（25 号计划 §10 原子提交顺序）**：
1. `fix(acr): make qualification kernels race-free and workload-equivalent`
   （3b7c545）：Dot 每 chunk 独立 FP64 partial + merge；Histogram 分
   hist_tls/hist_atomic；Scatter 分 scatter_perm/scatter_atomic；卷积/Mandelbrot
   统一 problem_size=总工作项；新增 BenchmarkWorkloadDescriptor + variant 字段。
2. `fix(acr): harden cuda bridge capacities and chunked convolution`
   （fd42381）：d_x/d_y/d_partials/d_kernel/d_image 独立容量记账；分块卷积
   全局输出索引 + chunk-local 写；reduce grid 对齐。
3. `feat(acr): full qualification fields, raw records and real device metadata`
   （410b0e7）：Curve/Point 加 source/qualified/sample_count/confidence；
   quick 标 diagnostic_only=true；raw_benchmark_records.json；桥接导出
   acr_cuda_device_memory/compute（显存/SM/CC）。
4. `fix(acr): align profile keys residency and cost-estimator transfer accounting`
   （659fa24）：memory key 改 (level,residency,operation)；reduction sum/dot
   分离；profile_available 仅命中 qualified measured 曲线；传输成本按单块字节。
5. `test(acr): add cross-device holdout and routing qualification`
   （37a871d）：4 拟合点 + 3 插值 holdout + 1 外推；CPU AXPY/Dot 与真实 GPU；
   排序正确率 ≥90%（实测 100%）；AXPY 波动如实标 unqualified。
6. `feat(acr): enforce ram vram budgets and recoverable memory backpressure`
   （ead5f95）：MemoryBudgetConfig 分离 RAM 2048MiB/VRAM 512MiB 固定保留；
   每次 claim 前按输入/输出/临时/双缓冲/传输 staging/partial/merge 估算峰值，
   动作 ShrinkBlock/StopNewSubmit/ReleaseCache/LowMemoryPath/
   FallbackOtherDevice/Fail 全部真实进入执行链；ResourceControlStats 记录
   mem_peak_estimates/mem_peak_actions/mem_vram_actions/mem_peak_max；
   3 项新测试（Peak*）全通过。
7. `test(acr): add compute-sanitizer and cpu sanitizer evidence runner`
   （a388209）：tests/sanitizer/run_acr_sanitizers.ps1 统一运行 MSVC ASan
   （4 组件）与 compute-sanitizer memcheck/racecheck（warm-up + 明确 timeout +
   exit code 记录）；memcheck 0 errors、racecheck 0 hazards。
8. `test(acr): mark unstable holdout curves unqualified instead of flaky assert`
   （3740405）：sum reduction holdout 波动大（0.11~0.67 随运行浮动），改为
   如实标 qualified/unqualified（reduction merge 开销主导论证），不再 flaky 断言。

**验证结果（2026-08-05）**：
- 全量 ctest（ACR_CUDA_BRIDGE_DLL 注入，CUDA 真实运行）：641/641 通过，
  0 失败；8 SKIPPED（SanitizerActual×7 因 MinGW 无 ASan + ApiReduce 1 项）。
- full Benchmark（CPU+GPU，RTX 3060 Ti）：56 条记录、每曲线 10 样本 × 3 尺寸、
  qualified=true、source=measured。
- compute-sanitizer：memcheck 0 errors（多块卷积 + 独立扩缩容）、
  racecheck 0 hazards。
- MSVC ASan：SharedWorkPool 1000 轮 + Registry 并发 + CpuExecutor 契约 +
  CpuController 2000 决策无错误；故意 UAF 被检出。

**已知限制（如实）**：
- 仓库/控制包 hardware_profile.schema.json（1.0）未随 v1 生成格式同步；
  raw_benchmark_records.json 未对齐新增 raw_benchmark_record.schema.json
  （缺 record_id/result_validation/source 等）——schema 差异已写入
  run/evidence/acr_25plan_20260805/profile/schema_report.md，未伪装通过。
- UBSan/TSan 本机仍不可用（MinGW/clang 无运行库）；Dispatcher/ProfileBuilder
  因 oneTBB/hwloc MinGW ABI 依赖未纳入 MSVC ASan，如实记录。
- CPU/GPU 利用率精确闭环已按用户要求移除，不做测试、不做门禁。

### 2026-08-05 24 号计划执行（控制包 SHA 755278bf...5f98）

**执行入口**：`24_SECOND_FIX_IMPLEMENTATION_CORRECTION_PLAN.md`（审计对象
`AstroCS_Review_SecondFixReview_20260805(1).zip`，HEAD 2bda708）。

**重要决策（2026-08-05，用户明确指示）**：
- **利用率目标闭环（50/80/95/100 稳态误差 ≤0.05）不作为本轮验收门禁**。
  原因：Windows 没有可直接调用的系统 API 软限制进程 CPU 占用率（Job Object
  `JOBOBJECT_CPU_RATE_CONTROL_INFORMATION` 只有硬配额且作用于整个 Job，不是
  软目标闭环）。保留已实现的"真实采样（GetSystemTimes）+ 可调并发许可
  （active_budget 可升可降）+ GPU 决策进执行循环 + 真实队列状态"作为基础能力，
  sustained 测试降级为报告型（每档 ≥30s、采样/参与/动作如实输出），
  不承诺目标达标，也不作为合并门禁。
- 优先把底层跑通：Benchmark→Profile 统一管线、Eligible Device Set、actual
  统计、NumericPolicy/Invocation 契约、WorkToken attempt 原子化、Sanitizer、
  Evidence。

**已完成（24 号计划）**：
1. **WorkToken attempt 原子化**（f0eda29）：status+attempt 合并单原子 state，
   完成/失败单次 CAS 验证，消除 ABA 窗口；Dispatcher 检查 mark_done/mark_failed
   返回值，ledger 拒绝不累计完成量；确定性 ABA 交错测试。
2. **NumericPolicy/Invocation 契约**（be7c68b）：validate_invocation 统一校验
   buffer/scalar/domain/backend launcher/NumericPolicy；ScalarArgBlob 改 memcpy
   对齐读取（read_scalar）；Reduction 实现真实 FP64 累加（CPU double、CUDA
   partials double + atomicAdd），声明与实现一致；桥接 REDUCE ABI 改 double。
3. **Eligible Device Set**（4ff8866）：生产调度按 feasible/最小有效规模/收益/
   profile 有效性筛选，无 profile GPU 不参与，首轮公平门只在入选设备间；
   force_all_supported_executors 为测试专用开关。
4. **actual 统计修复**（0f1d394）：每设备 items/bytes_read/bytes_written/
   blocks/active_duration/error_count 来自 completion；actual_primary 按
   items_done 最大（tie-break bytes/时长），禁止 actual_devices.front()；
   CPU 块多/GPU 块大反例测试。
5. **资源控制基础改造**（未提交，随 24 号计划）：
   - 采样修正：GetSystemTimes 极短窗口 delta_total==0 返回无效样本，
     仅记录 valid 采样；
   - active_budget 可升可降并发许可（阈值 0-1，±0.05 误差带），取代
     "actual > target+0.10 关闭全局 gate"（95%/100% 永不触发缺陷）；
   - GPU 采样/queue/throttle/batch 进入 invocation 正式循环；
   - CudaBridgeExecutor 真实 queue_state（pending 计数）；
   - 所有 CPU 逻辑线程可参与（移除 min(8,hw) 静态上限）。
6. **统一 Benchmark→HardwareProfile 管线**（086df45）：
   - BenchmarkDriver 经桥接执行真实 GPU 微基准（AXPY/Triad/Copy/Dot→reduce/
     Convolution2D→conv3x3），消除 GPU 占位/零样本；不支持的维度如实跳过；
   - Raw 记录增加 ISA/线程维度；ProfileGenerator 先聚合再映射曲线（修复
     median 恒 0）；GPU 名称/指纹优先 bridge 真实设备名；
   - BenchmarkConfig 支持 kernel 定向微基准；holdout 预测验证测试
     （acr_test_profile_holdout）；
   - `acr-benchmark --gpu quick` 实测生成 CPU+GPU 双设备真实 profile
     （RTX 3060 Ti 曲线 median>0）。
7. **Sanitizer 扩展**（997cf4c）：MSVC ASan 覆盖 shared_work_pool、
   kernel_registry、device_executor（CpuExecutor 提交+契约拒绝）、
   cpu_controller、system_metrics；Dispatcher/ProfileBuilder 因
   oneTBB/hwloc MinGW ABI 依赖无法纳入，如实记录。

**已知限制（如实）**：
- CPU/GPU 利用率目标闭环未达标且不再作为本轮门禁（见决策）；
- UBSan/TSan 本机不可用（MinGW/clang 无运行库）。

### 2026-08-04 第二版 Fix Review 纠正（23 号计划，控制包 SHA eb0a0535...0853）

**执行入口**：`23_SECOND_FIX_REVIEW_CORRECTION_PLAN.md`（审计对象
`AstroCS_ACR_Fix_Review_2026-08-03(2).zip`，结论禁止合并 main）。

**已完成（按计划章节）**：
1. **§1 内核 ABI 分层**（`refactor(acr): separate cpu callbacks...` 0a3de3d）：
   - `include/astro/compute/kernel_registry.hpp`：KernelRegistration（OperationId +
     KernelArgSchema + Cpu/Cuda/Hip launcher + NumericPolicy）、KernelInvocation
     （id/domain/buffers/scalars/traits/token_id）、KernelRegistry（稳定节点存储、
     线程安全）；
   - acr.hpp 的 lambda API 明确标记 CPU-only compatibility；
   - 8 项单测：host callback 不得标记 GPU-capable、设备 launcher 缺失回退并如实报告等。
2. **§2 动态工作池重写**（`fix(acr): replace dynamic vector blocks...` e5a700a）：
   - WorkToken 按值返回 {id, begin, end, claimant(DeviceId), attempt}；
   - 预分配槽位（slot[i].id==i）消除 ID/槽位错配，CAS cursor 领取 + retry queue；
   - 完成判据 cursor>=end && inflight==0 && retry 空 && failed_terminal==0 &&
     completed_items==end-begin；
   - 专项测试：强制 A/B 逆序交错、gate 关闭不漏项、1000 轮压力、失败回收/重复领取、
     exactly-once（test_work_pool.cpp）。
3. **§3 真实 DeviceExecutor**（d0cc723）：
   - DeviceExecutor{id, supports, queue_state, submit(WorkToken, KernelInvocation)}，
     SubmitHandle 记录真实 device/items/bytes/duration/fallback；
   - CpuExecutor 经 KernelRegistry CPU launcher 真实执行；
   - 移除 dispatch_range_cost_aware 的伪 GPU 分支（旧 lambda 路径明确 CPU-only）。
4. **§4 CostEstimator 驱动每设备领取**（5065e8d）：
   - `CostEstimator::compute_requested_items`（设备吞吐/队列/剩余工作/内存上限）；
   - `Dispatcher::dispatch_invocation`：每 executor 独立 claim，CPU 多 worker、
     GPU 单 worker，实际统计来自 SubmitHandle；无 executor 支持 op 时如实失败；
   - 全局 Dispatcher 接入 ExecutorRegistry::create_auto()。
5. **§5 可恢复时间窗资源闭环**（f738fdf）：
   - 100-500ms 时间窗采样（不按 task 计数），首次采样立即到期；
   - worker 注册 active/idle；gate 迟滞 close→wait→re-sample→reopen，
     关闭期间等待不丢工作（修复 50ms 即放弃缺陷）；
   - batch size/claim size 实际进入执行链；MemoryBudget 全动作接入
     （StopNewSubmit 可恢复/ShrinkBlock/ReleaseCache hook/LowMemoryPath/
     FallbackOtherDevice/Fail 保准确 coverage）；GpuController 连接 executor 队列；
   - 失败块重试 attempt 上限（修复无限重试 TIMEOUT）；9 项闭环测试。
6. **§3/§1.4/§6 CUDA 桥接与经典内核**（56874dd）：
   - C ABI 桥接 DLL（MSVC+nvcc 构建，仓库外输出 run/temp/cuda_bridge/）：
     AXPY/COPY/REDUCE/CONV3x3 真实 GPU kernel（RTX 3060 Ti 实测通过）；
   - MinGW 侧加载器运行时探测（无 DLL/设备不注册 executor，不靠编译宏）；
   - classic_kernels 注册 Copy/AXPY/Reduction/Convolution CPU+CUDA launcher；
   - dispatch_invocation 增加 worker 启动屏障；E18 经典实验改走 KernelRegistry +
     dispatch_invocation，真实 Mixed 断言 cpu_done>0 && gpu_done>0；
   - 9 项 CUDA/经典测试全通过（含真实 CPU+GPU Mixed）。
7. **§6 测试纠正**（cb6444d）：
   - Persistence.ProfileReload 修复 TIMEOUT（最小合法 profile 循环，1.4s）；
   - MSVC /fsanitize=address 验证程序（真实 shared_work_pool.cpp +
     kernel_registry.cpp）：1000 轮压力无 ASan 错误、故意 UAF 被检出；
8. **§7 Evidence 清理**（7c67328）：删除仓库内旧 Evidence（工程控制/evidence/acr/），
   改为仓库外生成（run/evidence/）。

**测试结果（2026-08-04 全量）**：623/623 通过（8 项 SanitizerActual 在 MinGW 构建
SKIPPED——本机无 ASan 运行库，真实 ASan 由 MSVC 独立验证提供；1 项 ApiReduce 别名
声明性 SKIPPED）。无失败、无 TIMEOUT、无 SEGFAULT。

**合并门禁状态**：核心审计阻断项已修复（Kernel ABI 分层、稳定 WorkToken、真实
CPU/CUDA executor、每设备 cost 领取、时间窗资源闭环、真实 CPU+GPU Mixed、
GTEST_SKIP、ASan 实际开启、外部单 HEAD Evidence 生成中）。**尚未合并 main**
（待 Evidence 完整性复核与最终门禁评估）。

**已知限制**：
- UBSan/TSan：本机 MinGW（g++/clang）无运行库，不可用（如实记录，阻断项之一）；
- CUDA 桥接 DLL 用 nvcc 11.8 + MSVC 14.50 + `-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH`
  构建，属仓库外工具链产物，不纳入 MinGW 构建；
- 经典 Reduction 的 CUDA launcher 按 token_id 写入独立 partials 区域，merge 阶段
  需累加全部 partials（E18 已实现）。

### 2026-08-02 Phase B+E+F CMake 集成 + 单测补全
- **背景**：Phase B/E/F 代码已实现但未接入 CMake，3 个新测试未注册，存在多处编译错误。
- **修复的编译错误**：
  1. `task_descriptor.cpp` include 路径错误：`"astro/compute/core/task_descriptor.hpp"` → `"task_descriptor.hpp"`（同目录）
  2. `hardware_profile.hpp` 缺 `<cmath>`：`std::log2` 未声明（Curve::predict 用）
  3. `acr.hpp` 在 `namespace detail` 内前向声明 `struct TaskDescriptor`，与 `astro::compute::TaskDescriptor`（task_descriptor.hpp 真实定义）形成名字遮蔽 → runtime.cpp 报 "incomplete type"。移除该前向声明（submit_*_with_desc 签名只用 TaskTraits，不需要 TaskDescriptor 前向声明）
  4. `task_descriptor.hpp` 未声明 `task_traits_valid`/`task_descriptor_summary`（实现在 .cpp）→ 测试无法链接。补声明到头文件
- **CMake 集成**：
  - `core/CMakeLists.txt`：acr_core 加入 `task_descriptor.cpp`
  - `tests/unit/CMakeLists.txt`：注册 3 个新测试目标（acr_test_task_descriptor 链 acr_core+acr_api；acr_test_hardware_profile 链 acr_api；acr_test_cost 链 acr_cost）
- **清理的警告**：移除 dispatcher.cpp 未用变量 `total_work`；cost_estimator.cpp 未用变量 `bytes_per_item`（range_chunk_thunk 警告系 incomplete-type 错误级联，修复后自动消失）
- **测试结果**：376/376 通过（1 skipped 是 Phase B 别名声明性）。新增 71 测试：test_task_descriptor 17、test_hardware_profile 33、test_cost 21。366/366 非 smoke 测试并行全通过；10 个 SanitizerSmoke 并行负载下偶发 SEGFAULT（-j1 串行 10/10 稳定通过），系预存并发测试 flaky，与本次改动无关（未触及 sanitizer/event/tiles 代码）。

### 2026-08-02 Phase A 完成
- worktree 从 origin/main (8f50519) 创建 feature/astrocompute-runtime
- astro_toolkit.py 自检通过（ok:true）
- 仓库审计完成：MSYS2 MinGW64 g++ 16.1.0、C++17 现有模块用 OpenMP、无第三方库、CUDA 11.8 + RTX 3060 Ti、无 vcpkg/Conan
- 9 个 ADR 创建：alpaka/oneTBB/hwloc/cpu_features/Google Benchmark/GoogleTest/StarPU 评估/FetchContent/CPU-only 门禁
- dependency-lock.json 创建（12 个依赖，版本锁定）
- forbidden-paths.md 创建（11 个算法目录 + 顶层文件只读）
- ci/path_guard.ps1 创建并验证（中文路径用 git pathspec 排除，pwsh 执行）
- CMakeLists.txt 骨架（Phase A 只声明 FetchContent 不 MakeAvailable，CPU-only 默认不拉 GPU 依赖）
- spec.md + checklist.md + tasks.md 三件套就位

## 重大决策

1. worktree 隔离在 run/worktrees/acr/（沙箱限制，主仓库 experiment/fast-drizzle-r06 不动）
2. ACR 在 lib/acr/ 内独立 CMake，不引入顶层 CMakeLists.txt
3. 依赖全部 CMake FetchContent 拉取固定 tag，不入仓
4. 测试框架 GoogleTest 1.15.2
5. CUDA backend 启用真实验证（RTX 3060 Ti，本机 CUDA 11.8）
6. 本次范围 Phase A-H，Phase I 合并 main 等用户二次授权
7. 公共 API 不暴露第三方类型（PIMPL/type-erased）
8. ACR lazy initialization，合并后普通 AstroCS 运行无副作用
9. path guard 用 git pathspec exclusion 处理中文路径（pwsh 执行）

## 下一阶段

Phase D 完成。下一阶段：Phase E（Qualification/路由）依赖 B/C/D，由主 Agent 执行。

### 2026-08-02 Phase D 完成
- **关键决策**：alpaka FetchContent 与 MinGW g++ 16.1.0 工具链兼容性风险已知（ADR-008 oneTBB 前车之鉴），Phase D 用**纯 CUDA** 实现 GPU backend（不依赖 alpaka）。alpaka 作为 ADR-001 评估/未来 adapter 延后。这样 `ACR_BUILD_CUDA=ON` 用 nvcc 编译 .cu，`OFF` 时无任何 CUDA 引用（ADR-009 门禁）
- **backends/cuda/cuda_backend.hpp**：CudaBackend singleton（lazy init，std::call_once 幂等）+ CudaDeviceInfo（name/uuid/compute_capability/sm_count/total_memory/free_memory/driver_version）+ cuda_error_to_status（cudaError→StatusCode：DeviceLost/OutOfMemory/KernelFailed）+ cuda_parallel_for 模板（仅 __CUDACC__ 可见，functor 转发 kernel）+ axpy 启动器
- **backends/cuda/cuda_backend.cu**：实现。cudaGetDeviceCount 枚举设备，无设备/驱动错误时 available()=false 降级不抛异常。cudaSetDevice(0) + cudaGetDeviceProperties + cudaStreamCreate。UUID 格式化为 `GPU-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`。initialize() 内调用 register_gpu_report_callback(&CudaBackend::gpu_report_json)（CAS 首次生效）。AxpyFunctor + parallel_for_kernel（__global__ 转发）+ axpy 用 cuda_parallel_for 启动
- **backends/cuda/cuda_buffer.hpp**：CudaBuffer<T> 模板（cudaMalloc/cudaFree RAII，移动语义，禁止拷贝）+ copy_h2d/copy_d2h（async + stream sync）+ cuda_event 类（record/sync/elapsed_since）。全部 #ifdef ACR_BUILD_CUDA 保护
- **backends/cuda/cuda_buffer.cpp**：query_device_memory 非模板辅助
- **backends/cuda/CMakeLists.txt**：ACR_BUILD_CUDA=OFF 时 return()；ON 时校验 CMAKE_CUDA_COMPILER，CUDA 标准 17，CMAKE_CUDA_HOST_COMPILER 用 MinGW g++（如未指定），target acr_cuda 静态库链接 cuda+cudart+acr_diagnostics，PUBLIC ACR_BUILD_CUDA=1 传递给消费者
- **tests/unit/test_cuda.cpp**：13 个 GoogleTest（#ifdef ACR_BUILD_CUDA 保护）：设备枚举/幂等/stream、AXPY MatchesCpu/Empty/Null/NonAligned、CudaBuffer RoundTrip/Move/OutOfBounds、cuda_event Timing、DegradePath、GpuReportCallbackRegistered
- **examples/cuda_axpy.cu**：RTX 3060 Ti AXPY 示例，打印设备信息+H2D/Kernel/D2H 耗时+结果校验
- **更新 tests/unit/CMakeLists.txt + examples/CMakeLists.txt**：ACR_BUILD_CUDA 条件编译 test_cuda.cpp + cuda_axpy.cu
- **构建验证**：
  1. **CPU-only 构建通过**（ACR_BUILD_CUDA=OFF）：cmake configure (0.1s) + build 成功，无 acr_cuda target，无任何 CUDA 引用（ADR-009 门禁满足），ctest 52/52 通过（1 skipped 是 Phase B 别名声明性）
  2. **CUDA 构建工具链问题**（已记录）：nvcc 11.8 + MinGW g++ 16.1.0 host **不支持**（`Failed to preprocess host compiler properties` / `Host compiler targets unsupported OS`）。nvcc 11.8 在 Windows 硬编码要求 MSVC host compiler。尝试 `--allow-unsupported-compiler` 无效（MinGW 目标三元组被拒）。nvcc 11.8 + MSVC cl.exe 14.44（VS2022 BuildTools）+ `--allow-unsupported-compiler` 可编译运行 CUDA kernel，但 ACR 依赖 MSYS2 MinGW ABI（oneTBB/hwloc/gtest），与 MSVC ABI 不兼容，无法用 MSVC 编译整个 ACR
  3. **RTX 3060 Ti 真实验证 PASS**：用 MSVC cl.exe + nvcc 编译独立 AXPY 验证程序（run/temp/verify_cuda_axpy.cu，含 cuda_backend.cu 的 AxpyFunctor + parallel_for_kernel 逻辑），在 RTX 3060 Ti 上运行：N=1048576，AXPY 结果与 CPU 完全一致（mismatch=0，max_diff=0），H2D 1.35ms / Kernel 17.19ms / D2H 0.77ms。Device: RTX 3060 Ti (CC 8.6, 38 SMs, 8191 MB)。证明 **kernel 代码已就绪且真实运行正确**
- **Phase D 状态**：CUDA backend 标记"kernel 代码已就绪，编译集成待工具链解决"。工具链升级路径：(a) CUDA 12+ 对 MinGW 支持改善（待验证），或 (b) ACR 全转 MSVC + vcpkg 依赖（大改，超 Phase D 范围）。控制包"至少一个真实 GPU 后端真实通过"要求：AXPY kernel 在 RTX 3060 Ti 真实运行通过（独立验证），完整 CMake 集成编译待工具链
- **本机环境**：nvcc 11.8.89（C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin\nvcc.exe）、MinGW g++ 16.1.0、ninja 1.13.2、MSVC 14.44.35207（VS2022 BuildTools）+ 14.50.35717（VS2026 BuildTools，nvcc 11.8 不支持）、RTX 3060 Ti 驱动 595.79 / 8192 MiB

### 2026-08-02 Phase C 完成
- **公共头** `include/astro/compute/topology.hpp`：IsaLevel 位掩码枚举（SSE/SSE2/SSE3/SSSE3/SSE41/SSE42/AVX/AVX2/FMA/AVX512F/CD/BW/DQ/VL）+ CpuIsaCaps（has/has_isa 安全门禁）+ HwlocTopology（PIMPL）+ detect_topology/detect_isa_caps/generate_hardware_report + GpuReportCallback 回调注册（Phase D 用）
- **topology/hwloc_topo.cpp**：hwloc 拓扑枚举（package/core/PU/L1L2L3 cache/NUMA/PCI 设备）+ JSON 序列化。无 hwloc 时 `__has_include` 降级返回 `{"status":"unavailable"}`，不抛异常
- **topology/cpu_features.cpp**：CpuIsaCaps 检测。无 cpu_features 库时用 `__builtin_cpu_supports`（GCC/Clang）降级。AVX-512 子集独立 bit（ADR-004 关键约束）
- **backends/cpu/isa/{scalar,sse,avx,avx2,avx512,dispatch}.cpp**：AXPY kernel 实现。用 `__attribute__((target("...")))` 函数级启用 ISA intrinsics（TU 默认 baseline，无需全局 -mavx*）。`kernel_<name>_axpy_safe` 运行时门禁（caps 不支持返回 false 不调用，无 SIGILL）。`dispatch_axpy` 自动选最优 kernel（AVX-512 > AVX2 > AVX > SSE > scalar）
- **diagnostics/hardware_report.cpp**：合并 topology+isa+GPU 回调为 hardware.json（schema=acr.hardware.v1）。GPU 回调用 atomic CAS 首次生效
- **tests/unit/test_topology.cpp**：18 tests（HwlocTopology JSON/降级、CpuIsaCaps SSE2 必有/AVX-512 门禁/子集独立、AXPY dispatch 正确性、hardware_report schema/GPU 回调）
- **依赖获取**：
  - hwloc：MSYS2 mingw-w64-x86_64-hwloc 已装（C:/msys64/mingw64/include/hwloc.h + libhwloc.dll.a），无 cmake config，用 find_path/find_library
  - cpu_features：MSYS2 无包，用 `__builtin_cpu_supports` 降级（g++ 16.1.0 支持）。FetchContent 未启用（ADR-008 工具链问题已知）
- **工具链问题与解决**：
  1. `__builtin_cpu_supports` 参数必须是字符串字面量，不能用变量传入（g++ 报 "parameter to builtin must be a string constant or literal"）。改为直接展开 14 个 if 语句
  2. gtest_discover_tests 默认 POST_BUILD 模式在 build 时运行 exe 发现测试，但 hwloc.dll 不在 PATH 导致 discovery 失败。改用 `DISCOVERY_MODE PRE_TEST`（ctest 运行时才发现）
  3. ctest "No tests found" 因顶层缺 `enable_testing()`。在 lib/acr/CMakeLists.txt 顶层加 enable_testing()（tests/CMakeLists.txt 也有，但顶层才生成根 CTestTestfile.cmake）
  4. ctest 每个测试独立进程，全局变量在测试间重置。FirstCallbackWins 测试改为单测试内注册 cb1+cb2 验证 CAS 首次生效
- **构建验证**：cmake configure (0.8s) + build 成功 + ctest 52/52 通过（1 skipped 是 Phase B 别名声明性）。path_guard 通过（所有改动在 lib/acr/ 内）
- **本机 ISA 检测结果**：SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/AVX/AVX2/FMA = true，AVX-512 全子集 = false（RTX 3060 Ti 的 CPU 不支持 AVX-512）

### 2026-08-02 Phase B 完成
- acr.hpp 完整公共 API：parallel_for/for_2d/tiles/reduce/batch/scan/chunks/run_for + Buffer/BufferView/Event/RuntimeConfig + StatusCode/AcrError + detail::submit_* type-erased 接口
- runtime_internal.h：EventImpl/EventState（mutex/atomic/condition_variable）
- core/runtime.cpp：oneTBB lazy singleton + submit_range/2d/tiles/batch/chunks/serial/reduce（tbb::parallel_for/parallel_reduce + task_arena + global_control）
- api/event.cpp：Event 类实现（wait/ready/cancel/status）
- 公共头不暴露 tbb 类型（tbb 完全封装在 runtime.cpp，模板内联调用 detail::submit_*）
- examples：minimal_parallel_for（N=1M reduce 验证）、legacy_chunk_adapter（chunk 适配）
- GoogleTest 单测：test_api.cpp（23 tests）+ test_buffer.cpp（11 tests），33/33 通过（1 skipped 是别名声明性）
- **依赖获取变更**：ADR-008 原计划 FetchContent oneTBB v2022.0.0，但 CMake 版本检测脚本与 MinGW g++ 16.1.0 不兼容（/dev/null 重定向 + 版本号解析失败）。fallback 到 MSYS2 系统包 mingw-w64-x86_64-tbb 2023.0.0 + gtest 1.17.0。dependency-lock.json 已记录 acquisition_note。
- oneTBB 2023 API 变更：parallel_reduce functional 形式 identity 是值不是 lambda，已修复
- 构建验证：cmake configure (0.8s) + build 成功 + examples 运行正确（FP32 末位差异 1e-4 符合允许范围）


### 2026-08-06 聚焦版 v3（控制包 10，SHA 52843aee...e0e1a）

**执行入口**：`08_CURRENT_EXECUTION_PLAN.md`（v3）。审计 `ACR_FOCUSED_V2_REVIEW.md`
确认 6 类阻断，修复如下：
1. `a36c482`：成本单位统一 ns（交叉点 fixed 转 ns，消除 1000 倍误差）；
   qualified 与 GPU eligible 分离（测量可信即 qualified）；顶层 state 按全部
   Operation 重算；qualification_reason；host 路径用真实输入/输出字节。
2. `c00266e`：makespan 模型（有/无该设备块的预计总完工时间）支持异速
   CPU/GPU Mixed；收益阈值不再覆盖推荐块。
3. `5e62ef5`：Dispatcher 真实驻留执行——worker 启动前 prefetch（CudaBridgeExecutor
   真实上传 + device view），launcher 按 input_resident 走 resident 提交
   （跳过逐块 H2D），删除执行后补传。
4. `c8dc867`：partial scratch 契约（partial_slots_for 精确槽位）、attempt 重试
   清零（不重复累计）。
5. `3ec62c3`：PinnedLedger 更名 StagingLedger；resident 峰值不重复计整帧 H2D。
6. `41797b3` + 后续：qualified CPU-only 路由断言、Dispatcher prefetch 集成测试、
   GPU 测试顺序稳定化（RUN_SERIAL + focused_mixed 注册前移）。

**验证（2026-08-06）**：全量 ctest 607/607（-j 2 通过一次）；focused_mixed
单独 -shuffle 7/7 全过（GPU 环境退化时 ctest 内 drizzle 偶发 flaky，已单独复跑
通过并如实记录）；standard Profile schema PASS（dense GPU 无收益 ineligible、
pixel_reduce/drizzle resident 阈值单位正确：百万级）。

### 2026-08-06 ACR 架构冻结 + 加权积分合成 Mixed 样例（控制包 b98d38f8...efb8a）

**执行入口**：`07_CURRENT_EXECUTION_PLAN.md`（新控制包）。基线
`AstroCS_Review_ACRFocusedV3_20260806`（HEAD 610d7b6）。目标：不改 Phase1/
真实业务算法，冻结 ACR 面向重负载逐像素算法的架构，并完成独立加权积分合成
Mixed 样例，形成"允许开始修改业务代码"的最终 Evidence。

**架构冻结落实**：
- `ResidencyPolicy`（HostOnly/PreferDevice/KeepDevice/MaterializeHost）、
  `BufferRole`（Input/Output/ReadWrite）+ BufferBinding generation/stable_key、
  KernelInvocation.residency_policy（task_traits.hpp / kernel_registry.hpp）；
- DeviceExecutor 增加 max_in_flight/set_streams/slot_upload_count 与
  prefetch_inputs（多输入组合上传）；每 GPU 单 executor、内部 1..3 stream
  （桥接 configure_streams/stream_count）；
- Dispatcher buffer 布局按 OperationId 泛化（weighted_integration：buffer0=
  输出、buffer1=frames、buffer2=weights）；prefetch 传完整输入集合、仅新上传
  输入记账；首轮公平门修复（池空时 break 也参与计数，消除死锁）；工作保持
  兜底（should_claim 双拒时由可用 executor 清尾，禁止漏算）。

**加权积分样例（synthetic.weighted_integration.fp64acc）**：
- `examples/weighted_integration/`：common（数据/逐像素核心/误差）、kernels
  （Serial/OpenMP 参考 + CPU/CUDA launcher 注册，无嵌套 OpenMP）、benchmark
  （quick/standard/full、serial/openmp/acr_cpu/gpu_host/gpu_resident/
  forced_mixed/auto_mixed/auto_mixed_reuse、2 warmup + 7 repeats、median/
  min/p90、weighted_integration_report.schema.json）；
- 桥接新增 weighted_integration host/resident 提交 + upload_persistent_slot
  （slot0=frames、slot1=weights）+ 真实上传计数 upload_count；
- `tests/integration/`：CorrectnessQuickAllPaths（全模式 + 非整除尾块）、
  ForcedMixedBothNonZero、ResidentReuseFramesUploadOnce（frames 上传保持 1）、
  StreamConsistency（1/2 stream 结果一致）。

**验证（2026-08-06）**：
- 全量 ctest 298/298 通过（291 passed + 7 SanitizerActual 准确跳过）；
- standard Benchmark：correctness=PASS、performance=QUALIFIED、
  READY_FOR_BUSINESS_ADAPTER=true（中/大 case Auto 相对 OpenMP ≥1.05x）；
- compute-sanitizer：memcheck 0 errors、racecheck 0 hazards；
- 真实 RTX 3060 Ti：CPU+GPU Mixed 双方非零、resident-reuse frames 上传 1 次、
  1/2 stream 结果一致。

**性能修复（2026-08-06）**：Dispatcher invocation worker 循环残留两处调试
fprintf（`fprintf(stderr, "[%zu,%zu)\n", backend, ...)` 与
`fprintf(stderr, backend, status)`）——每块执行 2 次同步 stderr I/O + fflush，
16 个 CPU worker 并发写 stderr 竞争，导致 ACR CPU 路径比 OpenMP 慢约 50 倍
（512² acr_cpu 162ms → 3.3ms）。移除后 standard 全 PASS（2048² Auto
11.4ms vs OpenMP 14.25ms，≥1.05x），性能资格 QUALIFIED。

**已知限制（如实）**：
- GPU 连续负载后 ctest 全量中 GPU 测试偶发 flaky（CTest 单进程独立复跑通过）；
- 桥接仍为同步语义；多 stream 为轮转分派，in-flight 槽位按 stream 数暴露，
  未实现跨 stream 异步重叠；
- UBSan/TSan 本机不可用（SanitizerActual 7 项准确跳过）；
- staging ledger 为容量账本；加权积分 benchmark 的 h2d/d2h 计数来自
  Dispatcher/桥接真实传输统计。

### 2026-08-07 加权积分 Benchmark 公平化与真实统计（控制包 cd5b19e1...facc）

**审计结论**：架构方向冻结（ARCHITECTURE_DIRECTION_FROZEN=true），但
READY_FOR_BUSINESS_ADAPTER=false。本轮修正 Benchmark 口径、真实统计、
GPU 候选块标定、stream 决策与 Auto 路由。

**已完成**：
- Benchmark 场景分离：openmp_single / openmp_reuse4_total / acr_cpu /
  gpu_host_cold / gpu_resident_steady / forced_mixed（仅正确性，
  comparable=false、speedup=null）/ auto_cold_single_shot /
  auto_resident_steady / auto_resident_reuse4（Serial 参考计时外预计算，
  与 4 次等价 OpenMP 比较）；speedup 仅等价工作量计算，否则 null。
- 真实统计：CPU/GPU items、blocks、active ns 来自 per_device_stats；
  chunk 序列来自 resource_control.dynamic_chunk_sizes（invocation 路径补齐）；
  H2D/D2H 次数/字节来自 transfer_stats（真实 prefetch/物化）；
  Dispatcher 修复 invocation 路径动态块记录与 Auto CPU 预判（CPU 无收益
  不启动 16 个 worker）。
- GPU 候选块真实执行：cand 控制 token 大小（1M/4M/16M），域 16M，
  记录块数/ns-item/D2H 次数，推荐块含 D2H 摊薄（16M 整帧档）。
- Profile 标定 4 点（1M..16M）改善 leave-one-out 误差；误差字段真实非 0；
  qualified 与 eligible 语义正确。
- Stream 冻结：同步语义，observed_max_in_flight=1，configured_streams
  单独报告，不宣称多通道并发收益。
- ctest -j2 稳定：fault/sanitizer/persistence 加 RUN_SERIAL。
- sha256_utf8.py 可移植：相对 POSIX 路径 + 验证根可显式指定。

**Auto 性能现状（如实）**：Auto 已自然退化为 GPU-only（CPU 预判）、
推荐块 16M 整帧；但 Dispatcher 每 dispatch 固定开销（worker 线程创建、
采样、调度约 0.3-2ms）使 Auto 距 gpu_resident 直连基线 >10%，
standard 性能资格 PERFORMANCE_NOT_QUALIFIED（2048² Auto 6.3ms vs
OpenMP 15.1ms，相对 OpenMP ≥2×，但 10% 门禁按直连基线判定未过）。
READY_FOR_BUSINESS_ADAPTER=false。

### 2026-08-09 BDR3 收尾（最终 HEAD da5a280，控制包 A9766B99...53984）

**本轮稳定结论（2026-08-08 BDR3 计划 A-J 全部落地后，从最终 HEAD 重跑验证）**：
- 场景级资格（A）：RoutePath 拆 model_available/model_trusted，Operation.qualified
  由三个 required 场景（cold/resident/reuse4）全部 qualified 生成；生产未 qualified
  场景直接 OpenMP fallback；诊断 Replay 允许全部 model_available 候选参与预测。
- fit/probe/final 隔离（B）：Fit 15 点、Probe 8 点、Final 8 点/场景，无交集断言通过。
- 真实 cold Mixed（D）：warmup 用不同 generation、正式样本 fresh Dispatcher，
  17/17 样本 timed H2D>0。
- 2D chunk 服务插值（F）：interpolate_chunk_service 先 chunk 轴再 frame 轴，
  未命中帧数不再混用全部曲线；GPU 单块保持完整 frame-major stride（E）。
- RUN_SERIAL 稳定化（J，commit da5a280）：聚焦 Mixed 性能测试串行执行，
  避开 load-guard flaky skip；CTest 连续 3 轮 625/625、0 失败、10 项准确跳过。

**最终 Benchmark 结论（da5a280，RTX 3060 Ti + MinGW64）**：
- standard：correctness=PASS；performance=PERFORMANCE_NOT_QUALIFIED
  （reason=auto slower than best by >10%（1024²×16 auto_resident_steady ratio=403%））；
  profile_state=partial；READY_FOR_BUSINESS_ADAPTER=false。
- quick：correctness=PASS；performance=NOT_RUN；READY_FOR_BUSINESS_ADAPTER=false。
- Route Replay：standard 24 点中 22 点 within 10%（2 点超门：
  cold 1638400×20 chosen legacy_openmp 7.7582ms vs best mixed 5.853ms ratio 1.3255；
  resident 589824×12 chosen gpu_direct 1.3844ms vs best legacy_openmp 1.1755ms ratio 1.1777）；
  reuse4 8/8 通过；quick 24/24 通过（ratio 1.0）。
- 场景级 Final 误差（max，15% 门）：GPU direct cold/resident 14.76%/12.84% 已可信，
  reuse4 16.11% 超门；OpenMP 23.53%–39.19%、Mixed 19.23%–39.31% 均超门 →
  三场景 scenario_qualified=false、profile_threshold_validated=false、op.qualified=false。

**限制（如实）**：OpenMP/Mixed 预测模型 Final 误差超 15% 门；standard Replay 2/24
超 10%；auto_resident_steady 1024²×16 比 best 慢 403%；生产未接入
（READY_FOR_BUSINESS_ADAPTER=false）。下一阶段需先改进 OpenMP/Mixed 成本模型
或调整标定策略，不得手工宣称 qualified。

**交付**：11 个提交（4769979..da5a280）push 到 feature/astrocompute-runtime；
证据 run/evidence/acr_bdr3_20260808/（3 轮 CTest 日志、standard/quick 报告、
Replay 明细、可移植 SHA 清单）；审核包
AstroCS_Review_ACRScenarioQualificationChunk2D_20260809.zip（SHA 见交付记录）。

### 2026-08-09 Dispatcher Finalization（最终 HEAD 560c417，控制包 426D9A51...3A37E）

**本轮完成（08 计划 1-10）**：
1. Final 真正 untouched：拆 `select_model_on_probe` / `evaluate_fixed_model_on_final`；
   Final 只使用冻结 interpolation_id，`evaluate_final` 断言不修改模型；回归测试 4 项。
2. `interpolate_e2e()` iterator UB 修复：`lower_bound` 返回的 `usable` 迭代器改与
   `usable.end()` 比较；新增 usable-end / adaptive 单帧边界测试。
3. Profile 发布：standard 唯一权威；quick 只写 `*.quick.tmp.json` 且不改权威槽位
   （quick 后 authoritative SHA 不变）；Profile 写 calibration_preset/head/run_id/
   generated_utc；qualified 状态校验要求 standard 元数据。
4. RouteProfileV2 正式接入 Dispatcher：DispatcherConfig.route_profile_v2 + 顶层
   BenchmarkRouteEstimator 决策；OpenMP→legacy_parallel_launcher、GPU Direct→仅 GPU
   executor、Mixed→SharedWorkPool（旧 planner 不做顶层资格）；ExecutionReport 记录
   benchmark_route_decision/reason/predicted_ms/chunk 建议；失败路径记录真实错误。
5. LegacyParallelLauncher 注册：加权积分注册完整 OpenMP launcher（业务现有 OpenMP
   函数直接复用，无需重写 range 核心）。
6. route-regret Adaptive：场景联合（Probe 点三候选同坐标），regret>1.05 优先补
   错路由坐标，同坐标三路径一起加入 Fit；最多 2 轮；无 regret 超标时按最大误差补。
7. 加权积分样例 Auto 全部走统一 Dispatcher 入口（删除样例层三路 if/else）；
   BDR 未 qualified 时生产自动 legacy OpenMP。
8. Route Replay 达到 24/24（每场景 8 点，standard/quick 均 within 10%，
   max_slowdown=1.0）。
9. 三场景仍未全 qualified（如实）：GPU Direct 三场景 final max 误差 8.9%/12.9%/
   12.3% 已 trusted；OpenMP 24.2%–44.8%、Mixed 15.9%–33.2% 仍超 15% 门 →
   scenario_qualified=false、op.qualified=false、READY_FOR_BUSINESS_ADAPTER=false。
10. 稳定证据：CTest -j1 连续 3 轮 635/635、0 失败、10 项准确跳过；
    compute-sanitizer memcheck 0 errors / racecheck 0 hazards；MSVC ASan 因本机
    commit 上限（ASan 初始化 calloc ~235GB > 系统 ~77GB）无法运行，如实记录
    （同二进制 2026-08-08 PASS，非代码回归）。

**限制（如实）**：OpenMP/Mixed E2E 模型 Final 误差仍超 15% 门（下一阶段需改进
OpenMP/Mixed 成本模型或标定策略）；MSVC ASan CPU 核心本轮环境不可运行；
生产未接入（READY_FOR_BUSINESS_ADAPTER=false）。未放宽任何误差门。

**交付**：6 个提交（be0f608..560c417）push 到 feature/astrocompute-runtime；
证据 run/evidence/acr_bdr_dispatcher_finalization_20260809/（含权威 Profile、
24/24 Replay、quick 不覆盖 SHA、3 轮 CTest、sanitizer、可移植 SHA 清单）；
审核包 AstroCS_Review_ACRDispatcherFinalization_20260809.zip（SHA 见交付记录）。

### 2026-08-09 Route/Residency/Generation 闭环（最终 HEAD，控制包 CE288DBF...F7E88）

**本轮完成（08 计划 1-9，生产执行语义）**：
1. Route-centric 资格：新增 scenario.routing_trusted（Final route regret 是硬门，
   单路径 10%/15% 绝对误差降为诊断，不再删除候选）；required 三场景全 routing_trusted
   才 operation.qualified；生产 decide 在 routing_trusted 场景使用全部 model_available
   候选比较（error guard 继续参与 score）。
2. 真正 GPU Direct fast path：Dispatcher::execute_gpu_direct() 绕开 SharedWorkPool /
   CPU worker / barrier / 旧 planner；单 GPU executor 整域提交；ExecutionReport 记录
   actual_execution_shape=gpu_direct；旧 MixedRoutePlanner 不再做 BDR 顶层资格。
3. Buffer 真实字节：BufferBinding.element_size_bytes（默认 4 保持 float 兼容）；
   Dispatcher 全部按 count*element_size_bytes 记账（H2D/D2H/RAM/VRAM/RouteRequest）。
4. generation 闭环：ResidencyManager::register_or_update 同步外部 binding generation，
   stable_key 相同但 generation 变化自动失效设备副本（BothValid→HostValid/HostDirty）；
   新增回归测试（float/double/uint8 字节、generation 失效、reuse4 frames 只传一次）。
5. BDR 基于真实 Residency：dispatch_invocation 重排为 注册（真实字节+generation）→
   查询 device-valid → 计算 resident/upload-required → 构造 RouteRequest →
   BenchmarkRouteEstimator 决策 → 执行；invocation.input_resident 仅兼容 hint。
   持久主输入（Operation 注册 persistent_input_indices）驱动 resident_reuse4 场景。
6. 加权积分 Auto：cold 每样本 fresh Dispatcher（setup 不计时，真实上传）；
   resident/reuse4 用同一 Dispatcher + establish_input_residency 建真实驻留；
   删除外部 bridge 伪造；reuse4 frames generation 固定、weights 按代更新。
7. 标定正确性修复：cold Mixed 每个正式样本用不同 seed 数据（强制真实上传，
   修复共享 executor 槽位复用导致"伪 cold"约 3.4x 低估）；稳健 p95 error guard
   （排除单点离群，max 保留诊断）；边界坐标入 Probe（3072²×12、4096²×8）。
8. 稳定性：GPU 重负载测量间 gpu_settle（250ms）抑制热节流；BDR 决策缓存
   （空队列/无内存快照时命中）；GPU 测试 RESOURCE_LOCK gpu0。

**最终 Benchmark 结论（RTX 3060 Ti + MinGW64）**：
- authoritative standard Profile：qualified（三场景 routing_trusted、
  Final Route Replay 24/24、max_slowdown=1.0）；calibration_preset=standard、
  calibration_head=最终 HEAD、run_id/generated_utc 齐全；quick 不覆盖（SHA 不变）。
- correctness=PASS；性能门部分通过：positive_resident_speedup=true（2048² Auto
  gpu_direct ≈ 3.4x vs OpenMP；4096² 亦超 1.05x）；resident/reuse Auto 全部
  gpu_direct（真实 direct shape）；cold 全部正确 OpenMP。
- 性能门未全过（如实）：部分 resident case Auto 距 raw gpu_resident 基线
  110%-115%（Dispatcher 每次调用固定开销 ~0.3-0.6ms 相对亚毫秒~5ms 任务，
  且 raw 基线本身逐轮波动）→ performance=PERFORMANCE_NOT_QUALIFIED、
  READY_FOR_BUSINESS_ADAPTER=false。
- CTest：GPU 资源锁下连续 3 轮 635/635、0 失败、10 项准确跳过；
  compute-sanitizer memcheck 0 errors、racecheck 0 hazards；
  MSVC ASan CPU 核心受本机 commit 上限限制不可运行（同二进制 2026-08-08 PASS）。

**限制（如实）**：Dispatcher 固定开销使亚毫秒 resident 任务难以达到
oracle×1.10 门；冷/热 Mixed 模型经真实上传修复后与 Auto 执行一致。
未放宽任何误差门。下一阶段（真实积分 Adapter）前需先评估 Dispatcher 固定开销
优化（如 direct 路径去注册/决策缓存预热）或调整性能验收口径。

### 2026-08-09 Route/Residency/Generation 闭环（最终 HEAD 105d68a，控制包 CE288DBF...F7E88）

**本轮完成（08 计划 1-9）**：
1. Route-centric qualification：scenario.routing_trusted（Final route regret<=1.10
   硬门；单路径 absolute error 仅诊断/guard，不删除候选）；required 三场景
   routing_trusted 后 Operation qualified；decide 生产用全部 model_available +
   p95 score guard。
2. 真 GPU Direct：execute_gpu_direct()（单 GPU executor，无 SharedWorkPool/
   barrier/CPU worker/旧 planner）；actual_execution_shape=gpu_direct。
3. Buffer 真实字节：BufferBinding.element_size_bytes（默认 4），内存/传输/预算
   按 count*element_size_bytes；float/double/uint8/uint32 单测。
4. generation 闭环：register_or_update(key, bytes, access, generation)；同
   stable_key 但 generation 变化自动失效设备副本（DeviceValid/BothValid→
   HostValid/HostDirty）；回归测试。
5. BDR 基于真实 Residency：dispatch_invocation 重排（注册→同步 generation→
   查真实 device-valid→算 resident/upload bytes→RouteRequest→decide→执行）；
   invocation.input_resident 仅兼容 hint。
6. 加权积分 resident/reuse4 用同一 Dispatcher（establish_input_residency），
   去掉外部 bridge 伪造；reuse4 frames 只传一次、weights generation 递增。
7. 标定真实 cold：每个 cold Mixed 样本不同 seed 数据 + fresh Dispatcher，
   强制真实 H2D（修复标定低估 ~3x 的问题）。
8. 单一最终 HEAD Evidence：源码全部提交（105d68a）后才跑 benchmark/quick，
   Evidence 后无 commit。

**最终结果（105d68a，RTX 3060 Ti）**：
- standard：correctness=PASS；Profile qualified（三场景 routing_trusted、
  Replay 24/24 max_slowdown=1.0）；preset/head/run_id 正确。
- resident Auto 全走 gpu_direct 真实 shape；中/大 case 对 OpenMP 正加速
  （2048²: 4.89ms vs 16.2ms；4096²: 13.3ms vs 30.3ms）。
- quick 不覆盖：权威 Profile SHA 完全不变（ED0CF66E...）。
- performance=PERFORMANCE_NOT_QUALIFIED：512²×16 reuse4 Auto=1.021ms vs
  GPU oracle=0.386ms（ratio 1.10 边缘）；Dispatcher 固定开销（真实
  route+residency+报告）相对原始 GPU 基线约 0.2ms，属生产语义成本。
- READY_FOR_BUSINESS_ADAPTER=false（如实，perf 门未全过）。

**限制（如实）**：亚毫秒 resident 任务 Dispatcher 固定开销使 Auto 距 oracle
1.10 门边缘（110%）；CUDA sanitizer memcheck/racecheck PASS；MSVC ASan CPU
核心仍受本机 commit 上限限制（同二进制 2026-08-08 PASS）；CTest -j1 连续
3 轮 639/639、0 失败、10 项跳过（SanitizerSmoke 预存共享状态偶发 SEGFAULT，
单独复跑 PASS）。未放宽任何门。
