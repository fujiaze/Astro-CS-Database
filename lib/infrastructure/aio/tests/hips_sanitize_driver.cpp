// hips_sanitize_driver.cpp - ASan/UBSan 驱动: HiPS writer + reader 可移植核心
// 用法: <exe> <out_dir>
#include "aio_hips.h"
#include "aio_hips_reader.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const char* out = argv[1];
    const uint32_t nside = 2048;
    const uint32_t leaf_order = 11;
    const double pi = std::acos(-1.0);
    const double a_cell = 4.0 * pi / (12.0 * (double)nside * nside);
    const size_t n = 512 * 512;

    AioHipsProductSet* ps = aio_hips_product_begin(
        out, nside, 512, AIO_HIPS_FLOAT32, AIO_HIPS_PRODUCT_ALL,
        "ivo://astrocs/test", "Sanitizer HiPS", "L", 300.0, "2026-08-08", 0);
    if (!ps) { std::fprintf(stderr, "begin fail: %s\n", aio_hips_last_error()); return 3; }
    for (uint64_t tile_ipix = 0; tile_ipix < 3; ++tile_ipix) {
        std::vector<float> flux(n, 0.0f), area(n, 0.0f);
        for (size_t i = 0; i < n; ++i) {
            // 合成数据: 中心区域有值, 边缘空
            // HIPS-IMG-001: i 是 NESTED local 序, 先位解交错得到 tile 内 (x,y)
            size_t x = 0, y = 0;
            for (unsigned b = 0; b < 9; ++b) {
                x |= ((i >> (2 * b)) & 1ULL) << b;
                y |= ((i >> (2 * b + 1)) & 1ULL) << b;
            }
            if (x > 100 && x < 400 && y > 100 && y < 400) {
                flux[i] = 2.0f;
                area[i] = (float)(0.5 * a_cell);
            }
        }
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tile_ipix;
        view.leaf_order = leaf_order;
        view.width = 512;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = flux.data();
        view.covered_area = area.data();
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
            std::fprintf(stderr, "tile fail: %s\n", aio_hips_last_error());
            aio_hips_abort(ps);
            return 4;
        }
    }
    AioHipsSnrPoint pts[2];
    pts[0] = {10.0, 89.5, 12.3, 1001, 1u, 1u};
    pts[1] = {20.0, -30.0, 5.5, 1002, 4u, 0u};
    aio_hips_write_snr_points(ps, pts, 2);
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "finalize fail: %s\n", aio_hips_last_error());
        return 5;
    }
    // 读回验证
    AioHipsDataset* ds = aio_hips_open(out, AIO_HIPS_RD_SIGNAL);
    if (!ds) { std::fprintf(stderr, "open fail: %s\n", aio_hips_reader_last_error()); return 6; }
    int tc = aio_hips_tile_count(ds);
    std::vector<float> buf(n);
    for (int i = 0; i < tc; ++i) {
        uint64_t ipix = 0;
        if (aio_hips_tile_ipix(ds, i, &ipix) != 0) return 7;
        if (aio_hips_read_tile_f32(ds, ipix, buf.data()) != 0) {
            std::fprintf(stderr, "read fail: %s\n", aio_hips_reader_last_error());
            return 8;
        }
    }
    aio_hips_close(ds);
    std::printf("HIPS_SANITIZE_OK tiles=%d\n", tc);
    return 0;
}
