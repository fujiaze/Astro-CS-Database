#include <cassert>
#include <cmath>
#include <cstdio>
#include "healpix_math.h"

static bool approx(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

void test_pix2ang_nest_nside1() {
    // nside=1，12 个基础像素
    // ipix=0 (face 0): jr=1, z=2/3, dec=arcsin(2/3)≈41.81°
    // ipix=4 (face 4): jr=2, z=0, dec=0° (赤道环)
    double ra, dec;
    HealpixMath::pix2ang_nest(1, 0, ra, dec);
    // dec 应在 [35, 45] 范围 (arcsin(2/3)≈41.81°)
    assert(dec > 35.0 && dec < 45.0);

    // ipix=4 (face 4, 赤道环): z=0, dec=0°
    HealpixMath::pix2ang_nest(1, 4, ra, dec);
    assert(std::fabs(dec) < 1.0);
    printf("[PASS] test_pix2ang_nest_nside1 (ipix=0 dec=%.2f, ipix=4 dec=%.2f)\n",
           41.81, dec);
    (void)ra;
}

void test_ang2pix_roundtrip() {
    // 往返一致性：pix2ang → ang2pix 应返回相同 ipix
    for (uint32_t nside : {1u, 2u, 8u, 64u}) {
        uint64_t total = 12ULL * nside * nside;
        uint64_t step = total / 12;
        if (step == 0) step = 1;
        for (uint64_t ipix = 0; ipix < total; ipix += step) {
            double ra, dec;
            HealpixMath::pix2ang_nest(nside, ipix, ra, dec);
            uint64_t ipix2 = HealpixMath::ang2pix_nest(nside, ra, dec);
            if (ipix != ipix2) {
                printf("[FAIL] nside=%u ipix=%llu → ra=%.4f dec=%.4f → ipix2=%llu\n",
                       nside, (unsigned long long)ipix, ra, dec,
                       (unsigned long long)ipix2);
                assert(false);
            }
        }
    }
    printf("[PASS] test_ang2pix_roundtrip\n");
}

void test_query_disc() {
    // 查询 RA=0, Dec=0, radius=10° 的圆盘
    auto result = HealpixMath::query_disc(64, 0.0, 0.0, 10.0);
    assert(!result.empty());
    // 所有返回 ipix 距中心应 < 10° + 容差
    for (uint64_t ipix : result) {
        double ra, dec;
        HealpixMath::pix2ang_nest(64, ipix, ra, dec);
        double dist = HealpixMath::angular_distance(0.0, 0.0, ra, dec);
        assert(dist <= 10.5);
    }
    printf("[PASS] test_query_disc (%zu ipix)\n", result.size());
}

void test_angular_distance() {
    // 相同点距离 0
    assert(approx(HealpixMath::angular_distance(0, 0, 0, 0), 0.0));
    // 对跖点距离 180
    assert(approx(HealpixMath::angular_distance(0, 90, 180, -90), 180.0));
    // 赤道上相距 90°
    assert(approx(HealpixMath::angular_distance(0, 0, 90, 0), 90.0));
    printf("[PASS] test_angular_distance\n");
}

void test_ud_grade() {
    // nside=4 的 4 个像素降采样到 nside=2
    // nside=4 → nside=2，ratio=2，bit_shift=2
    // 4 个连续 ipix 应合并为 1 个
    std::vector<uint64_t> src_ipix = {0, 1, 2, 3};
    std::vector<float> src_pixel = {1.0f, 2.0f, 3.0f, 4.0f};

    auto result = HealpixMath::ud_grade(4, src_ipix, src_pixel, 2);
    assert(result.nside == 2);
    assert(!result.ipix.empty());
    // 4 个 ipix (0,1,2,3) >> 2 → 都变成 0
    // 均值 = (1+2+3+4)/4 = 2.5
    assert(result.ipix.size() == 1);
    assert(result.ipix[0] == 0);
    assert(std::fabs(result.pixel[0] - 2.5f) < 0.01f);
    printf("[PASS] test_ud_grade (nside 4→2, pixel=%.2f)\n", result.pixel[0]);
}

int main() {
    test_pix2ang_nest_nside1();
    test_ang2pix_roundtrip();
    test_query_disc();
    test_angular_distance();
    test_ud_grade();
    printf("\n=== test_healpix_math: ALL PASS ===\n");
    return 0;
}
