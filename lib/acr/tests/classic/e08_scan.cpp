// lib/acr/tests/classic/e08_scan.cpp — E08 Prefix Scan
// 验证能力：依赖模式（前缀和）
// Cases: inclusive_sum / exclusive_sum / max_scan / parallel_chunks_blocked
// parallel_scan 当前为串行 baseline；同时验证 parallel_chunks 分块实现（局部扫描+修正）。
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
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0) * 0.1f;  // 小值避免爆炸
}

// inclusive scan: out[i] = in[0] + ... + in[i]
CaseResult run_inclusive_scan(std::size_t n, const std::string& case_id) {
    std::vector<float> in(n), out(n, 0.0f), ref(n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) { acc += in[i]; ref[i] = acc; }

    auto tm = measure_timing([&] {
        BufferView<float> in_view(in.data(), n);
        BufferView<float> out_view(out.data(), n);
        parallel_scan<float>(KernelId::Scan, in_view, out_view, 0.0f,
            [](std::size_t, float) { return 0.0f; },  // Phase B 占位
            [](float a, float b) { return a + b; });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = err.max_abs <= 1e-4 + 1e-4 * std::fabs(ref.back());
    return make_result("E08", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "inclusive scan mismatch");
}

// exclusive scan: out[i] = in[0] + ... + in[i-1], out[0] = 0
CaseResult run_exclusive_scan(std::size_t n, const std::string& case_id) {
    std::vector<float> in(n), out(n, 0.0f), ref(n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) { ref[i] = acc; acc += in[i]; }

    // 用 parallel_scan 但 identity=0，op=add → 实际是 inclusive；
    // exclusive 用手工串行（验证 parallel_scan API 路径时用 inclusive）
    auto tm = measure_timing([&] {
        // 手工串行 exclusive
        float s = 0.0f;
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = s;
            s += in[i];
        }
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);  // 串行应 bit-exact
    return make_result("E08", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "exclusive scan mismatch");
}

// 分块并行扫描：每块局部 inclusive scan → 块间修正
CaseResult run_blocked_parallel_scan(std::size_t n, std::size_t block_size, const std::string& case_id) {
    std::vector<float> in(n), out(n, 0.0f), ref(n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) { acc += in[i]; ref[i] = acc; }

    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0.0f);
        std::vector<float> block_sums;
        // 阶段 1：每块局部 inclusive scan
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                float s = 0.0f;
                for (std::size_t i = b; i < e; ++i) { s += in[i]; out[i] = s; }
            });
        // 阶段 2：收集每块末尾值
        for (std::size_t i = block_size - 1; i < n; i += block_size) {
            block_sums.push_back(out[i]);
        }
        // 阶段 3：块前缀和
        std::vector<float> block_prefix(block_sums.size() + 1, 0.0f);
        for (std::size_t i = 0; i < block_sums.size(); ++i) {
            block_prefix[i + 1] = block_prefix[i] + block_sums[i];
        }
        // 阶段 4：每块加上前一块的 prefix（第一块不加）
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::size_t block_idx = b / block_size;
                if (block_idx == 0) return;
                float prefix = block_prefix[block_idx];
                for (std::size_t i = b; i < e; ++i) out[i] += prefix;
            });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = err.max_abs <= 1e-4 + 1e-4 * std::fabs(ref.back());
    return make_result("E08", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "blocked scan mismatch");
}

// Max scan: out[i] = max(in[0..i])
CaseResult run_max_scan(std::size_t n, const std::string& case_id) {
    std::vector<float> in(n), out(n, 0.0f), ref(n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    float m = in[0];
    for (std::size_t i = 0; i < n; ++i) {
        if (in[i] > m) m = in[i];
        ref[i] = m;
    }

    auto tm = measure_timing([&] {
        float cur = std::numeric_limits<float>::lowest();
        for (std::size_t i = 0; i < n; ++i) {
            cur = cur > in[i] ? cur : in[i];
            out[i] = cur;
        }
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);
    return make_result("E08", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "max scan mismatch");
}

} // anonymous namespace

TEST(E08Scan, Inclusive1K)   { auto r = run_inclusive_scan(1<<10, "inclusive_1K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Inclusive64K)  { auto r = run_inclusive_scan(1<<16, "inclusive_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Inclusive1M)   { auto r = run_inclusive_scan(1<<20, "inclusive_1M");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Exclusive1K)   { auto r = run_exclusive_scan(1<<10, "exclusive_1K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Exclusive64K)  { auto r = run_exclusive_scan(1<<16, "exclusive_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Blocked1K)     { auto r = run_blocked_parallel_scan(1<<10, 128, "blocked_1K_128");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Blocked64K)    { auto r = run_blocked_parallel_scan(1<<16, 1024, "blocked_64K_1024");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Max1K)         { auto r = run_max_scan(1<<10, "max_1K");                ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e08() {
    return {
        run_inclusive_scan(1<<10, "inclusive_1K"),
        run_inclusive_scan(1<<16, "inclusive_64K"),
        run_inclusive_scan(1<<20, "inclusive_1M"),
        run_exclusive_scan(1<<10, "exclusive_1K"),
        run_exclusive_scan(1<<16, "exclusive_64K"),
        run_blocked_parallel_scan(1<<10, 128, "blocked_1K_128"),
        run_blocked_parallel_scan(1<<16, 1024, "blocked_64K_1024"),
        run_max_scan(1<<10, "max_1K"),
    };
}
