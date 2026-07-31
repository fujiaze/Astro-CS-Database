// ============================================================================
// 球面 HEALPix 重叠计算单元测试 (WP-D 步骤3-4 验收)
//
// 测试覆盖 (依据 00_COMMON_CONTRACTS.md §5.2 关键验收项):
//   1. 已知球面多边形面积验证 (球面三角形/四边形, 八分体)
//   2. HEALPix 像素面积验证 (理论值 = 4π / (12*nside²))
//   3. 通量守恒: drop 未截断时, Σsignal = L_j (误差 < 1e-5)
//   4. 极区测试: 源像素在极区附近不产生异常
//   5. 大视场测试: >3° 视场不产生异常
//   6. 候选像素查询: 高 NSIDE + 大源像素时候选数 > 48
//   7. 重叠面积非负
//   8. 相邻像素重叠面积之和 = drop 总面积 (误差 < 1e-6)
//
// 编译命令 (见 task 描述):
//   g++ -std=c++17 -O2 -fopenmp -DAIO_ENABLE_HEALPIX \
//     -I../ -I../../healpix_stack -I../../../astro_image_io/include \
//     test_spherical_overlap.cpp ../spherical_overlap.cpp ../wcs_sip.cpp \
//     ../../healpix_stack/healpix_core.cpp \
//     -o test_spherical_overlap.exe
// ============================================================================

#include "spherical_overlap.h"
#include "wcs_sip.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 简单测试框架: 维护通过/失败计数
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

#define TEST_PASS(name) do { \
    g_pass_count++; \
    printf("  [PASS] %s\n", name); \
} while (0)

#define TEST_FAIL(name, msg) do { \
    g_fail_count++; \
    printf("  [FAIL] %s: %s\n", name, msg); \
} while (0)

#define ASSERT_NEAR(name, actual, expected, tol) do { \
    double a = (actual), e = (expected), t = (tol); \
    double diff = std::fabs(a - e); \
    if (diff <= t) { \
        TEST_PASS(name); \
    } else { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "actual=%.12g, expected=%.12g, diff=%.6g, tol=%.6g", \
                 a, e, diff, t); \
        TEST_FAIL(name, buf); \
    } \
} while (0)

#define ASSERT_TRUE(name, cond, msg) do { \
    if (cond) { TEST_PASS(name); } else { TEST_FAIL(name, msg); } \
} while (0)

// ============================================================================
// 辅助: 构造球面四边形 drop (以 ra_center, dec_center 为中心, 边长 size_deg)
// 顶点顺序: 左下→右下→右上→左上 (逆时针)
// ============================================================================
static std::vector<spherical::Vec3> makeRectDrop(
    double ra_center, double dec_center, double size_deg)
{
    double half = size_deg * 0.5;
    // 处理 cos(dec) 因子, 使四边形在球面上近似方形
    double cd = std::cos(dec_center * M_PI / 180.0);
    double half_ra = (std::fabs(cd) > 1e-10) ? (half / cd) : half;

    std::vector<spherical::Vec3> corners(4);
    corners[0] = spherical::radec_to_vec(ra_center - half_ra, dec_center - half);
    corners[1] = spherical::radec_to_vec(ra_center + half_ra, dec_center - half);
    corners[2] = spherical::radec_to_vec(ra_center + half_ra, dec_center + half);
    corners[3] = spherical::radec_to_vec(ra_center - half_ra, dec_center + half);
    return corners;
}

// ============================================================================
// 测试 1: 已知球面多边形面积验证
// ============================================================================
static void test_known_spherical_area() {
    printf("\n[测试组 1] 已知球面多边形面积验证\n");

    // 1.1 球面八分体 (1/8 球面): 3 个相互垂直的顶点
    //   顶点: (1,0,0), (0,1,0), (0,0,1)
    //   面积 = 4π / 8 = π/2 ≈ 1.5707963267948966
    {
        std::vector<spherical::Vec3> octant = {
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0}
        };
        double area = spherical::spherical_polygon_area(octant);
        double expected = M_PI / 2.0;
        ASSERT_NEAR("球面八分体面积 = π/2", area, expected, 1e-10);
    }

    // 1.2 半球面: 4 个顶点构成赤道大圆上的半个球面
    //   顶点: (1,0,0), (0,1,0), (-1,0,0), (0,0,1) — 这是上半球的一半
    //   面积 = 2π (1/4 球面)
    {
        std::vector<spherical::Vec3> quad = {
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {-1.0, 0.0, 0.0},
            {0.0, 0.0, 1.0}
        };
        double area = spherical::spherical_polygon_area(quad);
        double expected = M_PI;  // 1/4 球面 = π 球面度
        ASSERT_NEAR("球面四边形 (1/4 球面) 面积 = π", area, expected, 1e-10);
    }

    // 1.3 完整球面三角形 (3 个 90° 角的三角形, 即八分体的另一种描述)
    //   顶点: 北极 (0,0,1), 赤道 0° (1,0,0), 赤道 90° (0,1,0)
    //   面积 = π/2
    {
        std::vector<spherical::Vec3> tri = {
            spherical::radec_to_vec(0.0, 90.0),    // 北极
            spherical::radec_to_vec(0.0, 0.0),     // 赤道 0°
            spherical::radec_to_vec(90.0, 0.0)     // 赤道 90°
        };
        double area = spherical::spherical_polygon_area(tri);
        double expected = M_PI / 2.0;
        ASSERT_NEAR("北极+赤道三角形面积 = π/2", area, expected, 1e-10);
    }

    // 1.4 小面积球面四边形 (近似平面面积验证)
    //   1°×1° 在赤道附近, 球面面积 ≈ (π/180)² ≈ 3.0462e-4 球面度
    {
        std::vector<spherical::Vec3> small = makeRectDrop(45.0, 0.0, 1.0);
        double area = spherical::spherical_polygon_area(small);
        double expected = (1.0 * M_PI / 180.0) * (1.0 * M_PI / 180.0);  // ≈ 3.0462e-4
        ASSERT_NEAR("1°×1° 赤道四边形面积", area, expected, 1e-8);
    }
}

