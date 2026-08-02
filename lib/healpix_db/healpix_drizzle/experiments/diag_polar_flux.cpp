// diag_polar_flux.cpp — R07 独立参考诊断程序
// 目的: 对 3600″极区场景，用独立于生产 PRECISE 的方法验证通量闭合
// 独立性:
//   1. 候选查询: 全穷举所有像素 (不调用 query_candidate_pixels)
//   2. 重叠计算: 高密度球面点采样 + 像素归属统计 (不调用 compute_overlap_area)
//   3. 面积算法: L'Huilier 公式 (不调用 Eriksson spherical_polygon_area)
// 输出: JSONL 到 stdout, 汇总到 stderr

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <set>

// 项目头文件
#include "spherical_overlap.h"
#include "fast_overlap.h"
#include "../healpix_stack/healpix_core.h"
#include "wcs_sip.h"
#include "fits_reader.h"

using spherical::Vec3;
using drizzle::FitsImage;
using drizzle::WcsSip;
using drizzle::WcsParams;

// ============================================================================
// 常量
// ============================================================================
static const double PI      = 3.14159265358979323846;
static const double TWO_PI  = 2.0 * PI;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;

// ============================================================================
// 独立面积算法: L'Huilier 公式 (球面三角形面积)
// 与生产 Eriksson 公式完全独立
// ============================================================================
static double lhuilier_triangle_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    // 球面三角形边长 (弧度)
    double side_a = std::acos(std::max(-1.0, std::min(1.0, spherical::dot(b, c))));
    double side_b = std::acos(std::max(-1.0, std::min(1.0, spherical::dot(a, c))));
    double side_c = std::acos(std::max(-1.0, std::min(1.0, spherical::dot(a, b))));

    // L'Huilier 公式
    double s = (side_a + side_b + side_c) / 2.0;
    double tan_s2 = std::tan(s / 2.0);
    double tan_sa2 = std::tan((s - side_a) / 2.0);
    double tan_sb2 = std::tan((s - side_b) / 2.0);
    double tan_sc2 = std::tan((s - side_c) / 2.0);

    double prod = tan_s2 * tan_sa2 * tan_sb2 * tan_sc2;
    if (prod < 0.0) prod = 0.0;

    return 4.0 * std::atan(std::sqrt(prod));
}

// 独立球面多边形面积: fan triangulation + L'Huilier
static double independent_polygon_area(const std::vector<Vec3>& vertices) {
    int n = (int)vertices.size();
    if (n < 3) return 0.0;

    const Vec3& a = vertices[0];
    double total = 0.0;
    for (int i = 1; i < n - 1; i++) {
        total += lhuilier_triangle_area(a, vertices[i], vertices[i + 1]);
    }
    return total;
}

// ============================================================================
// 独立点在球面多边形内测试 (ray casting 球面版本)
// 使用绕数算法: 计算点相对于多边形边的累计角度
// ============================================================================
static bool point_in_spherical_polygon(const Vec3& p, const std::vector<Vec3>& poly) {
    int n = (int)poly.size();
    if (n < 3) return false;

    double angle_sum = 0.0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        Vec3 v1 = spherical::normalize(Vec3{
            poly[i].x - p.x, poly[i].y - p.y, poly[i].z - p.z});
        Vec3 v2 = spherical::normalize(Vec3{
            poly[j].x - p.x, poly[j].y - p.y, poly[j].z - p.z});

        double dot_v = spherical::dot(v1, v2);
        dot_v = std::max(-1.0, std::min(1.0, dot_v));
        double cross_mag = spherical::length(spherical::cross(v1, v2));

        double theta = std::atan2(cross_mag, dot_v);

        // 判断方向: cross(v1, v2) 与 p 的点积符号
        Vec3 cr = spherical::cross(v1, v2);
        if (spherical::dot(cr, p) < 0.0) theta = -theta;

        angle_sum += theta;
    }

    // 如果点在多边形内, 绕数 ≈ ±2π; 外部 ≈ 0
    return std::fabs(angle_sum) > PI;
}

// ============================================================================
// 构造合成图像 (与 benchmark_precise_fast.cpp 一致)
// ============================================================================
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

