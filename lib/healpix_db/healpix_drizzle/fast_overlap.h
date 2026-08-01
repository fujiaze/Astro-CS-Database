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
// 切平面面积 → 球面度转换
//
// 对于以 (ra0, dec0) 为中心的切平面, 面元 dA_planar 对应的球面度为:
//   dOmega = dA_planar * cos²(theta) / cos(dec0)
// 其中 theta 是切点到面元的角距离.
//
// 但对于小区域 (高 NSIDE), cos²(theta) ≈ 1, cos(dec0) 的影响已被投影包含.
// 简化: dOmega ≈ dA_planar (当区域远小于 1 弧度时)
//
// 精确转换: 对多边形质心处的 cos 因子进行校正
//   Omega ≈ A_planar * cos_correction(center_theta)
//   cos_correction = 1 / (1 + (xi² + eta²)/3)  (二阶近似)
//
// 返回: 校正后的球面度
// ============================================================================
double planar_area_to_steradian(double area_planar,
                                 double center_xi, double center_eta,
                                 double dec0_deg);

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
