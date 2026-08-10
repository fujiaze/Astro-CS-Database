// lib/acr/tests/classic/e08_scan.cpp — E08 Prefix Scan
// 验证能力（17 §13）：依赖模式（前缀和）、证明依赖任务不能当普通 parallel_for
//
// Phase H 扩展：
//   - inclusive/exclusive
//   - uint32 输入、uint64 输出
//   - N=1、3、1025、2^20+17（边界尺寸）
//   - oneTBB adapter（用 ACR parallel_chunks 实现 blocked scan adapter）
//   - 逐元素 exact
//   - 证明依赖任务不能当普通 parallel_for（无块间修正时出错）
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== uint32 填充 =====
void fill_u32(std::vector<std::uint32_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<std::uint32_t>(rng.next() & 0xFFFFFFFFu);
}

void fill_fp32(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0) * 0.1f;
}

// ===== inclusive scan reference（uint32 输入 → uint64 输出）=====
void ref_inclusive_scan_u64(const std::vector<std::uint32_t>& in, std::vector<std::uint64_t>& out) {
    std::size_t n = in.size();
    out.assign(n, 0);
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) { acc += in[i]; out[i] = acc; }
}

// ===== exclusive scan reference（uint32 输入 → uint64 输出）=====
// out[i] = in[0] + ... + in[i-1], out[0] = 0
void ref_exclusive_scan_u64(const std::vector<std::uint32_t>& in, std::vector<std::uint64_t>& out) {
    std::size_t n = in.size();
    out.assign(n, 0);
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) { out[i] = acc; acc += in[i]; }
}

bool compare_u64_exact(const std::vector<std::uint64_t>& a,
                      const std::vector<std::uint64_t>& ref, ErrorStats& err) {
    bool ok = true;
    err = ErrorStats{};
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != ref[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(a[i]) -
                                                         static_cast<long long>(ref[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return ok;
}

// ===== blocked parallel inclusive scan（oneTBB adapter 风格）=====
// 阶段 1：每块局部 inclusive scan（parallel_chunks）
// 阶段 2：块尾值前缀和（串行，块数少）
// 阶段 3：每块加上前一块的 prefix（parallel_chunks）
// 整数加法可结合 → exact
CaseResult run_inclusive_scan_u64(std::size_t n, std::size_t block_size,
                                  const std::string& case_id) {
    std::vector<std::uint32_t> in(n);
    std::vector<std::uint64_t> out(n, 0), ref;
    fill_u32(in, FIXED_SEED);
    ref_inclusive_scan_u64(in, ref);

    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0);
        if (n == 0) return;
        if (n == 1) { out[0] = in[0]; return; }
        // 阶段 1：每块局部 inclusive scan
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::uint64_t s = 0;
                for (std::size_t i = b; i < e; ++i) { s += in[i]; out[i] = s; }
            });
        // 阶段 2：收集每块末尾值，串行前缀和
        std::vector<std::uint64_t> block_sums;
        for (std::size_t i = block_size - 1; i < n; i += block_size) {
            block_sums.push_back(out[i]);
        }
        std::vector<std::uint64_t> block_prefix(block_sums.size() + 1, 0);
        for (std::size_t i = 0; i < block_sums.size(); ++i) {
            block_prefix[i + 1] = block_prefix[i] + block_sums[i];
        }
        // 阶段 3：每块加上前一块的 prefix（第一块不加）
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::size_t block_idx = b / block_size;
                if (block_idx == 0) return;
                std::uint64_t prefix = block_prefix[block_idx];
                for (std::size_t i = b; i < e; ++i) out[i] += prefix;
            });
    });

    ErrorStats err;
    bool ok = compare_u64_exact(out, ref, err);
    return make_result("E08", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "inclusive scan u64 mismatch",
                       "cpu", "cpu");
}

// ===== blocked parallel exclusive scan（uint32 → uint64）=====
CaseResult run_exclusive_scan_u64(std::size_t n, std::size_t block_size,
                                  const std::string& case_id) {
    std::vector<std::uint32_t> in(n);
    std::vector<std::uint64_t> out(n, 0), ref;
    fill_u32(in, FIXED_SEED);
    ref_exclusive_scan_u64(in, ref);

    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0);
        if (n == 0) return;
        if (n == 1) { out[0] = 0; return; }
        // 阶段 1：每块局部 inclusive scan（临时存于 out）
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::uint64_t s = 0;
                for (std::size_t i = b; i < e; ++i) { s += in[i]; out[i] = s; }
            });
        // 阶段 2：每块总和的前缀和（exclusive：block_prefix[0]=0）
        std::vector<std::uint64_t> block_sums;
        for (std::size_t i = block_size - 1; i < n; i += block_size) {
            block_sums.push_back(out[i]);
        }
        std::vector<std::uint64_t> block_prefix(block_sums.size() + 1, 0);
        for (std::size_t i = 0; i < block_sums.size(); ++i) {
            block_prefix[i + 1] = block_prefix[i] + block_sums[i];
        }
        // 阶段 3：每元素 = block_prefix[block_idx] + (out[i] - in[i])
        //         即 exclusive = block_prefix + 局部 inclusive 减自身
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::size_t block_idx = b / block_size;
                std::uint64_t prefix = block_prefix[block_idx];
                for (std::size_t i = b; i < e; ++i) out[i] = prefix + (out[i] - in[i]);
            });
    });

    ErrorStats err;
    bool ok = compare_u64_exact(out, ref, err);
    return make_result("E08", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "exclusive scan u64 mismatch",
                       "cpu", "cpu");
}