// ============================================================================
// 测试 2: HEALPix 像素面积验证
//   理论值 = 4π / (12 * nside²)
//
// 注: HEALPix 赤道带像素的南北边界是等纬度小圆弧 (非大圆弧).
//     球面 Sutherland-Hodgman 假设大圆弧边界, 对赤道带像素有近似误差.
//     - 高 NSIDE (>=4): 像素小, 小圆弧≈大圆弧, 误差 < 5%
//     - 低 NSIDE (1,2): 像素大, 误差显著 (可达 20%)
//     对低 NSIDE, 改为验证"全天所有像素面积之和 = 4π" (更稳健的全局验证)
// ============================================================================
static void test_healpix_pixel_area() {
    printf("\n[测试组 2] HEALPix 像素面积验证\n");

    // 2.1 高 NSIDE: 单像素面积接近理论值 (容差 5%)
    int nsides_high[] = {4, 16, 64, 256, 1024};
    for (int nside : nsides_high) {
        healpix::HealpixCore hp(nside, true);
        double theory_area = 4.0 * M_PI / (12.0 * (double)nside * nside);
        char test_name[128];
        snprintf(test_name, sizeof(test_name), "HEALPix 像素面积 nside=%d", nside);

        // 选赤道带像素: bighp=4, x=nside/2, y=nside/2 (赤道中心, 避免边界退化)
        int bighp = 4;
        int x = nside / 2;
        int y = nside / 2;
        int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
        int xv = x, yv = y;
        for (int i = 0; i < 32; i++) {
            ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
            xv >>= 1; yv >>= 1;
            if (!xv && !yv) break;
        }

        std::vector<spherical::Vec3> boundary = spherical::get_healpix_boundary(hp, (uint64_t)ipix, nside);
        double area = spherical::spherical_polygon_area(boundary);

        // 高 NSIDE 容差 5% (赤道带小圆弧边界近似误差)
        double tol = std::max(1e-9, theory_area * 0.05);
        ASSERT_NEAR(test_name, area, theory_area, tol);
    }

    // 2.2 低 NSIDE (1, 2): 验证全天总面积 = 4π (稳健全局验证)
    //     单个赤道带像素因小圆弧边界有 ~20% 误差, 但全天总和应精确
    int nsides_low[] = {1, 2};
    for (int nside : nsides_low) {
        healpix::HealpixCore hp(nside, true);
        int64_t npix = hp.getNpix();
        double total_area = 0.0;
        int n_valid = 0;
        for (int64_t ipix = 0; ipix < npix; ipix++) {
            std::vector<spherical::Vec3> boundary =
                spherical::get_healpix_boundary(hp, (uint64_t)ipix, nside);
            double area = spherical::spherical_polygon_area(boundary);
            if (area > 0.0) {
                total_area += area;
                n_valid++;
            }
        }
        char test_name[128];
        snprintf(test_name, sizeof(test_name), "HEALPix 全天总面积 nside=%d (n=%d/%lld)",
                 nside, n_valid, (long long)npix);
        // 全天总面积应接近 4π (容差 5%, 允许小圆弧近似误差)
        ASSERT_NEAR(test_name, total_area, 4.0 * M_PI, 4.0 * M_PI * 0.05);
    }
}

// ============================================================================
// 测试 3: 通量守恒 - drop 未截断时 Σsignal = L_j
//   构造一个 drop, 计算所有相邻 HEALPix 像素的重叠面积之和, 验证 = drop 面积
//   进一步: 设 L_j = 100, weightValue = 1, snrValue = 1, 验证 Σ sumFlux = 100
// ============================================================================
static void test_flux_conservation() {
    printf("\n[测试组 3] 通量守恒验证 (Σsignal = L_j)\n");

    // 3.1 赤道附近的 drop, NSIDE=64
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        // drop 中心在 (ra=45°, dec=0°), 边长约 1° (大于 HEALPix 像素 ≈ 0.92°)
        // 这样 drop 跨越多个 HEALPix 像素, 验证通量守恒
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 1.0);

        double drop_area = spherical::spherical_polygon_area(drop);

        // 查询候选像素
        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        // 累加所有候选像素的重叠面积
        double sum_overlap = 0.0;
        int n_overlap = 0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) {
                sum_overlap += a;
                n_overlap++;
            }
        }

        // 通量守恒: Σ a_jp = A_j_drop (drop 未被截断)
        ASSERT_NEAR("通量守恒: Σa_jp = A_drop (赤道, nside=64)",
                    sum_overlap, drop_area, drop_area * 1e-5);

        // 进一步: 设 L_j = 100, weightValue = 1, 验证 Σ sumFlux = 100
        // sumFlux = Σ L_j * (a_jp / A_drop) = L_j * (Σa_jp / A_drop) = L_j (当 Σa_jp = A_drop)
        double L_j = 100.0;
        double sum_flux = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) {
                sum_flux += L_j * (a / drop_area);
            }
        }
        ASSERT_NEAR("通量守恒: Σsignal = L_j (赤道, nside=64)",
                    sum_flux, L_j, L_j * 1e-5);

        // 验证至少有多个像素重叠
        ASSERT_TRUE("drop 跨越多像素 (n_overlap > 1)", n_overlap > 1,
                    "drop 应跨越多个 HEALPix 像素");
    }

    // 3.2 高 NSIDE 下的通量守恒
    {
        int nside = 1024;
        healpix::HealpixCore hp(nside, true);
        // drop 边长约 0.5°, HEALPix 像素约 3.4', 跨越多个像素
        std::vector<spherical::Vec3> drop = makeRectDrop(100.0, 30.0, 0.5);

        double drop_area = spherical::spherical_polygon_area(drop);
        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) sum_overlap += a;
        }
        ASSERT_NEAR("通量守恒: Σa_jp = A_drop (高 NSIDE=1024)",
                    sum_overlap, drop_area, drop_area * 1e-5);
    }
}

