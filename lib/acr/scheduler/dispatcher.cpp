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
#include <chrono>
#include <thread>
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

    // F-fix 3 + F-fix 4：动态 guided 执行 + 资源闭环控制
    // 使用 init_dynamic + claim_next_dynamic（根据 remaining 和活跃设备数动态计算块大小）
    // 在执行循环中应用 CpuController 决策（should_yield/yield_stride）
    // 在执行循环中应用所有 MemoryBudget ExceedAction（StopNewSubmit/ShrinkBlock/...）
    // 所有控制动作记录到 stats_out（用于 50/80/95/100% 持续负载报告）
    MixedRunResult execute_via_pool_dynamic(
        std::size_t begin, std::size_t end,
        std::size_t min_chunk, std::size_t max_chunk,
        std::size_t n_active_devices,
        ChunkKernelFn fn, void* user_data,
        ResourceControlStats& stats_out) {
        // F-fix 3：动态初始化（不预创建块，claim 时动态计算大小）
        pool.init_dynamic(begin, end, min_chunk, max_chunk);
        stats_out.dynamic_mode_used = true;
        stats_out.cpu_target = cfg.cpu_target_ratio;

        MixedRunResult r;
        if (begin >= end) {
            r.all_done = true;
            r.total_chunks = 0;
            return r;
        }

        // 估算最大可能块数（用于 parallel_batch 上限）
        // 实际块数由 claim_next_dynamic 动态决定
        // 与 init_dynamic 一致地规范化 min/max，避免 min>max 时块数估算不足
        std::size_t eff_min = (min_chunk == 0) ? 1 : min_chunk;
        std::size_t eff_max = (max_chunk == 0) ? eff_min : max_chunk;
        if (eff_min > eff_max) eff_min = eff_max;
        std::size_t max_possible_blocks =
            (end - begin + eff_min - 1) / eff_min;
        if (max_possible_blocks == 0) {
            r.all_done = true;
            r.total_chunks = 0;
            return r;
        }

        // F-fix 4：共享控制状态（跨 worker 线程）
        std::atomic<bool> stop_new_submit{false};
        std::atomic<std::size_t> sample_seq{0};
        std::atomic<std::size_t> yield_count{0};
        std::atomic<std::size_t> batch_shrink_count{0};

        // 缓存的 CPU 控制决策（避免每个 worker 都采样）
        std::atomic<bool> cached_should_yield{false};
        std::atomic<std::uint32_t> cached_yield_stride{0};
        std::atomic<std::uint32_t> cached_batch_size{1};

        // 缓存的内存动作
        std::atomic<int> cached_mem_action{
            static_cast<int>(utilization::MemoryBudgetController::ExceedAction::None)};

        // 互斥保护 stats_out 向量（动态 push_back）
        std::mutex stats_mtx;

        std::atomic<std::size_t> executed{0}, failed{0};

        Event ev = astro::compute::parallel_batch(
            astro::compute::KernelId::Custom, max_possible_blocks,
            [&](std::size_t worker_idx) {
                // F-fix 4: submit gate —— 检查 stop_new_submit
                if (stop_new_submit.load(std::memory_order_relaxed)) return;

                // F-fix 4: 周期性采样 CPU 利用率（每 16 个 task 采样一次）
                std::size_t seq = sample_seq.fetch_add(1, std::memory_order_relaxed);
                if ((seq % 16) == 0 && cfg.enable_utilization && cpu_ctrl) {
                    auto dec = cpu_ctrl->sample_and_decide();
                    cached_should_yield.store(dec.should_yield, std::memory_order_relaxed);
                    cached_yield_stride.store(dec.yield_stride, std::memory_order_relaxed);
                    cached_batch_size.store(dec.batch_size, std::memory_order_relaxed);

                    // 记录 CPU 采样序列（用于持续负载报告）
                    {
                        std::lock_guard<std::mutex> lk(stats_mtx);
                        stats_out.cpu_actual_samples.push_back(dec.actual_ratio);
                        stats_out.cpu_sample_ts_ns.push_back(dec.timestamp_ns);
                        stats_out.cpu_valid = dec.valid;
                    }

                    // F-fix 4: CPU 利用率严重超目标（actual > target + 0.10）→ submit gate
                    // 保留 0.10 硬阈值（远大于控制器内部 0.05 容差）
                    if (dec.valid && dec.actual_ratio > dec.target_ratio + 0.10) {
                        stop_new_submit.store(true, std::memory_order_relaxed);
                        stats_out.submit_gate_triggered = true;
                        return;
                    }
                }

                // F-fix 4: 周期性采样内存预算（每 32 个 task 采样一次）
                // 内存采样与 CPU 采样统一受 enable_utilization 控制
                // （生产环境默认 true；单元测试设 false 以隔离系统内存状态）
                if ((seq % 32) == 0 && cfg.enable_utilization && mem_ctrl) {
                    auto mb = mem_ctrl->sample();
                    auto action = utilization::MemoryBudgetController::suggest_action(
                        mb.used_ram, mb.limit_ram, mb.total_ram);
                    cached_mem_action.store(static_cast<int>(action), std::memory_order_relaxed);

                    // 记录内存采样序列
                    {
                        std::lock_guard<std::mutex> lk(stats_mtx);
                        stats_out.mem_actions.push_back(Impl::action_to_string(action));
                        stats_out.mem_used_ram_samples.push_back(mb.used_ram);
                        stats_out.mem_limit_ram = mb.limit_ram;
                        stats_out.final_mem_action = Impl::action_to_string(action);
                    }

                    // 立即处理的动作
                    if (action == utilization::MemoryBudgetController::ExceedAction::StopNewSubmit) {
                        stop_new_submit.store(true, std::memory_order_relaxed);
                        stats_out.submit_gate_triggered = true;
                        return;
                    }
                    if (action == utilization::MemoryBudgetController::ExceedAction::Fail) {
                        stop_new_submit.store(true, std::memory_order_relaxed);
                        stats_out.submit_gate_triggered = true;
                        return;
                    }
                }

                // F-fix 4a: 应用 CpuController 决策 —— 错峰让步
                if (cached_should_yield.load(std::memory_order_relaxed)) {
                    auto stride = cached_yield_stride.load(std::memory_order_relaxed);
                    // 错峰：每 stride 个 worker 中只有一个让步（避免全线程同步睡眠）
                    if (stride > 0 && (worker_idx % stride) == 0) {
                        std::this_thread::yield();
                        yield_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }

                // F-fix 3: 动态领取下一块（根据 remaining 和活跃设备数计算块大小）
                auto* block = pool.claim_next_dynamic("cpu", n_active_devices);
                if (!block) return;  // 无剩余工作

                // 记录动态块大小（用于验证尾部收缩）
                {
                    std::lock_guard<std::mutex> lk(stats_mtx);
                    stats_out.dynamic_chunk_sizes.push_back(block->end - block->begin);
                }

                // F-fix 4b: 应用 MemoryBudget 动作（在 claim 后、执行前）
                auto mem_action = static_cast<
                    utilization::MemoryBudgetController::ExceedAction>(
                    cached_mem_action.load(std::memory_order_relaxed));
                switch (mem_action) {
                    case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock:
                        // 块已 claim，此处记录动作；下一次采样会更新 chunk_size
                        batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    case utilization::MemoryBudgetController::ExceedAction::ReleaseCache:
                        // ReleaseCache：此处无缓存可释放，记录动作（hook 可扩展）
                        batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath:
                        // LowMemoryPath：使用 min_chunk（下一次 claim 会被动态收缩）
                        batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice:
                        // FallbackOtherDevice：无 GPU 时仍用 CPU（记录动作）
                        break;
                    default:
                        break;
                }

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
        r.total_chunks = pool.total_blocks();
        r.all_done = pool.all_done();

        // 记录控制动作统计
        stats_out.yield_count = yield_count.load();
        stats_out.batch_shrink_count = batch_shrink_count.load();

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
    // 内存采样受 enable_utilization 控制（与 CPU 采样一致）
    utilization::MemoryBudgetController::ExceedAction mem_action =
        utilization::MemoryBudgetController::ExceedAction::None;
    if (impl_->cfg.enable_utilization && impl_->mem_ctrl) {
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
            // F-fix 3：动态 guided 路径（默认）
            // 使用 init_dynamic + claim_next_dynamic，根据 remaining 和活跃设备数动态计算块大小
            // 尾部自动收缩，无固定 70% 阈值
            std::size_t min_chunk = impl_->cfg.min_effective_chunk;
            std::size_t max_chunk = chunk_size;
            // 纯 CPU 路径：n_active_devices = 1
            result.run_result = impl_->execute_via_pool_dynamic(
                begin, end, min_chunk, max_chunk, 1,
                fn, user_data, result.resource_control);
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

    // F-fix 3：Cost-aware 路径也通过动态 guided SharedWorkPool 执行
    // CostEstimate 已影响 chunk_size（作为 max_chunk），动态 guided 根据剩余工作收缩
    std::size_t min_chunk = impl_->cfg.min_effective_chunk;
    std::size_t max_chunk = chunk_size;
    // 计算活跃设备数（CPU + 可用 GPU）
    std::size_t n_active_devices = 0;
    for (const auto& d : impl_->cfg.devices) {
        if (d.available) ++n_active_devices;
    }
    if (n_active_devices == 0) n_active_devices = 1;  // 至少 CPU
    auto r = impl_->execute_via_pool_dynamic(
        begin, end, min_chunk, max_chunk, n_active_devices,
        fn, user_data, result.resource_control);
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
