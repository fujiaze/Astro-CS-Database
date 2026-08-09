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
#include "../routing/route_profile_v2.hpp"
#include "../routing/benchmark_route_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"
#include "../utilization/memory_budget.hpp"
#include "../utilization/staging_ledger.hpp"

#include "astro/compute/acr.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace astro::compute::scheduler {

// ============================================================================
// ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §4）：Invocation buffer 角色布局
// ----------------------------------------------------------------------------
// 由 OperationId 决定（与 KernelRegistry launcher 的 buffer 约定一致）：
//   dense/chain/weighted_integration：buffer0=输出（独占范围）、
//                                      buffer1..N-1=只读输入
//   reduce/drizzle：                    buffer0=只读输入、
//                                      buffer1=私有 partial（读改写）
// Dispatcher 按此布局决定 prefetch 目标与 GPU 输出物化目标。
// ============================================================================
struct InvocationBufferLayout {
    std::vector<std::size_t> inputs;    // 只读输入 buffer index（prefetch 目标）
    std::vector<std::size_t> outputs;   // 输出 buffer index（D2H/物化目标）
};

InvocationBufferLayout layout_for_invocation(const KernelInvocation& inv) {
    const bool reduce_like =
        (inv.id == kOpPixelReduceFp64Acc ||
         inv.id == kOpDrizzleLikeScatterFp64Acc);
    if (reduce_like) return {{0}, {1}};
    if (inv.id == kOpWeightedIntegrationFp64Acc) return {{1, 2}, {0}};
    return {{1}, {0}};
}

