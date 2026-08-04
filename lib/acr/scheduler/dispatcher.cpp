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
#include "../utilization/gpu_controller.hpp"
#include "../utilization/memory_budget.hpp"

#include "astro/compute/acr.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace astro::compute::scheduler {

// ============================================================================
// F-fix 9: 可恢复的 submit gate（带迟滞 + 超时放弃）
// ----------------------------------------------------------------------------
// 替代旧的 std::atomic<bool> stop_new_submit（一旦 true 永久停止）。
// 行为：
//   - close():       gate 关闭（可恢复）。CPU > target+0.10 或 MemoryBudget
//                    StopNewSubmit 时触发。
//   - close_permanent(): gate 永久关闭（MemoryBudget Fail 时触发）。
//   - try_recover(): 迟滞恢复。当 CPU 降到 target-0.05 以下且内存动作
//                    不是 stop/fail 时重新开放 gate。
//   - gate 关闭时不立即返回，而是等待一小段时间（10ms × 重试次数）后
//     检查是否恢复；持续关闭超过 5 秒才最终放弃剩余工作。
// ============================================================================
struct RecoverableGate {
    std::atomic<bool> closed{false};
    std::atomic<bool> permanent_fail{false};   // Fail 动作标记，永久关闭
    std::atomic<std::uint64_t> close_ts_ns{0}; // 最近一次关闭时间戳
    std::atomic<std::uint64_t> close_count{0}; // 关闭次数（含重关闭）
    std::atomic<std::uint64_t> recover_count{0};

    bool should_submit() const noexcept {
        return !closed.load(std::memory_order_relaxed) &&
               !permanent_fail.load(std::memory_order_relaxed);
    }

    bool is_permanent_fail() const noexcept {
        return permanent_fail.load(std::memory_order_relaxed);
    }

