// lib/acr/tests/classic/e09_gather_scatter.cpp — E09 Gather/Scatter/SpMV
// 验证能力（17 §14）：不规则内存、稀疏度
//
// Phase H 扩展：
//   - index 模式：identity、reverse、prime stride、随机 permutation
//   - active fraction：1/5/10/25/50/100%
//   - 无冲突 scatter（唯一索引）+ 整数 atomic scatter（累加语义）
//   - 固定稀疏矩阵 SpMV（CSR 格式）
//   - exact 或 FP64 参考
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== 索引模式 =====
enum class IndexMode { Identity, Reverse, PrimeStride, RandomPerm };

const char* index_mode_name(IndexMode m) {
    switch (m) {
        case IndexMode::Identity:    return "identity";
        case IndexMode::Reverse:     return "reverse";
        case IndexMode::PrimeStride: return "prime";
        case IndexMode::RandomPerm:  return "random";
    }
    return "unknown";
}

// 生成索引（size n，范围 [0, range_n)）
std::vector<std::size_t> make_indices(std::size_t n, std::size_t range_n,
                                      IndexMode mode, std::uint64_t seed) {
    std::vector<std::size_t> idx(n);
    switch (mode) {
        case IndexMode::Identity:
            for (std::size_t i = 0; i < n; ++i) idx[i] = i % range_n;
            break;
        case IndexMode::Reverse:
            for (std::size_t i = 0; i < n; ++i) idx[i] = (range_n - 1 - (i % range_n));
            break;
        case IndexMode::PrimeStride: {
            // 用质数步长 stride，idx[i] = (i * stride) % range_n
            // 选一个与 range_n 互质的 stride（简化：用小质数）
            std::size_t stride = 1009;  // 质数
            while (range_n > 1 && std::gcd(stride, range_n) != 1) ++stride;
            for (std::size_t i = 0; i < n; ++i) idx[i] = (i * stride) % range_n;
            break;
        }
        case IndexMode::RandomPerm: {
            LCG rng(seed);
            for (std::size_t i = 0; i < n; ++i) idx[i] = rng.next() % range_n;
            break;
        }
    }
    return idx;
}

void fill_fp32(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0);
}

void fill_u32(std::vector<std::uint32_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<std::uint32_t>(rng.next() & 0xFFFFFFFFu);
}

// ===== Gather: out[i] = in[indices[i]] =====
CaseResult run_gather(std::size_t n, std::size_t src_n, IndexMode mode,
                      const std::string& case_id) {
    std::vector<float> src(src_n), out(n, 0.0f), ref(n, 0.0f);
    auto indices = make_indices(n, src_n, mode, FIXED_SEED ^ 0x1111);
    fill_fp32(src, FIXED_SEED);
    for (std::size_t i = 0; i < n; ++i) ref[i] = src[indices[i]];

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Gather, Range1D{0, n}, [&](std::size_t i) {
            out[i] = src[indices[i]];
        });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), n);
    bool ok = (err.max_abs == 0.0);  // gather bit-exact
    return make_result("E09", case_id, "fp32", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gather mismatch",
                       "cpu", "cpu");
}

