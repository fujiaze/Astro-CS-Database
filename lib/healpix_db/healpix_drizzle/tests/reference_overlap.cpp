// ============================================================================
// Phase C3.2 + C2.3: 独立参考路径 (蒙特卡洛) + 跨经度 0/360 测试
//
// 目的:
// 现有 test_spherical_overlap.cpp 用 compute_overlap_area 自证正确
// (Σa_jp = A_drop), 存在自证循环。本文件实现完全独立的蒙特卡洛参考
// 路径, 并补充跨经度 0/360、单像素通量闭合、support 面积等测试。
//
// 独立性保证:
// - 蒙特卡洛路径不调用 compute_overlap_area / sutherland_hodgman_spherical
// - 点在多边形内测试用球面 winding number (与 Girard 面积法独立)
// - 像素归属用生产 HEALPix API: hp.radec2pix (ang2pix 逆运算)
// - 蒙特卡洛面积用解析包围盒面积 A_box = Δra·Δsin(dec), 不依赖 spherical_polygon_area
//
// 编译 (从 tests/ 目录):
// g++ -std=c++17 -O2 -fopenmp -I.. -I../../healpix_stack \
// reference_overlap.cpp ../spherical_overlap.cpp \
// ../../healpix_stack/healpix_core.cpp -lm -o reference_overlap.exe
// ============================================================================

#include "spherical_overlap.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <random>

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================================
// 常量
// ============================================================================
static const double PI      = 3.14159265358979323846;
static const double TWO_PI  = 2.0 * PI;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;

// ============================================================================
// 简单测试框架 (与 test_spherical_overlap.cpp 风格一致)
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
// 注: radec_to_vec 用 cos/sin 处理 RA, 天然支持 0/360 跨界 (360.15° ≡ 0.15°)
// ============================================================================
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

// ============================================================================
// 球面点在多边形内测试 (Gnomonic 投影 + 平面射线法)
//
// 算法 (与 Girard 面积法完全独立):
// 1. 以多边形质心为切点, 构造局部切平面坐标系 (e1, e2, center)
// 2. 用 Gnomonic (TAN) 投影将多边形顶点和测试点投影到切平面
// - Gnomonic 投影将大圆弧映射为直线, 故球面多边形 → 平面多边形 (精确)
// 3. 在切平面上用射线法判断点是否在平面多边形内
//
// 优点:
// - 完全在 3D 单位向量空间构造坐标系, 不受 RA 0/360 跨界影响
// - 对跖点自然排除 (dot(p, center) <= 0 → 背面 → 外部), 无数值不稳定
// - 与 Girard 定理面积计算使用完全不同的方法路径
// - 精确: 大圆弧边 → 直线边, 无近似误差
// ============================================================================
static bool point_in_spherical_polygon(const spherical::Vec3& p,
                                       const std::vector<spherical::Vec3>& polygon)
{
    int n = (int)polygon.size();
    if (n < 3) return false;

    // 1. 多边形质心 (切点)
    spherical::Vec3 center = {0.0, 0.0, 0.0};
    for (const auto& v : polygon) { center.x += v.x; center.y += v.y; center.z += v.z; }
    center = spherical::normalize(center);

    // 测试点在背面 (含对跖点) → 外部
    double dp = spherical::dot(p, center);
    if (dp <= 1e-9) return false;

    // 2. 构造切平面局部正交基 (e1, e2, center)
    spherical::Vec3 ref = (std::fabs(center.x) < 0.9)
        ? spherical::Vec3{1.0, 0.0, 0.0}
        : spherical::Vec3{0.0, 1.0, 0.0};
    spherical::Vec3 e1 = spherical::normalize(spherical::cross(ref, center));
    spherical::Vec3 e2 = spherical::cross(center, e1);

    // 3. Gnomonic 投影: (x, y) = (dot(v, e1)/dot(v, center), dot(v, e2)/dot(v, center))
    std::vector<double> xs(n), ys(n);
    for (int i = 0; i < n; i++) {
        double dv = spherical::dot(polygon[i], center);
        if (dv <= 1e-9) return false;  // 顶点在背面 (小多边形不应发生)
        xs[i] = spherical::dot(polygon[i], e1) / dv;
        ys[i] = spherical::dot(polygon[i], e2) / dv;
    }
    double px = spherical::dot(p, e1) / dp;
    double py = spherical::dot(p, e2) / dp;

    // 4. 顶点重合 → 边界 (内部)
    for (int i = 0; i < n; i++) {
        if (std::fabs(px - xs[i]) < 1e-12 && std::fabs(py - ys[i]) < 1e-12) return true;
    }

    // 5. 平面射线法: 从 (px, py) 向 +x 发射射线, 计算与多边形边的交点数
    int crossings = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double yi = ys[i], yj = ys[j];
        double xi = xs[i], xj = xs[j];
        // 边 (i,j) 跨越 py 所在水平线 (半开区间避免端点重复计数)
        if ((yi <= py && yj > py) || (yj <= py && yi > py)) {
            double t = (py - yi) / (yj - yi);
            double x_int = xi + t * (xj - xi);
            if (x_int > px) crossings++;
        }
    }
    return (crossings % 2) == 1;
}

