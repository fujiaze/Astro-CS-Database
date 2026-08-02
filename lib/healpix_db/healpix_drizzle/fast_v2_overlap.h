#ifndef FAST_V2_OVERLAP_H
#define FAST_V2_OVERLAP_H

// ============================================================================
// FAST v2 Drizzle 重叠计算 (R08 实验, 纯 C++)
//
// 设计目标:
//   消除 FAST v1 在高 NSIDE 时固定 gnomonic 投影的开销, 同时修复低 NSIDE
//   平面近似失真 (面积误差 9.35%). 通过分层混合路径对常见场景快速处理,
//   对病态场景精确回退 PRECISE.
//
// 三层混合路径:
//   1. FAST_COMMON_EXACT  — 高 NSIDE、小 drop、低畸变、候选数 <= 4:
//      - drop 完全落入 1 个像素 → drop 面积
//      - drop 完全包含像素      → 解析面积 π/(3·NSIDE²)
//      - 2/3/4 像素交叠         → 局部切平面 2D 裁剪 + 精确 Jacobian
//
//   2. FAST_CACHED_APPROX — 块级 WCS 线性化稳定时使用仿射 + 缓存权重模板
//      (本实验版本暂未实现, 直接走 PRECISE_FALLBACK)
//
//   3. PRECISE_FALLBACK   — 极区 / base-face 边界 / 大 drop / 强 SIP /
//      候选数 > 8 / 曲率残差超限 → 直接调用 spherical::compute_overlap_area
//
// 与 PRECISE 共享:
//   - WCS/SIP 解算 (WcsSip)
//   - NSIDE/NESTED/ICRS (HealpixCore)
//   - pixfrac 语义
//   - 候选像素召回 (spherical::query_candidate_pixels)
//   - 总通量守恒 (signal = flux * area_ratio)
//
// 验收 (ACCEPTANCE_GATES.md):
//   - 候选集合零漏选 (与 PRECISE 共享 query_candidate_pixels)
//   - 总通量语义不变
//   - 报告原始面积误差、signal/support、质心、PSF 误差
//   - 病态场景自动回退 PRECISE
// ============================================================================

#include "healpix_core.h"
#include "spherical_overlap.h"   // 复用 Vec3, PixelToSkyFn, query_candidate_pixels

#include <vector>
#include <cstdint>

namespace fast_v2 {

// ============================================================================
// FAST v2 诊断输出
// ============================================================================
struct FastV2Diagnostics {
    enum Path {
        COMMON_EXACT      = 0,   // 常见路径: 候选少 + 低畸变, 局部解析/2D 裁剪
        CACHED_APPROX     = 1,   // 块级仿射 + 缓存权重模板 (实验阶段未实现)
        PRECISE_FALLBACK  = 2    // 病态场景回退 PRECISE
    };

    Path    path_used = PRECISE_FALLBACK;
    int     candidate_count = 0;       // 本 drop 的候选像素数
    double  raw_area = 0.0;            // 归一化前面积 (本目标像素)
    double  precise_area = 0.0;        // PRECISE 参考 (仅诊断模式填入)
    double  area_rel_err = 0.0;        // |raw - precise| / precise (仅诊断模式)
    double  compute_time_us = 0.0;     // 微秒 (本目标像素重叠计算耗时)

    // 拓扑分类 (COMMON_EXACT 路径下细分)
    enum Topology {
        TOPO_UNKNOWN          = 0,
        TOPO_DROP_IN_PIXEL    = 1,   // drop 完全落入 1 个目标像素
        TOPO_PIXEL_IN_DROP    = 2,   // 目标像素完全包含于 drop
        TOPO_2PIXEL_OVERLAP   = 3,   // drop 跨 2 个相邻像素
        TOPO_3PIXEL_OVERLAP   = 4,   // drop 跨 3 个相邻像素
        TOPO_4PIXEL_OVERLAP   = 5,   // drop 跨 4 个相邻像素
        TOPO_COMPLEX          = 6,   // 一般情况, 走切平面 2D 裁剪
        TOPO_PRECISE_FALLBACK = 7    // 回退 PRECISE
    };
    int     topology = TOPO_UNKNOWN;
};

// ============================================================================
// FAST v2 重叠面积计算
//
// drop_corners:        drop 球面多边形顶点 (单位向量, 来自 build_drop_polygon_*)
// hp:                  HEALPix 核心
// target_ipix:         目标像素 NESTED 索引
// nside:               NSIDE (冗余, 与 hp.getNside() 一致)
// center_ra,center_dec:drop 中心天球坐标 (切平面切点)
// healpix_samples:     HEALPix 边界采样段数 (COMMON_EXACT 路径用 1-2)
// diag:                诊断输出 (可选, nullptr 则不输出)
//
// 返回: 球面重叠面积 (球面度, steradian). 不相交返回 0.
// ============================================================================
double compute_overlap_area_v2(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix, int nside,
    double center_ra, double center_dec,
    int healpix_samples,
    FastV2Diagnostics* diag = nullptr);

// ============================================================================
// 路径分类决策 (供 benchmark 单独调用观察决策)
//
// 返回应当使用的路径. 不实际计算面积.
// ============================================================================
FastV2Diagnostics::Path decide_path(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    int nside,
    double center_ra, double center_dec,
    int sip_order,
    int candidate_count);

// ============================================================================
// 拓扑分类 (COMMON_EXACT 路径下细分)
//
// 给定 drop 与候选像素集合, 判断本目标像素属于哪种拓扑关系.
// 不计算面积, 仅做几何分类.
// ============================================================================
FastV2Diagnostics::Topology classify_topology(
    const std::vector<spherical::Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix, int nside,
    const std::vector<uint64_t>& all_candidates);

} // namespace fast_v2

#endif // FAST_V2_OVERLAP_H
