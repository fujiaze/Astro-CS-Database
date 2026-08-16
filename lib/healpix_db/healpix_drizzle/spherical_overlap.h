#ifndef SPHERICAL_OVERLAP_H
#define SPHERICAL_OVERLAP_H

// ============================================================================
// 球面 HEALPix 重叠计算模块 (WP-D 步骤3-4)
//
// 用途:
// 替换 drizzle_engine.cpp 中的局部切平面近似 + 人工 HEALPix 菱形近似,
// 实现真实球面多边形裁剪与球面面积计算, 修复极区/大视场/高 WCS 畸变误差.
//
// 核心算法:
// - 球面向量 (Vec3) 与基本运算 (cross/dot/normalize)
// - 球面多边形面积 (Girard 定理: 面积 = Σ内角 - (n-2)π)
// - 球面 Sutherland-Hodgman 多边形裁剪 (大圆弧裁剪, 法向量定义保留侧)
// - HEALPix 像素球面边界获取 (4 个角顶点)
// - 源像素 drop 与目标 HEALPix 像素的球面重叠面积
// - 候选像素查询 (基于 drop 多边形球面包围盒, 不限于 1-ring)
//
// 精度: float64 内部精度, 球面面积误差 < 1e-6 球面度
//
// 参考:
// - Gorski et al. 2005, HEALPix Framework
// - Chamberlain & Duquette 2007, 球面多边形面积算法
// - Sutherland & Hodgman 1974, 多边形裁剪
// ============================================================================

#include "healpix_core.h"
#include <array>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace spherical {

// ============================================================================
// 球面向量 (单位向量) — 模板双实例 ( 阶段7: 真 FP32/FP64 Scalar 几何)
// Vec3T<float>: FP32 科学路径 (IEEE binary32 全链)
// Vec3T<double>: FP64 科学路径 (IEEE binary64, 与旧 Vec3 一致)
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

// 顶点数组版球面多边形面积 (内环无 vector 分配; 与 vector 版等价)
template <typename T>
T spherical_polygon_area_n(const Vec3T<T>* vertices, int n);

// ============================================================================
// 球面 Sutherland-Hodgman 多边形裁剪
//
// 用一组大圆弧裁剪球面多边形:
// - subject: 被裁剪的球面多边形 (单位向量, 任意方向)
// - clip_plane_normals: 裁剪平面法向量列表 (每个定义一个大圆, 保留 dot(n, v) > 0 一侧)
// 返回: 裁剪后的球面多边形顶点 (单位向量)
//
// 算法: 经典 Sutherland-Hodgman 推广到球面
// - 边 (S→E) 为大圆弧, 法向量 = cross(S, E)
// - 与裁剪大圆 (法向量 n) 的交点 = ±normalize(cross(n_edge, n_clip))
// - 选择位于 S, E 之间的交点 (dot(intersection, S+E) > 0)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> sutherland_hodgman_spherical(
    const std::vector<Vec3T<T>>& subject,
    const std::vector<Vec3T<T>>& clip_plane_normals);

// 固定容量球面 S-H 裁剪 (栈 buffer, 内环无堆分配)
// subject/输出 最多 16 顶点 (drop 4 角 × 裁剪 4 边 → 交集 ≤ 8 顶点, 留裕量)
// 返回输出顶点数 (0 = 无交集); 等价于 sutherland_hodgman_spherical
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
// nside: NSIDE (冗余参数, 与 hp.getNside 一致, 保留接口一致性)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside);

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点 (带自适应采样)
//
// 对赤道带像素 (bighp 4-7, 或 bighp 0-3/8-11 的赤道部分):
// - 所有 4 条边均采样 N 段, 每段用大圆弧近似
// (赤道带像素的边在 (z, phi) 空间为线性曲线, 非大圆弧;
// 采样后用多段大圆弧近似, 显著降低面积误差)
// 对极区像素 (bighp 0-3/8-11 的极冠部分):
// - 所有边保持 4 个角顶点 (极区边为大圆弧)
//
// 采样数 samples_per_edge=8 时, 赤道带像素 4 边各 8 段, 总共 4*8=32 个顶点
// samples_per_edge=1 时退化为 4 个角顶点 (与 get_healpix_boundary 一致)
//
// hp: HEALPix 核心
// ipix: 像素 NESTED 索引
// nside: NSIDE (冗余参数, 与 hp.getNside 一致)
// samples_per_edge: 每条边的采样段数 (>=1)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary_sampled(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge = 8);

