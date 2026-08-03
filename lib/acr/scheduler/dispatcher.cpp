// lib/acr/scheduler/dispatcher.cpp — Dispatcher 实现
// Phase F3：增强 cost-aware 工作保持调度。
// Commit F：接入 CpuController（95% 软目标）+ MemoryBudgetController（RAM/VRAM 预算）
//           + guided 尾部收缩（completion > 70% 时 chunk_size 动态缩小）
#include "dispatcher.hpp"

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../utilization/cpu_controller.hpp"
#include "../utilization/memory_budget.hpp"

#include <algorithm>
#include <vector>

namespace astro::compute::scheduler {

struct Dispatcher::Impl {
    DispatcherConfig cfg;
    MixedRunner runner;
    QueueAwareEstimator estimator;
    FallbackPolicy fallback_policy;
    CurrentState current_state;
    // Commit F：utilization + memory budget
    std::unique_ptr<utilization::CpuController> cpu_ctrl;
    std::unique_ptr<utilization::MemoryBudgetController> mem_ctrl;

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
        return cost::device_id_to_backend(estimate.preferred_device);
    }

    // Commit F：将 ExceedAction 转为字符串
    static std::string action_to_string(utilization::MemoryBudgetController::ExceedAction a) {
        switch (a) {
            case utilization::MemoryBudgetController::ExceedAction::None: return "none";
            case utilization::MemoryBudgetController::ExceedAction::StopNewSubmit: return "stop";
            case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock: return "shrink";
            case utilization::MemoryBudgetController::ExceedAction::ReleaseCache: return "release_cache";
            case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath: return "low_memory";
            case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice: return "fallback";
            case utilization::MemoryBudgetController::ExceedAction::Fail: return "fail";
            default: return "unknown";
        }
    }
};

Dispatcher::Dispatcher() : impl_(std::make_unique<Impl>()) {
    // Commit F：初始化 utilization 控制器
    impl_->cpu_ctrl = std::make_unique<utilization::CpuController>();
    impl_->mem_ctrl = std::make_unique<utilization::MemoryBudgetController>();
}
Dispatcher::~Dispatcher() = default;

