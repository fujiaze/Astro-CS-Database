// ============================================================================
// R06 诊断: 28 个 overlap 像素的详细分析
//
// 定位 sum_overlap < drop_area 的 0.015 像素差值来源
// ============================================================================
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

#include "spherical_overlap.h"
#include "healpix_core.h"

static const double PI      = 3.14159265358979323846;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;

static std::vector<spherical::Vec3> makeRectDrop(
    double ra_center, double dec_center, double size_deg)
{
    double half = size_deg * 0.5;
    double cd = std::cos(dec_center * DEG2RAD);
    double half_ra = (std::fabs(cd) > 1e-10) ? (half / cd) : half;
    std::vector<spherical::Vec3> corners(4);
    corners[0] = spherical::radec_to_vec(ra_center - half_ra, dec_center - half);
    corners[1] = spherical::radec_to_vec(ra_center + half_ra, dec_center - half);
    corners[2] = spherical::radec_to_vec(ra_center + half_ra, dec_center + half);
    corners[3] = spherical::radec_to_vec(ra_center - half_ra, dec_center + half);
    return corners;
}

int main() {
    printf("================================================================\n");
    printf("R06 诊断: 28 个 overlap 像素详细分析\n");
    printf("================================================================\n\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);
    std::vector<spherical::Vec3> drop = makeRectDrop(60.0, 30.0, 1.0);
    double drop_area = spherical::spherical_polygon_area(drop);
    double theory_pixel_area = 4.0 * PI / (12.0 * nside * nside);

    printf("drop_area = %.10e, theory_pixel_area = %.10e\n", drop_area, theory_pixel_area);
    printf("drop 覆盖约 %.1f 个像素\n\n", drop_area / theory_pixel_area);

    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop, hp, candidates);

    // 逐个分析有 overlap 的像素
    struct PixelInfo {
        uint64_t ipix;
        double overlap;
        double ra, dec;
        bool fully_inside;  // 像素中心在 drop 内
        bool is_boundary;   // overlap < 0.9 * theory_pixel_area
    };
    std::vector<PixelInfo> infos;

    double sum_overlap = 0.0;
    int n_fully_inside = 0, n_boundary = 0;
    double sum_fully = 0.0, sum_boundary = 0.0;

    for (uint64_t ipix : candidates) {
        double a = spherical::compute_overlap_area(drop, hp, ipix);
        if (a < 1e-15) continue;

        double ra_p, dec_p;
        hp.pix2radec(ipix, &ra_p, &dec_p);
        bool inside = (ra_p > 59.423 && ra_p < 60.577 && dec_p > 29.5 && dec_p < 30.5);
        bool boundary = (a < 0.9 * theory_pixel_area);

        infos.push_back({ipix, a, ra_p, dec_p, inside, boundary});
        sum_overlap += a;

        if (inside && !boundary) {
            n_fully_inside++;
            sum_fully += a;
        } else {
            n_boundary++;
            sum_boundary += a;
        }
    }

    printf("==== 分类统计 ====\n");
    printf("完全包含像素: %d 个, sum=%.10e, 理论sum=%.10e, 差=%.6e\n",
           n_fully_inside, sum_fully, n_fully_inside * theory_pixel_area,
           sum_fully - n_fully_inside * theory_pixel_area);
    printf("边界像素: %d 个, sum=%.10e\n", n_boundary, sum_boundary);
    printf("sum_overlap = %.10e, drop_area = %.10e, 差值 = %.6e\n\n",
           sum_overlap, drop_area, drop_area - sum_overlap);

    // 打印所有完全包含像素的 overlap vs 理论面积
    printf("==== 完全包含像素 overlap vs 理论面积 ====\n");
    double fully_err_max = 0;
    for (const auto& info : infos) {
        if (info.fully_inside && !info.is_boundary) {
            double err = info.overlap - theory_pixel_area;
            double rel = std::fabs(err) / theory_pixel_area;
            if (rel > fully_err_max) fully_err_max = rel;
            if (rel > 1e-8) {
                printf("  ipix=%llu ra=%.6f dec=%.6f overlap=%.10e err=%.6e rel=%.3e\n",
                       (unsigned long long)info.ipix, info.ra, info.dec,
                       info.overlap, err, rel);
            }
        }
    }
    printf("完全包含像素最大相对误差: %.3e\n\n", fully_err_max);

    // 打印所有边界像素
    printf("==== 边界像素 (按 overlap 排序) ====\n");
    std::sort(infos.begin(), infos.end(), [](const PixelInfo& a, const PixelInfo& b) {
        return a.overlap < b.overlap;
    });
    for (const auto& info : infos) {
        if (info.is_boundary) {
            printf("  ipix=%llu ra=%.6f dec=%.6f overlap=%.6e (%.1f%% of pixel) %s\n",
                   (unsigned long long)info.ipix, info.ra, info.dec,
                   info.overlap, 100.0 * info.overlap / theory_pixel_area,
                   info.fully_inside ? "inside" : "OUTSIDE");
        }
    }

    // 关键: 检查 drop 边界上的像素
    printf("\n==== drop 边界像素分析 ====\n");
    // drop 的 4 条边:
    // 南: dec=29.5, 北: dec=30.5 (小圆弧, 大圆弧近似向北弯曲)
    // 西: ra=59.423, 东: ra=60.577 (大圆弧, 精确)
    printf("南边 (dec≈29.5): 大圆弧向北弯曲, 实际覆盖 dec>29.5 区域\n");
    printf("北边 (dec≈30.5): 大圆弧向北弯曲, 实际覆盖 dec>30.5 区域\n");
    printf("  → 大圆弧 drop 比等dec矩形面积更大\n");
    printf("  → 但 drop_area 也用大圆弧计算, sum 应 = drop_area\n\n");

    // 检查: 是否有像素中心在 drop 外但 overlap > 0 (大圆弧弯曲导致)
    printf("==== drop 中心外但 overlap>0 的像素 ====\n");
    for (const auto& info : infos) {
        if (!info.fully_inside) {
            printf("  ipix=%llu ra=%.6f dec=%.6f overlap=%.6e\n",
                   (unsigned long long)info.ipix, info.ra, info.dec, info.overlap);
        }
    }

    // 最终: 理论分析
    printf("\n==== 理论分析 ====\n");
    printf("sum_overlap = %.10e\n", sum_overlap);
    printf("drop_area   = %.10e\n", drop_area);
    printf("差值        = %.6e sr = %.6e sq.deg = %.4f 像素\n",
           drop_area - sum_overlap,
           (drop_area - sum_overlap) * RAD2DEG * RAD2DEG,
           (drop_area - sum_overlap) / theory_pixel_area);
    printf("rel_err     = %.6e\n", std::fabs(sum_overlap - drop_area) / drop_area);

    // 检查: 如果用 4 顶点 HEALPix 边界 (不做自适应细分), 结果如何?
    printf("\n==== 对比: 4 顶点边界 vs 自适应细分 ====\n");
    // spherical_overlap 内部用自适应细分, 这里无法直接切换
    // 但可以检查: 自适应细分是否引入了额外误差
    printf("(自适应细分在 nside=256 时产生 8 顶点, 每条边 2 段)\n");
    printf("(8 个裁剪平面 vs 4 个裁剪平面, S-H 累积误差更大)\n");

    return 0;
}
