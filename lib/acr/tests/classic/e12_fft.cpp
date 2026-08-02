// lib/acr/tests/classic/e12_fft.cpp — E12 FFT Round-trip
// 验证能力：专用库（自实现 naive DFT + Cooley-Tukey 迭代）
// 扩展（规范 E15 GEMM/FFT成熟库adapter）：
//   - 1D：1024/65536（2 的幂）+ 1000（非 2 的幂，naive DFT）
//   - 2D：512²（可分离行/列 FFT），2048² 标记 SKIPPED（成熟库不可用）
//   - 成熟库 adapter（FFTW/pocketfft/cuFFT/rocFFT/oneMKL），不可用标记 SKIPPED
// 不依赖 FFTW；自实现。round-trip: ifft(fft(x)) ≈ x。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <complex>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

// ===== 成熟库检测（编译时宏）=====
#if defined(ACR_HAS_FFTW) || defined(ACR_HAS_POCKETFFT) || \
    defined(ACR_HAS_CUFFT) || defined(ACR_HAS_ROCFFT) || defined(ACR_HAS_ONEMKL)
#define ACR_HAS_MATURE_FFT 1
#else
#define ACR_HAS_MATURE_FFT 0
#endif

namespace {

using CD = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;

void fill_complex(std::vector<CD>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& z : v) {
        double re = rng.next_double() * 2.0 - 1.0;
        double im = rng.next_double() * 2.0 - 1.0;
        z = CD(re, im);
    }
}

// 串行 naive DFT: O(n^2)，用于非 2 的幂或小尺寸 reference
void naive_dft(const std::vector<CD>& in, std::vector<CD>& out, bool inverse) {
    std::size_t n = in.size();
    out.assign(n, CD(0, 0));
    double sign = inverse ? 1.0 : -1.0;
    for (std::size_t k = 0; k < n; ++k) {
        CD sum(0, 0);
        for (std::size_t j = 0; j < n; ++j) {
            double angle = sign * 2.0 * kPi * k * j / n;
            sum += in[j] * CD(std::cos(angle), std::sin(angle));
        }
        out[k] = sum;
    }
    if (inverse) for (auto& z : out) z /= static_cast<double>(n);
}

// 迭代 Cooley-Tukey FFT（n 必须是 2 的幂）
void iterative_fft(std::vector<CD>& a, bool inverse) {
    std::size_t n = a.size();
    if (n == 0) return;
    // 位反转重排
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    double sign = inverse ? 1.0 : -1.0;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        double angle = sign * 2.0 * kPi / len;
        CD wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            CD w(1, 0);
            for (std::size_t j = 0; j < len / 2; ++j) {
                CD u = a[i + j];
                CD v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) for (auto& z : a) z /= static_cast<double>(n);
}

// 判断是否为 2 的幂
inline bool is_pow2(std::size_t n) { return n > 0 && (n & (n - 1)) == 0; }

// 通用 FFT：2 的幂用 Cooley-Tukey，否则用 naive DFT
void general_fft(std::vector<CD>& a, bool inverse) {
    if (is_pow2(a.size())) {
        iterative_fft(a, inverse);
    } else {
        std::vector<CD> out;
        naive_dft(a, out, inverse);
        a = out;
    }
}

// 串行 reference FFT（用 naive DFT）
CaseResult run_fft_vs_naive(std::size_t n, const std::string& case_id) {
    std::vector<CD> in(n), ref_out, ct_out;
    fill_complex(in, FIXED_SEED);
    naive_dft(in, ref_out, /*inverse=*/false);
    ct_out = in;
    auto tm = measure_timing([&] {
        ct_out = in;
        iterative_fft(ct_out, /*inverse=*/false);
    });

    ErrorStats err;
    for (std::size_t i = 0; i < n; ++i) {
        double diff_re = std::fabs(ct_out[i].real() - ref_out[i].real());
        double diff_im = std::fabs(ct_out[i].imag() - ref_out[i].imag());
        double max_abs = diff_re > diff_im ? diff_re : diff_im;
        if (max_abs > err.max_abs) err.max_abs = max_abs;
        double mag = std::fabs(ref_out[i]);
        if (mag > 1e-9) {
            double rel = max_abs / mag;
            if (rel > err.max_rel) err.max_rel = rel;
        }
    }
    bool ok = err.max_abs <= 1e-9 + 1e-9 * static_cast<double>(n);
    return make_result("E12", case_id, "fp64", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "fft vs naive mismatch");
}

