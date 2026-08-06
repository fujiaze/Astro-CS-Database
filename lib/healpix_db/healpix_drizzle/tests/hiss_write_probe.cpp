// hiss_write_probe.cpp - HISS Writer 剖析 (复现完整帧 285 Tile 规模)
#include "hiss_format.h"
#include "aio_util.h"
#include <cstdio>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cmath>

using namespace hiss;

static double now_ms() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    int n_tiles = (argc > 1) ? atoi(argv[1]) : 285;
    uint32_t nside = 65536;
    uint32_t depth = compute_tile_depth(nside);
    uint32_t tile_nside = compute_tile_nside(nside);
    uint32_t n_leaf = 1u << (2 * depth);
    double A_p = 2.438197e-10;
    printf("probe: %d tiles x %u leaf (nside=%u tile_nside=%u)\n",
           n_tiles, n_leaf, nside, tile_nside);

    HissGridSpec grid;
    grid.nside = nside; grid.tile_nside = tile_nside;
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 0.8;
    HissMetadata meta;
    meta.nside = nside; meta.tile_nside = tile_nside;
    meta.ordering = 1; meta.radesys = 0; meta.pixfrac = 0.8;
    meta.photappl = 1;   // 模拟测光已应用 (BUNIT=ASTROCS_RELATIVE_FLUX)
    snprintf(meta.filter, sizeof(meta.filter), "R");
    meta.exptime = 180.0;

    const char* path = "run/temp/probe.hiss";
    std::remove(path);
    std::string final_path(path);
    std::string partial = final_path + ".partial";
    std::string tmppool = final_path + ".tmppool";
    std::remove(partial.c_str()); std::remove(tmppool.c_str());

    HissWriter writer;
    double t_open0 = now_ms();
    int wret = writer.open(path, grid, meta);
    double t_open1 = now_ms();
    printf("open: %.1f ms\n", t_open1 - t_open0);
    if (wret != 0) { printf("open failed rc=%d\n", wret); return 1; }

    double t_add_total = 0, t_compress = 0, t_finalize_acc = 0;
    for (int t = 0; t < n_tiles; t++) {
        DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = (uint64_t)t;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        // 稀疏模拟: 每 Tile 覆盖 ~215k leaf (与完整帧一致), 其余 0
        // 82% 占用模拟 + 随机值分布 (真实 drizzle 数据特征)
        std::srand(42 + t);
        for (uint32_t i = 0; i < n_leaf; i++) {
            if (i % 5 == 4) continue;
            acc.pixels[i].sum_flux = (double)(std::rand() % 20000);
            acc.pixels[i].sum_area = A_p;
            acc.pixels[i].n_contrib = 1;
        }
        double t0 = now_ms();
        int tr = writer.add_tile((uint64_t)t, acc, nullptr, OccupancyMode::FULL);
        double t1 = now_ms();
        if (tr != 0) { printf("add_tile %d failed rc=%d\n", t, tr); return 1; }
        t_add_total += t1 - t0;
    }
    printf("add_tile x%d: %.1f ms (%.2f ms/tile)\n",
           n_tiles, t_add_total, t_add_total / n_tiles);

    double t_fin0 = now_ms();
    int fr = writer.finalize();
    double t_fin1 = now_ms();
    printf("finalize: %.1f ms\n", t_fin1 - t_fin0);
    if (fr != 0) { printf("finalize failed rc=%d\n", fr); return 1; }
    return 0;
}
