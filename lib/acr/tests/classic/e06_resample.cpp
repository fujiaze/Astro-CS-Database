// lib/acr/tests/classic/e06_resample.cpp — E06 Bilinear Affine Resampling
// 验证能力：坐标计算、gather
// Cases: upscale_2x / downscale_0_5x / rotate_45 / affine_identity
// 仿射变换：dst(x,y) = src(a00*x + a01*y + tx, a10*x + a11*y + ty)
// 边界 clamp，双线性插值。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

void fill_image(std::vector<float>& img, std::size_t w, std::size_t h, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < w * h; ++i) {
        img[i] = static_cast<float>(rng.next_double() * 2.0 - 1.0);
    }
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 双线性采样（clamp 边界）
inline float sample_bilinear(const std::vector<float>& src, std::size_t w, std::size_t h,
                             float fx, float fy) {
    fx = clampf(fx, 0.0f, static_cast<float>(w - 1));
    fy = clampf(fy, 0.0f, static_cast<float>(h - 1));
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

// 仿射变换 reference
void ref_affine(const std::vector<float>& src, std::vector<float>& dst,
                std::size_t src_w, std::size_t src_h,
                std::size_t dst_w, std::size_t dst_h,
                float a00, float a01, float tx,
                float a10, float a11, float ty) {
    for (std::size_t y = 0; y < dst_h; ++y) {
        for (std::size_t x = 0; x < dst_w; ++x) {
            float sx = a00 * x + a01 * y + tx;
            float sy = a10 * x + a11 * y + ty;
            dst[y * dst_w + x] = sample_bilinear(src, src_w, src_h, sx, sy);
        }
    }
}

CaseResult run_affine(std::size_t src_w, std::size_t src_h,
                      std::size_t dst_w, std::size_t dst_h,
                      float a00, float a01, float tx,
                      float a10, float a11, float ty,
                      const std::string& case_id) {
    std::vector<float> src(src_w * src_h), dst(dst_w * dst_h, 0.0f), ref_dst(dst_w * dst_h, 0.0f);
    fill_image(src, src_w, src_h, FIXED_SEED);
    ref_affine(src, ref_dst, src_w, src_h, dst_w, dst_h, a00, a01, tx, a10, a11, ty);

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Custom, Extent2D{dst_w, dst_h},
            [&](std::size_t x, std::size_t y) {
                float sx = a00 * x + a01 * y + tx;
                float sy = a10 * x + a11 * y + ty;
                dst[y * dst_w + x] = sample_bilinear(src, src_w, src_h, sx, sy);
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), dst_w * dst_h);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E06", case_id, "fp32", dst_w * dst_h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "affine resample mismatch");
}

} // anonymous namespace

TEST(E06Resample, Identity)      { auto r = run_affine(64, 64, 64, 64, 1, 0, 0, 0, 1, 0, "identity");          ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Upscale2x)     { auto r = run_affine(32, 32, 64, 64, 0.5f, 0, 0, 0, 0.5f, 0, "upscale_2x");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, DownscaleHalf) { auto r = run_affine(64, 64, 32, 32, 2.0f, 0, 0, 0, 2.0f, 0, "downscale_half");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Rotate45)      {
    const float s = std::sin(3.14159265f / 4.0f);
    const float c = std::cos(3.14159265f / 4.0f);
    auto r = run_affine(64, 64, 64, 64, c, -s, 32, s, c, 32, "rotate45");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
TEST(E06Resample, Translate)     { auto r = run_affine(64, 64, 64, 64, 1, 0, 5.5f, 0, 1, -3.5f, "translate");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Edge13)        { auto r = run_affine(13, 13, 13, 13, 1, 0, 0, 0, 1, 0, "edge13");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, NonSquareRect) { auto r = run_affine(16, 32, 32, 16, 0.5f, 0, 0, 0, 2.0f, 0, "nonsquare_rect");ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E06Resample, Large256)      { auto r = run_affine(256, 256, 256, 256, 1, 0, 0, 0, 1, 0, "large256");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e06() {
    const float s45 = std::sin(3.14159265f / 4.0f);
    const float c45 = std::cos(3.14159265f / 4.0f);
    return {
        run_affine(64, 64, 64, 64, 1, 0, 0, 0, 1, 0, "identity"),
        run_affine(32, 32, 64, 64, 0.5f, 0, 0, 0, 0.5f, 0, "upscale_2x"),
        run_affine(64, 64, 32, 32, 2.0f, 0, 0, 0, 2.0f, 0, "downscale_half"),
        run_affine(64, 64, 64, 64, c45, -s45, 32, s45, c45, 32, "rotate45"),
        run_affine(64, 64, 64, 64, 1, 0, 5.5f, 0, 1, -3.5f, "translate"),
        run_affine(13, 13, 13, 13, 1, 0, 0, 0, 1, 0, "edge13"),
        run_affine(16, 32, 32, 16, 0.5f, 0, 0, 0, 2.0f, 0, "nonsquare_rect"),
        run_affine(256, 256, 256, 256, 1, 0, 0, 0, 1, 0, "large256"),
    };
}
