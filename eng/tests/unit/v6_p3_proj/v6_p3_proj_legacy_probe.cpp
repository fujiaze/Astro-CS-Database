// eng/tests/unit/v6_p3_proj/v6_p3_proj_legacy_probe.cpp — 偏差证据探针（非 V6 实现）
// 打印 legacy registry v1（lib/algorithms/projection/p3_projection.cpp）的四投影 pix2world，
// 供 p3_proj_legacy_deviation.py 与 astropy/WCSLIB（标准 FITS WCS Paper II）对比，
// 量化 V6 前的 CAR/AIT 约定偏差。本探针不参与 V6 生产路径。
#include "p3_projection.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using astrocs::phase3proj::P3ProjectionDescriptor;
using astrocs::phase3proj::P3ProjectionId;
using astrocs::phase3proj::P3ProjectionStatus;

int main(int argc, char** argv) {
    if (argc < 7) {
        std::fprintf(stderr, "usage: %s PROJ RA0 DEC0 SCALE W H [grid_n]\n", argv[0]);
        return 2;
    }
    const char* proj = argv[1];
    const double ra0 = std::atof(argv[2]), dec0 = std::atof(argv[3]);
    const double scale = std::atof(argv[4]);
    const int w = std::atoi(argv[5]), h = std::atoi(argv[6]);
    const int gn = (argc > 7) ? std::atoi(argv[7]) : 5;
    P3ProjectionId id = P3ProjectionId::TAN;
    if (std::strcmp(proj, "SIN") == 0) id = P3ProjectionId::SIN;
    else if (std::strcmp(proj, "CAR") == 0) id = P3ProjectionId::CAR;
    else if (std::strcmp(proj, "AIT") == 0) id = P3ProjectionId::AIT;
    P3ProjectionDescriptor d;
    const P3ProjectionStatus mk =
        astrocs::phase3proj::p3_projection_make(id, ra0, dec0, scale, w, h,
                                                "east_left", 0.0, &d);
    std::printf("V6PROBE proj=%s status_make=%d crval1=%.17g crval2=%.17g scale=%.17g "
                "w=%d h=%d crpix1=%.17g crpix2=%.17g cd11=%.17g cd12=%.17g cd21=%.17g "
                "cd22=%.17g\n",
                proj, (int)mk, ra0, dec0, scale, w, h, d.crpix_x, d.crpix_y,
                d.cd[0][0], d.cd[0][1], d.cd[1][0], d.cd[1][1]);
    if (mk != P3ProjectionStatus::P3_PROJ_OK) return 0;
    for (int jj = 0; jj < gn; ++jj) {
        const double y = (h - 1) * (gn == 1 ? 0.0 : double(jj) / (gn - 1));
        for (int ii = 0; ii < gn; ++ii) {
            const double x = (w - 1) * (gn == 1 ? 0.0 : double(ii) / (gn - 1));
            double ra = 0, dec = 0;
            const P3ProjectionStatus st =
                astrocs::phase3proj::p3_projection_pix2world(&d, x, y, &ra, &dec);
            std::printf("ROW x=%.17g y=%.17g status=%d ra=%.17g dec=%.17g\n", x, y,
                        (int)st, ra, dec);
        }
    }
    std::printf("END\n");
    return 0;
}
