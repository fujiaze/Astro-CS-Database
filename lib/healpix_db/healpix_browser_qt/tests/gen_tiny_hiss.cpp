// ============================================================================
// gen_tiny_hiss.cpp - 生成 tiny 合成 FP32/FP64 HISS (Browser 双精度测试自包含)
// 编译 (从 lib/astro_image_io/ 目录, 参考 test_precision_dual):
//   g++ -std=c++17 -O2 -fopenmp -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
//     -Iinclude -Isrc \
//     <此文件> src/hiss_codec.cpp src/hiss_common.cpp \
//     src/hiss_tile_model.cpp src/hiss_transform.cpp \
//     src/hiss_writer.cpp src/hiss_stream_writer.cpp \
//     src/hiss_reader.cpp src/healpix/aio_healpix_io.cpp \
//     src/aio_api.cpp src/aio_log.cpp -lzstd -lm -o gen_tiny_hiss.exe
// ============================================================================
#include "hiss_format.h"
#include "hiss_tile_model.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace hiss;

static bool write_one(const std::string& path, bool f64) {
    HissGridSpec grid;
    grid.nside = 64;
    grid.tile_nside = compute_tile_nside(64);
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    const uint32_t n_leaf = 16;
    const double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    std::vector<double> flux = {0.1, 0.2, 100.5, 200.25, 1.0/3.0, 3.5, 7.75,
                                1234.0, 4096.0, 0.125, -0.0625, 65536.0,
                                1.0, 255.99609375, 0.0, 8192.0};

    DrizzleTileAccumulator acc;
    acc.tile_nside = grid.tile_nside;
    acc.parent_ipix = 42;
    acc.pixel_area = A_p;
    acc.pixels.resize(n_leaf);
    for (uint32_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = flux[i];
        acc.pixels[i].sum_area = A_p;
        acc.pixels[i].n_contrib = 1;
    }

    // 确定性 SNR 控制点 (f32 写 8B/点, f64 写 12B/点, 由 writer 按 snr_dtype 决定)
    hiss::HissSnrBlock snr;
    snr.estimator_id = 1;
    snr.sampling_scale = 1.0f;
    snr.points = {
        {0u, 12.5f},
        {5u, 200.25f},
        {15u, 0.125f}
    };

    HissWriter w;
    if (w.open(path.c_str(), grid, meta) != 0) return false;
    int rc = f64 ? w.add_tile_f64(42, acc, &snr, OccupancyMode::FULL)
                 : w.add_tile(42, acc, &snr, OccupancyMode::FULL);
    if (rc != 0) return false;
    if (w.finalize() != 0) return false;
    return true;
}

int main(int argc, char** argv) {
    std::string outdir = (argc > 1) ? argv[1] : ".";
    bool ok = true;
    ok &= write_one(outdir + "/tiny_fp32.hiss", false);
    ok &= write_one(outdir + "/tiny_fp64.hiss", true);
    printf("%s\n", ok ? "OK" : "FAIL");
    return ok ? 0 : 1;
}
