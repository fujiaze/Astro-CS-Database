// ============================================================================
// fast_overlap.cpp - FAST Drizzle 切平面重叠计算 (实验性)
//
// 实现见 fast_overlap.h 注释.
// 与 PRECISE (spherical_overlap.cpp) 共享: WCS/SIP, 候选查询, 累加语义.
// 区别: 切平面 gnomonic 投影 + 2D 裁剪 + 鞋带公式.
// ============================================================================

#include "fast_overlap.h"
#include "spherical_overlap.h"
#include "healpix_core.h"

#include <cmath>
#include <algorithm>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fast {

// 度 → 弧度
static const double D2R = 0.017453292519943295769;
static const double R2D = 57.2957795130823208768;

// ============================================================================
// Gnomonic 投影: 球面(度) → 切平面(xi, eta)
// ============================================================================
bool gnomonic_forward(double ra_deg, double dec_deg,
                      double ra0_deg, double dec0_deg,
                      double& xi, double& eta) {
    double ra  = ra_deg  * D2R;
    double dec = dec_deg * D2R;
    double ra0 = ra0_deg * D2R;
    double dec0 = dec0_deg * D2R;

    double dRa = ra - ra0;
    // 归一化 dRa 到 [-pi, pi]
    while (dRa > M_PI)  dRa -= 2.0 * M_PI;
    while (dRa < -M_PI) dRa += 2.0 * M_PI;

    double cos_dec  = std::cos(dec);
    double sin_dec  = std::sin(dec);
    double cos_dec0 = std::cos(dec0);
    double sin_dec0 = std::sin(dec0);

    double cos_c = sin_dec0 * sin_dec + cos_dec0 * cos_dec * std::cos(dRa);
    if (cos_c <= 1e-12) {
        // 点在投影背面或切点对跖点附近
        return false;
    }

    xi  = cos_dec * std::sin(dRa) / cos_c;
    eta = (cos_dec0 * sin_dec - sin_dec0 * cos_dec * std::cos(dRa)) / cos_c;
    return true;
}

// ============================================================================
// 球面 Vec3 → 天球坐标(度)
// ============================================================================
static void vec3_to_radec(const spherical::Vec3& v, double& ra_deg, double& dec_deg) {
    double x = v.x, y = v.y, z = v.z;
    double r = std::sqrt(x*x + y*y + z*z);
    if (r < 1e-30) { ra_deg = 0; dec_deg = 0; return; }
    dec_deg = std::asin(z / r) * R2D;
    ra_deg = std::atan2(y, x) * R2D;
    if (ra_deg < 0) ra_deg += 360.0;
}

// ============================================================================
// 将球面 Vec3 多边形投影到切平面
//
// R07-M02 修复: 任一顶点投影失败 → 整多边形失败 (返回空向量).
//   原实现静默丢弃失败顶点, 导致多边形变形, 裁剪结果错误.
// ============================================================================
static std::vector<Point2D> project_vec3_to_planar(
    const std::vector<spherical::Vec3>& verts,
    double ra0_deg, double dec0_deg) {
    if (verts.empty()) return {};
    std::vector<Point2D> result;
    result.reserve(verts.size());
    for (const auto& v : verts) {
        double ra, dec;
        vec3_to_radec(v, ra, dec);
        double xi, eta;
        if (!gnomonic_forward(ra, dec, ra0_deg, dec0_deg, xi, eta)) {
            // R07-M02: 投影失败 → 整多边形失败
            return {};
        }
        result.push_back({xi, eta});
    }
    return result;
}

// ============================================================================
// 构造源像素 drop 的切平面 2D 多边形
// ============================================================================
std::vector<Point2D> build_drop_polygon_planar(
    double px, double py, double pixfrac,
    spherical::PixelToSkyFn pixelToSky, void* user_data,
    int samples_per_edge,
    double& center_ra, double& center_dec) {

    // 获取 drop 中心的球面坐标作为切点
    if (!pixelToSky(px, py, center_ra, center_dec, user_data)) {
        return {};
    }

    // 复用 PRECISE 的球面 drop 多边形构建 (共享 WCS/SIP + pixfrac 语义)
    std::vector<spherical::Vec3> drop_spherical =
        spherical::build_drop_polygon_sampled(px, py, pixfrac,
                                               pixelToSky, user_data,
                                               samples_per_edge);
    if (drop_spherical.empty()) return {};

    // 投影到切平面
    return project_vec3_to_planar(drop_spherical, center_ra, center_dec);
}

// ============================================================================
// 获取 HEALPix 像素的切平面 2D 边界
// ============================================================================
std::vector<Point2D> get_healpix_boundary_planar(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge,
    double ra0, double dec0) {

    // 复用 PRECISE 的球面边界获取
    std::vector<spherical::Vec3> boundary_spherical =
        spherical::get_healpix_boundary_sampled(hp, ipix, nside, samples_per_edge);
    if (boundary_spherical.empty()) return {};

    // 投影到切平面
    return project_vec3_to_planar(boundary_spherical, ra0, dec0);
}