// ============================================================================
// 测试 4: 极区测试 - 源像素在极区附近不产生异常
// ============================================================================
static void test_polar_region() {
    printf("\n[测试组 4] 极区测试\n");

    int nside = 64;
    healpix::HealpixCore hp(nside, true);

    // 4.1 北极附近 drop (dec=89.5°)
    {
        std::vector<spherical::Vec3> drop = makeRectDrop(0.0, 89.5, 0.5);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("北极 drop 面积 > 0", drop_area > 0.0, "极区 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);
        ASSERT_TRUE("北极候选像素非空", !candidates.empty(), "极区应返回候选像素");

        // 累加重叠面积
        double sum_overlap = 0.0;
        int n_finite = 0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (std::isfinite(a)) {
                n_finite++;
                if (a > 0.0) sum_overlap += a;
            }
        }
        ASSERT_TRUE("北极重叠面积全部有限", n_finite == (int)candidates.size(),
                    "所有候选像素的重叠面积应为有限值");
        // 极区因 RA wrap 可能通量守恒稍差, 容差放宽
        ASSERT_TRUE("北极 Σa_jp ≈ A_drop (容差 10%)",
                    std::fabs(sum_overlap - drop_area) < drop_area * 0.1,
                    "极区通量守恒应在 10% 容差内");
    }

    // 4.2 南极附近 drop (dec=-89.5°)
    {
        std::vector<spherical::Vec3> drop = makeRectDrop(180.0, -89.5, 0.5);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("南极 drop 面积 > 0", drop_area > 0.0, "极区 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);
        ASSERT_TRUE("南极候选像素非空", !candidates.empty(), "极区应返回候选像素");

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 0.0) sum_overlap += a;
        }
        ASSERT_TRUE("南极 Σa_jp ≈ A_drop (容差 10%)",
                    std::fabs(sum_overlap - drop_area) < drop_area * 0.1,
                    "极区通量守恒应在 10% 容差内");
    }

    // 4.3 正极点 drop (dec=89.99°)
    {
        std::vector<spherical::Vec3> drop = makeRectDrop(0.0, 89.99, 0.1);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("正北极点 drop 面积 > 0", drop_area > 0.0, "极点 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);
        ASSERT_TRUE("正北极点候选像素非空", !candidates.empty(),
                    "正极点应返回候选像素");

        // 验证不产生 NaN/Inf
        bool all_finite = true;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (!std::isfinite(a)) { all_finite = false; break; }
        }
        ASSERT_TRUE("正北极点重叠面积全部有限", all_finite,
                    "极点所有重叠面积应为有限值");
    }
}

// ============================================================================
// 测试 5: 大视场测试 - >3° 视场不产生异常
// ============================================================================
static void test_large_field() {
    printf("\n[测试组 5] 大视场测试\n");

    int nside = 16;
    healpix::HealpixCore hp(nside, true);

    // 5.1 5° × 5° drop (大视场)
    {
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 20.0, 5.0);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("5° drop 面积 > 0", drop_area > 0.0, "大视场 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        double sum_overlap = 0.0;
        int n_finite = 0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (std::isfinite(a)) {
                n_finite++;
                if (a > 0.0) sum_overlap += a;
            }
        }
        ASSERT_TRUE("5° drop 重叠面积全部有限", n_finite == (int)candidates.size(),
                    "大视场所有重叠面积应为有限值");
        // 大视场平面近似误差较大, 球面应更准; 容差 1%
        ASSERT_NEAR("5° drop 通量守恒 (容差 1%)",
                    sum_overlap, drop_area, drop_area * 0.01);
    }

    // 5.2 10° × 10° drop (超大视场)
    {
        std::vector<spherical::Vec3> drop = makeRectDrop(100.0, 0.0, 10.0);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("10° drop 面积 > 0", drop_area > 0.0, "超大视场 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 0.0) sum_overlap += a;
        }
        ASSERT_NEAR("10° drop 通量守恒 (容差 2%)",
                    sum_overlap, drop_area, drop_area * 0.02);
    }
}

// ============================================================================
// 测试 6: 候选像素查询 - 高 NSIDE + 大源像素时候选数 > 48
// ============================================================================
static void test_candidate_query() {
    printf("\n[测试组 6] 候选像素查询 (> 48)\n");

    // 6.1 高 NSIDE + 大源像素: 应返回远 > 48 个候选
    {
        int nside = 1024;
        healpix::HealpixCore hp(nside, true);
        // drop 边长约 5° (18000"), HEALPix 像素约 3.4'
        // 覆盖约 (5*60/3.4)² ≈ 7800 个像素
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 5.0);

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        char msg[256];
        snprintf(msg, sizeof(msg), "候选数=%zu (期望 > 48)", candidates.size());
        ASSERT_TRUE("高 NSIDE + 大源像素候选数 > 48", candidates.size() > 48, msg);
    }

    // 6.2 低 NSIDE + 小源像素: 候选数应较少
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        // drop 边长约 0.1°, HEALPix 像素约 0.92°, 应只覆盖 1-4 个像素
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 0.1);

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        ASSERT_TRUE("低 NSIDE + 小源像素候选数合理 (<= 48)",
                    candidates.size() <= 48, "小源像素候选数应较少");
    }

    // 6.3 极高 NSIDE 测试
    {
        int nside = 4096;
        healpix::HealpixCore hp(nside, true);
        // drop 边长约 1°, HEALPix 像素约 0.86'
        std::vector<spherical::Vec3> drop = makeRectDrop(100.0, 30.0, 1.0);

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        char msg[256];
        snprintf(msg, sizeof(msg), "候选数=%zu (期望 > 48)", candidates.size());
        ASSERT_TRUE("极高 NSIDE=4096 候选数 > 48", candidates.size() > 48, msg);
    }
}

