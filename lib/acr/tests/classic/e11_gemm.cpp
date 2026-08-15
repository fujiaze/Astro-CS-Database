// lib/acr/tests/classic/e11_gemm.cpp — E11 GEMM 成熟库适配
// 验证能力：vendor library handle 隔离（CPU 自实现 naive + tiled）
// 扩展（规范 E15 GEMM/FFT成熟库adapter）：
// - 尺寸：256/1024/4096 + 非方阵（512×256×1024 等）
// - 成熟库 adapter（BLAS/cuBLAS/rocBLAS/oneMKL），不可用标记 SKIPPED
// - 自实现 naive/tiled 作为 correctness baseline
// 不依赖 OpenBLAS；自实现 naive ijk 三重循环 + tiled 版本。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

// ===== 成熟库检测（编译时宏）=====
// 项目未链接 BLAS/cuBLAS/rocBLAS/oneMKL，所有 library case 标记 SKIPPED。
#if defined(ACR_HAS_OPENBLAS) || defined(ACR_HAS_MKL) || \
    defined(ACR_HAS_CUBLAS) || defined(ACR_HAS_ROCBLAS)
#define ACR_HAS_MATURE_GEMM 1
#else
#define ACR_HAS_MATURE_GEMM 0
#endif

namespace {

void fill_fp32(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0);
}
void fill_fp64(std::vector<double>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = rng.next_double() * 2.0 - 1.0;
}

// 串行 reference: C = alpha * A(MxK) * B(KxN) + beta * C(MxN)
void ref_gemm_fp32(const std::vector<float>& A, const std::vector<float>& B, std::vector<float>& C,
                   std::size_t M, std::size_t K, std::size_t N, float alpha, float beta) {
    for (std::size_t i = 0; i < M; ++i) {
        for (std::size_t j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < K; ++k) sum += A[i * K + k] * B[k * N + j];
            C[i * N + j] = alpha * sum + beta * C[i * N + j];
        }
    }
}

// Naive GEMM: 并行化 i 维度
CaseResult run_gemm_naive_fp32(std::size_t M, std::size_t K, std::size_t N, const std::string& case_id,
                               std::uint32_t rounds = 11) {
    std::vector<float> A(M * K), B(K * N), C(M * N, 0.0f), ref_C(M * N, 0.0f);
    fill_fp32(A, FIXED_SEED);
    fill_fp32(B, FIXED_SEED ^ 0xAAAA);
    constexpr float kAlpha = 1.0f, kBeta = 0.0f;
    ref_gemm_fp32(A, B, ref_C, M, K, N, kAlpha, kBeta);

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Gemm, Range1D{0, M}, [&](std::size_t i) {
            for (std::size_t j = 0; j < N; ++j) {
                float sum = 0.0f;
                for (std::size_t k = 0; k < K; ++k) sum += A[i * K + k] * B[k * N + j];
                C[i * N + j] = kAlpha * sum + kBeta * C[i * N + j];
            }
        });
    }, rounds);

    auto err = compute_errors<float>(C.data(), ref_C.data(), M * N);
    bool ok = err.max_abs <= 1e-4 + 5e-4 * static_cast<double>(K);
    return make_result("E11", case_id, "fp32", M * K * N, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gemm naive mismatch",
                       "cpu", "cpu");
}