// ===== 证明依赖任务不能当普通 parallel_for（无块间修正会出错）=====
// 故意不做阶段 2/3 修正，验证大尺寸下结果错误（证明扫描是依赖模式）
CaseResult run_naive_parallel_no_fix(std::size_t n, std::size_t block_size,
                                     const std::string& case_id) {
    std::vector<std::uint32_t> in(n);
    std::vector<std::uint64_t> out(n, 0), ref;
    fill_u32(in, FIXED_SEED);
    ref_inclusive_scan_u64(in, ref);

    // 只做阶段 1（每块局部 inclusive），不做块间修正
    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0);
        parallel_chunks(KernelId::Scan, Range1D{0, n}, block_size,
            [&](std::size_t b, std::size_t e) {
                std::uint64_t s = 0;
                for (std::size_t i = b; i < e; ++i) { s += in[i]; out[i] = s; }
            });
    });

    ErrorStats err;
    bool exact = compare_u64_exact(out, ref, err);
    // 期望：n > block_size 时一定不 exact（证明依赖性）
    bool demonstrates_dependency = (!exact) && (n > block_size);
    // 这是一个"反向"测试：演示 naive parallel_for 不正确
    return make_result("E08", case_id, "integer", n, demonstrates_dependency, err, tm,
                       demonstrates_dependency ? "PASS" : "FAIL",
                       demonstrates_dependency ? "" : "naive parallel should fail for scan",
                       "cpu", "cpu");
}

// ===== FP32 inclusive scan（向后兼容，用 parallel_scan API）=====
CaseResult run_inclusive_scan_fp32(std::size_t n, const std::string& case_id) {
    std::vector<float> in(n), out(n, 0.0f), ref(n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) { acc += in[i]; ref[i] = acc; }

    auto tm = measure_timing([&] {
        BufferView<float> in_view(in.data(), n);
        BufferView<float> out_view(out.data(), n);
        parallel_scan<float>(KernelId::Scan, in_view, out_view, 0.0f,
            [](std::size_t, float) { return 0.0f; },
            [](float a, float b) { return a + b; });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = err.max_abs <= 1e-4 + 1e-4 * std::fabs(ref.back());
    return make_result("E08", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "inclusive scan fp32 mismatch");
}

// ===== max scan（FP32，向后兼容）=====
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

// ===== 向后兼容 TEST =====
TEST(E08Scan, Inclusive1K)   { auto r = run_inclusive_scan_fp32(1<<10, "inclusive_1K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Inclusive64K)  { auto r = run_inclusive_scan_fp32(1<<16, "inclusive_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, Max1K)         { auto r = run_max_scan(1<<10, "max_1K");                     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §13 规范 =====
// 边界尺寸 N=1, 3, 1025, 2^20+17
TEST(E08Scan, InclusiveU64_N1)      { auto r = run_inclusive_scan_u64(1, 64, "incl_u64_n1");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, InclusiveU64_N3)      { auto r = run_inclusive_scan_u64(3, 64, "incl_u64_n3");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, InclusiveU64_N1025)   { auto r = run_inclusive_scan_u64(1025, 128, "incl_u64_n1025"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, InclusiveU64_N1M_plus17){ auto r = run_inclusive_scan_u64((1<<20)+17, 1024, "incl_u64_n1m_plus17"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// exclusive scan
TEST(E08Scan, ExclusiveU64_N1)      { auto r = run_exclusive_scan_u64(1, 64, "excl_u64_n1");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, ExclusiveU64_N3)      { auto r = run_exclusive_scan_u64(3, 64, "excl_u64_n3");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, ExclusiveU64_N1025)   { auto r = run_exclusive_scan_u64(1025, 128, "excl_u64_n1025"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, ExclusiveU64_N1M_plus17){ auto r = run_exclusive_scan_u64((1<<20)+17, 1024, "excl_u64_n1m_plus17"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 不同 block_size
TEST(E08Scan, InclusiveU64_N4K_B256){ auto r = run_inclusive_scan_u64(4096, 256, "incl_u64_n4k_b256"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E08Scan, ExclusiveU64_N4K_B64) { auto r = run_exclusive_scan_u64(4096, 64, "excl_u64_n4k_b64");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 证明依赖任务不能当普通 parallel_for
TEST(E08Scan, NaiveParallelFails_N4K){ auto r = run_naive_parallel_no_fix(4096, 128, "naive_no_fix_n4k"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e08() {
    return {
        // 向后兼容
        run_inclusive_scan_fp32(1<<10, "inclusive_1K"),
        run_inclusive_scan_fp32(1<<16, "inclusive_64K"),
        run_max_scan(1<<10, "max_1K"),
        // Phase H 扩展：inclusive u64
        run_inclusive_scan_u64(1, 64, "incl_u64_n1"),
        run_inclusive_scan_u64(3, 64, "incl_u64_n3"),
        run_inclusive_scan_u64(1025, 128, "incl_u64_n1025"),
        run_inclusive_scan_u64((1<<20)+17, 1024, "incl_u64_n1m_plus17"),
        // exclusive u64
        run_exclusive_scan_u64(1, 64, "excl_u64_n1"),
        run_exclusive_scan_u64(3, 64, "excl_u64_n3"),
        run_exclusive_scan_u64(1025, 128, "excl_u64_n1025"),
        run_exclusive_scan_u64((1<<20)+17, 1024, "excl_u64_n1m_plus17"),
        // block_size 变化
        run_inclusive_scan_u64(4096, 256, "incl_u64_n4k_b256"),
        run_exclusive_scan_u64(4096, 64, "excl_u64_n4k_b64"),
        // 依赖性证明
        run_naive_parallel_no_fix(4096, 128, "naive_no_fix_n4k"),
    };
}