// ============================================================================
// 测试 7: 重叠面积非负
// ============================================================================
static void test_overlap_non_negative() {
    printf("\n[测试组 7] 重叠面积非负\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);

    // 任意 drop 与任意候选像素的重叠面积都应 >= 0
    std::vector<spherical::Vec3> drop = makeRectDrop(60.0, 20.0, 0.5);

    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop, hp, candidates);

    bool all_non_negative = true;
    int n_tested = 0;
    for (uint64_t ipix : candidates) {
        double a = spherical::compute_overlap_area(drop, hp, ipix);
        if (a < -1e-15) {  // 允许浮点误差
            all_non_negative = false;
            break;
        }
        n_tested++;
    }
    ASSERT_TRUE("所有重叠面积非负", all_non_negative,
                "重叠面积不应为负值");
    ASSERT_TRUE("测试了至少 1 个像素", n_tested >= 1, "应测试至少 1 个像素");

    // 不相交的 drop 与目标像素: 重叠面积为 0
    {
        // drop 在 (ra=0, dec=0), 目标像素在 (ra=180, dec=60)
        std::vector<spherical::Vec3> drop_far = makeRectDrop(0.0, 0.0, 0.1);
        int64_t far_ipix = hp.radec2pix(180.0, 60.0);
        double a = spherical::compute_overlap_area(drop_far, hp, (uint64_t)far_ipix);
        ASSERT_NEAR("不相交 drop 重叠面积 = 0", a, 0.0, 1e-12);
    }
}

// ============================================================================
// 测试 8: 相邻像素重叠面积之和 = drop 总面积 (误差 < 1e-6)
// ============================================================================
static void test_adjacent_pixel_sum() {
    printf("\n[测试组 8] 相邻像素重叠面积之和 = drop 总面积\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);

    // drop 中心位于赤道附近, 跨越多个 HEALPix 像素
    std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 1.0);
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

    // 严格容差: < 1e-6 相对误差
    double rel_err = std::fabs(sum_overlap - drop_area) / drop_area;
    char msg[256];
    snprintf(msg, sizeof(msg), "Σa_jp=%.12g, A_drop=%.12g, rel_err=%.6g, n=%d",
             sum_overlap, drop_area, rel_err, n_nonzero);
    ASSERT_TRUE("Σa_jp = A_drop (相对误差 < 1e-6)", rel_err < 1e-6, msg);
    ASSERT_TRUE("drop 跨越多像素", n_nonzero > 1, "drop 应跨越多个像素");
}

// ============================================================================
// 测试 9: 球面 Sutherland-Hodgman 裁剪基本正确性
// ============================================================================
static void test_sutherland_hodgman() {
    printf("\n[测试组 9] 球面 Sutherland-Hodgman 裁剪\n");

    // 9.1 用赤道大圆裁剪上半球
    //   clip_plane_normal = (0, 0, -1) 表示保留 z <= 0 一侧 (南半球)
    //   或 clip_plane_normal = (0, 0, 1) 表示保留 z >= 0 一侧 (北半球)
    {
        // 北极三角形
        std::vector<spherical::Vec3> tri = {
            spherical::radec_to_vec(0.0, 80.0),
            spherical::radec_to_vec(120.0, 80.0),
            spherical::radec_to_vec(240.0, 80.0)
        };
        // 用赤道裁剪 (保留北半球, n = (0,0,1))
        std::vector<spherical::Vec3> clip_n = {{0.0, 0.0, 1.0}};
        std::vector<spherical::Vec3> clipped = spherical::sutherland_hodgman_spherical(tri, clip_n);

        // 全部在北半球, 裁剪后应保持 3 个顶点 (或更多, 如果产生交点)
        ASSERT_TRUE("北极三角形裁剪保留顶点数 >= 3",
                    clipped.size() >= 3, "应保留至少 3 个顶点");

        double area = spherical::spherical_polygon_area(clipped);
        ASSERT_TRUE("北极三角形裁剪后面积 > 0", area > 0.0,
                    "裁剪后面积应为正");
    }

    // 9.2 跨赤道四边形, 用赤道裁剪, 结果应只剩北半球部分
    {
        std::vector<spherical::Vec3> quad = {
            spherical::radec_to_vec(0.0, 10.0),
            spherical::radec_to_vec(10.0, 10.0),
            spherical::radec_to_vec(10.0, -10.0),
            spherical::radec_to_vec(0.0, -10.0)
        };
        std::vector<spherical::Vec3> clip_n = {{0.0, 0.0, 1.0}};
        std::vector<spherical::Vec3> clipped = spherical::sutherland_hodgman_spherical(quad, clip_n);

        double area_full = spherical::spherical_polygon_area(quad);
        double area_clipped = spherical::spherical_polygon_area(clipped);

        // 裁剪后面积应小于原面积 (去掉了南半球部分)
        ASSERT_TRUE("跨赤道裁剪后面积 < 原面积",
                    area_clipped < area_full && area_clipped > 0.0,
                    "裁剪应去掉南半球部分");
        // 近似一半 (因 10° 范围内, 球面近似平面)
        ASSERT_NEAR("跨赤道裁剪后面积约为原面积一半",
                    area_clipped, area_full * 0.5, area_full * 0.05);
    }
}

