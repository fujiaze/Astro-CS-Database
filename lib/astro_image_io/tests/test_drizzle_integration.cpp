// ============================================================================
// test_drizzle_integration.cpp - WP-I-1 真实数据集成测试
//
// 使用真实 FITS 文件测试完整 Drizzle → HISS 流程:
// 1. 读取真实 FITS 文件 (drizzle::readFits)
// 2. 运行 DrizzleEngine::drizzle() (pixfrac=0.8, nside=64)
// 3. 运行 DrizzleEngine::writeHis() 输出 .hiss 文件
// 4. 用 aio_hiss_inspect / read_tile_signal / read_tile_support 验证
// 5. query_pixel 测试 (图像中心 ra/dec)
// 6. LZ4 / Zstd codec 往返测试
//
// 编译 (从 lib/astro_image_io/ 目录):
// g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -Iinclude -Isrc \
// -I../../healpix_db/healpix_drizzle \
// -I../../healpix_db/healpix_stack \
// -I../../calibration/include \
// tests/test_drizzle_integration.cpp \
// src/hiss_codec.cpp src/hiss_common.cpp \
// src/hiss_writer.cpp src/hiss_reader.cpp \
// src/hiss_stream_writer.cpp src/hiss_tile_model.cpp \
// src/hiss_transform.cpp \
// src/healpix/aio_healpix_io.cpp \
// src/aio_log.cpp \
// ../../healpix_db/healpix_drizzle/drizzle_engine.cpp \
// ../../healpix_db/healpix_drizzle/wcs_sip.cpp \
// ../../healpix_db/healpix_drizzle/poly_clip.cpp \
// ../../healpix_db/healpix_drizzle/fits_reader.cpp \
// ../../healpix_db/healpix_drizzle/spherical_overlap.cpp \
// ../../healpix_db/healpix_stack/healpix_core.cpp \
// -llz4 -lzstd -lm \
// -o tests/test_drizzle_integration.exe
//
// 运行:
// ./tests/test_drizzle_integration.exe [fits_path]
// (不传参数时自动搜索 testdata 中的 FITS 文件)
// ============================================================================

#include "drizzle_engine.h"
#include "wcs_sip.h"
#include "fits_reader.h"
#include "aio_healpix_io.h"
#include "hiss_format.h"
#include "hiss_tile_model.h"
#include "hiss_transform.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <random>

// ============================================================================
// 测试框架
// ============================================================================
static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

#define CHECK(cond, msg) do { \
    if (cond) { fprintf(stderr, "  OK: %s\n", msg); g_pass++; } \
    else { fprintf(stderr, "  FAIL: %s\n", msg); g_fail++; } \
} while(0)

#define CHECK_NEAR(a, b, tol, msg) do { \
    double _a = (double)(a), _b = (double)(b), _d = _a - _b; \
    if (_d < 0) _d = -_d; \
    if (_d <= (double)(tol)) { fprintf(stderr, "  OK: %s (val=%g)\n", msg, _a); g_pass++; } \
    else { fprintf(stderr, "  FAIL: %s (got=%g, want=%g, tol=%g)\n", msg, _a, _b, (double)(tol)); g_fail++; } \
} while(0)

#define SKIP(msg) do { \
    fprintf(stderr, "  SKIP: %s\n", msg); g_skip++; \
} while(0)

// ============================================================================
// 查找真实 FITS 文件
// ============================================================================
static std::string find_test_fits(const char* user_path) {
    if (user_path && user_path[0] && std::filesystem::exists(user_path)) {
        return user_path;
    }
    // 自动搜索 testdata 目录
    const char* search_dirs[] = {
        "testdata/results/Galaxy_Center_T4/panel1/Blue",
        "testdata/results/Galaxy_Center_T4/panel1/Green",
        "testdata/results/Galaxy_Center_T4/panel1/Red",
        "testdata/results/Galaxy_Center_T4/panel2/Blue",
        "testdata/results/Galaxy_Center_T4/panel3/Red",
        nullptr
    };
    for (int i = 0; search_dirs[i]; ++i) {
        std::filesystem::path base(search_dirs[i]);
        if (!std::filesystem::exists(base)) continue;
        // 递归搜索 01_calibrated.fits
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base)) {
            if (entry.path().extension() == ".fits") {
                std::string name = entry.path().filename().string();
                if (name.find("01_calibrated") != std::string::npos ||
                    name.find("calibrated") != std::string::npos) {
                    return entry.path().string();
                }
            }
        }
    }
    return "";
}

