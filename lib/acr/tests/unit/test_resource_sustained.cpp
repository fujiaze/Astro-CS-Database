// lib/acr/tests/unit/test_resource_sustained.cpp — 持续负载资源闭环验证
//
// 23 号计划 §6/F-fix 10：CPU 目标 50/80/95/100% 每档持续至少 30 秒，
// 使用真实 CpuController 采样（GetSystemTimes）+ 真实控制动作，
// 报告平均/P95/最大偏差/控制动作序列/worker 参与。
// 本测试约 2 分钟（4 档 × 30s），在证据阶段执行。
//
// 2026-08-05 决策：利用率目标闭环（稳态误差 ≤0.05）不作为本轮验收门禁。
// Windows 没有可直接调用的系统 API 软限制进程 CPU 占用率（Job Object 只有
// 硬配额且影响整个 Job），因此 50/80/95/100 仅作“真实采样 + 可调并发许可 +
// 控制动作”的报告型验证，不承诺目标达标；avg/p95/偏差如实输出。
#include <gtest/gtest.h>

#include "dispatcher.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::scheduler;

namespace {

cost::CostEstimate make_cpu_only_estimate(std::size_t recommended_chunk) {
    cost::CostEstimate est;
    cost::DeviceCost dc;
    dc.device_id = kHwCpuDeviceId;
    dc.backend = "cpu";
    dc.recommended_chunk = recommended_chunk;
    dc.min_effective_chunk = 64;
    dc.feasible = true;
    dc.profile_available = true;
    dc.reason = "sustained-test";
    est.per_device.push_back(dc);
    est.preferred_device = kHwCpuDeviceId;
    est.profile_available = true;
    return est;
}

// CPU 忙循环 kernel（消耗真实 CPU，供利用率采样）
void busy_kernel(std::size_t /*idx*/, std::size_t b, std::size_t e, void* ud) {
    volatile std::uint64_t acc = 0;
    for (std::size_t i = b; i < e; ++i) {
        for (int k = 0; k < 4096; ++k) {
            acc += static_cast<std::uint64_t>(i) * 2654435761u;
        }
    }
    auto* sink = static_cast<std::atomic<std::uint64_t>*>(ud);
    sink->fetch_add(acc, std::memory_order_relaxed);
}

double p95(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    std::vector<double> s = v;
    std::sort(s.begin(), s.end());
    return s[static_cast<std::size_t>(s.size() * 0.95)];
}

std::string summarize(double target, const ResourceControlStats& rc) {
    const auto& samples = rc.cpu_actual_samples;
    double avg = 0.0;
    double max_err = 0.0;
    for (double s : samples) {
        avg += s;
        max_err = std::max(max_err, std::fabs(s - target));
    }
    if (!samples.empty()) avg /= static_cast<double>(samples.size());
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "target=%.2f samples=%zu avg=%.4f p95=%.4f max_err=%.4f "
        "gate_close=%zu gate_recover=%zu workers_registered=%u actions=%zu "
        "gate_aborted=%d all_done(last)=%d",
        target, samples.size(), avg, p95(samples), max_err,
        rc.gate_close_count, rc.gate_recover_count, rc.workers_registered,
        rc.control_actions.size(), rc.gate_aborted ? 1 : 0,
        static_cast<int>(true));
    return std::string(buf);
}

} // anonymous namespace

// ============================================================================
// CPU 目标 50/80/95/100% 每档持续 30 秒（真实采样 + 控制动作）
// ============================================================================
TEST(ResourceSustained, CpuTargetLevels30sEach) {
    const double targets[] = {0.50, 0.80, 0.95, 1.00};
    std::printf("[ResourceSustained] begin 4 levels x 30s\n");
    for (double target : targets) {
        runtime_init();
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}};
        cfg.fallback_strategy = FallbackStrategy::ToCpu;
        cfg.enable_utilization = true;
        cfg.control_window_ms = 100;
        cfg.cpu_target_ratio = target;
        d.configure(cfg);

        auto est = make_cpu_only_estimate(65536);
        std::atomic<std::uint64_t> sink{0};
        auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
            busy_kernel(0, b, e, ud);
        };
        // 固定规模：4096 iters/item × 16 线程 ≈ 2.8-4M items/s → 170M ≈ 42-60s
        // 每档持续 ≥30s；ctest 超时 240s 足够（4 档合计约 2.5-3 分钟）
        const std::size_t sustained_items = 170u << 20;

        TaskDescriptor task;
        task.range = Range1D{0, sustained_items};
        const auto t_start = std::chrono::steady_clock::now();
        auto r = d.dispatch_range_cost_aware(task, est, fn, &sink);
        ASSERT_TRUE(r.run_result.all_done) << r.run_result.error_message;
        const auto& rc = r.resource_control;
        ResourceControlStats total = rc;

        const double elapsed_sec =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
        const std::string summary = summarize(target, total);
        std::printf("[ResourceSustained] %s sustained_items=%zu elapsed=%.1fs\n",
                    summary.c_str(), sustained_items, elapsed_sec);
        // 有效采样必须非空、worker 参与、每档 ≥30s（报告型验收）
        EXPECT_GT(total.cpu_actual_samples.size(), 0u);
        EXPECT_GT(total.workers_registered, 0u);
        EXPECT_GE(elapsed_sec, 30.0);
        runtime_shutdown();
    }
    std::printf("[ResourceSustained] done\n");
}