// ============================================================================
// F-fix 9: 可恢复的 submit gate（带迟滞 + 超时放弃）
// ----------------------------------------------------------------------------
// 替代旧的 std::atomic<bool> stop_new_submit（一旦 true 永久停止）。
// 行为：
//   - close():       gate 关闭（可恢复）。MemoryBudget StopNewSubmit 时触发。
//   - close_permanent(): gate 永久关闭（MemoryBudget Fail 时触发）。
//   - try_recover(): 恢复。仅依据内存动作（26 号计划 §2：恢复不读取
//                    CPU/GPU 利用率），动作不是 stop/fail 时重新开放 gate。
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

    // 尝试恢复 gate（仅依据内存动作，26 号计划 §9）。返回是否真的发生了状态转换。
    bool try_recover(const std::string& mem_action) noexcept {
        if (permanent_fail.load(std::memory_order_relaxed)) return false;
        if (!closed.load(std::memory_order_relaxed)) return false;
        // 恢复只依据内存动作（非 stop/fail），不读取 CPU/GPU 利用率
        if (mem_action != "stop" && mem_action != "fail") {
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
    // 26 号计划 §2/§9：只保留 MemoryBudget（独立开关，与利用率解耦）
    std::unique_ptr<utilization::MemoryBudgetController> mem_ctrl;
    // 聚焦版（08 号计划 §5）：OperationProfile 驱动的 Mixed 路由规划
    MixedRoutePlanner planner;
    // 聚焦版（08 号计划 §6）：数据驻留状态（Host/Device/Both/dirty）
    ResidencyManager residency;
    // 聚焦版 v2（08 号计划 §6）：真实 pinned staging reservation ledger
    utilization::StagingLedger staging_ledger;
    // 聚焦版 v2（08 号计划 §4）：dispatcher 级桥接句柄（整帧上传复用）
    void* bridge_handle{nullptr};

    // 惰性获取 dispatcher 桥接句柄（用于真实整帧上传/下载）
    void* ensure_bridge_handle() {
        if (bridge_handle != nullptr) return bridge_handle;
        using namespace astro::compute::cuda::bridge;
        ensure_bridge_loaded();
        auto& api = astro::compute::cuda::bridge::api();
        if (!api.loaded()) return nullptr;
        const char* err = nullptr;
        if (api.init(&err) <= 0) return nullptr;
        bridge_handle = api.executor_create(0, 65536, 256, &err);
        return bridge_handle;
    }
    // F-fix 2：SharedWorkPool
    SharedWorkPool pool;
    // F-fix 6 + F-fix 7：设备执行器注册表（多设备工作保持）
    std::shared_ptr<ExecutorRegistry> executors;
    // F-fix 9: ReleaseCache 动作的回调（可选）。
    // 由外部注册（如缓存模块），MemoryBudget ReleaseCache 时调用。
    // 返回实际释放字节数（06 号规范 §7）
    std::function<std::size_t()> cache_release_hook;

    // Dispatcher Finalization：BDR 决策缓存（仅空队列/无内存快照时命中；
    // 生产带真实 queue/memory 快照时旁路）。同请求重复提交不再重复跑
    // 三路径插值 + Mixed 模拟（06/08 计划稳态语义；不写回 Profile）。
    struct BdrCacheKey {
        std::string operation_id;
        std::uint64_t output_items{0};
        std::uint32_t frame_count{0};
        std::uint8_t input_residency{0};
        std::uint8_t output_policy{0};
        std::uint32_t reuse_count_hint{1};
        bool operator==(const BdrCacheKey& o) const noexcept {
            return operation_id == o.operation_id &&
                   output_items == o.output_items &&
                   frame_count == o.frame_count &&
                   input_residency == o.input_residency &&
                   output_policy == o.output_policy &&
                   reuse_count_hint == o.reuse_count_hint;
        }
    };
    struct BdrCacheHash {
        std::size_t operator()(const BdrCacheKey& k) const noexcept {
            std::size_t h = std::hash<std::string>{}(k.operation_id);
            h ^= std::hash<std::uint64_t>{}(k.output_items) +
                 std::size_t{0x9e3779b97f4a7c15ULL} + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint32_t>{}(k.frame_count) +
                 std::size_t{0x9e3779b97f4a7c15ULL} + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint8_t>{}(k.input_residency) +
                 std::size_t{0x9e3779b97f4a7c15ULL} + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint8_t>{}(k.output_policy) +
                 std::size_t{0x9e3779b97f4a7c15ULL} + (h << 6) + (h >> 2);
            h ^= std::hash<std::uint32_t>{}(k.reuse_count_hint) +
                 std::size_t{0x9e3779b97f4a7c15ULL} + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<BdrCacheKey, routing::RouteDecision, BdrCacheHash>
        bdr_cache;
    static constexpr std::size_t kBdrCacheMax = 128;

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

    // 25 号计划 §7：claim 前内存峰值估算。
    // 覆盖输入、输出、传输 staging（GPU）、双缓冲、临时区（halo/tile）、
    // reduction partial 与 merge 缓冲；返回单次 claim 的保守峰值字节。
    static std::uint64_t estimate_claim_peak_bytes(
        std::size_t items, const TaskTraits& traits, bool is_gpu,
        bool data_resident = false) noexcept {
        if (items == 0) return 0;
        const std::uint64_t read_bytes =
            static_cast<std::uint64_t>(items) * traits.bytes_read_per_item;
        const std::uint64_t write_bytes =
            static_cast<std::uint64_t>(items) * traits.bytes_written_per_item;
        const std::uint64_t io = read_bytes + write_bytes;
        if (io == 0) return 0;  // 无每项字节信息时不参与 claim 前预算
        // 传输 staging（GPU）：输入已驻留时不重复计整帧 H2D（08 号计划 §5），
        // 只计输出 D2H staging
        const std::uint64_t staging =
            is_gpu ? (data_resident ? write_bytes : io) : 0;
        const std::uint64_t double_buf = io;                // 双缓冲
        const std::uint64_t temp = read_bytes / 12 + 1;     // halo/tile 临时区
        const bool is_reduction =
            (traits.task_class == TaskClass::reduction);
        const std::uint64_t partial =
            is_reduction ? (256u * 1024) : 0;               // reduction partial
        const std::uint64_t merge =
            is_reduction ? (256u * 1024) : 0;               // reduction merge
        return io + staging + double_buf + temp + partial + merge;
    }

    // F-fix 2：通过 SharedWorkPool 执行（CostEstimator 驱动）
    // 从 pool claim_next 领取块，执行并标记结果。
    // 失败块可回收（reclaim_failed）。
    MixedRunResult execute_via_pool(std::size_t begin, std::size_t end,
                                    std::size_t chunk_size,
                                    ChunkKernelFn fn, void* user_data) {
        constexpr std::uint32_t kMaxAttempts = 4;  // 失败重试上限
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
                    // 24 号计划 §6：ledger 拒绝（旧 attempt 等）不得累计完成量
                    if (pool.mark_done(token)) {
                        executed.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        // 重新入池重试（由其他 worker 领取）
                        pool.mark_failed(token, /*retryable=*/true);
                        failed.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    const bool retryable = token.attempt < kMaxAttempts;
                    if (pool.mark_failed(token, retryable)) {
                        failed.fetch_add(1, std::memory_order_relaxed);
                    }
                    if (!retryable) return;
                }
            });
        (void)ev;

        r.executed_on_cpu = executed.load();
        r.failed_chunks = failed.load();
        r.all_done = pool.all_done();
        return r;
    }

    // F-fix 3 + F-fix 9 + 26 号计划 §2/§9：动态 guided 执行 + 内存预算反压
    // 使用 init_dynamic + claim_next_dynamic（根据 remaining 和活跃设备数动态计算块大小）
    // 在执行循环中应用所有 MemoryBudget ExceedAction，且动作必须实际改变执行：
    //   - ShrinkBlock: 缩小 current_max_chunk（×0.8，不小于 min_chunk）
    //   - ReleaseCache: 调用 cache_release_hook（若有）
    //   - LowMemoryPath: current_max_chunk = min_chunk
    //   - FallbackOtherDevice: 标记当前设备不可用（多设备由 execute_via_executors 处理）
    //   - StopNewSubmit: 关闭 RecoverableGate（可恢复）
    //   - Fail: 永久关闭 gate
    // 26 号计划 §2：不再读取/控制 CPU/GPU 利用率；gate 关闭只由内存预算触发，
    // 恢复只依据内存动作。gate 关闭时不立即返回，而是等待后检查恢复；
    // 持续关闭超过 kGateTimeoutNs（5 秒）才最终放弃剩余工作。
    MixedRunResult execute_via_pool_dynamic(
        std::size_t begin, std::size_t end,
        std::size_t min_chunk, std::size_t max_chunk,
        ChunkKernelFn fn, void* user_data,
        ResourceControlStats& stats_out,
        const TaskTraits* traits = nullptr) {
        // traits（可选）：提供每项字节信息时，在每次 claim 前执行内存峰值
        // 预算检查（25 号计划 §7）；nullptr 时仅依赖 200ms 系统采样。
        // F-fix 3：动态初始化（不预创建块，claim 时动态计算大小）
        pool.init_dynamic(begin, end, min_chunk, max_chunk);
        stats_out.dynamic_mode_used = true;

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
        // 初始为 eff_max；MemoryBudget 建议会缩小它
        std::atomic<std::size_t> current_max_chunk{eff_max};

        std::atomic<std::size_t> batch_shrink_count{0};

        // 缓存的内存动作
        std::atomic<int> cached_mem_action{
            static_cast<int>(utilization::MemoryBudgetController::ExceedAction::None)};
        // 25 号计划 §7：claim 前预算检查复用最近一次系统采样缓存
        // （200ms 窗口内不重复读系统内存，峰值估算始终实时计算）
        utilization::MemoryBudget cached_mb;
        std::atomic<bool> cached_mb_valid{false};

        // 互斥保护 stats_out 向量（动态 push_back）
        std::mutex stats_mtx;

        std::atomic<std::size_t> executed{0}, failed{0};

        constexpr std::uint64_t kGateTimeoutNs =
            5ULL * 1000 * 1000 * 1000;                  // 5 秒超时放弃
        constexpr std::uint32_t kMaxAttempts = 4;       // 失败重试上限

        // 26 号计划 §9：内存采样注入（测试可覆盖真实采样器；生产用真实控制器）
        auto sample_mem = [&]() -> utilization::MemoryBudget {
            if (cfg.memory_sampler_override) return cfg.memory_sampler_override();
            return mem_ctrl ? mem_ctrl->sample() : utilization::MemoryBudget{};
        };
        const std::chrono::milliseconds kMemWindowMs(200);

        auto record_action = [&](const std::string& action) {
            std::lock_guard<std::mutex> lk(stats_mtx);
            stats_out.control_actions.push_back(action);
        };

        // worker 槽位上限（16）：batch item 即 worker 槽位，每个 item 内 while 循环
        // 连续领取多块；避免按最大可能块数创建任务导致大量空转 worker（注册/采样膨胀）。
        const std::size_t worker_slots = std::min(
            max_possible_blocks,
            std::max<std::size_t>(1,
                std::min<std::size_t>(16, std::thread::hardware_concurrency())));
        Event ev = astro::compute::parallel_batch(
            astro::compute::KernelId::Custom, worker_slots,
            [&](std::size_t /*worker_idx*/) {
                // 内存时间窗采样状态（200ms）；首次采样立即到期，
                // 保证短负载也有采样与控制动作
                auto last_mem_sample =
                    std::chrono::steady_clock::now() - kMemWindowMs;

                while (true) {
                    const auto now = std::chrono::steady_clock::now();

                    // ---- 内存时间窗采样 + MemoryBudget 动作 ----
                    if (cfg.enable_memory_budget &&
                        (now - last_mem_sample) >= kMemWindowMs) {
                        last_mem_sample = now;
                        auto mb = sample_mem();
                        auto action = utilization::MemoryBudgetController::suggest_action(
                            mb.used_ram, mb.limit_ram, mb.total_ram);
                        cached_mem_action.store(static_cast<int>(action),
                                                std::memory_order_relaxed);
                        {
                            std::lock_guard<std::mutex> lk(stats_mtx);
                            // 缓存最近系统采样，供 claim 前峰值预算检查复用
                            cached_mb = mb;
                            cached_mb_valid.store(true, std::memory_order_relaxed);
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
                                    try {
                                        const std::size_t freed =
                                            cache_release_hook();
                                        if (freed > 0) {
                                            record_action("release_cache:" +
                                                           std::to_string(freed));
                                        }
                                    } catch (...) {}
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
                        // 恢复只依据内存动作（26 号计划 §9），不读取 CPU/GPU 利用率
                        if (gate.try_recover(Impl::action_to_string(action))) {
                            stats_out.gate_recover_count++;
                            record_action("gate_recover");
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

                    // ---- 25 号计划 §7：claim 前内存峰值预算检查 ----
                    // 提供 traits（每项字节信息）时，按输入/输出/临时/双缓冲/
                    // staging/partial/merge 峰值估算 + 最近系统采样判断动作；
                    // 动作必须真实改变执行（缩块/停提交/释放缓存/回退/失败）。
                    if (traits && cfg.enable_memory_budget && mem_ctrl) {
                        std::size_t pre_requested =
                            current_max_chunk.load(std::memory_order_relaxed);
                        const std::uint64_t peak = Impl::estimate_claim_peak_bytes(
                            pre_requested, *traits, /*is_gpu=*/false);
                        bool pre_skip = false;
                        if (peak > 0) {
                            utilization::MemoryBudget cached_mb_copy;
                            bool cached_ok = false;
                            {
                                std::lock_guard<std::mutex> lk(stats_mtx);
                                cached_ok = cached_mb_valid.load(
                                    std::memory_order_relaxed);
                                cached_mb_copy = cached_mb;
                            }
                            utilization::MemoryBudgetController::ExceedAction pre_action =
                                utilization::MemoryBudgetController::ExceedAction::None;
                            if (cached_ok && cached_mb_copy.ram_valid &&
                                cached_mb_copy.limit_ram > 0) {
                                pre_action =
                                    utilization::MemoryBudgetController::suggest_action(
                                        cached_mb_copy.used_ram + peak,
                                        cached_mb_copy.limit_ram,
                                        cached_mb_copy.total_ram);
                            }
                            {
                                std::lock_guard<std::mutex> lk(stats_mtx);
                                stats_out.mem_peak_estimates.push_back(peak);
                                stats_out.mem_peak_max =
                                    std::max(stats_out.mem_peak_max, peak);
                                stats_out.mem_peak_actions.push_back(
                                    Impl::action_to_string(pre_action));
                            }
                            switch (pre_action) {
                                case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock:
                                    pre_requested = std::max(
                                        (pre_requested * 4) / 5, eff_min);
                                    batch_shrink_count.fetch_add(
                                        1, std::memory_order_relaxed);
                                    record_action("shrink_block");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::ReleaseCache:
                                    if (cache_release_hook) {
                                        try {
                                            const std::size_t freed =
                                                cache_release_hook();
                                            if (freed > 0) {
                                                record_action("release_cache:" +
                                                               std::to_string(freed));
                                            }
                                        } catch (...) {}
                                    }
                                    pre_requested = std::max(
                                        (pre_requested * 4) / 5, eff_min);
                                    batch_shrink_count.fetch_add(
                                        1, std::memory_order_relaxed);
                                    record_action("release_cache");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath:
                                    pre_requested = eff_min;
                                    batch_shrink_count.fetch_add(
                                        1, std::memory_order_relaxed);
                                    record_action("low_memory_path");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice:
                                    // CPU-only 路径无其他设备：缩小块继续
                                    pre_requested = std::max(
                                        (pre_requested * 4) / 5, eff_min);
                                    record_action("fallback_other_device");
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::StopNewSubmit:
                                    if (gate.close()) {
                                        stats_out.submit_gate_triggered = true;
                                        stats_out.gate_close_count++;
                                        record_action("stop_new_submit");
                                    }
                                    pre_skip = true;
                                    break;
                                case utilization::MemoryBudgetController::ExceedAction::Fail:
                                    gate.close_permanent();
                                    stats_out.submit_gate_triggered = true;
                                    stats_out.gate_close_count++;
                                    record_action("fail");
                                    pre_skip = true;
                                    break;
                                default:
                                    break;
                            }
                            if (pre_skip) {
                                if (gate.is_permanent_fail()) break;
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(10));
                                continue;
                            }
                            const std::size_t cur_max =
                                current_max_chunk.load(std::memory_order_relaxed);
                            if (pre_requested != cur_max && pre_requested >= eff_min) {
                                current_max_chunk.store(
                                    pre_requested, std::memory_order_relaxed);
                                pool.set_dynamic_max_chunk(pre_requested);
                            }
                        }
                    }
                    // 23 号计划 §5：requested_items 实际使用 current_max_chunk
                    auto token = pool.claim_next_dynamic(
                        kHwCpuDeviceId,
                        current_max_chunk.load(std::memory_order_relaxed));
                    if (!token.valid()) {
                        break;  // 无剩余工作（含 retry 清空）
                    }
                    {
                        std::lock_guard<std::mutex> lk(stats_mtx);
                        stats_out.dynamic_chunk_sizes.push_back(token.size());
                    }
                    try {
                        fn(token.id, token.begin, token.end, user_data);
                        // 24 号计划 §6：ledger 拒绝不得累计完成量
                        if (pool.mark_done(token)) {
                            executed.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            pool.mark_failed(token, /*retryable=*/true);
                            failed.fetch_add(1, std::memory_order_relaxed);
                        }
                    } catch (...) {
                        // 失败块进入 retry queue 可重试，但 attempt 有上限
                        // （防确定性失败死循环导致 TIMEOUT）
                        const bool retryable = token.attempt < kMaxAttempts;
                        if (pool.mark_failed(token, retryable)) {
                            failed.fetch_add(1, std::memory_order_relaxed);
                        }
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
        stats_out.batch_shrink_count = batch_shrink_count.load();
        stats_out.gate_close_count = gate.close_count.load();
        stats_out.gate_recover_count = gate.recover_count.load();
        // gate 仍关闭且超时 → 标记为放弃
        if (gate.closed.load() && gate.closed_duration_ns() > kGateTimeoutNs) {
            stats_out.gate_aborted = true;
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
        std::size_t bytes_read{0};
        std::size_t bytes_written{0};
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

    // 24 号计划 §2：Eligible Device Set 判定。
    // 设备必须：有成本数据、feasible、任务规模达到最小有效块、预计收益覆盖
    // 启动/传输/同步/merge 成本（成本明细非零时判定）、GPU 必须有有效 profile
    // （无 profile 的 GPU 不参与生产执行；CPU 无 profile 走明确保守回退）。
    static bool is_executor_eligible(DeviceExecutor* exec,
                                     const cost::DeviceCost* dc,
                                     std::size_t task_size,
                                     bool force_all) {
        if (force_all) return true;  // 测试专用：仅调度验证
        if (dc == nullptr) return false;
        if (!dc->feasible) return false;
        const std::size_t min_chunk =
            std::max(exec->min_effective_chunk(), dc->min_effective_chunk);
        if (min_chunk > 0 && task_size < min_chunk) {
            return false;  // 任务规模未达到最小有效块
        }
        // 收益判定：仅当成本明细非零时执行（避免占位/测试成本全零误伤）
        if (dc->profile_available &&
            (dc->launch_cost_ns > 0 || dc->transfer_cost_ns > 0 ||
             dc->merge_cost_ns > 0)) {
            const double overhead =
                dc->launch_cost_ns + dc->transfer_cost_ns + dc->merge_cost_ns;
            if (dc->compute_cost_ns <= overhead) {
                return false;  // 预计无收益
            }
        }
        // GPU 必须有有效 profile；无 profile 的 GPU 不参与生产（CPU 可保守回退）
        if (exec->backend_type().rfind("cuda", 0) == 0 && !dc->profile_available) {
            return false;
        }
        return true;
    }

    // ===== Dispatcher Finalization：真正 GPU Direct fast path =====
    // BDR 选择 GPU Direct 时（03 号规范 / 08 计划 2）：
    //   - 不创建 SharedWorkPool / CPU worker / barrier / 旧 planner；
    //   - 单 GPU executor：必要输入 prefetch 后整域一次提交（同步语义）；
    //   - 完整记录 items/chunks/H2D/D2H/elapsed。
    struct GpuDirectResult {
        MixedRunResult run_result;
        std::vector<InvocationExecStats> per_exec_stats;
        std::vector<std::string> actual_devices;
        std::uint64_t h2d_bytes_this{0};
    };

    GpuDirectResult execute_gpu_direct(
        const KernelInvocation& invocation,
        const InvocationBufferLayout& layout,
        const std::vector<const void*>& input_hosts,
        const std::vector<std::size_t>& input_bytes,
        const std::vector<std::string>& input_keys,
        const std::vector<bool>& was_resident) {
        GpuDirectResult out;
        out.run_result.total_chunks = 0;
        out.run_result.all_done = false;

        DeviceExecutor* gpu = nullptr;
        if (executors) {
            for (auto* e : executors->available_executors()) {
                if (e->backend_type().rfind("cuda", 0) == 0) {
                    gpu = e;
                    break;
                }
            }
        }
        if (gpu == nullptr || !gpu->supports(invocation.id)) {
            out.run_result.error_message =
                "gpu direct: no cuda executor for operation: " +
                std::string(invocation.id);
            return out;
        }

        // prefetch 全部只读输入（真实上传；已驻留/未变 generation 复用不重传）
        std::uint64_t h2d = 0;
        if (!input_hosts.empty()) {
            if (gpu->prefetch_inputs(input_hosts, input_bytes)) {
                for (std::size_t i = 0; i < input_keys.size(); ++i) {
                    if (!was_resident[i]) {
                        h2d += input_bytes[i];
                        residency.mark_uploaded(input_keys[i]);
                        residency.mark_device_allocated(input_keys[i]);
                    }
                }
            } else {
                out.run_result.error_message = "gpu direct: prefetch failed";
                return out;
            }
        }
        out.h2d_bytes_this = h2d;

        // 整域一次提交（首版 1 GPU / 1 stream 同步语义）
        KernelInvocation inv = invocation;
        inv.domain = WorkDomain{invocation.domain.begin,
                                invocation.domain.end};
        inv.token_id = 0;
        inv.attempt = 0;
        inv.input_resident = true;  // prefetch 后真实驻留
        WorkToken token;
        token.id = 0;
        token.begin = invocation.domain.begin;
        token.end = invocation.domain.end;
        token.attempt = 0;
        SubmitHandle h = gpu->submit(token, inv);
        if (h.status != SubmitStatus::Ok) {
            out.run_result.error_message =
                "gpu direct submit failed: " + h.error;
            return out;
        }

        Impl::InvocationExecStats st;
        st.device_id = gpu->device_id();
        st.backend_type = gpu->backend_type();
        st.done_blocks = 1;
        st.failed_blocks = 0;
        st.items_done = h.items_done;
        st.bytes_done = h.bytes_done;
        st.bytes_read = h.items_done * invocation.traits.bytes_read_per_item;
        st.bytes_written =
            h.items_done * invocation.traits.bytes_written_per_item;
        st.elapsed_ns = h.elapsed_ns;
        out.per_exec_stats.push_back(std::move(st));
        out.actual_devices = {gpu->device_id()};

        out.run_result.total_chunks = 1;
        out.run_result.executed_on_cpu = 0;
        out.run_result.executed_on_gpu = 1;
        out.run_result.fallback_chunks = 0;
        out.run_result.all_done = true;

        return out;
    }

    MixedRunResult execute_invocation_via_executors(
        const KernelInvocation& invocation,
        const cost::CostEstimate& estimate,
        ResourceControlStats& stats_out,
        std::vector<InvocationExecStats>& per_exec_stats_out,
        std::vector<std::string>& actual_devices_out,
        bool data_resident = false,
        const routing::RouteDecision* bdr = nullptr) {
        MixedRunResult r;
        r.total_chunks = 0;
        r.all_done = false;
        // Dispatcher Finalization：BDR 激活时旧 focused OperationProfile /
        // MixedRoutePlanner 不得再做顶层设备资格（06/08 计划"双重路由禁止"）。
        const bool bdr_active = (bdr != nullptr);
        const qualification::focused::OperationProfile* plan_profile =
            bdr_active ? nullptr : cfg.operation_profile;
        // BDR Mixed 时 CPU/GPU 块来自二维 service 模型（bdr->cpu/gpu_chunk_items）；
        // 旧 CostEstimator 只作为非 BDR 兼容回退。
        const std::uint64_t bdr_cpu_chunk =
            bdr_active ? bdr->cpu_chunk_items : 0u;
        const std::uint64_t bdr_gpu_chunk =
            bdr_active ? bdr->gpu_chunk_items : 0u;

        const std::size_t begin = invocation.domain.begin;
        const std::size_t end = invocation.domain.end;
        if (begin >= end) {
            r.all_done = true;
            return r;
        }

        // 1. 收集支持该 OperationId 且满足 Eligible Device Set 的 executor
        auto all = executors ? executors->available_executors()
                             : std::vector<DeviceExecutor*>{};
        std::vector<DeviceExecutor*> supported;
        const std::size_t task_size = end - begin;
        // 聚焦版 v2（05 号规范 §2）：Auto 模式在 worker 启动前执行收益门。
        // GPU 只有在当前路径（host/resident）eligible 且任务规模达到
        // min_profitable_items 时才进入 worker 集合；不满足则不启动 worker。
        const auto auto_plan = (plan_profile != nullptr)
            ? planner.plan(std::string(invocation.id), task_size,
                           data_resident)
            : MixedRoutePlan{};
        for (auto* exec : all) {
            // 聚焦版 RouteMode（08 号计划 §3/§5）：
            // CpuOnly/GpuOnly 强制只启用一类设备（对照/回退/资格测试）
            if (cfg.route_mode == RouteMode::CpuOnly &&
                exec->backend_type() != "cpu") {
                continue;
            }
            if (cfg.route_mode == RouteMode::GpuOnly &&
                exec->backend_type() == "cpu") {
                continue;
            }
            // Dispatcher Finalization：BDR 顶层已选定路径，按 chosen 过滤
            // 执行器，并绕过旧 planner 收益门 / eligible 判定（BDR 是权威）。
            if (bdr_active) {
                if (bdr->chosen == routing::RouteKind::OpenMP &&
                    exec->backend_type() != "cpu") {
                    continue;
                }
                if (bdr->chosen == routing::RouteKind::GpuDirect &&
                    exec->backend_type().rfind("cuda", 0) != 0) {
                    continue;
                }
                if (exec && exec->supports(invocation.id)) {
                    supported.push_back(exec);
                }
                continue;
            }
            // Auto 前置收益门（仅 AutoMixed 且非 ForcedMixed）：
            // GPU 无合格路径或规模不足时不得进入 worker 集合；
            // CpuOnly/GpuOnly 是强制对照模式，不受收益门限制。
            if (cfg.route_mode == RouteMode::AutoMixed &&
                plan_profile != nullptr &&
                !cfg.force_all_supported_executors &&
                exec->backend_type().rfind("cuda", 0) == 0) {
                if (!auto_plan.profile_available) continue;
                const std::size_t min_items = data_resident
                    ? auto_plan.gpu_min_resident_items
                    : auto_plan.gpu_min_host_items;
                if (min_items > 0 && task_size < min_items) continue;
            }
            if (exec && exec->supports(invocation.id) &&
                is_executor_eligible(exec, find_device_cost(estimate, exec->id()),
                                     task_size,
                                     cfg.force_all_supported_executors)) {
                supported.push_back(exec);
            }
        }
        if (supported.empty()) {
            // 无 eligible executor：如实失败（不伪装 CPU/GPU 执行）
            r.error_message = "no eligible executor for operation: " +
                              std::string(invocation.id);
            return r;
        }

        // 2. 池参数：min = 各 executor 最小有效块的最小值；
        //    max = 各 executor 推荐块的最大值（每设备按自身 requested_items 领取）
        std::size_t min_chunk = supported[0]->min_effective_chunk();
        std::size_t max_chunk = supported[0]->recommended_chunk();
        const bool gpu_participating = std::any_of(
            supported.begin(), supported.end(),
            [](const DeviceExecutor* e) {
                return e->backend_type().rfind("cuda", 0) == 0;
            });
        if (bdr_active && bdr_cpu_chunk > 0 && bdr_gpu_chunk > 0 &&
            gpu_participating) {
            // BDR Mixed：CPU/GPU 块来自二维 service 模型（唯一来源）
            min_chunk = static_cast<std::size_t>(
                std::min(bdr_cpu_chunk, bdr_gpu_chunk));
            max_chunk = static_cast<std::size_t>(
                std::max(bdr_cpu_chunk, bdr_gpu_chunk));
            for (auto* exec : supported) {
                min_chunk = std::min(min_chunk, exec->min_effective_chunk());
            }
        } else if (plan_profile != nullptr &&
            auto_plan.profile_available && gpu_participating) {
            // ACR 架构冻结（07 号计划 C/E）：Auto+Profile 且 GPU 实际参与时，
            // 块大小来自 OperationProfile（混合小块用于分摊传输/尾段）。
            min_chunk = std::min(
                min_chunk, std::min(auto_plan.cpu_chunk_items,
                                    auto_plan.gpu_chunk_items));
            max_chunk = std::max(auto_plan.cpu_chunk_items,
                                 auto_plan.gpu_chunk_items);
            for (auto* exec : supported) {
                min_chunk = std::min(
                    min_chunk, exec->min_effective_chunk());
            }
        } else {
            // 纯 CPU（GPU 无收益/未参与）或 profile 不可用：
            // CPU 块回退 estimate 推荐块，与 CpuOnly 直连一致，
            // 避免混合小块带来的 worker 领取/同步开销（08 计划 J flaky 根因）。
            for (auto* exec : supported) {
                const cost::DeviceCost* dc =
                    find_device_cost(estimate, exec->id());
                const std::size_t rec =
                    dc && dc->recommended_chunk > 0
                        ? dc->recommended_chunk
                        : exec->recommended_chunk();
                min_chunk = std::min(min_chunk, exec->min_effective_chunk());
                max_chunk = std::max(max_chunk, rec);
            }
        }
        if (min_chunk == 0) min_chunk = 1;
        if (max_chunk == 0) max_chunk = min_chunk;
        if (min_chunk > max_chunk) min_chunk = max_chunk;

        pool.init_dynamic(begin, end, min_chunk, max_chunk);
        stats_out.dynamic_mode_used = true;

        // 3. 每个 executor 一个或多个 worker 线程循环领取执行
        const std::size_t n_exec = supported.size();
        per_exec_stats_out.assign(n_exec, InvocationExecStats{});
        std::vector<std::atomic<std::size_t>> exec_done(n_exec);
        std::vector<std::atomic<std::size_t>> exec_failed(n_exec);
        std::vector<std::atomic<std::size_t>> exec_items(n_exec);
        std::vector<std::atomic<std::size_t>> exec_bytes(n_exec);
        std::vector<std::atomic<std::size_t>> exec_bytes_read(n_exec);
        std::vector<std::atomic<std::size_t>> exec_bytes_written(n_exec);
        std::vector<std::atomic<std::uint64_t>> exec_elapsed(n_exec);
        // 23 号计划 §4/§6：首轮领取公平门——每个 executor 至少先领取一块，
        // 之后才允许任意设备连续领取。防止快速 CPU worker 在慢设备（GPU）线程
        // 首次领取前耗尽整个池，保证真实 Mixed 确定发生（cpu_done>0 && gpu_done>0）。
        std::vector<std::atomic<bool>> first_claimed(n_exec);
        std::atomic<std::size_t> first_round_made{0};
        std::atomic<bool> first_round_done{false};
        for (auto& f : first_claimed) f.store(false, std::memory_order_relaxed);
        for (auto& a : exec_done) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_failed) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_items) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_bytes) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_bytes_read) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_bytes_written) a.store(0, std::memory_order_relaxed);
        for (auto& a : exec_elapsed) a.store(0, std::memory_order_relaxed);

        constexpr std::uint32_t kMaxAttempts = 4;  // 重试上限（防确定性失败死循环）

        // 23 号计划 §4：worker 数（CPU 可用配置覆盖；GPU 单 worker）
        std::vector<std::size_t> workers_per_exec(n_exec);
        std::size_t total_workers = 0;
        for (std::size_t i = 0; i < n_exec; ++i) {
            if (supported[i]->backend_type() == "cpu") {
                // ACR 架构冻结（07 号计划 E）：Auto+Profile 场景，worker 启动前
                // 预判 CPU 是否可能缩短总完工时间（用 Profile 值估算）：
                //   CPU 一块完成时间 < GPU 清空剩余的时间 → CPU 才启动。
                //   否则 CPU 不启动（自然 GPU-only），避免 16 个 CPU worker
                //   线程创建/调度固定开销拖慢小任务 Auto（实测 512² Auto
                //   曾慢于 GPU resident 约 11 倍）。
                bool cpu_skip = false;
                if (cfg.route_mode == RouteMode::AutoMixed &&
                    plan_profile != nullptr &&
                    auto_plan.profile_available) {
                    // Profile 的 cpu_ns_per_item 是 ACR CPU 全量并行吞吐
                    // （多 worker 分摊）；而每个 CPU 块由单个 worker 单线程
                    // 串行执行，单线程成本 ≈ 并行吞吐 × worker 数。
                    const double cpu_single_ns =
                        auto_plan.cpu_ns_per_item *
                        static_cast<double>(
                            std::thread::hardware_concurrency());
                    const double cpu_block_ns =
                        cpu_single_ns *
                            static_cast<double>(auto_plan.cpu_chunk_items) +
                        auto_plan.cpu_fixed_ns;
                    const double gpu_rest_ns =
                        auto_plan.gpu_ns_per_item *
                            static_cast<double>(task_size) +
                        auto_plan.gpu_fixed_ns;
                    // 保守参与门：CPU 一块必须显著快于 GPU 清空剩余
                    // （×0.35 折扣 Profile 预测误差与 CPU 线程启动开销；
                    // 加权 GPU 明显占优时
                    // Auto 自然 GPU-only，避免 16 个 CPU worker 拖慢）。
                    cpu_skip = (cpu_block_ns >= gpu_rest_ns * 0.35);
                }
                workers_per_exec[i] =
                    cpu_skip ? 0
                             : (cfg.invocation_cpu_workers > 0
                                    ? cfg.invocation_cpu_workers
                                    : std::thread::hardware_concurrency());
            } else {
                workers_per_exec[i] = 1;
            }
            total_workers += workers_per_exec[i];
        }
        if (total_workers == 0) {
            // 防御：所有设备都被预判排除时回退 CPU 单 worker（禁止漏算）
            for (std::size_t i = 0; i < n_exec; ++i) {
                if (supported[i]->backend_type() == "cpu") {
                    workers_per_exec[i] = 1;
                    total_workers = 1;
                    break;
                }
            }
        }
        // 启动屏障：所有 executor worker 同时开始领取（消除首领取偏差，
        // 保证 CPU 与 GPU 都能拿到非零工作，不被单方先耗尽）
        std::barrier start_barrier(total_workers);

        // ===== 23 号计划 §5：可恢复资源闭环（时间窗采样 + 迟滞 gate）=====
        RecoverableGate gate;
        std::atomic<std::size_t> current_max_chunk{max_chunk};
        std::atomic<int> cached_mem_action{
            static_cast<int>(utilization::MemoryBudgetController::ExceedAction::None)};
        // 25 号计划 §7：claim 前预算检查复用最近一次系统采样缓存
        utilization::MemoryBudget cached_mb;
        std::atomic<bool> cached_mb_valid{false};
        std::mutex stats_mtx;
        std::atomic<std::size_t> batch_shrink_count{0};

        constexpr std::uint64_t kGateTimeoutNs = 5ULL * 1000 * 1000 * 1000;
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
            const std::size_t n_workers = workers_per_exec[i];
            for (std::size_t w = 0; w < n_workers; ++w) {
                workers.emplace_back([&, i, exec] {
                    start_barrier.arrive_and_wait();
                    const auto mem_window = std::chrono::milliseconds(200);
                    auto last_mem_sample =
                        std::chrono::steady_clock::now() - mem_window;

                    while (true) {
                        const auto now = std::chrono::steady_clock::now();

                        // ---- 内存时间窗采样 + MemoryBudget 动作 ----
                        if (cfg.enable_memory_budget &&
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
                                // 缓存最近系统采样，供 claim 前峰值预算检查复用
                                cached_mb = mb;
                                cached_mb_valid.store(true, std::memory_order_relaxed);
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
                                        try {
                                            const std::size_t freed =
                                                cache_release_hook();
                                            if (freed > 0) {
                                                record_action("release_cache:" +
                                                               std::to_string(freed));
                                            }
                                        } catch (...) {}
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
                            // 恢复只依据内存动作（26 号计划 §9），不读取 CPU/GPU 利用率
                            if (gate.try_recover(Impl::action_to_string(action))) {
                                stats_out.gate_recover_count++;
                                record_action("gate_recover");
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

                        const cost::DeviceCost* dc =
                            find_device_cost(estimate, exec->id());
                        const std::size_t remaining = pool.remaining_work();
                        if (remaining == 0 && pool.retry_pending_count() == 0) {
                            // 首轮公平门（无 OperationProfile 时启用）：池已被
                            // 其他设备抢空时，本 executor 无首块可领也必须参与
                            // 公平门计数并解除等待（否则其他 worker 无限等待
                            // first_round_done 造成死锁）。
                            if (!plan_profile &&
                                !first_round_done.load(
                                    std::memory_order_acquire)) {
                                if (!first_claimed[i].exchange(
                                        true, std::memory_order_acq_rel)) {
                                    if (first_round_made.fetch_add(
                                            1, std::memory_order_acq_rel) + 1 >=
                                        n_exec) {
                                        first_round_done.store(
                                            true, std::memory_order_release);
                                    }
                                }
                            }
                            break;
                        }
                        // 聚焦版（08 号计划 §5）：OperationProfile 驱动规划
                        // 决定 CPU/GPU 独立块大小与边际收益门；无合格 Profile
                        // 时回退旧 CostEstimator 路径（保守 CPU fallback）。
                        std::size_t requested = 0;
                        const auto plan = planner.plan(
                            std::string(invocation.id), remaining,
                            data_resident);
                        // Dispatcher Finalization：BDR Mixed 只执行 Estimator
                        // 给出的 chunk（06/08 计划；旧 planner 不得改写顶层选择）。
                        if (bdr_active &&
                            bdr->chosen == routing::RouteKind::Mixed) {
                            requested =
                                (exec->backend_type() == "cpu")
                                    ? bdr->cpu_chunk_items
                                    : bdr->gpu_chunk_items;
                            if (requested == 0) {
                                requested =
                                    cost::global_cost_estimator()
                                        .compute_requested_items(
                                            dc ? *dc : fallback_cost(exec),
                                            remaining,
                                            exec->queue_state().depth);
                            }
                            if (requested == 0) requested = 1;
                        } else if (plan.profile_available) {
                            requested =
                                (exec->backend_type() == "cpu")
                                    ? plan.cpu_chunk_items
                                    : plan.gpu_chunk_items;
                            if (requested == 0) requested = 1;
                            // 运行时完成时间只用于本次队列/尾部判断
                            // （不写回 Profile，不跨运行学习）
                            auto measured_for = [&](std::size_t idx) -> double {
                                const std::size_t items =
                                    exec_items[idx].load(
                                        std::memory_order_relaxed);
                                const auto el =
                                    exec_elapsed[idx].load(
                                        std::memory_order_relaxed);
                                return (items > 0 && el > 0)
                                    ? static_cast<double>(el) /
                                          static_cast<double>(items)
                                    : 0.0;
                            };
                            const double measured_ns = measured_for(i);
                            // 另一设备实测速率（用于边际收益对比）
                            double other_measured_ns = 0.0;
                            for (std::size_t j = 0; j < n_exec; ++j) {
                                if (j != i &&
                                    supported[j]->backend_type() !=
                                        exec->backend_type()) {
                                    other_measured_ns = measured_for(j);
                                    break;
                                }
                            }
                    // 边际收益门（05 号规范 §4/§5）：预计拖尾的设备停止新
                    // claim，由最快设备清尾；Auto 不得强制慢设备重新领取。
                    // ForcedMixed（force_all）保留首块参与（仅正确性测试）。
                    if (!MixedRoutePlanner::should_claim(
                            plan, exec->backend_type(), remaining,
                            exec->queue_state().depth, measured_ns,
                            other_measured_ns,
                            cfg.force_all_supported_executors)) {
                        break;  // 停止该设备 claim（Auto：最快设备清尾）
                    }
                        } else {
                            requested =
                                cost::global_cost_estimator().compute_requested_items(
                                    dc ? *dc : fallback_cost(exec),
                                    remaining,
                                    exec->queue_state().depth);
                        }
                        // 资源闭环：claim size 受当前 max_chunk 约束（ShrinkBlock 等）
                        const std::size_t max_c =
                            current_max_chunk.load(std::memory_order_relaxed);
                        if (requested > max_c) requested = max_c;
                        // ---- 25 号计划 §7：claim 前内存峰值预算检查 ----
                        // 按输入/输出/临时/双缓冲/传输 staging/partial/merge
                        // 峰值估算 + 最近系统采样判断动作；动作必须真实改变执行
                        // （缩块/停提交/释放缓存/低内存路径/回退其他设备/失败）。
                        if (cfg.enable_memory_budget && mem_ctrl) {
                            const bool is_gpu =
                                exec->backend_type().rfind("cuda", 0) == 0;
                            bool pre_skip = false;
                            // 06 号规范 §7.1/§7.4：Shrink 后循环重估，
                            // 直到满足预算或达到最小块（上限 8 次）
                            for (int shrink_guard = 0; shrink_guard < 8;
                                 ++shrink_guard) {
                                const std::uint64_t peak =
                                    Impl::estimate_claim_peak_bytes(
                                        requested, invocation.traits, is_gpu,
                                        data_resident);
                                if (peak == 0) break;
                                utilization::MemoryBudget cached_mb_copy;
                                bool cached_ok = false;
                                {
                                    std::lock_guard<std::mutex> lk(stats_mtx);
                                    cached_ok = cached_mb_valid.load(
                                        std::memory_order_relaxed);
                                    cached_mb_copy = cached_mb;
                                }
                                utilization::MemoryBudgetController::ExceedAction pre_action =
                                    utilization::MemoryBudgetController::ExceedAction::None;
                                if (cached_ok && is_gpu) {
                                    for (const auto& g : cached_mb_copy.gpus) {
                                        if (g.backend == exec->device_id() &&
                                            g.valid && g.limit_vram > 0) {
                                            pre_action =
                                                utilization::MemoryBudgetController::
                                                    suggest_action(
                                                        g.used_vram + peak,
                                                        g.limit_vram,
                                                        g.total_vram);
                                            break;
                                        }
                                    }
                                } else if (cached_ok &&
                                           cached_mb_copy.ram_valid &&
                                           cached_mb_copy.limit_ram > 0) {
                                    pre_action =
                                        utilization::MemoryBudgetController::
                                            suggest_action(
                                                cached_mb_copy.used_ram + peak,
                                                cached_mb_copy.limit_ram,
                                                cached_mb_copy.total_ram);
                                }
                                {
                                    std::lock_guard<std::mutex> lk(stats_mtx);
                                    stats_out.mem_peak_estimates.push_back(peak);
                                    stats_out.mem_peak_max =
                                        std::max(stats_out.mem_peak_max, peak);
                                    stats_out.mem_peak_actions.push_back(
                                        Impl::action_to_string(pre_action));
                                    if (is_gpu) {
                                        stats_out.mem_vram_actions.push_back(
                                            Impl::action_to_string(pre_action));
                                    }
                                }
                                bool stop_loop = false;
                                switch (pre_action) {
                                    case utilization::MemoryBudgetController::ExceedAction::ShrinkBlock:
                                        requested = std::max(
                                            (requested * 4) / 5, min_chunk);
                                        batch_shrink_count.fetch_add(
                                            1, std::memory_order_relaxed);
                                        record_action("shrink_block");
                                        // 缩小后继续循环重估
                                        if (requested <= min_chunk) {
                                            stop_loop = true;
                                        }
                                        break;
                                    case utilization::MemoryBudgetController::ExceedAction::ReleaseCache:
                                        if (cache_release_hook) {
                                            try {
                                            const std::size_t freed =
                                                cache_release_hook();
                                            if (freed > 0) {
                                                record_action("release_cache:" +
                                                               std::to_string(freed));
                                            }
                                        } catch (...) {}
                                        }
                                        requested = std::max(
                                            (requested * 4) / 5, min_chunk);
                                        batch_shrink_count.fetch_add(
                                            1, std::memory_order_relaxed);
                                        record_action("release_cache");
                                        // 释放后重新采样和重算（循环）
                                        break;
                                    case utilization::MemoryBudgetController::ExceedAction::LowMemoryPath:
                                        requested = min_chunk;
                                        batch_shrink_count.fetch_add(
                                            1, std::memory_order_relaxed);
                                        record_action("low_memory_path");
                                        stop_loop = true;
                                        break;
                                    case utilization::MemoryBudgetController::ExceedAction::FallbackOtherDevice:
                                        // 工作留在 pool，由其他 executor 领取
                                        record_action("fallback_other_device");
                                        pre_skip = true;
                                        stop_loop = true;
                                        break;
                                    case utilization::MemoryBudgetController::ExceedAction::StopNewSubmit:
                                        if (gate.close()) {
                                            stats_out.submit_gate_triggered = true;
                                            stats_out.gate_close_count++;
                                            record_action("stop_new_submit");
                                        }
                                        pre_skip = true;
                                        stop_loop = true;
                                        break;
                                    case utilization::MemoryBudgetController::ExceedAction::Fail:
                                        gate.close_permanent();
                                        stats_out.submit_gate_triggered = true;
                                        stats_out.gate_close_count++;
                                        record_action("fail");
                                        pre_skip = true;
                                        stop_loop = true;
                                        break;
                                    default:
                                        stop_loop = true;
                                        break;
                                }
                                if (stop_loop) break;
                            }
                            if (pre_skip) {
                                if (gate.is_permanent_fail()) break;
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(10));
                                continue;
                            }
                            if (requested <
                                current_max_chunk.load(
                                    std::memory_order_relaxed)) {
                                current_max_chunk.store(
                                    requested, std::memory_order_relaxed);
                                pool.set_dynamic_max_chunk(requested);
                            }
                        }
                        // ---- 聚焦版 v2：pinned staging reserve（GPU）----
                        // 真实 reservation ledger；预算不足时缩小块重试一次
                        if (cfg.enable_memory_budget &&
                            exec->backend_type().rfind("cuda", 0) == 0) {
                            const std::size_t staging =
                                requested * invocation.traits.bytes_read_per_item +
                                requested * invocation.traits.bytes_written_per_item;
                            if (staging > 0 &&
                                !staging_ledger.reserve(staging)) {
                                const std::size_t shrunk =
                                    std::max(requested / 2, min_chunk);
                                if (shrunk < requested &&
                                    staging_ledger.reserve(
                                        shrunk *
                                        (invocation.traits.bytes_read_per_item +
                                         invocation.traits.bytes_written_per_item))) {
                                    requested = shrunk;
                                    batch_shrink_count.fetch_add(
                                        1, std::memory_order_relaxed);
                                    record_action("staging_shrink");
                                } else {
                                    // pinned 预算不足：跳过该块（等释放）
                                    record_action("staging_wait");
                                    std::this_thread::sleep_for(
                                        std::chrono::milliseconds(10));
                                    continue;
                                }
                            }
                        }
                        auto token = pool.claim_next_dynamic(exec->id(), requested);
                        if (!token.valid()) {
                            // 首轮公平门（无 OperationProfile 时启用）：每个
                            // executor 至少 claim 一块后才允许任意设备连续领取。
                            // 若池已被其他设备抢空（本 executor 无首块可领），
                            // 也必须参与公平门计数并解除等待——否则其他 worker
                            // 会无限等待 first_round_done 造成死锁（fix: 慢设备
                            // 首块让位后由最快设备清尾，公平门自然结束）。
                            if (!plan_profile &&
                                !first_round_done.load(std::memory_order_acquire)) {
                                if (!first_claimed[i].exchange(
                                        true, std::memory_order_acq_rel)) {
                                    if (first_round_made.fetch_add(
                                            1, std::memory_order_acq_rel) + 1 >=
                                        n_exec) {
                                        first_round_done.store(
                                            true, std::memory_order_release);
                                    }
                                }
                            }
                            break;
                        }
                        {
                            std::lock_guard<std::mutex> lk(stats_mtx);
                            // 真实领取块序列（07 号计划 B：chunk 统计）
                            stats_out.dynamic_chunk_sizes.push_back(
                                token.size());
                        }
                        // 首轮公平门（仅影响每个 executor 的第一块之前）。
                        // 聚焦版：OperationProfile 规划模式（08 号计划 §5）下
                        // 禁用强制公平门——由 planner 的边际收益门决定设备是否
                        // 参与，避免慢设备被公平门阻塞导致死锁。
                        if (!plan_profile &&
                            !first_round_done.load(std::memory_order_acquire)) {
                            if (!first_claimed[i].exchange(true,
                                                           std::memory_order_acq_rel)) {
                                if (first_round_made.fetch_add(1,
                                                std::memory_order_acq_rel) + 1 ==
                                    n_exec) {
                                    first_round_done.store(true,
                                                           std::memory_order_release);
                                }
                            } else {
                                while (!first_round_done.load(
                                           std::memory_order_acquire)) {
                                    std::this_thread::yield();
                                }
                            }
                        }
                        // 每个 token 一个独立 invocation（domain 为 token 范围）
                        KernelInvocation inv = invocation;
                        inv.domain = WorkDomain{token.begin, token.end};
                        inv.token_id = token.id;
                        inv.attempt = token.attempt;
                        // 聚焦版 v3：输入已 prefetch → launcher 走 resident 路径
                        inv.input_resident = data_resident;
                        SubmitHandle handle = exec->submit(token, inv);
                        if (handle.status == SubmitStatus::Ok) {
                            // 24 号计划 §6：ledger 拒绝不得累计 actual 统计
                            if (pool.mark_done(token)) {
                                exec_done[i].fetch_add(1, std::memory_order_relaxed);
                                exec_items[i].fetch_add(
                                    handle.items_done, std::memory_order_relaxed);
                                exec_bytes[i].fetch_add(
                                    handle.bytes_done, std::memory_order_relaxed);
                                exec_bytes_read[i].fetch_add(
                                    handle.items_done * invocation.traits.bytes_read_per_item,
                                    std::memory_order_relaxed);
                                exec_bytes_written[i].fetch_add(
                                    handle.items_done * invocation.traits.bytes_written_per_item,
                                    std::memory_order_relaxed);
                                exec_elapsed[i].fetch_add(
                                    handle.elapsed_ns, std::memory_order_relaxed);
                            } else {
                                pool.mark_failed(token, /*retryable=*/true);
                                exec_failed[i].fetch_add(1, std::memory_order_relaxed);
                            }
                        } else {
                            // Rejected（op 不支持/设备不可用）→ 终态失败；
                            // kernel 执行失败 → 重试（attempt 上限内）
                            if (r.error_message.empty() &&
                                !handle.error.empty()) {
                                r.error_message = handle.error;
                            }
                            const bool retryable =
                                (handle.status != SubmitStatus::Rejected) &&
                                (token.attempt < kMaxAttempts);
                            if (pool.mark_failed(token, retryable)) {
                                exec_failed[i].fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        // 释放该块的 pinned staging（真实记账闭环）
                        if (cfg.enable_memory_budget &&
                            exec->backend_type().rfind("cuda", 0) == 0) {
                            staging_ledger.release(
                                requested * invocation.traits.bytes_read_per_item +
                                requested * invocation.traits.bytes_written_per_item);
                        }
                    }
                });
            }
        }
        for (auto& th : workers) th.join();
        // ACR 架构冻结（05 号规范 §2）：工作保持——禁止漏算。
        // should_claim 的 makespan 优化在实测速率噪声/双方互斥拒绝时可能让
        // 所有 worker 停止 claim 而池仍有剩余（all_done=false 静默漏算）。
        // 兜底：worker 全部退出后，由第一个可用 executor 按剩余工作清尾，
        // 保证 completed==total 判据成立；不改变 Auto 生产路由语义。
        if (!pool.all_done() && pool.remaining_work() > 0) {
            for (std::size_t i = 0; i < n_exec; ++i) {
                DeviceExecutor* exec = supported[i];
                if (!exec->available()) continue;
                while (true) {
                    const std::size_t rem = pool.remaining_work();
                    if (rem == 0) break;
                    auto token = pool.claim_next_dynamic(exec->id(), rem);
                    if (!token.valid()) break;
                    stats_out.dynamic_chunk_sizes.push_back(token.size());
                    KernelInvocation inv2 = invocation;
                    inv2.domain = WorkDomain{token.begin, token.end};
                    inv2.token_id = token.id;
                    inv2.attempt = token.attempt;
                    inv2.input_resident = data_resident;
                    SubmitHandle h2 = exec->submit(token, inv2);
                    if (h2.status == SubmitStatus::Ok &&
                        pool.mark_done(token)) {
                        exec_done[i].fetch_add(1, std::memory_order_relaxed);
                        exec_items[i].fetch_add(
                            h2.items_done, std::memory_order_relaxed);
                        exec_bytes[i].fetch_add(
                            h2.bytes_done, std::memory_order_relaxed);
                        exec_bytes_read[i].fetch_add(
                            h2.items_done *
                                invocation.traits.bytes_read_per_item,
                            std::memory_order_relaxed);
                        exec_bytes_written[i].fetch_add(
                            h2.items_done *
                                invocation.traits.bytes_written_per_item,
                            std::memory_order_relaxed);
                        exec_elapsed[i].fetch_add(
                            h2.elapsed_ns, std::memory_order_relaxed);
                    } else {
                        pool.mark_failed(token, /*retryable=*/false);
                        exec_failed[i].fetch_add(
                            1, std::memory_order_relaxed);
                        break;  // 失败停止兜底（避免死循环）
                    }
                }
                if (pool.all_done()) break;
            }
        }
        for (auto* exec : supported) exec->sync();

        // 记录资源控制统计
        stats_out.batch_shrink_count = batch_shrink_count.load();
        stats_out.gate_close_count = gate.close_count.load();
        stats_out.gate_recover_count = gate.recover_count.load();
        if (gate.closed.load() && gate.closed_duration_ns() > kGateTimeoutNs) {
            stats_out.gate_aborted = true;
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
            per_exec_stats_out[i].bytes_read =
                exec_bytes_read[i].load(std::memory_order_relaxed);
            per_exec_stats_out[i].bytes_written =
                exec_bytes_written[i].load(std::memory_order_relaxed);
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
    impl_->mem_ctrl = std::make_unique<utilization::MemoryBudgetController>();
}
Dispatcher::~Dispatcher() = default;

void Dispatcher::set_cache_release_hook(std::function<std::size_t()> hook) {
    impl_->cache_release_hook = std::move(hook);
}

void Dispatcher::configure(const DispatcherConfig& cfg) {
    impl_->cfg = cfg;
    impl_->bdr_cache.clear();  // 配置变化 → 决策缓存失效
    impl_->executors = cfg.executors;  // F-fix 6 + F-fix 7
    // 聚焦版：OperationProfile 驱动规划（nullptr=保守 CPU fallback）
    impl_->planner.set_profile(cfg.operation_profile);
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

    if (impl_->mem_ctrl) {
        // 25 号计划 §7：MemoryBudget 配置由 DispatcherConfig 显式注入；
        // 未显式提供时使用默认配置（禁止 configure 内悄悄覆盖用户值）
        impl_->mem_ctrl->configure(cfg.memory_budget_explicit
                                       ? cfg.memory_budget
                                       : utilization::MemoryBudgetConfig{});
        for (const auto& bn : mcfg.gpu_backends) {
            impl_->mem_ctrl->register_backend(bn);
        }
        // 聚焦版 v2：pinned ledger limit 来自 MemoryBudget 配置
        const auto& mcfg_budget = cfg.memory_budget_explicit
            ? cfg.memory_budget : utilization::MemoryBudgetConfig{};
        const std::uint64_t total_ram = impl_->mem_ctrl->sample().total_ram;
        impl_->staging_ledger.configure(
            utilization::MemoryBudgetController::compute_limit(
                total_ram, mcfg_budget.pinned_ratio,
                mcfg_budget.pinned_fixed_reserve_bytes));
    }
}

bool Dispatcher::establish_input_residency(
    const KernelInvocation& invocation) {
    const InvocationBufferLayout layout = layout_for_invocation(invocation);
    // 注册 buffer（真实字节 + generation 同步），与 dispatch_invocation 一致
    for (std::size_t bi = 0; bi < invocation.buffers.bindings.size(); ++bi) {
        const auto& binding = invocation.buffers.bindings[bi];
        const std::string key = binding.stable_key.empty()
            ? "buf-" + std::to_string(
                           reinterpret_cast<std::uintptr_t>(binding.data))
            : binding.stable_key;
        const std::size_t bytes = binding.count * binding.element_size_bytes;
        const bool is_output =
            std::find(layout.outputs.begin(), layout.outputs.end(), bi) !=
            layout.outputs.end();
        const bool is_read_write =
            binding.role == BufferRole::ReadWrite ||
            (is_output && invocation.partition ==
                              PartitionKind::PrivatePartialThenMerge);
        const BufferAccess access = is_read_write
            ? BufferAccess::ReadWrite
            : (is_output ? BufferAccess::Write : BufferAccess::Read);
        // ACR 基座收尾（02_GENERATION_COHERENCE.md）：同 stable_key 但
        // generation 变化 → 先失效 executor 驻留视图（同 host 指针原地修改
        // 时 CUDA executor 可能仍按指针缓存 device view），再更新 manager。
        const std::uint64_t old_gen =
            impl_->residency.recorded_generation(key);
        const bool had_device_copy =
            impl_->residency.is_device_valid(key, "cuda:0");
        impl_->residency.register_or_update(key, bytes, access,
                                            binding.generation);
        if (binding.generation != old_gen && had_device_copy &&
            impl_->executors) {
            for (auto* e : impl_->executors->available_executors()) {
                if (e->backend_type().rfind("cuda", 0) == 0) {
                    e->invalidate_input(binding.data);
                }
            }
        }
    }
    // 收集尚未驻留的只读输入
    std::vector<const void*> upload_hosts;
    std::vector<std::size_t> upload_bytes;
    std::vector<std::string> upload_keys;
    for (std::size_t idx : layout.inputs) {
        const auto* b = invocation.buffers.find(idx);
        if (b == nullptr) continue;
        const std::string key = b->stable_key.empty()
            ? "buf-" + std::to_string(
                           reinterpret_cast<std::uintptr_t>(b->data))
            : b->stable_key;
        if (impl_->residency.is_device_valid(key, "cuda:0")) continue;
        upload_hosts.push_back(b->data);
        upload_bytes.push_back(b->count * b->element_size_bytes);
        upload_keys.push_back(key);
    }
    if (upload_hosts.empty()) return true;
    if (!impl_->executors) return false;
    DeviceExecutor* gpu = nullptr;
    for (auto* e : impl_->executors->available_executors()) {
        if (e->backend_type().rfind("cuda", 0) == 0) {
            gpu = e;
            break;
        }
    }
    if (gpu == nullptr || !gpu->prefetch_inputs(upload_hosts, upload_bytes)) {
        return false;
    }
    for (std::size_t i = 0; i < upload_keys.size(); ++i) {
        impl_->residency.mark_uploaded(upload_keys[i]);
        impl_->residency.mark_device_allocated(upload_keys[i]);
    }
    return true;
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

    // 26 号计划 §2：执行前检查内存预算（独立于利用率控制）
    utilization::MemoryBudgetController::ExceedAction mem_action =
        utilization::MemoryBudgetController::ExceedAction::None;
    if (impl_->cfg.enable_memory_budget && impl_->mem_ctrl) {
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

            // 中间采样（仅内存预算）
            if (impl_->cfg.enable_memory_budget && impl_->mem_ctrl) {
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
                fn, user_data, result.resource_control, &task.traits);
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

        result.current_state_json = impl_->current_state.status_json();
        return result;
    }

    // F-fix 3：Cost-aware 路径也通过动态 guided SharedWorkPool 执行
    // CostEstimate 已影响 chunk_size（作为 max_chunk），动态 guided 根据剩余工作收缩
    std::size_t min_chunk = impl_->cfg.min_effective_chunk;
    std::size_t max_chunk = chunk_size;
    auto r = impl_->execute_via_pool_dynamic(
        begin, end, min_chunk, max_chunk,
        fn, user_data, result.resource_control, &task.traits);
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
    if (result.actual_execution_shape == "gpu_direct") {
        // GPU Direct 不经过 SharedWorkPool：coverage 直接为整域 1 块
        result.coverage.total = (r.executed_on_gpu > 0) ? 1 : 0;
        result.coverage.done = (r.executed_on_gpu > 0) ? 1 : 0;
    } else {
        impl_->import_pool_coverage();
        result.coverage = Impl::coverage_from_pool(impl_->pool);
    }

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

    // GPU Direct 快路径：跳过完整状态 JSON 序列化（避免亚毫秒任务的固定开销）
    if (result.actual_execution_shape != "gpu_direct") {
        result.current_state_json = impl_->current_state.status_json();
    }
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

    const std::size_t begin = invocation.domain.begin;
    const std::size_t end = invocation.domain.end;
    (void)task;

    const InvocationBufferLayout layout = layout_for_invocation(invocation);

    // ===== Dispatcher Finalization：真实 Residency + 真实字节（04 号契约）=====
    // 1. 注册 buffer：真实字节 = count*element_size_bytes（禁止固定 sizeof(float)），
    //    并同步外部 binding generation（generation 变化自动失效设备副本）。
    for (std::size_t bi = 0; bi < invocation.buffers.bindings.size(); ++bi) {
        const auto& binding = invocation.buffers.bindings[bi];
        const std::string key = binding.stable_key.empty()
            ? "buf-" +
                  std::to_string(
                      reinterpret_cast<std::uintptr_t>(binding.data))
            : binding.stable_key;
        const std::size_t bytes = binding.count * binding.element_size_bytes;
        const bool is_output =
            std::find(layout.outputs.begin(), layout.outputs.end(), bi) !=
            layout.outputs.end();
        const bool is_read_write =
            binding.role == BufferRole::ReadWrite ||
            (is_output && invocation.partition ==
                              PartitionKind::PrivatePartialThenMerge);
        const BufferAccess access = is_read_write
            ? BufferAccess::ReadWrite
            : (is_output ? BufferAccess::Write : BufferAccess::Read);
        // ACR 基座收尾（02_GENERATION_COHERENCE.md）：同 stable_key 但
        // generation 变化 → 先失效 executor 驻留视图（同 host 指针原地修改
        // 时 CUDA executor 可能仍按指针缓存 device view），再更新 manager。
        const std::uint64_t old_gen =
            impl_->residency.recorded_generation(key);
        const bool had_device_copy =
            impl_->residency.is_device_valid(key, "cuda:0");
        impl_->residency.register_or_update(key, bytes, access,
                                            binding.generation);
        if (binding.generation != old_gen && had_device_copy &&
            impl_->executors) {
            for (auto* e : impl_->executors->available_executors()) {
                if (e->backend_type().rfind("cuda", 0) == 0) {
                    e->invalidate_input(binding.data);
                }
            }
        }
    }

    // 2. 查询真实 device-valid（BDR 事实来源；invocation.input_resident 仅兼容 hint）。
    std::vector<const void*> input_hosts;
    std::vector<std::size_t> input_bytes;
    std::vector<std::string> input_keys;
    std::vector<bool> was_resident;
    std::uint64_t total_input_bytes = 0;
    std::uint64_t resident_input_bytes = 0;
    std::uint64_t upload_required_bytes = 0;
    for (std::size_t idx : layout.inputs) {
        const auto* b = invocation.buffers.find(idx);
        if (b == nullptr) continue;
        const std::string key = b->stable_key.empty()
            ? "buf-" + std::to_string(
                           reinterpret_cast<std::uintptr_t>(b->data))
            : b->stable_key;
        const std::size_t bytes = b->count * b->element_size_bytes;
        const bool valid = impl_->residency.is_device_valid(key, "cuda:0");
        input_hosts.push_back(b->data);
        input_bytes.push_back(bytes);
        input_keys.push_back(key);
        was_resident.push_back(valid);
        total_input_bytes += bytes;
        if (valid) {
            resident_input_bytes += bytes;
        } else {
            upload_required_bytes += bytes;
        }
    }
    const bool data_resident =
        !input_keys.empty() &&
        std::all_of(was_resident.begin(), was_resident.end(),
                    [](bool v) { return v; });
    // 04 号规范 §4：reuse>1 时仅要求持久主输入（如 frames）真实驻留即可走
    // resident_reuse4 场景；weights 等小可变输入按 generation 更新不影响场景。
    bool persistent_resident = false;
    const KernelRegistration* reg =
        global_kernel_registry().find(invocation.id);
    if (reg != nullptr && !reg->persistent_input_indices.empty()) {
        bool all_persistent_ok = true;
        for (std::size_t pidx : reg->persistent_input_indices) {
            const auto* b = invocation.buffers.find(pidx);
            if (b == nullptr) {
                all_persistent_ok = false;
                break;
            }
            const std::string key = b->stable_key.empty()
                ? "buf-" + std::to_string(
                               reinterpret_cast<std::uintptr_t>(b->data))
                : b->stable_key;
            if (!impl_->residency.is_device_valid(key, "cuda:0")) {
                all_persistent_ok = false;
                break;
            }
        }
        persistent_resident = all_persistent_ok;
    }
    const bool route_resident =
        data_resident ||
        (invocation.reuse_count_hint > 1 && persistent_resident);
    std::uint64_t output_bytes = 0;
    for (const auto& b : invocation.buffers.bindings) {
        if (b.role == BufferRole::Output) {
            output_bytes += b.count * b.element_size_bytes;
        }
    }

    // 3. BDR 顶层决策（基于真实驻留 + 真实字节；场景由 reuse_count_hint 与
    //    主要输入实际驻留推导；input_resident 不再作为生产事实）。
    routing::RouteDecision bdr_decision;
    bool bdr_active = false;
    if (impl_->cfg.route_mode == RouteMode::AutoMixed &&
        impl_->cfg.route_profile_v2 != nullptr) {
        routing::RouteRequest req;
        req.operation_id = std::string(invocation.id);
        req.output_items = end - begin;
        req.frame_count =
            invocation.frame_count > 0 ? invocation.frame_count : 1u;
        req.input_bytes = total_input_bytes;
        req.output_bytes = output_bytes;
        req.input_residency = route_resident
            ? routing::InputResidency::DeviceResident
            : routing::InputResidency::HostOnly;
        req.output_policy =
            invocation.residency_policy == ResidencyPolicy::KeepDevice
                ? routing::OutputMaterialization::KeepDevice
                : routing::OutputMaterialization::HostRequired;
        req.reuse_count_hint = invocation.reuse_count_hint;
        // ACR 基座收尾（03_RESOURCE_AND_FALLBACK.md）：BDR 决策前真实采样
        // RAM/VRAM 容量与最小 queue 状态；禁止传空快照绕过门禁。
        routing::MemorySnapshot mem_snap;
        routing::QueueSnapshot queue_snap;
        if (impl_->mem_ctrl || impl_->cfg.memory_sampler_override) {
            const auto mb = impl_->cfg.memory_sampler_override
                ? impl_->cfg.memory_sampler_override()
                : impl_->mem_ctrl->sample();
            if (mb.ram_valid && mb.limit_ram > mb.used_ram) {
                mem_snap.ram_available_bytes =
                    mb.limit_ram - mb.used_ram;
            }
            for (const auto& g : mb.gpus) {
                if (g.backend == "cuda:0" && g.valid &&
                    g.limit_vram > g.used_vram) {
                    mem_snap.vram_available_bytes =
                        g.limit_vram - g.used_vram;
                }
            }
        }
        if (impl_->executors) {
            for (auto* e : impl_->executors->available_executors()) {
                if (e->backend_type().rfind("cuda", 0) == 0) {
                    const auto qs = e->queue_state();
                    if (qs.depth > 0 || qs.busy) {
                        // 保守等待罚分：每在队块 1ms（不做精确利用率控制）
                        queue_snap.gpu_delay_ms =
                            std::min<std::size_t>(qs.depth, 8) * 1.0;
                    }
                }
            }
        }
        req.queues = queue_snap;
        req.memory = mem_snap;
        req.upload_required_bytes = upload_required_bytes;
        routing::BenchmarkRouteEstimator est;
        est.set_profile(impl_->cfg.route_profile_v2);
        // 稳态决策缓存：仅 queue 空闲且 VRAM 充足（或 headroom 未知=0 表示不
        // 限制）时命中；此时决策不依赖快照具体数值，缓存结果与实时 decide 一致。
        // queue busy / VRAM 不足时旁路缓存（决策依赖快照）。
        const std::uint64_t incremental_vram =
            upload_required_bytes + output_bytes + output_bytes;
        const bool vram_headroom_ok =
            mem_snap.vram_available_bytes == 0 ||
            incremental_vram <= mem_snap.vram_available_bytes;
        const bool cacheable =
            req.queues.cpu_delay_ms == 0.0 &&
            req.queues.gpu_delay_ms == 0.0 && vram_headroom_ok;
        Impl::BdrCacheKey ck;
        ck.operation_id = req.operation_id;
        ck.output_items = req.output_items;
        ck.frame_count = req.frame_count;
        ck.input_residency =
            req.input_residency == routing::InputResidency::DeviceResident
                ? 1u
                : 0u;
        ck.output_policy =
            req.output_policy == routing::OutputMaterialization::KeepDevice
                ? 1u
                : 0u;
        ck.reuse_count_hint = req.reuse_count_hint;
        if (cacheable) {
            const auto cit = impl_->bdr_cache.find(ck);
            if (cit != impl_->bdr_cache.end()) {
                bdr_decision = cit->second;
            } else {
                bdr_decision = est.decide(req, /*diagnostic=*/false);
                if (impl_->bdr_cache.size() >= Impl::kBdrCacheMax) {
                    impl_->bdr_cache.clear();
                }
                impl_->bdr_cache.emplace(ck, bdr_decision);
            }
        } else {
            bdr_decision = est.decide(req, /*diagnostic=*/false);
        }
        bdr_active = true;
        result.benchmark_route_decision =
            bdr_decision.chosen == routing::RouteKind::OpenMP
                ? "openmp"
                : bdr_decision.chosen == routing::RouteKind::GpuDirect
                      ? "gpu_direct"
                      : "mixed";
        result.benchmark_route_reason = bdr_decision.reason;
        result.benchmark_cpu_chunk_items = bdr_decision.cpu_chunk_items;
        result.benchmark_gpu_chunk_items = bdr_decision.gpu_chunk_items;
        result.benchmark_predicted_ms =
            bdr_decision.chosen == routing::RouteKind::OpenMP
                ? bdr_decision.openmp.predicted_ms
                : bdr_decision.chosen == routing::RouteKind::GpuDirect
                      ? bdr_decision.gpu_direct.predicted_ms
                      : bdr_decision.mixed.predicted_ms;
        result.benchmark_input_residency =
            route_resident ? "resident" : "cold";
        result.benchmark_resident_input_bytes = resident_input_bytes;
        result.benchmark_upload_required_bytes = upload_required_bytes;
    }

    // 4. 执行：OpenMP → legacy direct；GPU Direct → execute_gpu_direct；
    //    Mixed/非 BDR → prefetch + SharedWorkPool（现有工作保持池）。
    std::vector<Impl::InvocationExecStats> per_exec_stats;
    std::vector<std::string> actual_devices;
    std::uint64_t h2d_bytes_this = 0;
    const auto slot_counts = [&]() -> std::pair<std::uint64_t, std::uint64_t> {
        if (!impl_->executors) return {0, 0};
        auto* cuda = impl_->executors->find("cuda:0");
        if (cuda == nullptr) return {0, 0};
        return {cuda->slot_upload_count(0), cuda->slot_upload_count(1)};
    };
    MixedRunResult r;
    r.total_chunks = 0;
    r.all_done = false;

    if (bdr_active && bdr_decision.chosen == routing::RouteKind::OpenMP) {
        // OpenMP：注册的 legacy_parallel launcher 直接执行完整域（不拆块）
        const KernelRegistration* reg =
            global_kernel_registry().find(invocation.id);
        if (reg != nullptr && reg->legacy_parallel != nullptr) {
            const std::size_t n = end - begin;
            auto t0 = std::chrono::steady_clock::now();
            reg->legacy_parallel(invocation, nullptr);
            const auto t1 = std::chrono::steady_clock::now();
            r.all_done = true;
            r.total_chunks = 1;
            r.executed_on_cpu = 1;
            r.executed_on_gpu = 0;
            actual_devices = {"cpu"};
            Impl::InvocationExecStats st;
            st.device_id = "cpu";
            st.backend_type = "cpu";
            st.done_blocks = 1;
            st.items_done = n;
            st.bytes_read = 0;
            st.bytes_written = 0;
            st.elapsed_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    t1 - t0)
                    .count());
            per_exec_stats.push_back(std::move(st));
            result.actual_execution_shape = "legacy_openmp";
        } else {
            r.error_message =
                "openmp route selected but no legacy_parallel launcher: " +
                std::string(invocation.id);
            result.actual_execution_shape = "legacy_openmp";
        }
    } else if (bdr_active &&
               bdr_decision.chosen == routing::RouteKind::GpuDirect) {
        // 真正 GPU Direct：prefetch + 单 GPU executor，绕开 SharedWorkPool
        auto gd = impl_->execute_gpu_direct(
            invocation, layout, input_hosts, input_bytes, input_keys,
            was_resident);
        if (!gd.run_result.all_done) {
            // ACR 基座收尾（03_RESOURCE_AND_FALLBACK.md）：Auto 模式下
            // GPU Direct prefetch/submit 失败 → 不提交部分结果，完整域
            // Legacy OpenMP 重算；报告真实 fallback。
            result.benchmark_fallback = true;
            result.benchmark_fallback_reason =
                gd.run_result.error_message;
            result.actual_execution_shape = "legacy_openmp";
            const KernelRegistration* reg =
                global_kernel_registry().find(invocation.id);
            if (reg != nullptr && reg->legacy_parallel != nullptr) {
                const std::size_t n = end - begin;
                auto t0 = std::chrono::steady_clock::now();
                reg->legacy_parallel(invocation, nullptr);
                const auto t1 = std::chrono::steady_clock::now();
                r.all_done = true;
                r.total_chunks = 1;
                r.executed_on_cpu = 1;
                r.executed_on_gpu = 0;
                actual_devices = {"cpu"};
                Impl::InvocationExecStats st;
                st.device_id = "cpu";
                st.backend_type = "cpu";
                st.done_blocks = 1;
                st.items_done = n;
                st.bytes_read = 0;
                st.bytes_written = 0;
                st.elapsed_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count());
                per_exec_stats.push_back(std::move(st));
            } else {
                r.error_message =
                    "gpu direct failed, no legacy fallback: " +
                    gd.run_result.error_message;
                result.actual_execution_shape = "gpu_direct";
            }
        } else {
            r = gd.run_result;
            per_exec_stats = std::move(gd.per_exec_stats);
            actual_devices = std::move(gd.actual_devices);
            h2d_bytes_this = gd.h2d_bytes_this;
            result.actual_execution_shape = "gpu_direct";
        }
    } else {
        // Mixed（或 CpuOnly/GpuOnly/无 BDR）：真实 prefetch + SharedWorkPool
        result.actual_execution_shape =
            bdr_active ? "mixed_pool" : "legacy_openmp";
        if (!input_hosts.empty() && impl_->executors) {
            for (auto* e : impl_->executors->available_executors()) {
                if (e->backend_type().rfind("cuda", 0) == 0 &&
                    e->prefetch_inputs(input_hosts, input_bytes)) {
                    for (std::size_t i = 0; i < input_keys.size(); ++i) {
                        if (!was_resident[i]) {
                            h2d_bytes_this += input_bytes[i];
                            impl_->residency.mark_uploaded(input_keys[i]);
                            impl_->residency.mark_device_allocated(
                                input_keys[i]);
                        }
                    }
                    break;
                }
            }
        }
        r = impl_->execute_invocation_via_executors(
            invocation, estimate, result.resource_control, per_exec_stats,
            actual_devices, data_resident || h2d_bytes_this > 0,
            bdr_active ? &bdr_decision : nullptr);
    }
    result.run_result = r;
    result.actual_devices_used = actual_devices;
    // ACR 架构冻结（07 号计划 B）：真实传输统计。
    // h2d：本 dispatch 组合 prefetch 实际上传字节（仅新上传输入）；
    // d2h：每 GPU 块同步物化一次输出范围（同步桥接语义）。
    result.transfer_stats.h2d_count = (h2d_bytes_this > 0) ? 1 : 0;
    result.transfer_stats.h2d_bytes = h2d_bytes_this;
    result.transfer_stats.d2h_count = r.executed_on_gpu;
    const auto slot_after = slot_counts();
    result.transfer_stats.frames_upload_count = slot_after.first;
    result.transfer_stats.weights_upload_count = slot_after.second;

    // 聚焦版 v3：真实驻留驱动。
    //  - 输入已在 worker 启动前 prefetch（真实一次上传）；
    //  - 输出经同步桥接真实 D2H 一次；
    //  - 不再执行后补传输入。
    if (r.executed_on_gpu > 0) {
        const auto* output = !layout.outputs.empty()
            ? invocation.buffers.find(layout.outputs.front())
            : nullptr;
        if (output != nullptr) {
            // 同步桥接执行完成后输出已 D2H（真实下载一次）
            const std::string key = output->stable_key.empty()
                ? "buf-" + std::to_string(
                                reinterpret_cast<std::uintptr_t>(output->data))
                : output->stable_key;
            impl_->residency.mark_downloaded(key);
        }
    }

    // 24 号计划 §3：actual_primary 按每设备真实 items_done 最大者确定；
    // 相同则比较实际处理字节，再相同比较有效执行时间。禁止用
    // actual_devices.front() 或 executor 顺序代替统计结果。
    {
        const Impl::InvocationExecStats* best = nullptr;
        for (const auto& s : per_exec_stats) {
            if (s.items_done == 0 && s.done_blocks == 0) continue;
            if (best == nullptr) { best = &s; continue; }
            if (s.items_done > best->items_done) {
                best = &s;
            } else if (s.items_done == best->items_done) {
                const std::size_t s_bytes = s.bytes_read + s.bytes_written;
                const std::size_t b_bytes = best->bytes_read + best->bytes_written;
                if (s_bytes > b_bytes) {
                    best = &s;
                } else if (s_bytes == b_bytes && s.elapsed_ns > best->elapsed_ns) {
                    best = &s;
                }
            }
        }
        if (best == nullptr) {
            result.actual_primary_backend = "none";
        } else if (best->backend_type.rfind("cuda", 0) == 0) {
            result.actual_primary_backend = best->device_id;  // 如 "cuda:0"
        } else {
            result.actual_primary_backend = best->backend_type;  // "cpu"
        }
    }

    // 24 号计划 §3：输出完整 per-device 真实统计
    result.per_device_stats.clear();
    for (const auto& s : per_exec_stats) {
        CostAwareResult::PerDeviceStats pds;
        pds.device_id = s.device_id;
        pds.backend = s.backend_type;
        pds.items_done = s.items_done;
        pds.bytes_read = s.bytes_read;
        pds.bytes_written = s.bytes_written;
        pds.blocks_done = s.done_blocks;
        pds.active_duration_ns = s.elapsed_ns;
        pds.error_count = s.failed_blocks;
        result.per_device_stats.push_back(pds);
    }
    // ACR 架构冻结（07 号计划 B）：GPU 输出物化字节 = GPU items × 输出每项字节
    // （同步桥接每块 D2H 其拥有范围；输出元素为 float）。
    {
        std::uint64_t gpu_items = 0;
        for (const auto& pds : result.per_device_stats) {
            if (pds.backend.rfind("cuda", 0) == 0) {
                gpu_items += pds.items_done;
            }
        }
        const auto* out_buf = !layout.outputs.empty()
            ? invocation.buffers.find(layout.outputs.front()) : nullptr;
        std::size_t out_bytes_per_item = out_buf != nullptr
            ? out_buf->element_size_bytes
            : sizeof(float);
        if (out_buf != nullptr && invocation.domain.size() > 0 &&
            out_buf->count / invocation.domain.size() > 0) {
            out_bytes_per_item *=
                out_buf->count / invocation.domain.size();
        }
        result.transfer_stats.d2h_bytes =
            gpu_items * out_bytes_per_item;
    }

    result.total_chunks = r.total_chunks;
    result.chunks_on_cpu = r.executed_on_cpu;
    result.chunks_on_gpu = r.executed_on_gpu;
    result.chunks_fallback = r.fallback_chunks;

    // ACR 基座收尾（04_EVIDENCE_TRUTH.md）：Direct 路径（gpu_direct 或
    // legacy_openmp）不读 SharedWorkPool（可能为 stale），coverage 直接用
    // 完整域语义；仅 mixed_pool 才从真实 pool 导入。
    if (result.actual_execution_shape == "gpu_direct" ||
        result.actual_execution_shape == "legacy_openmp") {
        result.coverage.total = 1;
        result.coverage.claimed = r.all_done ? 1 : 0;
        result.coverage.done = r.all_done ? 1 : 0;
        result.coverage.pending = r.all_done ? 0 : 1;
        result.coverage.failed = 0;
    } else {
        impl_->import_pool_coverage();
        result.coverage = Impl::coverage_from_pool(impl_->pool);
    }

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