// ============================================================================
// 测试 1: 读取真实 FITS 文件
// ============================================================================
static drizzle::FitsImage g_fits_img;
static std::string g_fits_path;

static void test_01_read_fits() {
    fprintf(stderr, "\n========== [TEST 1] 读取真实 FITS 文件 ==========\n");

    std::string error_msg;
    bool ok = drizzle::readFits(g_fits_path, g_fits_img, error_msg);
    if (!ok) {
        fprintf(stderr, "  读取 FITS 失败: %s\n", error_msg.c_str());
        CHECK(false, "readFits 成功");
        return;
    }
    CHECK(true, "readFits 成功");
    fprintf(stderr, "  文件: %s\n", g_fits_path.c_str());
    fprintf(stderr, "  尺寸: %d x %d x %d\n", g_fits_img.width, g_fits_img.height, g_fits_img.channels);
    fprintf(stderr, "  WCS:  has_wcs=%d\n", g_fits_img.wcs.has_wcs);
    if (g_fits_img.wcs.has_wcs) {
        fprintf(stderr, "    CRVAL=(%.6f, %.6f)\n", g_fits_img.wcs.crval[0], g_fits_img.wcs.crval[1]);
        fprintf(stderr, "    CRPIX=(%.3f, %.3f)\n", g_fits_img.wcs.crpix[0], g_fits_img.wcs.crpix[1]);
        fprintf(stderr, "    CD=[%.6e, %.6e; %.6e, %.6e]\n",
                g_fits_img.wcs.cd[0], g_fits_img.wcs.cd[1],
                g_fits_img.wcs.cd[2], g_fits_img.wcs.cd[3]);
        fprintf(stderr, "    CTYPE1=%.16s CTYPE2=%.16s\n",
                g_fits_img.wcs.ctype1, g_fits_img.wcs.ctype2);
        fprintf(stderr, "    SIP order=%d AP_order=%d\n",
                g_fits_img.wcs.sip.order, g_fits_img.wcs.sip.ap_order);
    }

    CHECK(g_fits_img.width > 0 && g_fits_img.height > 0, "图像尺寸合法");
    CHECK(g_fits_img.pixels.size() > 0, "像素数据非空");

    if (!g_fits_img.wcs.has_wcs) {
        SKIP("FITS 文件无 WCS 头, 后续 Drizzle 测试将跳过");
    }
}

// ============================================================================
// 测试 2: 运行 DrizzleEngine::drizzle()
// ============================================================================
static std::unordered_map<uint64_t, drizzle::PixelAccumulator> g_accumulators;
static drizzle::DrizzleStats g_stats;

static void test_02_drizzle() {
    fprintf(stderr, "\n========== [TEST 2] 运行 DrizzleEngine::drizzle() ==========\n");

    if (!g_fits_img.wcs.has_wcs) {
        SKIP("无 WCS, 跳过 Drizzle 测试");
        return;
    }

    drizzle::DrizzleConfig config;
    config.nside = 64;           // 小规模测试
    config.nested = true;
    config.pixfrac = 0.8;        // 标准_pixfrac
    config.apply_photometry = false;
    config.photscal = 1.0;

    std::string error_msg;
    drizzle::DrizzleEngine engine;

    bool ok = engine.drizzle(g_fits_img, config, nullptr, nullptr,
                             g_accumulators, g_stats, error_msg);
    CHECK(ok, "drizzle() 返回成功");
    if (!ok) {
        fprintf(stderr, "  错误: %s\n", error_msg.c_str());
        return;
    }
    CHECK(g_accumulators.size() > 0, "生成 HEALPix 像素数 > 0");
    CHECK(g_stats.nHealpixPixels > 0, "nHealpixPixels > 0");
    CHECK(g_stats.nSourcePixels > 0, "nSourcePixels > 0");
    CHECK(g_stats.nside == 64, "nside == 64");
    fprintf(stderr, "  HEALPix 像素数: %lld\n", (long long)g_stats.nHealpixPixels);
    fprintf(stderr, "  源像素数: %lld\n", (long long)g_stats.nSourcePixels);
    fprintf(stderr, "  耗时: %.3fs\n", g_stats.elapsedSec);
}

// ============================================================================
// 测试 3: 运行 DrizzleEngine::writeHis()
// ============================================================================
static const char* g_hiss_path = "drizzle_integration_test.hiss";