// ============================================================================
// 球面包围盒 (RA/Dec 空间, 处理 0/360 跨界)
//
// 用相对 RA (相对于 drop 中心) 计算 min/max, 避免跨界时 [359.9, 0.1] 误判。
// 加 margin 容纳大圆弧相对直边的弧垂 (sagitta ≈ α²/8)。
// ============================================================================
struct BoundingBox {
    double ra_center;    // 中心 RA (度)
    double ra_min_rel;   // 最小相对 RA (度)
    double ra_max_rel;   // 最大相对 RA (度)
    double dec_min;      // 最小 Dec (度)
    double dec_max;      // 最大 Dec (度)
    double area_sr;      // 包围盒球面面积 (球面度) = Δra_rad · Δsin(dec)
};

static BoundingBox compute_bbox(const std::vector<spherical::Vec3>& corners)
{
    BoundingBox bb;
    bb.ra_center = 0.0; bb.ra_min_rel = 0.0; bb.ra_max_rel = 0.0;
    bb.dec_min = 0.0; bb.dec_max = 0.0; bb.area_sr = 0.0;
    if (corners.empty()) return bb;

    // 中心 = 顶点向量平均 (归一化)
    spherical::Vec3 center = {0.0, 0.0, 0.0};
    for (const auto& v : corners) { center.x += v.x; center.y += v.y; center.z += v.z; }
    center = spherical::normalize(center);
    double center_ra, center_dec;
    spherical::vec_to_radec(center, center_ra, center_dec);
    bb.ra_center = center_ra;

    double ra_min_rel = 1e9, ra_max_rel = -1e9;
    double dec_min = 1e9, dec_max = -1e9;
    for (const auto& v : corners) {
        double ra, dec;
        spherical::vec_to_radec(v, ra, dec);
        double rel = ra - center_ra;
        while (rel > 180.0) rel -= 360.0;
        while (rel < -180.0) rel += 360.0;
        ra_min_rel = std::min(ra_min_rel, rel);
        ra_max_rel = std::max(ra_max_rel, rel);
        dec_min = std::min(dec_min, dec);
        dec_max = std::max(dec_max, dec);
    }

    // margin: 容纳大圆弧弧垂 + 采样安全余量
    double ra_range = ra_max_rel - ra_min_rel;
    double dec_range = dec_max - dec_min;
    double margin = std::max(0.1, std::max(ra_range, dec_range) * 0.15);
    ra_min_rel -= margin; ra_max_rel += margin;
    dec_min -= margin; dec_max += margin;
    if (dec_min < -89.9) dec_min = -89.9;
    if (dec_max > 89.9) dec_max = 89.9;

    bb.ra_min_rel = ra_min_rel;
    bb.ra_max_rel = ra_max_rel;
    bb.dec_min = dec_min;
    bb.dec_max = dec_max;

    // 球面面积 = Δra_rad · (sin(dec_max) - sin(dec_min))
    double dra_rad = (ra_max_rel - ra_min_rel) * DEG2RAD;
    double s_min = std::sin(dec_min * DEG2RAD);
    double s_max = std::sin(dec_max * DEG2RAD);
    bb.area_sr = dra_rad * (s_max - s_min);
    return bb;
}

// ============================================================================
// 蒙特卡洛参考路径: 一次性采样, 返回所有像素的命中计数
//
// 算法:
// 1. 计算 drop 的球面包围盒 (处理 0/360 跨界)
// 2. 在包围盒内按球面面积均匀采样 (ra 均匀 + sin(dec) 均匀)
// 3. 对每个采样点:
// a. 转 Vec3, 用 point_in_spherical_polygon 判断是否在 drop 内
// b. 若在 drop 内, 用 hp.radec2pix 判断属于哪个 HEALPix 像素
// 4. 统计每个像素的命中数
//
// 面积计算 (完全独立, 不调用 spherical_polygon_area):
// - A_box = Δra_rad · Δsin(dec) (解析公式)
// - A_drop_mc = A_box · (N_inside / N_total)
// - A_overlap(p) = A_box · (N_target_p / N_total)
//
// 并行: OpenMP, 每线程独立 RNG + 本地计数, 最后合并
// ============================================================================
struct MCAllPixelsResult {
    double a_box;           // 包围盒球面面积 (球面度)
    double drop_area_mc;    // 蒙特卡洛 drop 面积 (A_box · N_inside / N_total)
    long long n_total;      // 总采样数
    long long n_inside_drop;// 落入 drop 内的采样数
    std::unordered_map<uint64_t, long long> pixel_counts;  // ipix → 命中数
};

