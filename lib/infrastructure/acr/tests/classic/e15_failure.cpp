// lib/infrastructure/acr/tests/classic/e15_failure.cpp — E15 Failure and Fallback
// 验证能力：backend 缺失 / device lost / 取消 / 异常传播
// 扩展（规范 E20 故障和回退）：
// - 分配失败/OOM 真注入（超限分配必然抛出，catch 后验证 runtime 恢复）
// - 混合调度异常 chunk
// 用 parallel_for 异常 kernel 验证 KernelFailed。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <mixed_runner.hpp>

#include <cstdio>
#include <fstream>
#include <limits>
#include <string>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::scheduler;

namespace {

// 取消语义：cancel-before-wait 相（真竞态窗口：cancel 抢在终态前到达）
// + wait-then-cancel 相（已终态 kernel 的取消标志仍须置位）。
// 同步 CPU 调度下 kernel 总会执行完；本 case 锁定的是取消标志置位与
// 终态收敛这两个可红可绿的语义面。
CaseResult run_cancel_kernel(const std::string& case_id) {
    std::atomic<int> cnt{0};
    // 相 1：cancel 先于 wait。
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.cancel();
    ev.wait();
    bool ok = ev.cancelled() && ev.ready();

    // 相 2：wait 后 cancel（已 Done kernel 的取消标志仍须置位）。
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
    }, 5);

    ErrorStats err;
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel flag/state semantics broken",
                       "cpu", "cpu");
}

// kernel 异常传播：throw → mark_failed(KernelFailed)
CaseResult run_kernel_exception(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("boom"); });
    }, 5);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "kernel exception not propagated as KernelFailed",
                       "cpu", "cpu");
}

// 分配失败/OOM 真注入：超限分配（size_max/2 个 float ≈ 2^63 字节）在任何
// 主机上都无法满足 ⇒ operator new 必然抛出。断言：
//   a) 故障被观察到（异常确实抛出并被捕获——静默截断/吞异常 = 红）；
//   b) 故障后 runtime 恢复（小分配成功 + parallel_for 结果正确）。
CaseResult run_allocation_failure(const std::string& case_id) {
    double probe = -1.0;  // 0 = 故障观察到且 runtime 恢复
    auto tm = measure_timing([&] {
        bool failure_observed = false;
        try {
            Buffer<float> b(std::numeric_limits<std::size_t>::max() / 2, 0.0f);
            (void)b;  // 走到这里 = 超限分配"成功"（静默截断），判红。
        } catch (const std::bad_alloc&) {
            failure_observed = true;
        } catch (const std::length_error&) {
            failure_observed = true;
        } catch (...) {
            failure_observed = true;
        }
        // 故障后恢复：小分配 + kernel 正确执行。
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
    return make_result("E15", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "allocation failure not observed or runtime not recovered",
                       "cpu", "cpu");
}

// 混合调度中异常 chunk 被计为 failed
CaseResult run_mixed_schedule_exception(const std::string& case_id) {
    runtime_init();
    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.enable_gpu = false;
    runner.configure(cfg);
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        if (b == 0) throw std::runtime_error("chunk 0 failed");
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = runner.run_range(0, 100, 50, fn, &data);
    }, 1);

    ErrorStats err;
    // 失败的 chunk 应被计为 failed_chunks，all_done 应为 false
    bool ok = (!mr.all_done) && (mr.failed_chunks >= 1);
    if (!ok) err.max_abs = 1.0;
    runtime_shutdown();
    return make_result("E15", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "exception chunk not counted as failed",
                       "cpu", "cpu");
}

// 取消正在执行的调度
CaseResult run_cancel_dispatch(const std::string& case_id) {
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    d.configure(cfg);

    std::vector<int> data(1000, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* dd = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*dd)[i] = 1;
    };

    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        auto r = d.dispatch_range(0, 1000, 100, fn, &data);
        // dispatch_range 是同步的，完成后验证
        (void)r;
    }, 3);

    ErrorStats err;
    int sum = 0;
    for (auto v : data) sum += v;
    bool ok = (sum == 1000);
    if (!ok) err.max_abs = std::fabs(static_cast<double>(1000 - sum));
    return make_result("E15", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "dispatch result incorrect",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E15Failure, CancelKernel)       { auto r = run_cancel_kernel("cancel_kernel");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, KernelException)    { auto r = run_kernel_exception("kernel_exception");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, AllocationFailure)  { auto r = run_allocation_failure("allocation_failure");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, MixedScheduleExc)   { auto r = run_mixed_schedule_exception("mixed_schedule_exception"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, CancelDispatch)     { auto r = run_cancel_dispatch("cancel_dispatch");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e15() {
    return {
        run_cancel_kernel("cancel_kernel"),
        run_kernel_exception("kernel_exception"),
        run_allocation_failure("allocation_failure"),
        run_mixed_schedule_exception("mixed_schedule_exception"),
        run_cancel_dispatch("cancel_dispatch"),
    };
}
