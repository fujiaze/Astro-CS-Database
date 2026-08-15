// lib/acr/tests/classic/e06_resample.cpp — E06 Bilinear Affine Resampling
// 验证能力（17 §11）：坐标计算、gather、边界策略
//
// Phase H 扩展：
// - 变换：固定平移、旋转、非均匀 scale
// - 图像：棋盘、斜坡、随机
// - 尺寸：513×509、2048²、4096²（大尺寸限 FP32 以控制时长）
// - 精度：FP32/FP64
// - 边界：zero/clamp
// - 参考：FP64 坐标和插值
//
// 仿射变换：dst(x,y) = src(a00*x + a01*y + tx, a10*x + a11*y + ty)
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== 边界策略 =====
enum class BorderMode { Clamp, Zero };

// ===== 图像填充模式 =====
enum class ImagePattern { Checkerboard, Ramp, Random };

void fill_image_fp32(std::vector<float>& img, std::size_t w, std::size_t h,
                     ImagePattern pat, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            std::size_t i = y * w + x;
            switch (pat) {
                case ImagePattern::Checkerboard:
                    img[i] = static_cast<float>(((x ^ y) & 1u) ? 1.0f : 0.0f);
                    break;
                case ImagePattern::Ramp:
                    img[i] = static_cast<float>(static_cast<double>(x) /
                                                static_cast<double>(w > 1 ? w - 1 : 1));
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
                case ImagePattern::Checkerboard:
                    img[i] = static_cast<double>(((x ^ y) & 1u) ? 1.0 : 0.0);
                    break;
                case ImagePattern::Ramp:
                    img[i] = static_cast<double>(x) /
                             static_cast<double>(w > 1 ? w - 1 : 1);
                    break;
                case ImagePattern::Random:
                    img[i] = rng.next_double() * 2.0 - 1.0;
                    break;
            }
        }
    }
}

inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 双线性采样（FP64 reference，clamp/zero 边界）
inline double sample_bilinear_fp64(const std::vector<double>& src,
                                    std::size_t w, std::size_t h,
                                    double fx, double fy, BorderMode border) {
    if (fx < 0.0 || fx > static_cast<double>(w - 1) ||
        fy < 0.0 || fy > static_cast<double>(h - 1)) {
        if (border == BorderMode::Zero) return 0.0;
        // clamp
        fx = clampd(fx, 0.0, static_cast<double>(w - 1));
        fy = clampd(fy, 0.0, static_cast<double>(h - 1));
    }
    long x0 = static_cast<long>(std::floor(fx));
    long y0 = static_cast<long>(std::floor(fy));
    long x1 = (x0 + 1 < static_cast<long>(w)) ? x0 + 1 : x0;
    long y1 = (y0 + 1 < static_cast<long>(h)) ? y0 + 1 : y0;
    double dx = fx - static_cast<double>(x0);
    double dy = fy - static_cast<double>(y0);
    double v00 = src[y0 * w + x0];
    double v01 = src[y0 * w + x1];
    double v10 = src[y1 * w + x0];
    double v11 = src[y1 * w + x1];
    double v0 = v00 * (1.0 - dx) + v01 * dx;
    double v1 = v10 * (1.0 - dx) + v11 * dx;
    return v0 * (1.0 - dy) + v1 * dy;
}

// 双线性采样（FP32，clamp/zero 边界）
inline float sample_bilinear_fp32(const std::vector<float>& src,
                                   std::size_t w, std::size_t h,
                                   float fx, float fy, BorderMode border) {
    if (fx < 0.0f || fx > static_cast<float>(w - 1) ||
        fy < 0.0f || fy > static_cast<float>(h - 1)) {
        if (border == BorderMode::Zero) return 0.0f;
        fx = fx < 0.0f ? 0.0f : (fx > static_cast<float>(w - 1) ?
                                  static_cast<float>(w - 1) : fx);
        fy = fy < 0.0f ? 0.0f : (fy > static_cast<float>(h - 1) ?
                                  static_cast<float>(h - 1) : fy);
    }
    long x0 = static_cast<long>(std::floor(fx));
    long y0 = static_cast<long>(std::floor(fy));
    long x1 = (x0 + 1 < static_cast<long>(w)) ? x0 + 1 : x0;
    long y1 = (y0 + 1 < static_cast<long>(h)) ? y0 + 1 : y0;
    float dx = fx - static_cast<float>(x0);
    float dy = fy - static_cast<float>(y0);
    float v00 = src[y0 * w + x0];
    float v01 = src[y0 * w + x1];
    float v10 = src[y1 * w + x0];
    float v11 = src[y1 * w + x1];
    float v0 = v00 * (1.0f - dx) + v01 * dx;
    float v1 = v10 * (1.0f - dx) + v11 * dx;
    return v0 * (1.0f - dy) + v1 * dy;
}