static MCAllPixelsResult monte_carlo_all_pixels(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    long long n_samples)
{
    MCAllPixelsResult res;
    res.a_box = 0.0;
    res.drop_area_mc = 0.0;
    res.n_total = n_samples;
    res.n_inside_drop = 0;
    if ((int)drop_corners.size() < 3 || n_samples <= 0) return res;

    BoundingBox bb = compute_bbox(drop_corners);
    res.a_box = bb.area_sr;

    double ra_range = bb.ra_max_rel - bb.ra_min_rel;
    double sin_dec_min = std::sin(bb.dec_min * DEG2RAD);
    double sin_dec_max = std::sin(bb.dec_max * DEG2RAD);
    double sin_dec_range = sin_dec_max - sin_dec_min;

    long long n_inside = 0;
    std::unordered_map<uint64_t, long long> counts;

    int n_threads = 1;
#ifdef _OPENMP
    n_threads = omp_get_max_threads();
#endif

    #pragma omp parallel
    {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#else
        int tid = 0;
#endif
        std::mt19937_64 rng(0x9E3779B97F4A7C15ULL ^ ((uint64_t)tid * 0x9E3779B9U));
        std::uniform_real_distribution<double> uni(0.0, 1.0);

        long long local_inside = 0;
        std::unordered_map<uint64_t, long long> local_counts;
        local_counts.reserve(64);

        #pragma omp for schedule(static)
        for (long long i = 0; i < n_samples; i++) {
            double u = uni(rng);
            double v = uni(rng);

            // ra = center + min_rel + u * range, 包裹到 [0, 360)
            double ra = bb.ra_center + bb.ra_min_rel + u * ra_range;
            ra = ra - 360.0 * std::floor(ra / 360.0);
            if (ra >= 360.0) ra -= 360.0;

            // sin(dec) 均匀 → 球面面积均匀
            double s = sin_dec_min + v * sin_dec_range;
            if (s > 1.0) s = 1.0;
            if (s < -1.0) s = -1.0;
            double dec = std::asin(s) * RAD2DEG;

            spherical::Vec3 pt = spherical::radec_to_vec(ra, dec);
            if (!point_in_spherical_polygon(pt, drop_corners)) continue;

            local_inside++;
            int64_t ipix = hp.radec2pix(ra, dec);
            local_counts[(uint64_t)ipix]++;
        }

        #pragma omp critical
        {
            n_inside += local_inside;
            for (const auto& kv : local_counts) {
                counts[kv.first] += kv.second;
            }
        }
    }

    res.n_inside_drop = n_inside;
    res.pixel_counts = std::move(counts);
    res.drop_area_mc = res.a_box * (double)n_inside / (double)n_samples;
    return res;
}

// 单像素蒙特卡洛参考面积 (任务要求的接口)
static double monte_carlo_overlap_area(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix,
    long long n_samples = 1000000)
{
    auto res = monte_carlo_all_pixels(drop_corners, hp, n_samples);
    auto it = res.pixel_counts.find(target_ipix);
    if (it == res.pixel_counts.end()) return 0.0;
    return res.a_box * (double)it->second / (double)res.n_total;
}

