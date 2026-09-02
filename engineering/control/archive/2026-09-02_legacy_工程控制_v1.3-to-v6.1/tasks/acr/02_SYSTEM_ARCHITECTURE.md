# 系统架构

## 1. 分层

```text
Future hotspot（本支线不接入）
        |
        | parallel_for / parallel_tiles / parallel_reduce / parallel_batch
        v
ACR Public API
        |
        +-- TaskDescriptor Builder
        +-- Buffer / Residency / Event
        +-- HardwareProfile Store (read-only at runtime)
        +-- CostEstimator
        +-- Work-Conserving Dispatcher
        +-- Utilization & Capacity Controller
        +-- Diagnostics / Fallback / Coverage
        |
        +--> CPU Backend
        |      +-- oneTBB arena
        |      +-- scalar baseline
        |      +-- SSE / AVX / AVX2 / AVX-512 variants
        |
        +--> GPU Backends
        |      +-- portable adapter selected by ADR
        |      +-- CUDA / HIP / SYCL plugins
        |
        +--> Library Adapters
               +-- FFT / BLAS / scan / primitives
```

## 2. 核心数据流

```text
API call
  → validate TaskTraits
  → build TaskDescriptor
  → locate valid HardwareProfile
  → enumerate eligible devices and chunk candidates
  → CostEstimator predicts queue + transfer + compute + merge
  → Dispatcher creates shared unstarted work pool
  → CPU/GPU workers dynamically claim chunks
  → coverage and events ensure exactly once
  → merge or publish output residency
```

无有效画像时走明确 CPU fallback，不得悄悄伪造 GPU 路由。

## 3. HardwareProfile

按设备保存多维能力曲线，而不是每 kernel 固定路线：

- arithmetic：精度、操作、ISA/线程或 GPU；
- memory：缓存、主存、显存、NUMA和尺寸；
- transfer：H2D/D2H/P2P、普通/pinned、尺寸；
- reduction：操作、精度、尺寸；
- convolution：direct/separable/FFT、核、stride、尺寸和驻留；
- irregular：gather/scatter/sparsity/atomic contention；
- branch：uniformity/work variance；
- overhead：submit/launch/event/alloc/sync/merge；
- library：FFT/BLAS/scan adapter曲线；
- confidence：样本范围、留出误差和低置信度标记。

正式 schema 见 `schemas/hardware_profile.schema.json`。

## 4. TaskDescriptor

任务提供少量、稳定、可审计的特征：

- TaskClass、工作域和精度；
- 读写字节、shape、stride、halo；
- 连续/局部/随机/scatter；
- 稀疏度、原子冲突、分支均匀性；
- 数据驻留；
- 可拆性、混合设备安全性、合并策略。

普通算法作者选预定义 TaskClass，不要求手算精确 FLOP。

## 5. CostEstimator

对每个设备与候选块估算：

```text
T_finish = queue_wait
         + submit_or_launch
         + transfer
         + profile_predicted_compute
         + merge_or_sync
```

- 画像曲线按 log2 尺寸分段插值；
- 小任务必须计入固定开销；
- 数据驻留优先；
- 稀疏、原子和分支使用对应能力族；
- 低置信度模型加安全惩罚；
- 不能用理论峰值替代实测画像。

## 6. 动态 Dispatcher

- 共享未开始工作池；
- CPU 和每张 GPU 独立 worker/queue；
- 设备按成本模型领取适合自己的批次；
- 完成后继续领取；
- 尾部 guided 收缩；
- 已开始块不迁移；
- coverage ID 保证每块恰好一次；
- 设备失效时只回收未开始块；
- 不为“吃满”进行预计负收益的数据迁移。

实际 CPU/GPU 完成量是运行结果，不是输入参数或持久路由。

## 7. 资源控制

路由和资源控制分离：

- CostEstimator 决定哪个设备更适合下一块；
- Utilization Controller 调节队列水位和提交节奏，使 CPU/GPU 接近用户目标；
- Capacity Controller 负责 RAM/VRAM 上限；
- 控制器不得修改 HardwareProfile。

## 8. Lazy 与 dormant

ACR 所有全局资源必须 lazy：

- 普通 AstroCS 启动不创建线程；
- 不枚举 GPU；
- 不读取画像；
- 不发未标定警告；
- GPU SDK不是 CPU-only 构建强制依赖。

## 9. 推荐目录

```text
lib/acr/
  include/astro/compute/
  api/
  core/
  buffers/
  topology/
  qualification/
    benchmarks/
    profile/
    model_fit/
  routing/
    task_descriptor/
    cost_estimator/
  scheduler/
  utilization/
  backends/
    cpu/
    cuda/
    hip/
    sycl/
    portable/
    libraries/
  diagnostics/
  cli/
  tests/
```
