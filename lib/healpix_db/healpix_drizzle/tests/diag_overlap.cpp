// ============================================================================
// 诊断程序: 检查 compute_overlap_area 的几何精度
//
// 目标:
//   1. 确认 get_healpix_boundary_sampled 的细分后顶点数
//   2. 确认 HEALPix 像素面积 vs 理论值
//   3. 确认 drop 完全在 HEALPix 内时 overlap_area == drop_area
//   4. 确认 drop 跨越 HEALPix 边界时 sum(overlap) == drop_area
// ============================================================================

#include "spherical_overlap.h"
#include "drizzle_engine.h"
#include "wcs_sip.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static spherical::Vec3 radec_to_vec_local(double ra, double dec) {
    return spherical::radec_to_vec(ra, dec);
}

// 构造简单 WCS (对角 CD, 无 SIP)
static drizzle::WcsParams make_simple_wcs(double scale_arcsec_per_px,
                                          int img_w, int img_h,
                                          double crval_ra = 45.0,
                                          double crval_dec = 0.0) {
    drizzle::WcsParams wcs;
    wcs.has_wcs = true;
    double scale_deg = scale_arcsec_per_px / 3600.0;
    wcs.cd[0] = scale_deg;  wcs.cd[1] = 0.0;
    wcs.cd[2] = 0.0;        wcs.cd[3] = scale_deg;
    wcs.crval[0] = crval_ra;
    wcs.crval[1] = crval_dec;
    wcs.crpix[0] = img_w * 0.5 + 0.5;
    wcs.crpix[1] = img_h * 0.5 + 0.5;
    std::strcpy(wcs.ctype1, "RA---TAN-SIP");
    std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
    wcs.sip.order = 0;
    wcs.sip.ap_order = 0;
    return wcs;
}

// 构造 NESTED ipix
static uint64_t make_nested_ipix(int bighp, int x, int y, int nside) {
    int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
    int xv = x, yv = y;
    for (int i = 0; i < 32; i++) {
        ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
        xv >>= 1; yv >>= 1;
        if (!xv && !yv) break;
    }
    return (uint64_t)ipix;
}

// WCS pixelToSky 回调
static bool wcsPixelToSkyCallback(double x, double y, double& ra, double& dec,
                                  void* user_data) {
    const drizzle::WcsSip* wcs = static_cast<const drizzle::WcsSip*>(user_data);
    wcs->pixelToSky(x, y, ra, dec);
    return std::isfinite(ra) && std::isfinite(dec);
}