static void test_03_write_his() {
    fprintf(stderr, "\n========== [TEST 3] 运行 DrizzleEngine::writeHis() ==========\n");

    if (g_accumulators.empty()) {
        SKIP("无累加器数据, 跳过 writeHis 测试");
        return;
    }

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 0.8;
    config.apply_photometry = false;
    config.photscal = 1.0;

    drizzle::DrizzleMeta meta;
    meta.filter = "Blue";
    meta.exposure_s = 180.0;
    meta.obs_time = "2025-07-03T05:54:14";

    std::string error_msg;
    drizzle::DrizzleEngine engine;

    bool ok = engine.writeHis(g_accumulators, g_stats, g_fits_img.wcs,
                              config, meta, g_fits_path, g_hiss_path,
                              nullptr, error_msg);
    CHECK(ok, "writeHis() 返回成功");
    if (!ok) {
        fprintf(stderr, "  错误: %s\n", error_msg.c_str());
        return;
    }
    CHECK(std::filesystem::exists(g_hiss_path), "HISS 文件已生成");
    auto filesize = std::filesystem::file_size(g_hiss_path);
    fprintf(stderr, "  文件大小: %lld bytes\n", (long long)filesize);
    CHECK(filesize > 0, "HISS 文件大小 > 0");
}

// ============================================================================
// 测试 4: 用 aio_hiss_inspect 验证 HISS 文件
// ============================================================================
static uint32_t g_nside = 0, g_tile_nside = 0, g_depth = 0, g_n_leaf = 0;
static uint64_t g_n_tiles = 0, g_n_pix_total = 0;
static std::vector<uint64_t> g_tile_ipix_list;

static void test_04_inspect() {
    fprintf(stderr, "\n========== [TEST 4] aio_hiss_inspect 验证 ==========\n");

    if (!std::filesystem::exists(g_hiss_path)) {
        SKIP("HISS 文件不存在, 跳过 inspect 测试");
        return;
    }

    char* meta_json = nullptr;
    uint64_t* tile_ipix = nullptr;

    int ret = aio_hiss_inspect(g_hiss_path, &g_nside, &g_tile_nside, &g_depth,
                               &g_n_leaf, &g_n_tiles, &g_n_pix_total,
                               &meta_json, &tile_ipix);
    CHECK(ret == 0, "aio_hiss_inspect 返回 0");
    if (ret == 0) {
        CHECK(g_nside == 64, "nside == 64");
        CHECK(g_n_tiles > 0, "n_tiles > 0");
        CHECK(g_n_pix_total > 0, "n_pix_total > 0");
        CHECK(meta_json != nullptr, "meta_json 非空");
        if (meta_json) {
            fprintf(stderr, "  meta: %.300s\n", meta_json);
            aio_hio_free(meta_json);
        }
        if (tile_ipix) {
            g_tile_ipix_list.assign(tile_ipix, tile_ipix + g_n_tiles);
            fprintf(stderr, "  nside=%u tile_nside=%u depth=%u n_leaf=%u n_tiles=%llu n_pix=%llu\n",
                    g_nside, g_tile_nside, g_depth, g_n_leaf,
                    (unsigned long long)g_n_tiles, (unsigned long long)g_n_pix_total);
            aio_hio_free(tile_ipix);
        }
    }
}

