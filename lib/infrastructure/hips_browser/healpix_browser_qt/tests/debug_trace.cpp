#include <cassert>
#include <cmath>
#include <cstdio>
#include "healpix_math.h"

int main() {
    // 测试 nside=2 全部 48 像素的往返一致性
    uint32_t nside = 2;
    int pass = 0, fail = 0;
    for (uint64_t ipix = 0; ipix < 48; ipix++) {
        double ra, dec;
        HealpixMath::pix2ang_nest(nside, ipix, ra, dec);
        uint64_t ipix2 = HealpixMath::ang2pix_nest(nside, ra, dec);
        if (ipix == ipix2) {
            pass++;
        } else {
            fail++;
            if (fail <= 5) {
                printf("FAIL ipix=%llu ra=%.2f dec=%.2f → ipix2=%llu\n",
                       (unsigned long long)ipix, ra, dec, (unsigned long long)ipix2);
            }
        }
    }
    printf("nside=%u: pass=%d fail=%d\n\n", nside, pass, fail);

    // 测试多个 nside
    for (uint32_t ns : {1u, 2u, 4u, 8u, 64u}) {
        pass = 0; fail = 0;
        uint64_t total = 12ULL * ns * ns;
        for (uint64_t ipix = 0; ipix < total; ipix++) {
            double ra, dec;
            HealpixMath::pix2ang_nest(ns, ipix, ra, dec);
            uint64_t ipix2 = HealpixMath::ang2pix_nest(ns, ra, dec);
            if (ipix == ipix2) pass++;
            else fail++;
        }
        printf("nside=%u: pass=%d fail=%d (total=%llu)\n",
               ns, pass, fail, (unsigned long long)total);
    }
    return 0;
}
