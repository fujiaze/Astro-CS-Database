// tests/backend/phase2_fixture_main.cpp — 合成 HiPS fixture (CLI-005)
// 用法: phase2_fixture --make <dir>   生成 <dir>/F1.hips 与 <dir>/F2.hips(order0, 12 基元, 常量域)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "aio_hips.h"

namespace {
constexpr uint32_t TW = 512;  // nside=512 → 叶级 order 0(tile 全域)
constexpr float F1 = 1.00f, F2 = 1.25f, AREA = 1.0e-8f;
}

static bool write_frame(const std::string& path, float flux) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), TW, TW, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test", "CLI-005 synthetic", "R", 60.0, "2026-08-28T00:00:00Z", 0);
    if (!ps) { std::fprintf(stderr, "begin failed: %s\n", aio_hips_last_error()); return false; }
    std::vector<float> sig(TW * TW, flux), area(TW * TW, AREA);
    for (uint64_t ipix = 0; ipix < 12; ++ipix) {   // nside=512 NESTED 基元 index=ipix/... 用 0..11 作为 order0 tile 父单元
        AstroSphereTileView v{};
        v.parent_ipix = ipix;   // K=0: parent=nside3 基元... 由 writer 映射
        v.leaf_order = 9;   // nside=512 → leaf L=9, tile_order=0
        v.width = TW;
        v.data_type = AIO_HIPS_FLOAT32;
        v.flux_sum = sig.data();
        v.covered_area = area.data();
        v.valid_mask = nullptr;
        if (aio_hips_write_signal_support_tile(ps, &v) != 0) {
            std::fprintf(stderr, "tile write failed ipix=%llu\n", (unsigned long long)ipix);
            aio_hips_abort(ps);
            return false;
        }
    }
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "finalize failed: %s\n", aio_hips_last_error());
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3 || std::strcmp(argv[1], "--make") != 0) {
        std::fprintf(stderr, "usage: --make <dir>\n");
        return 2;
    }
    const std::string dir = argv[2];
    if (!write_frame(dir + "/F1.hips", F1)) return 3;
    if (!write_frame(dir + "/F2.hips", F2)) return 3;
    std::printf("HIPS_FIXTURES_OK\n");
    return 0;
}
