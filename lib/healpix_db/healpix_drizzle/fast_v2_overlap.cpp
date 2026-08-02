// ============================================================================
// fast_v2_overlap.cpp - FAST v2 Drizzle 重叠计算实现 (R08 实验, 纯 C++)
//
// 实现见 fast_v2_overlap.h 注释.
//
// 关键设计:
//   1. 路径决策 (decide_path):
//      - 极区 (|dec| > 88°)              → PRECISE_FALLBACK
//      - 候选数 > 8                       → PRECISE_FALLBACK
//      - SIP 阶数 > 2                     → PRECISE_FALLBACK
//      - drop 跨越 base face 边界          → PRECISE_FALLBACK
//      - 其他 (常见路径)                   → COMMON_EXACT
//
//   2. 拓扑分类 (classify_topology):
//      - drop 4 角均在目标像素内 → TOPO_DROP_IN_PIXEL (直接返回 drop 面积)
//      - 目标像素 4 角均在 drop 内 → TOPO_PIXEL_IN_DROP (返回解析面积 π/(3·NSIDE²))
//      - 候选数 2/3/4 → TOPO_2/3/4_PIXEL_OVERLAP (切平面 2D 裁剪)
//      - 否则 → TOPO_COMPLEX (切平面 2D 裁剪)
//
//   3. COMMON_EXACT 面积计算:
//      - TOPO_DROP_IN_PIXEL: spherical::spherical_polygon_area(drop_corners)
//      - TOPO_PIXEL_IN_DROP: π/(3·NSIDE²) (HEALPix 解析面积)
//      - 其他: 切平面 2D Sutherland-Hodgman 裁剪 + 三角剖分 Gaussian 积分
//              (复用 fast v1 的 planar_polygon_to_steradian 算法)
//
//   4. PRECISE_FALLBACK:
//      - 直接调用 spherical::compute_overlap_area(drop_corners, hp, target_ipix)
// ============================================================================

#include "fast_v2_overlap.h"
#include "spherical_overlap.h"
#include "healpix_core.h"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fast_v2 {

// ============================================================================
// 常量
// ============================================================================
static const double D2R = 0.017453292519943295769;
static const double R2D = 57.2957795130823208768;

// 路径决策阈值
static const double POLAR_DEC_THRESHOLD_DEG = 88.0;   // |dec| > 88° → 极区
// R08 注: query_candidate_pixels 使用 3×hp_res 保守缓冲保证零漏选,
//   导致候选数总是远大于实际重叠像素数 (例如 NSIDE=4M + 0.1"/pix 时候选数 ≈ 56,
//   但实际非零重叠仅 12). 若严格按设计文档"候选数 > 8 → PRECISE_FALLBACK",
//   COMMON_EXACT 路径几乎永不触发, FAST v2 退化为 PRECISE.
//   实验阶段放宽到 64, 让 COMMON_EXACT 路径覆盖大多数正常场景,
//   PRECISE_FALLBACK 仅用于真正病态 (极区 / base face 边界 / 强 SIP / 候选数极多).
static const int    MAX_CANDIDATES_COMMON   = 64;
static const int    MAX_SIP_ORDER_COMMON    = 2;       // SIP 阶数 > 2 → PRECISE

// ============================================================================
// 2D 点 (切平面坐标)
// ============================================================================
struct Point2D {
    double x, y;
};

// ============================================================================
// Gnomonic 投影: 球面坐标 → 切平面坐标 (与 fast v1 一致)
// ============================================================================
static bool gnomonic_forward(double ra_deg, double dec_deg,
                             double ra0_deg, double dec0_deg,
                             double& xi, double& eta) {
    double ra   = ra_deg  * D2R;
    double dec  = dec_deg * D2R;
    double ra0  = ra0_deg * D2R;
    double dec0 = dec0_deg * D2R;

    double dRa = ra - ra0;
    while (dRa >  M_PI) dRa -= 2.0 * M_PI;
    while (dRa < -M_PI) dRa += 2.0 * M_PI;

    double cos_dec  = std::cos(dec);
    double sin_dec  = std::sin(dec);
    double cos_dec0 = std::cos(dec0);
    double sin_dec0 = std::sin(dec0);

    double cos_c = sin_dec0 * sin_dec + cos_dec0 * cos_dec * std::cos(dRa);
    if (cos_c <= 1e-12) return false;  // 投影背面

    xi  = cos_dec * std::sin(dRa) / cos_c;
    eta = (cos_dec0 * sin_dec - sin_dec0 * cos_dec * std::cos(dRa)) / cos_c;
    return true;
}

