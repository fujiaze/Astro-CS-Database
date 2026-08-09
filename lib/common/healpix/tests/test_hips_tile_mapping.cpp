// ============================================================================
// test_hips_tile_mapping.cpp - 共享 HEALPix core 标准 HiPS tile 排列验证 (V5)
//
// 冻结标准 (IVOA HiPS 1.0 Image tile packaging, CDS Hipsgen MAPTILES
// 外部 Oracle 逐像素验证, 见 run/temp/v5_oracle 证据):
//   NESTED local index 位交错解码 -> (x, y)  (x=偶数位, y=奇数位)
//   FITS 列 (NAXIS1) = y, FITS 行 (NAXIS2) = tile_width-1-x
//   行主序 fits_index = (tile_width-1-x)*tile_width + y
//
// 硬门: 262144 全量 local 双向 roundtrip + fits 映射一致性 + 随机/边界。
// 用法: test_hips_tile_mapping.exe
// ============================================================================

#include "healpix/healpix_core.h"

#include <cstdio>
#include <cstdint>
#include <random>

int main() {
    const uint32_t shift = 9;
    const uint32_t tw = 512;
    const uint64_t n = 1ULL << (2 * shift);

    std::uint64_t bad_xy = 0, bad_fi = 0, bad_inv = 0;
    for (uint64_t local = 0; local < n; ++local) {
        uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, shift, x, y);
        const uint64_t back = astrocs::healpix::xy_to_nested_local(x, y, shift);
        if (back != local) {
            ++bad_xy;
            if (bad_xy <= 5)
                std::printf("XY_BAD local=%llu x=%u y=%u back=%llu\n",
                            (unsigned long long)local, x, y, (unsigned long long)back);
        }
        const uint64_t expect = (uint64_t)(511u - x) * 512u + (uint64_t)y;
        const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(local, shift, tw);
        if (fi != expect) {
            ++bad_fi;
            if (bad_fi <= 5)
                std::printf("FI_BAD local=%llu x=%u y=%u expect=%llu got=%llu\n",
                            (unsigned long long)local, x, y,
                            (unsigned long long)expect, (unsigned long long)fi);
        }
        const uint64_t inv = astrocs::healpix::fits_index_to_nested_local(fi, shift, tw);
        if (inv != local) {
            ++bad_inv;
            if (bad_inv <= 5)
                std::printf("INV_BAD local=%llu fi=%llu inv=%llu\n",
                            (unsigned long long)local, (unsigned long long)fi,
                            (unsigned long long)inv);
        }
    }

    // 边界/随机一致性 (两个已知锚点 + 随机)
    std::mt19937_64 rng(20260809ULL);
    std::uint64_t bad_rand = 0;
    const uint64_t anchors[] = {0, 1, 2, 3, 4, 511, 512, 262143, 131071, 65536};
    std::uint64_t samples = sizeof(anchors) / sizeof(anchors[0]);
    for (uint64_t a : anchors) {
        uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(a, shift, x, y);
        const uint64_t expect = (uint64_t)(511u - x) * 512u + (uint64_t)y;
        if (astrocs::healpix::nested_local_to_fits_index(a, shift, tw) != expect) ++bad_rand;
    }
    for (int i = 0; i < 100000; ++i) {
        const uint64_t local = rng() % n;
        uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, shift, x, y);
        const uint64_t expect = (uint64_t)(511u - x) * 512u + (uint64_t)y;
        if (astrocs::healpix::nested_local_to_fits_index(local, shift, tw) != expect) {
            ++bad_rand;
            break;
        }
        ++samples;
    }

    std::printf("cells=%llu bad_xy=%llu bad_fi=%llu bad_inv=%llu bad_rand=%llu samples=%llu\n",
                (unsigned long long)n, (unsigned long long)bad_xy,
                (unsigned long long)bad_fi, (unsigned long long)bad_inv,
                (unsigned long long)bad_rand, (unsigned long long)samples);
    if (bad_xy == 0 && bad_fi == 0 && bad_inv == 0 && bad_rand == 0) {
        std::printf("RESULT: PASS (standard HiPS tile mapping, 512x512)\n");
        return 0;
    }
    std::printf("RESULT: FAIL\n");
    return 1;
}
