// P1-DRZ-TEST · 独立几何原语 (oracle 与 fixture 共用的 test-side 实现)
//
// 独立性规则 (模板 <prefix>-TEST §3): oracle 不调用被测函数、不复制同一
// 实现; 可用解析解/高精度朴素实现。本文件实现与生产 spherical_overlap/
// wcs_sip **不同源** 的同义数学原语:
//   - TAN 前向: 向量旋转法 (切点基 east/north + 单位向量归一), 生产为
//     atan2/asin 球面三角公式路径 (wcs_sip.cpp tanIntermediateToWorldT);
//   - SIP: FITS 标准多项式定义 Σ_{i+j≤order} c[i·6+j]·dx^i·dy^j
//     (数学定义共享, 实现独立);
//   - 球面四边形面积: Van Oosterom–Strackee 立体角公式 (2·atan2),
//     生产为 Eriksson 扇形三角剖分 (spherical_overlap.cpp);
//   - 重叠面积: 公共切平面 gnomonic 投影 + 平面 Sutherland–Hodgman +
//     shoelace (θ≲2e-3 rad 时相对误差 ~ρ²/2 ≈ 2e-6, 容差 1e-3 内);
//   - leaf 地址: astrocs::healpix 权威 NESTED 核心 (单权威, 头部声明
//     astropy-healpix 外部 oracle 交叉验证 mismatch=0; 与
//     oracle_independent_test 的 radec2pix 用法同规)。
#ifndef P1DRZ_GEOM_HPP
#define P1DRZ_GEOM_HPP

#include "fits_reader.h"
#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace p1drz {

struct GVec3 {
    double x, y, z;
};

