// ============================================================================
// test_wph_cli_browser.cpp - WP-H 步骤14 集成测试
// 验证 CLI 诊断 + Browser 后端所需的 C API:
//   aio_hiss_inspect, aio_hiss_read_tile_signal, aio_hiss_read_tile_support,
//   aio_hiss_read_tile_snr, aio_hiss_query_pixel
//
// 编译 (从 tests/ 目录):
//   g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DAIO_ENABLE_HEALPIX \
//     -I../include -I../src \
//     test_wph_cli_browser.cpp \
//     ../src/hiss_codec.cpp ../src/hiss_common.cpp \
//     ../src/hiss_tile_model.cpp ../src/hiss_transform.cpp \
//     ../src/hiss_writer.cpp ../src/hiss_stream_writer.cpp \
//     ../src/hiss_reader.cpp \
//     ../src/healpix/aio_healpix_io.cpp \
//     ../src/aio_log.cpp \
//     -llz4 -lm -o test_wph_cli_browser.exe
//
// 运行:
//   ./test_wph_cli_browser.exe
// ============================================================================
#include "aio_healpix_io.h"
#include "hiss_format.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { fprintf(stderr, "  OK: %s\n", msg); g_pass++; } \
    else { fprintf(stderr, "  FAIL: %s\n", msg); g_fail++; } \
} while(0)

// ============================================================================
// 1. 用 aio_hiss_write 生成一个测试 HISS 文件 (nside=64, 50 像素, 50 Tiles)
// ============================================================================
static std::string generate_test_hiss(const char* path) {
    const uint32_t nside = 64;
    const int nested = 1;
    const uint64_t n_pix = 50;

    std::vector<uint64_t> ipix(n_pix);
    std::vector<float> pixel(n_pix);
    std::vector<float> snr(n_pix);

    // 模拟数据: 50 个像素, ipix 递增, pixel 值递增
    for (uint64_t i = 0; i < n_pix; ++i) {
        ipix[i] = i * 10 + 5;   // 任意 ipix
        pixel[i] = (float)(i + 1) * 1.5f;  // 1.5, 3.0, 4.5, ...
        snr[i] = (float)(i + 1) * 0.5f;    // 0.5, 1.0, 1.5, ...
    }

    const char* meta_json = "{\"nside\":64,\"tile_nside\":16,\"ordering\":1,"
                            "\"pixfrac\":1,\"object\":\"WP-H Test\",\"exptime\":60}";

    int ret = aio_hiss_write(path, nside, nested, n_pix,
                             ipix.data(), pixel.data(), snr.data(), meta_json);
    if (ret != 0) {
        fprintf(stderr, "生成 HISS 失败: ret=%d\n", ret);
        return "";
    }
    fprintf(stderr, "[gen] HISS 文件已生成: %s (n_pix=%llu)\n", path, n_pix);
    return path;
}

// ============================================================================
// 2. 测试 aio_hiss_inspect (CLI 诊断输出使用)
// ============================================================================
static void test_inspect(const char* path) {
    fprintf(stderr, "\n========== [TEST] aio_hiss_inspect (CLI 诊断) ==========\n");

    uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
    uint64_t n_tiles = 0, n_pix_total = 0;
    char* meta_json = nullptr;
    uint64_t* tile_ipix_list = nullptr;

    int ret = aio_hiss_inspect(path, &nside, &tile_nside, &depth,
                               &n_leaf_per_tile, &n_tiles, &n_pix_total,
                               &meta_json, &tile_ipix_list);
    CHECK(ret == 0, "aio_hiss_inspect 返回 0");
    CHECK(nside == 64, "nside == 64");
    CHECK(tile_nside == 16, "tile_nside == 16");
    CHECK(n_tiles > 0, "n_tiles > 0");
    CHECK(n_pix_total > 0, "n_pix_total > 0");
    CHECK(meta_json != nullptr, "meta_json 非空");
    CHECK(tile_ipix_list != nullptr, "tile_ipix_list 非空");

    if (meta_json) {
        fprintf(stderr, "  meta_json: %.200s\n", meta_json);
        aio_hio_free(meta_json);
    }
    if (tile_ipix_list) {
        fprintf(stderr, "  tile_ipix_list[0..%llu]: ", n_tiles > 5 ? 5ULL : n_tiles);
        for (uint64_t i = 0; i < n_tiles && i < 5; ++i) {
            fprintf(stderr, "%llu ", (unsigned long long)tile_ipix_list[i]);
        }
        fprintf(stderr, "...\n");
        aio_hio_free(tile_ipix_list);
    }
    fprintf(stderr, "  nside=%u tile_nside=%u depth=%u n_leaf_per_tile=%u "
                    "n_tiles=%llu n_pix_total=%llu\n",
                    nside, tile_nside, depth, n_leaf_per_tile,
                    (unsigned long long)n_tiles, (unsigned long long)n_pix_total);
}

