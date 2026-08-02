// lib/acr/scheduler/dispatcher.cpp — Dispatcher 实现
// Phase F3：增强 cost-aware 工作保持调度。
#include "dispatcher.hpp"

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

#include <algorithm>
#include <vector>

namespace astro::compute::scheduler {

struct Dispatcher::Impl {
    DispatcherConfig cfg;
    MixedRunner runner;
    QueueAwareEstimator estimator;
    FallbackPolicy fallback_policy;
    CurrentState current_state;

    std::string pick_backend_impl(const TaskEstimate& task) const {
        // 小数据优先 CPU
        if (task.bytes_per_chunk * task.chunk_count < cfg.small_data_threshold_bytes) {
            // 检查 CPU 是否可用
            for (const auto& d : cfg.devices) {
                if (d.backend == "cpu" && d.available) return "cpu";
            }
        }
        // 工作保持：选 finish 最短的可用设备
        std::string best = estimator.pick_best_device(cfg.devices, task);
        if (best.empty()) {
            // 没有可用设备，回退 CPU（即使 unavailable 也走 CPU）
            return "cpu";
        }
        return best;
    }

    // Phase F3：从 CostEstimate 提取推荐块大小和 backend
    std::size_t pick_chunk_size_from_estimate(const cost::CostEstimate& estimate) const {
        // 优先用 preferred_device 的 recommended_chunk
        for (const auto& dc : estimate.per_device) {
            if (dc.device_id == estimate.preferred_device && dc.recommended_chunk > 0) {
                return dc.recommended_chunk;
            }
        }
        // 兜底：取第一个可行设备的推荐块
        for (const auto& dc : estimate.per_device) {
            if (dc.feasible && dc.recommended_chunk > 0) return dc.recommended_chunk;
        }
        return 65536;  // 默认
    }

    std::string pick_backend_from_estimate(const cost::CostEstimate& estimate) const {
        if (!estimate.preferred_backend.empty()) return estimate.preferred_backend;
        return "cpu";
    }
};

Dispatcher::Dispatcher() : impl_(std::make_unique<Impl>()) {}
Dispatcher::~Dispatcher() = default;

void Dispatcher::configure(const DispatcherConfig& cfg) {
    impl_->cfg = cfg;
    impl_->fallback_policy.set_strategy(cfg.fallback_strategy);
    MixedRunnerConfig mcfg;
    mcfg.preferred_backend = cfg.preferred_backend;
    mcfg.fallback_strategy = cfg.fallback_strategy;
    // 从 devices 提取 GPU backends
    std::vector<std::string> backend_names;
    for (const auto& d : cfg.devices) {
        backend_names.push_back(d.backend);
        if (d.backend.rfind("cuda", 0) == 0) {
            mcfg.gpu_backends.push_back(d.backend);
            mcfg.enable_gpu = true;
        }
    }
    impl_->runner.configure(mcfg);
    impl_->current_state.init_devices(backend_names);
}

MixedRunResult Dispatcher::dispatch_range(std::size_t begin, std::size_t end,
                                          std::size_t chunk_size,
                                          ChunkKernelFn fn, void* user_data) {
    return impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
}

MixedRunResult Dispatcher::dispatch_chunks(const std::vector<RangeChunk>& chunks,
                                           ChunkKernelFn fn, void* user_data) {
    return impl_->runner.run_chunks(chunks, fn, user_data);
}

std::string Dispatcher::pick_backend(const TaskEstimate& task) const {
    return impl_->pick_backend_impl(task);
}

FallbackDecision Dispatcher::handle_failure(const std::string& failed_backend,
                                            const CoverageBitmap& bitmap) const {
    std::vector<std::string> available;
    for (const auto& d : impl_->cfg.devices) {
        if (d.available && d.backend != failed_backend) {
            available.push_back(d.backend);
        }
    }
    return impl_->fallback_policy.decide(failed_backend, bitmap, available);
}

// ===== Phase F3：Cost-aware 工作保持调度 =====
CostAwareResult Dispatcher::dispatch_range_cost_aware(
    const TaskDescriptor& task,
    const cost::CostEstimate& estimate,
    ChunkKernelFn fn, void* user_data) {
    CostAwareResult result;
    result.preferred_backend = impl_->pick_backend_from_estimate(estimate);
    result.used_cost_estimator = estimate.profile_available;

    // 决定 chunk_size
    std::size_t chunk_size = impl_->pick_chunk_size_from_estimate(estimate);
    if (chunk_size == 0) chunk_size = 65536;

    // 工作域
    std::size_t begin = 0, end = 0;
    if (task.range.size() > 0) {
        begin = task.range.begin;
        end = task.range.end;
    } else if (task.extent.count() > 0) {
        // 2D 任务线性化为 1D（按 tile 索引）
        begin = 0;
        end = task.extent.count();
    } else {
        begin = 0;
        end = task.item_count;
    }

    // 无 GPU 可用时退化为纯 CPU dispatch_range
    bool has_gpu = false;
    for (const auto& d : impl_->cfg.devices) {
        if (d.backend.rfind("cuda", 0) == 0 && d.available) { has_gpu = true; break; }
    }
    if (!has_gpu || !estimate.profile_available) {
        // 纯 CPU 路径
        result.actual_primary_backend = "cpu";
        auto r = impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
        result.run_result = r;
        result.total_chunks = r.total_chunks;
        result.chunks_on_cpu = r.executed_on_cpu;
        result.chunks_on_gpu = r.executed_on_gpu;
        result.chunks_fallback = r.fallback_chunks;
        impl_->current_state.init_coverage(r.total_chunks);
        for (std::size_t i = 0; i < r.total_chunks; ++i) {
            impl_->current_state.coverage().mark_done(i);
        }
        result.current_state_json = impl_->current_state.status_json();
        return result;
    }

    // Cost-aware 路径：当前实现仍走 MixedRunner（GPU 真实执行由 backend 提供）
    // CostEstimator 已决定 chunk_size 和 preferred_backend，MixedRunner 据此分发
    auto r = impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
    result.run_result = r;
    result.actual_primary_backend = result.preferred_backend;
    result.total_chunks = r.total_chunks;
    result.chunks_on_cpu = r.executed_on_cpu;
    result.chunks_on_gpu = r.executed_on_gpu;
    result.chunks_fallback = r.fallback_chunks;

    // 更新 CurrentState
    impl_->current_state.init_coverage(r.total_chunks);
    for (std::size_t i = 0; i < r.total_chunks; ++i) {
        impl_->current_state.coverage().mark_done(i);
    }
    if (auto* cpu_state = impl_->current_state.find_device("cpu")) {
        cpu_state->chunks_completed.store(r.executed_on_cpu, std::memory_order_relaxed);
    }
    for (const auto& d : impl_->cfg.devices) {
        if (d.backend.rfind("cuda", 0) == 0) {
            if (auto* gpu_state = impl_->current_state.find_device(d.backend)) {
                gpu_state->chunks_completed.store(r.executed_on_gpu, std::memory_order_relaxed);
            }
        }
    }
    result.current_state_json = impl_->current_state.status_json();
    return result;
}

const CurrentState& Dispatcher::current_state() const noexcept {
    return impl_->current_state;
}

CurrentState& Dispatcher::current_state() noexcept {
    return impl_->current_state;
}

const MixedRunner& Dispatcher::runner() const noexcept { return impl_->runner; }
const QueueAwareEstimator& Dispatcher::estimator() const noexcept { return impl_->estimator; }
const FallbackPolicy& Dispatcher::fallback_policy() const noexcept { return impl_->fallback_policy; }

} // namespace astro::compute::scheduler
