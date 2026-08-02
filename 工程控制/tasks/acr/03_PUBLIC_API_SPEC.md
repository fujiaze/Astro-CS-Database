# 公共接口设计

## 1. 目标

算法作者：

- 不写线程管理；
- 不选择CPU或GPU；
- 不填写CPU/GPU百分比；
- 不处理显存传输；
- 只写一个工作项、Tile、局部归约或独立批次逻辑；
- 选择一个最接近的任务类别，必要时给少量特征提示。

## 2. 核心类型

```cpp
namespace astro::compute {

using OperationId = std::string_view; // 诊断、缓存和实现兼容性标识，不是固定比例路由键

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
    custom
};

enum class AccessPattern { contiguous, strided, local_neighborhood, random, scatter };
enum class WorkUniformity { uniform, mildly_variable, highly_variable };
enum class IntensityClass { memory_bound, balanced, compute_bound };

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

## 3. 独立元素

```cpp
template<class Kernel, class... Args>
Event parallel_for(OperationId id,
                   Range1D range,
                   TaskTraits traits,
                   Kernel kernel,
                   Args&&... args);
```

最小调用：

```cpp
auto e = acr::parallel_for(
    "classic.axpy.fp32",
    {0, n},
    acr::TaskTraits{
        .task_class = acr::TaskClass::elementwise,
        .access = acr::AccessPattern::contiguous,
        .intensity = acr::IntensityClass::memory_bound
    },
    AxpyItem{}, out, x, y, alpha);
e.wait();
```

ACR根据画像和数据驻留自动决定CPU、GPU、块大小和并发方式。

## 4. Tile接口

```cpp
template<class Kernel, class... Args>
Event parallel_tiles(OperationId id,
                     Extent2D extent,
                     TileShape preferred_tile,
                     TaskTraits traits,
                     Kernel kernel,
                     Args&&... args);
```

ACR负责：边缘Tile、halo只读视图、输出所有权、CPU/GPU动态领取和尾部收缩。

## 5. Reduce接口

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

每设备先产生局部结果，再按NumericPolicy合并。默认允许正常浮点末位差异。

## 6. Batch接口

```cpp
template<class Kernel, class... Args>
Event parallel_batch(OperationId id,
                     std::size_t item_count,
                     TaskTraits traits,
                     Kernel kernel,
                     Args&&... args);
```

适合大量独立对象。高度不均匀时设置 `uniformity=highly_variable`，调度器使用更细粒度动态领取。

## 7. 专用原语

存在跨项依赖或冲突时，不得伪装成普通for：

- prefix scan使用成熟scan adapter；
- histogram使用局部直方图/原子专用路径；
- FFT/GEMM调用成熟库adapter；
- scatter必须声明冲突模型。

## 8. 禁止的API

不得出现：

```cpp
run(..., cpu_share=0.2, gpu_share=0.8);
set_route_weight("cuda:0", 0.8);
```

用户只可配置资源占用上限、启用/禁用后端和回退策略。

## 9. 禁止暴露第三方类型

公共头文件不得暴露 `tbb::*`、`alpaka::*`、CUDA/HIP/SYCL、StarPU或vendor handle。