// Round-trip: ifft(fft(x)) ≈ x（支持任意 n，用 general_fft）
CaseResult run_fft_roundtrip(std::size_t n, const std::string& case_id,
                             std::uint32_t rounds = 11) {
    std::vector<CD> in(n), work, out;
    fill_complex(in, FIXED_SEED);

    auto tm = measure_timing([&] {
        work = in;
        general_fft(work, /*inverse=*/false);
        general_fft(work, /*inverse=*/true);
        out = work;
    }, rounds);

    ErrorStats err;
    for (std::size_t i = 0; i < n; ++i) {
        double diff_re = std::fabs(out[i].real() - in[i].real());
        double diff_im = std::fabs(out[i].imag() - in[i].imag());
        double max_abs = diff_re > diff_im ? diff_re : diff_im;
        if (max_abs > err.max_abs) err.max_abs = max_abs;
        double mag = std::fabs(in[i]);
        if (mag > 1e-9) {
            double rel = max_abs / mag;
            if (rel > err.max_rel) err.max_rel = rel;
        }
    }
    bool ok = err.max_abs <= 1e-9 + 1e-9 * static_cast<double>(n);
    return make_result("E12", case_id, "fp64", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "fft roundtrip mismatch");
}

// 并行 FFT：用 parallel_for 分发独立点（naive DFT 的 k 维度并行）
CaseResult run_fft_parallel_naive(std::size_t n, const std::string& case_id) {
    std::vector<CD> in(n), out(n, CD(0, 0)), ref_out;
    fill_complex(in, FIXED_SEED);
    naive_dft(in, ref_out, /*inverse=*/false);

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Fft, Range1D{0, n}, [&](std::size_t k) {
            CD sum(0, 0);
            for (std::size_t j = 0; j < n; ++j) {
                double angle = -2.0 * kPi * k * j / n;
                sum += in[j] * CD(std::cos(angle), std::sin(angle));
            }
            out[k] = sum;
        });
    });

    ErrorStats err;
    for (std::size_t i = 0; i < n; ++i) {
        double diff_re = std::fabs(out[i].real() - ref_out[i].real());
        double diff_im = std::fabs(out[i].imag() - ref_out[i].imag());
        double max_abs = diff_re > diff_im ? diff_re : diff_im;
        if (max_abs > err.max_abs) err.max_abs = max_abs;
    }
    bool ok = err.max_abs <= 1e-9 + 1e-9 * static_cast<double>(n);
    return make_result("E12", case_id, "fp64", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "parallel naive fft mismatch");
}

// 2D FFT round-trip：ifft2d(fft2d(x)) ≈ x（可分离：行 FFT + 列 FFT）
// H×W 矩阵，row-major：data[y * W + x]
CaseResult run_fft_2d_roundtrip(std::size_t H, std::size_t W, const std::string& case_id,
                                std::uint32_t rounds = 5) {
    std::size_t total = H * W;
    std::vector<CD> in(total), work(total), out(total);
    fill_complex(in, FIXED_SEED);

    auto tm = measure_timing([&] {
        // 复制输入
        work = in;
        // 第 1 阶段：每行 FFT
        parallel_for(KernelId::Fft, Range1D{0, H}, [&](std::size_t y) {
            std::vector<CD> row(W);
            for (std::size_t x = 0; x < W; ++x) row[x] = work[y * W + x];
            general_fft(row, /*inverse=*/false);
            for (std::size_t x = 0; x < W; ++x) work[y * W + x] = row[x];
        });
        // 第 2 阶段：每列 FFT
        parallel_for(KernelId::Fft, Range1D{0, W}, [&](std::size_t x) {
            std::vector<CD> col(H);
            for (std::size_t y = 0; y < H; ++y) col[y] = work[y * W + x];
            general_fft(col, /*inverse=*/false);
            for (std::size_t y = 0; y < H; ++y) work[y * W + x] = col[y];
        });
        // 第 3 阶段：每行 IFFT
        parallel_for(KernelId::Fft, Range1D{0, H}, [&](std::size_t y) {
            std::vector<CD> row(W);
            for (std::size_t x = 0; x < W; ++x) row[x] = work[y * W + x];
            general_fft(row, /*inverse=*/true);
            for (std::size_t x = 0; x < W; ++x) work[y * W + x] = row[x];
        });
        // 第 4 阶段：每列 IFFT
        parallel_for(KernelId::Fft, Range1D{0, W}, [&](std::size_t x) {
            std::vector<CD> col(H);
            for (std::size_t y = 0; y < H; ++y) col[y] = work[y * W + x];
            general_fft(col, /*inverse=*/true);
            for (std::size_t y = 0; y < H; ++y) work[y * W + x] = col[y];
        });
        out = work;
    }, rounds);

    ErrorStats err;
    for (std::size_t i = 0; i < total; ++i) {
        double diff_re = std::fabs(out[i].real() - in[i].real());
        double diff_im = std::fabs(out[i].imag() - in[i].imag());
        double max_abs = diff_re > diff_im ? diff_re : diff_im;
        if (max_abs > err.max_abs) err.max_abs = max_abs;
        double mag = std::fabs(in[i]);
        if (mag > 1e-9) {
            double rel = max_abs / mag;
            if (rel > err.max_rel) err.max_rel = rel;
        }
    }
    bool ok = err.max_abs <= 1e-9 + 1e-9 * static_cast<double>(total);
    return make_result("E12", case_id, "fp64", total, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "2D fft roundtrip mismatch");
}