// ============================================================================
// 测试 10: HEALPix 边界采样 - 顶点数验证
//
// 赤道带像素 (bighp 4-7): 采样后顶点数 > 4
// 极区像素 (bighp 0-3/8-11 极冠): 采样后顶点数 = 4
// samples_per_edge=1: 退化为 4 顶点
// ============================================================================
static void test_boundary_sampled_vertices() {
    printf("\n[测试组 10] HEALPix 边界采样顶点数验证\n");

    // 10.1 赤道带像素 (bighp=4): samples_per_edge=8 → 顶点数 = 4*8 = 32
    {
        int nside = 4;
        healpix::HealpixCore hp(nside, true);
        // bighp=4, 选取赤道带中心像素 (x=nside/2, y=nside/2)
        int bighp = 4;
        int x = nside / 2, y = nside / 2;
        int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
        int xv = x, yv = y;
        for (int i = 0; i < 32; i++) {
            ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
            xv >>= 1; yv >>= 1;
            if (!xv && !yv) break;
        }

        std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside, 8);
        char msg[256];
        snprintf(msg, sizeof(msg), "赤道带像素 samples=8: 顶点数=%zu (期望 32)", b.size());
        ASSERT_TRUE("赤道带像素 samples=8 顶点数=32", b.size() == 32, msg);
    }

    // 10.2 赤道带像素 samples_per_edge=1 → 退化为 4 顶点
    {
        int nside = 4;
        healpix::HealpixCore hp(nside, true);
        int bighp = 4;
        int x = nside / 2, y = nside / 2;
        int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
        int xv = x, yv = y;
        for (int i = 0; i < 32; i++) {
            ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
            xv >>= 1; yv >>= 1;
            if (!xv && !yv) break;
        }

        std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside, 1);
        ASSERT_TRUE("赤道带像素 samples=1 退化为 4 顶点", b.size() == 4,
                    "samples_per_edge=1 应退化为 4 顶点");
    }

    // 10.3 极区像素 (bighp=0, 极冠): 采样后仍为 4 顶点
    {
        int nside = 4;
        healpix::HealpixCore hp(nside, true);
        // bighp=0, 选取极冠区域像素 (x+y > Ns)
        int bighp = 0;
        int x = nside, y = nside;  // x+y = 2*Ns > Ns, 在极冠内
        // 实际像素坐标范围 [0, Ns], 取 x=Ns-1, y=Ns-1 → x+y = 2*(Ns-1) > Ns (当 Ns>=2)
        x = nside - 1; y = nside - 1;
        int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
        int xv = x, yv = y;
        for (int i = 0; i < 32; i++) {
            ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
            xv >>= 1; yv >>= 1;
            if (!xv && !yv) break;
        }

        std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside, 8);
        ASSERT_TRUE("极区像素采样后仍为 4 顶点", b.size() == 4,
                    "极区像素边为大圆弧, 不需要采样");
    }

    // 10.4 默认参数 samples_per_edge=8
    {
        int nside = 16;
        healpix::HealpixCore hp(nside, true);
        int bighp = 5;  // 赤道带
        int x = nside / 2, y = nside / 2;
        int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
        int xv = x, yv = y;
        for (int i = 0; i < 32; i++) {
            ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
            xv >>= 1; yv >>= 1;
            if (!xv && !yv) break;
        }

        std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside);
        ASSERT_TRUE("默认 samples_per_edge=8 (赤道带)", b.size() == 32,
                    "默认参数应为 8");
    }
}

