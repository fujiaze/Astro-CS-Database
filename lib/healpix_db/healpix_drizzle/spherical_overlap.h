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
// 球面向量 (单位向量) — 模板双实例 (R11 阶段7: 真 FP32/FP64 Scalar 几何)
//   Vec3T<float>  : FP32 科学路径 (IEEE binary32 全链)
//   Vec3T<double> : FP64 科学路径 (IEEE binary64, 与旧 Vec3 一致)
// ============================================================================
template <typename T>
struct Vec3T {
    T x, y, z;
};
using Vec3 = Vec3T<double>;  // 向后兼容别名 (旧调用方仍用 double)

// ---- 基本向量运算 ----
template <typename T>
Vec3T<T> cross(const Vec3T<T>& a, const Vec3T<T>& b);
template <typename T>
T dot(const Vec3T<T>& a, const Vec3T<T>& b);
template <typename T>
Vec3T<T> normalize(const Vec3T<T>& v);
template <typename T>
T length(const Vec3T<T>& v);

// 球面坐标(度) → 笛卡尔单位向量
// ra_deg: 赤经 [0, 360), dec_deg: 赤纬 [-90, +90]
template <typename T>
Vec3T<T> radec_to_vec(T ra_deg, T dec_deg);

// 笛卡尔向量 → 球面坐标(度)
// 返回 ra_deg ∈ [0, 360), dec_deg ∈ [-90, +90]
template <typename T>
void vec_to_radec(const Vec3T<T>& v, T& ra_deg, T& dec_deg);

// 两单位向量的角距离(弧度)
template <typename T>
T angular_distance(const Vec3T<T>& a, const Vec3T<T>& b);

// ============================================================================
// 球面多边形面积 (Girard 定理 / 球面 excess 公式)
//
// 公式: Area = Σ内角 - (n-2)π
// 输入: vertices 按顺序排列的球面顶点 (单位向量, 逆时针或顺时针)
// 输出: 球面面积 (球面度, steradian). 自动取绝对值, 不依赖顶点方向.
//
// 精度: float64, 已知球面多边形面积误差 < 1e-10
// ============================================================================
template <typename T>
T spherical_polygon_area(const std::vector<Vec3T<T>>& vertices);

// R11 (阶段7): 顶点数组版球面多边形面积 (内环无 vector 分配; 与 vector 版等价)
template <typename T>
T spherical_polygon_area_n(const Vec3T<T>* vertices, int n);

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
template <typename T>
std::vector<Vec3T<T>> sutherland_hodgman_spherical(
    const std::vector<Vec3T<T>>& subject,
    const std::vector<Vec3T<T>>& clip_plane_normals);

// R11 (阶段7): 固定容量球面 S-H 裁剪 (栈 buffer, 内环无堆分配)
//   subject/输出 最多 16 顶点 (drop 4 角 × 裁剪 4 边 → 交集 ≤ 8 顶点, 留裕量)
//   返回输出顶点数 (0 = 无交集); 等价于 sutherland_hodgman_spherical
int sutherland_hodgman_spherical_fixed(
    const Vec3* subject, int n_subject,
    const std::vector<Vec3>& clip_plane_normals,
    Vec3* out, int max_out);

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
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside);

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点 (带自适应采样)
//
// 对赤道带像素 (bighp 4-7, 或 bighp 0-3/8-11 的赤道部分):
//   - 所有 4 条边均采样 N 段, 每段用大圆弧近似
//   (赤道带像素的边在 (z, phi) 空间为线性曲线, 非大圆弧;
//    采样后用多段大圆弧近似, 显著降低面积误差)
// 对极区像素 (bighp 0-3/8-11 的极冠部分):
//   - 所有边保持 4 个角顶点 (极区边为大圆弧)
//
// 采样数 samples_per_edge=8 时, 赤道带像素 4 边各 8 段, 总共 4*8=32 个顶点
// samples_per_edge=1 时退化为 4 个角顶点 (与 get_healpix_boundary 一致)
//
// hp: HEALPix 核心
// ipix: 像素 NESTED 索引
// nside: NSIDE (冗余参数, 与 hp.getNside() 一致)
// samples_per_edge: 每条边的采样段数 (>=1)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary_sampled(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge = 8);