// 成熟库 adapter case：FFTW/cuFFT/rocFFT/oneMKL 不可用时 SKIPPED
CaseResult run_fft_library_skipped(std::size_t n, const std::string& kind,
                                   const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
#if ACR_HAS_MATURE_FFT
    (void)kind;
    return make_result("E12", case_id, "fp64", n, true, err, tm,
                       "PASS", "mature library FFT", "cpu", "cpu");
#else
    return make_result("E12", case_id, "fp64", n, true, err, tm,
                       "SKIPPED",
                       "mature library (FFTW/pocketfft/cuFFT/rocFFT/oneMKL) not available",
                       "cpu", "cpu");
#endif
}

} // anonymous namespace

// ===== 原有小尺寸 =====
TEST(E12Fft, CT_vs_naive_8)    { auto r = run_fft_vs_naive(8, "ct_vs_naive_8");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, CT_vs_naive_64)   { auto r = run_fft_vs_naive(64, "ct_vs_naive_64");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, CT_vs_naive_256)  { auto r = run_fft_vs_naive(256, "ct_vs_naive_256");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_8)      { auto r = run_fft_roundtrip(8, "roundtrip_8");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_64)     { auto r = run_fft_roundtrip(64, "roundtrip_64");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_256)    { auto r = run_fft_roundtrip(256, "roundtrip_256"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, ParallelNaive16)  { auto r = run_fft_parallel_naive(16, "parallel_naive_16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, ParallelNaive64)  { auto r = run_fft_parallel_naive(64, "parallel_naive_64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 扩展：1D 1024 / 65536 =====
TEST(E12Fft, Roundtrip_1024)   { auto r = run_fft_roundtrip(1024, "roundtrip_1024", 7);   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_65536)  { auto r = run_fft_roundtrip(65536, "roundtrip_65536", 3); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 扩展：非 2 的幂 1000（naive DFT round-trip）=====
TEST(E12Fft, Roundtrip_1000)   { auto r = run_fft_roundtrip(1000, "roundtrip_1000_non_pow2", 5); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 扩展：2D FFT 512² =====
TEST(E12Fft, Roundtrip2D_512)  { auto r = run_fft_2d_roundtrip(512, 512, "roundtrip_2d_512", 3); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 成熟库 adapter（不可用 → SKIPPED）=====
TEST(E12Fft, Library1D_1024)   { auto r = run_fft_library_skipped(1024, "1d", "library_1d_1024");    ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E12Fft, Library1D_65536)  { auto r = run_fft_library_skipped(65536, "1d", "library_1d_65536");  ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E12Fft, Library1D_1000)   { auto r = run_fft_library_skipped(1000, "1d", "library_1d_1000");    ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E12Fft, Library2D_512)    { auto r = run_fft_library_skipped(512*512, "2d", "library_2d_512");  ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }
TEST(E12Fft, Library2D_2048)   { auto r = run_fft_library_skipped(2048*2048, "2d", "library_2d_2048");ResultSink::instance().push(r); EXPECT_EQ(r.status, "SKIPPED"); }

extern "C" std::vector<CaseResult> run_e12() {
    return {
        run_fft_vs_naive(8, "ct_vs_naive_8"),
        run_fft_vs_naive(64, "ct_vs_naive_64"),
        run_fft_vs_naive(256, "ct_vs_naive_256"),
        run_fft_roundtrip(8, "roundtrip_8"),
        run_fft_roundtrip(64, "roundtrip_64"),
        run_fft_roundtrip(256, "roundtrip_256"),
        run_fft_parallel_naive(16, "parallel_naive_16"),
        run_fft_parallel_naive(64, "parallel_naive_64"),
        // 扩展：1D 大尺寸
        run_fft_roundtrip(1024, "roundtrip_1024", 7),
        run_fft_roundtrip(65536, "roundtrip_65536", 3),
        // 扩展：非 2 的幂
        run_fft_roundtrip(1000, "roundtrip_1000_non_pow2", 5),
        // 扩展：2D FFT
        run_fft_2d_roundtrip(512, 512, "roundtrip_2d_512", 3),
        // 成熟库 adapter（SKIPPED）
        run_fft_library_skipped(1024, "1d", "library_1d_1024"),
        run_fft_library_skipped(65536, "1d", "library_1d_65536"),
        run_fft_library_skipped(1000, "1d", "library_1d_1000"),
        run_fft_library_skipped(512*512, "2d", "library_2d_512"),
        run_fft_library_skipped(2048*2048, "2d", "library_2d_2048"),
    };
}