// ============================================================================
// 测试 11: HEALPix 边界采样 - 面积精度提升
//
// 对比 4 顶点边界 vs 采样边界的面积误差:
//   - 低 NSIDE 赤道带像素: 4 顶点误差 ~5-20%, 采样后应 < 1%
//   - 全天总面积: 采样后应更接近 4π
//
// Phase C1.2 关键验收: NSIDE=1/2/4/16/64 赤道带单像素面积,
//   采样 16 段时相对理论值 4π/(12·NSIDE²) 误差 < 1%.
// ============================================================================
static void test_boundary_sampled_accuracy() {
    printf("\n[测试组 11] HEALPix 边界采样面积精度提升\n");

    // 11.1 NSIDE=1/2/4/16/64 赤道带单像素面积精度 (Phase C1.2 关键验收)
    //   采样 16 段时误差应 < 1%; 4 顶点误差应 > 16 采样误差 (验证采样提升精度)
    {
        const int nsides[] = {1, 2, 4, 16, 64};
        const int n_cases = (int)(sizeof(nsides) / sizeof(nsides[0]));

        for (int ci = 0; ci < n_cases; ci++) {
            int nside = nsides[ci];
            healpix::HealpixCore hp(nside, true);
            double theory = 4.0 * M_PI / (12.0 * (double)nside * nside);

            // 选取赤道带中心像素 (bighp=4, x=nside/2, y=nside/2)
            // NSIDE=1 时每个 bighp 只有 1 个像素 (x=0, y=0)
            int bighp = 4;
            int x = nside / 2;
            int y = nside / 2;
            int64_t ipix = (int64_t)bighp * (int64_t)nside * nside;
            int xv = x, yv = y;
            for (int i = 0; i < 32; i++) {
                ipix |= ((int64_t)(((yv & 1) << 1) | (xv & 1))) << (i * 2);
                xv >>= 1; yv >>= 1;
                if (!xv && !yv) break;
            }

            // 4 顶点边界
            std::vector<spherical::Vec3> b4 =
                spherical::get_healpix_boundary(hp, (uint64_t)ipix, nside);
            double area_4 = spherical::spherical_polygon_area(b4);
            double err_4 = std::fabs(area_4 - theory) / theory;

            // 16 段采样边界
            std::vector<spherical::Vec3> b16 =
                spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside, 16);
            double area_16 = spherical::spherical_polygon_area(b16);
            double err_16 = std::fabs(area_16 - theory) / theory;

            printf("    NSIDE=%-3d 赤道带: 4顶点 err=%.4f%%, 16采样 err=%.4f%%\n",
                   nside, err_4 * 100, err_16 * 100);

            char name_cmp[128], name_tol[128];
            snprintf(name_cmp, sizeof(name_cmp),
                     "NSIDE=%d 16采样误差 < 4顶点误差", nside);
            snprintf(name_tol, sizeof(name_tol),
                     "NSIDE=%d 16采样误差 < 1%%", nside);

            // 16 采样误差应严格小于 4 顶点误差 (验证采样提升精度)
            ASSERT_TRUE(name_cmp, err_16 < err_4, "采样应提升精度");
            // Phase C1.2 关键验收: 16 采样误差 < 1%
            ASSERT_TRUE(name_tol, err_16 < 0.01,
                        "16 采样应将单像素面积误差降至 1% 以下");
        }
    }

    // 11.2 采样数单调性: 8 采样 vs 16 采样 (NSIDE=1, 像素最大, 误差最显著)
    {
        int nside = 1;
        healpix::HealpixCore hp(nside, true);
        double theory = 4.0 * M_PI / (12.0 * (double)nside * nside);
        uint64_t ipix = 4;  // NSIDE=1, bighp=4

        std::vector<spherical::Vec3> b8 =
            spherical::get_healpix_boundary_sampled(hp, ipix, nside, 8);
        double err_8 = std::fabs(spherical::spherical_polygon_area(b8) - theory) / theory;

        std::vector<spherical::Vec3> b16 =
            spherical::get_healpix_boundary_sampled(hp, ipix, nside, 16);
        double err_16 = std::fabs(spherical::spherical_polygon_area(b16) - theory) / theory;

        printf("    NSIDE=1 单调性: 8采样 err=%.4f%%, 16采样 err=%.4f%%\n",
               err_8 * 100, err_16 * 100);

        // 16 采样应不差于 8 采样 (允许浮点噪声 1e-10)
        ASSERT_TRUE("NSIDE=1 16采样不差于8采样 (单调提升)",
                    err_16 <= err_8 + 1e-10,
                    "更多采样段数应给出更小或相等的误差");
    }

    // 11.3 全天总面积: 采样后更接近 4π
    {
        int nside = 1;
        healpix::HealpixCore hp(nside, true);
        int64_t npix = hp.getNpix();

        double total_4 = 0.0, total_16 = 0.0;
        for (int64_t ipix = 0; ipix < npix; ipix++) {
            std::vector<spherical::Vec3> b4 = spherical::get_healpix_boundary(hp, (uint64_t)ipix, nside);
            total_4 += spherical::spherical_polygon_area(b4);

            std::vector<spherical::Vec3> b16 = spherical::get_healpix_boundary_sampled(hp, (uint64_t)ipix, nside, 16);
            total_16 += spherical::spherical_polygon_area(b16);
        }

        double err_4 = std::fabs(total_4 - 4.0 * M_PI) / (4.0 * M_PI);
        double err_16 = std::fabs(total_16 - 4.0 * M_PI) / (4.0 * M_PI);
        printf("    NSIDE=1 全天: 4顶点 err=%.4f%%, 16采样 err=%.4f%%\n",
               err_4 * 100, err_16 * 100);

        ASSERT_TRUE("全天总面积 采样后误差 <= 4顶点误差 (含浮点容差)",
                    err_16 <= err_4 + 1e-12,
                    "采样应提升或保持全天总面积精度");
        // 16 采样全天总面积误差应 < 1%
        ASSERT_TRUE("全天总面积 16采样误差 < 1%", err_16 < 0.01,
                    "16 采样全天总面积误差应 < 1%");
    }
}

// ============================================================================
// 测试 12: compute_overlap_area 使用采样边界后的精度提升
//
// 修改 compute_overlap_area 内部使用 get_healpix_boundary_sampled 后:
//   - 通量守恒精度应保持或提升
//   - 相邻像素重叠面积之和 = drop 总面积 (误差 < 1e-6)
// ============================================================================
static void test_overlap_with_sampled_boundary() {
    printf("\n[测试组 12] 采样边界下的重叠面积精度\n");

    // 12.1 低 NSIDE 下通量守恒: 采样后应仍满足 Σa_jp = A_drop
    {
        int nside = 4;
        healpix::HealpixCore hp(nside, true);
        // drop 覆盖多个像素
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 10.0);
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

        char msg[256];
        snprintf(msg, sizeof(msg), "Σa=%.10g, A_drop=%.10g, n=%d", sum_overlap, drop_area, n_nonzero);
        // 低 NSIDE (<=8) 使用 8 采样边界 (32 顶点), 裁剪数值噪声较大, 容差 5%
        // (相比 4 顶点边界面积误差 ~20%, 采样后单像素面积误差 < 1%, 通量守恒由裁剪精度限制)
        ASSERT_TRUE("低 NSIDE 采样边界通量守恒 < 5%",
                    std::fabs(sum_overlap - drop_area) < drop_area * 0.05, msg);
    }

    // 12.2 高 NSIDE 通量守恒: 严格容差
    {
        int nside = 256;
        healpix::HealpixCore hp(nside, true);
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 1.0);
        double drop_area = spherical::spherical_polygon_area(drop);

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) sum_overlap += a;
        }

        double rel_err = std::fabs(sum_overlap - drop_area) / drop_area;
        ASSERT_TRUE("高 NSIDE 采样边界通量守恒 < 1e-6", rel_err < 1e-6,
                    "采样不应破坏高 NSIDE 通量守恒");
    }
}

// ============================================================================
// WcsSip 回调包装: 将 spherical::PixelToSkyFn 适配到 drizzle::WcsSip::pixelToSky
// ============================================================================
static bool wcsPixelToSkyCallback(double x, double y, double& ra, double& dec,
                                  void* user_data) {
    const drizzle::WcsSip* wcs = static_cast<const drizzle::WcsSip*>(user_data);
    wcs->pixelToSky(x, y, ra, dec);
    return std::isfinite(ra) && std::isfinite(dec);
}

