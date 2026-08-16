// lib/acr/tests/classic/e04_transpose.cpp — E04 Tiled Matrix Transpose
// 验证能力：2D range、Tile、边缘、stride
// Cases: 511×509、2048²、8191×4093；uint32/FP32；Tile 16/32/64；CPU/GPU；逐元素 exact
// 用 parallel_tiles 实现 tile 化转置，验证边缘 tile 正确 clamp。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== FP32 填充 =====
void fill_matrix_fp32(std::vector<float>& m, std::size_t rows, std::size_t cols, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < rows * cols; ++i) {
        m[i] = static_cast<float>(rng.next_double() * 2.0 - 1.0);
    }
}

// ===== uint32 填充（确定性，避免依赖浮点）=====
void fill_matrix_u32(std::vector<std::uint32_t>& m, std::size_t rows, std::size_t cols, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < rows * cols; ++i) {
        m[i] = static_cast<std::uint32_t>(rng.next() & 0xFFFFFFFFu);
    }
}

// ===== 串行转置 reference（FP32）=====
void ref_transpose_fp32(const std::vector<float>& src, std::vector<float>& dst,
                        std::size_t rows, std::size_t cols) {
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < cols; ++x) {
            dst[x * rows + y] = src[y * cols + x];
        }
    }
}

// ===== 串行转置 reference（uint32）=====
void ref_transpose_u32(const std::vector<std::uint32_t>& src, std::vector<std::uint32_t>& dst,
                       std::size_t rows, std::size_t cols) {
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < cols; ++x) {
            dst[x * rows + y] = src[y * cols + x];
        }
    }
}