// ===== 仿射变换 reference（FP64）=====
void ref_affine_fp64(const std::vector<double>& src, std::vector<double>& dst,
                     std::size_t src_w, std::size_t src_h,
                     std::size_t dst_w, std::size_t dst_h,
                     double a00, double a01, double tx,
                     double a10, double a11, double ty,
                     BorderMode border) {
    for (std::size_t y = 0; y < dst_h; ++y) {
        for (std::size_t x = 0; x < dst_w; ++x) {
            double sx = a00 * x + a01 * y + tx;
            double sy = a10 * x + a11 * y + ty;
            dst[y * dst_w + x] = sample_bilinear_fp64(src, src_w, src_h, sx, sy, border);
        }
    }
}

// ===== FP32 affine =====
CaseResult run_affine_fp32(std::size_t src_w, std::size_t src_h,
                            std::size_t dst_w, std::size_t dst_h,
                            float a00, float a01, float tx,
                            float a10, float a11, float ty,
                            ImagePattern pat, BorderMode border,
                            const std::string& case_id) {
    std::vector<float> src(src_w * src_h), dst(dst_w * dst_h, 0.0f);
    std::vector<double> src64(src_w * src_h), ref_dst(dst_w * dst_h, 0.0);
    fill_image_fp32(src, src_w, src_h, pat, FIXED_SEED);
    fill_image_fp64(src64, src_w, src_h, pat, FIXED_SEED);
    ref_affine_fp64(src64, ref_dst, src_w, src_h, dst_w, dst_h,
                    a00, a01, tx, a10, a11, ty, border);

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Custom, Extent2D{dst_w, dst_h},
            [&](std::size_t x, std::size_t y) {
                float sx = a00 * x + a01 * y + tx;
                float sy = a10 * x + a11 * y + ty;
                dst[y * dst_w + x] = sample_bilinear_fp32(src, src_w, src_h, sx, sy, border);
            });
    });

    // 与 FP64 reference 比较
    ErrorStats err;
    for (std::size_t i = 0; i < dst_w * dst_h; ++i) {
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
    err.rmse = (dst_w * dst_h > 0) ?
        std::sqrt(err.rmse / static_cast<double>(dst_w * dst_h)) : 0.0;

    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E06", case_id, "fp32", dst_w * dst_h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "affine fp32 mismatch");
}

// ===== FP64 affine =====
CaseResult run_affine_fp64(std::size_t src_w, std::size_t src_h,
                            std::size_t dst_w, std::size_t dst_h,
                            double a00, double a01, double tx,
                            double a10, double a11, double ty,
                            ImagePattern pat, BorderMode border,
                            const std::string& case_id) {
    std::vector<double> src(src_w * src_h), dst(dst_w * dst_h, 0.0), ref_dst(dst_w * dst_h, 0.0);
    fill_image_fp64(src, src_w, src_h, pat, FIXED_SEED);
    ref_affine_fp64(src, ref_dst, src_w, src_h, dst_w, dst_h,
                    a00, a01, tx, a10, a11, ty, border);

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Custom, Extent2D{dst_w, dst_h},
            [&](std::size_t x, std::size_t y) {
                double sx = a00 * x + a01 * y + tx;
                double sy = a10 * x + a11 * y + ty;
                dst[y * dst_w + x] = sample_bilinear_fp64(src, src_w, src_h, sx, sy, border);
            });
    });

    auto err = compute_errors<double>(dst.data(), ref_dst.data(), dst_w * dst_h);
    bool ok = err.max_abs <= 1e-12 + 1e-11 * 2.0;
    return make_result("E06", case_id, "fp64", dst_w * dst_h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "affine fp64 mismatch");
}