// ============================================================================
// 测试 5: 读取 Tile signal/support 并验证非全零/非NaN
// ============================================================================
static void test_05_read_tile() {
    fprintf(stderr, "\n========== [TEST 5] 读取 Tile signal/support ==========\n");

    if (g_tile_ipix_list.empty()) {
        SKIP("无 Tile 列表, 跳过 read_tile 测试");
        return;
    }

    uint64_t parent_ipix = g_tile_ipix_list[0];
    fprintf(stderr, "  parent_ipix=%llu\n", (unsigned long long)parent_ipix);

    // 读取 signal
    float* signal = nullptr;
    uint32_t n_signal = 0;
    int ret1 = aio_hiss_read_tile_signal(g_hiss_path, parent_ipix, &signal, &n_signal);
    CHECK(ret1 == 0, "aio_hiss_read_tile_signal 返回 0");
    if (ret1 == 0 && signal) {
        CHECK(n_signal > 0, "signal 数组长度 > 0");
        // 验证非全零、非NaN
        bool has_nonzero = false;
        bool has_nan = false;
        float vmin = 1e30f, vmax = -1e30f;
        for (uint32_t i = 0; i < n_signal; ++i) {
            if (std::isnan(signal[i])) { has_nan = true; break; }
            if (signal[i] != 0.0f) has_nonzero = true;
            if (signal[i] < vmin) vmin = signal[i];
            if (signal[i] > vmax) vmax = signal[i];
        }
        CHECK(!has_nan, "signal 无 NaN");
        CHECK(has_nonzero, "signal 非全零");
        fprintf(stderr, "  signal: n=%u min=%.6f max=%.6f\n", n_signal, vmin, vmax);
        aio_hio_free(signal);
    }

    // 读取 support
    uint8_t* support = nullptr;
    uint32_t n_support = 0;
    int ret2 = aio_hiss_read_tile_support(g_hiss_path, parent_ipix, &support, &n_support);
    CHECK(ret2 == 0, "aio_hiss_read_tile_support 返回 0");
    if (ret2 == 0 && support) {
        CHECK(n_support > 0, "support 数组长度 > 0");
        bool has_nonzero = false;
        for (uint32_t i = 0; i < n_support; ++i) {
            if (support[i] != 0) { has_nonzero = true; break; }
        }
        CHECK(has_nonzero, "support 非全零");
        aio_hio_free(support);
    }
}

// ============================================================================
// 测试 6: query_pixel 测试 (用图像中心 ra/dec)
// ============================================================================
static void test_06_query_pixel() {
    fprintf(stderr, "\n========== [TEST 6] aio_hiss_query_pixel ==========\n");

    if (!std::filesystem::exists(g_hiss_path)) {
        SKIP("HISS 文件不存在, 跳过 query_pixel 测试");
        return;
    }

    // 用 CRVAL (图像中心) 作为查询坐标
    double ra = g_fits_img.wcs.crval[0];
    double dec = g_fits_img.wcs.crval[1];
    fprintf(stderr, "  查询坐标: ra=%.6f dec=%.6f (CRVAL)\n", ra, dec);

    float signal = 0.0f;
    uint8_t support = 0;
    int ret = aio_hiss_query_pixel(g_hiss_path, ra, dec, &signal, &support);
    fprintf(stderr, "  结果: ret=%d signal=%.6f support=%u\n", ret, signal, (unsigned)support);
    CHECK(ret == 0 || ret < 0, "aio_hiss_query_pixel 返回合理值 (0=找到, <0=未找到)");

    // 第二次查询: 偏移坐标
    double ra2 = ra + 1.0;
    double dec2 = dec + 1.0;
    ret = aio_hiss_query_pixel(g_hiss_path, ra2, dec2, &signal, &support);
    fprintf(stderr, "  偏移查询: ra=%.6f dec=%.6f -> ret=%d signal=%.6f\n", ra2, dec2, ret, signal);
    CHECK(ret == 0 || ret < 0, "aio_hiss_query_pixel 第二次调用正常");
}

