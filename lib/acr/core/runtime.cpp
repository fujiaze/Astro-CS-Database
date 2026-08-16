// lib/acr/core/runtime.cpp — ACR 核心 runtime 实现
// Phase B：oneTBB 同步执行（提交即执行，完成后 mark_done）。
//
// 设计要点：
// - lazy singleton：首次 detail::submit_* 调用 ensure_runtime_initialized
// - tbb::global_control 限制全局工作线程数；tbb::task_arena 提供执行上下文
// - tbb 类型只在本文件出现，不暴露给公共头
// - EventImpl 生命周期：submit_* 创建 shared_ptr<EventImpl>，kernel 完成后 mark_done
// - 取消：检查 EventImpl::cancelled，被取消时提前返回
// - release_fn：kernel 完成/异常/取消后调用一次（RAII KernelGuard 保证）
// - 异常：kernel 抛异常 → catch → mark_failed(KernelFailed, e.what)
// - runtime_init 幂等：首次配置生效，后续调用 no-op；shutdown 后可重新 init
//
// Phase B4：submit_*_with_desc 接通 CostEstimator + Dispatcher 调用链。
// - 无画像/无 GPU 时退化为 submit_*（CPU tbb 路径）
// - 有画像 + GPU 时用 Dispatcher::dispatch_range_cost_aware

#include <algorithm>
#include <atomic>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/partitioner.h>
#include <tbb/task_arena.h>

#include "astro/compute/acr.hpp"
#include "astro/compute/runtime_internal.h"
#include "task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../scheduler/dispatcher.hpp"

namespace astro::compute {

// ============================================================================
// Runtime singleton
// ============================================================================

namespace {

struct RuntimeState {
    std::mutex mtx;                                   // 保护 init/shutdown/status 互斥
    std::atomic<bool> initialized{false};
    std::unique_ptr<tbb::task_arena> arena;
    std::unique_ptr<tbb::global_control> thread_control;
    RuntimeConfig config{};
    std::atomic<std::size_t> active_kernels{0};
    std::atomic<std::size_t> total_submitted{0};
    std::atomic<std::size_t> total_failed{0};
    std::atomic<std::size_t> total_cancelled{0};
    std::string log_level{"info"};
};

RuntimeState& runtime_state() {
    static RuntimeState inst;
    return inst;
}

std::uint32_t default_thread_count() noexcept {
    unsigned n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;
    return static_cast<std::uint32_t>(n);
}

} // anonymous namespace

// ============================================================================
// Runtime 控制 API
// ============================================================================

void runtime_init(const RuntimeConfig& config) {
    auto& s = runtime_state();
    std::lock_guard<std::mutex> lk(s.mtx);
    if (s.initialized.load(std::memory_order_acquire)) {
        // 幂等：首次配置生效，后续 init 调用忽略
        return;
    }
    const std::uint32_t max_threads =
        config.max_threads > 0 ? config.max_threads : default_thread_count();
    const std::uint32_t arena_conc =
        config.arena_concurrency > 0 ? config.arena_concurrency : max_threads;

    s.config = config;
    // global_control 限制 oneTBB 全局工作线程上限
    s.thread_control = std::make_unique<tbb::global_control>(
        tbb::global_control::max_allowed_parallelism,
        static_cast<std::size_t>(max_threads));
    // task_arena 提供独立执行上下文（arena_concurrency 个 slot）
    s.arena = std::make_unique<tbb::task_arena>(
        static_cast<int>(arena_conc));
    s.initialized.store(true, std::memory_order_release);
}

bool runtime_initialized() noexcept {
    return runtime_state().initialized.load(std::memory_order_acquire);
}

void runtime_shutdown() {
    auto& s = runtime_state();
    std::lock_guard<std::mutex> lk(s.mtx);
    if (!s.initialized.load(std::memory_order_acquire)) return;
    s.arena.reset();
    s.thread_control.reset();
    s.initialized.store(false, std::memory_order_release);
}

std::size_t runtime_worker_count() noexcept {
    auto& s = runtime_state();
    if (!s.initialized.load(std::memory_order_acquire)) {
        return static_cast<std::size_t>(default_thread_count());
    }
    return s.arena ? static_cast<std::size_t>(s.arena->max_concurrency()) : 0;
}

std::string runtime_status_json() {
    auto& s = runtime_state();
    std::lock_guard<std::mutex> lk(s.mtx);
    std::ostringstream oss;
    oss << "{";
    oss << "\"initialized\":" << (s.initialized.load(std::memory_order_acquire) ? "true" : "false");
    oss << ",\"max_threads\":" << s.config.max_threads;
    oss << ",\"arena_concurrency\":" << s.config.arena_concurrency;
    oss << ",\"enable_work_stealing\":" << (s.config.enable_work_stealing ? "true" : "false");
    oss << ",\"worker_count\":" << runtime_worker_count();
    oss << ",\"active_kernels\":" << s.active_kernels.load(std::memory_order_relaxed);
    oss << ",\"total_submitted\":" << s.total_submitted.load(std::memory_order_relaxed);
    oss << ",\"total_failed\":" << s.total_failed.load(std::memory_order_relaxed);
    oss << ",\"total_cancelled\":" << s.total_cancelled.load(std::memory_order_relaxed);
    oss << ",\"log_level\":\"" << s.log_level << "\"";
    oss << "}";
    return oss.str();
}

void runtime_set_log_level(const std::string& level) {
    auto& s = runtime_state();
    std::lock_guard<std::mutex> lk(s.mtx);
    s.log_level = level;
}

// ============================================================================
// detail: type-erased submit_* 实现
// ============================================================================

namespace detail {

namespace {

// 首次 API 调用时 lazy 初始化 runtime
void ensure_runtime_initialized() {
    if (runtime_state().initialized.load(std::memory_order_acquire)) return;
    runtime_init(RuntimeConfig{});
}

// RAII guard：计数 + 释放 user_data
// - 构造时 active_kernels++ / total_submitted++ / set_state(Running)
// - 析构时调用 release_fn（仅一次） + active_kernels--
// - release_early 用于 cancel 路径提前释放后再 return
struct KernelGuard {
    ReleaseFn rel;
    void* user_data;
    bool released{false};