inline GVec3 g_cross(const GVec3& a, const GVec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double g_dot(const GVec3& a, const GVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline GVec3 g_normalize(const GVec3& v) {
    const double l = std::sqrt(g_dot(v, v));
    return {v.x / l, v.y / l, v.z / l};
}

// radec (度) → 单位向量
inline GVec3 geom_radec_to_vec(double ra_deg, double dec_deg) {
    const double r = ra_deg * M_PI / 180.0, d = dec_deg * M_PI / 180.0;
    return {std::cos(d) * std::cos(r), std::cos(d) * std::sin(r), std::sin(d)};
}

// 单位向量 → radec (度), ra ∈ [0,360)
inline void geom_vec_to_radec(const GVec3& v, double& ra_deg, double& dec_deg) {
    dec_deg = std::asin(v.z) * 180.0 / M_PI;
    double ra = std::atan2(v.y, v.x) * 180.0 / M_PI;
    if (ra < 0.0) ra += 360.0;
    ra_deg = ra;
}

// ---------------------------------------------------------------------------
// 独立 TAN 前向 (含 SIP), 0-based 像素坐标 → (ra,dec) 度。
// FITS 标准 (Calabretta & Greisen 2002): dx = x-(CRPIX1-1); SIP 前向 A/B
// 加入像素偏移; CD → 中间坐标 (ξ,η); gnomonic 反投影。
// ---------------------------------------------------------------------------
inline void geom_eval_sip(const double* c, double dx, double dy, int order,
                          double* out) {
    double acc = 0.0;
    double pdx = 1.0;
    for (int i = 0; i <= order; ++i) {
        double pdy = 1.0;
        for (int j = 0; j <= order - i; ++j) {
            acc += c[i * 6 + j] * pdx * pdy;
            pdy *= dy;
        }
        pdx *= dx;
    }
    *out = acc;
}

inline void geom_pixel_to_sky(const drizzle::WcsParams& wcs, double x, double y,
                              double& ra_deg, double& dec_deg) {
    double dx = x - (wcs.crpix[0] - 1.0);
    double dy = y - (wcs.crpix[1] - 1.0);
    if (wcs.sip.order > 0) {
        double fa = 0.0, fb = 0.0;
        geom_eval_sip(wcs.sip.a, dx, dy, wcs.sip.order, &fa);
        geom_eval_sip(wcs.sip.b, dx, dy, wcs.sip.order, &fb);
        dx += fa;
        dy += fb;
    }
    const double xi = wcs.cd[0] * dx + wcs.cd[1] * dy;    // 度
    const double eta = wcs.cd[2] * dx + wcs.cd[3] * dy;   // 度

    // 向量旋转法 gnomonic 反投影
    const double r0 = wcs.crval[0] * M_PI / 180.0;
    const double d0 = wcs.crval[1] * M_PI / 180.0;
    const GVec3 n{std::cos(d0) * std::cos(r0), std::cos(d0) * std::sin(r0), std::sin(d0)};
    GVec3 east = g_normalize(g_cross({0.0, 0.0, 1.0}, n));
    const GVec3 north = g_cross(n, east);
    const double xi_r = xi * M_PI / 180.0, eta_r = eta * M_PI / 180.0;
    const GVec3 dir = g_normalize({n.x + xi_r * east.x + eta_r * north.x,
                                   n.y + xi_r * east.y + eta_r * north.y,
                                   n.z + xi_r * east.z + eta_r * north.z});
    geom_vec_to_radec(dir, ra_deg, dec_deg);
}

// ---------------------------------------------------------------------------
// drop 多边形: 像素中心 (0-based) ± 0.5·pixfrac 四角经独立 TAN 映射
// (与 drizzle_engine.cpp processPixelTiled Step1-2 的**语义**一致: half =
// 0.5·pixfrac, 角序 (px-h,py-h)→(px+h,py-h)→(px+h,py+h)→(px-h,py+h))。
// ---------------------------------------------------------------------------
inline std::vector<GVec3> geom_drop_polygon(const drizzle::WcsParams& wcs,
                                            double px, double py,
                                            double pixfrac) {
    const double h = 0.5 * pixfrac;
    const double c[4][2] = {{px - h, py - h}, {px + h, py - h},
                            {px + h, py + h}, {px - h, py + h}};
    std::vector<GVec3> out;
    out.reserve(4);
    for (int i = 0; i < 4; ++i) {
        double ra, dec;
        geom_pixel_to_sky(wcs, c[i][0], c[i][1], ra, dec);
        out.push_back(geom_radec_to_vec(ra, dec));
    }
    return out;
}

// ---------------------------------------------------------------------------
// 球面三角形立体角 (Van Oosterom & Strackee 1983):
//   tan(Ω/2) = a·(b×c) / (1 + a·b + b·c + c·a)  (单位向量)
// ---------------------------------------------------------------------------
inline double geom_solid_angle_tri(const GVec3& a, const GVec3& b, const GVec3& c) {
    const GVec3 n = g_cross(b, c);
    const double num = g_dot(a, n);
    const double den = 1.0 + g_dot(a, b) + g_dot(b, c) + g_dot(c, a);
    return 2.0 * std::atan2(std::fabs(num), den);
}

// 凸球面四边形面积 (对角三角剖分, 严格精确; 与生产 Eriksson 扇形剖分不同式)
inline double geom_quad_area(const std::vector<GVec3>& q) {
    return geom_solid_angle_tri(q[0], q[1], q[2]) +
           geom_solid_angle_tri(q[0], q[2], q[3]);
}

// ---------------------------------------------------------------------------
// 两凸球面多边形重叠面积 (球面度): 公共切平面 gnomonic + 平面 S-H + shoelace。
// 适用 θ ≲ 2e-3 rad (nside≥512 leaf 与源像素量级), 相对误差 ~ρ²/2 ≈ 2e-6。
// 顶点数 ≤ 8 (4 角 × 4 边裁剪), 栈容量安全。
// ---------------------------------------------------------------------------
namespace detail {

inline GVec3 g_sh_clip_edge(const GVec3& s, const GVec3& e) {
    return g_normalize(g_cross(s, e));
}

// 平面多边形有向面积 ×2
inline double g_shoelace2(const std::vector<double>& px, const std::vector<double>& py) {
    double a = 0.0;
    const int n = (int)px.size();
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        a += px[(std::size_t)i] * py[(std::size_t)j] - px[(std::size_t)j] * py[(std::size_t)i];
    }
    return a;
}

// 凸多边形 subject 被凸多边形 clip (CCW) 裁剪 (平面 2D Sutherland–Hodgman)
inline void g_clip_convex(const std::vector<double>& sx, const std::vector<double>& sy,
                          const std::vector<double>& cx, const std::vector<double>& cy,
                          std::vector<double>* ox, std::vector<double>* oy) {
    const int nc = (int)cx.size();
    std::vector<double> inx = sx, iny = sy;
    for (int e = 0; e < nc && !inx.empty(); ++e) {
        const int e2 = (e + 1) % nc;
        // 边 (c[e] → c[e2]) 的左半平面: cross(edge, p) >= 0
        const double ex = cx[(std::size_t)e2] - cx[(std::size_t)e];
        const double ey = cy[(std::size_t)e2] - cy[(std::size_t)e];
        std::vector<double> outx, outy;
        const int n = (int)inx.size();
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const double di = ex * (iny[(std::size_t)i] - cy[(std::size_t)e]) -
                              ey * (inx[(std::size_t)i] - cx[(std::size_t)e]);
            const double dj = ex * (iny[(std::size_t)j] - cy[(std::size_t)e]) -
                              ey * (inx[(std::size_t)j] - cx[(std::size_t)e]);
            if (di >= 0.0) {
                outx.push_back(inx[(std::size_t)i]);
                outy.push_back(iny[(std::size_t)i]);
            }
            if ((di > 0.0 && dj < 0.0) || (di < 0.0 && dj > 0.0)) {
                const double t = di / (di - dj);
                outx.push_back(inx[(std::size_t)i] + t * (inx[(std::size_t)j] - inx[(std::size_t)i]));
                outy.push_back(iny[(std::size_t)i] + t * (iny[(std::size_t)j] - iny[(std::size_t)i]));
            }
        }
        inx.swap(outx);
        iny.swap(outy);
    }
    ox->swap(inx);
    oy->swap(iny);
}

}  // namespace detail

