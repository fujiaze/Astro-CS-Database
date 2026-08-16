// astro/compute/task_traits.hpp — ACR 任务特征类型
// Phase B1：按 03_PUBLIC_API_SPEC.md §2 定义。
//
// 设计：
// 1. OperationId 是诊断/缓存标识（string_view），不是固定比例路由键
// 2. TaskClass 是任务类别枚举（不是 KernelId），用于 CostEstimator 选画像曲线
// 3. TaskTraits 由算法作者在 parallel_for 等调用点提供，描述任务的特征
// 4. 公共头不暴露第三方类型（tbb/alpaka/cuda/hip/sycl）
// 5. 默认值：elementwise + contiguous + uniform + memory_bound + fp32，可 splittable
// 6. mixed_device_safe=true 表示任务可在 CPU+GPU 混合执行（无设备间依赖）
// 7. requires_atomic=true 表示需要原子冲突处理（histogram/scatter）
// 8. halo_x/halo_y 用于 stencil_2d/convolution 的边界处理
// 9. （08 ）：RouteMode/PartitionKind 定义混合路由与分块契约，
// 目标 OperationId 常量仅覆盖积分/Drizzle 类重负载像素算法
//
// 注意：OperationId 用 string_view，调用方必须保证字符串字面量生命周期；
// TaskDescriptor 内部会复制为 std::string 以保证安全。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace astro::compute {

// ===== OperationId：诊断与缓存标识 =====
// 不是固定比例路由键。用于日志、coverage ID、profile 查找（向后兼容 KernelId）。
using OperationId = std::string_view;

// ===== 任务类别 =====
// CostEstimator 按类别选择画像曲线族（arithmetic/memory/reduction/convolution/...）
enum class TaskClass : std::uint8_t {
    elementwise,              // 逐元素（axpy/copy/scale）
    reduction,                // 归约（sum/dot/min/max）
    stencil_2d,               // 2D 模板（卷积直接/分离的子类）
    convolution_direct,       // 直接 2D 卷积
    convolution_separable,    // 可分离卷积
    resampling_gather,        // 重采样 gather
    histogram_atomic,         // 直方图（原子累加）
    sparse_gather,            // 稀疏 gather
    sparse_scatter,           // 稀疏 scatter
    branch_heavy,             // 分支密集（mandelbrot 等）
    batch_independent,        // 独立批次
    fft_library,              // FFT 库 adapter
    gemm_library,             // GEMM 库 adapter
    custom,                   // 自定义（fallback 到 elementwise 画像）
};

// ===== 访存模式 =====
enum class AccessPattern : std::uint8_t {
    contiguous,            // 连续（STREAM 风格）
    strided,               // 带步长
    local_neighborhood,    // 局部邻域（stencil/conv）
    random,                // 随机访问（gather）
    scatter,               // 散射写
};

// ===== 工作量均匀度 =====
enum class WorkUniformity : std::uint8_t {
    uniform,            // 每个工作项工作量相同
    mildly_variable,   // 轻微变化
    highly_variable,   // 高度变化（mandelbrot 等）→ 细粒度动态领取
};

// ===== 计算强度等级 =====
enum class IntensityClass : std::uint8_t {
    memory_bound,   // 受内存带宽限制
    balanced,       // 平衡
    compute_bound,  // 受计算限制
};

// ===== 数值策略 =====
struct NumericPolicy {
    enum class Compute : std::uint8_t { fp32, fp64 } compute{Compute::fp32};
    enum class Accumulator : std::uint8_t { same, fp64 } accumulator{Accumulator::same};
    bool deterministic_merge{false};  // 归并需要确定性（FP64 accumulator 或 deterministic 标志）
    bool allow_fast_math{false};      // 允许 fast-math（可能牺牲精度）
};

// ===== （08 ）：路由模式 =====
// 正常生产模式为 AutoMixed；CpuOnly/GpuOnly 只用于 correctness 对照、
// Benchmark、故障隔离和明确回退。AutoMixed 允许按边际收益自然退化为
// 仅一种设备，但不得使用固定 CPU/GPU 比例。
enum class RouteMode : std::uint8_t {
    AutoMixed = 0,   // 正常生产模式
    CpuOnly   = 1,   // 调试/对照/回退
    GpuOnly   = 2,   // 调试/对照/资格测试
};

// ===== （08 ）：分块契约 =====
// 算法明确如何安全拆分：
// IndependentOutputTiles：每个块拥有独立输出区域（积分优先）
// PrivatePartialThenMerge：设备/块写私有部分结果，最终明确合并（Drizzle 类）
// 禁止多个设备无协议地并发写同一输出。
enum class PartitionKind : std::uint8_t {
    IndependentOutputTiles = 0,
    PrivatePartialThenMerge = 1,
};

