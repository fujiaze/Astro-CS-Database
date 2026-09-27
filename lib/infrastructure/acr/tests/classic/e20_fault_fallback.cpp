// lib/infrastructure/acr/tests/classic/e20_fault_fallback.cpp — E20 故障和回退
// 规范 E20：显存不足、launch失败、分配失败、取消、异常、
// 未开始块回收，已完成块不重复。
// 聚焦于 Dispatcher/MixedRunner 的 fallback 行为。
// 非退化判据（能红能绿）：
// - OOM/分配失败 = 超限分配真注入（异常必抛）+ 故障后 runtime 恢复验证；
// - fallback = enable_gpu 混合场景真发生（fallback_chunks 与占位回退路径一致）；
// - no-replay 决策 = 带正负两例（空 coverage 的 skip 必须为 false）。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <fallback.hpp>
#include <mixed_runner.hpp>
#include <partitioner.hpp>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::scheduler;

namespace {

// 显存不足/OOM 真注入：超限分配必然抛出；断言故障被观察到且故障后
// runtime 恢复（小分配 + kernel 正确）。静默截断/吞异常/崩溃 = 红。
CaseResult run_oom_simulation(const std::string& case_id) {
    double probe = -1.0;
    auto tm = measure_timing([&] {
        bool failure_observed = false;
        try {
            Buffer<float> b(std::numeric_limits<std::size_t>::max() / 2, 0.0f);
            (void)b;  // 走到这里 = 静默截断，判红。
        } catch (...) {
            failure_observed = true;
        }
        bool recovered = false;
        try {
            Buffer<float> small(1024, 0.0f);
            small[0] = 1.0f;
            std::atomic<int> sum{0};
            Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
                [&sum](std::size_t) { sum.fetch_add(1, std::memory_order_relaxed); });
            ev.wait();
            recovered = (small[0] == 1.0f) && (sum.load() == 100);
        } catch (...) {
            recovered = false;
        }
        probe = (failure_observed && recovered) ? 0.0 : 1.0;
    }, 1);

    ErrorStats err;
    bool ok = (probe == 0.0);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "OOM not observed or runtime not recovered",
                       "cpu", "cpu");
}

// launch 失败：kernel 异常 → KernelFailed
CaseResult run_launch_failure(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("launch failed"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("launch failed"); });
    }, 3);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "launch failure not propagated as KernelFailed",
                       "cpu", "cpu");
}

// 分配失败：正常分配内容校验 + 超限注入双相（非退化：两相都可红）。
CaseResult run_allocation_failure(const std::string& case_id) {
    double probe = -1.0;
    auto tm = measure_timing([&] {
        bool normal_ok = true;
        try {
            for (int i = 0; i < 100; ++i) {
                Buffer<int> b(1024, i);
                b[0] = i + 1;
                if (b[0] != i + 1) normal_ok = false;  // 内容校验
            }
        } catch (...) {
            normal_ok = false;
        }
        bool failure_observed = false;
        try {
            Buffer<int> b(std::numeric_limits<std::size_t>::max() / 2, 0);
            (void)b;  // 静默截断 = 判红。
        } catch (...) {
            failure_observed = true;
        }
        probe = (normal_ok && failure_observed) ? 0.0 : 1.0;
    }, 1);

    ErrorStats err;
    bool ok = (probe == 0.0);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "alloc content wrong or failure not observed",
                       "cpu", "cpu");
}

// 取消语义：cancel-before-wait 相 + wait-then-cancel 相（与 E15 同工艺）。
CaseResult run_cancel_kernel(const std::string& case_id) {
    std::atomic<int> cnt{0};
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.cancel();
    ev.wait();
    bool ok = ev.cancelled() && ev.ready();

    Event ev2 = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev2.wait();
    ev2.cancel();
    ok = ok && ev2.cancelled() && (cnt.load() == 2000);

    auto tm = measure_timing([&] {
        Event e = parallel_for(KernelId::Custom, Range1D{0, 1000}, [](std::size_t) {});
        e.cancel();
        e.wait();
        e.cancel();
    }, 3);

    ErrorStats err;
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel flag/state semantics broken",
                       "cpu", "cpu");
}

