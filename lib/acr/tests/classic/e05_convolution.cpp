// lib/acr/tests/classic/e05_convolution.cpp — E05 Direct 2D Convolution
// 验证能力（17 §9）：Tile halo、二维拆分、边界策略、大核 direct
//
// Phase H 扩展：
// - 核：3×3 Sobel、5×5 Gaussian、7×7 固定、15×15 Gaussian/box、31×31 不可分离固定随机
// - 图像：512²、2048²（大核限 512² 以控制测试时长）
// - 模式：脉冲、梯度、固定随机
// - 精度：FP32/FP64
// - 边界：clamp/mirror
// - 参考：CPU scalar FP64 accumulation
// - 输出：max error、RMSE、吞吐、总时间
//
// 注：direct 31×31 on 2048² ≈ 4G MACs，单元测试中限制大核尺寸以保持合理运行时长。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== 边界策略 =====
enum class BorderMode { Clamp, Mirror };

inline long clamp_idx(long v, long maxv) {
    if (v < 0) return 0;
    if (v >= maxv) return maxv - 1;
    return v;
}

inline long mirror_idx(long v, long maxv) {
    // mirror 边界：反射索引
    while (v < 0 || v >= maxv) {
        if (v < 0) v = -v;
        if (v >= maxv) v = 2 * maxv - 2 - v;
    }
    return v;
}

inline long border_idx(long v, long maxv, BorderMode mode) {
    return mode == BorderMode::Clamp ? clamp_idx(v, maxv) : mirror_idx(v, maxv);
}

// ===== 图像填充模式 =====
enum class ImagePattern { Pulse, Gradient, Random };

void fill_image_fp32(std::vector<float>& img, std::size_t w, std::size_t h,
                     ImagePattern pat, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            std::size_t i = y * w + x;
            switch (pat) {
                case ImagePattern::Pulse:
                    // 中心脉冲，其余 0
                    img[i] = (x == w / 2 && y == h / 2) ? 1.0f : 0.0f;
                    break;
                case ImagePattern::Gradient:
                    img[i] = static_cast<float>(static_cast<double>(x + y) /
                                                static_cast<double>(w + h - 2));
                    break;
                case ImagePattern::Random:
                    img[i] = static_cast<float>(rng.next_double() * 2.0 - 1.0);
                    break;
            }
        }
    }
}

void fill_image_fp64(std::vector<double>& img, std::size_t w, std::size_t h,
                     ImagePattern pat, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            std::size_t i = y * w + x;
            switch (pat) {
                case ImagePattern::Pulse:
                    img[i] = (x == w / 2 && y == h / 2) ? 1.0 : 0.0;
                    break;
                case ImagePattern::Gradient:
                    img[i] = static_cast<double>(x + y) /
                             static_cast<double>(w + h - 2);
                    break;
                case ImagePattern::Random:
                    img[i] = rng.next_double() * 2.0 - 1.0;
                    break;
            }
        }
    }
}

// ===== 核生成 =====
// 3×3 Sobel X: [[-1,0,1],[-2,0,2],[-1,0,1]]
std::vector<double> make_sobel3x3() {
    return {-1, 0, 1, -2, 0, 2, -1, 0, 1};
}

// 5×5 Gaussian（归一化，sigma≈1.0）
std::vector<double> make_gaussian5x5() {
    // 简化 Gaussian 5×5
    std::vector<double> k = {
        1,  4,  7,  4, 1,
        4, 16, 26, 16, 4,
        7, 26, 41, 26, 7,
        4, 16, 26, 16, 4,
        1,  4,  7,  4, 1
    };
    double sum = 0;
    for (auto v : k) sum += v;
    for (auto& v : k) v /= sum;
    return k;
}

// 7×7 固定核（确定性随机，归一化）
std::vector<double> make_fixed7x7() {
    std::vector<double> k(49);
    LCG rng(FIXED_SEED ^ 0x707ULL);
    double sum = 0;
    for (auto& v : k) { v = rng.next_double() * 2.0 - 1.0; sum += std::fabs(v); }
    if (sum > 1e-12) for (auto& v : k) v /= sum;
    return k;
}

