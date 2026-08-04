// astro/compute/acr.hpp — ACR 公共 API
// Phase B：parallel_for/tiles/reduce/batch/scan/chunks/run_for + Buffer/Event。
// 设计（控制包 03_PUBLIC_API_SPEC.md）：
//   1. 公共头不暴露 tbb::/alpaka::/starpu_*/cuda*/hip*/sycl:: 类型
//   2. backend 类型只出现在 .cpp，tbb 完全封装在 runtime.cpp
//   3. 模板实现内联在此，调用 detail::submit_* type-erased 接口（不依赖 tbb 头）
//   4. ACR_KERNEL_ACC 映射 backend 注解（CPU baseline 空）
//   5. 默认 FP32 允许末位差异；FP64 声明需确定性归约
//   6. lazy initialization：首次 API 调用才初始化 runtime singleton
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "astro/compute/task_traits.hpp"

namespace astro::compute {

// ===== 错误码 =====
enum class StatusCode : int {
    Ok = 0, NotInitialized, InvalidArgument, OutOfMemory, OutOfBounds,
    BackendUnavailable, KernelFailed, Cancelled, ProfileMissing, ProfileStale,
    ProfileCorrupt, DeviceLost, UnsupportedPrecision, InternalError,
};

class AcrError : public std::runtime_error {
public:
    AcrError(StatusCode code, const std::string& msg) : std::runtime_error(msg), code_(code) {}
    StatusCode code() const noexcept { return code_; }
private:
    StatusCode code_;
};

// ===== Kernel 标识 =====
enum class KernelId : std::uint32_t {
    Custom = 0, Copy, Triad, AXPY, Dot, Transpose, Convolution2D,
    Histogram256, Scan, Gather, Scatter, Mandelbrot, Gemm, Fft,
};

// ===== 范围 =====
struct Range1D {
    std::size_t begin{0};
    std::size_t end{0};
    constexpr std::size_t size() const noexcept { return end >= begin ? end - begin : 0; }
    constexpr bool empty() const noexcept { return end <= begin; }
};

struct Extent2D {
    std::size_t width{0};
    std::size_t height{0};
    constexpr std::size_t count() const noexcept { return width * height; }
};

struct TileShape {
    std::size_t tile_w{0};
    std::size_t tile_h{0};
};

// ===== 数值策略 =====
enum class Precision { Default, FP32, FP64, Integer };

struct ExecutionHints {
    Precision precision{Precision::Default};
    bool deterministic{false};
    bool prefer_resident{false};
    std::uint32_t chunk_hint{0};
    std::uint32_t grainsize{0};
};

// ===== Event =====
// EventImpl 完整定义在 runtime_internal.h（仅 runtime.cpp/event.cpp include）。
// 公共头只前向声明，shared_ptr<detail::EventImpl> 不需要完整类型。
namespace detail { class EventImpl; }

class Event {
public:
    Event();
    ~Event();
    Event(Event&&) noexcept;
    Event& operator=(Event&&) noexcept;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    void wait() const;
    bool ready() const noexcept;
    void cancel();
    bool cancelled() const noexcept;
    StatusCode status() const noexcept;

    explicit Event(std::shared_ptr<detail::EventImpl> impl);
    detail::EventImpl* impl() const noexcept;
private:
    std::shared_ptr<detail::EventImpl> impl_;
};

// ===== BufferView =====
template<class T>
class BufferView {
public:
    BufferView() = default;
    BufferView(T* data, std::size_t count) : data_(data), count_(count) {}
    BufferView(T* data, std::size_t count, std::size_t stride, std::size_t pitch)
        : data_(data), count_(count), stride_(stride), pitch_(pitch) {}

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    std::size_t count() const noexcept { return count_; }
    std::size_t stride() const noexcept { return stride_; }
    std::size_t pitch() const noexcept { return pitch_; }
    bool empty() const noexcept { return count_ == 0; }

    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }

    BufferView<T> subview(std::size_t offset, std::size_t sub_count) const {
        if (offset + sub_count > count_) throw AcrError(StatusCode::OutOfBounds, "subview out of bounds");
        return BufferView<T>(data_ + offset, sub_count, stride_, pitch_);
    }
private:
    T* data_{nullptr};
    std::size_t count_{0};
    std::size_t stride_{1};
    std::size_t pitch_{0};
};