// 高 NSIDE（nside>=256）生产路径的固定 4 角边界（无堆分配）。
// 与 get_healpix_boundary 逐位等价（同一 4 角计算）。
template <typename T>
void get_healpix_boundary4(const healpix::HealpixCore& hp, uint64_t ipix,
                           int nside, std::array<Vec3T<T>, 4>& out);

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
// 改进3+5: 构造源像素 drop 球面多边形 (自适应 WCS 边细分)
//
// 对源像素 WCS/SIP 弯曲边进行自适应二分细分, 收敛条件 ( 改进5):
// WCS 中点 (pixelToSky 回调) 到 (p0,p1) 大圆弧平面的角距离
// = |asin(dot(normalize(cross(p0,p1)), p_mid_wcs))| < wcs_epsilon
//
// 改进5 根因: 原收敛条件 "WCS 中点 vs 大圆弧球面中点" 是错误的.
// TAN (gnomonic) 投影保证平面直线 ↔ 大圆弧, 但平面中点 ≠ 球面中点
// (gnomonic 参数化非线性: c = atan(rho), 平面 t=0.5 ≠ 球面 t=0.5).
// 新条件检查 WCS 中点是否在大圆弧平面上, 对 TAN 投影 dev≈0 (浮点精度).
//
// 相比 build_drop_polygon_sampled 的固定采样数, 自适应细分:
// - TAN 投影 (无 SIP): 快速收敛, 仅 4 角 (gnomonic 保证边是大圆弧)
// - SIP 投影: 递归细分到机器精度, 消除 SIP 畸变曲率面积误差
//
// src_scale_rad: 源像素边长 (弧度), 用于计算相对收敛阈值
// wcs_epsilon = src_scale_rad * 1e-12
// 通常传入 max_edge_rad (像素四角最大边角跨度)
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
// 1. 获取目标 HEALPix 像素 4 个角顶点 (单位向量)
// 2. 构造 4 个裁剪平面 (HEALPix 边的大圆法向量, 指向像素内部)
// 3. 用球面 Sutherland-Hodgman 裁剪 drop 多边形
// 4. 用 Girard 定理计算交集面积
// ============================================================================
template <typename T>
T compute_overlap_area(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix);

// drop 几何预计算模板
// Scalar=float 实例: 向量/法向量/角距离/面积类型贯穿 float (数据存储为
// IEEE binary32; 数值运算内部提升保稳定, 见实现说明)
// Scalar=double 实例: 全 double
template <typename Scalar>
struct DropGeometryT {
    std::vector<Vec3T<Scalar>> corners;        // Scalar 存储
    std::vector<Vec3T<Scalar>> clip_normals;   // 归一化裁剪平面法向量
    Vec3T<Scalar> center;                      // drop 包围圆中心 (归一化)
    double max_angle = 0.0;                    // 顶点到中心最大角距离 (弧度, 双精度计算)
    // drop 球面面积 (double 精度源, 构建时一次; 小 drop 完全包含
    // 快路径直接返回, 避免 S-H 角点重算误差) + double 角点缓存
    double drop_area = 0.0;
    std::vector<Vec3> corners_d;
    // 内部 double 精度缓存 (构建时一次, S-H/判定使用; 避免 float 存储舍入
    // 导致微小几何判定翻转, 且不逐候选重建)
    std::vector<Vec3> clip_normals_d;
    Vec3 center_d;
};

template <typename Scalar>
DropGeometryT<Scalar> build_drop_geometry(
    const std::vector<Vec3T<Scalar>>& drop_corners,
    const std::vector<Vec3>* corners_dbl = nullptr);

// 复用版——写入调用方提供的 DropGeometryT（内部 vector 已
// reserve，避免每像素 4 次堆分配）；科学语义与 build_drop_geometry 完全一致
// （同一实现，仅目标对象复用）。调用方保证 g 不被并发共享。
template <typename Scalar>
void build_drop_geometry_into(DropGeometryT<Scalar>& g,
                              const std::vector<Vec3T<Scalar>>& drop_corners,
                              const std::vector<Vec3>* corners_dbl = nullptr);

// 使用预计算 drop 几何的重叠面积 (Scalar 实例; 返回 Scalar)
// hp_res_rad 由调用方 run-context 传入，避免每候选重算
// pixelResolutionArcsec（整帧常量）；quick-reject 用 dot<cos(limit)
// 替代 acos（acos 在 [-1,1] 单调递减 → 数学等价）。
template <typename Scalar>
Scalar compute_overlap_area_g(const DropGeometryT<Scalar>& g,
                              const healpix::HealpixCore& hp, uint64_t target_ipix);

