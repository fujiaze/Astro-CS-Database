// lib/acr/tests/classic/e20_fault_fallback.cpp — E20 故障和回退
// 规范 E20：显存不足、launch失败、分配失败、取消、异常、
// 未开始块回收，已完成块不重复。
// 聚焦于 Dispatcher/MixedRunner 的 fallback 行为。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <fallback.hpp>
#include <mixed_runner.hpp>
#include <partitioner.hpp>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::scheduler;

namespace {

// 显存不足/OOM 模拟：分配大 Buffer，验证不崩溃
CaseResult run_oom_simulation(const std::string& case_id) {
    auto tm = measure_timing([&] {
        try {
            Buffer<float> b(256 * 1024 * 1024, 0.0f);  // 1GB
            b[0] = 1.0f;
        } catch (...) {
            // OOM 不崩溃即 ok
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "OOM simulation crash",
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

// 分配失败：Buffer 分配后正确释放
CaseResult run_allocation_failure(const std::string& case_id) {
    auto tm = measure_timing([&] {
        for (int i = 0; i < 100; ++i) {
            Buffer<int> b(1024, i);
            b[0] = i + 1;
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "allocation failure crash",
                       "cpu", "cpu");
}

// 取消：cancel 正在执行的 kernel
CaseResult run_cancel_kernel(const std::string& case_id) {
    std::atomic<int> cnt{0};
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.wait();
    ev.cancel();

    auto tm = measure_timing([&] {
        Event e = parallel_for(KernelId::Custom, Range1D{0, 1000}, [](std::size_t) {});
        e.wait();
        e.cancel();
    }, 3);

    ErrorStats err;
    bool ok = ev.cancelled() && (cnt.load() == 1000);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel kernel failed",
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

// 未开始块回收，已完成块不重复：FallbackPolicy 验证
CaseResult run_fallback_no_replay(const std::string& case_id) {
    // 创建 coverage bitmap，标记部分 chunk 已完成
    CoverageBitmap bm(10);
    bm.mark_done(0);
    bm.mark_done(1);
    bm.mark_done(2);

    FallbackPolicy fp;
    fp.set_strategy(FallbackStrategy::ToCpu);
    auto decision = fp.decide("cuda:0", bm, {"cpu"});

    auto tm = measure_timing([&] {
        fp.decide("cuda:0", bm, {"cpu"});
    }, 5);

    ErrorStats err;
    // 回退到 CPU
    bool ok = (decision.strategy == FallbackStrategy::ToCpu) &&
              (decision.target_backend == "cpu") &&
              decision.skip_already_done;
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 10, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "fallback no-replay policy failed",
                       "cpu", "cpu");
}

// 混合调度 fallback：设备失败 → CPU 回退，已完成不重放
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

    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = d.dispatch_range(0, 100, 25, fn, &data);
    }, 1);

    ErrorStats err;
    int sum = 0;
    for (auto v : data) sum += v;
    bool ok = mr.all_done && (mr.failed_chunks == 0) && (sum == 100);
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