// ============================================================================
// 球面 Vec3 → 天球坐标(度)
// ============================================================================
static void vec3_to_radec(const spherical::Vec3& v, double& ra_deg, double& dec_deg) {
    double x = v.x, y = v.y, z = v.z;
    double r = std::sqrt(x * x + y * y + z * z);
    if (r < 1e-30) { ra_deg = 0; dec_deg = 0; return; }
    dec_deg = std::asin(std::max(-1.0, std::min(1.0, z / r))) * R2D;
    ra_deg  = std::atan2(y, x) * R2D;
    if (ra_deg < 0.0) ra_deg += 360.0;
}

// ============================================================================
// 将球面 Vec3 多边形投影到切平面 (任一顶点失败 → 整多边形失败)
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
            return {};
        }
        result.push_back({xi, eta});
    }
    return result;
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
// 2D Sutherland-Hodgman 多边形裁剪 (与 fast v1 一致, 含 R07-M01/M03 修复)
// ============================================================================
static bool inside_2d(const Point2D& p, const Point2D& es, const Point2D& ee,
                      bool clip_ccw) {
    double cross = (ee.x - es.x) * (p.y - es.y) - (ee.y - es.y) * (p.x - es.x);
    return clip_ccw ? (cross >= 0) : (cross <= 0);
}

static bool intersect_2d(const Point2D& s, const Point2D& e,
                         const Point2D& cs, const Point2D& ce, Point2D& out) {
    double dx1 = e.x - s.x, dy1 = e.y - s.y;
    double dx2 = ce.x - cs.x, dy2 = ce.y - cs.y;
    double denom = dx1 * dy2 - dy1 * dx2;
    if (std::abs(denom) < 1e-30) return false;
    double t = ((cs.x - s.x) * dy2 - (cs.y - s.y) * dx2) / denom;
    if (t < -1e-12 || t > 1.0 + 1e-12) return false;
    out = {s.x + t * dx1, s.y + t * dy1};
    return true;
}

static std::vector<Point2D> clip_polygon_2d(
    const std::vector<Point2D>& subject,
    const std::vector<Point2D>& clip) {
    if (subject.size() < 3 || clip.size() < 3) return {};

    bool clip_ccw = (signed_area_2d(clip) > 0.0);

    std::vector<Point2D> output = subject;
    Point2D ces = clip.back();

    for (size_t i = 0; i < clip.size(); i++) {
        Point2D cee = clip[i];
        std::vector<Point2D> input = std::move(output);
        output.clear();
        if (input.empty()) break;

        Point2D s = input.back();
        for (size_t j = 0; j < input.size(); j++) {
            Point2D e = input[j];
            if (inside_2d(e, ces, cee, clip_ccw)) {
                if (!inside_2d(s, ces, cee, clip_ccw)) {
                    Point2D ipt;
                    if (intersect_2d(s, e, ces, cee, ipt)) output.push_back(ipt);
                }
                output.push_back(e);
            } else if (inside_2d(s, ces, cee, clip_ccw)) {
                Point2D ipt;
                if (intersect_2d(s, e, ces, cee, ipt)) output.push_back(ipt);
            }
            s = e;
        }
        ces = cee;
    }
    return output;
}