// ===== 向后兼容：旧签名（identity 默认 clamp）=====
CaseResult run_affine_compat(std::size_t src_w, std::size_t src_h,
                              std::size_t dst_w, std::size_t dst_h,
                              float a00, float a01, float tx,
                              float a10, float a11, float ty,
                              const std::string& case_id) {
    return run_affine_fp32(src_w, src_h, dst_w, dst_h,
                           a00, a01, tx, a10, a11, ty,
                           ImagePattern::Random, BorderMode::Clamp, case_id);
}

} // anonymous namespace

// ===== 向后兼容 TEST =====
TEST(E06Resample, Identity)      { auto r = run_affine_compat(64, 64, 64, 64, 1, 0, 0, 0, 1, 0, "identity");          ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Upscale2x)     { auto r = run_affine_compat(32, 32, 64, 64, 0.5f, 0, 0, 0, 0.5f, 0, "upscale_2x");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, DownscaleHalf) { auto r = run_affine_compat(64, 64, 32, 32, 2.0f, 0, 0, 0, 2.0f, 0, "downscale_half");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Rotate45)      {
    const float s = std::sin(3.14159265f / 4.0f);
    const float c = std::cos(3.14159265f / 4.0f);
    auto r = run_affine_compat(64, 64, 64, 64, c, -s, 32, s, c, 32, "rotate45");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, Translate)     { auto r = run_affine_compat(64, 64, 64, 64, 1, 0, 5.5f, 0, 1, -3.5f, "translate");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Edge13)        { auto r = run_affine_compat(13, 13, 13, 13, 1, 0, 0, 0, 1, 0, "edge13");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, NonSquareRect) { auto r = run_affine_compat(16, 32, 32, 16, 0.5f, 0, 0, 0, 2.0f, 0, "nonsquare_rect");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Large256)      { auto r = run_affine_compat(256, 256, 256, 256, 1, 0, 0, 0, 1, 0, "large256");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §11 规范 =====
// 513×509（非方阵）
TEST(E06Resample, Translate_513x509_Checker_Clamp_FP32) {
    auto r = run_affine_fp32(513, 509, 513, 509, 1.0f, 0.0f, 5.5f, 0.0f, 1.0f, -3.5f,
                              ImagePattern::Checkerboard, BorderMode::Clamp,
                              "translate_513x509_chk_clamp_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, Rotate_513x509_Ramp_Zero_FP32) {
    const float s = std::sin(3.14159265f / 6.0f);
    const float c = std::cos(3.14159265f / 6.0f);
    auto r = run_affine_fp32(513, 509, 513, 509, c, -s, 256, s, c, 254,
                              ImagePattern::Ramp, BorderMode::Zero,
                              "rotate_513x509_ramp_zero_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, NonUniformScale_513x509_Random_Clamp_FP64) {
    auto r = run_affine_fp64(513, 509, 513, 509, 1.5, 0.0, 0.0, 0.0, 0.75, 0.0,
                              ImagePattern::Random, BorderMode::Clamp,
                              "nuscale_513x509_rand_clamp_fp64");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}

// 2048²（方阵）
TEST(E06Resample, Translate_2048_Checker_Clamp_FP32) {
    auto r = run_affine_fp32(2048, 2048, 2048, 2048, 1.0f, 0.0f, 10.5f, 0.0f, 1.0f, -7.5f,
                              ImagePattern::Checkerboard, BorderMode::Clamp,
                              "translate_2048_chk_clamp_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, Rotate_2048_Ramp_Zero_FP32) {
    const float s = std::sin(3.14159265f / 6.0f);
    const float c = std::cos(3.14159265f / 6.0f);
    auto r = run_affine_fp32(2048, 2048, 2048, 2048, c, -s, 1024, s, c, 1024,
                              ImagePattern::Ramp, BorderMode::Zero,
                              "rotate_2048_ramp_zero_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, NonUniformScale_2048_Random_Clamp_FP64) {
    auto r = run_affine_fp64(2048, 2048, 2048, 2048, 1.25, 0.0, 0.0, 0.0, 0.8, 0.0,
                              ImagePattern::Random, BorderMode::Clamp,
                              "nuscale_2048_rand_clamp_fp64");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}

// 4096²（大方阵，限 FP32 控制时长）
TEST(E06Resample, Translate_4096_Checker_Clamp_FP32) {
    auto r = run_affine_fp32(4096, 4096, 4096, 4096, 1.0f, 0.0f, 15.5f, 0.0f, 1.0f, -12.5f,
                              ImagePattern::Checkerboard, BorderMode::Clamp,
                              "translate_4096_chk_clamp_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, Rotate_4096_Ramp_Zero_FP32) {
    const float s = std::sin(3.14159265f / 8.0f);
    const float c = std::cos(3.14159265f / 8.0f);
    auto r = run_affine_fp32(4096, 4096, 4096, 4096, c, -s, 2048, s, c, 2048,
                              ImagePattern::Ramp, BorderMode::Zero,
                              "rotate_4096_ramp_zero_fp32");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}

extern "C" std::vector<CaseResult> run_e06() {
    const float s45 = std::sin(3.14159265f / 4.0f);
    const float c45 = std::cos(3.14159265f / 4.0f);
    const float s30 = std::sin(3.14159265f / 6.0f);
    const float c30 = std::cos(3.14159265f / 6.0f);
    const float s22 = std::sin(3.14159265f / 8.0f);
    const float c22 = std::cos(3.14159265f / 8.0f);
    return {
        // 向后兼容
        run_affine_compat(64, 64, 64, 64, 1, 0, 0, 0, 1, 0, "identity"),
        run_affine_compat(32, 32, 64, 64, 0.5f, 0, 0, 0, 0.5f, 0, "upscale_2x"),
        run_affine_compat(64, 64, 32, 32, 2.0f, 0, 0, 0, 2.0f, 0, "downscale_half"),
        run_affine_compat(64, 64, 64, 64, c45, -s45, 32, s45, c45, 32, "rotate45"),
        run_affine_compat(64, 64, 64, 64, 1, 0, 5.5f, 0, 1, -3.5f, "translate"),
        run_affine_compat(13, 13, 13, 13, 1, 0, 0, 0, 1, 0, "edge13"),
        run_affine_compat(16, 32, 32, 16, 0.5f, 0, 0, 0, 2.0f, 0, "nonsquare_rect"),
        run_affine_compat(256, 256, 256, 256, 1, 0, 0, 0, 1, 0, "large256"),
        // Phase H 扩展：513×509
        run_affine_fp32(513, 509, 513, 509, 1.0f, 0.0f, 5.5f, 0.0f, 1.0f, -3.5f,
                        ImagePattern::Checkerboard, BorderMode::Clamp, "translate_513x509_chk_clamp_fp32"),
        run_affine_fp32(513, 509, 513, 509, c30, -s30, 256, s30, c30, 254,
                        ImagePattern::Ramp, BorderMode::Zero, "rotate_513x509_ramp_zero_fp32"),
        run_affine_fp64(513, 509, 513, 509, 1.5, 0.0, 0.0, 0.0, 0.75, 0.0,
                        ImagePattern::Random, BorderMode::Clamp, "nuscale_513x509_rand_clamp_fp64"),
        // 2048²
        run_affine_fp32(2048, 2048, 2048, 2048, 1.0f, 0.0f, 10.5f, 0.0f, 1.0f, -7.5f,
                        ImagePattern::Checkerboard, BorderMode::Clamp, "translate_2048_chk_clamp_fp32"),
        run_affine_fp32(2048, 2048, 2048, 2048, c30, -s30, 1024, s30, c30, 1024,
                        ImagePattern::Ramp, BorderMode::Zero, "rotate_2048_ramp_zero_fp32"),
        run_affine_fp64(2048, 2048, 2048, 2048, 1.25, 0.0, 0.0, 0.0, 0.8, 0.0,
                        ImagePattern::Random, BorderMode::Clamp, "nuscale_2048_rand_clamp_fp64"),
        // 4096²
        run_affine_fp32(4096, 4096, 4096, 4096, 1.0f, 0.0f, 15.5f, 0.0f, 1.0f, -12.5f,
                        ImagePattern::Checkerboard, BorderMode::Clamp, "translate_4096_chk_clamp_fp32"),
        run_affine_fp32(4096, 4096, 4096, 4096, c22, -s22, 2048, s22, c22, 2048,
                        ImagePattern::Ramp, BorderMode::Zero, "rotate_4096_ramp_zero_fp32"),
    };
}