// ===== （ACR 架构冻结 01_ARCHITECTURE_FREEZE.md §3）：驻留策略 =====
// 业务调用只提交一次 Operation，不指定 CPU/GPU 比例、不管理 CUDA stream、
// 不直接分配设备份额；输入/输出的驻留策略由调用方在 Invocation 上显式声明：
// HostOnly — 只从 host 访问（小数据/一次性任务默认）
// PreferDevice — 输入允许跨调用驻留，worker 启动前真实 prefetch
// KeepDevice — 中间结果/输出保留在 device（后续算子复用）
// MaterializeHost — 最终必须物化到 host（GPU 拥有范围 D2H 合并）
enum class ResidencyPolicy : std::uint8_t {
    HostOnly = 0,
    PreferDevice = 1,
    KeepDevice = 2,
    MaterializeHost = 3,
};

// ===== 目标 OperationId=====
// 当前底层合成测试至少覆盖以下 Operation；未来真实算法接入后使用真实
// OperationId 和同一注册机制替换对应合成 Profile。
inline constexpr std::string_view kOpDensePixelAccumulateFp32 =
    "synthetic.dense_pixel_accumulate.fp32";
inline constexpr std::string_view kOpDensePixelAccumulateFp64Acc =
    "synthetic.dense_pixel_accumulate.fp64acc";
inline constexpr std::string_view kOpPixelReduceFp64Acc =
    "synthetic.pixel_reduce.fp64acc";
inline constexpr std::string_view kOpDrizzleLikeScatterFp64Acc =
    "synthetic.drizzle_like_scatter.fp64acc";
inline constexpr std::string_view kOpResidentChain =
    "synthetic.resident_chain";
// ACR 架构冻结（07 C）：加权积分最小接入样例（IndependentOutputTiles）。
// FP32 输入/权重、FP64 累加、FP32 输出；帧栈 frame-major 连续布局。
inline constexpr std::string_view kOpWeightedIntegrationFp64Acc =
    "synthetic.weighted_integration.fp64acc";

// ===== TaskTraits：任务特征描述 =====
// 算法作者在 parallel_for/tiles/reduce/batch 调用点提供。
// CostEstimator 据此选择画像曲线并推算成本与块大小。
struct TaskTraits {
    TaskClass task_class{TaskClass::elementwise};
    AccessPattern access{AccessPattern::contiguous};
    WorkUniformity uniformity{WorkUniformity::uniform};
    IntensityClass intensity{IntensityClass::memory_bound};
    NumericPolicy numeric{};

    bool splittable{true};             // 是否可分块（依赖密集任务应 false）
    bool mixed_device_safe{true};      // 是否可 CPU+GPU 混合执行
    bool requires_atomic{false};       // 是否需要原子冲突处理
    double active_fraction_hint{1.0};  // active 比例（sparse 任务 0.01~0.5）
    std::size_t bytes_read_per_item{0};   // 每工作项读取字节（传输/带宽估算与执行报告用）
    std::size_t bytes_written_per_item{0};// 每工作项写入字节
    std::size_t halo_x{0};             // x 方向 halo（stencil/conv）
    std::size_t halo_y{0};             // y 方向 halo
};

// ===== 辅助：枚举转字符串（日志/诊断用）=====
inline const char* task_class_str(TaskClass c) noexcept {
    switch (c) {
        case TaskClass::elementwise:            return "elementwise";
        case TaskClass::reduction:              return "reduction";
        case TaskClass::stencil_2d:             return "stencil_2d";
        case TaskClass::convolution_direct:     return "convolution_direct";
        case TaskClass::convolution_separable:  return "convolution_separable";
        case TaskClass::resampling_gather:      return "resampling_gather";
        case TaskClass::histogram_atomic:       return "histogram_atomic";
        case TaskClass::sparse_gather:          return "sparse_gather";
        case TaskClass::sparse_scatter:         return "sparse_scatter";
        case TaskClass::branch_heavy:           return "branch_heavy";
        case TaskClass::batch_independent:      return "batch_independent";
        case TaskClass::fft_library:            return "fft_library";
        case TaskClass::gemm_library:           return "gemm_library";
        case TaskClass::custom:                 return "custom";
    }
    return "unknown";
}

inline const char* access_pattern_str(AccessPattern a) noexcept {
    switch (a) {
        case AccessPattern::contiguous:           return "contiguous";
        case AccessPattern::strided:              return "strided";
        case AccessPattern::local_neighborhood:   return "local_neighborhood";
        case AccessPattern::random:               return "random";
        case AccessPattern::scatter:              return "scatter";
    }
    return "unknown";
}

inline const char* work_uniformity_str(WorkUniformity u) noexcept {
    switch (u) {
        case WorkUniformity::uniform:           return "uniform";
        case WorkUniformity::mildly_variable:   return "mildly_variable";
        case WorkUniformity::highly_variable:   return "highly_variable";
    }
    return "unknown";
}

inline const char* intensity_class_str(IntensityClass i) noexcept {
    switch (i) {
        case IntensityClass::memory_bound:   return "memory_bound";
        case IntensityClass::balanced:       return "balanced";
        case IntensityClass::compute_bound:  return "compute_bound";
    }
    return "unknown";
}

} // namespace astro::compute