// 15×15 Gaussian（归一化）
std::vector<double> make_gaussian15x15() {
    std::vector<double> k(225);
    double sigma = 3.0;
    int half = 7;
    double sum = 0;
    for (int y = -half; y <= half; ++y) {
        for (int x = -half; x <= half; ++x) {
            double g = std::exp(-(static_cast<double>(x * x + y * y)) / (2.0 * sigma * sigma));
            k[(y + half) * 15 + (x + half)] = g;
            sum += g;
        }
    }
    for (auto& v : k) v /= sum;
    return k;
}

// 31×31 不可分离固定随机核（归一化）
std::vector<double> make_random31x31() {
    std::vector<double> k(961);
    LCG rng(FIXED_SEED ^ 0x313131ULL);
    double sum = 0;
    for (auto& v : k) { v = rng.next_double() * 2.0 - 1.0; sum += std::fabs(v); }
    if (sum > 1e-12) for (auto& v : k) v /= sum;
    return k;
}

// ===== 串行 reference 卷积（FP64 accumulation，clamp/mirror 边界）=====
void ref_convolve_fp64(const std::vector<double>& src, std::vector<double>& dst,
                       std::size_t w, std::size_t h,
                       const std::vector<double>& kernel, int ksize,
                       BorderMode border) {
    int half = ksize / 2;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            double sum = 0.0;
            for (int ky = 0; ky < ksize; ++ky) {
                for (int kx = 0; kx < ksize; ++kx) {
                    long sx = border_idx(static_cast<long>(x) + kx - half,
                                         static_cast<long>(w), border);
                    long sy = border_idx(static_cast<long>(y) + ky - half,
                                         static_cast<long>(h), border);
                    sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                }
            }
            dst[y * w + x] = sum;
        }
    }
}

// ===== 并行卷积 FP32（parallel_for_2d，每像素独立）=====
CaseResult run_conv2d_fp32(std::size_t w, std::size_t h, int ksize,
                            const std::vector<double>& kernel,
                            ImagePattern pat, BorderMode border,
                            const std::string& case_id) {
    std::vector<float> src(w * h), dst(w * h, 0.0f);
    std::vector<double> src64(w * h), ref_dst(w * h, 0.0);
    fill_image_fp32(src, w, h, pat, FIXED_SEED);
    fill_image_fp64(src64, w, h, pat, FIXED_SEED);
    ref_convolve_fp64(src64, ref_dst, w, h, kernel, ksize, border);

    int half = ksize / 2;
    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Convolution2D, Extent2D{w, h},
            [&](std::size_t x, std::size_t y) {
                float sum = 0.0f;
                for (int ky = 0; ky < ksize; ++ky) {
                    for (int kx = 0; kx < ksize; ++kx) {
                        long sx = border_idx(static_cast<long>(x) + kx - half,
                                             static_cast<long>(w), border);
                        long sy = border_idx(static_cast<long>(y) + ky - half,
                                             static_cast<long>(h), border);
                        sum += src[sy * w + sx] *
                               static_cast<float>(kernel[ky * ksize + kx]);
                    }
                }
                dst[y * w + x] = sum;
            });
    });

    // 与 FP64 reference 比较
    ErrorStats err;
    for (std::size_t i = 0; i < w * h; ++i) {
        double a = static_cast<double>(dst[i]);
        double r = ref_dst[i];
        double diff = std::fabs(a - r);
        if (diff > err.max_abs) err.max_abs = diff;
        double max_abs = std::fabs(a) > std::fabs(r) ? std::fabs(a) : std::fabs(r);
        if (max_abs > 1e-30) {
            double rel = diff / max_abs;
            if (rel > err.max_rel) err.max_rel = rel;
        }
        err.rmse += (a - r) * (a - r);
    }
    err.rmse = (w * h > 0) ? std::sqrt(err.rmse / static_cast<double>(w * h)) : 0.0;

    // FP32 容差：1e-5 + 5e-5*max（但大核累加误差更大，按 K² 放宽）
    double tol = 1e-5 + 5e-5 * static_cast<double>(ksize * ksize) * 0.5;
    bool ok = err.max_abs <= tol;
    return make_result("E05", case_id, "fp32", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "fp32 conv mismatch",
                       "cpu", "cpu");
}

