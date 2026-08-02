// lib/acr/tests/classic/e12_fft.cpp — E12 FFT Round-trip
// 验证能力：专用库（自实现 naive DFT + Cooley-Tukey 迭代）
// Cases: fft_naive / fft_ct_pow2 / fft_roundtrip / fft_inverse
// 不依赖 FFTW；自实现。round-trip: ifft(fft(x)) ≈ x。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <complex>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

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

// 串行 naive DFT: O(n^2)
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

// Round-trip: ifft(fft(x)) ≈ x
CaseResult run_fft_roundtrip(std::size_t n, const std::string& case_id) {
    std::vector<CD> in(n), work, out;
    fill_complex(in, FIXED_SEED);

    auto tm = measure_timing([&] {
        work = in;
        iterative_fft(work, /*inverse=*/false);
        iterative_fft(work, /*inverse=*/true);
        out = work;
    });

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
    bool ok = err.max_abs <= 1e-10 + 1e-10 * static_cast<double>(n);
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

} // anonymous namespace

TEST(E12Fft, CT_vs_naive_8)    { auto r = run_fft_vs_naive(8, "ct_vs_naive_8");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, CT_vs_naive_64)   { auto r = run_fft_vs_naive(64, "ct_vs_naive_64");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, CT_vs_naive_256)  { auto r = run_fft_vs_naive(256, "ct_vs_naive_256");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_8)      { auto r = run_fft_roundtrip(8, "roundtrip_8");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_64)     { auto r = run_fft_roundtrip(64, "roundtrip_64");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, Roundtrip_256)    { auto r = run_fft_roundtrip(256, "roundtrip_256"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, ParallelNaive16)  { auto r = run_fft_parallel_naive(16, "parallel_naive_16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E12Fft, ParallelNaive64)  { auto r = run_fft_parallel_naive(64, "parallel_naive_64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

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
    };
}