int main() {
    printf("================================================================\n");
    printf("诊断: compute_overlap_area 几何精度\n");
    printf("================================================================\n\n");

    // ---- 测试1: nside=64 赤道带像素面积精度 ----
    printf("==== 测试1: nside=64 赤道带像素面积精度 ====\n");
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        double theory_area = 4.0 * M_PI / (12.0 * (double)nside * nside);

        // 检查多个像素
        int test_pixels[][3] = {
            {4, 32, 32},  // 赤道, ra≈45°
            {4, 32, 31},  // 赤道, ra≈45°, dec<0
            {4, 40, 40},  // 赤道, ra偏大
            {4, 16, 16},  // 赤道, ra偏小
            {4, 50, 20},  // 赤道, 非对称
        };
        int n_tests = sizeof(test_pixels) / sizeof(test_pixels[0]);

        for (int t = 0; t < n_tests; t++) {
            int bighp = test_pixels[t][0];
            int x = test_pixels[t][1];
            int y = test_pixels[t][2];
            uint64_t ipix = make_nested_ipix(bighp, x, y, nside);

            std::vector<spherical::Vec3> boundary =
                spherical::get_healpix_boundary_sampled(hp, ipix, nside, 1);
            double area = spherical::spherical_polygon_area(boundary);

            double ra_c, dec_c;
            hp.pix2radec(ipix, &ra_c, &dec_c);

            printf("  ipix(bighp=%d, x=%d, y=%d) center=(%.4f, %.4f) "
                   "verts=%zu area=%.6e theory=%.6e rel_err=%.3e\n",
                   bighp, x, y, ra_c, dec_c,
                   boundary.size(), area, theory_area,
                   std::fabs(area - theory_area) / theory_area);
        }
    }

    // ---- 测试2: drop 完全在 HEALPix 内时 overlap_area == drop_area ----
    printf("\n==== 测试2: drop 完全在内时 overlap == drop_area ====\n");
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        uint64_t ipix = make_nested_ipix(4, 32, 32, nside);
        double ra_c, dec_c;
        hp.pix2radec(ipix, &ra_c, &dec_c);

        // 构造小 drop (1" 大小) 在像素中心
        double drop_size_deg = 1.0 / 3600.0;
        double half = drop_size_deg * 0.5;
        double cd = std::cos(dec_c * M_PI / 180.0);
        double half_ra = (std::fabs(cd) > 1e-10) ? (half / cd) : half;

        std::vector<spherical::Vec3> drop(4);
        drop[0] = spherical::radec_to_vec(ra_c - half_ra, dec_c - half);
        drop[1] = spherical::radec_to_vec(ra_c + half_ra, dec_c - half);
        drop[2] = spherical::radec_to_vec(ra_c + half_ra, dec_c + half);
        drop[3] = spherical::radec_to_vec(ra_c - half_ra, dec_c + half);

        double drop_area = spherical::spherical_polygon_area(drop);
        double overlap = spherical::compute_overlap_area(drop, hp, ipix);

        printf("  drop 1\" at pixel center (%.6f, %.6f)\n", ra_c, dec_c);
        printf("  drop_area = %.6e\n", drop_area);
        printf("  overlap   = %.6e\n", overlap);
        printf("  rel_err   = %.3e\n", std::fabs(overlap - drop_area) / drop_area);
    }

    // ---- 测试3: drop 跨越 HEALPix 边界时 sum(overlap) == drop_area ----
    printf("\n==== 测试3: drop 跨越边界时 sum(overlap) == drop_area ====\n");
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);

        // drop 在像素边界上 (ra=45° 是像素 (32,32) 的南-北对角线)
        // 实际上像素边界是 x=const 或 y=const, 不是 ra=const
        // 用像素 (32, 32) 的南角 (ra=45°, dec=0°) 作为 drop 中心
        // drop 跨越 y=32 边界 (像素 31 和 32 之间)

        double drop_size_deg = 1.0 / 3600.0;  // 1" 大小
        double half = drop_size_deg * 0.5;
        double ra_c = 45.0, dec_c = 0.0;
        double cd = std::cos(dec_c * M_PI / 180.0);
        double half_ra = half / cd;

        std::vector<spherical::Vec3> drop(4);
        drop[0] = spherical::radec_to_vec(ra_c - half_ra, dec_c - half);
        drop[1] = spherical::radec_to_vec(ra_c + half_ra, dec_c - half);
        drop[2] = spherical::radec_to_vec(ra_c + half_ra, dec_c + half);
        drop[3] = spherical::radec_to_vec(ra_c - half_ra, dec_c + half);

        double drop_area = spherical::spherical_polygon_area(drop);

        // 查询候选像素
        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        printf("  drop 1\" at (%.6f, %.6f), drop_area=%.6e\n", ra_c, dec_c, drop_area);
        printf("  candidates = %zu pixels\n", candidates.size());

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 0.0) {
                double ra_p, dec_p;
                hp.pix2radec(ipix, &ra_p, &dec_p);
                printf("    ipix=%llu center=(%.4f, %.4f) overlap=%.6e (%.2f%%)\n",
                       (unsigned long long)ipix, ra_p, dec_p, a, 100.0 * a / drop_area);
                sum_overlap += a;
            }
        }
        printf("  sum_overlap = %.6e, rel_err = %.3e\n",
               sum_overlap, std::fabs(sum_overlap - drop_area) / drop_area);
    }

    // ---- 测试4: 模拟整帧通量守恒 (16x16, 1"/px, nside=64) ----
    printf("\n==== 测试4: 整帧通量守恒模拟 (16x16, 1\"/px, nside=64) ====\n");
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        int w = 16, h = 16;
        double scale = 1.0;  // 1"/px
        auto wcs_params = make_simple_wcs(scale, w, h, 45.0, 0.0);
        drizzle::WcsSip wcsip(wcs_params);

        double total_drop_area = 0.0;
        double total_overlap_area = 0.0;
        int n_pixels = 0;
        int n_crossing = 0;  // 跨越边界的源像素数

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // drop 4 角 (pixfrac=1.0)
                double corners_xy[4][2] = {
                    {(double)x - 0.5, (double)y - 0.5},
                    {(double)x + 0.5, (double)y - 0.5},
                    {(double)x + 0.5, (double)y + 0.5},
                    {(double)x - 0.5, (double)y + 0.5}
                };

                std::vector<spherical::Vec3> drop(4);
                bool valid = true;
                for (int i = 0; i < 4; i++) {
                    double ra, dec;
                    wcsip.pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec);
                    if (!std::isfinite(ra) || !std::isfinite(dec)) { valid = false; break; }
                    drop[i] = spherical::radec_to_vec(ra, dec);
                }
                if (!valid) continue;

                double drop_area = spherical::spherical_polygon_area(drop);
                total_drop_area += drop_area;
                n_pixels++;

                // 查询候选
                std::vector<uint64_t> candidates;
                spherical::query_candidate_pixels(drop, hp, candidates);

                double sum_overlap = 0.0;
                for (uint64_t ipix : candidates) {
                    double a = spherical::compute_overlap_area(drop, hp, ipix);
                    if (a > 0.0) sum_overlap += a;
                }
                total_overlap_area += sum_overlap;

                double rel_err = std::fabs(sum_overlap - drop_area) / drop_area;
                if (rel_err > 1e-6) {
                    n_crossing++;
                    if (n_crossing <= 5) {
                        double ra_c, dec_c;
                        wcsip.pixelToSky(x, y, ra_c, dec_c);
                        printf("  pixel(%d,%d) center=(%.6f,%.6f) drop_area=%.6e "
                               "sum_overlap=%.6e rel_err=%.3e candidates=%zu\n",
                               x, y, ra_c, dec_c, drop_area, sum_overlap, rel_err,
                               candidates.size());
                    }
                }
            }
        }

        printf("\n  总计: %d 源像素, %d 跨越边界(rel_err>1e-6)\n", n_pixels, n_crossing);
        printf("  total_drop_area    = %.10e\n", total_drop_area);
        printf("  total_overlap_area = %.10e\n", total_overlap_area);
        printf("  rel_err = %.3e\n",
               std::fabs(total_overlap_area - total_drop_area) / total_drop_area);
    }

    // ---- 测试4b: pixel(7,7) 详细裁剪诊断 ----
    printf("\n==== 测试4b: pixel(7,7) 详细裁剪诊断 ====\n");
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        auto wcs_params = make_simple_wcs(1.0, 16, 16, 45.0, 0.0);
        drizzle::WcsSip wcsip(wcs_params);

        // pixel(7,7) drop 4 角
        double px = 7.0, py = 7.0;
        double corners_xy[4][2] = {
            {px - 0.5, py - 0.5}, {px + 0.5, py - 0.5},
            {px + 0.5, py + 0.5}, {px - 0.5, py + 0.5}
        };
        std::vector<spherical::Vec3> drop(4);
        for (int i = 0; i < 4; i++) {
            double ra, dec;
            wcsip.pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec);
            drop[i] = spherical::radec_to_vec(ra, dec);
            double r, d;
            spherical::vec_to_radec(drop[i], r, d);
            printf("  drop[%d] xy=(%.1f,%.1f) -> ra=%.10f dec=%.10f\n",
                   i, corners_xy[i][0], corners_xy[i][1], r, d);
        }
        double drop_area = spherical::spherical_polygon_area(drop);
        printf("  drop_area=%.6e\n", drop_area);

        // 查询候选
        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);
        printf("  candidates=%zu\n", candidates.size());

        double sum = 0.0;
        for (uint64_t ipix : candidates) {
            // 获取 HEALPix 边界
            std::vector<spherical::Vec3> hp_boundary =
                spherical::get_healpix_boundary_sampled(hp, ipix, nside, 1);

            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 0.0) {
                double ra_p, dec_p;
                hp.pix2radec(ipix, &ra_p, &dec_p);
                printf("\n  ipix=%llu center=(%.6f,%.6f) verts=%zu overlap=%.6e (%.2f%%)\n",
                       (unsigned long long)ipix, ra_p, dec_p,
                       hp_boundary.size(), a, 100.0 * a / drop_area);

                // 手动重复裁剪过程, 打印诊断
                spherical::Vec3 hp_center = {0, 0, 0};
                for (auto& v : hp_boundary) { hp_center.x += v.x; hp_center.y += v.y; hp_center.z += v.z; }
                hp_center = spherical::normalize(hp_center);
                double cra, cdec;
                spherical::vec_to_radec(hp_center, cra, cdec);
                printf("    hp_center=(%.6f,%.6f) verts=%zu\n", cra, cdec, hp_boundary.size());

                // 构造裁剪法向量
                std::vector<spherical::Vec3> clip_normals;
                int nb = (int)hp_boundary.size();
                for (int i = 0; i < nb; i++) {
                    auto& A = hp_boundary[i];
                    auto& B = hp_boundary[(i + 1) % nb];
                    spherical::Vec3 n = spherical::cross(A, B);
                    if (spherical::dot(n, hp_center) < 0.0) { n.x = -n.x; n.y = -n.y; n.z = -n.z; }
                    n = spherical::normalize(n);
                    clip_normals.push_back(n);
                }

                // 逐边裁剪, 打印每步结果
                std::vector<spherical::Vec3> current = drop;
                printf("    初始 drop verts=%zu\n", current.size());
                for (size_t ci = 0; ci < clip_normals.size() && current.size() >= 3; ci++) {
                    std::vector<spherical::Vec3> next =
                        spherical::sutherland_hodgman_spherical(current, {clip_normals[ci]});
                    printf("    clip[%zu] -> verts=%zu\n", ci, next.size());
                    current = next;
                }
                printf("    最终 intersection verts=%zu\n", current.size());
                if (current.size() >= 3) {
                    double manual_area = spherical::spherical_polygon_area(current);
                    printf("    manual_area=%.6e (vs compute_overlap=%.6e)\n", manual_area, a);
                }

                sum += a;
            }
        }
        printf("\n  sum_overlap=%.6e rel_err=%.3e\n", sum,
               std::fabs(sum - drop_area) / drop_area);
    }

    // ---- 测试5: nside=256, drop=1° at (60,30) 候选漏选诊断 ----
    printf("\n==== 测试5: nside=256, drop=1° at (60,30) 候选漏选诊断 ====\n");
    {
        int nside = 256;
        healpix::HealpixCore hp(nside, true);
        double ra_c = 60.0, dec_c = 30.0, size = 1.0;
        double half = size * 0.5;
        double cd = std::cos(dec_c * M_PI / 180.0);
        double half_ra = half / cd;

        std::vector<spherical::Vec3> drop(4);
        drop[0] = spherical::radec_to_vec(ra_c - half_ra, dec_c - half);
        drop[1] = spherical::radec_to_vec(ra_c + half_ra, dec_c - half);
        drop[2] = spherical::radec_to_vec(ra_c + half_ra, dec_c + half);
        drop[3] = spherical::radec_to_vec(ra_c - half_ra, dec_c + half);

        double drop_area = spherical::spherical_polygon_area(drop);

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        double sum_overlap = 0.0;
        int n_nonzero = 0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) {
                sum_overlap += a;
                n_nonzero++;
            }
        }

        printf("  drop 1° at (60, 30), drop_area=%.6e\n", drop_area);
        printf("  candidates=%zu, n_nonzero=%d\n", candidates.size(), n_nonzero);
        printf("  sum_overlap=%.6e, rel_err=%.3e\n",
               sum_overlap, std::fabs(sum_overlap - drop_area) / drop_area);

        // 检查候选缓冲是否足够
        double hp_res = hp.pixelResolutionArcsec();
        printf("  hp_res=%.2f\", 3.0*hp_res=%.2f\"\n", hp_res, 3.0 * hp_res);

        // 手动扩大候选范围, 检查是否有漏选
        // 用 5.0*hp_res 缓冲重新查询
        // (需要直接调用 queryDisc, 这里通过修改缓冲验证)
        // 由于 query_candidate_pixels 内部硬编码 3.0*hp_res, 这里无法直接修改
        // 但可以通过检查 candidates 之外是否有重叠像素来验证
        printf("  检查 candidates 之外是否有重叠像素...\n");

        // 获取 drop 中心附近的像素 (用 queryDisc 大范围查询)
        double query_radius_arcsec = (half + 5.0 * hp_res / 3600.0) * 3600.0;
        std::vector<int64_t> wide_pixels = hp.queryDisc(ra_c, dec_c, query_radius_arcsec);

        int n_missed = 0;
        for (int64_t ipix_signed : wide_pixels) {
            if (ipix_signed < 0) continue;
            uint64_t ipix = (uint64_t)ipix_signed;
            // 检查是否在 candidates 中
            bool found = false;
            for (uint64_t c : candidates) {
                if (c == ipix) { found = true; break; }
            }
            if (!found) {
                // 检查是否有重叠
                double a = spherical::compute_overlap_area(drop, hp, ipix);
                if (a > 1e-15) {
                    n_missed++;
                    double ra_p, dec_p;
                    hp.pix2radec(ipix, &ra_p, &dec_p);
                    printf("    MISSED ipix=%llu center=(%.4f, %.4f) overlap=%.6e\n",
                           (unsigned long long)ipix, ra_p, dec_p, a);
                }
            }
        }
        printf("  漏选像素数: %d\n", n_missed);
    }

    printf("\n================================================================\n");
    printf("诊断完成\n");
    printf("================================================================\n");
    return 0;
}
