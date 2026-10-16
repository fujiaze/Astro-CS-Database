// probe_p3_detail.cpp — S02 逐叶诊断探针（只读链接生产静态库 build/linux-sci）
//
// 目的：对「drop 中心 = nside 8192 四叶公共顶点」构型，把生产内部量逐叶导出：
//   drop 四角、g.drop_area、g.max_angle、g.center_d、clip_normals_d 的平面偏移 n·c、
//   候选 ipix 集合、逐叶 overlap_area、S-H 裁剪输出多边形。
// 用于与独立 Python Oracle（run/ACSDSDOC-02/S02/code/p3_oracle.py 系列）对拍。
//
// 用法：probe_p3_detail <out.json> [scale_arcsec] [pixfrac] [nside]
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using drizzle::WcsParams;
using drizzle::WcsSip;

static constexpr double PI = 3.14159265358979323846;
static constexpr double D2R = PI / 180.0;
static constexpr double R2D = 180.0 / PI;

using spherical::Vec3;

static Vec3 v3(double ra_deg, double dec_deg) {
    double a = ra_deg * D2R, d = dec_deg * D2R;
    return Vec3{std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}

int main(int argc, char** argv) {
    const char* outp = (argc > 1) ? argv[1] : "probe_p3_detail.json";
    double scale_arcsec = (argc > 2) ? std::atof(argv[2]) : 0.04;
    double pixfrac = (argc > 3) ? std::atof(argv[3]) : 0.8;
    int nside = (argc > 4) ? std::atoi(argv[4]) : 8192;

    const double SEAM_DEC = std::asin(2.0 / 3.0) * R2D;
    WcsParams p{};
    p.has_wcs = true;
    p.crval[0] = 45.0; p.crval[1] = SEAM_DEC;
    p.crpix[0] = 1.0;  p.crpix[1] = 1.0;          // 1x1 帧，中心 = CRVAL
    double s = scale_arcsec / 3600.0;
    p.cd[0] = s; p.cd[1] = 0.0; p.cd[2] = 0.0; p.cd[3] = s;
    std::strncpy(p.ctype1, "RA---TAN", sizeof(p.ctype1) - 1);
    std::strncpy(p.ctype2, "DEC--TAN", sizeof(p.ctype2) - 1);
    WcsSip wcs(p);

    const double dx[4] = {-0.5, +0.5, +0.5, -0.5};
    const double dy[4] = {-0.5, -0.5, +0.5, +0.5};
    std::vector<Vec3> corners;
    for (int i = 0; i < 4; ++i) {
        double ra, dec;
        wcs.pixelToSky(pixfrac * dx[i], pixfrac * dy[i], ra, dec);
        corners.push_back(v3(ra, dec));
    }

    spherical::DropGeometryT<double> g =
        spherical::build_drop_geometry<double>(corners, nullptr);

    healpix::HealpixCore hp(nside);
    std::vector<uint64_t> cands;
    spherical::query_candidate_pixels_fast<double>(corners, hp, cands);
    double hp_res = hp.pixelResolutionArcsec() * D2R / 3600.0;  // arcsec -> rad
    // 说明：生产 compute_overlap_area_g_ctx 内部用 hp.pixelResolutionArcsec()*ARCSEC_TO_RAD
    hp_res = hp.pixelResolutionArcsec() * (D2R / 3600.0);

    FILE* f = std::fopen(outp, "w");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", outp); return 2; }
    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"scale_arcsec\": %.10g, \"pixfrac\": %.10g, \"nside\": %d,\n",
                 scale_arcsec, pixfrac, nside);
    std::fprintf(f, "  \"crval\": [%.17g, %.17g],\n", p.crval[0], p.crval[1]);
    std::fprintf(f, "  \"drop_area_sr\": %.17g,\n", g.drop_area);
    std::fprintf(f, "  \"max_angle_rad\": %.17g,\n", g.max_angle);
    std::fprintf(f, "  \"center_d\": [%.17g, %.17g, %.17g],\n", g.center_d.x, g.center_d.y, g.center_d.z);
    std::fprintf(f, "  \"corners\": [");
    for (size_t i = 0; i < corners.size(); ++i)
        std::fprintf(f, "%s[%.17g,%.17g,%.17g]", i ? "," : "",
                     corners[i].x, corners[i].y, corners[i].z);
    std::fprintf(f, "],\n");
    std::fprintf(f, "  \"clip_normal_offsets\": [");
    for (size_t i = 0; i < g.clip_normals_d.size(); ++i) {
        const Vec3& n = g.clip_normals_d[i];
        double d = n.x * g.center_d.x + n.y * g.center_d.y + n.z * g.center_d.z;
        std::fprintf(f, "%s%.17g", i ? "," : "", d);
    }
    std::fprintf(f, "],\n");
    std::fprintf(f, "  \"n_candidates\": %zu,\n", cands.size());
    std::fprintf(f, "  \"per_leaf\": [\n");
    double sum = 0.0;
    for (size_t k = 0; k < cands.size(); ++k) {
        uint64_t ipix = cands[k];
        double a = spherical::compute_overlap_area_g_ctx<double>(g, hp, ipix, hp_res);
        sum += a;
        double ra_c, dec_c;
        hp.pix2radec((int64_t)ipix, &ra_c, &dec_c);
        std::array<Vec3, 4> b4{};
        spherical::get_healpix_boundary4<double>(hp, ipix, nside, b4);
        std::fprintf(f, "    {\"ipix\": %llu, \"center_deg\": [%.17g, %.17g], \"overlap_area_sr\": %.17g, \"boundary4\": [",
                     (unsigned long long)ipix, ra_c, dec_c, a);
        for (int i = 0; i < 4; ++i)
            std::fprintf(f, "%s[%.17g,%.17g,%.17g]", i ? "," : "", b4[i].x, b4[i].y, b4[i].z);
        std::fprintf(f, "]}%s\n", (k + 1 < cands.size()) ? "," : "");
    }
    std::fprintf(f, "  ],\n");
    std::fprintf(f, "  \"sum_overlap_sr\": %.17g,\n", sum);
    std::fprintf(f, "  \"sum_over_drop_minus_1\": %.17g\n", sum / g.drop_area - 1.0);
    std::fprintf(f, "}\n");
    std::fclose(f);
    std::printf("drop_area=%.17g sum=%.17g eps=%.6e nleaf=%zu max_angle=%.6e\n",
                g.drop_area, sum, sum / g.drop_area - 1.0, cands.size(), g.max_angle);
    return 0;
}