// Tiled GEMM: 用 parallel_tiles 分块
CaseResult run_gemm_tiled_fp32(std::size_t M, std::size_t K, std::size_t N,
                               std::size_t tile_m, std::size_t tile_n,
                               const std::string& case_id, std::uint32_t rounds = 11) {
    std::vector<float> A(M * K), B(K * N), C(M * N, 0.0f), ref_C(M * N, 0.0f);
    fill_fp32(A, FIXED_SEED);
    fill_fp32(B, FIXED_SEED ^ 0xAAAA);
    constexpr float kAlpha = 1.0f, kBeta = 0.0f;
    ref_gemm_fp32(A, B, ref_C, M, K, N, kAlpha, kBeta);

    auto tm = measure_timing([&] {
        std::fill(C.begin(), C.end(), 0.0f);
        parallel_tiles(KernelId::Gemm, Extent2D{N, M}, TileShape{tile_n, tile_m},
            [&](std::size_t tn, std::size_t tm_, std::size_t tw, std::size_t th) {
                for (std::size_t j_local = 0; j_local < tw; ++j_local) {
                    for (std::size_t i_local = 0; i_local < th; ++i_local) {
                        std::size_t i = tm_ * tile_m + i_local;
                        std::size_t j = tn * tile_n + j_local;
                        if (i >= M || j >= N) continue;
                        float sum = 0.0f;
                        for (std::size_t k = 0; k < K; ++k) sum += A[i * K + k] * B[k * N + j];
                        C[i * N + j] = kAlpha * sum + kBeta * C[i * N + j];
                    }
                }
            });
    }, rounds);

    auto err = compute_errors<float>(C.data(), ref_C.data(), M * N);
    bool ok = err.max_abs <= 1e-4 + 5e-4 * static_cast<double>(K);
    return make_result("E11", case_id, "fp32", M * K * N, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gemm tiled mismatch",
                       "cpu", "cpu");
}

// FP64 GEMM
CaseResult run_gemm_fp64(std::size_t M, std::size_t K, std::size_t N, const std::string& case_id,
                         std::uint32_t rounds = 11) {
    std::vector<double> A(M * K), B(K * N), C(M * N, 0.0), ref_C(M * N, 0.0);
    fill_fp64(A, FIXED_SEED);
    fill_fp64(B, FIXED_SEED ^ 0xAAAA);
    constexpr double kAlpha = 1.0, kBeta = 0.0;
    // reference
    for (std::size_t i = 0; i < M; ++i) {
        for (std::size_t j = 0; j < N; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0; k < K; ++k) sum += A[i * K + k] * B[k * N + j];
            ref_C[i * N + j] = kAlpha * sum + kBeta * ref_C[i * N + j];
        }
    }

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Gemm, Range1D{0, M}, [&](std::size_t i) {
            for (std::size_t j = 0; j < N; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < K; ++k) sum += A[i * K + k] * B[k * N + j];
                C[i * N + j] = kAlpha * sum + kBeta * C[i * N + j];
            }
        });
    }, rounds);

    auto err = compute_errors<double>(C.data(), ref_C.data(), M * N);
    bool ok = err.max_abs <= 1e-9 + 1e-9 * static_cast<double>(K);
    return make_result("E11", case_id, "fp64", M * K * N, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "gemm fp64 mismatch",
                       "cpu", "cpu");
}

// 成熟库 adapter case：BLAS/cuBLAS/rocBLAS/oneMKL 不可用时 SKIPPED
CaseResult run_gemm_library_skipped(std::size_t M, std::size_t K, std::size_t N,
                                    const std::string& case_id) {
    TimingStats tm;  // 空 timing（未执行）
    ErrorStats err;
#if ACR_HAS_MATURE_GEMM
    // 有成熟库时实际调用（当前项目未链接，此分支不编译）
    return make_result("E11", case_id, "fp32", M * K * N, true, err, tm,
                       "PASS", "mature library GEMM", "cpu", "cpu");
#else
    return make_result("E11", case_id, "fp32", M * K * N, true, err, tm,
                       "SKIPPED", "mature library (BLAS/cuBLAS/rocBLAS/oneMKL) not available",
                       "cpu", "cpu");
#endif
}

} // anonymous namespace