// ===== Tile 化转置（FP32）=====
CaseResult run_tiled_transpose_fp32(std::size_t rows, std::size_t cols,
                                     std::size_t tile_w, std::size_t tile_h,
                                     const std::string& case_id) {
    std::vector<float> src(rows * cols), dst(rows * cols, 0.0f), ref_dst(cols * rows, 0.0f);
    fill_matrix_fp32(src, rows, cols, FIXED_SEED);
    ref_transpose_fp32(src, ref_dst, rows, cols);

    std::size_t dst_rows = cols, dst_cols = rows;
    Extent2D extent{dst_cols, dst_rows};
    TileShape tile{tile_w, tile_h};

    auto tm = measure_timing([&] {
        parallel_tiles(KernelId::Transpose, extent, tile,
            [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
                for (std::size_t j = 0; j < th; ++j) {
                    for (std::size_t i = 0; i < tw; ++i) {
                        std::size_t dx = tx * tile_w + i;
                        std::size_t dy = ty * tile_h + j;
                        if (dx < dst_cols && dy < dst_rows) {
                            dst[dy * dst_cols + dx] = src[dx * cols + dy];
                        }
                    }
                }
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), rows * cols);
    bool ok = (err.max_abs <= 1e-6);  // 转置应 bit-exact
    return make_result("E04", case_id, "fp32", rows * cols, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "transpose mismatch");
}

// ===== Tile 化转置（uint32）=====
CaseResult run_tiled_transpose_u32(std::size_t rows, std::size_t cols,
                                    std::size_t tile_w, std::size_t tile_h,
                                    const std::string& case_id) {
    std::vector<std::uint32_t> src(rows * cols), dst(rows * cols, 0u), ref_dst(cols * rows, 0u);
    fill_matrix_u32(src, rows, cols, FIXED_SEED);
    ref_transpose_u32(src, ref_dst, rows, cols);

    std::size_t dst_rows = cols, dst_cols = rows;
    Extent2D extent{dst_cols, dst_rows};
    TileShape tile{tile_w, tile_h};

    auto tm = measure_timing([&] {
        parallel_tiles(KernelId::Transpose, extent, tile,
            [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
                for (std::size_t j = 0; j < th; ++j) {
                    for (std::size_t i = 0; i < tw; ++i) {
                        std::size_t dx = tx * tile_w + i;
                        std::size_t dy = ty * tile_h + j;
                        if (dx < dst_cols && dy < dst_rows) {
                            dst[dy * dst_cols + dx] = src[dx * cols + dy];
                        }
                    }
                }
            });
    });

    // 整数 exact
    ErrorStats err;
    bool ok = true;
    for (std::size_t i = 0; i < rows * cols; ++i) {
        if (dst[i] != ref_dst[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(dst[i]) - static_cast<double>(ref_dst[i]));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E04", case_id, "uint32", rows * cols, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "uint32 transpose mismatch");
}

// ===== parallel_for_2d 转置（每像素一个 kernel）=====
CaseResult run_2d_transpose(std::size_t rows, std::size_t cols, const std::string& case_id) {
    std::vector<float> src(rows * cols), dst(cols * rows, 0.0f), ref_dst(cols * rows, 0.0f);
    fill_matrix_fp32(src, rows, cols, FIXED_SEED);
    ref_transpose_fp32(src, ref_dst, rows, cols);

    std::size_t dst_rows = cols, dst_cols = rows;
    Extent2D extent{dst_cols, dst_rows};

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Transpose, extent,
            [&](std::size_t x, std::size_t y) {
                dst[y * dst_cols + x] = src[x * cols + y];
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), rows * cols);
    bool ok = (err.max_abs <= 1e-6);
    return make_result("E04", case_id, "fp32", rows * cols, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "2d transpose mismatch");
}

} // anonymous namespace

// ===== 保留向后兼容的小尺寸 TEST =====
TEST(E04Transpose, Square64Tile4)   { auto r = run_tiled_transpose_fp32(64, 64, 4, 4, "square64_tile4");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square256Tile16) { auto r = run_tiled_transpose_fp32(256, 256, 16, 16, "square256_tile16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8x32Tile4)   { auto r = run_tiled_transpose_fp32(8, 32, 4, 4, "rect_8x32_tile4");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Edge10x10Tile4)  { auto r = run_tiled_transpose_fp32(10, 10, 4, 4, "edge_10x10_tile4"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Edge13x7Tile4)   { auto r = run_tiled_transpose_fp32(13, 7, 4, 4, "edge_13x7_tile4");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dSquare64)   { auto r = run_2d_transpose(64, 64, "for2d_square64");          ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dEdge10x10)  { auto r = run_2d_transpose(10, 10, "for2d_edge10x10");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dLarge512)   { auto r = run_2d_transpose(512, 512, "for2d_large512");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §8 规范尺寸 + Tile 16/32/64 + uint32/FP32 =====
// 511×509（非方阵，边缘 tile 不完整）
TEST(E04Transpose, Rect511x509_Tile16_FP32) { auto r = run_tiled_transpose_fp32(511, 509, 16, 16, "rect_511x509_t16_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect511x509_Tile32_FP32) { auto r = run_tiled_transpose_fp32(511, 509, 32, 32, "rect_511x509_t32_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect511x509_Tile64_FP32) { auto r = run_tiled_transpose_fp32(511, 509, 64, 64, "rect_511x509_t64_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect511x509_Tile16_U32)  { auto r = run_tiled_transpose_u32(511, 509, 16, 16, "rect_511x509_t16_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect511x509_Tile32_U32)  { auto r = run_tiled_transpose_u32(511, 509, 32, 32, "rect_511x509_t32_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect511x509_Tile64_U32)  { auto r = run_tiled_transpose_u32(511, 509, 64, 64, "rect_511x509_t64_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 2048²（方阵，整齐 tile）
TEST(E04Transpose, Square2048_Tile16_FP32) { auto r = run_tiled_transpose_fp32(2048, 2048, 16, 16, "square_2048_t16_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square2048_Tile32_FP32) { auto r = run_tiled_transpose_fp32(2048, 2048, 32, 32, "square_2048_t32_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square2048_Tile64_FP32) { auto r = run_tiled_transpose_fp32(2048, 2048, 64, 64, "square_2048_t64_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square2048_Tile16_U32)  { auto r = run_tiled_transpose_u32(2048, 2048, 16, 16, "square_2048_t16_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square2048_Tile32_U32)  { auto r = run_tiled_transpose_u32(2048, 2048, 32, 32, "square_2048_t32_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square2048_Tile64_U32)  { auto r = run_tiled_transpose_u32(2048, 2048, 64, 64, "square_2048_t64_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 8191×4093（大非方阵，边缘 tile 不完整）
TEST(E04Transpose, Rect8191x4093_Tile16_FP32) { auto r = run_tiled_transpose_fp32(8191, 4093, 16, 16, "rect_8191x4093_t16_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8191x4093_Tile32_FP32) { auto r = run_tiled_transpose_fp32(8191, 4093, 32, 32, "rect_8191x4093_t32_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8191x4093_Tile64_FP32) { auto r = run_tiled_transpose_fp32(8191, 4093, 64, 64, "rect_8191x4093_t64_fp32"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8191x4093_Tile16_U32)  { auto r = run_tiled_transpose_u32(8191, 4093, 16, 16, "rect_8191x4093_t16_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8191x4093_Tile32_U32)  { auto r = run_tiled_transpose_u32(8191, 4093, 32, 32, "rect_8191x4093_t32_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8191x4093_Tile64_U32)  { auto r = run_tiled_transpose_u32(8191, 4093, 64, 64, "rect_8191x4093_t64_u32");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e04() {
    return {
        // 向后兼容
        run_tiled_transpose_fp32(64, 64, 4, 4, "square64_tile4"),
        run_tiled_transpose_fp32(256, 256, 16, 16, "square256_tile16"),
        run_tiled_transpose_fp32(8, 32, 4, 4, "rect_8x32_tile4"),
        run_tiled_transpose_fp32(10, 10, 4, 4, "edge_10x10_tile4"),
        run_tiled_transpose_fp32(13, 7, 4, 4, "edge_13x7_tile4"),
        run_2d_transpose(64, 64, "for2d_square64"),
        run_2d_transpose(10, 10, "for2d_edge10x10"),
        run_2d_transpose(512, 512, "for2d_large512"),
        // Phase H 扩展：511×509
        run_tiled_transpose_fp32(511, 509, 16, 16, "rect_511x509_t16_fp32"),
        run_tiled_transpose_fp32(511, 509, 32, 32, "rect_511x509_t32_fp32"),
        run_tiled_transpose_fp32(511, 509, 64, 64, "rect_511x509_t64_fp32"),
        run_tiled_transpose_u32(511, 509, 16, 16, "rect_511x509_t16_u32"),
        run_tiled_transpose_u32(511, 509, 32, 32, "rect_511x509_t32_u32"),
        run_tiled_transpose_u32(511, 509, 64, 64, "rect_511x509_t64_u32"),
        // 2048²
        run_tiled_transpose_fp32(2048, 2048, 16, 16, "square_2048_t16_fp32"),
        run_tiled_transpose_fp32(2048, 2048, 32, 32, "square_2048_t32_fp32"),
        run_tiled_transpose_fp32(2048, 2048, 64, 64, "square_2048_t64_fp32"),
        run_tiled_transpose_u32(2048, 2048, 16, 16, "square_2048_t16_u32"),
        run_tiled_transpose_u32(2048, 2048, 32, 32, "square_2048_t32_u32"),
        run_tiled_transpose_u32(2048, 2048, 64, 64, "square_2048_t64_u32"),
        // 8191×4093
        run_tiled_transpose_fp32(8191, 4093, 16, 16, "rect_8191x4093_t16_fp32"),
        run_tiled_transpose_fp32(8191, 4093, 32, 32, "rect_8191x4093_t32_fp32"),
        run_tiled_transpose_fp32(8191, 4093, 64, 64, "rect_8191x4093_t64_fp32"),
        run_tiled_transpose_u32(8191, 4093, 16, 16, "rect_8191x4093_t16_u32"),
        run_tiled_transpose_u32(8191, 4093, 32, 32, "rect_8191x4093_t32_u32"),
        run_tiled_transpose_u32(8191, 4093, 64, 64, "rect_8191x4093_t64_u32"),
    };
}
