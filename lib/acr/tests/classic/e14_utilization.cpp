// lib/acr/tests/classic/e14_utilization.cpp — E14 Resource Utilization Controller
// 验证能力：50/80/95/100% 目标点
// 用 CpuController + ActualTracker 验证记录实际利用率（不伪报）。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <actual_tracker.hpp>
#include <cpu_controller.hpp>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::utilization;

namespace {

// 测试目标点：set_target → decide → 验证 target/actual/error 字段
CaseResult run_target_point(double target, double actual, const std::string& case_id) {
    CpuController c;
    c.set_target(target);
    c.set_strategy(ControlStrategy::BatchSize);
    auto d = c.decide_with_actual(actual);

    auto tm = measure_timing([&] {
        // 重复 decide 验证稳定性
        for (int i = 0; i < 10; ++i) c.decide_with_actual(actual);
    });

    ErrorStats err;
    // 验证 target 被正确设置（clamp 到 [0,1]）
    double expected_target = target;
    if (expected_target > 1.0) expected_target = 1.0;
    if (expected_target < 0.0) expected_target = 0.0;
    err.max_abs = std::fabs(d.target_ratio - expected_target);
    err.max_rel = expected_target > 1e-9 ? err.max_abs / expected_target : 0.0;

    bool ok = (err.max_abs < 1e-9) &&
              (std::fabs(d.actual_ratio - actual) < 1e-9) &&
              (std::fabs(d.error_ratio - (actual - expected_target)) < 1e-9);
    return make_result("E14", case_id, "fp64", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "target/actual/error mismatch",
                       "cpu", "cpu");
}

// ActualTracker 记录多个样本，验证 average/max/min error
CaseResult run_actual_tracker(int n_samples, const std::string& case_id) {
    ActualTracker t;
    t.set_capacity(1024);

    // rounds=1：tracker 累积样本，多轮会重复记录
    auto tm = measure_timing([&] {
        t.record({0, 0.85, 0.95, -0.10, false, "cpu"});
        t.record({1, 0.92, 0.95, -0.03, false, "cpu"});
        t.record({2, 0.97, 0.95, +0.02, false, "cpu"});
        t.record({3, 0.99, 0.95, +0.04, false, "cpu"});
    }, 1);
    (void)n_samples;

    ErrorStats err;
    auto all = t.all();
    err.max_abs = std::fabs(static_cast<double>(all.size()) - 4.0);  // 4 samples
    bool ok = (all.size() == 4);
    if (ok) {
        // average error ≈ (-0.10 + -0.03 + 0.02 + 0.04) / 4 = -0.0175
        double avg = t.average_error();
        if (std::fabs(avg - (-0.0175)) > 1e-6) {
            ok = false;
            err.max_abs = std::fabs(avg - (-0.0175));
        }
    }
    return make_result("E14", case_id, "fp64", 4, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "actual tracker stats mismatch",
                       "cpu", "cpu");
}

// 不同策略验证（BatchSize / Yield / Priority / QueueDepth）
CaseResult run_strategy(ControlStrategy s, const std::string& case_id) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(s);

    auto tm = measure_timing([&] {
        c.decide_with_actual(0.95);  // 高于 target
        c.decide_with_actual(0.65);  // 低于 target
        c.decide_with_actual(0.80);  // 等于 target
    });

    ErrorStats err;
    bool ok = true;
    // 高于 target 时应该让步/减小 batch
    auto d_high = c.decide_with_actual(0.95);
    auto d_low = c.decide_with_actual(0.65);
    if (s == ControlStrategy::Yield) {
        if (!d_high.should_yield) { ok = false; err.max_abs = 1.0; }
        if (d_low.should_yield) { ok = false; err.max_abs = 1.0; }
    } else if (s == ControlStrategy::BatchSize) {
        if (d_high.batch_size != 1) { ok = false; err.max_abs = 1.0; }
        if (d_low.batch_size != 8) { ok = false; err.max_abs = 1.0; }
    }
    return make_result("E14", case_id, "fp64", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "strategy decision mismatch",
                       "cpu", "cpu");
}

// 持续控制循环：模拟 worker 反馈 actual → controller 决策 → 模拟调整
CaseResult run_control_loop(int iterations, const std::string& case_id) {
    CpuController c;
    c.set_target(0.80);
    c.set_strategy(ControlStrategy::BatchSize);
    ActualTracker tracker;

    double actual = 0.50;  // 起始低
    // rounds=1：tracker 累积样本且 actual 跨轮不重置，多轮会重复记录
    auto tm = measure_timing([&] {
        for (int i = 0; i < iterations; ++i) {
            auto d = c.decide_with_actual(actual);
            tracker.record({static_cast<std::uint64_t>(i), d.actual_ratio, d.target_ratio,
                            d.error_ratio, d.actual_estimated, "cpu"});
            // 模拟：batch_size 大 → actual 升高；batch_size 小 → actual 降低
            if (d.batch_size >= 4) actual += 0.05;
            else actual -= 0.02;
            // clamp
            if (actual > 1.0) actual = 1.0;
            if (actual < 0.0) actual = 0.0;
        }
    }, 1);

    ErrorStats err;
    auto all = tracker.all();
    err.max_abs = std::fabs(static_cast<double>(all.size()) - static_cast<double>(iterations));
    bool ok = (static_cast<int>(all.size()) == iterations);
    return make_result("E14", case_id, "fp64", iterations, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "control loop sample count mismatch",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E14Util, Target50)      { auto r = run_target_point(0.50, 0.45, "target_50");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, Target80)      { auto r = run_target_point(0.80, 0.75, "target_80");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, Target95)      { auto r = run_target_point(0.95, 0.92, "target_95");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, Target100)     { auto r = run_target_point(1.00, 0.99, "target_100");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, TargetClampHigh){auto r = run_target_point(2.00, 0.50, "target_clamp_high");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, TrackerSamples){ auto r = run_actual_tracker(4, "tracker_4_samples");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, StrategyBatch) { auto r = run_strategy(ControlStrategy::BatchSize, "strategy_batch"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, StrategyYield) { auto r = run_strategy(ControlStrategy::Yield, "strategy_yield");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, ControlLoop100){ auto r = run_control_loop(100, "control_loop_100");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E14Util, ControlLoop500){ auto r = run_control_loop(500, "control_loop_500");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e14() {
    return {
        run_target_point(0.50, 0.45, "target_50"),
        run_target_point(0.80, 0.75, "target_80"),
        run_target_point(0.95, 0.92, "target_95"),
        run_target_point(1.00, 0.99, "target_100"),
        run_target_point(2.00, 0.50, "target_clamp_high"),
        run_actual_tracker(4, "tracker_4_samples"),
        run_strategy(ControlStrategy::BatchSize, "strategy_batch"),
        run_strategy(ControlStrategy::Yield, "strategy_yield"),
        run_control_loop(100, "control_loop_100"),
        run_control_loop(500, "control_loop_500"),
    };
}