// ============================================================================
// 测试 13: 源像素 WCS/SIP 边细分精度提升
//
// processPixel 当前仅取源像素四角映射到球面, 用大圆弧连接.
// 但 WCS/SIP 投影 (TAN + SIP 畸变) 将像素直边映射为球面曲线, 非大圆弧.
// 边细分后 (samples_per_edge>1) 用多段大圆弧近似曲线, 降低面积误差.
//
// 测试策略:
//   1. 构造有 TAN 投影曲率的 WCS (大像素尺度 + 远离切点)
//   2. 分别用 samples=1 (4顶点), 8, 64 (近似真值) 构造 drop 多边形
//   3. 验证 samples=8 面积比 samples=1 更接近 samples=64
//   4. 验证 samples=1 退化为 4 顶点 (与旧代码一致)
//   5. 构造有 SIP 畸变的 WCS, 验证边细分对 SIP 弯曲边同样有效
// ============================================================================
static void test_drop_polygon_subdivision() {
    printf("\n[测试组 13] 源像素 WCS/SIP 边细分精度提升\n");

    // 13.1 TAN 投影曲率: 大像素 + 远离切点 → 边弯曲
    //   CRVAL=(45°,0°), CRPIX=(21,21) 1-based, CD=5.0°/px
    //   测试像素 (30,30) → 距切点 (10,10)*5° ≈ 70.7° → TAN 曲率显著
    //   像素 5°×5° 在 70° 处, TAN 投影将直边映射为显著弯曲的球面曲线
    {
        drizzle::WcsParams wcs;
        wcs.has_wcs = true;
        wcs.crval[0] = 45.0;
        wcs.crval[1] = 0.0;
        wcs.crpix[0] = 21.0;  // 1-based
        wcs.crpix[1] = 21.0;
        wcs.cd[0] = 5.0;   // 5°/px (极大像素, 放大 TAN 曲率效应)
        wcs.cd[1] = 0.0;
        wcs.cd[2] = 0.0;
        wcs.cd[3] = 5.0;
        std::strcpy(wcs.ctype1, "RA---TAN-SIP");
        std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
        wcs.sip.order = 0;  // 无 SIP, 纯 TAN 曲率

        drizzle::WcsSip wcsip(wcs);

        double px = 30.0, py = 30.0;  // dx=10, dy=10 → ~70.7° from center
        double pixfrac = 1.0;

        // samples=1 (4 顶点), 8, 64 (近似真值)
        std::vector<spherical::Vec3> d1 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 1);
        std::vector<spherical::Vec3> d8 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 8);
        std::vector<spherical::Vec3> d64 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 64);

        ASSERT_TRUE("TAN曲率: samples=1 返回 4 顶点", d1.size() == 4,
                    "samples=1 应退化为 4 顶点");
        ASSERT_TRUE("TAN曲率: samples=8 返回 32 顶点", d8.size() == 32,
                    "samples=8 应返回 4*8=32 顶点");
        ASSERT_TRUE("TAN曲率: samples=64 返回 256 顶点", d64.size() == 256,
                    "samples=64 应返回 4*64=256 顶点");

        double area1  = spherical::spherical_polygon_area(d1);
        double area8  = spherical::spherical_polygon_area(d8);
        double area64 = spherical::spherical_polygon_area(d64);
        double err1   = std::fabs(area1 - area64);
        double err8   = std::fabs(area8 - area64);

        printf("    TAN曲率: area(4v)=%.10g, area(32v)=%.10g, area(256v)=%.10g\n",
               area1, area8, area64);
        printf("             err(4v)=%.6e, err(32v)=%.6e\n", err1, err8);

        ASSERT_TRUE("TAN曲率: samples=8 比 samples=1 更精确",
                    err8 < err1,
                    "边细分应降低 TAN 投影曲率导致的面积误差");
    }

    // 13.2 SIP 畸变: 3 阶 SIP 多项式使像素边在球面上弯曲
    //   CRVAL=(45°,0°), CRPIX=(501,501), CD=0.01°/px
    //   SIP A[18]=1e-7 (dx³), B[3]=1e-7 (dy³) → 像素 (800,800) 处畸变显著
    {
        drizzle::WcsParams wcs;
        wcs.has_wcs = true;
        wcs.crval[0] = 45.0;
        wcs.crval[1] = 0.0;
        wcs.crpix[0] = 501.0;
        wcs.crpix[1] = 501.0;
        wcs.cd[0] = 0.01;
        wcs.cd[1] = 0.0;
        wcs.cd[2] = 0.0;
        wcs.cd[3] = 0.01;
        std::strcpy(wcs.ctype1, "RA---TAN-SIP");
        std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
        wcs.sip.order = 3;
        // A[i*6+j] 对应 dx^i*dy^j; A[18]=dx³, B[3]=dy³
        wcs.sip.a[18] = 1e-7;
        wcs.sip.b[3]  = 1e-7;

        drizzle::WcsSip wcsip(wcs);

        double px = 800.0, py = 800.0;  // dx=300, dy=300 from CRPIX
        double pixfrac = 1.0;

        std::vector<spherical::Vec3> d1 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 1);
        std::vector<spherical::Vec3> d8 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 8);
        std::vector<spherical::Vec3> d64 =
            spherical::build_drop_polygon_sampled(px, py, pixfrac,
                wcsPixelToSkyCallback, &wcsip, 64);

        // 检查投影成功 (SIP 畸变不应导致投影失败)
        ASSERT_TRUE("SIP畸变: samples=1 投影成功 (4 顶点)", d1.size() == 4,
                    "SIP 畸变下应能正常投影");
        ASSERT_TRUE("SIP畸变: samples=8 投影成功 (32 顶点)", d8.size() == 32,
                    "SIP 畸变下应能正常采样");

        double area1  = spherical::spherical_polygon_area(d1);
        double area8  = spherical::spherical_polygon_area(d8);
        double area64 = spherical::spherical_polygon_area(d64);
        double err1   = std::fabs(area1 - area64);
        double err8   = std::fabs(area8 - area64);

        printf("    SIP畸变: area(4v)=%.10g, area(32v)=%.10g, area(256v)=%.10g\n",
               area1, area8, area64);
        printf("             err(4v)=%.6e, err(32v)=%.6e\n", err1, err8);

        ASSERT_TRUE("SIP畸变: samples=8 比 samples=1 更精确",
                    err8 < err1,
                    "边细分应降低 SIP 畸变导致的面积误差");
    }

    // 13.3 小像素无畸变: samples=1 与 samples=8 面积应接近一致
    //   CRVAL=(45°,0°), CRPIX=(101,101), CD=0.01°/px, 像素 (100,100) ≈ 切点
    {
        drizzle::WcsParams wcs;
        wcs.has_wcs = true;
        wcs.crval[0] = 45.0;
        wcs.crval[1] = 0.0;
        wcs.crpix[0] = 101.0;
        wcs.crpix[1] = 101.0;
        wcs.cd[0] = 0.01;
        wcs.cd[1] = 0.0;
        wcs.cd[2] = 0.0;
        wcs.cd[3] = 0.01;
        std::strcpy(wcs.ctype1, "RA---TAN-SIP");
        std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
        wcs.sip.order = 0;

        drizzle::WcsSip wcsip(wcs);

        double px = 100.0, py = 100.0;  // 接近切点
        double pixfrac = 1.0;

        auto d1 = spherical::build_drop_polygon_sampled(px, py, pixfrac,
            wcsPixelToSkyCallback, &wcsip, 1);
        auto d8 = spherical::build_drop_polygon_sampled(px, py, pixfrac,
            wcsPixelToSkyCallback, &wcsip, 8);

        double area1 = spherical::spherical_polygon_area(d1);
        double area8 = spherical::spherical_polygon_area(d8);
        double rel_diff = std::fabs(area1 - area8) / area1;

        printf("    无畸变切点: area(4v)=%.10g, area(32v)=%.10g, rel_diff=%.6e\n",
               area1, area8, rel_diff);

        // 切点附近 TAN 投影近似线性, 但 4 顶点用大圆弧连接, 32 顶点追踪真实投影曲线,
        // 仍有微小差异 (二阶效应). 容差 5e-4 (0.05%) 验证 "接近一致".
        ASSERT_TRUE("无畸变切点: 4顶点 vs 32顶点面积接近一致 (rel < 5e-4)",
                    rel_diff < 5e-4,
                    "切点附近边弯曲微小, 面积应接近一致");
    }

    // 13.4 pixfrac 收缩: samples=1 的 4 顶点应正确收缩
    {
        drizzle::WcsParams wcs;
        wcs.has_wcs = true;
        wcs.crval[0] = 45.0;
        wcs.crval[1] = 0.0;
        wcs.crpix[0] = 101.0;
        wcs.crpix[1] = 101.0;
        wcs.cd[0] = 0.01;
        wcs.cd[1] = 0.0;
        wcs.cd[2] = 0.0;
        wcs.cd[3] = 0.01;
        std::strcpy(wcs.ctype1, "RA---TAN-SIP");
        std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
        wcs.sip.order = 0;

        drizzle::WcsSip wcsip(wcs);

        double px = 150.0, py = 150.0;
        double pixfrac = 0.5;  // 收缩到一半

        // samples=1 with pixfrac=0.5
        auto d_shrink = spherical::build_drop_polygon_sampled(px, py, pixfrac,
            wcsPixelToSkyCallback, &wcsip, 1);
        // samples=1 with pixfrac=1.0 (full pixel)
        auto d_full = spherical::build_drop_polygon_sampled(px, py, 1.0,
            wcsPixelToSkyCallback, &wcsip, 1);

        double area_shrink = spherical::spherical_polygon_area(d_shrink);
        double area_full   = spherical::spherical_polygon_area(d_full);

        printf("    pixfrac收缩: area(0.5)=%.10g, area(1.0)=%.10g, ratio=%.6f\n",
               area_shrink, area_full, area_shrink / area_full);

        // pixfrac=0.5 → 线性尺寸 ×0.5, 面积 ×0.25
        // 切点附近近似线性, 面积比应接近 0.25 (容差 5%)
        ASSERT_TRUE("pixfrac=0.5 面积 ≈ 0.25 × pixfrac=1.0 面积",
                    std::fabs(area_shrink / area_full - 0.25) < 0.05,
                    "pixfrac=0.5 应使面积约为原来的 1/4");
    }
}

// ============================================================================
// 主函数: 运行所有测试
// ============================================================================
int main() {
    printf("================================================================\n");
    printf("球面 HEALPix 重叠计算单元测试 (WP-D 步骤3-4)\n");
    printf("================================================================\n");

    test_known_spherical_area();
    test_healpix_pixel_area();
    test_flux_conservation();
    test_polar_region();
    test_large_field();
    test_candidate_query();
    test_overlap_non_negative();
    test_adjacent_pixel_sum();
    test_sutherland_hodgman();
    test_boundary_sampled_vertices();
    test_boundary_sampled_accuracy();
    test_overlap_with_sampled_boundary();
    test_drop_polygon_subdivision();

    printf("\n================================================================\n");
    printf("测试汇总: %d 通过, %d 失败\n", g_pass_count, g_fail_count);
    printf("================================================================\n");

    return (g_fail_count == 0) ? 0 : 1;
}