// ============================================================================
// 测试 1: point_in_spherical_polygon 基本正确性
// ============================================================================
static void test_point_in_polygon_basic() {
    printf("\n[测试组 R1] 球面点在多边形内测试 (winding number)\n");

    // 10° × 10° 赤道四边形, 中心 (45°, 0°)
    std::vector<spherical::Vec3> quad = makeRectDrop(45.0, 0.0, 10.0);

    // 1.1 中心点 → 内部
    {
        spherical::Vec3 p = spherical::radec_to_vec(45.0, 0.0);
        ASSERT_TRUE("中心点在四边形内", point_in_spherical_polygon(p, quad),
                    "中心点应在内部");
    }
    // 1.2 远处点 → 外部
    {
        spherical::Vec3 p = spherical::radec_to_vec(180.0, 45.0);
        ASSERT_TRUE("远处点在四边形外", !point_in_spherical_polygon(p, quad),
                    "远处点应在外部");
    }
    // 1.3 顶角点 → 边界 (内部)
    {
        spherical::Vec3 p = quad[0];
        ASSERT_TRUE("顶角点在边界上 (视为内部)",
                    point_in_spherical_polygon(p, quad), "顶角应在内部");
    }
    // 1.4 对跖点 → 外部
    {
        spherical::Vec3 p = spherical::radec_to_vec(225.0, 0.0);  // 45+180
        ASSERT_TRUE("对跖点在外部", !point_in_spherical_polygon(p, quad),
                    "对跖点应在外部");
    }
    // 1.5 跨 0/360 的四边形: 中心 (359.9°, 0°), 边长 0.5°
    {
        std::vector<spherical::Vec3> wrap_quad = makeRectDrop(359.9, 0.0, 0.5);
        // 中心点 → 内部
        spherical::Vec3 p_center = spherical::radec_to_vec(359.9, 0.0);
        ASSERT_TRUE("跨界四边形中心点在内部",
                    point_in_spherical_polygon(p_center, wrap_quad),
                    "跨界四边形中心应在内部");
        // ra=0.0° 处的点 (在跨界四边形内, 因四边形覆盖 359.65°~0.15°)
        spherical::Vec3 p_zero = spherical::radec_to_vec(0.0, 0.0);
        ASSERT_TRUE("ra=0° 点在跨界四边形内",
                    point_in_spherical_polygon(p_zero, wrap_quad),
                    "ra=0° 应在跨界四边形 (359.65~0.15) 内");
        // ra=10° 处的点 → 外部
        spherical::Vec3 p_far = spherical::radec_to_vec(10.0, 0.0);
        ASSERT_TRUE("ra=10° 点在跨界四边形外",
                    !point_in_spherical_polygon(p_far, wrap_quad),
                    "ra=10° 应在外部");
    }
    // 1.6 北极附近四边形
    {
        std::vector<spherical::Vec3> polar = makeRectDrop(0.0, 89.0, 1.0);
        spherical::Vec3 p_in = spherical::radec_to_vec(0.0, 89.0);
        ASSERT_TRUE("北极四边形中心在内部",
                    point_in_spherical_polygon(p_in, polar),
                    "北极四边形中心应在内部");
        spherical::Vec3 p_out = spherical::radec_to_vec(180.0, 0.0);
        ASSERT_TRUE("赤道点在北极四边形外",
                    !point_in_spherical_polygon(p_out, polar),
                    "赤道点应在外部");
    }
}

// ============================================================================
// 测试 2: 蒙特卡洛 drop 面积 vs Girard 定理面积 (交叉验证)
// ============================================================================
static void test_monte_carlo_drop_area() {
    printf("\n[测试组 R2] 蒙特卡洛 drop 面积 vs Girard 面积\n");

    struct Case { const char* name; double ra; double dec; double size; };
    Case cases[] = {
        {"赤道 1° drop",   45.0,  0.0, 1.0},
        {"赤道 5° drop",   45.0,  0.0, 5.0},
        {"中纬 2° drop",   100.0, 30.0, 2.0},
        {"跨界 1° drop",   359.9, 0.0, 1.0},
    };

    for (const auto& c : cases) {
        std::vector<spherical::Vec3> drop = makeRectDrop(c.ra, c.dec, c.size);
        double area_girard = spherical::spherical_polygon_area(drop);

        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        auto mc = monte_carlo_all_pixels(drop, hp, 1000000);

        double rel_err = std::fabs(mc.drop_area_mc - area_girard) / area_girard;
        char msg[256];
        snprintf(msg, sizeof(msg), "MC=%.10g, Girard=%.10g, rel_err=%.6g, N_in=%lld",
                 mc.drop_area_mc, area_girard, rel_err, mc.n_inside_drop);
        printf("    %s: %s\n", c.name, msg);

        // 10^6 采样, 统计误差 ~0.1%, 容差 0.5%
        ASSERT_TRUE((std::string(c.name) + " MC vs Girard < 0.5%").c_str(),
                    rel_err < 0.005, msg);
    }
}

