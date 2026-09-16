#include <cassert>
#include <cmath>
#include <cstdio>
#include "healpix_math.h"

int main() {
    // 调试 nside=1 的 12 个像素
    printf("=== nside=1 全部 12 像素 ===\n");
    for (uint64_t ipix = 0; ipix < 12; ipix++) {
        double ra, dec;
        HealpixMath::pix2ang_nest(1, ipix, ra, dec);
        printf("ipix=%llu: ra=%.4f dec=%.4f\n",
               (unsigned long long)ipix, ra, dec);
    }

    printf("\n=== nside=2 部分像素 ===\n");
    for (uint64_t ipix = 0; ipix < 48; ipix += 4) {
        double ra, dec;
        HealpixMath::pix2ang_nest(2, ipix, ra, dec);
        uint64_t ipix2 = HealpixMath::ang2pix_nest(2, ra, dec);
        printf("ipix=%llu: ra=%.4f dec=%.4f → ipix2=%llu %s\n",
               (unsigned long long)ipix, ra, dec,
               (unsigned long long)ipix2,
               ipix == ipix2 ? "OK" : "FAIL");
    }
    return 0;
}
