// lib/acr/scheduler/dispatcher.cpp — Dispatcher 实现
// Phase F3 + F-fix 1：cost-aware 调度 + 真实执行报告
//
// F-fix 1 修正：
//   - actual_primary_backend 由真实完成统计生成，不从预测值填写
//   - predicted_primary_backend 单独保留
//   - coverage 从 MixedRunner.last_coverage() 真实导入，不无条件 mark_done
//   - 固定尾段实验改名为 fixed_tail_chunking（不冒充动态 guided）
#include "dispatcher.hpp"
#include "shared_work_pool.hpp"

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../utilization/cpu_controller.hpp"
#include "../utilization/memory_budget.hpp"

#include "astro/compute/acr.hpp"

#include <algorithm>
#include <atomic>
#include <vector>

namespace astro::compute::scheduler {

struct Dispatcher::Impl {
    DispatcherConfig cfg;
    MixedRunner runner;
    QueueAwareEstimator estimator;
    FallbackPolicy fallback_policy;
    CurrentState current_state;
    // F-fix 4：utilization + memory budget
    std::unique_ptr<utilization::CpuController> cpu_ctrl;
    std::unique_ptr<utilization::MemoryBudgetController> mem_ctrl;
    // F-fix 2：SharedWorkPool
    SharedWorkPool pool;

    std::string pick_backend_impl(const TaskEstimate& task) const {
        // 小数据优先 CPU
        if (task.bytes_per_chunk * task.chunk_count < cfg.small_data_threshold_bytes) {
            for (const auto& d : cfg.devices) {
                if (d.backend == "cpu" && d.available) return "cpu";
            }
        }
        std::string best = estimator.pick_best_device(cfg.devices, task);
        if (best.empty()) return "cpu";
        return best;
    }

    std::size_t pick_chunk_size_from_estimate(const cost::CostEstimate& estimate) const {
        for (const auto& dc : estimate.per_device) {
            if (dc.device_id == estimate.preferred_device && dc.recommended_chunk > 0) {
                return dc.recommended_chunk;
            }
        }
        for (const auto& dc : estimate.per_device) {
            if (dc.feasible && dc.recommended_chunk > 0) return dc.recommended_chunk;
        }
        return 65536;
    }

    // F-fix 1：预测设备由 CostEstimate 推算
    std::string predict_backend_from_estimate(const cost::CostEstimate& estimate) const {
        return cost::device_id_to_backend(estimate.preferred_device);
    }

    // F-fix 1：实际主力 backend 由真实完成统计生成
    static std::string actual_backend_from_stats(std::size_t on_cpu,
                                                  std::size_t on_gpu,
                                                  std::size_t fallback) {
        // 实际执行主力 = 完成块数最多的设备
        if (on_cpu >= on_gpu && on_cpu > 0) return "cpu";
        if (on_gpu > on_cpu && on_gpu > 0) return "cuda:0";
        // fallback 块也算 CPU 执行
        if (fallback > 0) return "cpu";
        return "none";
    }

    // F-fix 1：实际使用的设备列表
    static std::vector<std::string> actual_devices_from_stats(std::size_t on_cpu,
                                                               std::size_t on_gpu) {
        std::vector<std::string> devices;
        if (on_cpu > 0) devices.push_back("cpu");
        if (on_gpu > 0) devices.push_back("cuda:0");
        return devices;
    }

    // F-fix 1：从 MixedRunner 的真实 coverage 导入 CurrentState
    void import_real_coverage(const MixedRunResult& r) {
        const auto& real_bm = runner.last_coverage();
        current_state.init_coverage(r.total_chunks);
        // 只标记真正完成的 chunk（失败/未开始不标 DONE）
        for (std::size_t i = 0; i < r.total_chunks && i < real_bm.chunk_count(); ++i) {
            if (real_bm.is_done(i)) {
                current_state.coverage().mark_done(i);
            }
        }
    }