// ===== 并行卷积 FP64（parallel_for_2d）=====
CaseResult run_conv2d_fp64(std::size_t w, std::size_t h, int ksize,
                            const std::vector<double>& kernel,
                            ImagePattern pat, BorderMode border,
                            const std::string& case_id) {
    std::vector<double> src(w * h), dst(w * h, 0.0), ref_dst(w * h, 0.0);
    fill_image_fp64(src, w, h, pat, FIXED_SEED);
    ref_convolve_fp64(src, ref_dst, w, h, kernel, ksize, border);

    int half = ksize / 2;
    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Convolution2D, Extent2D{w, h},
            [&](std::size_t x, std::size_t y) {
                double sum = 0.0;
                for (int ky = 0; ky < ksize; ++ky) {
                    for (int kx = 0; kx < ksize; ++kx) {
                        long sx = border_idx(static_cast<long>(x) + kx - half,
                                             static_cast<long>(w), border);
                        long sy = border_idx(static_cast<long>(y) + ky - half,
                                             static_cast<long>(h), border);
                        sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                    }
                }
                dst[y * w + x] = sum;
            });
    });

    // FP64 容差：1e-12 + 1e-11*max（大核累加误差放宽）
    auto err = compute_errors<double>(dst.data(), ref_dst.data(), w * h);
    double tol = 1e-12 + 1e-11 * static_cast<double>(ksize * ksize) * 0.5;
    bool ok = err.max_abs <= tol;
    return make_result("E05", case_id, "fp64", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "fp64 conv mismatch",
                       "cpu", "cpu");
}

// ===== Tile 化卷积（保留向后兼容）=====
CaseResult run_conv_tiled(std::size_t w, std::size_t h, int ksize,
                          std::size_t tile_w, std::size_t tile_h,
                          const std::string& case_id) {
    std::vector<float> src(w * h), dst(w * h, 0.0f);
    std::vector<double> ref_dst(w * h, 0.0);
    fill_image_fp32(src, w, h, ImagePattern::Random, FIXED_SEED);
    std::vector<double> kernel_d((std::size_t)ksize * (std::size_t)ksize);
    LCG krng(FIXED_SEED ^ 0xCAFE);
    double ksum = 0.0;
    for (auto& k : kernel_d) { k = krng.next_double() * 2.0 - 1.0; ksum += std::fabs(k); }
    if (ksum > 1e-12) for (auto& k : kernel_d) k /= ksum;
    std::vector<float> kernel((std::size_t)ksize * (std::size_t)ksize);
    for (int i = 0; i < ksize * ksize; ++i) kernel[i] = static_cast<float>(kernel_d[i]);

    std::vector<double> src64(w * h);
    fill_image_fp64(src64, w, h, ImagePattern::Random, FIXED_SEED);
    ref_convolve_fp64(src64, ref_dst, w, h, kernel_d, ksize, BorderMode::Clamp);

    int half = ksize / 2;
    auto tm = measure_timing([&] {
        parallel_tiles(KernelId::Convolution2D, Extent2D{w, h}, TileShape{tile_w, tile_h},
            [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
                for (std::size_t j = 0; j < th; ++j) {
                    for (std::size_t i = 0; i < tw; ++i) {
                        std::size_t x = tx * tile_w + i;
                        std::size_t y = ty * tile_h + j;
                        if (x >= w || y >= h) continue;
                        float sum = 0.0f;
                        for (int ky = 0; ky < ksize; ++ky) {
                            for (int kx = 0; kx < ksize; ++kx) {
                                long sx = clamp_idx(static_cast<long>(x) + kx - half,
                                                    static_cast<long>(w));
                                long sy = clamp_idx(static_cast<long>(y) + ky - half,
                                                    static_cast<long>(h));
                                sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                            }
                        }
                        dst[y * w + x] = sum;
                    }
                }
            });
    });

    ErrorStats err;
    for (std::size_t i = 0; i < w * h; ++i) {
        double diff = std::fabs(static_cast<double>(dst[i]) - ref_dst[i]);
        if (diff > err.max_abs) err.max_abs = diff;
    }
    double tol = 1e-5 + 5e-5 * static_cast<double>(ksize * ksize) * 0.5;
    bool ok = err.max_abs <= tol;
    return make_result("E05", case_id, "fp32", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "tiled convolution mismatch");
}

} // anonymous namespace