inline double geom_overlap_area(const std::vector<GVec3>& drop,
                                const std::vector<GVec3>& target) {
    // 公共切点: 8 顶点均值方向 (两组多边形相近, 切点在内部附近)
    GVec3 c{0, 0, 0};
    for (const auto& v : drop) { c.x += v.x; c.y += v.y; c.z += v.z; }
    for (const auto& v : target) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = g_normalize(c);
    const GVec3 east = g_normalize(g_cross({0.0, 0.0, 1.0}, c));
    const GVec3 north = g_cross(c, east);

    auto project = [&](const std::vector<GVec3>& poly,
                       std::vector<double>* px, std::vector<double>* py) {
        for (const auto& v : poly) {
            const double d = g_dot(v, c);
            if (d <= 1e-12) return false;  // 越半球 (小多边形不发生)
            px->push_back(g_dot(v, east) / d);
            py->push_back(g_dot(v, north) / d);
        }
        // 强制 CCW
        if (detail::g_shoelace2(*px, *py) < 0.0) {
            std::reverse(px->begin(), px->end());
            std::reverse(py->begin(), py->end());
        }
        return true;
    };

    std::vector<double> dx, dy, tx, ty, ox, oy;
    if (!project(drop, &dx, &dy)) return -1.0;
    if (!project(target, &tx, &ty)) return -1.0;
    detail::g_clip_convex(dx, dy, tx, ty, &ox, &oy);
    if ((int)ox.size() < 3) return 0.0;
    return 0.5 * std::fabs(detail::g_shoelace2(ox, oy));
}

// ---------------------------------------------------------------------------
// drop 的候选 leaf 集合 (K×K 均匀超采样 + 权威 ang2pix_nest)。
// 用途: 污染 leaf 集合断言 (FIX-DRZ-E) 与 touched 集合零漏选抽查。
// ---------------------------------------------------------------------------
inline std::set<std::uint64_t> geom_pixel_leaves(const drizzle::WcsParams& wcs,
                                                 double px, double py,
                                                 double pixfrac,
                                                 std::uint32_t nside, int k) {
    std::set<std::uint64_t> out;
    const double h = 0.5 * pixfrac;
    for (int i = 0; i < k; ++i)
        for (int j = 0; j < k; ++j) {
            const double sx = px - h + (2.0 * h) * (i + 0.5) / k;
            const double sy = py - h + (2.0 * h) * (j + 0.5) / k;
            double ra, dec;
            geom_pixel_to_sky(wcs, sx, sy, ra, dec);
            out.insert(astrocs::healpix::ang2pix_nest(nside, ra, dec));
        }
    return out;
}

}  // namespace p1drz

#endif  // P1DRZ_GEOM_HPP
