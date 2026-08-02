// diag_pixel_boundary.cpp — R07 聚焦诊断: 检查极区像素边界与裁剪
// 对 ipix 8189 (north_pf0p5 场景的失败像素) 输出:
//   1. get_healpix_boundary_sampled 顶点数与坐标
//   2. 边界是否凸 (检查每条边的法向量方向是否与像素中心一致)
//   3. Sutherland-Hodgman 裁剪结果

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <set>
#include <algorithm>

#include "spherical_overlap.h"
#include "fast_overlap.h"
#include "../healpix_stack/healpix_core.h"
#include "wcs_sip.h"
#include "fits_reader.h"

using spherical::Vec3;
using drizzle::FitsImage;
using drizzle::WcsSip;
using drizzle::WcsParams;

static const double PI      = 3.14159265358979323846;
static const double TWO_PI  = 2.0 * PI;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;

// 构造合成图像 (与 diag_polar_flux.cpp 一致)
static FitsImage make_synthetic_image(double scale_arcsec, int size,
                                      double ra0, double dec0) {
    FitsImage img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.pixels.resize(static_cast<size_t>(size) * size);
    std::fill(img.pixels.begin(), img.pixels.end(), 1000.0f);

    const double scale_deg = scale_arcsec / 3600.0;
    WcsParams& wcs = img.wcs;
    wcs.has_wcs = true;
    wcs.cd[0] = -scale_deg; wcs.cd[1] = 0.0;
    wcs.cd[2] = 0.0;        wcs.cd[3] = scale_deg;
    wcs.crval[0] = ra0;
    wcs.crval[1] = dec0;
    wcs.crpix[0] = static_cast<double>(size / 2 + 1);
    wcs.crpix[1] = static_cast<double>(size / 2 + 1);
    std::strcpy(wcs.ctype1, "RA---TAN");
    std::strcpy(wcs.ctype2, "DEC--TAN");
    wcs.sip.order = 0;
    wcs.sip.ap_order = 0;
    img.bzero = 0.0;
    img.bscale = 1.0;
    img.photscal = 0.0;
    img.photappl = 0;
    return img;
}