// ===== Buffer（拥有式 host 内存）=====
template<class T>
class Buffer {
public:
    Buffer() = default;
    explicit Buffer(std::size_t count) : data_(count > 0 ? new T[count]() : nullptr), count_(count) {}
    Buffer(std::size_t count, const T& init) : data_(count > 0 ? new T[count] : nullptr), count_(count) {
        std::fill(data_.get(), data_.get() + count, init);
    }
    Buffer(Buffer&& other) noexcept
        : data_(std::move(other.data_)), count_(other.count_) {
        other.count_ = 0;
    }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            count_ = other.count_;
            other.count_ = 0;
        }
        return *this;
    }
    T* data() noexcept { return data_.get(); }
    const T* data() const noexcept { return data_.get(); }
    std::size_t count() const noexcept { return count_; }
    bool empty() const noexcept { return count_ == 0; }
    BufferView<T> view() noexcept { return BufferView<T>(data_.get(), count_); }
    BufferView<const T> view() const noexcept { return BufferView<const T>(data_.get(), count_); }
    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    void resize(std::size_t new_count) { data_.reset(new_count > 0 ? new T[new_count]() : nullptr); count_ = new_count; }
private:
    std::unique_ptr<T[]> data_;
    std::size_t count_{0};
};

// ===== kernel 注解宏（CPU baseline 空，alpaka backend 映射 ALPAKA_FN_ACC）=====
#ifndef ACR_KERNEL_ACC
#define ACR_KERNEL_ACC
#endif
#ifndef ACR_KERNEL_HOST
#define ACR_KERNEL_HOST
#endif

// ===== Runtime 控制 =====
struct RuntimeConfig {
    std::uint32_t max_threads{0};
    std::uint32_t arena_concurrency{0};
    bool enable_work_stealing{true};
};
void runtime_init(const RuntimeConfig& config = {});
bool runtime_initialized() noexcept;
void runtime_shutdown();
std::size_t runtime_worker_count() noexcept;
std::string runtime_status_json();
void runtime_set_log_level(const std::string& level);

// ===== detail: type-erased runtime 接口（tbb 封装在 runtime.cpp）=====
namespace detail {

using RangeKernelFn   = void(*)(std::size_t begin, std::size_t end, void* user_data);
using TileKernelFn    = void(*)(std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th, void* user_data);
using ItemKernelFn    = void(*)(std::size_t item_index, void* user_data);
using ReduceKernelFn  = void(*)(std::size_t begin, std::size_t end, void* acc, void* user_data);
using ReduceCombineFn = void(*)(void* dst, const void* src, void* user_data);
using ScanKernelFn    = void(*)(std::size_t i, void* user_data);
using ReleaseFn       = void(*)(void* user_data);

Event submit_range(Range1D range, RangeKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);
Event submit_2d(Extent2D extent, TileKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);
Event submit_tiles(Extent2D extent, TileShape tile, TileKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);
Event submit_batch(std::size_t item_count, ItemKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);
Event submit_chunks(Range1D range, std::size_t chunk_size, RangeKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);
Event submit_serial(Range1D range, RangeKernelFn fn, void* user_data, ReleaseFn rel, ExecutionHints hints);

// 归约：分块局部归约 + 合并，结果写入 result_out
void submit_reduce(Range1D range, const void* identity, std::size_t elem_size,
                   ReduceKernelFn reduce_fn, ReduceCombineFn combine_fn,
                   void* user_data, ReleaseFn rel, ExecutionHints hints, void* result_out);

// ===== Phase B2：TaskTraits-based 新接口（接受 TaskDescriptor 字段，type-erased）=====
// 这些接口由 runtime.cpp 实现，内部构造 TaskDescriptor 并调用 CostEstimator + Dispatcher。
// 旧 submit_* 保留为 CPU baseline 直接路径（CostEstimator 可选 fallback）。
// 注意：TaskDescriptor 完整类型在 core/task_descriptor.hpp 的 astro::compute 命名空间内，
// runtime.cpp 通过 #include "task_descriptor.hpp" 获取完整类型；此处不在 detail 命名空间
// 前向声明，避免与 astro::compute::TaskDescriptor 形成名字遮蔽。

Event submit_range_with_desc(OperationId id, Range1D range, TaskTraits traits,
                              RangeKernelFn fn, void* user_data, ReleaseFn rel);
Event submit_tiles_with_desc(OperationId id, Extent2D extent, TileShape tile,
                              TaskTraits traits, TileKernelFn fn,
                              void* user_data, ReleaseFn rel);
Event submit_batch_with_desc(OperationId id, std::size_t item_count, TaskTraits traits,
                              ItemKernelFn fn, void* user_data, ReleaseFn rel);
void submit_reduce_with_desc(OperationId id, Range1D range, TaskTraits traits,
                              const void* identity, std::size_t elem_size,
                              ReduceKernelFn reduce_fn, ReduceCombineFn combine_fn,
                              void* user_data, ReleaseFn rel, void* result_out);

} // namespace detail

