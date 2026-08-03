# 公共接口设计

## 1. 目标

未来算法作者只需：

- 写单工作项、单 Tile、局部归约或独立批次逻辑；
- 选择 TaskClass；
- 必要时补充 halo、稀疏度、冲突和数值策略；
- 调用一个 ACR 函数。

算法作者不管理线程、不选设备、不填写 CPU/GPU 比例、不处理显存传输。

## 2. 核心类型

```cpp
namespace astro::compute {

using OperationId = std::string_view; // 诊断与实现兼容标识，不是固定路由主键

enum class TaskClass {
    elementwise,
    reduction,
    stencil_2d,
    convolution_direct,
    convolution_separable,
    resampling_gather,
    histogram_atomic,
    sparse_gather,
    sparse_scatter,
    branch_heavy,
    batch_independent,
    fft_library,
    gemm_library,
    scan_library,
    custom
};

enum class AccessPattern { contiguous, strided, local_neighborhood, random, scatter };
enum class WorkUniformity { uniform, mildly_variable, highly_variable };
enum class IntensityClass { memory_bound, balanced, compute_bound };

enum class DataResidence { host, device, replicated, unknown };

struct NumericPolicy {
    enum class Compute { fp32, fp64 } compute = Compute::fp32;
    enum class Accumulator { same, fp64 } accumulator = Accumulator::same;
    bool deterministic_merge = false;
    bool allow_fast_math = false;
};

struct TaskTraits {
    TaskClass task_class = TaskClass::elementwise;
    AccessPattern access = AccessPattern::contiguous;
    WorkUniformity uniformity = WorkUniformity::uniform;
    IntensityClass intensity = IntensityClass::memory_bound;
    NumericPolicy numeric{};

    bool splittable = true;
    bool mixed_device_safe = true;
    bool requires_atomic = false;

    double active_fraction_hint = 1.0;
    double atomic_contention_hint = 0.0;
    std::size_t bytes_read_per_item = 0;
    std::size_t bytes_written_per_item = 0;
    std::size_t halo_x = 0;
    std::size_t halo_y = 0;
};

struct Range1D { std::size_t begin, end; };
struct Extent2D { std::size_t width, height; };
struct TileShape { std::size_t width, height; };

class Event {
public:
    void wait() const;
    bool ready() const noexcept;
    void cancel();
};

template<class T> class BufferView;
template<class T> class Buffer;

}
```

诊断 JSON 形式见 `schemas/task_descriptor.schema.json`。

## 3. 接口

```cpp
template<class Kernel, class... Args>
Event parallel_for(OperationId id,
                   Range1D range,
                   TaskTraits traits,
                   Kernel kernel,
                   Args&&... args);
```

```cpp
template<class Kernel, class... Args>
Event parallel_tiles(OperationId id,
                     Extent2D extent,
                     TileShape preferred_tile,
                     TaskTraits traits,
                     Kernel kernel,
                     Args&&... args);
```

```cpp
template<class T, class MapKernel, class ReduceOp, class... Args>
T parallel_reduce(OperationId id,
                  Range1D range,
                  TaskTraits traits,
                  T identity,
                  MapKernel map,
                  ReduceOp reduce,
                  Args&&... args);
```

```cpp
template<class Kernel, class... Args>
Event parallel_batch(OperationId id,
                     std::size_t item_count,
                     TaskTraits traits,
                     Kernel kernel,
                     Args&&... args);
```

## 4. 强制调用链

所有接口必须实际进入：

```text
Public API
 → TaskDescriptorBuilder
 → ProfileStore
 → CostEstimator
 → Dispatcher
 → Backend
```

不得出现：

```cpp
parallel_for(OperationId /*ignored*/, ...)
```

也不得在有有效画像和合格 GPU 时仍无条件直达 CPU runtime。无画像 CPU fallback 必须是显式状态和日志路径。

## 5. 禁止 API

```cpp
run(..., cpu_share = 0.2, gpu_share = 0.8);
set_route_weight("cuda:0", 0.8);
set_preferred_backend("kernel", "gpu");
```

用户仅可配置：

- 资源目标和容量限制；
- 启用/禁用后端；
- stale profile策略；
- 诊断级别和回退策略。

## 6. 专用依赖任务

以下不能伪装普通 `parallel_for`：

- scan：成熟 scan primitive adapter；
- histogram/scatter：局部聚合或原子专用路径；
- FFT/GEMM：成熟库 adapter；
- 前后项依赖：显式 DAG 或专用算法。

## 7. 第三方隔离

公共头文件不得暴露 `tbb::*`、alpaka、CUDA/HIP/SYCL、StarPU或 vendor handle。所有第三方依赖留在 backend/adaptor 内。