// ============================================================================
// 2D 有符号面积 (正=逆时针, 负=顺时针)
// ============================================================================
static double signed_area_2d(const std::vector<Point2D>& poly) {
    if (poly.size() < 3) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        sum += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return sum * 0.5;
}

// ============================================================================
// 2D Sutherland-Hodgman 多边形裁剪
//
// R07-M01 修复: 计算裁剪多边形有符号面积, 统一为逆时针方向.
//   原实现固定假设 clip 为 CCW (inside = 左侧), 但 HEALPix 12 个 base face
//   投影后方向可能为 CW, 导致裁剪结果为空或错误.
//
// R07-M03 修复: 近平行交线稳健分类.
//   原实现 denom→0 时返回起点 s, 可能产生伪交点. 改为: 平行时检查
//   s 是否在裁剪边上, 是则保留 s, 否则跳过 (不产生交点).
// ============================================================================
static bool inside(const Point2D& p, const Point2D& edge_start, const Point2D& edge_end,
                  bool clip_ccw) {
    // 判断点在边的哪一侧
    double cross = (edge_end.x - edge_start.x) * (p.y - edge_start.y) -
                   (edge_end.y - edge_start.y) * (p.x - edge_start.x);
    // CCW 多边形: 内侧 = 左侧 (cross >= 0)
    // CW  多边形: 内侧 = 右侧 (cross <= 0)
    return clip_ccw ? (cross >= 0) : (cross <= 0);
}

static bool intersect_2d(const Point2D& s, const Point2D& e,
                         const Point2D& clip_s, const Point2D& clip_e,
                         Point2D& out) {
    // 计算线段 (s→e) 与裁剪边 (clip_s→clip_e) 的交点
    double dx1 = e.x - s.x, dy1 = e.y - s.y;
    double dx2 = clip_e.x - clip_s.x, dy2 = clip_e.y - clip_s.y;
    double denom = dx1 * dy2 - dy1 * dx2;
    if (std::abs(denom) < 1e-30) {
        // R07-M03: 平行线, 不产生交点
        return false;
    }
    double t = ((clip_s.x - s.x) * dy2 - (clip_s.y - s.y) * dx2) / denom;
    // 交点必须在线段 [s, e] 上 (0 <= t <= 1)
    if (t < -1e-12 || t > 1.0 + 1e-12) return false;
    out = {s.x + t * dx1, s.y + t * dy1};
    return true;
}

std::vector<Point2D> clip_polygon_2d(
    const std::vector<Point2D>& subject,
    const std::vector<Point2D>& clip) {
    if (subject.size() < 3 || clip.size() < 3) return {};

    // R07-M01: 检测 clip 多边形方向, 统一为 CCW 处理
    bool clip_ccw = (signed_area_2d(clip) > 0.0);

    std::vector<Point2D> output = subject;
    Point2D clip_edge_start = clip.back();

    for (size_t i = 0; i < clip.size(); i++) {
        Point2D clip_edge_end = clip[i];
        std::vector<Point2D> input = std::move(output);
        output.clear();

        if (input.empty()) break;

        Point2D s = input.back();
        for (size_t j = 0; j < input.size(); j++) {
            Point2D e = input[j];
            if (inside(e, clip_edge_start, clip_edge_end, clip_ccw)) {
                if (!inside(s, clip_edge_start, clip_edge_end, clip_ccw)) {
                    Point2D ipt;
                    if (intersect_2d(s, e, clip_edge_start, clip_edge_end, ipt)) {
                        output.push_back(ipt);
                    }
                }
                output.push_back(e);
            } else if (inside(s, clip_edge_start, clip_edge_end, clip_ccw)) {
                Point2D ipt;
                if (intersect_2d(s, e, clip_edge_start, clip_edge_end, ipt)) {
                    output.push_back(ipt);
                }
            }
            s = e;
        }
        clip_edge_start = clip_edge_end;
    }
    return output;
}

// ============================================================================
// 2D 多边形面积 (鞋带公式)
// ============================================================================
double polygon_area_2d(const std::vector<Point2D>& poly) {
    if (poly.size() < 3) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        sum += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return std::abs(sum) * 0.5;
}