// ===== 模板 API 实现（内联，调用 detail::submit_*，不依赖 tbb 头）=====

template<class KernelFn>
Event parallel_for(KernelId /*id*/, Range1D range, KernelFn&& fn, ExecutionHints hints = {}) {
    if (range.empty()) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t b, std::size_t e, void* ud) {
        F* f = static_cast<F*>(ud);
        for (std::size_t i = b; i < e; ++i) (*f)(i);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_range(range, wrapper, heap, rel, hints);
}

template<class KernelFn>
Event run_for(KernelId /*id*/, Range1D range, KernelFn&& fn, ExecutionHints hints = {}) {
    if (range.empty()) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t b, std::size_t e, void* ud) {
        F* f = static_cast<F*>(ud);
        for (std::size_t i = b; i < e; ++i) (*f)(i);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_serial(range, wrapper, heap, rel, hints);
}

template<class KernelFn>
Event parallel_for_2d(KernelId /*id*/, Extent2D extent, KernelFn&& fn, ExecutionHints hints = {}) {
    if (extent.count() == 0) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    // 2D：用 tile 接口，tile=1x1
    auto wrapper = +[](std::size_t tx, std::size_t ty, std::size_t /*tw*/, std::size_t /*th*/, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(tx, ty);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    TileShape t{1, 1};
    return detail::submit_tiles(extent, t, wrapper, heap, rel, hints);
}

template<class KernelFn>
Event parallel_tiles(KernelId /*id*/, Extent2D extent, TileShape tile, KernelFn&& fn, ExecutionHints hints = {}) {
    if (extent.count() == 0 || tile.tile_w == 0 || tile.tile_h == 0) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(tx, ty, tw, th);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_tiles(extent, tile, wrapper, heap, rel, hints);
}

template<class KernelFn>
Event parallel_batch(KernelId /*id*/, std::size_t item_count, KernelFn&& fn, ExecutionHints hints = {}) {
    if (item_count == 0) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t idx, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(idx);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_batch(item_count, wrapper, heap, rel, hints);
}

template<class KernelFn>
Event parallel_chunks(KernelId /*id*/, Range1D range, std::size_t chunk_size, KernelFn&& fn, ExecutionHints hints = {}) {
    if (range.empty() || chunk_size == 0) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t b, std::size_t e, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(b, e);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_chunks(range, chunk_size, wrapper, heap, rel, hints);
}

template<class T, class KernelFn, class ReduceOp>
T parallel_reduce(KernelId /*id*/, Range1D range, T identity, KernelFn&& fn, ReduceOp&& reduce_op,
                   ExecutionHints hints = {}) {
    if (range.empty()) return identity;
    using F = std::decay_t<KernelFn>;
    using R = std::decay_t<ReduceOp>;
    struct Bundle { F map_fn; R reduce_op; };
    Bundle* heap = new Bundle{std::forward<KernelFn>(fn), std::forward<ReduceOp>(reduce_op)};
    auto reduce_kernel = +[](std::size_t b, std::size_t e, void* acc, void* ud) {
        Bundle* p = static_cast<Bundle*>(ud);
        T* a = static_cast<T*>(acc);
        for (std::size_t i = b; i < e; ++i) *a = p->reduce_op(*a, p->map_fn(i));
    };
    auto combine = +[](void* dst, const void* src, void* ud) {
        Bundle* p = static_cast<Bundle*>(ud);
        *static_cast<T*>(dst) = p->reduce_op(*static_cast<T*>(dst), *static_cast<const T*>(src));
    };
    auto rel = +[](void* ud) { delete static_cast<Bundle*>(ud); };
    T result = identity;
    detail::submit_reduce(range, &identity, sizeof(T), reduce_kernel, combine, heap, rel, hints, &result);
    return result;
}

template<class T, class KernelFn, class Op>
Event parallel_scan(KernelId /*id*/, BufferView<T> input, BufferView<T> output, T identity,
                     KernelFn&& /*fn*/, Op&& op, ExecutionHints hints = {}) {
    // Phase B：串行扫描 + 屏障（Phase H E08 用专用库优化）
    if (input.count() != output.count() || input.count() == 0) {
        throw AcrError(StatusCode::InvalidArgument, "scan: input/output size mismatch");
    }
    using O = std::decay_t<Op>;
    O* heap = new O(std::forward<Op>(op));
    auto scan_fn = +[](std::size_t i, void* ud) {
        O* p = static_cast<O*>(ud);
        // 注意：串行扫描需要前缀和，这里由 runtime 串行执行保证顺序
        (void)p; (void)i;
    };
    auto rel = +[](void* ud) { delete static_cast<O*>(ud); };
    // 简化：Phase B 串行扫描
    T acc = identity;
    for (std::size_t i = 0; i < input.count(); ++i) {
        acc = op(acc, input[i]);
        output[i] = acc;
    }
    (void)scan_fn;
    delete heap;
    Event e;
    return e;
}

// ============================================================================
// Phase B2：TaskTraits-based 公共 API（新签名，强制 TaskTraits，无 cpu_share/gpu_share）
// ============================================================================
// 设计：
//   1. OperationId 是诊断/缓存标识（string_view），不是固定比例路由键
//   2. TaskTraits 强制提供（描述任务类别/访存/强度/数值策略）
//   3. 调用 detail::submit_*_with_desc → CostEstimator → Dispatcher → backend
//   4. 旧 KernelId-based API 保留向后兼容（走旧 submit_* → CPU runtime）
//   5. 公共头不暴露第三方类型（TaskDescriptor 前向声明，完整类型在 .cpp 内）
//   6. Args 通过 lambda 捕获传递（与旧 API 一致），不直接支持可变 Args
//      （spec 写 Args&&... 是 future-proofing，当前实现要求 kernel 单参数）
//
// ============================================================================
// 23 号计划 §1：CPU-only compatibility API 标记
// ----------------------------------------------------------------------------
// 以下 parallel_for/parallel_tiles/parallel_reduce/parallel_batch 接受普通
// C++ lambda / 函数对象 / host 函数指针，它们**只能作为 CPU 兼容执行入口**，
// 不能直接作为 CUDA/HIP/SYCL device kernel 执行。
//
// 可加速路径必须使用：
//   OperationId + KernelRegistration + KernelInvocation + KernelRegistry
// （见 include/astro/compute/kernel_registry.hpp）。
// Dispatcher 只会把 invocation 交给 supports() 为 true 的 executor；
// 设备 launcher 缺失时回退 CPU 并如实报告，不得静默伪装 GPU 执行。
// ============================================================================

// parallel_for(OperationId, Range1D, TaskTraits, KernelFn)
// ACR 根据画像和数据驻留自动决定 CPU/GPU、块大小、并发方式。
// [CPU-ONLY compatibility API] host lambda 不能直接作为 CUDA kernel 执行；
// 需要设备加速时通过 KernelRegistry 注册设备 launcher。
template<class KernelFn>
Event parallel_for(OperationId id, Range1D range, TaskTraits traits, KernelFn&& fn) {
    if (range.empty()) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t b, std::size_t e, void* ud) {
        F* f = static_cast<F*>(ud);
        for (std::size_t i = b; i < e; ++i) (*f)(i);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_range_with_desc(id, range, traits, wrapper, heap, rel);
}

// parallel_tiles(OperationId, Extent2D, TileShape, TaskTraits, KernelFn)
// ACR 负责：边缘 Tile、halo 只读视图、输出所有权、CPU/GPU 动态领取和尾部收缩。
// [CPU-ONLY compatibility API] 同上。
template<class KernelFn>
Event parallel_tiles(OperationId id, Extent2D extent, TileShape preferred_tile,
                     TaskTraits traits, KernelFn&& fn) {
    if (extent.count() == 0 || preferred_tile.tile_w == 0 || preferred_tile.tile_h == 0) {
        Event e; return e;
    }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(tx, ty, tw, th);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_tiles_with_desc(id, extent, preferred_tile, traits,
                                          wrapper, heap, rel);
}

// parallel_reduce(OperationId, Range1D, TaskTraits, T identity, MapKernel, ReduceOp)
// 每设备先产生局部结果，再按 NumericPolicy 合并。
// 默认允许正常浮点末位差异；deterministic_merge=true 或 fp64 accumulator 走确定性路径。
// [CPU-ONLY compatibility API] host map/reduce 不能直接作为 CUDA kernel 执行。
template<class T, class MapKernel, class ReduceOp>
T parallel_reduce(OperationId id, Range1D range, TaskTraits traits, T identity,
                  MapKernel&& map, ReduceOp&& reduce_op) {
    if (range.empty()) return identity;
    using F = std::decay_t<MapKernel>;
    using R = std::decay_t<ReduceOp>;
    struct Bundle { F map_fn; R reduce_op; };
    Bundle* heap = new Bundle{std::forward<MapKernel>(map), std::forward<ReduceOp>(reduce_op)};
    auto reduce_kernel = +[](std::size_t b, std::size_t e, void* acc, void* ud) {
        Bundle* p = static_cast<Bundle*>(ud);
        T* a = static_cast<T*>(acc);
        for (std::size_t i = b; i < e; ++i) *a = p->reduce_op(*a, p->map_fn(i));
    };
    auto combine = +[](void* dst, const void* src, void* ud) {
        Bundle* p = static_cast<Bundle*>(ud);
        *static_cast<T*>(dst) = p->reduce_op(*static_cast<T*>(dst), *static_cast<const T*>(src));
    };
    auto rel = +[](void* ud) { delete static_cast<Bundle*>(ud); };
    T result = identity;
    detail::submit_reduce_with_desc(id, range, traits, &identity, sizeof(T),
                                     reduce_kernel, combine, heap, rel, &result);
    return result;
}

// parallel_batch(OperationId, item_count, TaskTraits, KernelFn)
// 适合大量独立对象。高度不均匀时设置 uniformity=highly_variable，
// 调度器使用更细粒度动态领取。
// [CPU-ONLY compatibility API] 同上。
template<class KernelFn>
Event parallel_batch(OperationId id, std::size_t item_count, TaskTraits traits, KernelFn&& fn) {
    if (item_count == 0) { Event e; return e; }
    using F = std::decay_t<KernelFn>;
    F* heap = new F(std::forward<KernelFn>(fn));
    auto wrapper = +[](std::size_t idx, void* ud) {
        F* f = static_cast<F*>(ud);
        (*f)(idx);
    };
    auto rel = +[](void* ud) { delete static_cast<F*>(ud); };
    return detail::submit_batch_with_desc(id, item_count, traits, wrapper, heap, rel);
}

// ===== Phase B2 辅助：KernelId → OperationId 转换（向后兼容）=====
// 旧 API 用 KernelId，新 API 用 OperationId。此函数把 KernelId 转为 string_view，
// 供旧 API 走新路径时使用（诊断/日志）。
inline OperationId kernel_id_to_operation_id(KernelId k) noexcept {
    switch (k) {
        case KernelId::Custom:         return "kernel.custom";
        case KernelId::Copy:           return "kernel.copy";
        case KernelId::Triad:          return "kernel.triad";
        case KernelId::AXPY:           return "kernel.axpy";
        case KernelId::Dot:            return "kernel.dot";
        case KernelId::Transpose:      return "kernel.transpose";
        case KernelId::Convolution2D:  return "kernel.convolution2d";
        case KernelId::Histogram256:   return "kernel.histogram256";
        case KernelId::Scan:           return "kernel.scan";
        case KernelId::Gather:         return "kernel.gather";
        case KernelId::Scatter:        return "kernel.scatter";
        case KernelId::Mandelbrot:     return "kernel.mandelbrot";
        case KernelId::Gemm:           return "kernel.gemm";
        case KernelId::Fft:            return "kernel.fft";
    }
    return "kernel.unknown";
}

} // namespace astro::compute
