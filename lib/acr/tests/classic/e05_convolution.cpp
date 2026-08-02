// lib/acr/tests/classic/e05_convolution.cpp — E05 2D Convolution
// 验证能力：Tile halo、二维拆分
// Cases: conv3x3 / conv5x5 / conv3x3_tile / edge_cases
// 边界策略：clamp（replicate edge），与 reference 串行实现对比。
#include "classic_common.hpp"

#include <gtest/gtest.h>

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

// clamp 边界索引
inline long clamp_idx(long v, long maxv) {
    if (v < 0) return 0;
    if (v >= maxv) return maxv - 1;
    return v;
}

// 串行卷积 reference（clamp 边界）
void ref_convolve(const std::vector<float>& src, std::vector<float>& dst,
                  std::size_t w, std::size_t h,
                  const std::vector<float>& kernel, int ksize) {
    int half = ksize / 2;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            float sum = 0.0f;
            for (int ky = 0; ky < ksize; ++ky) {
                for (int kx = 0; kx < ksize; ++kx) {
                    long sx = clamp_idx(static_cast<long>(x) + kx - half, static_cast<long>(w));
                    long sy = clamp_idx(static_cast<long>(y) + ky - half, static_cast<long>(h));
                    sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                }
            }
            dst[y * w + x] = sum;
        }
    }
}

// 并行卷积（parallel_for_2d，每像素独立）
CaseResult run_conv_2d(std::size_t w, std::size_t h, int ksize, const std::string& case_id) {
    std::vector<float> src(w * h), dst(w * h, 0.0f), ref_dst(w * h, 0.0f);
    fill_image(src, w, h, FIXED_SEED);
    std::vector<float> kernel(ksize * ksize);
    LCG krng(FIXED_SEED ^ 0xCAFE);
    float ksum = 0.0f;
    for (auto& k : kernel) {
        k = static_cast<float>(krng.next_double() * 2.0 - 1.0);
        ksum += k;
    }
    // 归一化（避免数值爆炸）
    if (std::fabs(ksum) > 1e-6f) for (auto& k : kernel) k /= ksum;

    ref_convolve(src, ref_dst, w, h, kernel, ksize);
    int half = ksize / 2;

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Convolution2D, Extent2D{w, h},
            [&](std::size_t x, std::size_t y) {
                float sum = 0.0f;
                for (int ky = 0; ky < ksize; ++ky) {
                    for (int kx = 0; kx < ksize; ++kx) {
                        long sx = clamp_idx(static_cast<long>(x) + kx - half, static_cast<long>(w));
                        long sy = clamp_idx(static_cast<long>(y) + ky - half, static_cast<long>(h));
                        sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                    }
                }
                dst[y * w + x] = sum;
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), w * h);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E05", case_id, "fp32", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "convolution mismatch");
}

// Tile 化卷积（parallel_tiles，每 tile 处理一个块，halo 由内部读取）
CaseResult run_conv_tiled(std::size_t w, std::size_t h, int ksize,
                          std::size_t tile_w, std::size_t tile_h,
                          const std::string& case_id) {
    std::vector<float> src(w * h), dst(w * h, 0.0f), ref_dst(w * h, 0.0f);
    fill_image(src, w, h, FIXED_SEED);
    std::vector<float> kernel(ksize * ksize);
    LCG krng(FIXED_SEED ^ 0xCAFE);
    float ksum = 0.0f;
    for (auto& k : kernel) {
        k = static_cast<float>(krng.next_double() * 2.0 - 1.0);
        ksum += k;
    }
    if (std::fabs(ksum) > 1e-6f) for (auto& k : kernel) k /= ksum;

    ref_convolve(src, ref_dst, w, h, kernel, ksize);
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
                                long sx = clamp_idx(static_cast<long>(x) + kx - half, static_cast<long>(w));
                                long sy = clamp_idx(static_cast<long>(y) + ky - half, static_cast<long>(h));
                                sum += src[sy * w + sx] * kernel[ky * ksize + kx];
                            }
                        }
                        dst[y * w + x] = sum;
                    }
                }
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), w * h);
    bool ok = err.max_abs <= 1e-5 + 5e-5 * 2.0;
    return make_result("E05", case_id, "fp32", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "tiled convolution mismatch");
}

} // anonymous namespace

TEST(E05Conv, Conv3x3_64)      { auto r = run_conv_2d(64, 64, 3, "conv3x3_64");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv3x3_256)     { auto r = run_conv_2d(256, 256, 3, "conv3x3_256");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv5x5_64)      { auto r = run_conv_2d(64, 64, 5, "conv5x5_64");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Conv3x3_Edge13)  { auto r = run_conv_2d(13, 13, 3, "conv3x3_edge13"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_64_8)   { auto r = run_conv_tiled(64, 64, 3, 8, 8, "tiled3x3_64_8"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_Edge10) { auto r = run_conv_tiled(10, 10, 3, 4, 4, "tiled3x3_edge10"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled5x5_128_16) { auto r = run_conv_tiled(128, 128, 5, 16, 16, "tiled5x5_128_16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E05Conv, Tiled3x3_Rect)   { auto r = run_conv_tiled(32, 64, 3, 8, 8, "tiled3x3_rect"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e05() {
    return {
        run_conv_2d(64, 64, 3, "conv3x3_64"),
        run_conv_2d(256, 256, 3, "conv3x3_256"),
        run_conv_2d(64, 64, 5, "conv5x5_64"),
        run_conv_2d(13, 13, 3, "conv3x3_edge13"),
        run_conv_tiled(64, 64, 3, 8, 8, "tiled3x3_64_8"),
        run_conv_tiled(10, 10, 3, 4, 4, "tiled3x3_edge10"),
        run_conv_tiled(128, 128, 5, 16, 16, "tiled5x5_128_16"),
        run_conv_tiled(32, 64, 3, 8, 8, "tiled3x3_rect"),
    };
}