// 异常传播：throw → mark_failed
CaseResult run_exception_propagation(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("boom"); });
    }, 3);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "exception not propagated as KernelFailed",
                       "cpu", "cpu");
}

// 未开始块回收，已完成块不重复：FallbackPolicy 决策正负两例。
// 正例：3/10 chunk 已完成 → 决策 ToCpu/cpu 且 skip_already_done=true；
// 负例：空 coverage（0 完成）→ skip_already_done 必须为 false
// （无条件 skip = 判红）。
CaseResult run_fallback_no_replay(const std::string& case_id) {
    CoverageBitmap bm(10);
    bm.mark_done(0);
    bm.mark_done(1);
    bm.mark_done(2);

    FallbackPolicy fp;
    fp.set_strategy(FallbackStrategy::ToCpu);
    auto decision = fp.decide("cuda:0", bm, {"cpu"});

    CoverageBitmap bm_empty(10);
    auto decision_empty = fp.decide("cuda:0", bm_empty, {"cpu"});

    auto tm = measure_timing([&] {
        fp.decide("cuda:0", bm, {"cpu"});
    }, 5);

    ErrorStats err;
    bool ok = (decision.strategy == FallbackStrategy::ToCpu) &&
              (decision.target_backend == "cpu") &&
              decision.skip_already_done &&
              (!decision_empty.skip_already_done);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 10, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "fallback no-replay decision (pos/neg) failed",
                       "cpu", "cpu");
}

// 混合调度 fallback：enable_gpu=true 走占位回退路径（Phase H 前的
// GPU 请求 → CPU 回退实现），fallback_chunks 必须真发生。
CaseResult run_mixed_fallback_to_cpu(const std::string& case_id) {
    runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    d.configure(cfg);

    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    // 相 1：纯 CPU 路径（无回退发生）。
    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = d.dispatch_range(0, 100, 25, fn, &data);
    }, 1);
    int sum = 0;
    for (auto v : data) sum += v;
    bool ok = mr.all_done && (mr.failed_chunks == 0) && (sum == 100) &&
              (mr.fallback_chunks == 0);

    // 相 2：混合回退路径（MixedRunner enable_gpu=true → 占位回退）。
    MixedRunner runner;
    MixedRunnerConfig mcfg;
    mcfg.enable_gpu = true;
    mcfg.fallback_strategy = FallbackStrategy::ToCpu;
    runner.configure(mcfg);
    std::vector<int> data2(100, 0);
    auto fn2 = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    MixedRunResult mr2 = runner.run_range(0, 100, 25, fn2, &data2);
    int sum2 = 0;
    for (auto v : data2) sum2 += v;
    // 回退真发生（fallback_chunks==chunk 总数）且结果全对。
    ok = ok && mr2.all_done && (sum2 == 100) &&
         (mr2.fallback_chunks == 4) && (mr2.failed_chunks == 0);

    ErrorStats err;
    if (!ok) err.max_abs = 1.0;
    runtime_shutdown();
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "mixed fallback to CPU failed",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E20Fault, OomSimulation)     { auto r = run_oom_simulation("oom_simulation");           ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, LaunchFailure)     { auto r = run_launch_failure("launch_failure");           ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, AllocFailure)      { auto r = run_allocation_failure("allocation_failure");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, CancelKernel)      { auto r = run_cancel_kernel("cancel_kernel");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, ExceptionProp)     { auto r = run_exception_propagation("exception_propagation"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, FallbackNoReplay)  { auto r = run_fallback_no_replay("fallback_no_replay");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, MixedFallbackCpu)  { auto r = run_mixed_fallback_to_cpu("mixed_fallback_to_cpu"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e20() {
    return {
        run_oom_simulation("oom_simulation"),
        run_launch_failure("launch_failure"),
        run_allocation_failure("allocation_failure"),
        run_cancel_kernel("cancel_kernel"),
        run_exception_propagation("exception_propagation"),
        run_fallback_no_replay("fallback_no_replay"),
        run_mixed_fallback_to_cpu("mixed_fallback_to_cpu"),
    };
}
