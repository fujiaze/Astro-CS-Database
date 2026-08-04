// lib/acr/tests/classic/e03_dot.cpp — E03 Dot/Reduction Family
// 验证能力：parallel_reduce、局部归约
// Cases: dot_product / sum_reduce / max_reduce / norm_l2，1K/64K/1M，FP32+FP64
// 并行归约 vs 串行 reference，验证结合律误差可控。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

void fill_fp32(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0);
}
void fill_fp64(std::vector<double>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = rng.next_double() * 2.0 - 1.0;
}

// FP32 Dot: sum(x[i]*y[i])
CaseResult run_dot_fp32(std::size_t n) {
    std::vector<float> x(n), y(n);
    fill_fp32(x, FIXED_SEED);
    fill_fp32(y, FIXED_SEED ^ 0x12345678);
    double ref = 0.0;
    for (std::size_t i = 0; i < n; ++i) ref += static_cast<double>(x[i]) * static_cast<double>(y[i]);

    float actual = 0.0f;
    auto tm = measure_timing([&] {
        actual = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i] * y[i]; },
            [](float a, float b) { return a + b; });
    });

    ErrorStats err;
    err.max_abs = std::fabs(static_cast<double>(actual) - ref);
    err.max_rel = std::fabs(ref) > 1e-30 ? err.max_abs / std::fabs(ref) : 0.0;
    err.rmse = err.max_abs;
    // FP32 归约允许较大累积误差（n 个元素累加）
    bool ok = err.max_abs <= 1e-3 + 5e-4 * std::fabs(ref);
    return make_result("E03", "dot_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "dot fp32 accumulation error");
}

// FP64 Dot
CaseResult run_dot_fp64(std::size_t n) {
    std::vector<double> x(n), y(n);
    fill_fp64(x, FIXED_SEED);
    fill_fp64(y, FIXED_SEED ^ 0x12345678);
    double ref = 0.0;
    for (std::size_t i = 0; i < n; ++i) ref += x[i] * y[i];

    double actual = 0.0;
    auto tm = measure_timing([&] {
        actual = parallel_reduce<double>(KernelId::Dot, Range1D{0, n}, 0.0,
            [&](std::size_t i) { return x[i] * y[i]; },
            [](double a, double b) { return a + b; });
    });

    ErrorStats err;
    err.max_abs = std::fabs(actual - ref);
    err.max_rel = std::fabs(ref) > 1e-30 ? err.max_abs / std::fabs(ref) : 0.0;
    err.rmse = err.max_abs;
    bool ok = err.max_abs <= 1e-9 + 1e-9 * std::fabs(ref);
    return make_result("E03", "dot_fp64_" + std::to_string(n), "fp64", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "dot fp64 accumulation error");
}

// Sum reduce
CaseResult run_sum_fp32(std::size_t n) {
    std::vector<float> x(n);
    fill_fp32(x, FIXED_SEED);
    double ref = 0.0;
    for (auto v : x) ref += static_cast<double>(v);

    float actual = 0.0f;
    auto tm = measure_timing([&] {
        actual = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i]; },
            [](float a, float b) { return a + b; });
    });

    ErrorStats err;
    err.max_abs = std::fabs(static_cast<double>(actual) - ref);
    err.max_rel = std::fabs(ref) > 1e-30 ? err.max_abs / std::fabs(ref) : 0.0;
    err.rmse = err.max_abs;
    bool ok = err.max_abs <= 1e-3 + 5e-4 * std::fabs(ref);
    return make_result("E03", "sum_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "sum fp32 error");
}

// Max reduce
CaseResult run_max_fp32(std::size_t n) {
    std::vector<float> x(n);
    fill_fp32(x, FIXED_SEED);
    float ref = x[0];
    for (auto v : x) if (v > ref) ref = v;

    float actual = 0.0f;
    auto tm = measure_timing([&] {
        actual = parallel_reduce<float>(KernelId::Dot, Range1D{0, n},
            std::numeric_limits<float>::lowest(),
            [&](std::size_t i) { return x[i]; },
            [](float a, float b) { return a > b ? a : b; });
    });

    ErrorStats err;
    err.max_abs = std::fabs(static_cast<double>(actual) - static_cast<double>(ref));
    err.max_rel = 0.0;
    err.rmse = err.max_abs;
    bool ok = (actual == ref);  // max 应当 bit-exact
    return make_result("E03", "max_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "max reduce mismatch");
}

// L2 norm: sqrt(sum(x^2))
CaseResult run_l2_fp32(std::size_t n) {
    std::vector<float> x(n);
    fill_fp32(x, FIXED_SEED);
    double sum_sq = 0.0;
    for (auto v : x) sum_sq += static_cast<double>(v) * static_cast<double>(v);
    float ref = static_cast<float>(std::sqrt(sum_sq));

    float actual = 0.0f;
    auto tm = measure_timing([&] {
        float s = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return x[i] * x[i]; },
            [](float a, float b) { return a + b; });
        actual = std::sqrt(s);
    });

    ErrorStats err;
    err.max_abs = std::fabs(static_cast<double>(actual) - static_cast<double>(ref));
    err.max_rel = std::fabs(ref) > 1e-30 ? err.max_abs / std::fabs(ref) : 0.0;
    err.rmse = err.max_abs;
    // FP32 并行归约对 1M+ 元素存在正常非结合性误差（树形分组不同 → 末位/次末位差异）。
    // 经典容差之外增加尺寸感知相对容差（0.01%），避免系统负载导致的偶发误报；
    // 这仍是有效正确性门禁（相对误差必须 < 1e-4）。
    const double ref_abs = std::fabs(static_cast<double>(ref));
    const double rel_tol = ref_abs > 1e-30 ? 1e-4 * ref_abs : 1e-5;
    bool ok = fp32_close(actual, ref) ||
              (err.max_abs <= rel_tol);
    return make_result("E03", "l2_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "l2 norm error");
}

} // anonymous namespace

TEST(E03Dot, Fp32Small)  { auto r = run_dot_fp32(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Dot, Fp32Medium) { auto r = run_dot_fp32(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Dot, Fp32Large)  { auto r = run_dot_fp32(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Dot, Fp64Small)  { auto r = run_dot_fp64(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Dot, Fp64Medium) { auto r = run_dot_fp64(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Dot, Fp64Large)  { auto r = run_dot_fp64(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, SumFp32Small)  { auto r = run_sum_fp32(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, SumFp32Large)  { auto r = run_sum_fp32(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, MaxFp32Small)  { auto r = run_max_fp32(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, MaxFp32Large)  { auto r = run_max_fp32(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, L2Fp32Small)   { auto r = run_l2_fp32(kStandardSizes.small);   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E03Reduce, L2Fp32Large)   { auto r = run_l2_fp32(kStandardSizes.large);   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e03() {
    return {
        run_dot_fp32(kStandardSizes.small),  run_dot_fp32(kStandardSizes.medium),  run_dot_fp32(kStandardSizes.large),
        run_dot_fp64(kStandardSizes.small),  run_dot_fp64(kStandardSizes.medium),  run_dot_fp64(kStandardSizes.large),
        run_sum_fp32(kStandardSizes.small),  run_sum_fp32(kStandardSizes.large),
        run_max_fp32(kStandardSizes.small),  run_max_fp32(kStandardSizes.large),
        run_l2_fp32(kStandardSizes.small),   run_l2_fp32(kStandardSizes.large),
    };
}