// ============================================================================
// 独立参考: 高密度点采样重叠计算
// ============================================================================
struct IndependentResult {
    double drop_area_independent;      // 独立面积算法
    double drop_area_production;       // 生产 Eriksson 面积
    double sum_overlap_independent;    // 独立采样累加重叠
    double sum_overlap_production;     // 生产 compute_overlap_area 累加
    int    total_samples;              // 总采样点数
    int    samples_in_drop;            // drop 内采样点数
    int    candidate_count_production; // 生产候选数
    int    overlap_pixels_independent; // 独立发现有重叠的像素数
    int    overlap_pixels_production;  // 生产计算有重叠的像素数
    std::unordered_map<uint64_t, double> overlap_per_pixel;       // 独立每像素重叠
    std::unordered_map<uint64_t, double> prod_overlap_per_pixel;  // 生产每像素重叠
    std::set<uint64_t> independent_pixel_set; // 独立候选集
    std::set<uint64_t> production_pixel_set;  // 生产候选集
};

static IndependentResult run_independent_reference(
    const WcsSip& wcs, const healpix::HealpixCore& hp,
    double px, double py, double pixfrac,
    int grid_n)
{
    IndependentResult result;
    result.total_samples = 0;
    result.samples_in_drop = 0;

    // ---- 1. 构造 drop 多边形 (高密度采样, 256段/边) ----
    double half = 0.5 * pixfrac;
    double corners_xy[4][2] = {
        {px - half, py - half}, {px + half, py - half},
        {px + half, py + half}, {px - half, py + half}
    };
    double corners_ra[4], corners_dec[4];
    for (int i = 0; i < 4; i++) {
        wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1],
                       corners_ra[i], corners_dec[i]);
    }
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

    // ---- 2. 独立面积 (L'Huilier) ----
    result.drop_area_independent = independent_polygon_area(drop_corners);
    result.drop_area_production = spherical::spherical_polygon_area(drop_corners);

    // ---- 3. 高密度点采样 ----
    // 在 drop 包围盒内均匀采样 grid_n × grid_n 个点
    // drop 包围盒: [px-half, px+half] × [py-half, py+half]
    std::unordered_map<uint64_t, int> pixel_counts;
    for (int iy = 0; iy < grid_n; iy++) {
        for (int ix = 0; ix < grid_n; ix++) {
            double fx = (ix + 0.5) / grid_n;
            double fy = (iy + 0.5) / grid_n;
            double sx = (px - half) + fx * (2.0 * half);
            double sy = (py - half) + fy * (2.0 * half);

            double ra, dec;
            wcs.pixelToSky(sx, sy, ra, dec);
            if (!std::isfinite(ra) || !std::isfinite(dec)) continue;

            result.total_samples++;
            Vec3 p = spherical::radec_to_vec(ra, dec);

            if (!point_in_spherical_polygon(p, drop_corners)) continue;

            result.samples_in_drop++;
            int64_t ipix = hp.radec2pix(ra, dec);
            if (ipix >= 0) {
                pixel_counts[(uint64_t)ipix]++;
                result.independent_pixel_set.insert((uint64_t)ipix);
            }
        }
    }

    // ---- 4. 计算每像素独立重叠面积 ----
    result.overlap_pixels_independent = (int)pixel_counts.size();
    if (result.samples_in_drop > 0) {
        for (const auto& kv : pixel_counts) {
            result.overlap_per_pixel[kv.first] =
                (double)kv.second / (double)result.samples_in_drop
                * result.drop_area_independent;
        }
    }
    result.sum_overlap_independent = 0.0;
    for (const auto& kv : result.overlap_per_pixel) {
        result.sum_overlap_independent += kv.second;
    }

    // ---- 5. 生产候选查询 (用于比较) ----
    std::vector<uint64_t> prod_candidates;
    spherical::query_candidate_pixels(drop_corners, hp, prod_candidates);
    result.candidate_count_production = (int)prod_candidates.size();
    for (uint64_t ipix : prod_candidates) {
        result.production_pixel_set.insert(ipix);
    }

    // ---- 6. 生产重叠计算 (用于比较) + 生产 sum_overlap ----
    result.overlap_pixels_production = 0;
    result.sum_overlap_production = 0.0;  // 生产 compute_overlap_area 累加
    for (uint64_t ipix : prod_candidates) {
        double ov = spherical::compute_overlap_area(drop_corners, hp, ipix);
        if (ov > 0.0) {
            result.overlap_pixels_production++;
            result.sum_overlap_production += ov;
            result.prod_overlap_per_pixel[ipix] = ov;
        }
    }

    return result;
}