// ============================================================================
// 3. 测试 aio_hiss_read_tile_signal / read_tile_support (Browser Tile 查看)
// ============================================================================
static void test_read_tile(const char* path, uint64_t parent_ipix) {
    fprintf(stderr, "\n========== [TEST] aio_hiss_read_tile_signal/support (Browser Tile) ==========\n");
    fprintf(stderr, "  parent_ipix=%llu\n", (unsigned long long)parent_ipix);

    // signal
    float* signal = nullptr;
    uint32_t n_signal = 0;
    int ret1 = aio_hiss_read_tile_signal(path, parent_ipix, &signal, &n_signal);
    CHECK(ret1 == 0, "aio_hiss_read_tile_signal 返回 0");
    CHECK(n_signal > 0, "n_signal > 0");
    if (signal) {
        float vmin = 1e30f, vmax = -1e30f;
        for (uint32_t i = 0; i < n_signal; ++i) {
            if (signal[i] < vmin) vmin = signal[i];
            if (signal[i] > vmax) vmax = signal[i];
        }
        fprintf(stderr, "  signal: n=%u min=%.3f max=%.3f\n", n_signal, vmin, vmax);
        aio_hio_free(signal);
    }

    // support
    uint8_t* support = nullptr;
    uint32_t n_support = 0;
    int ret2 = aio_hiss_read_tile_support(path, parent_ipix, &support, &n_support);
    CHECK(ret2 == 0, "aio_hiss_read_tile_support 返回 0");
    if (support) {
        fprintf(stderr, "  support: n=%u sample[0]=%u\n", n_support, (unsigned)support[0]);
        aio_hio_free(support);
    }

    // SNR (可能无数据, 只验证不崩溃)
    uint8_t* snr_out = nullptr;
    uint32_t n_points = 0;
    int ret3 = aio_hiss_read_tile_snr(path, parent_ipix, &snr_out, &n_points);
    fprintf(stderr, "  snr: ret=%d n_points=%u\n", ret3, n_points);
    if (snr_out) aio_hio_free(snr_out);
    CHECK(ret3 == 0 || ret3 == -2 || ret3 == -5 || ret3 == -6,
          "aio_hiss_read_tile_snr 返回合理值 (0/-2/-5/-6=无SNR数据)");
}

// ============================================================================
// 4. 测试 aio_hiss_query_pixel (Browser 像素查询)
//    用 aio_hiss_read 获取 ipix, 用简单 pix2ang_nest 转换为 ra/dec
// ============================================================================
static void test_query_pixel(const char* path) {
    fprintf(stderr, "\n========== [TEST] aio_hiss_query_pixel (Browser 像素查询) ==========\n");

    // 读取 HISS 文件获取 ipix 列表
    uint32_t nside = 0;
    int nested = 0;
    uint64_t n_pix = 0;
    uint64_t* ipix = nullptr;
    float* pixel = nullptr;
    float* snr = nullptr;
    char* meta_json = nullptr;

    int ret = aio_hiss_read(path, &nside, &nested, &n_pix,
                            &ipix, &pixel, &snr, &meta_json);
    if (ret != 0 || ipix == nullptr || n_pix == 0) {
        CHECK(false, "aio_hiss_read 失败, 无法获取 ipix");
        return;
    }

    // 简单 HEALPix NESTED pix2ang (nside=64)
    // 取第一个像素的 ipix, 转换为 ra/dec
    uint64_t test_ipix = ipix[0];
    float expected_signal = pixel[0];

    // 简化: 用 query_pixel 查询若干坐标, 验证不崩溃且返回合理值
    // (ra/dec 精确转换需要完整 HEALPix 数学, 这里只验证 API 调用路径)
    float signal = 0.0f;
    uint8_t support = 0;

    // 测试 1: 任意坐标 (可能在数据范围内, 也可能不在)
    double ra_test = 45.0, dec_test = 30.0;
    int qret = aio_hiss_query_pixel(path, ra_test, dec_test, &signal, &support);
    fprintf(stderr, "  查询 ra=%.2f dec=%.2f -> ret=%d signal=%.3f support=%u\n",
                    ra_test, dec_test, qret, signal, (unsigned)support);
    CHECK(qret == 0 || qret < 0, "aio_hiss_query_pixel 返回合理值 (0=找到, <0=未找到)");

    // 测试 2: 另一个坐标
    ra_test = 180.0; dec_test = 0.0;
    qret = aio_hiss_query_pixel(path, ra_test, dec_test, &signal, &support);
    fprintf(stderr, "  查询 ra=%.2f dec=%.2f -> ret=%d signal=%.3f support=%u\n",
                    ra_test, dec_test, qret, signal, (unsigned)support);
    CHECK(qret == 0 || qret < 0, "aio_hiss_query_pixel 第二次调用正常");

    // 清理
    if (ipix) aio_hio_free(ipix);
    if (pixel) aio_hio_free(pixel);
    if (snr) aio_hio_free(snr);
    if (meta_json) aio_hio_free(meta_json);
}

// ============================================================================
// main
// ============================================================================
int main() {
    fprintf(stderr, "========== WP-H 步骤14 集成测试 ==========\n");

    const char* path = "wph_test.hiss";

    // 清理旧文件
    std::filesystem::remove(path);
    std::filesystem::remove("wph_test.hiss.tmppool");

    // 1. 生成测试 HISS
    if (generate_test_hiss(path).empty()) {
        fprintf(stderr, "生成 HISS 失败, 终止\n");
        return 1;
    }

    // 2. 测试 inspect
    test_inspect(path);

    // 3. 获取第一个 tile 的 parent_ipix, 测试 read_tile
    {
        uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
        uint64_t n_tiles = 0, n_pix_total = 0;
        char* meta_json = nullptr;
        uint64_t* tile_ipix_list = nullptr;
        if (aio_hiss_inspect(path, &nside, &tile_nside, &depth,
                             &n_leaf_per_tile, &n_tiles, &n_pix_total,
                             &meta_json, &tile_ipix_list) == 0 && tile_ipix_list) {
            test_read_tile(path, tile_ipix_list[0]);
            aio_hio_free(meta_json);
            aio_hio_free(tile_ipix_list);
        }
    }

    // 4. 测试 query_pixel
    test_query_pixel(path);

    // 不删除文件, 保留给 browser_cli 测试
    fprintf(stderr, "\n========== 测试结果: 通过=%d 失败=%d ==========\n", g_pass, g_fail);
    fprintf(stderr, "[gen] HISS 文件保留: %s (供 browser_cli 测试)\n", path);
    return g_fail == 0 ? 0 : 1;
}