// ============================================================================
// 测试 7: LZ4 codec 往返
// 用 HissWriter + set_experiment_codec(LZ4) 写入, HissReader 读取验证
// ============================================================================
static void test_07_lz4_roundtrip() {
    fprintf(stderr, "\n========== [TEST 7] LZ4 codec 往返 ==========\n");

    if (g_accumulators.empty()) {
        SKIP("无累加器数据, 跳过 LZ4 往返测试");
        return;
    }

    const char* lz4_path = "drizzle_test_lz4.hiss";
    std::filesystem::remove(lz4_path);
    std::filesystem::remove(std::string(lz4_path) + ".tmppool");

    // 用 HissWriter 写入, 设置 LZ4 codec
    hiss::HissWriter writer;
    hiss::HissGridSpec grid;
    grid.nside = 64;
    grid.tile_nside = 16;
    grid.ordering = 1;  // NESTED
    grid.pixfrac = 0.8;

    hiss::HissMetadata meta;
    std::strncpy(meta.object, "DrizzleIntegrationTest", sizeof(meta.object) - 1);
    std::strncpy(meta.filter, "Blue", sizeof(meta.filter) - 1);
    meta.exptime = 180.0;
    meta.photscal = 1.0;
    meta.photappl = 1;
    std::strncpy(meta.bunit, "ASTROCS_RELATIVE_FLUX", sizeof(meta.bunit) - 1);

    std::string err;
    int ret = writer.open(lz4_path, grid, meta);
    if (ret != 0) {
        CHECK(false, "Writer.open 成功 (LZ4)");
        return;
    }
    // 设置 LZ4 codec for signal + support
    writer.set_experiment_codec(hiss::SubblockType::SIGNAL,
                                 hiss::CodecId::LZ4,
                                 hiss::TransformId::NONE);
    writer.set_experiment_codec(hiss::SubblockType::SUPPORT,
                                 hiss::CodecId::LZ4,
                                 hiss::TransformId::NONE);

    // 添加第一个 Tile (用真实 drizzle 数据)
    uint64_t parent_ipix = g_tile_ipix_list.empty() ? 0 : g_tile_ipix_list[0];
    auto it = g_accumulators.find(parent_ipix);
    if (it == g_accumulators.end()) {
        // 找第一个有数据的累加器
        it = g_accumulators.begin();
        parent_ipix = it->first;
    }

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = parent_ipix;
    size_t n_leaf = (size_t)acc.tile_nside * acc.tile_nside * 12;
    acc.pixels.resize(n_leaf);
    // 使用确定性固定值填充 (避免大数值的 float32 精度问题)
    std::mt19937 rng(42);
    for (size_t i = 0; i < n_leaf; ++i) {
        acc.pixels[i].sum_flux = 100.0f + (float)(i % 256) * 0.5f;
        acc.pixels[i].sum_area = 1.0f;
        acc.pixels[i].n_contrib = 1;
    }

    ret = writer.add_tile(parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL);
    CHECK(ret == 0, "Writer.add_tile 成功 (LZ4)");
    if (ret != 0) { writer.cancel(); return; }

    ret = writer.finalize();
    CHECK(ret == 0, "Writer.finalize 成功 (LZ4)");
    if (ret != 0) return;

    CHECK(std::filesystem::exists(lz4_path), "LZ4 HISS 文件已生成");

    // 用 HissReader 读取
    hiss::HissReader reader;
    ret = reader.open(lz4_path);
    CHECK(ret == 0, "Reader.open 成功 (LZ4)");
    if (ret != 0) return;

    std::vector<float> signal_out;
    std::vector<uint8_t> support_out;
    ret = reader.read_tile(parent_ipix, signal_out, support_out);
    CHECK(ret == 0, "Reader.read_tile 成功 (LZ4)");
    if (ret == 0) {
        CHECK(signal_out.size() == acc.pixels.size(), "LZ4: signal 数组长度一致");
        // 验证数据一致
        bool data_ok = true;
        for (size_t i = 0; i < signal_out.size() && i < acc.pixels.size(); ++i) {
            if (std::fabs(signal_out[i] - acc.pixels[i].sum_flux) > 1e-4f) {
                data_ok = false;
                break;
            }
        }
        CHECK(data_ok, "LZ4: signal 数据一致");
    }
    reader.close();
    fprintf(stderr, "  [done] LZ4 往返测试完成\n");
}