// ============================================================================
// 测试 3: 蒙特卡洛 vs 生产算法对比 (核心独立验证)
//
// 用 nside=64, 1°×1° drop, MC 5×10^6 采样
// 对每个有显著重叠的像素, 对比 MC 面积与 compute_overlap_area 面积
// 验证最大相对误差 < 1%
// ============================================================================
static void test_monte_carlo_vs_production() {
    printf("\n[测试组 R3] 蒙特卡洛 vs 生产算法对比\n");

    int nside = 64;
    healpix::HealpixCore hp(nside, true);
    std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 1.0);
    double drop_area = spherical::spherical_polygon_area(drop);

    // 蒙特卡洛 (5×10^6 采样, 提高精度)
    long long n_samples = 5000000;
    auto mc = monte_carlo_all_pixels(drop, hp, n_samples);

    printf("    MC: N_total=%lld, N_inside=%lld, A_drop_mc=%.10g, A_drop_girard=%.10g\n",
           mc.n_total, mc.n_inside_drop, mc.drop_area_mc, drop_area);

    // 生产算法
    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop, hp, candidates);

    double sum_prod = 0.0, sum_mc = 0.0;
    double max_rel_err = 0.0;
    int n_compared = 0;
    double threshold = drop_area * 0.005;  // 只对比重叠 > 0.5% 的像素

    printf("    %-12s %14s %14s %10s\n", "ipix", "prod_area", "mc_area", "rel_err");

    for (uint64_t ipix : candidates) {
        double a_prod = spherical::compute_overlap_area(drop, hp, ipix);
        sum_prod += a_prod;

        double a_mc = 0.0;
        auto it = mc.pixel_counts.find(ipix);
        if (it != mc.pixel_counts.end()) {
            a_mc = mc.a_box * (double)it->second / (double)mc.n_total;
        }
        sum_mc += a_mc;

        if (a_prod > threshold) {
            n_compared++;
            double rel = std::fabs(a_prod - a_mc) / a_prod;
            if (rel > max_rel_err) max_rel_err = rel;
            printf("    %-12llu %14.8g %14.8g %9.4f%%\n",
                   (unsigned long long)ipix, a_prod, a_mc, rel * 100.0);
        }
    }

    // MC 中有但 candidates 中没有的像素 (应该只有噪声级计数)
    for (const auto& kv : mc.pixel_counts) {
        uint64_t ipix = kv.first;
        if (std::binary_search(candidates.begin(), candidates.end(), ipix)) continue;
        double a_mc = mc.a_box * (double)kv.second / (double)mc.n_total;
        sum_mc += a_mc;  // 已在上面加过? 不, 上面只遍历 candidates
        // 这些是 MC 噪声 (落在 candidates 之外的像素), 不计入对比
    }

    printf("    ---- 汇总 ----\n");
    printf("    Σ prod = %.10g (A_drop=%.10g, rel=%.6g)\n",
           sum_prod, drop_area, std::fabs(sum_prod - drop_area) / drop_area);
    printf("    Σ mc   = %.10g (A_drop=%.10g, rel=%.6g)\n",
           sum_mc, drop_area, std::fabs(sum_mc - drop_area) / drop_area);
    printf("    对比像素数 = %d, 最大相对误差 = %.4f%%\n", n_compared, max_rel_err * 100.0);

    // 验证: 最大相对误差 < 1%
    char msg[256];
    snprintf(msg, sizeof(msg), "max_rel_err=%.4f%% (n=%d)", max_rel_err * 100.0, n_compared);
    ASSERT_TRUE("MC vs 生产 最大相对误差 < 1%", max_rel_err < 0.01, msg);

    // 验证: MC 通量守恒
    double mc_flux_err = std::fabs(sum_mc - drop_area) / drop_area;
    snprintf(msg, sizeof(msg), "MC Σa=%.10g, A_drop=%.10g, err=%.6g", sum_mc, drop_area, mc_flux_err);
    ASSERT_TRUE("MC 通量守恒 < 1%", mc_flux_err < 0.01, msg);

    // 验证: 生产通量守恒
    double prod_flux_err = std::fabs(sum_prod - drop_area) / drop_area;
    snprintf(msg, sizeof(msg), "prod Σa=%.10g, A_drop=%.10g, err=%.6g", sum_prod, drop_area, prod_flux_err);
    ASSERT_TRUE("生产通量守恒 < 1e-6", prod_flux_err < 1e-6, msg);
}