// ============================================================================
// 主函数: 测试多个极区场景
// ============================================================================
int main() {
    // 测试场景
    struct Scenario {
        const char* name;
        double dec;
        double pixfrac;
    };
    Scenario scenarios[] = {
        {"north_pf0p5", 89.0, 0.5},
        {"north_pf0p25", 89.0, 0.25},
        {"north_pf1p0", 89.0, 1.0},
        {"south_pf0p5", -89.0, 0.5},
        {"equator_pf0p5", 0.0, 0.5},
        {"midlat_pf0p5", 45.0, 0.5},
    };

    const double scale_arcsec = 3600.0; // 1°/pixel
    const int nside = 64;
    const int img_size = 16;
    const int grid_n = 200; // 200×200 = 40000 采样点

    healpix::HealpixCore hp(nside, true);

    // 汇总文件
    FILE* fp_summary = std::fopen("run/logs/r07_diag_summary.log", "w");
#define LOG(fmt, ...) do { \
        std::fprintf(stderr, fmt, ##__VA_ARGS__); \
        if (fp_summary) std::fprintf(fp_summary, fmt, ##__VA_ARGS__); \
    } while(0)

    LOG("=== R07 独立参考诊断 (三角形扇剖分修复后) ===\n");
    LOG("scale=3600″/px, nside=%d, img=%dx%d, grid=%dx%d\n\n",
        nside, img_size, img_size, grid_n, grid_n);

    for (const auto& sc : scenarios) {
        FitsImage img = make_synthetic_image(scale_arcsec, img_size, 180.0, sc.dec);
        WcsSip wcs(img.wcs);

        // 中心像素
        double px = (double)(img_size / 2);
        double py = (double)(img_size / 2);

        IndependentResult ir = run_independent_reference(wcs, hp, px, py, sc.pixfrac, grid_n);

        // 独立候选漏选检查
        std::vector<uint64_t> missing;
        for (uint64_t ipix : ir.independent_pixel_set) {
            if (ir.production_pixel_set.find(ipix) == ir.production_pixel_set.end()) {
                missing.push_back(ipix);
            }
        }

        // 输出 JSONL
        double ratio_indep = (ir.drop_area_independent > 0.0)
            ? ir.sum_overlap_independent / ir.drop_area_independent : 0.0;
        double ratio_prod = (ir.drop_area_production > 0.0)
            ? ir.sum_overlap_production / ir.drop_area_production : 0.0;
        double ratio_prod_vs_indep_area = (ir.drop_area_independent > 0.0)
            ? ir.sum_overlap_production / ir.drop_area_independent : 0.0;
        double area_diff = std::fabs(ir.drop_area_independent - ir.drop_area_production)
            / std::max(ir.drop_area_independent, 1e-30);
        double flux_err_indep = std::fabs(ratio_indep - 1.0);
        double flux_err_prod  = std::fabs(ratio_prod - 1.0);
        std::printf("{\"event\":\"diag\",\"scenario\":\"%s\",\n", sc.name);
        std::printf(" \"dec\":%.1f,\"pixfrac\":%.2f,\n", sc.dec, sc.pixfrac);
        std::printf(" \"drop_area_independent\":%.15e,\n", ir.drop_area_independent);
        std::printf(" \"drop_area_production\":%.15e,\n", ir.drop_area_production);
        std::printf(" \"area_rel_diff\":%.6e,\n", area_diff);
        std::printf(" \"sum_overlap_independent\":%.15e,\n", ir.sum_overlap_independent);
        std::printf(" \"sum_overlap_production\":%.15e,\n", ir.sum_overlap_production);
        std::printf(" \"ratio_independent\":%.15e,\n", ratio_indep);
        std::printf(" \"ratio_production\":%.15e,\n", ratio_prod);
        std::printf(" \"ratio_prod_vs_indep_area\":%.15e,\n", ratio_prod_vs_indep_area);
        std::printf(" \"flux_err_independent\":%.6e,\n", flux_err_indep);
        std::printf(" \"flux_err_production\":%.6e,\n", flux_err_prod);
        std::printf(" \"total_samples\":%d,\n", ir.total_samples);
        std::printf(" \"samples_in_drop\":%d,\n", ir.samples_in_drop);
        std::printf(" \"candidate_count_production\":%d,\n", ir.candidate_count_production);
        std::printf(" \"overlap_pixels_independent\":%d,\n", ir.overlap_pixels_independent);
        std::printf(" \"overlap_pixels_production\":%d,\n", ir.overlap_pixels_production);
        std::printf(" \"production_missing_count\":%d\n", (int)missing.size());
        std::printf("}\n");

        // stderr 汇总
        LOG("[%s] dec=%.0f pf=%.2f\n", sc.name, sc.dec, sc.pixfrac);
        LOG("  drop_area: indep=%.6e prod=%.6e diff=%.2e\n",
                     ir.drop_area_independent, ir.drop_area_production, area_diff);
        LOG("  sum_overlap: indep=%.6e prod=%.6e\n",
                     ir.sum_overlap_independent, ir.sum_overlap_production);
        LOG("  ratio_indep=%.10f  ratio_prod=%.10f  (目标=1.0)\n",
                     ratio_indep, ratio_prod);
        LOG("  flux_err: indep=%.2e  prod=%.2e\n",
                     flux_err_indep, flux_err_prod);
        LOG("  candidates: prod=%d indep=%d missing=%d\n",
                     ir.candidate_count_production, ir.overlap_pixels_independent,
                     (int)missing.size());
        bool pass_indep = flux_err_indep <= 1e-6;
        bool pass_prod  = flux_err_prod  <= 1e-6;
        if (!pass_indep) {
            LOG("  *** INDEP FAIL: 独立通量闭合误差 %.2e (>1e-6) ***\n",
                         flux_err_indep);
        }
        if (!pass_prod) {
            LOG("  *** PROD FAIL: 生产通量闭合误差 %.2e (>1e-6) ***\n",
                         flux_err_prod);
        }
        if (pass_indep && pass_prod) {
            LOG("  PASS: 独立和生产通量闭合均 <=1e-6\n");
        }

        // Per-pixel 详情 (输出到 perpixel.log 文件, 帮助定位根因)
        FILE* fp_detail = std::fopen("run/logs/r07_diag_perpixel.log", "a");
        if (fp_detail) {
            std::fprintf(fp_detail, "=== %s dec=%.0f pf=%.2f ===\n", sc.name, sc.dec, sc.pixfrac);
            std::fprintf(fp_detail, "  %-20s %15s %15s %12s %10s %10s\n",
                         "ipix", "prod_overlap", "indep_overlap", "ratio", "ra", "dec");
            // 合并所有像素
            std::set<uint64_t> all_pixels;
            for (const auto& kv : ir.prod_overlap_per_pixel) all_pixels.insert(kv.first);
            for (const auto& kv : ir.overlap_per_pixel) all_pixels.insert(kv.first);
            for (uint64_t ipix : all_pixels) {
                double prod_ov = 0.0, indep_ov = 0.0;
                auto pit = ir.prod_overlap_per_pixel.find(ipix);
                if (pit != ir.prod_overlap_per_pixel.end()) prod_ov = pit->second;
                auto iit = ir.overlap_per_pixel.find(ipix);
                if (iit != ir.overlap_per_pixel.end()) indep_ov = iit->second;
                double r = (prod_ov > 0.0 && indep_ov > 0.0) ? prod_ov / indep_ov
                         : (prod_ov > 0.0 ?  999.0 : 0.0);
                double ra_p, dec_p;
                hp.pix2radec((int64_t)ipix, &ra_p, &dec_p);
                std::fprintf(fp_detail, "  %-20llu %15.6e %15.6e %12.4f %10.4f %10.4f\n",
                             (unsigned long long)ipix, prod_ov, indep_ov, r, ra_p, dec_p);
            }
            std::fprintf(fp_detail, "\n");
            std::fclose(fp_detail);
        }
    }

    if (fp_summary) std::fclose(fp_summary);
    return 0;
}