    // F-fix 1：从 MixedRunResult 生成 CoverageStats
    static CoverageStats coverage_from_result(const MixedRunResult& r) {
        CoverageStats cs;
        cs.total = r.total_chunks;
        cs.done = r.executed_on_cpu + r.executed_on_gpu;
        cs.failed = r.failed_chunks;
        cs.pending = (r.total_chunks > cs.done + cs.failed) ?
                     (r.total_chunks - cs.done - cs.failed) : 0;
        cs.claimed = cs.done + cs.failed;  // 已领取 = 已完成 + 已失败
        return cs;
    }

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

    // F-fix 2：通过 SharedWorkPool 执行（CostEstimator 驱动）
    // 从 pool claim_next 领取块，执行并标记结果。
    // 失败块可回收（reclaim_failed）。
    MixedRunResult execute_via_pool(std::size_t begin, std::size_t end,
                                    std::size_t chunk_size,
                                    ChunkKernelFn fn, void* user_data) {
        pool.init(begin, end, chunk_size);
        MixedRunResult r;
        r.total_chunks = pool.total_blocks();

        if (r.total_chunks == 0) {
            r.all_done = true;
            return r;
        }

        // 使用 parallel_batch 从 pool claim 并执行
        // 每个 task 尝试 claim_next 一个块，成功则执行
        std::atomic<std::size_t> executed{0}, failed{0};
        Event ev = astro::compute::parallel_batch(
            astro::compute::KernelId::Custom, r.total_chunks,
            [&](std::size_t) {
                auto* block = pool.claim_next("cpu");
                if (!block) return;
                try {
                    fn(block->id, block->begin, block->end, user_data);
                    pool.mark_done(block->id);
                    executed.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    pool.mark_failed(block->id);
                    failed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        (void)ev;

        r.executed_on_cpu = executed.load();
        r.failed_chunks = failed.load();
        r.all_done = pool.all_done();
        return r;
    }

    // F-fix 1：从 SharedWorkPool 的真实统计生成 CoverageStats
    static CoverageStats coverage_from_pool(const SharedWorkPool& p) {
        CoverageStats cs;
        cs.total = p.total_blocks();
        cs.done = p.done_count();
        cs.failed = p.failed_count();
        cs.claimed = cs.done + cs.failed;
        cs.pending = (cs.total > cs.claimed) ? (cs.total - cs.claimed) : 0;
        return cs;
    }

    // F-fix 1：从 SharedWorkPool 导入 coverage 到 CurrentState
    void import_pool_coverage() {
        const auto& blocks = pool.blocks();
        current_state.init_coverage(blocks.size());
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].status.load(std::memory_order_relaxed) == WorkBlockStatus::Done) {
                current_state.coverage().mark_done(i);
            }
        }
    }
};

Dispatcher::Dispatcher() : impl_(std::make_unique<Impl>()) {
    impl_->cpu_ctrl = std::make_unique<utilization::CpuController>();
    impl_->mem_ctrl = std::make_unique<utilization::MemoryBudgetController>();
}
Dispatcher::~Dispatcher() = default;

void Dispatcher::configure(const DispatcherConfig& cfg) {
    impl_->cfg = cfg;
    impl_->fallback_policy.set_strategy(cfg.fallback_strategy);
    MixedRunnerConfig mcfg;
    mcfg.fallback_strategy = cfg.fallback_strategy;
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

    if (impl_->cfg.enable_utilization && impl_->cpu_ctrl) {
        impl_->cpu_ctrl->set_target(cfg.cpu_target_ratio);
    }
    if (impl_->mem_ctrl) {
        utilization::MemoryBudgetConfig mbcfg;
        impl_->mem_ctrl->configure(mbcfg);
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

// ===== Phase F3 + F-fix 1：Cost-aware 调度 =====
CostAwareResult Dispatcher::dispatch_range_cost_aware(
    const TaskDescriptor& task,
    const cost::CostEstimate& estimate,
    ChunkKernelFn fn, void* user_data) {
    CostAwareResult result;
    result.used_cost_estimator = estimate.profile_available;

    // F-fix 1：预测设备由 CostEstimate 推算（单独保留）
    result.predicted_primary_backend = impl_->predict_backend_from_estimate(estimate);

    // 决定 chunk_size
    std::size_t chunk_size = impl_->pick_chunk_size_from_estimate(estimate);
    if (chunk_size == 0) chunk_size = 65536;

    // 工作域
    std::size_t begin = 0, end = 0;
    if (task.range.size() > 0) {
        begin = task.range.begin;
        end = task.range.end;
    } else if (task.extent.count() > 0) {
        begin = 0;
        end = task.extent.count();
    } else {
        begin = 0;
        end = task.item_count;
    }

    // F-fix 4：执行前采样 CPU 利用率（建立基线）
    utilization::CpuControlDecision cpu_dec;
    if (impl_->cfg.enable_utilization && impl_->cpu_ctrl) {
        cpu_dec = impl_->cpu_ctrl->sample_and_decide();
    }

    // F-fix 4：执行前检查内存预算
    utilization::MemoryBudgetController::ExceedAction mem_action =
        utilization::MemoryBudgetController::ExceedAction::None;
    if (impl_->mem_ctrl) {
        auto mem_budget = impl_->mem_ctrl->sample();
        mem_action = utilization::MemoryBudgetController::suggest_action(
            mem_budget.used_ram, mem_budget.limit_ram, mem_budget.total_ram);
        result.mem_action = Impl::action_to_string(mem_action);
    } else {
        result.mem_action = "none";
    }

    // 内存预算失败：直接返回
    if (mem_action == utilization::MemoryBudgetController::ExceedAction::Fail) {
        result.run_result.all_done = false;
        result.run_result.error_message = "memory budget exceeded (Fail)";
        result.actual_primary_backend = "none";
        result.coverage = Impl::coverage_from_result(result.run_result);
        return result;
    }
    // 内存预算要求缩小块
    if (mem_action == utilization::MemoryBudgetController::ExceedAction::ShrinkBlock) {
        chunk_size = std::max(chunk_size / 2, impl_->cfg.min_effective_chunk);
    }

    // 无 GPU 可用时退化为纯 CPU
    bool has_gpu = false;
    for (const auto& d : impl_->cfg.devices) {
        if (d.backend.rfind("cuda", 0) == 0 && d.available) { has_gpu = true; break; }
    }

    // F-fix 1：固定尾段实验（审计改名为 fixed_tail_chunking，不冒充 guided）
    const bool use_fixed_tail =
        impl_->cfg.enable_fixed_tail_chunking &&
        (end > begin) &&
        (end - begin) > impl_->cfg.min_effective_chunk * 4;

    if (!has_gpu || !estimate.profile_available) {
        // 纯 CPU 路径
        if (use_fixed_tail) {
            // 固定尾段实验：分两段，后段缩块（仅实验，不是动态 guided）
            std::size_t split_point = begin + static_cast<std::size_t>(
                (end - begin) * impl_->cfg.fixed_tail_threshold);

            auto r1 = impl_->runner.run_range(begin, split_point, chunk_size, fn, user_data);

            // 中间采样
            if (impl_->cpu_ctrl) {
                cpu_dec = impl_->cpu_ctrl->sample_and_decide();
            }
            if (impl_->mem_ctrl) {
                auto mem_budget = impl_->mem_ctrl->sample();
                mem_action = utilization::MemoryBudgetController::suggest_action(
                    mem_budget.used_ram, mem_budget.limit_ram, mem_budget.total_ram);
            }

            // 第二段：固定缩块
            std::size_t tail_chunk = std::max(chunk_size / 2, impl_->cfg.min_effective_chunk);
            if (mem_action == utilization::MemoryBudgetController::ExceedAction::ShrinkBlock) {
                tail_chunk = std::max(tail_chunk / 2, impl_->cfg.min_effective_chunk);
            }

            auto r2 = impl_->runner.run_range(split_point, end, tail_chunk, fn, user_data);

            // 合并结果
            result.run_result.total_chunks = r1.total_chunks + r2.total_chunks;
            result.run_result.executed_on_cpu = r1.executed_on_cpu + r2.executed_on_cpu;
            result.run_result.executed_on_gpu = r1.executed_on_gpu + r2.executed_on_gpu;
            result.run_result.failed_chunks = r1.failed_chunks + r2.failed_chunks;
            result.run_result.fallback_chunks = r1.fallback_chunks + r2.fallback_chunks;
            result.run_result.all_done = r1.all_done && r2.all_done;
            result.run_result.error_message = r2.error_message.empty() ? r1.error_message : r2.error_message;

            result.fixed_tail_chunking_used = true;
            result.fixed_tail_min_chunk = tail_chunk;
        } else {
            // F-fix 2：正常路径通过 SharedWorkPool 执行（CostEstimator 驱动）
            result.run_result = impl_->execute_via_pool(begin, end, chunk_size, fn, user_data);
        }

        // F-fix 1：actual_primary_backend 由真实完成统计生成
        result.actual_primary_backend = Impl::actual_backend_from_stats(
            result.run_result.executed_on_cpu,
            result.run_result.executed_on_gpu,
            result.run_result.fallback_chunks);
        result.actual_devices_used = Impl::actual_devices_from_stats(
            result.run_result.executed_on_cpu,
            result.run_result.executed_on_gpu);

        result.total_chunks = result.run_result.total_chunks;
        result.chunks_on_cpu = result.run_result.executed_on_cpu;
        result.chunks_on_gpu = result.run_result.executed_on_gpu;
        result.chunks_fallback = result.run_result.fallback_chunks;

        // F-fix 2：coverage 从 SharedWorkPool 真实导入（不无条件 mark_done）
        if (use_fixed_tail) {
            // fixed_tail 仍用 runner，coverage 从 runner 导入
            impl_->import_real_coverage(result.run_result);
            result.coverage = Impl::coverage_from_result(result.run_result);
        } else {
            // 正常路径从 pool 导入
            impl_->import_pool_coverage();
            result.coverage = Impl::coverage_from_pool(impl_->pool);
        }

        // F-fix 4：执行后采样 CPU 利用率
        if (impl_->cpu_ctrl) {
            cpu_dec = impl_->cpu_ctrl->sample_and_decide();
            result.cpu_actual_ratio = cpu_dec.actual_ratio;
            result.cpu_actual_valid = cpu_dec.valid;
        }

        result.current_state_json = impl_->current_state.status_json();
        return result;
    }

    // F-fix 2：Cost-aware 路径也通过 SharedWorkPool 执行
    // CostEstimate 已影响 chunk_size，进而影响 pool 的块划分
    auto r = impl_->execute_via_pool(begin, end, chunk_size, fn, user_data);
    result.run_result = r;

    // F-fix 1：actual_primary_backend 由真实完成统计生成
    // 不从 estimate.preferred_device 填写
    result.actual_primary_backend = Impl::actual_backend_from_stats(
        r.executed_on_cpu, r.executed_on_gpu, r.fallback_chunks);
    result.actual_devices_used = Impl::actual_devices_from_stats(
        r.executed_on_cpu, r.executed_on_gpu);

    result.total_chunks = r.total_chunks;
    result.chunks_on_cpu = r.executed_on_cpu;
    result.chunks_on_gpu = r.executed_on_gpu;
    result.chunks_fallback = r.fallback_chunks;

    // F-fix 2：coverage 从 SharedWorkPool 导入
    impl_->import_pool_coverage();
    result.coverage = Impl::coverage_from_pool(impl_->pool);

    // 更新 CurrentState 设备统计
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

    // F-fix 4：执行后采样 CPU 利用率
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
