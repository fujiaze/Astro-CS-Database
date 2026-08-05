// ============================================================================
// test_browser_dual_dtype.cpp - Browser 后端 FP32/FP64 双精度无头测试 (R11)
// 验证: open_file 检测精度; read_tile_signal(F32) / read_tile_signal_f64(F64);
//       query_pixel(F32) / query_pixel_f64(F64) 各自只走对应 dtype (禁止静默转换)。
// 数据: 已有 fp32/fp64 frame.hiss (r11_gates 产物, 不运行 Drizzle)。
// ============================================================================
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include "browser_backend.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("[PASS] %s\n", msg); ++g_pass; } \
    else { printf("[FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static const char* HISS_FP32 =
    "run/temp/r11_gates/output/fp32/hiss_verify/frame.hiss";
static const char* HISS_FP64 =
    "run/temp/r11_gates/output/fp64/hiss_verify/frame.hiss";

int main() {
    // ---- FP32 文件: 必须走 FP32 路径, F64 读取应失败 (禁止静默转换) ----
    {
        BrowserBackend b;
        CHECK(b.open_file(HISS_FP32) == 0, "FP32 文件打开成功");
        CHECK(!b.is_fp64(), "FP32 文件 is_fp64()==false");
        HissHeader hdr;
        CHECK(b.load_hiss(HISS_FP32, hdr) == 0 && !hdr.tile_ipix_list.empty(),
              "FP32 文件 header/tile 目录可读");
        uint64_t p0 = hdr.tile_ipix_list.empty() ? 0 : hdr.tile_ipix_list[0];
        HissTileData tile;
        CHECK(b.read_tile_signal(p0, tile) == 0 &&
              tile.signal != nullptr && tile.n_signal > 0,
              "FP32 文件 read_tile_signal (float) 成功");
        if (tile.signal) { b.release_tile(tile); }
        HissTileData t2;
        int r64 = b.read_tile_signal_f64(p0, t2);
        CHECK(r64 != 0, "FP32 文件 read_tile_signal_f64 被拒绝 (禁止静默转换)");
        if (t2.signal_f64) { b.release_tile(t2); }
        float s32 = 0.0f; uint8_t sup = 0;
        CHECK(b.query_pixel(272.9, -23.25, s32, sup) == 0, "FP32 文件 query_pixel (float) 成功");
        double s64 = 0.0;
        CHECK(b.query_pixel_f64(272.9, -23.25, s64, sup) != 0,
              "FP32 文件 query_pixel_f64 被拒绝");
    }
    // ---- FP64 文件: 必须走 FP64 路径, F32 读取应失败 ----
    {
        BrowserBackend b;
        CHECK(b.open_file(HISS_FP64) == 0, "FP64 文件打开成功");
        CHECK(b.is_fp64(), "FP64 文件 is_fp64()==true");
        HissHeader hdr;
        CHECK(b.load_hiss(HISS_FP64, hdr) == 0 && !hdr.tile_ipix_list.empty(),
              "FP64 文件 header/tile 目录可读");
        uint64_t p0 = hdr.tile_ipix_list.empty() ? 0 : hdr.tile_ipix_list[0];
        HissTileData tile;
        CHECK(b.read_tile_signal_f64(p0, tile) == 0 &&
              tile.signal_f64 != nullptr && tile.n_signal > 0,
              "FP64 文件 read_tile_signal_f64 (double) 成功");
        if (tile.signal_f64) { b.release_tile(tile); }
        HissTileData t2;
        int r32 = b.read_tile_signal(p0, t2);
        CHECK(r32 != 0, "FP64 文件 read_tile_signal (float) 被拒绝");
        if (t2.signal) { b.release_tile(t2); }
        double s64 = 0.0; uint8_t sup = 0;
        CHECK(b.query_pixel_f64(272.9, -23.25, s64, sup) == 0,
              "FP64 文件 query_pixel_f64 (double) 成功");
        float s32 = 0.0f;
        CHECK(b.query_pixel(272.9, -23.25, s32, sup) != 0,
              "FP64 文件 query_pixel (float) 被拒绝");
    }
    printf("== 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