// ============================================================================
// 切平面多边形面积 → 球面度 (三角剖分 + 3点 Gaussian 积分)
// 与 fast v1 planar_polygon_to_steradian 一致.
// dΩ = ∫∫ dξ dη / (1 + ξ² + η²)^(3/2)
// ============================================================================
static double planar_polygon_to_steradian(const std::vector<Point2D>& poly) {
    int n = (int)poly.size();
    if (n < 3) return 0.0;

    double cx = 0.0, cy = 0.0;
    for (const auto& p : poly) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;

    const double w[3]  = {1.0/3.0, 1.0/3.0, 1.0/3.0};
    const double b1[3] = {2.0/3.0, 1.0/6.0, 1.0/6.0};
    const double b2[3] = {1.0/6.0, 2.0/3.0, 1.0/6.0};

    double total_omega = 0.0;
    for (int i = 0; i < n; i++) {
        const Point2D& A = poly[i];
        const Point2D& B = poly[(i + 1) % n];
        double ax = A.x - cx, ay = A.y - cy;
        double bx = B.x - cx, by = B.y - cy;
        double tri_area = std::fabs(ax * by - ay * bx) * 0.5;
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
// 获取 HEALPix 像素的切平面 2D 边界
// ============================================================================
static std::vector<Point2D> get_healpix_boundary_planar(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge, double ra0, double dec0) {

    std::vector<spherical::Vec3> boundary_spherical =
        spherical::get_healpix_boundary_sampled(hp, ipix, nside, samples_per_edge);
    if (boundary_spherical.empty()) return {};

    return project_vec3_to_planar(boundary_spherical, ra0, dec0);
}

// ============================================================================
// 辅助: drop 包含像素 4 角检测
// 检查目标像素的所有 4 个角点是否都在 drop 多边形内部 (球面半空间法).
// ============================================================================
static bool pixel_fully_in_drop(
    const std::vector<spherical::Vec3>& drop_corners,
    const std::vector<spherical::Vec3>& drop_clip_normals,
    const spherical::Vec3& drop_centroid,
    const healpix::HealpixCore& hp, uint64_t ipix) {

    // 像素 4 角顶点 (无采样, 仅 4 角)
    std::vector<spherical::Vec3> hp_corners =
        spherical::get_healpix_boundary(hp, ipix, hp.getNside());
    if (hp_corners.size() < 3) return false;

    // drop 裁剪法向量 (已归一化使质心在正侧)
    // 调用方应已构造, 此处防御性重建
    std::vector<spherical::Vec3> normals = drop_clip_normals;
    if (normals.empty()) {
        int nd = (int)drop_corners.size();
        normals.reserve(nd);
        for (int j = 0; j < nd; j++) {
            const spherical::Vec3& P1 = drop_corners[j];
            const spherical::Vec3& P2 = drop_corners[(j + 1) % nd];
            spherical::Vec3 n = spherical::cross(P1, P2);
            if (spherical::dot(n, drop_centroid) < 0.0) {
                n.x = -n.x; n.y = -n.y; n.z = -n.z;
            }
            normals.push_back(spherical::normalize(n));
        }
    }

    // 所有像素角点均在 drop 内 → 像素完全被 drop 包含
    for (const auto& v : hp_corners) {
        for (const auto& n : normals) {
            if (spherical::dot(v, n) < -1e-12) {
                return false;
            }
        }
    }
    return true;
}

// ============================================================================
// 辅助: drop 完全落入目标像素检测
// 检查 drop 的所有顶点是否都在目标像素内部 (球面半空间法).
// 目标像素的 4 个裁剪法向量 = normalize(cross(B_i, B_{i+1})), 方向使像素中心在正侧.
// ============================================================================
static bool drop_fully_in_pixel(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t ipix) {

    std::vector<spherical::Vec3> hp_boundary =
        spherical::get_healpix_boundary(hp, ipix, hp.getNside());
    if (hp_boundary.size() < 3) return false;

    // 像素中心 (用于定向法向量)
    double ra_c, dec_c;
    hp.pix2radec((int64_t)ipix, &ra_c, &dec_c);
    spherical::Vec3 hp_center = spherical::radec_to_vec(ra_c, dec_c);

    int nb = (int)hp_boundary.size();
    std::vector<spherical::Vec3> clip_normals;
    clip_normals.reserve(nb);
    for (int j = 0; j < nb; j++) {
        const spherical::Vec3& P1 = hp_boundary[j];
        const spherical::Vec3& P2 = hp_boundary[(j + 1) % nb];
        spherical::Vec3 n = spherical::cross(P1, P2);
        if (spherical::dot(n, hp_center) < 0.0) {
            n.x = -n.x; n.y = -n.y; n.z = -n.z;
        }
        clip_normals.push_back(spherical::normalize(n));
    }

    // drop 所有顶点均在像素内 → drop 完全落入像素
    for (const auto& v : drop_corners) {
        for (const auto& n : clip_normals) {
            if (spherical::dot(v, n) < -1e-12) {
                return false;
            }
        }
    }
    return true;
}

// ============================================================================
// 辅助: 计算 drop 多边形所有顶点所在的 base face 集合
//   base_face = radec2pix(ra, dec) / (nside²) (NESTED 编码下)
// 用于判断 drop 是否跨越 base face 边界.
// ============================================================================
static bool drop_crosses_base_face(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, int nside) {

    int64_t npix_per_bighp = (int64_t)nside * nside;
    int first_bighp = -1;

    for (const auto& v : drop_corners) {
        double ra, dec;
        spherical::vec_to_radec(v, ra, dec);
        int64_t ipix = hp.radec2pix(ra, dec);
        int bighp = (int)(ipix / npix_per_bighp);
        if (first_bighp < 0) {
            first_bighp = bighp;
        } else if (bighp != first_bighp) {
            return true;  // 跨越 base face
        }
    }
    return false;
}

// ============================================================================
// 路径决策
// ============================================================================
FastV2Diagnostics::Path decide_path(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    int nside,
    double center_ra, double center_dec,
    int sip_order,
    int candidate_count) {

    // 1. 极区检测
    if (std::fabs(center_dec) > POLAR_DEC_THRESHOLD_DEG) {
        return FastV2Diagnostics::PRECISE_FALLBACK;
    }

    // 2. SIP 阶数检测
    if (sip_order > MAX_SIP_ORDER_COMMON) {
        return FastV2Diagnostics::PRECISE_FALLBACK;
    }

    // 3. 候选数检测
    if (candidate_count > MAX_CANDIDATES_COMMON) {
        return FastV2Diagnostics::PRECISE_FALLBACK;
    }

    // 4. drop 跨 base face 边界检测
    if (drop_crosses_base_face(drop_corners, hp, nside)) {
        return FastV2Diagnostics::PRECISE_FALLBACK;
    }

    return FastV2Diagnostics::COMMON_EXACT;
}

// ============================================================================
// 拓扑分类
// ============================================================================
FastV2Diagnostics::Topology classify_topology(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix, int nside,
    const std::vector<uint64_t>& all_candidates) {

    // 1. drop 完全落入目标像素
    if (drop_fully_in_pixel(drop_corners, hp, target_ipix)) {
        return FastV2Diagnostics::TOPO_DROP_IN_PIXEL;
    }

    // 2. 目标像素完全包含于 drop
    // 构造 drop 裁剪法向量 (供 pixel_fully_in_drop 复用)
    int nd = (int)drop_corners.size();
    spherical::Vec3 drop_centroid = {0.0, 0.0, 0.0};
    for (const auto& v : drop_corners) {
        drop_centroid.x += v.x; drop_centroid.y += v.y; drop_centroid.z += v.z;
    }
    drop_centroid = spherical::normalize(drop_centroid);

    std::vector<spherical::Vec3> drop_normals;
    drop_normals.reserve(nd);
    for (int j = 0; j < nd; j++) {
        const spherical::Vec3& P1 = drop_corners[j];
        const spherical::Vec3& P2 = drop_corners[(j + 1) % nd];
        spherical::Vec3 n = spherical::cross(P1, P2);
        if (spherical::dot(n, drop_centroid) < 0.0) {
            n.x = -n.x; n.y = -n.y; n.z = -n.z;
        }
        drop_normals.push_back(spherical::normalize(n));
    }

    if (pixel_fully_in_drop(drop_corners, drop_normals, drop_centroid,
                             hp, target_ipix)) {
        return FastV2Diagnostics::TOPO_PIXEL_IN_DROP;
    }

    // 3. 按候选数分类
    int nc = (int)all_candidates.size();
    if (nc == 2) return FastV2Diagnostics::TOPO_2PIXEL_OVERLAP;
    if (nc == 3) return FastV2Diagnostics::TOPO_3PIXEL_OVERLAP;
    if (nc == 4) return FastV2Diagnostics::TOPO_4PIXEL_OVERLAP;

    return FastV2Diagnostics::TOPO_COMPLEX;
}

// ============================================================================
// 主入口: FAST v2 重叠面积计算
// ============================================================================
double compute_overlap_area_v2(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix, int nside,
    double center_ra, double center_dec,
    int healpix_samples,
    FastV2Diagnostics* diag) {

    auto t_start = std::chrono::steady_clock::now();

    if (diag) {
        diag->path_used = FastV2Diagnostics::PRECISE_FALLBACK;
        diag->candidate_count = 0;
        diag->raw_area = 0.0;
        diag->precise_area = 0.0;
        diag->area_rel_err = 0.0;
        diag->compute_time_us = 0.0;
        diag->topology = FastV2Diagnostics::TOPO_UNKNOWN;
    }

    if (drop_corners.size() < 3) {
        return 0.0;
    }

    // ---- 1. 查询候选像素 (与 PRECISE 共享, 保证零漏选) ----
    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop_corners, hp, candidates);

    if (diag) diag->candidate_count = (int)candidates.size();

    // ---- 2. 路径决策 ----
    // 注意: 本函数不知 SIP 阶数, 调用方应在更高层判断 (benchmark 传 0).
    // 这里默认 sip_order = 0 (无 SIP) → 不会因 SIP 触发回退.
    // 极区 / base face / 候选数 仍是主要回退触发条件.
    const int sip_order_assumed = 0;
    FastV2Diagnostics::Path path = decide_path(
        drop_corners, hp, nside, center_ra, center_dec,
        sip_order_assumed, (int)candidates.size());

    // ---- 3. PRECISE_FALLBACK ----
    if (path == FastV2Diagnostics::PRECISE_FALLBACK) {
        double area = spherical::compute_overlap_area(drop_corners, hp, target_ipix);
        if (diag) {
            diag->path_used = FastV2Diagnostics::PRECISE_FALLBACK;
            diag->topology = FastV2Diagnostics::TOPO_PRECISE_FALLBACK;
            diag->raw_area = area;
            diag->precise_area = area;  // 自身即参考
            diag->area_rel_err = 0.0;
            auto t_end = std::chrono::steady_clock::now();
            diag->compute_time_us = std::chrono::duration<double, std::micro>(
                t_end - t_start).count();
        }
        return area;
    }

    // ---- 4. COMMON_EXACT 路径: 拓扑分类 ----
    FastV2Diagnostics::Topology topo = classify_topology(
        drop_corners, hp, target_ipix, nside, candidates);

    if (diag) {
        diag->path_used = FastV2Diagnostics::COMMON_EXACT;
        diag->topology = (int)topo;
    }

    double area = 0.0;

    // ---- 4a. drop 完全落入目标像素 → 返回 drop 球面面积 ----
    if (topo == FastV2Diagnostics::TOPO_DROP_IN_PIXEL) {
        area = spherical::spherical_polygon_area(drop_corners);
    }
    // ---- 4b. 目标像素完全包含于 drop → 解析面积 π/(3·NSIDE²) ----
    else if (topo == FastV2Diagnostics::TOPO_PIXEL_IN_DROP) {
        area = M_PI / (3.0 * (double)nside * (double)nside);
    }
    // ---- 4c. 2/3/4 像素交叠 / 一般复杂交叠 → 切平面 2D 裁剪 ----
    else {
        // 将 drop 球面多边形投影到切平面 (切点 = drop 中心)
        std::vector<Point2D> drop_planar =
            project_vec3_to_planar(drop_corners, center_ra, center_dec);
        if (drop_planar.size() < 3) {
            // 投影失败 → 回退 PRECISE
            area = spherical::compute_overlap_area(drop_corners, hp, target_ipix);
            if (diag) {
                diag->path_used = FastV2Diagnostics::PRECISE_FALLBACK;
                diag->topology = FastV2Diagnostics::TOPO_PRECISE_FALLBACK;
            }
        } else {
            // 获取目标像素切平面边界
            std::vector<Point2D> hp_planar =
                get_healpix_boundary_planar(hp, target_ipix, nside,
                                             healpix_samples,
                                             center_ra, center_dec);
            if (hp_planar.size() < 3) {
                area = spherical::compute_overlap_area(drop_corners, hp, target_ipix);
                if (diag) {
                    diag->path_used = FastV2Diagnostics::PRECISE_FALLBACK;
                    diag->topology = FastV2Diagnostics::TOPO_PRECISE_FALLBACK;
                }
            } else {
                // 2D 裁剪
                std::vector<Point2D> clipped = clip_polygon_2d(drop_planar, hp_planar);
                if (clipped.size() < 3) {
                    area = 0.0;
                } else {
                    // 三角剖分 + 3点 Gaussian 积分 → 球面度
                    area = planar_polygon_to_steradian(clipped);
                    // 防御性: 若面积为负 (浮点误差) 归零
                    if (area < 0.0) area = 0.0;
                }
            }
        }
    }

    if (diag) {
        diag->raw_area = area;
        diag->precise_area = 0.0;  // 默认不计算 PRECISE 参考 (benchmark 单独计算)
        diag->area_rel_err = 0.0;
        auto t_end = std::chrono::steady_clock::now();
        diag->compute_time_us = std::chrono::duration<double, std::micro>(
            t_end - t_start).count();
    }

    return area;
}

} // namespace fast_v2