// ============================================================================
// 测试 4: 单像素通量闭合
// 4.1 drop 完全在单个 HEALPix 像素内 → overlap ≈ drop_area
// 4.2 drop 完全包含单个 HEALPix 像素 → overlap ≈ A_pixel
// ============================================================================
static void test_single_pixel_flux_closure() {
    printf("\n[测试组 R4] 单像素通量闭合\n");

    // 4.1 小 drop 完全在单个像素内 (nside=64, pixel ≈ 0.92°)
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        // 取像素中心
        int64_t ipix = hp.radec2pix(45.0, 0.0);
        double ra_c, dec_c;
        hp.pix2radec(ipix, &ra_c, &dec_c);

        // 0.001° × 0.001° drop, 远小于像素 (0.92°)
        std::vector<spherical::Vec3> drop = makeRectDrop(ra_c, dec_c, 0.001);
        double drop_area = spherical::spherical_polygon_area(drop);

        // 验证 drop 中心确实在该像素内
        int64_t ipix_check = hp.radec2pix(ra_c, dec_c);
        ASSERT_TRUE("小 drop 中心在目标像素内",
                    (uint64_t)ipix_check == (uint64_t)ipix,
                    "drop 中心应在目标像素内");

        double overlap = spherical::compute_overlap_area(drop, hp, (uint64_t)ipix);
        char msg[256];
        snprintf(msg, sizeof(msg), "overlap=%.14g, drop_area=%.14g", overlap, drop_area);
        // drop 完全在像素内 → overlap = drop_area (裁剪返回 drop 不变)
        ASSERT_NEAR("drop 内含于像素: overlap = drop_area (tol 1e-10)",
                    overlap, drop_area, drop_area * 1e-10);
    }

    // 4.2 大 drop 完全包含单个像素 (nside=16, pixel ≈ 3.7°)
    {
        int nside = 16;
        healpix::HealpixCore hp(nside, true);
        int64_t ipix = hp.radec2pix(45.0, 0.0);

        // 20° × 20° drop, 远大于像素
        std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 20.0);

        // 验证像素 4 角均在 drop 内 (确认 drop 完全包含像素)
        std::vector<spherical::Vec3> pix_bound = spherical::get_healpix_boundary<double>(hp, (uint64_t)ipix, nside);
        bool all_inside = true;
        for (const auto& v : pix_bound) {
            if (!point_in_spherical_polygon(v, drop)) { all_inside = false; break; }
        }
        ASSERT_TRUE("像素 4 角均在 drop 内 (drop 完全包含像素)",
                    all_inside, "drop 应完全包含像素");

        // A_pixel_ref: 同样用 4 角顶点 (与 compute_overlap_area 内部一致, nside>8 → samples=1)
        double a_pixel_ref = spherical::spherical_polygon_area(pix_bound);
        double overlap = spherical::compute_overlap_area(drop, hp, (uint64_t)ipix);

        char msg[256];
        snprintf(msg, sizeof(msg), "overlap=%.14g, A_pixel_ref=%.14g", overlap, a_pixel_ref);
        printf("    drop 包含像素: overlap=%.14g, A_pixel_ref=%.14g, diff=%.6g\n",
               overlap, a_pixel_ref, std::fabs(overlap - a_pixel_ref));

        // drop 完全包含像素 → 裁剪返回像素边界 → overlap = A_pixel_ref
        // 容差 1e-10 (浮点精度; 裁剪交点计算可能有微小误差)
        ASSERT_NEAR("drop 包含像素: overlap = A_pixel (tol 1e-10)",
                    overlap, a_pixel_ref, a_pixel_ref * 1e-10);
    }
}