    KernelGuard(EventImpl* ev, ReleaseFn r, void* ud) : rel(r), user_data(ud) {
        auto& s = runtime_state();
        s.active_kernels.fetch_add(1, std::memory_order_relaxed);
        s.total_submitted.fetch_add(1, std::memory_order_relaxed);
        ev->set_state(EventState::Running);
    }
    ~KernelGuard() {
        if (rel && !released) {
            try { rel(user_data); } catch (...) { /* swallow release exception */ }
        }
        runtime_state().active_kernels.fetch_sub(1, std::memory_order_relaxed);
    }
    void release_early() {
        if (rel && !released) {
            try { rel(user_data); } catch (...) {}
            released = true;
        }
    }
};

// 执行 kernel，捕获异常 → mark_failed
template<class Fn>
void run_kernel(const std::shared_ptr<EventImpl>& ev, Fn&& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        ev->mark_failed(StatusCode::KernelFailed, e.what());
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        ev->mark_failed(StatusCode::KernelFailed, "unknown kernel exception");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
    }
}

// kernel 正常完成且未被 cancel/fail 时 mark_done
void finalize_event(const std::shared_ptr<EventImpl>& ev) {
    if (ev->state.load(std::memory_order_acquire) == EventState::Running) {
        ev->mark_done();
    }
}

// 在 arena 内执行 parallel_for，根据 grainsize 选择 partitioner
template<class Body>
void arena_parallel_for(std::size_t begin, std::size_t end,
                        std::uint32_t grainsize, Body&& body) {
    auto& s = runtime_state();
    s.arena->execute([&] {
        if (grainsize > 0) {
            tbb::parallel_for(
                tbb::blocked_range<std::size_t>(begin, end, grainsize),
                std::forward<Body>(body),
                tbb::simple_partitioner{});
        } else {
            tbb::parallel_for(
                tbb::blocked_range<std::size_t>(begin, end),
                std::forward<Body>(body));
        }
    });
}

} // anonymous namespace

// ----- submit_range -----
Event submit_range(Range1D range, RangeKernelFn fn, void* user_data,
                   ReleaseFn rel, ExecutionHints hints) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    run_kernel(ev, [&] {
        if (range.empty()) return;
        arena_parallel_for(range.begin, range.end, hints.grainsize,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                fn(r.begin(), r.end(), user_data);
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_2d -----
Event submit_2d(Extent2D extent, TileKernelFn fn, void* user_data,
                ReleaseFn rel, ExecutionHints hints) {
    // 2D 等价于 1x1 tile
    TileShape t{1, 1};
    return submit_tiles(extent, t, fn, user_data, rel, hints);
}

// ----- submit_tiles -----
Event submit_tiles(Extent2D extent, TileShape tile, TileKernelFn fn,
                   void* user_data, ReleaseFn rel, ExecutionHints hints) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (tile.tile_w == 0 || tile.tile_h == 0) {
        ev->mark_failed(StatusCode::InvalidArgument, "submit_tiles: tile size zero");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        return Event(ev);
    }

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    const std::size_t tiles_x =
        (extent.width + tile.tile_w - 1) / tile.tile_w;
    const std::size_t tiles_y =
        (extent.height + tile.tile_h - 1) / tile.tile_h;
    const std::size_t total_tiles = tiles_x * tiles_y;

    run_kernel(ev, [&] {
        if (total_tiles == 0) return;
        arena_parallel_for(0, total_tiles, hints.grainsize,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                for (std::size_t idx = r.begin(); idx < r.end(); ++idx) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    const std::size_t ty = idx / tiles_x;
                    const std::size_t tx = idx % tiles_x;
                    const std::size_t tw = (std::min)(tile.tile_w, extent.width - tx * tile.tile_w);
                    const std::size_t th = (std::min)(tile.tile_h, extent.height - ty * tile.tile_h);
                    fn(tx, ty, tw, th, user_data);
                }
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_batch -----
Event submit_batch(std::size_t item_count, ItemKernelFn fn, void* user_data,
                   ReleaseFn rel, ExecutionHints hints) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    run_kernel(ev, [&] {
        if (item_count == 0) return;
        arena_parallel_for(0, item_count, hints.grainsize,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                for (std::size_t i = r.begin(); i < r.end(); ++i) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    fn(i, user_data);
                }
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_chunks -----
Event submit_chunks(Range1D range, std::size_t chunk_size, RangeKernelFn fn,
                    void* user_data, ReleaseFn rel, ExecutionHints hints) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (chunk_size == 0) {
        ev->mark_failed(StatusCode::InvalidArgument, "submit_chunks: chunk_size zero");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        return Event(ev);
    }

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    const std::size_t total = range.size();
    const std::size_t num_chunks = (total + chunk_size - 1) / chunk_size;

    run_kernel(ev, [&] {
        if (num_chunks == 0) return;
        // chunk 级并行：每个分块处理一个 chunk，grainsize=1 即一个 chunk 一个任务
        const std::uint32_t gs = hints.grainsize > 0 ? hints.grainsize : 1;
        arena_parallel_for(0, num_chunks, gs,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                for (std::size_t c = r.begin(); c < r.end(); ++c) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    const std::size_t b = range.begin + c * chunk_size;
                    const std::size_t e = (std::min)(b + chunk_size, range.end);
                    fn(b, e, user_data);
                }
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_serial -----
Event submit_serial(Range1D range, RangeKernelFn fn, void* user_data,
                    ReleaseFn rel, ExecutionHints /*hints*/) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    run_kernel(ev, [&] {
        for (std::size_t i = range.begin; i < range.end; ++i) {
            if (ev->cancelled.load(std::memory_order_relaxed)) return;
            fn(i, i + 1, user_data);
        }
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_reduce -----
// parallel_reduce：每个分块构造 identity 副本，reduce_fn 累加，combine_fn 合并
// 最终结果写入 result_out
void submit_reduce(Range1D range, const void* identity, std::size_t elem_size,
                   ReduceKernelFn reduce_fn, ReduceCombineFn combine_fn,
                   void* user_data, ReleaseFn rel, ExecutionHints hints,
                   void* result_out) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (identity == nullptr || result_out == nullptr || elem_size == 0) {
        ev->mark_failed(StatusCode::InvalidArgument,
                        "submit_reduce: invalid identity/result/elem_size");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 初始化 result = identity
    std::memcpy(result_out, identity, elem_size);

    if (range.empty()) {
        finalize_event(ev);
        return;
    }

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    run_kernel(ev, [&] {
        const std::size_t gs = hints.grainsize > 0 ? hints.grainsize : 64;
        auto& s = runtime_state();
        s.arena->execute([&] {
            // identity 值（oneTBB functional 形式：range, identity_value, reduce, combine）
            std::vector<unsigned char> identity_acc(elem_size);
            std::memcpy(identity_acc.data(), identity, elem_size);
            auto final_acc = tbb::parallel_reduce(
                tbb::blocked_range<std::size_t>(range.begin, range.end, gs),
                identity_acc,
                [&](const tbb::blocked_range<std::size_t>& r,
                    std::vector<unsigned char> acc) -> std::vector<unsigned char> {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return acc;
                    reduce_fn(r.begin(), r.end(), acc.data(), user_data);
                    return acc;
                },
                [&](std::vector<unsigned char> a,
                    const std::vector<unsigned char>& b) -> std::vector<unsigned char> {
                    combine_fn(a.data(), b.data(), user_data);
                    return a;
                });
            std::memcpy(result_out, final_acc.data(), elem_size);
        });
    });

    finalize_event(ev);
}

// ============================================================================
// Phase B4：submit_*_with_desc 实现（CostEstimator + Dispatcher 调用链）
// ============================================================================
// 设计：
// 1. 构造 TaskDescriptor
// 2. 从 global_cost_estimator 获取 CostEstimate
// 3. 无画像/无 GPU → 退化为 submit_*（CPU tbb 路径）
// 4. 有画像 + GPU → 用 Dispatcher::dispatch_range_cost_aware
// 5. 无论走哪条路径，release_fn 都由 KernelGuard 保证调用一次

// 全局 Dispatcher 单例（首次调用时初始化）
namespace {
scheduler::Dispatcher& global_dispatcher() {
    static scheduler::Dispatcher inst;
    static std::once_flag flag;
    std::call_once(flag, []() {
        scheduler::DispatcherConfig cfg;
        // 默认只有 CPU（legacy lambda API 为 CPU-only compatibility 路径）；
        // 可加速 KernelInvocation 路径通过 executor 注册表（create_auto 运行时
        // 探测 CUDA 桥接，无 GPU 时仅 CPU executor）。
        cfg.devices = {{"cpu", 0, 0, 50.0, true}};
        cfg.fallback_strategy = scheduler::FallbackStrategy::ToCpu;
        cfg.executors = std::make_shared<scheduler::ExecutorRegistry>(
            scheduler::ExecutorRegistry::create_auto());
        inst.configure(cfg);
    });
    return inst;
}

// RangeKernelFn → ChunkKernelFn 适配器
// RangeKernelFn: void(*)(std::size_t begin, std::size_t end, void* user_data)
// ChunkKernelFn: void(*)(std::size_t chunk_idx, std::size_t begin, std::size_t end, void* user_data)
// 适配器忽略 chunk_idx，直接调用 RangeKernelFn
struct RangeKernelAdapter {
    RangeKernelFn fn;
    void* user_data;
};
// 静态 thunk：从 user_data 取出 adapter，调用 fn(begin, end, original_user_data)
// 注意：adapter 本身作为 user_data 传给 Dispatcher
void range_chunk_thunk(std::size_t /*chunk_idx*/, std::size_t begin, std::size_t end, void* ud) {
    RangeKernelAdapter* adapter = static_cast<RangeKernelAdapter*>(ud);
    adapter->fn(begin, end, adapter->user_data);
}

// ===== Commit C 公共辅助：CostEstimator 调用 + CPU fallback 选择 =====
// 设计（20_PHASE_I_AUDIT_ACTION_PLAN.md §3 Commit C）：
// 1. 所有 submit_*_with_desc 必须接通 CostEstimator，不得忽略 OperationId/traits
// 2. CostEstimator 异常不阻断执行，退化为默认 CPU 路径（grainsize=0）
// 3. 无 GPU 或 estimate.profile_available=false 时明确走 CPU tbb 路径
// 4. 有 GPU + 画像可用时通过 Dispatcher 派发（仅 range 路径，tiles/batch/reduce 后续接入）
// 5. recommended_chunk 作为 CPU grainsize 提示（0 表示用 tbb 默认）

// 安全调用 CostEstimator：异常时返回空 estimate（fallback）
cost::CostEstimate estimate_cost_safely(const TaskDescriptor& task) {
    cost::CostEstimate estimate;
    try {
        estimate = cost::global_cost_estimator().estimate(task);
    } catch (...) {
        // CostEstimator 异常不阻断执行，退化为默认 CPU 路径
    }
    return estimate;
}

// 从 CostEstimate 提取 CPU 推荐块作为 grainsize
// 无 CPU 设备或 recommended_chunk=0 时返回 0（tbb 默认 grainsize）
std::uint32_t pick_cpu_grainsize(const cost::CostEstimate& estimate) noexcept {
    for (const auto& dc : estimate.per_device) {
        if (dc.device_id == kCpuDeviceId && dc.recommended_chunk > 0) {
            return static_cast<std::uint32_t>(
                std::min(dc.recommended_chunk, static_cast<std::size_t>(UINT32_MAX)));
        }
    }
    return 0;
}

// 检查 Dispatcher 是否有可用的 GPU backend
bool has_gpu_backend() {
    for (const auto& b : global_dispatcher().current_state().backends()) {
        if (b.rfind("cuda", 0) == 0) return true;
    }
    return false;
}
} // anonymous namespace

// ----- submit_range_with_desc -----
Event submit_range_with_desc(OperationId id, Range1D range, TaskTraits traits,
                              RangeKernelFn fn, void* user_data, ReleaseFn rel) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    // 1. 构造 TaskDescriptor
    TaskDescriptor task = make_range_descriptor(id, range, traits, Precision::Default);

    // 2. 通过 CostEstimator 估算成本（接通调用链，不得忽略 OperationId/traits）
    // CostEstimate 提供 preferred_device + recommended_chunk_size
    cost::CostEstimate estimate = estimate_cost_safely(task);

    // 3. 无 GPU 或画像不可用时明确走 CPU tbb 路径（CPU fallback）
    // CostEstimate.recommended_chunk 用于 grainsize 优化
    if (!has_gpu_backend() || !estimate.profile_available) {
        // CPU 路径：直接用 tbb parallel_for（与旧 API 一致）
        // 用 CostEstimate 的 recommended_chunk 作为 grainsize 提示
        std::uint32_t grainsize = pick_cpu_grainsize(estimate);
        run_kernel(ev, [&] {
            if (range.empty()) return;
            arena_parallel_for(range.begin, range.end, grainsize,
                [&](const tbb::blocked_range<std::size_t>& r) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    fn(r.begin(), r.end(), user_data);
                });
        });
    } else {
        // GPU 可用路径：通过 Dispatcher 分发
        // 适配 RangeKernelFn → ChunkKernelFn
        RangeKernelAdapter adapter{fn, user_data};
        auto result = global_dispatcher().dispatch_range_cost_aware(
            task, estimate, range_chunk_thunk, &adapter);
        if (!result.run_result.all_done && result.run_result.failed_chunks > 0) {
            ev->mark_failed(StatusCode::KernelFailed,
                            "dispatch_range_cost_aware: " + result.run_result.error_message);
            runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        }
    }

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_tiles_with_desc -----
Event submit_tiles_with_desc(OperationId id, Extent2D extent, TileShape tile,
                              TaskTraits traits, TileKernelFn fn,
                              void* user_data, ReleaseFn rel) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (tile.tile_w == 0 || tile.tile_h == 0) {
        ev->mark_failed(StatusCode::InvalidArgument, "submit_tiles_with_desc: tile size zero");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        return Event(ev);
    }

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    // 构造 TaskDescriptor 并接通 CostEstimator（Commit C：不得忽略 OperationId/traits）
    TaskDescriptor task = make_tiles_descriptor(id, extent, tile, traits, Precision::Default);
    cost::CostEstimate estimate = estimate_cost_safely(task);

    // 2D tiles 当前只走 CPU tbb 路径（GPU tiles 派发由 Phase F3+ 接入，需 TileKernelFn → ChunkKernelFn 适配）
    // 用 CostEstimate.recommended_chunk 作为 grainsize 提示（按 tile 数计）
    // 明确 CPU fallback：无 GPU 或画像不可用时 grainsize=0 走 tbb 默认
    (void)has_gpu_backend();  // tiles 的 GPU 派发尚未接入，此处仅诊断
    std::uint32_t grainsize = pick_cpu_grainsize(estimate);
    // tiles 任务按 tile 数分块，grainsize 表示每分块处理的 tile 数
    // recommended_chunk 是按元素数计，转换为 tile 数（向下取整，至少 1）
    if (grainsize > 0 && tile.tile_w > 0 && tile.tile_h > 0) {
        std::size_t elems_per_tile = static_cast<std::size_t>(tile.tile_w) * tile.tile_h;
        if (elems_per_tile > 0) {
            std::size_t tiles_per_chunk = estimate.per_device.empty()
                ? 0 : (estimate.per_device.front().recommended_chunk / elems_per_tile);
            grainsize = tiles_per_chunk > 0
                ? static_cast<std::uint32_t>(std::min(tiles_per_chunk,
                                                     static_cast<std::size_t>(UINT32_MAX)))
                : 1u;
        }
    }

    const std::size_t tiles_x = (extent.width + tile.tile_w - 1) / tile.tile_w;
    const std::size_t tiles_y = (extent.height + tile.tile_h - 1) / tile.tile_h;
    const std::size_t total_tiles = tiles_x * tiles_y;

    run_kernel(ev, [&] {
        if (total_tiles == 0) return;
        arena_parallel_for(0, total_tiles, grainsize,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                for (std::size_t idx = r.begin(); idx < r.end(); ++idx) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    const std::size_t ty = idx / tiles_x;
                    const std::size_t tx = idx % tiles_x;
                    const std::size_t tw = (std::min)(tile.tile_w, extent.width - tx * tile.tile_w);
                    const std::size_t th = (std::min)(tile.tile_h, extent.height - ty * tile.tile_h);
                    fn(tx, ty, tw, th, user_data);
                }
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_batch_with_desc -----
Event submit_batch_with_desc(OperationId id, std::size_t item_count, TaskTraits traits,
                              ItemKernelFn fn, void* user_data, ReleaseFn rel) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        guard.release_early();
        return Event(ev);
    }

    // 构造 TaskDescriptor 并接通 CostEstimator（Commit C：不得忽略 OperationId/traits）
    TaskDescriptor task = make_batch_descriptor(id, item_count, traits, Precision::Default);
    cost::CostEstimate estimate = estimate_cost_safely(task);

    // batch 当前走 CPU tbb 路径（GPU batch 派发由 Phase F3+ 接入）
    // 用 CostEstimate.recommended_chunk 作为 grainsize 提示（按 item 数计）
    // 明确 CPU fallback：无 GPU 或画像不可用时 grainsize=0 走 tbb 默认
    (void)has_gpu_backend();
    std::uint32_t grainsize = pick_cpu_grainsize(estimate);

    run_kernel(ev, [&] {
        if (item_count == 0) return;
        arena_parallel_for(0, item_count, grainsize,
            [&](const tbb::blocked_range<std::size_t>& r) {
                if (ev->cancelled.load(std::memory_order_relaxed)) return;
                for (std::size_t i = r.begin(); i < r.end(); ++i) {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return;
                    fn(i, user_data);
                }
            });
    });

    finalize_event(ev);
    return Event(ev);
}

// ----- submit_reduce_with_desc -----
void submit_reduce_with_desc(OperationId id, Range1D range, TaskTraits traits,
                              const void* identity, std::size_t elem_size,
                              ReduceKernelFn reduce_fn, ReduceCombineFn combine_fn,
                              void* user_data, ReleaseFn rel, void* result_out) {
    ensure_runtime_initialized();
    auto ev = std::make_shared<EventImpl>();
    KernelGuard guard(ev.get(), rel, user_data);

    if (identity == nullptr || result_out == nullptr || elem_size == 0) {
        ev->mark_failed(StatusCode::InvalidArgument,
                        "submit_reduce_with_desc: invalid identity/result/elem_size");
        runtime_state().total_failed.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    std::memcpy(result_out, identity, elem_size);

    if (range.empty()) {
        finalize_event(ev);
        return;
    }

    if (ev->cancelled.load(std::memory_order_relaxed)) {
        ev->mark_cancelled();
        runtime_state().total_cancelled.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 构造 TaskDescriptor 并接通 CostEstimator（Commit C：不得忽略 OperationId/traits）
    TaskDescriptor task = make_reduce_descriptor(id, range, traits, Precision::Default);
    cost::CostEstimate estimate = estimate_cost_safely(task);

    // 归约当前走 CPU tbb parallel_reduce 路径（GPU 归约需专门 reduction kernel，Phase F3+ 接入）
    // 用 CostEstimate.recommended_chunk 作为 grainsize 提示（按元素数计）
    // 明确 CPU fallback：无 GPU 或画像不可用时用默认 grainsize=64
    (void)has_gpu_backend();
    std::uint32_t grainsize = pick_cpu_grainsize(estimate);
    const std::size_t gs = grainsize > 0 ? static_cast<std::size_t>(grainsize) : 64;

    run_kernel(ev, [&] {
        auto& s = runtime_state();
        s.arena->execute([&] {
            std::vector<unsigned char> identity_acc(elem_size);
            std::memcpy(identity_acc.data(), identity, elem_size);
            auto final_acc = tbb::parallel_reduce(
                tbb::blocked_range<std::size_t>(range.begin, range.end, gs),
                identity_acc,
                [&](const tbb::blocked_range<std::size_t>& r,
                    std::vector<unsigned char> acc) -> std::vector<unsigned char> {
                    if (ev->cancelled.load(std::memory_order_relaxed)) return acc;
                    reduce_fn(r.begin(), r.end(), acc.data(), user_data);
                    return acc;
                },
                [&](std::vector<unsigned char> a,
                    const std::vector<unsigned char>& b) -> std::vector<unsigned char> {
                    combine_fn(a.data(), b.data(), user_data);
                    return a;
                });
            std::memcpy(result_out, final_acc.data(), elem_size);
        });
    });

    finalize_event(ev);
}

} // namespace detail

} // namespace astro::compute
