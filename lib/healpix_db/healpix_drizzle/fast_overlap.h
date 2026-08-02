#ifndef FAST_OVERLAP_H
#define FAST_OVERLAP_H

// ============================================================================
// FAST Drizzle 重叠计算 (实验性, R04 FAST_DRIZZLE_CPP_PLAN)
//
// 与 PRECISE (spherical_overlap.h) 的区别:
//   PRECISE: 球面 Sutherland-Hodgman 裁剪 + Girard 定理面积 (高精度, 慢)
//   FAST:    切平面 gnomonic 投影 + 2D Sutherland-Hodgman 裁剪 + 鞋带公式 (快, 小误差)
//
// 允许的近似:
//   - 源像素/HEALPix 局部边界投影到切平面 (小像素时误差可忽略)
//   - 2D 多边形面积近似球面面积
//
// 不得近似 (与 PRECISE 共享):
//   - WCS/SIP 解算 (使用相同 WcsSip)
//   - NSIDE/NESTED/ICRS (使用相同 HealpixCore)
//   - pixfrac 语义
//   - 候选像素召回 (使用相同 query_candidate_pixels)
//   - 总通量守恒 (signal = flux * area_ratio)
//   - signal/support 类型
//
// 精度特征:
//   - 高 NSIDE (小像素): 误差 < 0.01%
//   - 低 NSIDE (大像素) / 极区: 误差增大 (这是 FAST 的已知 tradeoff)
//   - 加速比: 预期 3-10x (取决于候选数和边界采样数)
// ============================================================================

#include "healpix_core.h"
#include "spherical_overlap.h"   // 复用 Vec3, PixelToSkyFn, query_candidate_pixels
#include <vector>
#include <cstdint>

namespace fast {

// ============================================================================
// 2D 点 (切平面坐标)
// ============================================================================
struct Point2D {
    double x, y;
};

// ============================================================================
// Gnomonic 投影: 球面坐标 → 切平面坐标
//
// 以 (ra0, dec0) 为切点, 将 (ra, dec) 投影到切平面 (xi, eta).
// xi 指向东方, eta 指向北方.
//
// 公式 (Calabretta & Greisen 2002, gnomonic = TAN 投影):
//   cos(c) = sin(dec0)*sin(dec) + cos(dec0)*cos(dec)*dRa
//   xi  = cos(dec)*sin(dRa) / cos(c)
//   eta = (cos(dec0)*sin(dec) - sin(dec0)*cos(dec)*cos(dRa)) / cos(c)
//   其中 dRa = ra - ra0 (弧度)
//
// 返回 true 成功, false 表示点在投影背面 (cos(c) <= 0)
// ============================================================================
bool gnomonic_forward(double ra_deg, double dec_deg,
                      double ra0_deg, double dec0_deg,
                      double& xi, double& eta);

// ============================================================================
// 构造源像素 drop 的切平面 2D 多边形
//
// 与 spherical::build_drop_polygon_sampled 共享 WCS/SIP 和 pixfrac 语义,
// 但将球面顶点投影到以 drop 中心为切点的切平面.
//
// px, py: 像素中心 (0-based)
// pixfrac: 收缩因子 (0, 1]
// pixelToSky: 像素→天球 回调 (与 PRECISE 相同)
// user_data: 回调不透明指针
// samples_per_edge: 每条边采样段数 (FAST 建议 2-4, PRECISE 用 8+)
// center_ra, center_dec: 输出切点坐标 (用于后续投影一致性)
// 返回: 2D 多边形顶点 (逆时针). 投影失败时返回空向量.
// ============================================================================
std::vector<Point2D> build_drop_polygon_planar(
    double px, double py, double pixfrac,
    spherical::PixelToSkyFn pixelToSky, void* user_data,
    int samples_per_edge,
    double& center_ra, double& center_dec);

// ============================================================================
// 获取 HEALPix 像素的切平面 2D 边界
//
// 将 HEALPix 像素的球面边界 (4 角或采样后顶点) 投影到以 (ra0, dec0) 为切点的切平面.
// 使用与 PRECISE 相同的 get_healpix_boundary_sampled 获取球面边界,
// 然后投影到切平面.
//
// hp: HEALPix 核心
// ipix: 像素 NESTED 索引
// nside: NSIDE
// samples_per_edge: 边界采样段数 (FAST 建议 1-2, PRECISE 用 8+)
// ra0, dec0: 切点坐标 (度)
// 返回: 2D 多边形顶点 (逆时针). 投影失败时返回空向量.
// ============================================================================
std::vector<Point2D> get_healpix_boundary_planar(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge,
    double ra0, double dec0);

// ============================================================================
// 2D Sutherland-Hodgman 多边形裁剪
//
// subject: 被裁剪多边形 (drop 的切平面投影)
// clip: 裁剪多边形 (HEALPix 像素的切平面投影, 凸多边形)
// 返回: 裁剪后的 2D 多边形顶点. 不相交返回空向量.
// ============================================================================
std::vector<Point2D> clip_polygon_2d(
    const std::vector<Point2D>& subject,
    const std::vector<Point2D>& clip);

// ============================================================================
// 2D 多边形面积 (鞋带公式 / Shoelace formula)
//
// 返回: 多边形面积 (切平面坐标系的面积单位, 非球面度)
// 顶点必须按逆时针或顺时针排列. 返回绝对值.
// ============================================================================
double polygon_area_2d(const std::vector<Point2D>& poly);

// ============================================================================
// 切平面面积 → 球面度转换 (质心近似)
//
// R07-B08 修复: 正确的 gnomonic 投影 Jacobian
//   dΩ = dξ dη / (1 + ξ² + η²)^(3/2)
//   质心近似: Ω ≈ A / (1 + ξ_c² + η_c²)^(3/2)
//
// 适用于小像素 (高 NSIDE). 大像素请用 planar_polygon_to_steradian.
// ============================================================================
double planar_area_to_steradian(double area_planar,
                                 double center_xi, double center_eta,
                                 double dec0_deg);

// ============================================================================
// 切平面多边形面积 → 球面度 (高精度, 三角剖分 + 3点 Gaussian 积分)
//
// 将多边形从质心三角剖分, 对每个三角形用 3 点对称 Gaussian 积分计算
// ∫∫ dξ dη / (1 + ξ² + η²)^(3/2).
// ============================================================================
double planar_polygon_to_steradian(const std::vector<Point2D>& poly);

// ============================================================================
// FAST: 计算源像素 drop 与目标 HEALPix 像素的切平面重叠面积
//
// 与 spherical::compute_overlap_area 的区别:
//   PRECISE: 球面裁剪 + Girard → 球面度
//   FAST: 切平面投影 + 2D 裁剪 + 鞋带 → 球面度 (近似)
//
// drop_corners: drop 球面多边形顶点 (与 PRECISE 共享, 来自 build_drop_polygon_sampled)
// hp: HEALPix 核心
// target_ipix: 目标像素 NESTED 索引
// nside: NSIDE
// drop_center_ra, drop_center_dec: drop 中心天球坐标 (作为切点)
// healpix_samples: HEALPix 边界采样段数 (FAST 建议 1-2)
// 返回: 重叠面积 (球面度, steradian). 不相交返回 0.
// ============================================================================
double compute_overlap_area_fast(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix, int nside,
    double drop_center_ra, double drop_center_dec,
    int healpix_samples);

} // namespace fast

#endif // FAST_OVERLAP_H
