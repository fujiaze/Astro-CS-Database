// ============================================================================
// R06 诊断: 候选像素零漏选失败根因定位
//
// 测试 1.7 场景: nside=256, drop at (60,30) 1°, rel_err=7.847e-04
// 目标: 找出 sum_overlap < drop_area 的具体丢失位置
// ============================================================================
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cstring>

#include "spherical_overlap.h"
#include "healpix_core.h"

static const double PI      = 3.14159265358979323846;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;

// 构造 1°×1° drop (与 makeRectDrop 一致)
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
    printf("R06 诊断: 候选像素零漏选根因定位\n");
    printf("================================================================\n\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);
    std::vector<spherical::Vec3> drop = makeRectDrop(60.0, 30.0, 1.0);
    double drop_area = spherical::spherical_polygon_area(drop);
    printf("nside=%d, drop at (60,30) size=1°\n", nside);
    printf("drop_area = %.10e steradian\n", drop_area);

    // drop 顶点
    printf("\ndrop 顶点:\n");
    for (size_t i = 0; i < drop.size(); i++) {
        double r, d;
        spherical::vec_to_radec(drop[i], r, d);
        printf("  [%zu] ra=%.6f dec=%.6f\n", i, r, d);
    }

    // 候选查询
    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop, hp, candidates);
    printf("\n候选像素数 = %zu\n", candidates.size());

    // 计算每个候选的 overlap
    double sum_overlap = 0.0;
    int n_overlap = 0;
    int n_zero = 0;
    double max_overlap = 0.0;
    double min_overlap = 1e300;

    struct PixelInfo {
        uint64_t ipix;
        double overlap;
        double ra, dec;
    };
    std::vector<PixelInfo> infos;
    infos.reserve(candidates.size());

    for (uint64_t ipix : candidates) {
        double a = spherical::compute_overlap_area(drop, hp, ipix);
        double ra_p, dec_p;
        hp.pix2radec(ipix, &ra_p, &dec_p);

        infos.push_back({ipix, a, ra_p, dec_p});

        if (a > 1e-15) {
            sum_overlap += a;
            n_overlap++;
            if (a > max_overlap) max_overlap = a;
            if (a < min_overlap) min_overlap = a;
        } else {
            n_zero++;
        }
    }

    printf("n_overlap(>1e-15) = %d\n", n_overlap);
    printf("n_zero = %d\n", n_zero);
    printf("max_overlap = %.6e\n", max_overlap);
    printf("min_overlap = %.6e\n", min_overlap);
    printf("sum_overlap = %.10e\n", sum_overlap);
    printf("rel_err = %.6e\n", std::fabs(sum_overlap - drop_area) / drop_area);

    // 分析: 找出 drop 边界附近的像素, 检查是否有漏选
    // drop 范围: ra [59.423, 60.577], dec [29.5, 30.5]
    printf("\n==== 边界像素分析 ====\n");

    // 检查 drop 4 条边附近的像素
    // 南边 dec=29.5, 北边 dec=30.5, 西边 ra=59.423, 东边 ra=60.577
    int n_south_edge = 0, n_north_edge = 0, n_west_edge = 0, n_east_edge = 0;
    int n_south_zero = 0, n_north_zero = 0, n_west_zero = 0, n_east_zero = 0;
    double south_loss = 0, north_loss = 0, west_loss = 0, east_loss = 0;

    for (const auto& info : infos) {
        // 像素中心到 drop 边界的距离 (度)
        double dr_south = 29.5 - info.dec;  // 正: 在 drop 南边外
        double dr_north = info.dec - 30.5;  // 正: 在 drop 北边外
        double dr_west  = 59.423 - info.ra; // 正: 在 drop 西边外
        double dr_east  = info.ra - 60.577; // 正: 在 drop 东边外

        double hp_size = 0.005; // nside=256 像素约 0.00373°, 取 0.005 作为阈值

        if (std::fabs(dr_south) < hp_size && info.ra > 59.4 && info.ra < 60.6) {
            n_south_edge++;
            if (info.overlap < 1e-15) { n_south_zero++; south_loss += 0; }
        }
        if (std::fabs(dr_north) < hp_size && info.ra > 59.4 && info.ra < 60.6) {
            n_north_edge++;
            if (info.overlap < 1e-15) { n_north_zero++; north_loss += 0; }
        }
        if (std::fabs(dr_west) < hp_size && info.dec > 29.4 && info.dec < 30.6) {
            n_west_edge++;
            if (info.overlap < 1e-15) { n_west_zero++; west_loss += 0; }
        }
        if (std::fabs(dr_east) < hp_size && info.dec > 29.4 && info.dec < 30.6) {
            n_east_edge++;
            if (info.overlap < 1e-15) { n_east_zero++; east_loss += 0; }
        }
    }

    printf("南边(dec=29.5): edge=%d zero=%d\n", n_south_edge, n_south_zero);
    printf("北边(dec=30.5): edge=%d zero=%d\n", n_north_edge, n_north_zero);
    printf("西边(ra=59.423): edge=%d zero=%d\n", n_west_edge, n_west_zero);
    printf("东边(ra=60.577): edge=%d zero=%d\n", n_east_edge, n_east_zero);

    // 检查: 是否有候选像素的 overlap_area 异常小 (但非零)
    printf("\n==== 微小 overlap 像素分析 ====\n");
    int n_small = 0;
    double small_sum = 0.0;
    for (const auto& info : infos) {
        if (info.overlap > 1e-15 && info.overlap < 1e-12) {
            n_small++;
            small_sum += info.overlap;
        }
    }
    printf("微小 overlap (1e-15 < a < 1e-12) 像素数 = %d, sum = %.6e\n", n_small, small_sum);

    // 关键诊断: 检查 drop 内部是否有"空洞" (overlap=0 但像素中心在 drop 内)
    printf("\n==== drop 内部空洞检查 ====\n");
    int n_inside_zero = 0;
    for (const auto& info : infos) {
        if (info.overlap < 1e-15) {
            // 像素中心是否在 drop 内部?
            if (info.ra > 59.423 && info.ra < 60.577 &&
                info.dec > 29.5 && info.dec < 30.5) {
                n_inside_zero++;
                if (n_inside_zero <= 20) {
                    printf("  内部空洞: ipix=%llu ra=%.6f dec=%.6f overlap=%.3e\n",
                           (unsigned long long)info.ipix, info.ra, info.dec, info.overlap);
                }
            }
        }
    }
    printf("drop 内部 overlap=0 的像素数 = %d\n", n_inside_zero);

    // 关键诊断: 检查 drop 边界外是否有 overlap>0 的像素 (正常, 因为像素部分在 drop 内)
    printf("\n==== 边界外 overlap>0 像素分析 ====\n");
    int n_outside_pos = 0;
    double outside_sum = 0.0;
    for (const auto& info : infos) {
        if (info.overlap > 1e-15) {
            bool outside = (info.ra < 59.423 || info.ra > 60.577 ||
                           info.dec < 29.5 || info.dec > 30.5);
            if (outside) {
                n_outside_pos++;
                outside_sum += info.overlap;
            }
        }
    }
    printf("drop 中心外 overlap>0 的像素数 = %d, sum = %.6e\n", n_outside_pos, outside_sum);

    // 最终: 理论 drop_area vs sum_overlap
    printf("\n==== 最终对比 ====\n");
    printf("drop_area     = %.10e\n", drop_area);
    printf("sum_overlap   = %.10e\n", sum_overlap);
    printf("差值          = %.6e steradian\n", drop_area - sum_overlap);
    printf("差值 (平方度) = %.6e\n", (drop_area - sum_overlap) * RAD2DEG * RAD2DEG);
    printf("差值 (像素数) = %.2f 个\n",
           (drop_area - sum_overlap) / (4.0 * PI / (12.0 * nside * nside)));
    printf("rel_err       = %.6e\n", std::fabs(sum_overlap - drop_area) / drop_area);

    return 0;
}
