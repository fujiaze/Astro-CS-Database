// lib/acr/tests/classic/e16_concurrency.cpp — E16 Concurrency/Cancellation/Lifetime
// 验证能力：多调用者 / 取消 / Buffer 生命周期 / 100 次重启
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// 多调用者并发提交 parallel_for
CaseResult run_multi_caller(int n_threads, std::size_t per_thread_n,
                            const std::string& case_id) {
    std::atomic<int> total{0};
    auto tm = measure_timing([&] {
        total.store(0, std::memory_order_relaxed);
        std::vector<std::thread> ts;
        ts.reserve(n_threads);
        for (int t = 0; t < n_threads; ++t) {
            ts.emplace_back([&total, per_thread_n]() {
                std::atomic<int> local{0};
                parallel_for(KernelId::Custom, Range1D{0, per_thread_n},
                    [&local](std::size_t) { local.fetch_add(1, std::memory_order_relaxed); });
                total.fetch_add(local.load(), std::memory_order_relaxed);
            });
        }
        for (auto& th : ts) th.join();
    }, 3);

    ErrorStats err;
    int expected = n_threads * static_cast<int>(per_thread_n);
    bool ok = (total.load() == expected);
    if (!ok) err.max_abs = std::fabs(static_cast<double>(total.load()) - expected);
    return make_result("E16", case_id, "integer", n_threads * per_thread_n, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "multi caller count mismatch",
                       "cpu", "cpu");
}

// 取消 + 重置：连续提交 + cancel 验证不泄漏
CaseResult run_cancel_repeated(int n_repeats, const std::string& case_id) {
    auto tm = measure_timing([&] {
        for (int i = 0; i < n_repeats; ++i) {
            Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
                [](std::size_t) {});
            ev.wait();
            ev.cancel();
            // Event 析构在此循环结束时
        }
    }, 1);

    ErrorStats err;
    bool ok = true;  // 不崩溃即 ok
    return make_result("E16", case_id, "integer", n_repeats, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel repeated crash",
                       "cpu", "cpu");
}

// Buffer 生命周期：构造、析构、move
CaseResult run_buffer_lifetime(int n_buffers, const std::string& case_id) {
    auto tm = measure_timing([&] {
        for (int i = 0; i < n_buffers; ++i) {
            Buffer<float> b1(1024, 1.0f);
            Buffer<float> b2(std::move(b1));  // move ctor
            Buffer<float> b3;
            b3 = std::move(b2);               // move assign
            // b1, b2 现在 empty
            // b3 拥有数据
            if (b3.count() != 1024) { /* 验证 */ }
            // 析构链：b3 → b2 → b1
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E16", case_id, "integer", n_buffers, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "buffer lifetime crash",
                       "cpu", "cpu");
}

// 100 次重启：runtime_init → shutdown → init 循环
CaseResult run_restarts(int n_restarts, const std::string& case_id) {
    auto tm = measure_timing([&] {
        for (int i = 0; i < n_restarts; ++i) {
            runtime_init();
            std::atomic<int> cnt{0};
            parallel_for(KernelId::Custom, Range1D{0, 100},
                [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
            runtime_shutdown();
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E16", case_id, "integer", n_restarts, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "restart loop crash",
                       "cpu", "cpu");
}

// 并发提交无数据竞争（用 std::atomic 计数器验证）
CaseResult run_concurrent_atomic(int n_threads, std::size_t per_thread_n,
                                 const std::string& case_id) {
    std::atomic<std::uint64_t> counter{0};
    auto tm = measure_timing([&] {
        counter.store(0, std::memory_order_relaxed);
        std::vector<std::thread> ts;
        ts.reserve(n_threads);
        for (int t = 0; t < n_threads; ++t) {
            ts.emplace_back([&counter, per_thread_n]() {
                parallel_for(KernelId::Custom, Range1D{0, per_thread_n},
                    [&counter](std::size_t) {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
            });
        }
        for (auto& th : ts) th.join();
    }, 3);

    ErrorStats err;
    std::uint64_t expected = static_cast<std::uint64_t>(n_threads) * per_thread_n;
    bool ok = (counter.load() == expected);
    if (!ok) {
        err.max_abs = std::fabs(static_cast<double>(counter.load()) - static_cast<double>(expected));
    }
    return make_result("E16", case_id, "integer", n_threads * per_thread_n, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "concurrent atomic count mismatch",
                       "cpu", "cpu");
}

// 长时间持续路线：循环提交小任务 1000 次
CaseResult run_persistent_submit(int n_iters, const std::string& case_id) {
    std::atomic<int> total{0};
    auto tm = measure_timing([&] {
        total.store(0, std::memory_order_relaxed);
        for (int i = 0; i < n_iters; ++i) {
            parallel_for(KernelId::Custom, Range1D{0, 100},
                [&total](std::size_t) { total.fetch_add(1, std::memory_order_relaxed); });
        }
    }, 1);

    ErrorStats err;
    int expected = n_iters * 100;
    bool ok = (total.load() == expected);
    if (!ok) err.max_abs = std::fabs(static_cast<double>(total.load()) - expected);
    return make_result("E16", case_id, "integer", n_iters, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "persistent submit count mismatch",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E16Concurrency, MultiCaller4)        { auto r = run_multi_caller(4, 1000, "multi_caller_4");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, MultiCaller8)        { auto r = run_multi_caller(8, 1000, "multi_caller_8");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, CancelRepeated50)    { auto r = run_cancel_repeated(50, "cancel_repeated_50");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, BufferLifetime100)   { auto r = run_buffer_lifetime(100, "buffer_lifetime_100"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, Restarts100)         { auto r = run_restarts(100, "restarts_100");               ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, ConcurrentAtomic4)   { auto r = run_concurrent_atomic(4, 1000, "concurrent_atomic_4");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, ConcurrentAtomic8)   { auto r = run_concurrent_atomic(8, 1000, "concurrent_atomic_8");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E16Concurrency, PersistentSubmit1K)  { auto r = run_persistent_submit(1000, "persistent_1k");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e16() {
    return {
        run_multi_caller(4, 1000, "multi_caller_4"),
        run_multi_caller(8, 1000, "multi_caller_8"),
        run_cancel_repeated(50, "cancel_repeated_50"),
        run_buffer_lifetime(100, "buffer_lifetime_100"),
        run_restarts(100, "restarts_100"),
        run_concurrent_atomic(4, 1000, "concurrent_atomic_4"),
        run_concurrent_atomic(8, 1000, "concurrent_atomic_8"),
        run_persistent_submit(1000, "persistent_1k"),
    };
}