// ===== 向后兼容 TEST =====
TEST(E05Conv, Conv3x3_64)      { auto r = run_conv2d_fp32(64, 64, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv3x3_256)     { auto r = run_conv2d_fp32(256, 256, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_256"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv5x5_64)      { auto r = run_conv2d_fp32(64, 64, 5, make_gaussian5x5(), ImagePattern::Random, BorderMode::Clamp, "conv5x5_64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv3x3_Edge13)  { auto r = run_conv2d_fp32(13, 13, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_edge13"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_64_8)   { auto r = run_conv_tiled(64, 64, 3, 8, 8, "tiled3x3_64_8"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_Edge10) { auto r = run_conv_tiled(10, 10, 3, 4, 4, "tiled3x3_edge10"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled5x5_128_16) { auto r = run_conv_tiled(128, 128, 5, 16, 16, "tiled5x5_128_16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_Rect)   { auto r = run_conv_tiled(32, 64, 3, 8, 8, "tiled3x3_rect"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：规范核 × 尺寸 × 模式 × 精度 × 边界 =====
// 3×3 Sobel
TEST(E05Conv, Sobel3x3_512_Random_Clamp_FP32)  { auto r = run_conv2d_fp32(512, 512, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "sobel3_512_rand_clamp_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Sobel3x3_512_Gradient_Mirror_FP32) { auto r = run_conv2d_fp32(512, 512, 3, make_sobel3x3(), ImagePattern::Gradient, BorderMode::Mirror, "sobel3_512_grad_mirror_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Sobel3x3_2048_Pulse_Clamp_FP64)  { auto r = run_conv2d_fp64(2048, 2048, 3, make_sobel3x3(), ImagePattern::Pulse, BorderMode::Clamp, "sobel3_2048_pulse_clamp_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Sobel3x3_2048_Random_Mirror_FP64) { auto r = run_conv2d_fp64(2048, 2048, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Mirror, "sobel3_2048_rand_mirror_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 5×5 Gaussian
TEST(E05Conv, Gaussian5x5_512_Random_Clamp_FP32) { auto r = run_conv2d_fp32(512, 512, 5, make_gaussian5x5(), ImagePattern::Random, BorderMode::Clamp, "gauss5_512_rand_clamp_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Gaussian5x5_512_Gradient_Mirror_FP32) { auto r = run_conv2d_fp32(512, 512, 5, make_gaussian5x5(), ImagePattern::Gradient, BorderMode::Mirror, "gauss5_512_grad_mirror_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Gaussian5x5_2048_Pulse_Clamp_FP64) { auto r = run_conv2d_fp64(2048, 2048, 5, make_gaussian5x5(), ImagePattern::Pulse, BorderMode::Clamp, "gauss5_2048_pulse_clamp_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 7×7 固定核
TEST(E05Conv, Fixed7x7_512_Random_Clamp_FP32) { auto r = run_conv2d_fp32(512, 512, 7, make_fixed7x7(), ImagePattern::Random, BorderMode::Clamp, "fixed7_512_rand_clamp_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Fixed7x7_512_Gradient_Mirror_FP32) { auto r = run_conv2d_fp32(512, 512, 7, make_fixed7x7(), ImagePattern::Gradient, BorderMode::Mirror, "fixed7_512_grad_mirror_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Fixed7x7_1024_Random_Clamp_FP64) { auto r = run_conv2d_fp64(1024, 1024, 7, make_fixed7x7(), ImagePattern::Random, BorderMode::Clamp, "fixed7_1024_rand_clamp_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 15×15 Gaussian（大核，限 512² 控制时长）
TEST(E05Conv, Gaussian15x15_512_Random_Clamp_FP32) { auto r = run_conv2d_fp32(512, 512, 15, make_gaussian15x15(), ImagePattern::Random, BorderMode::Clamp, "gauss15_512_rand_clamp_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Gaussian15x15_512_Pulse_Mirror_FP32) { auto r = run_conv2d_fp32(512, 512, 15, make_gaussian15x15(), ImagePattern::Pulse, BorderMode::Mirror, "gauss15_512_pulse_mirror_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Gaussian15x15_512_Gradient_Clamp_FP64) { auto r = run_conv2d_fp64(512, 512, 15, make_gaussian15x15(), ImagePattern::Gradient, BorderMode::Clamp, "gauss15_512_grad_clamp_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 31×31 不可分离固定随机核（大核，限 512² 控制时长）
TEST(E05Conv, Random31x31_512_Random_Clamp_FP32) { auto r = run_conv2d_fp32(512, 512, 31, make_random31x31(), ImagePattern::Random, BorderMode::Clamp, "rand31_512_rand_clamp_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Random31x31_512_Pulse_Mirror_FP32) { auto r = run_conv2d_fp32(512, 512, 31, make_random31x31(), ImagePattern::Pulse, BorderMode::Mirror, "rand31_512_pulse_mirror_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Random31x31_512_Gradient_Clamp_FP64) { auto r = run_conv2d_fp64(512, 512, 31, make_random31x31(), ImagePattern::Gradient, BorderMode::Clamp, "rand31_512_grad_clamp_fp64"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e05() {
    return {
        // 向后兼容
        run_conv2d_fp32(64, 64, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_64"),
        run_conv2d_fp32(256, 256, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_256"),
        run_conv2d_fp32(64, 64, 5, make_gaussian5x5(), ImagePattern::Random, BorderMode::Clamp, "conv5x5_64"),
        run_conv2d_fp32(13, 13, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "conv3x3_edge13"),
        run_conv_tiled(64, 64, 3, 8, 8, "tiled3x3_64_8"),
        run_conv_tiled(10, 10, 3, 4, 4, "tiled3x3_edge10"),
        run_conv_tiled(128, 128, 5, 16, 16, "tiled5x5_128_16"),
        run_conv_tiled(32, 64, 3, 8, 8, "tiled3x3_rect"),
        // Phase H 扩展
        run_conv2d_fp32(512, 512, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Clamp, "sobel3_512_rand_clamp_fp32"),
        run_conv2d_fp32(512, 512, 3, make_sobel3x3(), ImagePattern::Gradient, BorderMode::Mirror, "sobel3_512_grad_mirror_fp32"),
        run_conv2d_fp64(2048, 2048, 3, make_sobel3x3(), ImagePattern::Pulse, BorderMode::Clamp, "sobel3_2048_pulse_clamp_fp64"),
        run_conv2d_fp64(2048, 2048, 3, make_sobel3x3(), ImagePattern::Random, BorderMode::Mirror, "sobel3_2048_rand_mirror_fp64"),
        run_conv2d_fp32(512, 512, 5, make_gaussian5x5(), ImagePattern::Random, BorderMode::Clamp, "gauss5_512_rand_clamp_fp32"),
        run_conv2d_fp32(512, 512, 5, make_gaussian5x5(), ImagePattern::Gradient, BorderMode::Mirror, "gauss5_512_grad_mirror_fp32"),
        run_conv2d_fp64(2048, 2048, 5, make_gaussian5x5(), ImagePattern::Pulse, BorderMode::Clamp, "gauss5_2048_pulse_clamp_fp64"),
        run_conv2d_fp32(512, 512, 7, make_fixed7x7(), ImagePattern::Random, BorderMode::Clamp, "fixed7_512_rand_clamp_fp32"),
        run_conv2d_fp32(512, 512, 7, make_fixed7x7(), ImagePattern::Gradient, BorderMode::Mirror, "fixed7_512_grad_mirror_fp32"),
        run_conv2d_fp64(1024, 1024, 7, make_fixed7x7(), ImagePattern::Random, BorderMode::Clamp, "fixed7_1024_rand_clamp_fp64"),
        run_conv2d_fp32(512, 512, 15, make_gaussian15x15(), ImagePattern::Random, BorderMode::Clamp, "gauss15_512_rand_clamp_fp32"),
        run_conv2d_fp32(512, 512, 15, make_gaussian15x15(), ImagePattern::Pulse, BorderMode::Mirror, "gauss15_512_pulse_mirror_fp32"),
        run_conv2d_fp64(512, 512, 15, make_gaussian15x15(), ImagePattern::Gradient, BorderMode::Clamp, "gauss15_512_grad_clamp_fp64"),
        run_conv2d_fp32(512, 512, 31, make_random31x31(), ImagePattern::Random, BorderMode::Clamp, "rand31_512_rand_clamp_fp32"),
        run_conv2d_fp32(512, 512, 31, make_random31x31(), ImagePattern::Pulse, BorderMode::Mirror, "rand31_512_pulse_mirror_fp32"),
        run_conv2d_fp64(512, 512, 31, make_random31x31(), ImagePattern::Gradient, BorderMode::Clamp, "rand31_512_grad_clamp_fp64"),
    };
}