    // 关闭 gate（可恢复）。返回是否真的发生了状态转换。
    bool close() noexcept {
        if (permanent_fail.load(std::memory_order_relaxed)) return false;
        bool expected = false;
        if (closed.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
            close_ts_ns.store(now_ns(), std::memory_order_relaxed);
            close_count.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    // 永久关闭 gate（MemoryBudget Fail）
    void close_permanent() noexcept {
        permanent_fail.store(true, std::memory_order_relaxed);
        bool expected = false;
        if (closed.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
            close_ts_ns.store(now_ns(), std::memory_order_relaxed);
            close_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // 尝试恢复 gate（带迟滞）。返回是否真的发生了状态转换。
    bool try_recover(double cpu_actual, double cpu_target,
                     const std::string& mem_action) noexcept {
        if (permanent_fail.load(std::memory_order_relaxed)) return false;
        if (!closed.load(std::memory_order_relaxed)) return false;
        // 迟滞：CPU 降到 target-0.05 以下且内存恢复（非 stop/fail）
        if (cpu_actual < cpu_target - 0.05 &&
            mem_action != "stop" && mem_action != "fail") {
            bool expected = true;
            if (closed.compare_exchange_strong(expected, false,
                                                std::memory_order_acq_rel)) {
                recover_count.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }

    // gate 关闭时长（ns）。gate 已开放时返回 0。
    std::uint64_t closed_duration_ns() const noexcept {
        if (!closed.load(std::memory_order_relaxed)) return 0;
        std::uint64_t ts = close_ts_ns.load(std::memory_order_relaxed);
        if (ts == 0) return 0;
        return now_ns() - ts;
    }

    static std::uint64_t now_ns() noexcept {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    }
};

struct Dispatcher::Impl {
    DispatcherConfig cfg;
    MixedRunner runner;
    QueueAwareEstimator estimator;
    FallbackPolicy fallback_policy;
    CurrentState current_state;
    // F-fix 4：utilization + memory budget
    std::unique_ptr<utilization::CpuController> cpu_ctrl;
    std::unique_ptr<utilization::MemoryBudgetController> mem_ctrl;
    std::unique_ptr<utilization::GpuController> gpu_ctrl;
    // F-fix 2：SharedWorkPool
    SharedWorkPool pool;
    // F-fix 6 + F-fix 7：设备执行器注册表（多设备工作保持）
    std::shared_ptr<ExecutorRegistry> executors;
    // F-fix 9: ReleaseCache 动作的回调（可选）。
    // 由外部注册（如缓存模块），MemoryBudget ReleaseCache 时调用。
    std::function<void()> cache_release_hook;

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
        // F-fix 5: 使用 WorkToken 值令牌，不依赖池内地址
        std::atomic<std::size_t> executed{0}, failed{0};
        Event ev = astro::compute::parallel_batch(
            astro::compute::KernelId::Custom, r.total_chunks,
            [&](std::size_t) {
                auto token = pool.claim_next(kHwCpuDeviceId);
                if (!token.valid()) return;
                try {
                    fn(token.id, token.begin, token.end, user_data);
                    pool.mark_done(token);
                    executed.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    pool.mark_failed(token);
                    failed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        (void)ev;

        r.executed_on_cpu = executed.load();
        r.failed_chunks = failed.load();
        r.all_done = pool.all_done();
        return r;
    }

    // F-fix 3 + F-fix 4 + F-fix 9：动态 guided 执行 + 可恢复资源闭环控制
    // 使用 init_dynamic + claim_next_dynamic（根据 remaining 和活跃设备数动态计算块大小）
    // 在执行循环中应用 CpuController 决策（should_yield/yield_stride/batch_size）
    // 在执行循环中应用所有 MemoryBudget ExceedAction，且动作必须实际改变执行：
    //   - ShrinkBlock: 缩小 current_max_chunk（×0.8，不小于 min_chunk）
    //   - ReleaseCache: 调用 cache_release_hook（若有）
    //   - LowMemoryPath: current_max_chunk = min_chunk
    //   - FallbackOtherDevice: 标记当前设备不可用（多设备由 execute_via_executors 处理）
    //   - StopNewSubmit: 关闭 RecoverableGate（可恢复）
    //   - Fail: 永久关闭 gate
    // F-fix 9 关键改动：
    //   - stop_new_submit（atomic<bool>）→ RecoverableGate（带迟滞 + 超时放弃）
    //   - cached_batch_size 实际用于调整 current_max_chunk（→ pool.set_dynamic_max_chunk）
    //   - gate 关闭时不立即返回，而是等待 10ms×kGateWaitRetries 后检查恢复；
    //     持续关闭超过 kGateTimeoutNs（5 秒）才最终放弃剩余工作。
    MixedRunResult execute_via_pool_dynamic(
        std::size_t begin, std::size_t end,
        std::size_t min_chunk, std::size_t max_chunk,
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

        // F-fix 9: 可恢复的 submit gate（替代旧 stop_new_submit）
        RecoverableGate gate;

        // F-fix 9: 动态 current_max_chunk（所有 worker 共享，通过 pool 同步）
        // 初始为 eff_max；CpuController/MemoryBudget 建议会缩小它
        std::atomic<std::size_t> current_max_chunk{eff_max};

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

        constexpr std::uint64_t kGateTimeoutNs =
            5ULL * 1000 * 1000 * 1000;                  // 5 秒超时放弃
        constexpr std::uint32_t kMaxAttempts = 4;       // 失败重试上限

        // 23 号计划 §5：采样注入（测试可覆盖真实采样器；生产用真实控制器）
        auto sample_cpu = [&]() -> utilization::CpuControlDecision {
            if (cfg.cpu_sampler_override) return cfg.cpu_sampler_override();
            return cpu_ctrl ? cpu_ctrl->sample_and_decide()
                            : utilization::CpuControlDecision{};
        };
        auto sample_mem = [&]() -> utilization::MemoryBudget {
            if (cfg.memory_sampler_override) return cfg.memory_sampler_override();
            return mem_ctrl ? mem_ctrl->sample() : utilization::MemoryBudget{};
        };
        const std::chrono::milliseconds kMemWindowMs(200);

        auto record_action = [&](const std::string& action) {
            std::lock_guard<std::mutex> lk(stats_mtx);
            stats_out.control_actions.push_back(action);
        };

        Event ev = astro::compute::parallel_batch(
            astro::compute::KernelId::Custom, max_possible_blocks,
            [&](std::size_t worker_idx) {
                // 23 号计划 §5：worker 注册（所有 CPU 逻辑线程均可参与）
                std::uint32_t worker_id = 0;
                bool registered = false;
                if (cfg.enable_utilization && cpu_ctrl) {
                    worker_id = cpu_ctrl->register_worker();
                    registered = true;
                }

                const auto cpu_window = std::chrono::milliseconds(
                    cpu_ctrl ? cpu_ctrl->control_window_ms() : 200);
                // 时间窗采样状态（100-500ms，不按 task 计数）；
                // 首次采样立即到期，保证短负载也有采样与控制动作
                auto last_cpu_sample =
                    std::chrono::steady_clock::now() - cpu_window;
                auto last_mem_sample =
                    std::chrono::steady_clock::now() - kMemWindowMs;

                while (true) {
                    const auto now = std::chrono::steady_clock::now();

                    // ---- CPU 时间窗采样 ----
                    if (cfg.enable_utilization &&
                        (now - last_cpu_sample) >= cpu_window) {
                        last_cpu_sample = now;
                        auto dec = sample_cpu();
                        cached_should_yield.store(dec.should_yield, std::memory_order_relaxed);
                        cached_yield_stride.store(dec.yield_stride, std::memory_order_relaxed);
                        cached_batch_size.store(dec.batch_size, std::memory_order_relaxed);
                        {
                            std::lock_guard<std::mutex> lk(stats_mtx);
                            stats_out.cpu_actual_samples.push_back(dec.actual_ratio);
                            stats_out.cpu_sample_ts_ns.push_back(dec.timestamp_ns);
                            stats_out.cpu_valid = dec.valid;
                        }

                        // 迟滞恢复：CPU 降到 target-0.05 以下且内存未 stop/fail
                        auto mem_act_cached = static_cast<
                            utilization::MemoryBudgetController::ExceedAction>(
                            cached_mem_action.load(std::memory_order_relaxed));
                        std::string mem_act_str = Impl::action_to_string(mem_act_cached);
                        if (gate.try_recover(dec.actual_ratio, dec.target_ratio,
                                             mem_act_str)) {
                            stats_out.gate_recover_count++;
                            record_action("gate_recover");
                        }
                        // CPU 严重超目标 → 关闭 gate（可恢复）
                        if (dec.valid && dec.actual_ratio > dec.target_ratio + 0.10) {
                            if (gate.close()) {
                                stats_out.submit_gate_triggered = true;
                                stats_out.gate_close_count++;
                                record_action("gate_close");
                            }
                        }
                        // batch_size → current_max_chunk（claim size 实际生效）
                        std::uint32_t bs = dec.batch_size;
                        if (bs > 0) {
                            std::size_t cur = current_max_chunk.load(std::memory_order_relaxed);
                            std::size_t new_max = cur;
                            if (bs <= 1) {
                                new_max = std::max(cur / 4, eff_min);
                            } else if (bs <= 2) {
                                new_max = std::max(cur / 2, eff_min);
                            } else if (bs <= 4) {
                                new_max = cur;
                            } else if (cur < eff_max) {
                                new_max = std::min(eff_max, cur + (eff_max - cur) / 2);
                            }
                            if (new_max != cur && new_max >= eff_min) {
                                current_max_chunk.store(new_max, std::memory_order_relaxed);
                                pool.set_dynamic_max_chunk(new_max);
                                record_action("batch_resize");
                            }
                        }
                    }

                    // ---- 内存时间窗采样 + MemoryBudget 动作 ----
                    if (cfg.enable_utilization &&
                        (now - last_mem_sample) >= kMemWindowMs) {
                        last_mem_sample = now;
                        auto mb = sample_mem();
                        auto action = utilization::MemoryBudgetController::suggest_action(
                            mb.used_ram, mb.limit_ram, mb.total_ram);
                        cached_mem_action.store(static_cast<int>(action),
                                                std::memory_order_relaxed);
                        {
                            std::lock_guard<std::mutex> lk(stats_mtx);
                            stats_out.mem_actions.push_back(Impl::action_to_string(action));
                            stats_out.mem_used_ram_samples.push_back(mb.used_ram);
                            stats_out.mem_limit_ram = mb.limit_ram;
                            stats_out.final_mem_action = Impl::action_to_string(action);
                        }

                        std::size_t cur = current_max_chunk.load(std::memory_order_relaxed);
                        std::size_t new_max = cur;
                        switch (action) {
                            case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock:
                                new_max = std::max((cur * 4) / 5, eff_min);
                                batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                record_action("shrink_block");
                                break;
                            case utilization::MemoryBudgetController::ExceedAction::ReleaseCache:
                                if (cache_release_hook) {
                                    try { cache_release_hook(); } catch (...) {}
                                }
                                batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                record_action("release_cache");
                                break;
                            case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath:
                                new_max = eff_min;
                                batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                record_action("low_memory_path");
                                break;
                            case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice:
                                // CPU-only 路径无其他设备；invocation 路径由 retry queue
                                // 重新入池并让其他 executor 领取
                                record_action("fallback_other_device");
                                break;
                            case utilization::MemoryBudgetController::ExceedAction::StopNewSubmit:
                                if (gate.close()) {
                                    stats_out.submit_gate_triggered = true;
                                    stats_out.gate_close_count++;
                                    record_action("stop_new_submit");
                                }
                                break;
                            case utilization::MemoryBudgetController::ExceedAction::Fail:
                                gate.close_permanent();
                                stats_out.submit_gate_triggered = true;
                                stats_out.gate_close_count++;
                                record_action("fail");
                                break;
                            default:
                                break;
                        }
                        if (new_max != cur && new_max >= eff_min) {
                            current_max_chunk.store(new_max, std::memory_order_relaxed);
                            pool.set_dynamic_max_chunk(new_max);
                        }
                    }

                    // gate 关闭：等待并继续采样（迟滞恢复），
                    // 禁止直接 return 丢工作；只有永久失败/超时才退出
                    if (!gate.should_submit()) {
                        if (gate.is_permanent_fail()) break;
                        if (gate.closed_duration_ns() > kGateTimeoutNs) {
                            stats_out.gate_aborted = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    // 错峰让步（每 stride 个 worker 一个让步）
                    if (cached_should_yield.load(std::memory_order_relaxed)) {
                        auto stride = cached_yield_stride.load(std::memory_order_relaxed);
                        if (stride > 0 && (worker_idx % stride) == 0) {
                            std::this_thread::yield();
                            yield_count.fetch_add(1, std::memory_order_relaxed);
                            record_action("yield");
                        }
                    }

                    if (registered && cpu_ctrl) {
                        cpu_ctrl->mark_worker_active(worker_id);
                    }
                    // 23 号计划 §5：requested_items 实际使用 current_max_chunk
                    auto token = pool.claim_next_dynamic(
                        kHwCpuDeviceId,
                        current_max_chunk.load(std::memory_order_relaxed));
                    if (!token.valid()) {
                        if (registered && cpu_ctrl) {
                            cpu_ctrl->mark_worker_idle(worker_id);
                        }
                        break;  // 无剩余工作（含 retry 清空）
                    }
                    {
                        std::lock_guard<std::mutex> lk(stats_mtx);
                        stats_out.dynamic_chunk_sizes.push_back(token.size());
                    }
                    try {
                        fn(token.id, token.begin, token.end, user_data);
                        pool.mark_done(token);
                        executed.fetch_add(1, std::memory_order_relaxed);
                    } catch (...) {
                        // 失败块进入 retry queue 可重试，但 attempt 有上限
                        // （防确定性失败死循环导致 TIMEOUT）
                        const bool retryable = token.attempt < kMaxAttempts;
                        pool.mark_failed(token, retryable);
                        failed.fetch_add(1, std::memory_order_relaxed);
                        if (!retryable) break;  // 终态失败：不再重试该块
                    }
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
        stats_out.gate_close_count = gate.close_count.load();
        stats_out.gate_recover_count = gate.recover_count.load();
        // gate 仍关闭且超时 → 标记为放弃
        if (gate.closed.load() && gate.closed_duration_ns() > kGateTimeoutNs) {
            stats_out.gate_aborted = true;
        }
        // 23 号计划 §5：worker 参与记录
        if (cfg.enable_utilization && cpu_ctrl) {
            auto part = cpu_ctrl->worker_participation();
            stats_out.workers_registered = part.registered_count;
            stats_out.workers_active = part.active_count;
        }

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
    // F-fix 5: 使用 done_bitmap 而非 pool.blocks()
    void import_pool_coverage() {
        auto bm = pool.done_bitmap();
        current_state.init_coverage(bm.size());
        for (std::size_t i = 0; i < bm.size(); ++i) {
            if (bm[i]) {
                current_state.coverage().mark_done(i);
            }
        }
    }

    // ===== 23 号计划 §3/§4：通过 DeviceExecutor 执行 KernelInvocation =====
    // 每个 executor 按自身 DeviceCost（recommended_chunk + 队列 + 剩余工作）
    // 计算 requested_items 并独立 claim；CPU/GPU 同时领取，设备忙时不等待。
    // 实际统计（device/items/bytes/duration）来自 SubmitHandle（真实完成）。
    struct InvocationExecStats {
        std::string device_id;
        std::string backend_type;
        std::size_t done_blocks{0};
        std::size_t failed_blocks{0};
        std::size_t items_done{0};
        std::size_t bytes_done{0};
        std::uint64_t elapsed_ns{0};
    };

    // 从 estimate 取某设备的 DeviceCost；缺失时用 executor 默认值（fallback）
    static const cost::DeviceCost* find_device_cost(
        const cost::CostEstimate& estimate, DeviceId device) {
        for (const auto& dc : estimate.per_device) {
            if (dc.device_id == device) return &dc;
        }
        return nullptr;
    }

    MixedRunResult execute_invocation_via_executors(
        const KernelInvocation& invocation,
        const cost::CostEstimate& estimate,
        ResourceControlStats& stats_out,
        std::vector<InvocationExecStats>& per_exec_stats_out,
        std::vector<std::string>& actual_devices_out) {
        MixedRunResult r;
        r.total_chunks = 0;
        r.all_done = false;

        const std::size_t begin = invocation.domain.begin;
        const std::size_t end = invocation.domain.end;
        if (begin >= end) {
            r.all_done = true;
            return r;
        }

        // 1. 收集支持该 OperationId 的可用 executor
        auto all = executors ? executors->available_executors()
                             : std::vector<DeviceExecutor*>{};
        std::vector<DeviceExecutor*> supported;
        for (auto* exec : all) {
            if (exec && exec->supports(invocation.id)) {
                supported.push_back(exec);
            }
        }
        if (supported.empty()) {
            // 无 executor 支持该 op：如实失败（不伪装 CPU/GPU 执行）
            r.error_message = "no executor supports operation: " +
                              std::string(invocation.id);
            return r;
        }

        // 2. 池参数：min = 各 executor 最小有效块的最小值；
        //    max = 各 executor 推荐块的最大值（每设备按自身 requested_items 领取）
        std::size_t min_chunk = supported[0]->min_effective_chunk();
        std::size_t max_chunk = supported[0]->recommended_chunk();
        for (auto* exec : supported) {
            const cost::DeviceCost* dc = find_device_cost(estimate, exec->id());
            const std::size_t rec =
                dc && dc->recommended_chunk > 0 ? dc->recommended_chunk
                                                : exec->recommended_chunk();
            min_chunk = std::min(min_chunk, exec->min_effective_chunk());
            max_chunk = std::max(max_chunk, rec);
        }
        if (min_chunk == 0) min_chunk = 1;
        if (max_chunk == 0) max_chunk = min_chunk;
        if (min_chunk > max_chunk) min_chunk = max_chunk;

        pool.init_dynamic(begin, end, min_chunk, max_chunk);
        stats_out.dynamic_mode_used = true;
        stats_out.cpu_target = cfg.cpu_target_ratio;

        // 3. 每个 executor 一个或多个 worker 线程循环领取执行
        const std::size_t n_exec = supported.size();
        per_exec_stats_out.assign(n_exec, InvocationExecStats{});
        std::vector<std::atomic<std::size_t>> exec_done(n_exec);
        std::vector<std::atomic<std::size_t>> exec_failed(n_exec);
        std::vector<std::atomic<std::size_t>> exec_items(n_exec);
        std::vector<std::atomic<std::size_t>> exec_bytes(n_exec);
        std::vector<std::atomic<std::uint64_t>> exec_elapsed(n_exec);
        for (auto& a : exec_done) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_failed) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_items) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_bytes) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_elapsed) a.store(0, std::memory_order_relaxed);

        constexpr std::uint32_t kMaxAttempts = 4;  // 重试上限（防确定性失败死循环）

        // ===== 23 号计划 §5：可恢复资源闭环（时间窗采样 + 迟滞 gate）=====
        RecoverableGate gate;
        std::atomic<std::size_t> current_max_chunk{max_chunk};
        std::atomic<int> cached_mem_action{
            static_cast<int>(utilization::MemoryBudgetController::ExceedAction::None)};
        std::mutex stats_mtx;
        std::atomic<std::size_t> yield_count{0};
        std::atomic<std::size_t> batch_shrink_count{0};

        constexpr std::uint64_t kGateTimeoutNs = 5ULL * 1000 * 1000 * 1000;
        auto sample_cpu = [&]() -> utilization::CpuControlDecision {
            if (cfg.cpu_sampler_override) return cfg.cpu_sampler_override();
            return cpu_ctrl ? cpu_ctrl->sample_and_decide()
                            : utilization::CpuControlDecision{};
        };
        auto sample_mem = [&]() -> utilization::MemoryBudget {
            if (cfg.memory_sampler_override) return cfg.memory_sampler_override();
            return mem_ctrl ? mem_ctrl->sample() : utilization::MemoryBudget{};
        };
        auto record_action = [&](const std::string& action) {
            std::lock_guard<std::mutex> lk(stats_mtx);
            stats_out.control_actions.push_back(action);
        };

        std::vector<std::thread> workers;
        for (std::size_t i = 0; i < n_exec; ++i) {
            DeviceExecutor* exec = supported[i];
            // CPU 多 worker 用满多核；GPU 单 worker（submit 已同步）
            const std::size_t n_workers =
                (exec->backend_type() == "cpu")
                    ? std::min<std::size_t>(8, std::thread::hardware_concurrency())
                    : 1;
            for (std::size_t w = 0; w < n_workers; ++w) {
                workers.emplace_back([&, i, exec] {
                    std::uint32_t worker_id = 0;
                    bool registered = false;
                    if (cfg.enable_utilization && cpu_ctrl &&
                        exec->backend_type() == "cpu") {
                        worker_id = cpu_ctrl->register_worker();
                        registered = true;
                    }
                    if (gpu_ctrl && exec->backend_type().rfind("cuda", 0) == 0) {
                        gpu_ctrl->register_backend(exec->device_id());
                        gpu_ctrl->report_queue_depth(exec->device_id(),
                                                     static_cast<std::uint32_t>(
                                                         exec->queue_state().depth));
                    }
                    const auto cpu_window = std::chrono::milliseconds(
                        cpu_ctrl ? cpu_ctrl->control_window_ms() : 200);
                    const auto mem_window = std::chrono::milliseconds(200);
                    auto last_cpu_sample =
                        std::chrono::steady_clock::now() - cpu_window;
                    auto last_mem_sample =
                        std::chrono::steady_clock::now() - mem_window;

                    while (true) {
                        const auto now = std::chrono::steady_clock::now();

                        // ---- CPU 时间窗采样 + gate 迟滞 ----
                        if (cfg.enable_utilization &&
                            (now - last_cpu_sample) >= cpu_window) {
                            last_cpu_sample = now;
                            auto dec = sample_cpu();
                            {
                                std::lock_guard<std::mutex> lk(stats_mtx);
                                stats_out.cpu_actual_samples.push_back(dec.actual_ratio);
                                stats_out.cpu_sample_ts_ns.push_back(dec.timestamp_ns);
                                stats_out.cpu_valid = dec.valid;
                            }
                            auto mem_act_cached = static_cast<
                                utilization::MemoryBudgetController::ExceedAction>(
                                cached_mem_action.load(std::memory_order_relaxed));
                            std::string mem_act_str =
                                Impl::action_to_string(mem_act_cached);
                            if (gate.try_recover(dec.actual_ratio, dec.target_ratio,
                                                 mem_act_str)) {
                                stats_out.gate_recover_count++;
                                record_action("gate_recover");
                            }
                            if (dec.valid &&
                                dec.actual_ratio > dec.target_ratio + 0.10) {
                                if (gate.close()) {
                                    stats_out.submit_gate_triggered = true;
                                    stats_out.gate_close_count++;
                                    record_action("gate_close");
                                }
                            }
                        }

                        // ---- 内存时间窗采样 + MemoryBudget 动作 ----
                        if (cfg.enable_utilization &&
                            (now - last_mem_sample) >= mem_window) {
                            last_mem_sample = now;
                            auto mb = sample_mem();
                            auto action =
                                utilization::MemoryBudgetController::suggest_action(
                                    mb.used_ram, mb.limit_ram, mb.total_ram);
                            cached_mem_action.store(static_cast<int>(action),
                                                    std::memory_order_relaxed);
                            {
                                std::lock_guard<std::mutex> lk(stats_mtx);
                                stats_out.mem_actions.push_back(
                                    Impl::action_to_string(action));
                                stats_out.mem_used_ram_samples.push_back(mb.used_ram);
                                stats_out.mem_limit_ram = mb.limit_ram;
                                stats_out.final_mem_action =
                                    Impl::action_to_string(action);
                            }
                            std::size_t cur =
                                current_max_chunk.load(std::memory_order_relaxed);
                            std::size_t new_max = cur;
                            switch (action) {
                                case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock:
                                    new_max = std::max((cur * 4) / 5, min_chunk);
                                    batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                    record_action("shrink_block");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::ReleaseCache:
                                    if (cache_release_hook) {
                                        try { cache_release_hook(); } catch (...) {}
                                    }
                                    batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                    record_action("release_cache");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath:
                                    new_max = min_chunk;
                                    batch_shrink_count.fetch_add(1, std::memory_order_relaxed);
                                    record_action("low_memory_path");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice:
                                    // 失败块已入 retry queue，其他 executor 可领取
                                    record_action("fallback_other_device");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::StopNewSubmit:
                                    if (gate.close()) {
                                        stats_out.submit_gate_triggered = true;
                                        stats_out.gate_close_count++;
                                        record_action("stop_new_submit");
                                    }
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::Fail:
                                    gate.close_permanent();
                                    stats_out.submit_gate_triggered = true;
                                    stats_out.gate_close_count++;
                                    record_action("fail");
                                    break;
                                default:
                                    break;
                            }
                            if (new_max != cur && new_max >= min_chunk) {
                                current_max_chunk.store(new_max, std::memory_order_relaxed);
                                pool.set_dynamic_max_chunk(new_max);
                            }
                        }

                        // gate 关闭：等待并继续采样（迟滞恢复），
                        // 禁止直接 return 丢工作；只有永久失败/超时才退出
                        if (!gate.should_submit()) {
                            if (gate.is_permanent_fail()) break;
                            if (gate.closed_duration_ns() > kGateTimeoutNs) {
                                stats_out.gate_aborted = true;
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            continue;
                        }

                        if (registered && cpu_ctrl) {
                            cpu_ctrl->mark_worker_active(worker_id);
                        }
                        const cost::DeviceCost* dc =
                            find_device_cost(estimate, exec->id());
                        const std::size_t remaining = pool.remaining_work();
                        if (remaining == 0 && pool.retry_pending_count() == 0) {
                            if (registered && cpu_ctrl) {
                                cpu_ctrl->mark_worker_idle(worker_id);
                            }
                            break;
                        }
                        std::size_t requested =
                            cost::global_cost_estimator().compute_requested_items(
                                dc ? *dc : fallback_cost(exec),
                                remaining,
                                exec->queue_state().depth);
                        // 资源闭环：claim size 受当前 max_chunk 约束（ShrinkBlock 等）
                        const std::size_t max_c =
                            current_max_chunk.load(std::memory_order_relaxed);
                        if (requested > max_c) requested = max_c;
                        auto token = pool.claim_next_dynamic(exec->id(), requested);
                        if (!token.valid()) {
                            if (registered && cpu_ctrl) {
                                cpu_ctrl->mark_worker_idle(worker_id);
                            }
                            break;
                        }
                        if (gpu_ctrl && exec->backend_type().rfind("cuda", 0) == 0) {
                            gpu_ctrl->report_queue_depth(
                                exec->device_id(),
                                static_cast<std::uint32_t>(
                                    exec->queue_state().depth));
                        }

                        // 每个 token 一个独立 invocation（domain 为 token 范围）
                        KernelInvocation inv = invocation;
                        inv.domain = WorkDomain{token.begin, token.end};
                        SubmitHandle handle = exec->submit(token, inv);
                        if (handle.status == SubmitStatus::Ok) {
                            pool.mark_done(token);
                            exec_done[i].fetch_add(1, std::memory_order_relaxed);
                            exec_items[i].fetch_add(
                                handle.items_done, std::memory_order_relaxed);
                            exec_bytes[i].fetch_add(
                                handle.bytes_done, std::memory_order_relaxed);
                            exec_elapsed[i].fetch_add(
                                handle.elapsed_ns, std::memory_order_relaxed);
                        } else {
                            // Rejected（op 不支持/设备不可用）→ 终态失败；
                            // kernel 执行失败 → 重试（attempt 上限内）
                            const bool retryable =
                                (handle.status != SubmitStatus::Rejected) &&
                                (token.attempt < kMaxAttempts);
                            pool.mark_failed(token, retryable);
                            exec_failed[i].fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    if (registered && cpu_ctrl) {
                        cpu_ctrl->unregister_worker(worker_id);
                    }
                });
            }
        }
        for (auto& th : workers) th.join();
        for (auto* exec : supported) exec->sync();

        // 记录资源控制统计
        stats_out.yield_count = yield_count.load();
        stats_out.batch_shrink_count = batch_shrink_count.load();
        stats_out.gate_close_count = gate.close_count.load();
        stats_out.gate_recover_count = gate.recover_count.load();
        if (gate.closed.load() && gate.closed_duration_ns() > kGateTimeoutNs) {
            stats_out.gate_aborted = true;
        }
        if (cfg.enable_utilization && cpu_ctrl) {
            auto part = cpu_ctrl->worker_participation();
            stats_out.workers_registered = part.registered_count;
            stats_out.workers_active = part.active_count;
        }

        // 4. 汇总真实统计
        std::size_t total_done = 0;
        std::size_t total_failed = 0;
        for (std::size_t i = 0; i < n_exec; ++i) {
            const std::size_t d = exec_done[i].load(std::memory_order_relaxed);
            const std::size_t f = exec_failed[i].load(std::memory_order_relaxed);
            per_exec_stats_out[i].done_blocks = d;
            per_exec_stats_out[i].failed_blocks = f;
            per_exec_stats_out[i].device_id = supported[i]->device_id();
            per_exec_stats_out[i].backend_type = supported[i]->backend_type();
            per_exec_stats_out[i].items_done =
                exec_items[i].load(std::memory_order_relaxed);
            per_exec_stats_out[i].bytes_done =
                exec_bytes[i].load(std::memory_order_relaxed);
            per_exec_stats_out[i].elapsed_ns =
                exec_elapsed[i].load(std::memory_order_relaxed);
            if (d > 0) {
                actual_devices_out.push_back(supported[i]->device_id());
            }
            total_done += d;
            total_failed += f;
        }

        r.total_chunks = pool.total_blocks();
        r.executed_on_cpu = 0;
        r.executed_on_gpu = 0;
        for (std::size_t i = 0; i < n_exec; ++i) {
            if (supported[i]->backend_type() == "cpu") {
                r.executed_on_cpu += per_exec_stats_out[i].done_blocks;
            } else if (supported[i]->backend_type().rfind("cuda", 0) == 0) {
                r.executed_on_gpu += per_exec_stats_out[i].done_blocks;
            }
        }
        r.failed_chunks = total_failed;
        r.all_done = pool.all_done();
        (void)total_done;
        return r;
    }

    // 无 DeviceCost 时的 fallback 成本（用 executor 自身推荐值）
    static cost::DeviceCost fallback_cost(const DeviceExecutor* exec) {
        cost::DeviceCost dc;
        dc.device_id = exec->id();
        dc.backend = exec->backend_type();
        dc.recommended_chunk = exec->recommended_chunk();
        dc.min_effective_chunk = exec->min_effective_chunk();
        dc.feasible = exec->available();
        dc.profile_available = false;
        dc.reason = "fallback-executor-default";
        return dc;
    }
};

Dispatcher::Dispatcher() : impl_(std::make_unique<Impl>()) {
    impl_->cpu_ctrl = std::make_unique<utilization::CpuController>();
    impl_->mem_ctrl = std::make_unique<utilization::MemoryBudgetController>();
    impl_->gpu_ctrl = std::make_unique<utilization::GpuController>();
}
Dispatcher::~Dispatcher() = default;

void Dispatcher::set_cache_release_hook(std::function<void()> hook) {
    impl_->cache_release_hook = std::move(hook);
}

void Dispatcher::configure(const DispatcherConfig& cfg) {
    impl_->cfg = cfg;
    impl_->executors = cfg.executors;  // F-fix 6 + F-fix 7
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
        if (cfg.control_window_ms >= 100 && cfg.control_window_ms <= 500) {
            impl_->cpu_ctrl->set_control_window_ms(cfg.control_window_ms);
        }
    }
    if (impl_->mem_ctrl) {
        utilization::MemoryBudgetConfig mbcfg;
        impl_->mem_ctrl->configure(mbcfg);
        for (const auto& bn : mcfg.gpu_backends) {
            impl_->mem_ctrl->register_backend(bn);
        }
    }
    if (impl_->gpu_ctrl) {
        impl_->gpu_ctrl->set_target(cfg.cpu_target_ratio);
        for (const auto& bn : mcfg.gpu_backends) {
            impl_->gpu_ctrl->register_backend(bn);
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

    // 23 号计划 §1：旧 lambda/ChunkKernelFn 是 CPU-only compatibility 路径，
    // 一律不走 GPU executor；可加速路径通过 dispatch_invocation(KernelInvocation)
    // 进入 DeviceExecutor（见 23 号计划 §3/§4）。
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
            result.run_result = impl_->execute_via_pool_dynamic(
                begin, end, min_chunk, max_chunk,
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
    auto r = impl_->execute_via_pool_dynamic(
        begin, end, min_chunk, max_chunk,
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

// ===== 23 号计划 §3/§4：可加速 KernelInvocation 派发 =====
CostAwareResult Dispatcher::dispatch_invocation(
    const TaskDescriptor& task,
    const cost::CostEstimate& estimate,
    const KernelInvocation& invocation) {
    CostAwareResult result;
    result.used_cost_estimator = estimate.profile_available;
    result.predicted_primary_backend = impl_->predict_backend_from_estimate(estimate);

    // 工作域来自 invocation（token 子域在 executor worker 内拆分）
    const std::size_t begin = invocation.domain.begin;
    const std::size_t end = invocation.domain.end;
    (void)task;

    std::vector<Impl::InvocationExecStats> per_exec_stats;
    std::vector<std::string> actual_devices;
    auto r = impl_->execute_invocation_via_executors(
        invocation, estimate, result.resource_control, per_exec_stats, actual_devices);
    result.run_result = r;
    result.actual_devices_used = actual_devices;

    // actual_primary_backend：真实完成工作量（items/bytes）最大者
    {
        std::size_t best_items = 0;
        std::size_t best_bytes = 0;
        for (const auto& s : per_exec_stats) {
            best_items += s.items_done;
            best_bytes += s.bytes_done;
        }
        if (best_items == 0 && best_bytes == 0 && r.failed_chunks > 0) {
            result.actual_primary_backend = "none";
        } else if (r.executed_on_gpu > 0 && r.executed_on_gpu > r.executed_on_cpu) {
            result.actual_primary_backend = actual_devices.empty()
                ? "cuda" : actual_devices.front();
        } else if (r.executed_on_cpu > 0) {
            result.actual_primary_backend = "cpu";
        } else {
            result.actual_primary_backend = "none";
        }
    }

    result.total_chunks = r.total_chunks;
    result.chunks_on_cpu = r.executed_on_cpu;
    result.chunks_on_gpu = r.executed_on_gpu;
    result.chunks_fallback = r.fallback_chunks;

    impl_->import_pool_coverage();
    result.coverage = Impl::coverage_from_pool(impl_->pool);

    // 更新 CurrentState（基于真实完成，按 device_id 匹配）
    if (auto* cpu_state = impl_->current_state.find_device("cpu")) {
        cpu_state->chunks_completed.store(r.executed_on_cpu, std::memory_order_relaxed);
    }
    for (const auto& s : per_exec_stats) {
        if (s.backend_type.rfind("cuda", 0) == 0) {
            if (auto* gpu_state = impl_->current_state.find_device(s.device_id)) {
                gpu_state->chunks_completed.store(
                    s.done_blocks, std::memory_order_relaxed);
            }
        }
    }

    // 执行后采样 CPU 利用率
    if (impl_->cpu_ctrl) {
        auto cpu_dec = impl_->cpu_ctrl->sample_and_decide();
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