// ===== 整数 Gather（uint32）=====
CaseResult run_gather_u32(std::size_t n, std::size_t src_n, IndexMode mode,
                          const std::string& case_id) {
    std::vector<std::uint32_t> src(src_n), out(n, 0), ref(n, 0);
    auto indices = make_indices(n, src_n, mode, FIXED_SEED ^ 0x2222);
    fill_u32(src, FIXED_SEED);
    for (std::size_t i = 0; i < n; ++i) ref[i] = src[indices[i]];

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Gather, Range1D{0, n}, [&](std::size_t i) {
            out[i] = src[indices[i]];
        });
    });

    ErrorStats err;
    bool ok = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (out[i] != ref[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(out[i]) -
                                                         static_cast<long long>(ref[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E09", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gather u32 mismatch",
                       "cpu", "cpu");
}

// ===== Scatter（无冲突，唯一索引）=====
// 生成 n 个唯一索引（Floyd 采样），active fraction 控制有效写入数
CaseResult run_scatter_unique(std::size_t n, std::size_t dst_n, double active_frac,
                              const std::string& case_id) {
    std::size_t active = static_cast<std::size_t>(static_cast<double>(n) * active_frac);
    if (active > n) active = n;
    std::vector<float> in(n), out(dst_n, 0.0f), ref(dst_n, 0.0f);
    fill_fp32(in, FIXED_SEED);
    // 生成 active 个唯一索引
    LCG rng(FIXED_SEED ^ 0x3333);
    std::vector<bool> used(dst_n, false);
    std::vector<std::size_t> indices(active);
    std::size_t placed = 0;
    while (placed < active) {
        std::size_t idx = rng.next() % dst_n;
        if (!used[idx]) { used[idx] = true; indices[placed++] = idx; }
    }
    for (std::size_t i = 0; i < active; ++i) ref[indices[i]] = in[i];

    auto tm = measure_timing([&] {
        std::fill(out.begin(), out.end(), 0.0f);
        parallel_for(KernelId::Scatter, Range1D{0, active}, [&](std::size_t i) {
            out[indices[i]] = in[i];
        });
    });

    auto err = compute_errors<float>(out.data(), ref.data(), dst_n);
    bool ok = (err.max_abs == 0.0);
    return make_result("E09", case_id, "fp32", active, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "scatter unique mismatch",
                       "cpu", "cpu");
}

// ===== 整数 atomic scatter（累加语义，冲突允许）=====
CaseResult run_scatter_atomic_u32(std::size_t n, std::size_t dst_n,
                                  const std::string& case_id) {
    std::vector<std::uint32_t> in(n);
    std::vector<std::size_t> indices(n);
    fill_u32(in, FIXED_SEED);
    auto idx = make_indices(n, dst_n, IndexMode::RandomPerm, FIXED_SEED ^ 0x4444);
    indices = idx;
    // reference：累加语义
    std::vector<std::uint64_t> ref(dst_n, 0);
    for (std::size_t i = 0; i < n; ++i) ref[indices[i]] += in[i];

    std::vector<std::atomic<std::uint64_t>> atomic_out(dst_n);
    for (auto& a : atomic_out) a.store(0, std::memory_order_relaxed);

    auto tm = measure_timing([&] {
        for (auto& a : atomic_out) a.store(0, std::memory_order_relaxed);
        parallel_for(KernelId::Scatter, Range1D{0, n}, [&](std::size_t i) {
            atomic_out[indices[i]].fetch_add(in[i], std::memory_order_relaxed);
        });
    });

    ErrorStats err;
    bool ok = true;
    for (std::size_t i = 0; i < dst_n; ++i) {
        std::uint64_t v = atomic_out[i].load(std::memory_order_relaxed);
        if (v != ref[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(v) -
                                                         static_cast<long long>(ref[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E09", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "atomic scatter u32 mismatch",
                       "cpu", "cpu");
}

// ===== SpMV（CSR 格式稀疏矩阵-向量乘）=====
// y = A * x，A 是固定稀疏矩阵（确定性生成）
CaseResult run_spmv_csr(std::size_t n_rows, std::size_t n_cols, std::size_t nnz_per_row,
                        const std::string& case_id) {
    std::size_t nnz = n_rows * nnz_per_row;
    // 构造 CSR：每行 nnz_per_row 个非零元，列索引确定性
    std::vector<std::size_t> row_ptr(n_rows + 1);
    std::vector<std::size_t> col_idx(nnz);
    std::vector<float> vals(nnz);
    LCG rng(FIXED_SEED ^ 0x5555);
    row_ptr[0] = 0;
    for (std::size_t r = 0; r < n_rows; ++r) {
        for (std::size_t k = 0; k < nnz_per_row; ++k) {
            col_idx[r * nnz_per_row + k] = rng.next() % n_cols;
            vals[r * nnz_per_row + k] = static_cast<float>(rng.next_double() * 2.0 - 1.0);
        }
        row_ptr[r + 1] = row_ptr[r] + nnz_per_row;
    }
    std::vector<float> x(n_cols), y(n_rows, 0.0f), ref(n_rows, 0.0f);
    fill_fp32(x, FIXED_SEED ^ 0x6666);
    // reference
    for (std::size_t r = 0; r < n_rows; ++r) {
        double s = 0.0;
        for (std::size_t k = row_ptr[r]; k < row_ptr[r + 1]; ++k) {
            s += static_cast<double>(vals[k]) * x[col_idx[k]];
        }
        ref[r] = static_cast<float>(s);
    }

    auto tm = measure_timing([&] {
        std::fill(y.begin(), y.end(), 0.0f);
        parallel_for(KernelId::Gather, Range1D{0, n_rows}, [&](std::size_t r) {
            float s = 0.0f;
            for (std::size_t k = row_ptr[r]; k < row_ptr[r + 1]; ++k) {
                s += vals[k] * x[col_idx[k]];
            }
            y[r] = s;
        });
    });

    auto err = compute_errors<float>(y.data(), ref.data(), n_rows);
    bool ok = err.max_abs <= 1e-4 + 1e-4 * static_cast<double>(nnz_per_row);
    return make_result("E09", case_id, "fp32", nnz, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "spmv mismatch",
                       "cpu", "cpu");
}

} // anonymous namespace

// ===== 向后兼容 TEST =====
TEST(E09GatherScatter, GatherDense1K)    { auto r = run_gather(1<<10, 1<<10, IndexMode::Identity, "gather_dense_1K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, GatherSparse1K)   { auto r = run_gather(1<<10, 1<<14, IndexMode::RandomPerm, "gather_sparse_1K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterUnique1K)  { auto r = run_scatter_unique(1<<10, 1<<12, 1.0, "scatter_unique_1K");          ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterAtomic1K)  { auto r = run_scatter_atomic_u32(1<<10, 1<<8, "scatter_atomic_1K");            ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §14 规范 =====
// index 模式 × gather
TEST(E09GatherScatter, Gather_Identity_4K)  { auto r = run_gather(1<<12, 1<<12, IndexMode::Identity, "gather_identity_4K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Gather_Reverse_4K)   { auto r = run_gather(1<<12, 1<<12, IndexMode::Reverse, "gather_reverse_4K");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Gather_Prime_4K)     { auto r = run_gather(1<<12, 1<<12, IndexMode::PrimeStride, "gather_prime_4K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Gather_Random_4K)    { auto r = run_gather(1<<12, 1<<12, IndexMode::RandomPerm, "gather_random_4K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, GatherU32_Random_4K) { auto r = run_gather_u32(1<<12, 1<<12, IndexMode::RandomPerm, "gather_u32_random_4K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// active fraction × scatter（无冲突）
TEST(E09GatherScatter, Scatter_Active1pct)  { auto r = run_scatter_unique(1<<14, 1<<14, 0.01, "scatter_active_1pct");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Scatter_Active5pct)  { auto r = run_scatter_unique(1<<14, 1<<14, 0.05, "scatter_active_5pct");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Scatter_Active10pct) { auto r = run_scatter_unique(1<<14, 1<<14, 0.10, "scatter_active_10pct"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Scatter_Active25pct) { auto r = run_scatter_unique(1<<14, 1<<14, 0.25, "scatter_active_25pct"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Scatter_Active50pct) { auto r = run_scatter_unique(1<<14, 1<<14, 0.50, "scatter_active_50pct"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, Scatter_Active100pct){ auto r = run_scatter_unique(1<<14, 1<<14, 1.00, "scatter_active_100pct");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 整数 atomic scatter
TEST(E09GatherScatter, ScatterAtomicU32_4K) { auto r = run_scatter_atomic_u32(1<<12, 1<<8, "scatter_atomic_u32_4K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, ScatterAtomicU32_64K){ auto r = run_scatter_atomic_u32(1<<16, 1<<10, "scatter_atomic_u32_64K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// SpMV
TEST(E09GatherScatter, SpMV_1Kx1K_10nnz)   { auto r = run_spmv_csr(1<<10, 1<<10, 10, "spmv_1kx1k_10nnz");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E09GatherScatter, SpMV_4Kx4K_20nnz)   { auto r = run_spmv_csr(1<<12, 1<<12, 20, "spmv_4kx4k_20nnz");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e09() {
    return {
        // 向后兼容
        run_gather(1<<10, 1<<10, IndexMode::Identity, "gather_dense_1K"),
        run_gather(1<<10, 1<<14, IndexMode::RandomPerm, "gather_sparse_1K"),
        run_scatter_unique(1<<10, 1<<12, 1.0, "scatter_unique_1K"),
        run_scatter_atomic_u32(1<<10, 1<<8, "scatter_atomic_1K"),
        // Phase H 扩展：index 模式
        run_gather(1<<12, 1<<12, IndexMode::Identity, "gather_identity_4K"),
        run_gather(1<<12, 1<<12, IndexMode::Reverse, "gather_reverse_4K"),
        run_gather(1<<12, 1<<12, IndexMode::PrimeStride, "gather_prime_4K"),
        run_gather(1<<12, 1<<12, IndexMode::RandomPerm, "gather_random_4K"),
        run_gather_u32(1<<12, 1<<12, IndexMode::RandomPerm, "gather_u32_random_4K"),
        // active fraction
        run_scatter_unique(1<<14, 1<<14, 0.01, "scatter_active_1pct"),
        run_scatter_unique(1<<14, 1<<14, 0.05, "scatter_active_5pct"),
        run_scatter_unique(1<<14, 1<<14, 0.10, "scatter_active_10pct"),
        run_scatter_unique(1<<14, 1<<14, 0.25, "scatter_active_25pct"),
        run_scatter_unique(1<<14, 1<<14, 0.50, "scatter_active_50pct"),
        run_scatter_unique(1<<14, 1<<14, 1.00, "scatter_active_100pct"),
        // atomic scatter
        run_scatter_atomic_u32(1<<12, 1<<8, "scatter_atomic_u32_4K"),
        run_scatter_atomic_u32(1<<16, 1<<10, "scatter_atomic_u32_64K"),
        // SpMV
        run_spmv_csr(1<<10, 1<<10, 10, "spmv_1kx1k_10nnz"),
        run_spmv_csr(1<<12, 1<<12, 20, "spmv_4kx4k_20nnz"),
    };
}
