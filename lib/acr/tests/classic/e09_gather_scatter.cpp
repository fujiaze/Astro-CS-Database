// lib/acr/tests/classic/e09_gather_scatter.cpp — E09 Gather/Scatter
// 验证能力：不规则内存、稀疏度
// Cases: gather_dense / gather_sparse / scatter_dense / scatter_sparse
// gather: out[i] = in[indices[i]]；scatter: out[indices[i]] = in[i]
// 用不同稀疏度（dense=100% / sparse=10%）验证。
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

// 生成确定性索引（在 [0, n) 范围内）
void fill_indices(std::vector<std::size_t>& idx, std::size_t n, std::uint64_t seed,
                  std::size_t range_n) {
    LCG rng(seed);
    for (auto& i : idx) i = rng.next() % range_n;
}

// Gather: out[i] = in[indices[i]]
CaseResult run_gather(std::size_t n, std::size_t src_n, const std::string& case_id) {
    std::vector<float> src(src_n), out(n, 0.0f), ref(n, 0.0f);
    std::vector<std::size_t> indices(n);
    fill_fp32(src, FIXED_SEED);
    fill_indices(indices, n, FIXED_SEED ^ 0x1111, src_n);
    for (std::size_t i = 0; i < n; ++i) ref[i] = src[indices[i]];

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Gather, Range1D{0, n}, [&](std::size_t i) {
            out[i] = src[indices[i]];
        });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);  // gather bit-exact
    return make_result("E09", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gather mismatch");
}

// Scatter: out[indices[i]] = in[i]（无冲突，因 indices 唯一）
CaseResult run_scatter_unique(std::size_t n, std::size_t dst_n, const std::string& case_id) {
    std::vector<float> in(n), out(dst_n, 0.0f), ref(dst_n, 0.0f);
    std::vector<std::size_t> indices(n);
    fill_fp32(in, FIXED_SEED);
    // 生成唯一索引（用排列）
    for (std::size_t i = 0; i < dst_n; ++i) ref[i] = 0.0f;
    // 用 LCG 生成 n 个不同索引（Floyd 算法）
    LCG rng(FIXED_SEED ^ 0x2222);
    std::vector<bool> used(dst_n, false);
    std::size_t placed = 0;
    while (placed < n) {
        std::size_t idx = rng.next() % dst_n;
        if (!used[idx]) {
            used[idx] = true;
            indices[placed++] = idx;
        }
    }
    for (std::size_t i = 0; i < n; ++i) ref[indices[i]] = in[i];

    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0.0f);
        parallel_for(KernelId::Scatter, Range1D{0, n}, [&](std::size_t i) {
            out[indices[i]] = in[i];
        });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), dst_n);
    bool ok = (err.max_abs == 0.0);
    return make_result("E09", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "scatter mismatch");
}

// Scatter with atomic（冲突允许，验证原子语义）
CaseResult run_scatter_atomic(std::size_t n, std::size_t dst_n, const std::string& case_id) {
    std::vector<float> in(n);
    std::vector<std::size_t> indices(n);
    fill_fp32(in, FIXED_SEED);
    fill_indices(indices, n, FIXED_SEED ^ 0x3333, dst_n);
    // reference：累加语义（同一 dst bin 累加所有 in[i]）
    std::vector<double> ref(dst_n, 0.0);
    for (std::size_t i = 0; i < n; ++i) ref[indices[i]] += static_cast<double>(in[i]);

    std::vector<std::atomic<double>> atomic_out(dst_n);
    for (auto& a : atomic_out) a.store(0.0, std::memory_order_relaxed);

    auto tm = measure_timing([&] {
        for (auto& a : atomic_out) a.store(0.0, std::memory_order_relaxed);
        parallel_for(KernelId::Scatter, Range1D{0, n}, [&](std::size_t i) {
            // CAS 累加 double
            double cur = atomic_out[indices[i]].load(std::memory_order_relaxed);
            double new_val = cur + static_cast<double>(in[i]);
            while (!atomic_out[indices[i]].compare_exchange_weak(cur, new_val,
                       std::memory_order_relaxed, std::memory_order_relaxed)) {
                new_val = cur + static_cast<double>(in[i]);
            }
        });
    });

    std::vector<double> out(dst_n);
    for (std::size_t i = 0; i < dst_n; ++i) out[i] = atomic_out[i].load(std::memory_order_relaxed);

    ErrorStats err;
    for (std::size_t i = 0; i < dst_n; ++i) {
        double diff = std::fabs(out[i] - ref[i]);
        if (diff > err.max_abs) err.max_abs = diff;
    }
    bool ok = err.max_abs <= 1e-4 + 1e-4 * 10.0;
    return make_result("E09", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "atomic scatter mismatch");
}

} // anonymous namespace

TEST(E09GatherScatter, GatherDense1K)    { auto r = run_gather(1<<10, 1<<10, "gather_dense_1K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, GatherSparse1K)   { auto r = run_gather(1<<10, 1<<14, "gather_sparse_1K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, GatherLarge64K)   { auto r = run_gather(1<<16, 1<<16, "gather_large_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterUnique1K)  { auto r = run_scatter_unique(1<<10, 1<<12, "scatter_unique_1K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterUnique64K) { auto r = run_scatter_unique(1<<16, 1<<18, "scatter_unique_64K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterAtomic1K)  { auto r = run_scatter_atomic(1<<10, 1<<8, "scatter_atomic_1K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterAtomic64K) { auto r = run_scatter_atomic(1<<16, 1<<10, "scatter_atomic_64K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, GatherMixed)      { auto r = run_gather(1<<14, 1<<12, "gather_mixed_16K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e09() {
    return {
        run_gather(1<<10, 1<<10, "gather_dense_1K"),
        run_gather(1<<10, 1<<14, "gather_sparse_1K"),
        run_gather(1<<16, 1<<16, "gather_large_64K"),
        run_scatter_unique(1<<10, 1<<12, "scatter_unique_1K"),
        run_scatter_unique(1<<16, 1<<18, "scatter_unique_64K"),
        run_scatter_atomic(1<<10, 1<<8, "scatter_atomic_1K"),
        run_scatter_atomic(1<<16, 1<<10, "scatter_atomic_64K"),
        run_gather(1<<14, 1<<12, "gather_mixed_16K"),
    };
}