// ============================================================================
// 测试 5: support 面积误差验证
// - support_p = a_jp / A_p ∈ [0, 1] (A_p 用与 compute_overlap_area 一致的边界面积)
// - Σ (support_p × A_p) = A_drop (面积守恒)
//
// 注: A_p 用 spherical_polygon_area(get_healpix_boundary<double>(hp, ipix, nside)),
// 与 compute_overlap_area 内部使用的边界一致 (nside>8 → samples=1 → 4 角顶点).
// 理论面积 4π/(12·nside²) 与大圆弧边界面积有微小差异 (赤道带小圆弧近似),
// 用边界面积可保证 support ∈ [0, 1] 精确成立 (重叠 ≤ 像素自身面积).
// ============================================================================
static void test_support_area() {
    printf("\n[测试组 R5] support 面积验证\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);
    std::vector<spherical::Vec3> drop = makeRectDrop(45.0, 0.0, 1.0);
    double drop_area = spherical::spherical_polygon_area(drop);

    double a_pixel_theory = 4.0 * PI / (12.0 * (double)nside * nside);

    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop, hp, candidates);

    double sum_weighted = 0.0;
    int n_overlap = 0;
    bool all_in_range = true;
    double max_support = 0.0, min_support = 1.0;
    double max_theory_excess = 0.0;  // 理论面积 support 超出 1 的最大值

    for (uint64_t ipix : candidates) {
        double a_jp = spherical::compute_overlap_area(drop, hp, ipix);
        if (a_jp <= 1e-15) continue;
        n_overlap++;

        // A_p_boundary: 与 compute_overlap_area 内部一致的边界面积
        std::vector<spherical::Vec3> pb = spherical::get_healpix_boundary<double>(hp, ipix, nside);
        double a_p_boundary = spherical::spherical_polygon_area(pb);

        double support = a_jp / a_p_boundary;
        sum_weighted += support * a_p_boundary;  // = a_jp

        if (support > max_support) max_support = support;
        if (support < min_support) min_support = support;

        // support 应 ∈ [0, 1] (允许 Sutherland-Hodgman 多阶段裁剪的浮点累计误差)
        if (support < -1e-6 || support > 1.0 + 1e-6) {
            all_in_range = false;
            printf("    像素 %llu: support=%.15g 超出 [0,1]\n",
                   (unsigned long long)ipix, support);
        }

        // 记录理论面积 support 超出 1 的量 (用于展示边界近似误差)
        double support_theory = a_jp / a_pixel_theory;
        if (support_theory > 1.0) {
            max_theory_excess = std::max(max_theory_excess, support_theory - 1.0);
        }
    }

    printf("    n_overlap=%d, support ∈ [%.8f, %.8f], Σ(support·A_p)=%.10g, A_drop=%.10g\n",
           n_overlap, min_support, max_support, sum_weighted, drop_area);
    printf("    理论面积 support 最大超出 1 的量: %.6g (大圆弧边界近似误差)\n",
           max_theory_excess);

    ASSERT_TRUE("support 全部 ∈ [0, 1]", all_in_range,
                "support 应在 [0, 1] 范围内");

    // Σ (support × A_p) = Σ a_jp = A_drop (通量守恒)
    double rel_err = std::fabs(sum_weighted - drop_area) / drop_area;
    char msg[256];
    snprintf(msg, sizeof(msg), "Σ(support·A_p)=%.10g, A_drop=%.10g, rel_err=%.6g",
             sum_weighted, drop_area, rel_err);
    ASSERT_TRUE("Σ(support × A_p) = A_drop (< 1e-6)", rel_err < 1e-6, msg);
}