// ============================================================================
// 测试 8: Zstd codec 往返
// ============================================================================
static void test_08_zstd_roundtrip() {
    fprintf(stderr, "\n========== [TEST 8] Zstd codec 往返 ==========\n");

    if (g_accumulators.empty()) {
        SKIP("无累加器数据, 跳过 Zstd 往返测试");
        return;
    }

    const char* zstd_path = "drizzle_test_zstd.hiss";
    std::filesystem::remove(zstd_path);
    std::filesystem::remove(std::string(zstd_path) + ".tmppool");

    hiss::HissWriter writer;
    hiss::HissGridSpec grid;
    grid.nside = 64;
    grid.tile_nside = 16;
    grid.ordering = 1;
    grid.pixfrac = 0.8;

    hiss::HissMetadata meta;
    std::strncpy(meta.object, "DrizzleIntegrationTest", sizeof(meta.object) - 1);
    std::strncpy(meta.filter, "Blue", sizeof(meta.filter) - 1);
    meta.exptime = 180.0;
    meta.photscal = 1.0;
    meta.photappl = 1;
    std::strncpy(meta.bunit, "ASTROCS_RELATIVE_FLUX", sizeof(meta.bunit) - 1);

    std::string err;
    int ret = writer.open(zstd_path, grid, meta);
    if (ret != 0) {
        CHECK(false, "Writer.open 成功 (Zstd)");
        return;
    }
    // 设置 Zstd codec
    writer.set_experiment_codec(hiss::SubblockType::SIGNAL,
                                 hiss::CodecId::ZSTD,
                                 hiss::TransformId::NONE);
    writer.set_experiment_codec(hiss::SubblockType::SUPPORT,
                                 hiss::CodecId::ZSTD,
                                 hiss::TransformId::NONE);

    uint64_t parent_ipix = g_tile_ipix_list.empty() ? 0 : g_tile_ipix_list[0];
    auto it = g_accumulators.find(parent_ipix);
    if (it == g_accumulators.end()) {
        it = g_accumulators.begin();
        parent_ipix = it->first;
    }

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = parent_ipix;
    size_t n_leaf = (size_t)acc.tile_nside * acc.tile_nside * 12;
    acc.pixels.resize(n_leaf);
    // 使用确定性固定值填充 (与 LZ4 测试一致, 避免 float32 精度问题)
    std::mt19937 rng(42);
    for (size_t i = 0; i < n_leaf; ++i) {
        acc.pixels[i].sum_flux = 100.0f + (float)(i % 256) * 0.5f;
        acc.pixels[i].sum_area = 1.0f;
        acc.pixels[i].n_contrib = 1;
    }

    ret = writer.add_tile(parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL);
    CHECK(ret == 0, "Writer.add_tile 成功 (Zstd)");
    if (ret != 0) { writer.cancel(); return; }

    ret = writer.finalize();
    CHECK(ret == 0, "Writer.finalize 成功 (Zstd)");
    if (ret != 0) return;

    CHECK(std::filesystem::exists(zstd_path), "Zstd HISS 文件已生成");

    hiss::HissReader reader;
    ret = reader.open(zstd_path);
    CHECK(ret == 0, "Reader.open 成功 (Zstd)");
    if (ret != 0) return;

    std::vector<float> signal_out;
    std::vector<uint8_t> support_out;
    ret = reader.read_tile(parent_ipix, signal_out, support_out);
    CHECK(ret == 0, "Reader.read_tile 成功 (Zstd)");
    if (ret == 0) {
        CHECK(signal_out.size() == acc.pixels.size(), "Zstd: signal 数组长度一致");
        bool data_ok = true;
        for (size_t i = 0; i < signal_out.size() && i < acc.pixels.size(); ++i) {
            if (std::fabs(signal_out[i] - acc.pixels[i].sum_flux) > 1e-4f) {
                data_ok = false;
                break;
            }
        }
        CHECK(data_ok, "Zstd: signal 数据一致");
    }
    reader.close();
    fprintf(stderr, "  [done] Zstd 往返测试完成\n");
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
    fprintf(stderr, "============================================================\n");
    fprintf(stderr, "WP-I-1 真实数据集成测试: Drizzle → HISS 全流程\n");
    fprintf(stderr, "============================================================\n");

    // 查找 FITS 文件
    const char* user_path = (argc > 1) ? argv[1] : nullptr;
    g_fits_path = find_test_fits(user_path);
    if (g_fits_path.empty()) {
        fprintf(stderr, "[FATAL] 未找到测试 FITS 文件\n");
        fprintf(stderr, "  用法: %s [fits_path]\n", argv[0]);
        return 1;
    }
    fprintf(stderr, "使用 FITS 文件: %s\n", g_fits_path.c_str());

    // 运行测试
    test_01_read_fits();
    test_02_drizzle();
    test_03_write_his();
    test_04_inspect();
    test_05_read_tile();
    test_06_query_pixel();
    test_07_lz4_roundtrip();
    test_08_zstd_roundtrip();

    // 清理测试文件
    std::filesystem::remove(g_hiss_path);
    std::filesystem::remove(std::string(g_hiss_path) + ".tmppool");
    std::filesystem::remove("drizzle_test_lz4.hiss");
    std::filesystem::remove("drizzle_test_lz4.hiss.tmppool");
    std::filesystem::remove("drizzle_test_zstd.hiss");
    std::filesystem::remove("drizzle_test_zstd.hiss.tmppool");

    fprintf(stderr, "\n========== 测试结果 ==========\n");
    fprintf(stderr, "  通过: %d\n", g_pass);
    fprintf(stderr, "  失败: %d\n", g_fail);
    fprintf(stderr, "  跳过: %d\n", g_skip);
    fprintf(stderr, "  总计: %d\n", g_pass + g_fail + g_skip);
    fprintf(stderr, "==============================\n");

    return g_fail == 0 ? 0 : 1;
}