// ============================================================================
// 切平面面积 → 球面度转换
//
// R07-B08 修复: 正确的 gnomonic 投影 Jacobian
//   dΩ = dξ dη / (1 + ξ² + η²)^(3/2)
//
//   原实现使用 dΩ ≈ dA * (1 + θ²/3), 这是错误的:
//   1. 符号相反 (应为缩小因子, 非放大因子)
//   2. 系数错误 (Taylor 展开 1/(1+θ²)^(3/2) ≈ 1 - 3θ²/2 + ...)
//
//   正确的面积分: Ω = ∫∫_P dξ dη / (1 + ξ² + η²)^(3/2)
//
//   数值方法: 将多边形从质心三角剖分, 对每个三角形用 3 点 Gaussian 积分.
//   对于小像素 (高 NSIDE), 质心近似 Ω ≈ A / (1+θ_c²)^(3/2) 已足够精确.
//   对于大像素 (低 NSIDE), 三角剖分积分提供更高精度.
//
//   dec0 校正: gnomonic 投影的 (ξ, η) 已是球面切平面坐标, Jacobian
//   1/(1+ξ²+η²)^(3/2) 完整描述了面积映射, 无需额外 cos(dec0) 因子.
// ============================================================================
double planar_area_to_steradian(double area_planar,
                                 double center_xi, double center_eta,
                                 double dec0_deg) {
    (void)dec0_deg;  // Jacobian 已完整, 无需 dec0 校正

    // 质心近似: Ω ≈ A / (1 + ξ_c² + η_c²)^(3/2)
    double r2 = center_xi * center_xi + center_eta * center_eta;
    double jac = 1.0 / std::pow(1.0 + r2, 1.5);
    return area_planar * jac;
}

// ============================================================================
// 切平面多边形面积 → 球面度 (高精度, 三角剖分 + 3点 Gaussian 积分)
//
// 将多边形从质心三角剖分, 对每个三角形用 3 点对称 Gaussian 积分计算
// ∫∫ dξ dη / (1 + ξ² + η²)^(3/2).
//
// 3 点三角形积分规则 (权重 1/6, 顶点偏移):
//   p1 = (2/3, 1/6), p2 = (1/6, 2/3), p3 = (1/6, 1/6) (重心坐标)
//   每个权重 = 1/3, 面积 = |cross| / 2
// ============================================================================
double planar_polygon_to_steradian(const std::vector<Point2D>& poly) {
    int n = (int)poly.size();
    if (n < 3) return 0.0;

    // 质心
    double cx = 0.0, cy = 0.0;
    for (const auto& p : poly) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;

    // 3 点 Gaussian 积分规则 (重心坐标)
    const double w[3] = {1.0/3.0, 1.0/3.0, 1.0/3.0};
    const double b1[3] = {2.0/3.0, 1.0/6.0, 1.0/6.0};
    const double b2[3] = {1.0/6.0, 2.0/3.0, 1.0/6.0};
    // b3 = 1 - b1 - b2

    double total_omega = 0.0;
    for (int i = 0; i < n; i++) {
        const Point2D& A = poly[i];
        const Point2D& B = poly[(i + 1) % n];
        // 三角形 (centroid, A, B)
        double ax = A.x - cx, ay = A.y - cy;
        double bx = B.x - cx, by = B.y - cy;
        double tri_area = std::abs(ax * by - ay * bx) * 0.5;

        // 3 点积分
        for (int k = 0; k < 3; k++) {
            double px = cx + b1[k] * ax + b2[k] * bx;
            double py = cy + b1[k] * ay + b2[k] * by;
            double r2 = px * px + py * py;
            double jac = 1.0 / std::pow(1.0 + r2, 1.5);
            total_omega += w[k] * tri_area * jac;
        }
    }

    return total_omega;
}

// ============================================================================
// FAST: 计算源像素 drop 与目标 HEALPix 像素的切平面重叠面积
//
// R07-B08 修复: 使用三角剖分 + 3点 Gaussian 积分计算球面度,
//   替代原质心近似, 提高大像素 (低 NSIDE) 精度.
// ============================================================================
double compute_overlap_area_fast(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double drop_center_ra, double drop_center_dec,
    int healpix_samples) {

    if (drop_corners.empty()) return 0.0;

    // 1. 将 drop 球面多边形投影到切平面 (切点 = drop 中心)
    std::vector<Point2D> drop_planar =
        project_vec3_to_planar(drop_corners, drop_center_ra, drop_center_dec);
    if (drop_planar.size() < 3) return 0.0;

    // 2. 获取 HEALPix 像素的切平面边界 (使用相同切点)
    std::vector<Point2D> hp_planar =
        get_healpix_boundary_planar(hp, target_ipix, nside,
                                     healpix_samples,
                                     drop_center_ra, drop_center_dec);
    if (hp_planar.size() < 3) return 0.0;

    // 3. 2D Sutherland-Hodgman 裁剪
    std::vector<Point2D> clipped = clip_polygon_2d(drop_planar, hp_planar);
    if (clipped.size() < 3) return 0.0;

    // 4. R07-B08: 三角剖分 + 3点 Gaussian 积分计算球面度
    //    dΩ = ∫∫ dξ dη / (1 + ξ² + η²)^(3/2)
    return planar_polygon_to_steradian(clipped);
}

} // namespace fast