// ============================================================================
// 回调类型: 像素坐标 → 天球坐标
// px, py: 像素坐标 (0-based)
// ra, dec: 输出天球坐标 (度)
// user_data: 不透明指针, 由调用者传入
// 返回: true 成功, false 表示投影未定义 (例如 TAN 投影背面)
// ============================================================================
typedef bool (*PixelToSkyFn)(double px, double py, double& ra, double& dec,
                             void* user_data);

// ============================================================================
// 构造源像素 drop 球面多边形 (带边采样)
//
// 源像素中心 (px, py), 像素范围 ±0.5 (pixfrac=1.0 时).
// pixfrac 收缩后, 四角向中心移动 pixfrac 倍.
// 每条边采样 samples_per_edge 段, 每个采样点通过 pixelToSky 回调映射到天球,
// 再转换为球面单位向量.
//
// samples_per_edge=1: 退化为 4 个角顶点 (与旧 processPixel Step1-3 行为一致)
// samples_per_edge=N: 4 条边各采样 N 段, 共 4*N 个顶点 (每边不含末点, 避免重复)
//
// px, py: 像素中心 (0-based)
// pixfrac: 收缩因子 (0, 1] (1.0 = 不收缩)
// pixelToSky: 像素→天球 回调函数
// user_data: 传给回调的不透明指针
// samples_per_edge: 每条边的采样段数 (>=1)
// 返回: 球面多边形顶点 (逆时针, 单位向量). 投影失败时返回空向量.
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> build_drop_polygon_sampled(
    T px, T py, T pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    int samples_per_edge);

// ============================================================================
// R08 改进3+5: 构造源像素 drop 球面多边形 (自适应 WCS 边细分)
//
// 对源像素 WCS/SIP 弯曲边进行自适应二分细分, 收敛条件 (R08 改进5):
//   WCS 中点 (pixelToSky 回调) 到 (p0,p1) 大圆弧平面的角距离
//   = |asin(dot(normalize(cross(p0,p1)), p_mid_wcs))| < wcs_epsilon
//
// R08 改进5 根因: 原收敛条件 "WCS 中点 vs 大圆弧球面中点" 是错误的.
//   TAN (gnomonic) 投影保证平面直线 ↔ 大圆弧, 但平面中点 ≠ 球面中点
//   (gnomonic 参数化非线性: c = atan(rho), 平面 t=0.5 ≠ 球面 t=0.5).
//   新条件检查 WCS 中点是否在大圆弧平面上, 对 TAN 投影 dev≈0 (浮点精度).
//
// 相比 build_drop_polygon_sampled 的固定采样数, 自适应细分:
//   - TAN 投影 (无 SIP): 快速收敛, 仅 4 角 (gnomonic 保证边是大圆弧)
//   - SIP 投影: 递归细分到机器精度, 消除 SIP 畸变曲率面积误差
//
// src_scale_rad: 源像素边长 (弧度), 用于计算相对收敛阈值
//   wcs_epsilon = src_scale_rad * 1e-12
//   通常传入 max_edge_rad (像素四角最大边角跨度)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> build_drop_polygon_adaptive(
    T px, T py, T pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    T src_scale_rad);

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
template <typename T>
T compute_overlap_area(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix);

// R11 (阶段7): drop 几何预计算 (包围圆 + 裁剪法向量), 供 compute_overlap_area_g 复用
//   避免每候选重复构造 (高 NSIDE 下每源像素 ~10 候选 → 显著降低三角函数开销)
struct DropGeometry {
    std::vector<Vec3> corners;        // double 内部
    std::vector<Vec3> clip_normals;   // 归一化裁剪平面法向量
    Vec3 center;                      // drop 包围圆中心 (归一化)
    double max_angle = 0.0;           // drop 顶点到中心最大角距离 (弧度)
};

DropGeometry build_drop_geometry(const std::vector<Vec3>& drop_corners);

// 使用预计算 drop 几何的重叠面积 (与 compute_overlap_area 语义一致)
double compute_overlap_area_g(const DropGeometry& g,
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
template <typename T>
void query_candidate_pixels(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates);

// R11: NESTED 直接候选枚举 (替代 queryDisc BFS)
// 保守覆盖: drop 包围圆 + 1.2×hp_res 像素外接半径 (零漏选, 允许少量 false positives)
// 与 query_candidate_pixels 语义一致, 供 pixfrac=1 共享顶点路径使用
template <typename T>
void query_candidate_pixels_fast(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates);

} // namespace spherical

#endif // SPHERICAL_OVERLAP_H
