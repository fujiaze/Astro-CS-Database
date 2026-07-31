#ifndef SPHERICAL_OVERLAP_H
#define SPHERICAL_OVERLAP_H

// ============================================================================
// 球面 HEALPix 重叠计算模块 (WP-D 步骤3-4)
//
// 用途:
//   替换 drizzle_engine.cpp 中的局部切平面近似 + 人工 HEALPix 菱形近似,
//   实现真实球面多边形裁剪与球面面积计算, 修复极区/大视场/高 WCS 畸变误差.
//
// 核心算法:
//   - 球面向量 (Vec3) 与基本运算 (cross/dot/normalize)
//   - 球面多边形面积 (Girard 定理: 面积 = Σ内角 - (n-2)π)
//   - 球面 Sutherland-Hodgman 多边形裁剪 (大圆弧裁剪, 法向量定义保留侧)
//   - HEALPix 像素球面边界获取 (4 个角顶点)
//   - 源像素 drop 与目标 HEALPix 像素的球面重叠面积
//   - 候选像素查询 (基于 drop 多边形球面包围盒, 不限于 1-ring)
//
// 精度: float64 内部精度, 球面面积误差 < 1e-6 球面度
//
// 参考:
//   - Gorski et al. 2005, HEALPix Framework
//   - Chamberlain & Duquette 2007, 球面多边形面积算法
//   - Sutherland & Hodgman 1974, 多边形裁剪
// ============================================================================

#include "healpix_core.h"
#include <vector>
#include <cstdint>

namespace spherical {

// ============================================================================
// 球面向量 (单位向量)
// ============================================================================
struct Vec3 {
    double x, y, z;
};

// ---- 基本向量运算 ----
Vec3   cross(const Vec3& a, const Vec3& b);
double dot(const Vec3& a, const Vec3& b);
Vec3   normalize(const Vec3& v);
double length(const Vec3& v);

// 球面坐标(度) → 笛卡尔单位向量
// ra_deg: 赤经 [0, 360), dec_deg: 赤纬 [-90, +90]
Vec3 radec_to_vec(double ra_deg, double dec_deg);

// 笛卡尔向量 → 球面坐标(度)
// 返回 ra_deg ∈ [0, 360), dec_deg ∈ [-90, +90]
void vec_to_radec(const Vec3& v, double& ra_deg, double& dec_deg);

// 两单位向量的角距离(弧度)
double angular_distance(const Vec3& a, const Vec3& b);

// ============================================================================
// 球面多边形面积 (Girard 定理 / 球面 excess 公式)
//
// 公式: Area = Σ内角 - (n-2)π
// 输入: vertices 按顺序排列的球面顶点 (单位向量, 逆时针或顺时针)
// 输出: 球面面积 (球面度, steradian). 自动取绝对值, 不依赖顶点方向.
//
// 精度: float64, 已知球面多边形面积误差 < 1e-10
// ============================================================================
double spherical_polygon_area(const std::vector<Vec3>& vertices);

// ============================================================================
// 球面 Sutherland-Hodgman 多边形裁剪
//
// 用一组大圆弧裁剪球面多边形:
//   - subject: 被裁剪的球面多边形 (单位向量, 任意方向)
//   - clip_plane_normals: 裁剪平面法向量列表 (每个定义一个大圆, 保留 dot(n, v) > 0 一侧)
// 返回: 裁剪后的球面多边形顶点 (单位向量)
//
// 算法: 经典 Sutherland-Hodgman 推广到球面
//   - 边 (S→E) 为大圆弧, 法向量 = cross(S, E)
//   - 与裁剪大圆 (法向量 n) 的交点 = ±normalize(cross(n_edge, n_clip))
//   - 选择位于 S, E 之间的交点 (dot(intersection, S+E) > 0)
// ============================================================================
std::vector<Vec3> sutherland_hodgman_spherical(
    const std::vector<Vec3>& subject,
    const std::vector<Vec3>& clip_plane_normals);

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点
//
// 4 个角顶点 (单位向量), 顺序为 (x,y)→(x+1,y)→(x+1,y+1)→(x,y+1) 的逆时针环绕
// 注: HEALPix 像素在赤道带为菱形, 在极区为三角形 (一角可能退化, 但仍返回 4 个顶点)
//
// hp: HEALPix 核心
// ipix: 像素 NESTED 索引
// nside: NSIDE (冗余参数, 与 hp.getNside() 一致, 保留接口一致性)
// ============================================================================
std::vector<Vec3> get_healpix_boundary(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside);

// ============================================================================
// 计算源像素 drop 与目标 HEALPix 像素的球面重叠面积
//
// drop_corners: drop 球面多边形顶点 (已通过 WCS/SIP 映射到球面, 单位向量)
// hp: HEALPix 核心
// target_ipix: 目标像素 NESTED 索引
// 返回: 球面重叠面积 (球面度, steradian). 不相交返回 0.
//
// 实现:
//   1. 获取目标 HEALPix 像素 4 个角顶点 (单位向量)
//   2. 构造 4 个裁剪平面 (HEALPix 边的大圆法向量, 指向像素内部)
//   3. 用球面 Sutherland-Hodgman 裁剪 drop 多边形
//   4. 用 Girard 定理计算交集面积
// ============================================================================
double compute_overlap_area(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix);

// ============================================================================
// 查询与 drop 多边形可能相交的所有 HEALPix 像素 (不限于 1-ring)
//
// drop_corners: drop 球面多边形顶点
// hp: HEALPix 核心
// candidates: 输出候选像素列表 (NESTED ipix, 已去重排序)
//
// 实现:
//   1. 计算 drop 多边形的球面包围圆 (中心向量 + 最大角半径)
//   2. 加上 1 个 HEALPix 像素分辨率作为缓冲 (避免边缘漏选)
//   3. 用 hp.queryDisc 查询圆盘内所有像素
//   4. 高 NSIDE + 大源像素时, 候选数可远 > 48
// ============================================================================
void query_candidate_pixels(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates);

} // namespace spherical

#endif // SPHERICAL_OVERLAP_H
