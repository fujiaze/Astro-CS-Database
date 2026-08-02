# 系统架构

## 1. 分层

```text
Future AstroCS Hotspot
        |
        | parallel_for / parallel_tiles / parallel_reduce / parallel_batch
        | TaskDescriptor: class, size, precision, access, residency, halo...
        v
ACR Public API
        |
        +-- Task Descriptor & Kernel Registry
        +-- Buffer / Residency / Event Layer
        +-- Hardware Profile Reader
        +-- Cost Estimator
        +-- Work-Conserving Dispatcher
        +-- Utilization Budget Controller
        +-- Diagnostics & Fallback
        |
        +--> CPU Backend
        |      +-- oneTBB task arena
        |      +-- baseline/scalar
        |      +-- SSE/AVX/AVX2/AVX-512 variants
        |
        +--> Accelerator Backends
               +-- alpaka adapter
               +-- CUDA / HIP / SYCL optional plugins
               +-- vendor FFT/BLAS/primitive adapters
```

## 2. Hardware Profile，不是每 kernel 固定比例

Qualification 输出一份机器级多维画像：

- CPU各ISA和线程规模的算术/内存/归约曲线；
- 每张GPU的算术、显存、原子、分支、卷积/Stencil曲线；
- H2D、D2H、双向传输、pinned memory和启动延迟；
- task提交、event、同步、分配和队列开销；
- NUMA本地/远端带宽；
- 专用库能力和workspace成本。

不保存用户可编辑的 CPU/GPU权重，不以某个业务 kernel 为主键生成百分比路线。

## 3. Task Descriptor

任务至少描述：

- `TaskClass`；
- 工作域形状和项目数；
- FP32/FP64及累加精度；
- 连续、局部邻域、随机、scatter或原子访存；
- 每项大致读写字节和计算强度等级；
- 分支/工作量均匀性；
- halo、稀疏度或冲突级别；
- 输入和输出当前位置；
- 是否可拆、是否允许CPU/GPU混合。

普通算法作者优先选择预定义类别，不要求填写精确FLOP数。

## 4. Cost Estimator

估算每个候选设备和块大小：

```text
T = queue_wait
  + task_or_kernel_launch
  + required_transfer
  + profile_predicted_compute
  + merge_or_sync
```

性能项从画像的分段曲线或对数尺寸插值获取，不使用硬件理论峰值代替实测。

## 5. Work-Conserving Dispatcher

- 所有合格设备拥有自己的执行队列；
- 共享未开始工作池；
- GPU按画像领取较大批次，CPU领取较小批次；
- 设备完成后继续领取剩余工作；
- 尾部自动缩小批次，避免一个设备拿走过大尾块；
- 首选设备忙时，空闲合格设备可领取工作；
- 不移动已开始工作；
- 不修改硬件画像；
- 不为填满设备制造明显负收益的数据迁移。

## 6. Buffer与驻留

统一记录：类型、shape、stride、所有权、host/device副本、有效副本、写后失效、异步事件和Tile视图。未来可非拥有式包装PipelineFrame，但本支线不修改PipelineFrame。

## 7. Utilization Controller

资源控制和路由分离：

- 路由器判断在哪执行预计更合适；
-控制器限制提交节奏、队列深度和容量，使CPU/GPU约达到配置目标；
- 控制器不得修改画像参数。

## 8. 推荐目录

```text
source/compute/
  api/
  core/
  buffers/
  topology/
  qualification/
    benchmarks/
    profile/
  routing/
    task_descriptor/
    cost_model/
  scheduler/
  backends/
    cpu/
    alpaka/
    cuda/
    hip/
    sycl/
    libraries/
  diagnostics/
  cli/
tests/compute/
tools/acr_benchmark/
```