void Dispatcher::configure(const DispatcherConfig& cfg) {
    impl_->cfg = cfg;
    impl_->fallback_policy.set_strategy(cfg.fallback_strategy);
    MixedRunnerConfig mcfg;
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

    // Commit F：配置 utilization 控制器
    if (impl_->cfg.enable_utilization && impl_->cpu_ctrl) {
        impl_->cpu_ctrl->set_target(cfg.cpu_target_ratio);
    }
    if (impl_->mem_ctrl) {
        utilization::MemoryBudgetConfig mbcfg;
        impl_->mem_ctrl->configure(mbcfg);
        // 注册 GPU backends 用于 VRAM 监控
        for (const auto& bn : mcfg.gpu_backends) {
            impl_->mem_ctrl->register_backend(bn);
        }
    }
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

    // Commit F：执行前采样 CPU 利用率（建立基线）
    utilization::CpuControlDecision cpu_dec;
    if (impl_->cfg.enable_utilization && impl_->cpu_ctrl) {
        cpu_dec = impl_->cpu_ctrl->sample_and_decide();
    }

    // Commit F：执行前检查内存预算
    utilization::MemoryBudgetController::ExceedAction mem_action =
        utilization::MemoryBudgetController::ExceedAction::None;
    if (impl_->mem_ctrl) {
        auto mem_budget = impl_->mem_ctrl->sample();
        mem_action = utilization::MemoryBudgetController::suggest_action(
            mem_budget.used_ram, mem_budget.limit_ram, mem_budget.total_ram);
        result.mem_action = Impl::action_to_string(mem_action);
    }

    // 内存预算失败：直接返回
    if (mem_action == utilization::MemoryBudgetController::ExceedAction::Fail) {
        result.run_result.all_done = false;
        result.run_result.error_message = "memory budget exceeded (Fail)";
        result.actual_primary_backend = "none";
        return result;
    }
    // 内存预算要求缩小块
    if (mem_action == utilization::MemoryBudgetController::ExceedAction::ShrinkBlock) {
        chunk_size = std::max(chunk_size / 2, impl_->cfg.min_effective_chunk);
    }

    // 无 GPU 可用时退化为纯 CPU dispatch_range
    bool has_gpu = false;
    for (const auto& d : impl_->cfg.devices) {
        if (d.backend.rfind("cuda", 0) == 0 && d.available) { has_gpu = true; break; }
    }
    if (!has_gpu || !estimate.profile_available) {
        // 纯 CPU 路径
        result.actual_primary_backend = "cpu";

        // Commit F：guided 尾部收缩分段执行
        const bool can_split =
            impl_->cfg.enable_guided_tail &&
            impl_->cfg.enable_utilization &&
            (end > begin) &&
            (end - begin) > impl_->cfg.min_effective_chunk * 4;

        if (can_split) {
            // 分段：第一阶段正常 chunk_size 执行 [begin, split_point)
            std::size_t split_point = begin + static_cast<std::size_t>(
                (end - begin) * impl_->cfg.guided_tail_threshold);

            auto r1 = impl_->runner.run_range(begin, split_point, chunk_size, fn, user_data);

            // 中间采样 CPU 利用率
            if (impl_->cpu_ctrl) {
                cpu_dec = impl_->cpu_ctrl->sample_and_decide();
            }
            // 中间检查内存预算
            if (impl_->mem_ctrl) {
                auto mem_budget = impl_->mem_ctrl->sample();
                mem_action = utilization::MemoryBudgetController::suggest_action(
                    mem_budget.used_ram, mem_budget.limit_ram, mem_budget.total_ram);
            }

            // 第二阶段：guided 收缩 chunk_size
            std::size_t guided_chunk = std::max(chunk_size / 2, impl_->cfg.min_effective_chunk);
            // 内存紧张：进一步收缩
            if (mem_action == utilization::MemoryBudgetController::ExceedAction::ShrinkBlock) {
                guided_chunk = std::max(guided_chunk / 2, impl_->cfg.min_effective_chunk);
            }
            // CPU 利用率超目标 +5%：收缩以让步
            if (cpu_dec.valid && cpu_dec.actual_ratio > impl_->cfg.cpu_target_ratio + 0.05) {
                guided_chunk = std::max(guided_chunk / 2, impl_->cfg.min_effective_chunk);
            }

            auto r2 = impl_->runner.run_range(split_point, end, guided_chunk, fn, user_data);

            // 合并结果
            result.run_result.total_chunks = r1.total_chunks + r2.total_chunks;
            result.run_result.executed_on_cpu = r1.executed_on_cpu + r2.executed_on_cpu;
            result.run_result.executed_on_gpu = r1.executed_on_gpu + r2.executed_on_gpu;
            result.run_result.failed_chunks = r1.failed_chunks + r2.failed_chunks;
            result.run_result.fallback_chunks = r1.fallback_chunks + r2.fallback_chunks;
            result.run_result.all_done = r1.all_done && r2.all_done;
            result.run_result.error_message = r2.error_message.empty() ? r1.error_message : r2.error_message;

            result.guided_tail_used = true;
            result.guided_min_chunk = guided_chunk;
        } else {
            auto r = impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
            result.run_result = r;
        }

        result.total_chunks = result.run_result.total_chunks;
        result.chunks_on_cpu = result.run_result.executed_on_cpu;
        result.chunks_on_gpu = result.run_result.executed_on_gpu;
        result.chunks_fallback = result.run_result.fallback_chunks;
        impl_->current_state.init_coverage(result.total_chunks);
        for (std::size_t i = 0; i < result.total_chunks; ++i) {
            impl_->current_state.coverage().mark_done(i);
        }

        // Commit F：执行后采样 CPU 利用率
        if (impl_->cpu_ctrl) {
            cpu_dec = impl_->cpu_ctrl->sample_and_decide();
            result.cpu_actual_ratio = cpu_dec.actual_ratio;
            result.cpu_actual_valid = cpu_dec.valid;
        }

        result.current_state_json = impl_->current_state.status_json();
        return result;
    }

    // Cost-aware 路径：当前实现仍走 MixedRunner（GPU 真实执行由 backend 提供）
    // CostEstimator 已决定 chunk_size 和 preferred_device，MixedRunner 据此分发
    auto r = impl_->runner.run_range(begin, end, chunk_size, fn, user_data);
    result.run_result = r;
    result.actual_primary_backend = impl_->pick_backend_from_estimate(estimate);
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

    // Commit F：执行后采样 CPU 利用率
    if (impl_->cpu_ctrl) {
        cpu_dec = impl_->cpu_ctrl->sample_and_decide();
        result.cpu_actual_ratio = cpu_dec.actual_ratio;
        result.cpu_actual_valid = cpu_dec.valid;
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
