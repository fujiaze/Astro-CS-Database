// lib/acr/tests/classic/e02_axpy.cpp — E02 AXPY/FMA
// 验证能力：算术吞吐、FMA
// Cases: AXPY (y = a*x + y) / FMA (z = a*x + b*y) / Scal (y = a*y)，1K/64K/1M
// FP32 + FP64 双精度版本验证 FMA 行为。
#include "classic_common.hpp"

#include <gtest/gtest.h>

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

// FP32 AXPY: y[i] = a * x[i] + y[i]
CaseResult run_axpy_fp32(std::size_t n) {
    std::vector<float> x(n), y(n), y_ref(n), y_orig(n);
    fill_fp32(x, FIXED_SEED);
    fill_fp32(y, FIXED_SEED ^ 0xAAAAAAAA);
    y_orig = y;
    y_ref = y;
    constexpr float kA = 1.75f;
    for (std::size_t i = 0; i < n; ++i) y_ref[i] = kA * x[i] + y_ref[i];

    auto tm = measure_timing([&] {
        std::copy(y_orig.begin(), y_orig.end(), y.begin());
        parallel_for(KernelId::AXPY, Range1D{0, n}, [&](std::size_t i) {
            y[i] = kA * x[i] + y[i];
        });
    });
    auto err = compute_errors<float>(y.data(), y_ref.data(), n);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E02", "axpy_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "axpy fp32 mismatch");
}

// FP64 AXPY
CaseResult run_axpy_fp64(std::size_t n) {
    std::vector<double> x(n), y(n), y_ref(n), y_orig(n);
    fill_fp64(x, FIXED_SEED);
    fill_fp64(y, FIXED_SEED ^ 0xAAAAAAAA);
    y_orig = y;
    y_ref = y;
    constexpr double kA = 1.75;
    for (std::size_t i = 0; i < n; ++i) y_ref[i] = kA * x[i] + y_ref[i];

    auto tm = measure_timing([&] {
        std::copy(y_orig.begin(), y_orig.end(), y.begin());
        parallel_for(KernelId::AXPY, Range1D{0, n}, [&](std::size_t i) {
            y[i] = kA * x[i] + y[i];
        });
    });
    auto err = compute_errors<double>(y.data(), y_ref.data(), n);
    bool ok = err.max_abs <= 1e-12 + 1e-11 * 2.0;
    return make_result("E02", "axpy_fp64_" + std::to_string(n), "fp64", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "axpy fp64 mismatch");
}

// FMA: z[i] = a*x[i] + b*y[i]（三操作数融合乘加，独立写第三个数组）
CaseResult run_fma_fp32(std::size_t n) {
    std::vector<float> x(n), y(n), z(n, 0.0f), z_ref(n, 0.0f);
    fill_fp32(x, FIXED_SEED);
    fill_fp32(y, FIXED_SEED ^ 0x55555555);
    constexpr float kA = 1.5f, kB = 0.5f;
    for (std::size_t i = 0; i < n; ++i) z_ref[i] = kA * x[i] + kB * y[i];

    auto tm = measure_timing([&] {
        parallel_for(KernelId::AXPY, Range1D{0, n}, [&](std::size_t i) {
            z[i] = kA * x[i] + kB * y[i];
        });
    });
    auto err = compute_errors<float>(z.data(), z_ref.data(), n);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E02", "fma_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "fma fp32 mismatch");
}

// Scal: y[i] = a * y[i]
CaseResult run_scal_fp32(std::size_t n) {
    std::vector<float> y(n), y_ref(n), y_orig(n);
    fill_fp32(y, FIXED_SEED);
    y_orig = y;
    y_ref = y;
    constexpr float kA = 0.625f;
    for (auto& v : y_ref) v = kA * v;

    auto tm = measure_timing([&] {
        std::copy(y_orig.begin(), y_orig.end(), y.begin());
        parallel_for(KernelId::AXPY, Range1D{0, n}, [&](std::size_t i) {
            y[i] = kA * y[i];
        });
    });
    auto err = compute_errors<float>(y.data(), y_ref.data(), n);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 1.0;
    return make_result("E02", "scal_fp32_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "scal mismatch");
}

} // anonymous namespace

TEST(E02Axpy, Fp32Small)  { auto r = run_axpy_fp32(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Axpy, Fp32Medium) { auto r = run_axpy_fp32(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Axpy, Fp32Large)  { auto r = run_axpy_fp32(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Axpy, Fp64Small)  { auto r = run_axpy_fp64(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Axpy, Fp64Medium) { auto r = run_axpy_fp64(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Axpy, Fp64Large)  { auto r = run_axpy_fp64(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Fma,  Fp32Small)  { auto r = run_fma_fp32(kStandardSizes.small);   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Fma,  Fp32Medium) { auto r = run_fma_fp32(kStandardSizes.medium);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Fma,  Fp32Large)  { auto r = run_fma_fp32(kStandardSizes.large);   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Scal, Fp32Small)  { auto r = run_scal_fp32(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Scal, Fp32Medium) { auto r = run_scal_fp32(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E02Scal, Fp32Large)  { auto r = run_scal_fp32(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e02() {
    return {
        run_axpy_fp32(kStandardSizes.small),  run_axpy_fp32(kStandardSizes.medium),  run_axpy_fp32(kStandardSizes.large),
        run_axpy_fp64(kStandardSizes.small),  run_axpy_fp64(kStandardSizes.medium),  run_axpy_fp64(kStandardSizes.large),
        run_fma_fp32(kStandardSizes.small),   run_fma_fp32(kStandardSizes.medium),   run_fma_fp32(kStandardSizes.large),
        run_scal_fp32(kStandardSizes.small),  run_scal_fp32(kStandardSizes.medium), run_scal_fp32(kStandardSizes.large),
    };
}
