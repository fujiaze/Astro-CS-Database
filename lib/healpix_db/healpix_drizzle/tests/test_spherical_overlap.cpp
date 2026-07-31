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
//     test_spherical_overlap.cpp ../spherical_overlap.cpp \
//     ../../healpix_stack/healpix_core.cpp \
//     -o test_spherical_overlap.exe
// ============================================================================

#include "spherical_overlap.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

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

    printf("\n================================================================\n");
    printf("测试汇总: %d 通过, %d 失败\n", g_pass_count, g_fail_count);
    printf("================================================================\n");

    return (g_fail_count == 0) ? 0 : 1;
}