template <typename Scalar>
Scalar compute_overlap_area_g_ctx(const DropGeometryT<Scalar>& g,
                                  const healpix::HealpixCore& hp,
                                  uint64_t target_ipix,
                                  double hp_res_rad);

// ============================================================================
// 定点优化（DRIZZLE_TARGETED 优先级 1）：
// bounded target-ipix geometry cache。
//
// 一次 run 内同一个目标 HEALPix leaf 被大量 drop 候选重复访问，而
// compute_overlap_area_g_ctx 每候选重算 pix2radec + radec_to_vec +
// get_healpix_boundary4（4 角）。缓存 {center, boundary4} 消除重复：
// - 容量有界（默认 8192，LRU 淘汰；线程私有，禁止跨线程共享）；
// - key = target ipix；每次 run（drizzleTiled）开始 clear，
// 避免跨 run 的 NSIDE 不同导致的几何污染；
// - 科学语义与 compute_overlap_area_g_ctx 逐位等价（同一数值路径，
// 仅 geometry 取缓存）；false_negative 不变（缓存只复用不预筛）。
// ============================================================================
struct TargetPixelGeometry {
    Vec3 center{0.0, 0.0, 0.0};           // leaf 中心单位向量
    std::array<Vec3, 4> boundary4{};      // 4 角（nside>=256 生产路径）
    bool ready = false;
};

class TargetGeomCache {
public:
    explicit TargetGeomCache(std::size_t capacity = 8192)
        : capacity_(capacity == 0 ? 1 : capacity) {}

    // 获取 target ipix 几何；未命中时构建并缓存。返回不可为 null。
    // 线程私有对象（thread_local），禁止并发共享。
    const TargetPixelGeometry* get_or_build(const healpix::HealpixCore& hp,
                                            std::uint64_t ipix,
                                            bool* built_out = nullptr);

    void clear();
    std::size_t size() const { return map_.size(); }
    std::size_t capacity() const { return capacity_; }
    std::size_t hits() const { return hits_; }
    std::size_t misses() const { return misses_; }

private:
    struct Entry {
        std::uint64_t ipix;
        TargetPixelGeometry geom;
    };
    std::size_t capacity_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    std::deque<std::uint64_t> lru_;      // front = most recent
    std::unordered_map<std::uint64_t, Entry> map_;
};

// 使用缓存的目标重叠面积（科学语义与 compute_overlap_area_g_ctx 等价；
// nside<256 低 NSIDE 路径退回逐调用构建并写入缓存）。
template <typename Scalar>
Scalar compute_overlap_area_g_ctx_cached(
    const DropGeometryT<Scalar>& g, const healpix::HealpixCore& hp,
    std::uint64_t target_ipix, double hp_res_rad,
    TargetGeomCache& cache);

// overlap 路径计数 (quick=相离, fully=drop 包含像素,
// dropin=drop 在像素内, sh=部分相交 S-H); 仅统计, 不改变逻辑
long long profile_overlap_path_counts(long long* fully, long long* dropin,
                                      long long* sh);

// ============================================================================
// 查询与 drop 多边形可能相交的所有 HEALPix 像素 (不限于 1-ring)
//
// drop_corners: drop 球面多边形顶点
// hp: HEALPix 核心
// candidates: 输出候选像素列表 (NESTED ipix, 已去重排序)
//
// 实现:
// 1. 计算 drop 多边形的球面包围圆 (中心向量 + 最大角半径)
// 2. 加上 1 个 HEALPix 像素分辨率作为缓冲 (避免边缘漏选)
// 3. 用 hp.queryDisc 查询圆盘内所有像素
// 4. 高 NSIDE + 大源像素时, 候选数可远 > 48
// ============================================================================
template <typename T>
void query_candidate_pixels(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates);

// NESTED 直接候选枚举 (替代 queryDisc BFS)
// 保守覆盖: drop 包围圆 + 1.2×hp_res 像素外接半径 (零漏选, 允许少量 false positives)
// 与 query_candidate_pixels 语义一致, 供 pixfrac=1 共享顶点路径使用
template <typename T>
void query_candidate_pixels_fast(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates,
    bool* used_fallback = nullptr);

} // namespace spherical

#endif // SPHERICAL_OVERLAP_H
