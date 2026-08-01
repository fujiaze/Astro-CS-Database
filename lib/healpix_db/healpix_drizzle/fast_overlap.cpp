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
// ============================================================================
static std::vector<Point2D> project_vec3_to_planar(
    const std::vector<spherical::Vec3>& verts,
    double ra0_deg, double dec0_deg) {
    std::vector<Point2D> result;
    result.reserve(verts.size());
    for (const auto& v : verts) {
        double ra, dec;
        vec3_to_radec(v, ra, dec);
        double xi, eta;
        if (gnomonic_forward(ra, dec, ra0_deg, dec0_deg, xi, eta)) {
            result.push_back({xi, eta});
        }
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
// 2D Sutherland-Hodgman 多边形裁剪
// ============================================================================
static bool inside(const Point2D& p, const Point2D& edge_start, const Point2D& edge_end) {
    // 判断点在边的哪一侧 (逆时针多边形, 内侧 = 左侧)
    double cross = (edge_end.x - edge_start.x) * (p.y - edge_start.y) -
                   (edge_end.y - edge_start.y) * (p.x - edge_start.x);
    return cross >= 0;
}

static Point2D intersect(const Point2D& s, const Point2D& e,
                         const Point2D& clip_s, const Point2D& clip_e) {
    // 计算线段 (s→e) 与裁剪边 (clip_s→clip_e) 的交点
    double dx1 = e.x - s.x, dy1 = e.y - s.y;
    double dx2 = clip_e.x - clip_s.x, dy2 = clip_e.y - clip_s.y;
    double denom = dx1 * dy2 - dy1 * dx2;
    if (std::abs(denom) < 1e-30) return s;  // 平行, 返回起点

    double t = ((clip_s.x - s.x) * dy2 - (clip_s.y - s.y) * dx2) / denom;
    return {s.x + t * dx1, s.y + t * dy1};
}

std::vector<Point2D> clip_polygon_2d(
    const std::vector<Point2D>& subject,
    const std::vector<Point2D>& clip) {
    if (subject.empty() || clip.empty()) return {};

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
            if (inside(e, clip_edge_start, clip_edge_end)) {
                if (!inside(s, clip_edge_start, clip_edge_end)) {
                    output.push_back(intersect(s, e, clip_edge_start, clip_edge_end));
                }
                output.push_back(e);
            } else if (inside(s, clip_edge_start, clip_edge_end)) {
                output.push_back(intersect(s, e, clip_edge_start, clip_edge_end));
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
// ============================================================================
double planar_area_to_steradian(double area_planar,
                                 double center_xi, double center_eta,
                                 double dec0_deg) {
    // 切平面上面元的角距: theta = sqrt(xi² + eta²)
    // 球面度校正因子: 1/cos²(theta) (切平面在边缘拉伸)
    // 对于小区域 (theta << 1 rad), 校正 ≈ 1
    // 二阶近似: cos²(theta) ≈ 1 - theta²
    // 面积校正: dOmega ≈ dA_planar * (1 + theta²/3) (面积分均值)

    double theta2 = center_xi * center_xi + center_eta * center_eta;
    double correction = 1.0 + theta2 / 3.0;

    // dec0 校正: 切平面 xi 方向对应 RA 差, 需要乘 cos(dec0)
    // 但 gnomonic 投影已将 RA 差转换为 xi = cos(dec)*sin(dRa)/cos(c),
    // 所以 xi 已包含 cos(dec) 因子, 无需额外校正
    // (与 TAN 投影的 CD 矩阵语义一致)

    return area_planar * correction;
}

// ============================================================================
// FAST: 计算源像素 drop 与目标 HEALPix 像素的切平面重叠面积
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

    // 4. 鞋带公式计算面积
    double area_planar = polygon_area_2d(clipped);

    // 5. 转换为球面度 (切平面面积 → steradian)
    // 使用裁剪多边形质心作为校正参考点
    double cx = 0.0, cy = 0.0;
    for (const auto& p : clipped) { cx += p.x; cy += p.y; }
    cx /= clipped.size();
    cy /= clipped.size();

    return planar_area_to_steradian(area_planar, cx, cy, drop_center_dec);
}

} // namespace fast
