// bench_write.cpp - 等规模 HISS 写入测试 (4096^2 @ NSIDE=65536)
#include "drizzle_engine.h"
#include "hiss_format.h"
#include <cstdio>
#include <chrono>
#include <vector>
#include <cstring>
#include <string>

using namespace drizzle;

static double now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    const int size = (argc > 1) ? atoi(argv[1]) : 4096;
    const int nside = 65536;
    FitsImage img;
    img.width = size; img.height = size; img.channels = 1;
    img.wcs.has_wcs = true;
    std::strncpy(img.wcs.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(img.wcs.ctype2, "DEC--TAN-SIP", 15);
    img.wcs.crval[0] = 272.886595; img.wcs.crval[1] = -23.254083;
    img.wcs.crpix[0] = size / 2.0 + 0.5; img.wcs.crpix[1] = size / 2.0 + 0.5;
    double s = 6.3 / 3600.0;
    img.wcs.cd[0] = -s; img.wcs.cd[1] = 0.0; img.wcs.cd[2] = 0.0; img.wcs.cd[3] = s;
    img.pixels.resize((size_t)size * size);
    img.pixels_f64.resize((size_t)size * size);
    for (size_t i = 0; i < img.pixels.size(); i++) {
        float v = 1000.0f + 0.01f * (float)(i % size) + 0.005f * (float)(i / size);
        img.pixels[i] = v;
        img.pixels_f64[i] = v;
    }
    DrizzleConfig cfg;
    cfg.nside = nside; cfg.nested = true; cfg.pixfrac = 0.8;
    cfg.precision_mode = 0; cfg.threads = 16;
    cfg.photometry_applied_upstream = true;
    DrizzleEngine engine;
    std::vector<TileAccumulatorT<float>> tiles;
    DrizzleStats st; std::string err;
    double t0 = now_s();
    if (!engine.drizzleTiled(img, cfg, nullptr, nullptr, tiles, st, err)) {
        printf("drizzle 失败: %s\n", err.c_str()); return 1;
    }
    double t1 = now_s();
    printf("core drizzle: %.2fs (leaf=%lld tile=%zu)\n",
           t1 - t0, (long long)st.nHealpixPixels, tiles.size());
    DrizzleMeta meta;
    meta.filter = "R"; meta.exposure_s = 180.0;
    const char* out = "run/temp/bench_write.hiss";
    std::remove(out);
    // 手动 writer 循环 + 分段计时 (替代 writeHisTilesT)
    uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
    uint32_t tile_nside = hiss::compute_tile_nside((uint32_t)nside);
    uint32_t n_leaf_per_tile = 1u << (2 * depth);
    int shift = 2 * (int)depth;
    double A_p = 4.0 * 3.141592653589793 / (12.0 * nside * nside);
    hiss::HissGridSpec grid;
    grid.nside = nside; grid.tile_nside = tile_nside;
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 0.8;
    hiss::HissMetadata hmeta;
    hmeta.nside = nside; hmeta.tile_nside = tile_nside;
    hmeta.ordering = 1; hmeta.radesys = 0; hmeta.pixfrac = 0.8;
    hmeta.photappl = 1;
    snprintf(hmeta.filter, sizeof(hmeta.filter), "R");
    hmeta.exptime = 180.0;
    std::remove(out);
    hiss::HissWriter writer;
    double t2 = now_s();
    int wr = writer.open(out, grid, hmeta);
    double t_open = now_s();
    printf("open: %.3fs rc=%d\n", t_open - t2, wr);
    double t_acc = 0, t_add = 0;
    for (const auto& tile : tiles) {
        if (tile.touched.empty()) continue;
        double ta0 = now_s();
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = tile.parent_ipix;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf_per_tile);
        for (uint32_t local : tile.touched)
            if (local < tile.pixels.size()) {
                acc.pixels[local].sum_flux = (double)tile.pixels[local].sumFlux;
                acc.pixels[local].sum_area = (double)tile.pixels[local].sumArea;
                acc.pixels[local].n_contrib = tile.pixels[local].nContrib;
            }
        double ta1 = now_s();
        int tr = writer.add_tile(tile.parent_ipix, acc, nullptr,
                                 hiss::OccupancyMode::FULL);
        double ta2 = now_s();
        t_acc += ta1 - ta0; t_add += ta2 - ta1;
        if (tr != 0) { printf("add_tile fail rc=%d\n", tr); return 1; }
    }
    double t3 = now_s();
    int fr = writer.finalize();
    double t4 = now_s();
    printf("acc 构造: %.2fs; add_tile: %.2fs (%.2fms/tile); finalize: %.2fs rc=%d\n",
           t_acc, t_add, t_add / tiles.size() * 1000.0, t4 - t3, fr);
    printf("write 总计: %.2fs\n", t4 - t2);
    return 0;
}