int main() {
    const double scale_arcsec = 3600.0;
    const int nside = 64;
    const int img_size = 16;
    const double dec0 = 89.0;
    const double ra0 = 180.0;
    const double pixfrac = 0.5;

    healpix::HealpixCore hp(nside, true);

    FitsImage img = make_synthetic_image(scale_arcsec, img_size, ra0, dec0);
    WcsSip wcs(img.wcs);

    // 构造 drop (256段/边, 与 diag_polar_flux.cpp 一致)
    double px = (double)(img_size / 2);
    double py = (double)(img_size / 2);
    double half = 0.5 * pixfrac;
    double corners_xy[4][2] = {
        {px - half, py - half}, {px + half, py - half},
        {px + half, py + half}, {px - half, py + half}
    };
    std::vector<Vec3> drop_corners;
    drop_corners.reserve(4 * 256);
    for (int e = 0; e < 4; e++) {
        int en = (e + 1) % 4;
        for (int s = 0; s < 256; s++) {
            double t = (double)s / 256.0;
            double x = corners_xy[e][0] + t * (corners_xy[en][0] - corners_xy[e][0]);
            double y = corners_xy[e][1] + t * (corners_xy[en][1] - corners_xy[e][1]);
            double ra, dec;
            wcs.pixelToSky(x, y, ra, dec);
            drop_corners.push_back(spherical::radec_to_vec(ra, dec));
        }
    }

    std::printf("drop_corners: %zu vertices\n", drop_corners.size());
    std::printf("drop_area (Eriksson): %.15e\n", spherical::spherical_polygon_area(drop_corners));

    // 测试 ipix 8189 (失败像素) 和 8191 (OK像素)
    uint64_t test_pixels[] = {8189, 8191, 8183};
    for (uint64_t ipix : test_pixels) {
        std::printf("\n=== ipix %llu ===\n", (unsigned long long)ipix);
        double ra_c, dec_c;
        hp.pix2radec((int64_t)ipix, &ra_c, &dec_c);
        std::printf("  center: ra=%.6f dec=%.6f\n", ra_c, dec_c);

        // 获取边界
        std::vector<Vec3> boundary = spherical::get_healpix_boundary_sampled(hp, ipix, nside, 1);
        std::printf("  boundary vertices: %zu\n", boundary.size());

        // 输出每个顶点
        for (size_t i = 0; i < boundary.size(); i++) {
            double r, d;
            spherical::vec_to_radec(boundary[i], r, d);
            std::printf("    [%2zu] ra=%.6f dec=%.6f\n", i, r, d);
        }

        // 计算像素中心向量
        Vec3 hp_center_vec = spherical::radec_to_vec(ra_c, dec_c);

        // 用边界顶点平均作为中心 (与生产一致)
        Vec3 hp_center_avg = {0, 0, 0};
        for (const auto& v : boundary) {
            hp_center_avg.x += v.x; hp_center_avg.y += v.y; hp_center_avg.z += v.z;
        }
        hp_center_avg = spherical::normalize(hp_center_avg);

        double ra_a, dec_a;
        spherical::vec_to_radec(hp_center_avg, ra_a, dec_a);
        std::printf("  center_avg: ra=%.6f dec=%.6f\n", ra_a, dec_a);
        std::printf("  center_pix: ra=%.6f dec=%.6f\n", ra_c, dec_c);

        // 检查每条边的法向量方向
        int nb = (int)boundary.size();
        int flip_count_avg = 0, flip_count_pix = 0;
        std::printf("  edge normals (flip check):\n");
        for (int i = 0; i < nb; i++) {
            const Vec3& A = boundary[i];
            const Vec3& B = boundary[(i + 1) % nb];
            Vec3 n = spherical::cross(A, B);
            double dot_avg = spherical::dot(n, hp_center_avg);
            double dot_pix = spherical::dot(n, hp_center_vec);
            if (dot_avg < 0.0) flip_count_avg++;
            if (dot_pix < 0.0) flip_count_pix++;
            // 只输出前5和后5条边
            if (i < 5 || i >= nb - 5) {
                std::printf("    edge[%2d]: dot_avg=%+.6e dot_pix=%+.6e %s\n",
                            i, dot_avg, dot_pix,
                            (dot_avg * dot_pix < 0) ? "*** MISMATCH ***" : "");
            } else if (i == 5) {
                std::printf("    ...\n");
            }
        }
        std::printf("  flip_count: avg=%d pix=%d (mismatches=%d)\n",
                    flip_count_avg, flip_count_pix,
                    (flip_count_avg != flip_count_pix) ? std::abs(flip_count_avg - flip_count_pix) : 0);

        // 生产重叠面积
        double ov_prod = spherical::compute_overlap_area(drop_corners, hp, ipix);
        std::printf("  prod_overlap: %.15e\n", ov_prod);

        // 手动实现: 用 pix2radec 中心做三角形扇剖分
        // 对每个三角形 (hp_center_vec, boundary[i], boundary[i+1]) 与 drop 裁剪
        double total_tri = 0.0;
        for (int i = 0; i < nb; i++) {
            const Vec3& A = boundary[i];
            const Vec3& B = boundary[(i + 1) % nb];

            // 三角形 (hp_center_vec, A, B)
            std::vector<Vec3> tri = {hp_center_vec, A, B};

            // 构造三角形3条边的裁剪法向量
            std::vector<Vec3> tri_normals;
            for (int j = 0; j < 3; j++) {
                const Vec3& P1 = tri[j];
                const Vec3& P2 = tri[(j + 1) % 3];
                Vec3 n = spherical::cross(P1, P2);
                // 三角形中心
                Vec3 tc = spherical::normalize(Vec3{
                    (tri[0].x + tri[1].x + tri[2].x) / 3.0,
                    (tri[0].y + tri[1].y + tri[2].y) / 3.0,
                    (tri[0].z + tri[1].z + tri[2].z) / 3.0
                });
                if (spherical::dot(n, tc) < 0.0) {
                    n.x = -n.x; n.y = -n.y; n.z = -n.z;
                }
                tri_normals.push_back(spherical::normalize(n));
            }

            // S-H 裁剪
            auto intersection = spherical::sutherland_hodgman_spherical(drop_corners, tri_normals);
            if (intersection.size() >= 3) {
                double tri_area = spherical::spherical_polygon_area(intersection);
                total_tri += tri_area;
            }
        }
        std::printf("  tri_fan_overlap: %.15e\n", total_tri);
        std::printf("  ratio tri/prod: %.6f\n", (ov_prod > 0) ? total_tri / ov_prod : 0.0);
    }

    return 0;
}
