// lib/acr/tests/classic/e01_memory.cpp — E01 Memory Copy/Read/Write/Triad
// 验证能力：Map、连续内存、ISA、线程
// Cases: copy / read_sum / write / triad（各 1K/64K/1M）
// 所有 kernel 用 ACR parallel_for 调度；与串行 reference 对比。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <numeric>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// 填充确定性输入（LCG）
void fill_input(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) {
        // [-1, 1]
        x = static_cast<float>(rng.next_double() * 2.0 - 1.0);
    }
}

// Reference Copy: dst[i] = src[i]
void ref_copy(const std::vector<float>& src, std::vector<float>& dst) {
    for (std::size_t i = 0; i < src.size(); ++i) dst[i] = src[i];
}

// Reference Read: sum of src
float ref_read_sum(const std::vector<float>& src) {
    double s = 0.0;
    for (auto x : src) s += static_cast<double>(x);
    return static_cast<float>(s);
}

// Reference Write: dst[i] = const
void ref_write(std::vector<float>& dst, float val) {
    for (auto& x : dst) x = val;
}

// Reference Triad: dst[i] = a * x[i] + y[i]
void ref_triad(std::vector<float>& dst, float a,
               const std::vector<float>& x, const std::vector<float>& y) {
    for (std::size_t i = 0; i < dst.size(); ++i) dst[i] = a * x[i] + y[i];
}

// 运行 Copy case，返回 CaseResult
CaseResult run_copy(std::size_t n) {
    std::vector<float> src(n), dst(n, 0.0f), ref(n, 0.0f);
    fill_input(src, FIXED_SEED);
    ref_copy(src, ref);

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Copy, Range1D{0, n}, [&](std::size_t i) {
            dst[i] = src[i];
        });
    });

    auto err = compute_errors<float>(dst.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);  // bit-exact for copy
    return make_result("E01", "copy_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "copy mismatch");
}

// 运行 Read (sum) case
CaseResult run_read(std::size_t n) {
    std::vector<float> src(n);
    fill_input(src, FIXED_SEED);
    float ref = ref_read_sum(src);

    float actual = 0.0f;
    auto tm = measure_timing([&] {
        actual = parallel_reduce<float>(KernelId::Dot, Range1D{0, n}, 0.0f,
            [&](std::size_t i) { return src[i]; },
            [](float a, float b) { return a + b; });
    });

    ErrorStats err;
    err.max_abs = std::fabs(static_cast<double>(actual) - static_cast<double>(ref));
    err.max_rel = std::fabs(ref) > 1e-30 ? err.max_abs / std::fabs(ref) : 0.0;
    err.rmse = err.max_abs;
    bool ok = fp32_close(actual, ref);
    return make_result("E01", "read_sum_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "sum mismatch");
}

// 运行 Write case
CaseResult run_write(std::size_t n) {
    std::vector<float> dst(n, 0.0f), ref(n, 0.0f);
    constexpr float kVal = 3.14f;
    ref_write(ref, kVal);

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Copy, Range1D{0, n}, [&](std::size_t i) {
            dst[i] = kVal;
        });
    });

    auto err = compute_errors<float>(dst.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);
    return make_result("E01", "write_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "write mismatch");
}

// 运行 Triad case
CaseResult run_triad(std::size_t n) {
    std::vector<float> x(n), y(n), dst(n, 0.0f), ref(n, 0.0f);
    fill_input(x, FIXED_SEED);
    fill_input(y, FIXED_SEED ^ 0xDEADBEEF);
    constexpr float kA = 2.5f;
    ref_triad(ref, kA, x, y);

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Triad, Range1D{0, n}, [&](std::size_t i) {
            dst[i] = kA * x[i] + y[i];
        });
    });

    auto err = compute_errors<float>(dst.data(), ref.data(), n);
    bool ok = (err.max_abs <= 1e-5 + 5e-5 * 1.0);  // 单次 FMA 误差
    return make_result("E01", "triad_" + std::to_string(n), "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "triad mismatch");
}

} // anonymous namespace

// ===== GoogleTest 入口 =====

TEST(E01Memory, CopySmall)   { auto r = run_copy(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, CopyMedium)  { auto r = run_copy(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, CopyLarge)   { auto r = run_copy(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, ReadSmall)   { auto r = run_read(kStandardSizes.small);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, ReadMedium)  { auto r = run_read(kStandardSizes.medium); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, ReadLarge)   { auto r = run_read(kStandardSizes.large);  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, WriteSmall)  { auto r = run_write(kStandardSizes.small); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, WriteMedium) { auto r = run_write(kStandardSizes.medium);ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, WriteLarge)  { auto r = run_write(kStandardSizes.large); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, TriadSmall)  { auto r = run_triad(kStandardSizes.small); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, TriadMedium) { auto r = run_triad(kStandardSizes.medium);ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E01Memory, TriadLarge)  { auto r = run_triad(kStandardSizes.large); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// classic_runner 调用入口
extern "C" std::vector<CaseResult> run_e01() {
    return {
        run_copy(kStandardSizes.small),  run_copy(kStandardSizes.medium), run_copy(kStandardSizes.large),
        run_read(kStandardSizes.small),  run_read(kStandardSizes.medium), run_read(kStandardSizes.large),
        run_write(kStandardSizes.small), run_write(kStandardSizes.medium),run_write(kStandardSizes.large),
        run_triad(kStandardSizes.small), run_triad(kStandardSizes.medium),run_triad(kStandardSizes.large),
    };
}
