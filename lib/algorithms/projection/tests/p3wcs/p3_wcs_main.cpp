// tests/backend/p3_wcs_main.cpp — P3-002 WCS 探针(供 Python 独立参考比对)
// 用法:
//   p3_wcs make <ra> <dec> <scale> <W> <H> <east_left|east_right> <pa_deg>
//       → "OK <crpix_x> <crpix_y> <cd11> <cd12> <cd21> <cd22>" | "FAIL <code>"
//   p3_wcs p2w <ra> <dec> <scale> <W> <H> <parity> <pa> <x> <y>
//       → "OK <ra> <dec>" | "FAIL <code>"
//   p3_wcs w2p <...同上...> <ra> <dec> → "OK <x> <y>" | "FAIL <code>"
//   p3_wcs kw <ra> <dec> <scale> <W> <H> <parity> <pa> → 关键字文本
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "p3_wcs.h"

using namespace astrocs::phase3;

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string mode = argv[1];
    auto d5 = [](double v) { return std::abs(v - (int)(v + (v >= 0 ? 0.5 : -0.5))) < 1e-12; };
    (void)d5;
    if (mode == "kw") {
        if (argc < 9) return 2;
        P3WcsDescriptor d;
        const P3WcsStatus st = p3_wcs_make(atof(argv[2]), atof(argv[3]), atof(argv[4]),
                                           atoi(argv[5]), atoi(argv[6]), argv[7],
                                           atof(argv[8]), &d);
        if (st != P3_WCS_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("%s", p3_wcs_fits_keywords(&d).c_str());
        return 0;
    }
    if (mode == "make") {
        if (argc < 9) return 2;
        P3WcsDescriptor d;
        const P3WcsStatus st = p3_wcs_make(atof(argv[2]), atof(argv[3]), atof(argv[4]),
                                           atoi(argv[5]), atoi(argv[6]), argv[7],
                                           atof(argv[8]), &d);
        if (st != P3_WCS_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("OK %.10f %.10f %.12e %.12e %.12e %.12e\n",
                    d.crpix_x, d.crpix_y, d.cd[0][0], d.cd[0][1], d.cd[1][0], d.cd[1][1]);
        return 0;
    }
    if (argc < 11) return 2;
    P3WcsDescriptor d;
    const P3WcsStatus st = p3_wcs_make(atof(argv[2]), atof(argv[3]), atof(argv[4]),
                                       atoi(argv[5]), atoi(argv[6]), argv[7],
                                       atof(argv[8]), &d);
    if (st != P3_WCS_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
    if (mode == "p2w") {
        double ra, dec;
        const P3WcsStatus s = p3_wcs_pix2world(&d, atof(argv[9]), atof(argv[10]), &ra, &dec);
        if (s != P3_WCS_OK) { std::printf("FAIL %d\n", (int)s); return 1; }
        std::printf("OK %.12f %.12f\n", ra, dec);
        return 0;
    }
    if (mode == "w2p") {
        double x, y;
        const P3WcsStatus s = p3_wcs_world2pix(&d, atof(argv[9]), atof(argv[10]), &x, &y);
        if (s != P3_WCS_OK) { std::printf("FAIL %d\n", (int)s); return 1; }
        std::printf("OK %.12f %.12f\n", x, y);
        return 0;
    }
    return 2;
}