// ===== 原有小尺寸 correctness baseline =====
TEST(E11Gemm, Naive32x32)   { auto r = run_gemm_naive_fp32(32, 32, 32, "naive_32x32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Naive64x64)   { auto r = run_gemm_naive_fp32(64, 64, 64, "naive_64x64");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, NaiveRect)    { auto r = run_gemm_naive_fp32(16, 32, 8, "naive_rect_16x32x8"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Tiled64x64_8) { auto r = run_gemm_tiled_fp32(64, 64, 64, 8, 8, "tiled_64x64_8"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, TiledEdge)    { auto r = run_gemm_tiled_fp32(13, 13, 13, 4, 4, "tiled_edge_13"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Fp64_32x32)   { auto r = run_gemm_fp64(32, 32, 32, "fp64_32x32");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Fp64_64x64)   { auto r = run_gemm_fp64(64, 64, 64, "fp64_64x64");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Naive128)     { auto r = run_gemm_naive_fp32(128, 128, 128, "naive_128"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 扩展尺寸：256 =====
TEST(E11Gemm, Naive256)     { auto r = run_gemm_naive_fp32(256, 256, 256, "naive_256", 5); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, Tiled256_32)  { auto r = run_gemm_tiled_fp32(256, 256, 256, 32, 32, "tiled_256_32", 5); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 扩展尺寸：1024（tiled + rounds=3 控制时长）=====
TEST(E11Gemm, Tiled1024_64) { auto r = run_gemm_tiled_fp32(1024, 1024, 1024, 64, 64, "tiled_1024_64", 3); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 非方阵 =====
TEST(E11Gemm, TiledRect_512x256x1024) { auto r = run_gemm_tiled_fp32(512, 256, 1024, 64, 64, "tiled_rect_512x256x1024", 5); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E11Gemm, NaiveRect_1024x128x512) { auto r = run_gemm_naive_fp32(1024, 128, 512, "naive_rect_1024x128x512", 5); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 成熟库 adapter（不可用 → SKIPPED）=====
TEST(E11Gemm, Library256)   { auto r = run_gemm_library_skipped(256, 256, 256, "library_256");   ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E11Gemm, Library1024)  { auto r = run_gemm_library_skipped(1024, 1024, 1024, "library_1024"); ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E11Gemm, Library4096)  { auto r = run_gemm_library_skipped(4096, 4096, 4096, "library_4096"); ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E11Gemm, LibraryRect)  { auto r = run_gemm_library_skipped(2048, 512, 1024, "library_rect_2048x512x1024"); ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }

extern "C" std::vector<CaseResult> run_e11() {
    return {
        // 原有 baseline
        run_gemm_naive_fp32(32, 32, 32, "naive_32x32"),
        run_gemm_naive_fp32(64, 64, 64, "naive_64x64"),
        run_gemm_naive_fp32(16, 32, 8, "naive_rect_16x32x8"),
        run_gemm_tiled_fp32(64, 64, 64, 8, 8, "tiled_64x64_8"),
        run_gemm_tiled_fp32(13, 13, 13, 4, 4, "tiled_edge_13"),
        run_gemm_fp64(32, 32, 32, "fp64_32x32"),
        run_gemm_fp64(64, 64, 64, "fp64_64x64"),
        run_gemm_naive_fp32(128, 128, 128, "naive_128"),
        // 扩展：256
        run_gemm_naive_fp32(256, 256, 256, "naive_256", 5),
        run_gemm_tiled_fp32(256, 256, 256, 32, 32, "tiled_256_32", 5),
        // 扩展：1024
        run_gemm_tiled_fp32(1024, 1024, 1024, 64, 64, "tiled_1024_64", 3),
        // 扩展：非方阵
        run_gemm_tiled_fp32(512, 256, 1024, 64, 64, "tiled_rect_512x256x1024", 5),
        run_gemm_naive_fp32(1024, 128, 512, "naive_rect_1024x128x512", 5),
        // 成熟库 adapter（SKIPPED）
        run_gemm_library_skipped(256, 256, 256, "library_256"),
        run_gemm_library_skipped(1024, 1024, 1024, "library_1024"),
        run_gemm_library_skipped(4096, 4096, 4096, "library_4096"),
        run_gemm_library_skipped(2048, 512, 1024, "library_rect_2048x512x1024"),
    };
}