// ============================================================================
// 测试 6: 跨经度 0/360 测试
// 6.1 drop 中心在 ra=359.9°, 跨越 0/360 边界
// 6.2 drop 完全包含 ra=0 经线
// 6.3 drop 中心在 ra=0.1° (对称性验证)
// ============================================================================
static void test_cross_longitude_0_360() {
    printf("\n[测试组 R6] 跨经度 0/360 测试\n");

    // 6.1 drop 中心在 ra=359.9°, 边长 0.5° (跨越 0/360)
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        std::vector<spherical::Vec3> drop = makeRectDrop(359.9, 0.0, 0.5);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("跨界 drop 面积 > 0", drop_area > 0.0, "跨界 drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);
        ASSERT_TRUE("跨界 drop 候选像素非空", !candidates.empty(),
                    "应返回候选像素");

        // 验证候选包含 ra=0 附近 和 ra=359° 附近的像素
        bool has_near_0 = false, has_near_360 = false;
        for (uint64_t ipix : candidates) {
            double ra, dec;
            hp.pix2radec((int64_t)ipix, &ra, &dec);
            if (ra < 1.0) has_near_0 = true;
            if (ra > 359.0) has_near_360 = true;
        }
        ASSERT_TRUE("候选包含 ra≈0° 附近像素", has_near_0,
                    "跨界 drop 候选应包含 ra=0 附近像素");
        ASSERT_TRUE("候选包含 ra≈359° 附近像素", has_near_360,
                    "跨界 drop 候选应包含 ra=359° 附近像素");

        // 通量守恒: Σ a_jp = A_drop
        double sum_overlap = 0.0;
        int n_nonzero = 0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) { sum_overlap += a; n_nonzero++; }
        }
        char msg[256];
        snprintf(msg, sizeof(msg), "Σa=%.10g, A_drop=%.10g, n=%d",
                 sum_overlap, drop_area, n_nonzero);
        printf("    6.1 跨界 (359.9°): %s\n", msg);
        ASSERT_TRUE("跨界 drop 通量守恒 < 1e-4",
                    std::fabs(sum_overlap - drop_area) < drop_area * 1e-4, msg);

        // 蒙特卡洛独立验证
        auto mc = monte_carlo_all_pixels(drop, hp, 1000000);
        double mc_sum = 0.0;
        for (uint64_t ipix : candidates) {
            auto it = mc.pixel_counts.find(ipix);
            if (it != mc.pixel_counts.end())
                mc_sum += mc.a_box * (double)it->second / (double)mc.n_total;
        }
        double mc_err = std::fabs(mc_sum - drop_area) / drop_area;
        snprintf(msg, sizeof(msg), "MC Σa=%.10g, A_drop=%.10g, err=%.6g",
                 mc_sum, drop_area, mc_err);
        printf("    6.1 跨界 MC: %s\n", msg);
        ASSERT_TRUE("跨界 drop MC 通量守恒 < 1%", mc_err < 0.01, msg);
    }

    // 6.2 drop 完全包含 ra=0 经线 (中心 ra=0°, 边长 2° → 覆盖 359°~1°)
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        std::vector<spherical::Vec3> drop = makeRectDrop(0.0, 0.0, 2.0);
        double drop_area = spherical::spherical_polygon_area(drop);
        ASSERT_TRUE("含 ra=0 经线 drop 面积 > 0", drop_area > 0.0,
                    "drop 面积应为正");

        std::vector<uint64_t> candidates;
        spherical::query_candidate_pixels(drop, hp, candidates);

        bool has_near_0 = false, has_near_360 = false;
        for (uint64_t ipix : candidates) {
            double ra, dec;
            hp.pix2radec((int64_t)ipix, &ra, &dec);
            if (ra < 1.0) has_near_0 = true;
            if (ra > 359.0) has_near_360 = true;
        }
        ASSERT_TRUE("含 ra=0 经线: 候选含 ra≈0° 像素", has_near_0,
                    "应包含 ra=0 附近像素");
        ASSERT_TRUE("含 ra=0 经线: 候选含 ra≈359° 像素", has_near_360,
                    "应包含 ra=359° 附近像素");

        double sum_overlap = 0.0;
        for (uint64_t ipix : candidates) {
            double a = spherical::compute_overlap_area(drop, hp, ipix);
            if (a > 1e-15) sum_overlap += a;
        }
        char msg[256];
        snprintf(msg, sizeof(msg), "Σa=%.10g, A_drop=%.10g", sum_overlap, drop_area);
        printf("    6.2 含 ra=0: %s\n", msg);
        ASSERT_TRUE("含 ra=0 经线 drop 通量守恒 < 1e-4",
                    std::fabs(sum_overlap - drop_area) < drop_area * 1e-4, msg);
    }

    // 6.3 对称性: drop 中心在 ra=0.1° vs ra=359.9°, 结果应对称
    {
        int nside = 64;
        healpix::HealpixCore hp(nside, true);
        std::vector<spherical::Vec3> drop_a = makeRectDrop(0.1, 0.0, 0.5);
        std::vector<spherical::Vec3> drop_b = makeRectDrop(359.9, 0.0, 0.5);
        double area_a = spherical::spherical_polygon_area(drop_a);
        double area_b = spherical::spherical_polygon_area(drop_b);

        // 面积应几乎相等 (球面对称)
        ASSERT_NEAR("对称性: 两 drop 面积相等", area_a, area_b,
                    area_a * 1e-10);

        // 通量守恒均成立
        std::vector<uint64_t> cand_a, cand_b;
        spherical::query_candidate_pixels(drop_a, hp, cand_a);
        spherical::query_candidate_pixels(drop_b, hp, cand_b);

        double sum_a = 0.0, sum_b = 0.0;
        for (uint64_t ipix : cand_a) {
            double a = spherical::compute_overlap_area(drop_a, hp, ipix);
            if (a > 1e-15) sum_a += a;
        }
        for (uint64_t ipix : cand_b) {
            double a = spherical::compute_overlap_area(drop_b, hp, ipix);
            if (a > 1e-15) sum_b += a;
        }
        char msg[256];
        snprintf(msg, sizeof(msg), "sum_a=%.10g, sum_b=%.10g", sum_a, sum_b);
        printf("    6.3 对称性: %s\n", msg);
        ASSERT_NEAR("对称性: 通量守恒值相等", sum_a, sum_b, area_a * 1e-4);

        // 候选数应相同 (对称结构)
        ASSERT_TRUE("对称性: 候选像素数相同",
                    cand_a.size() == cand_b.size(),
                    "对称 drop 应有相同候选数");
    }
}

// ============================================================================
// 主函数
// ============================================================================
int main() {
    printf("================================================================\n");
    printf("Phase C3.2 + C2.3: 独立参考路径 + 跨经度 0/360 测试\n");
    printf("================================================================\n");

    test_point_in_polygon_basic();
    test_monte_carlo_drop_area();
    test_monte_carlo_vs_production();
    test_single_pixel_flux_closure();
    test_support_area();
    test_cross_longitude_0_360();

    printf("\n================================================================\n");
    printf("测试汇总: %d 通过, %d 失败\n", g_pass_count, g_fail_count);
    printf("================================================================\n");

    return (g_fail_count == 0) ? 0 : 1;
}
