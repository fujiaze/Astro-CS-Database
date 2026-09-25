#ifndef ASTROCS_v6_SPHERICAL_OVERLAP_H
#define ASTROCS_v6_SPHERICAL_OVERLAP_H

// ============================================================================
// ACSD v6 Phase1 Drizzle 球面 overlap 适配层
//
// 任务 IMPL-P1-DRZ-001。把既有球面几何（ALG-DRZ-001：三层候选缓冲、
// 球面 Sutherland–Hodgman 裁剪、FP64 球面面积）产出的源像素 drop × 目标
// HEALPix NESTED leaf 交叠面积 a_jp [px^2] 接入 v6 SB/方差/相关算子。
//
// 冻结锚：
//   FZ-FORMULA-DRIZZLE-SB  D_p = Sum_j a_jp；w_SB_jp = a_jp / A_pixel_j
//   FZ-COND-FLUX-CONSERV   Sum_p a_jp = A_drop_j = pixfrac^2 * A_pixel_j
//   ALG-P1-001 §6.1/§6.6   pixfrac in (0,1] 非法即拒绝；RING 拒绝；几何 FP64
//
// 本层不接线任何 session。
// ============================================================================

#include "healpix_core.h"
#include "spherical_overlap.h"
#include "v6_drizzle_science.h"

#include <cstdint>
#include <vector>

namespace astrocs {
namespace v6 {
namespace drizzle {

// 源像素 drop 规格。A_pixel<=0 时由 drop 球面多边形面积反推
// （A_pixel = drop_area / pixfrac^2），此时几何闭合按构造精确成立。
struct SourceDropSpec {
    double px = 0.0;
    double py = 0.0;
    double pixfrac = 1.0;
    double A_pixel = 0.0; // [px^2]（pixfrac=1）
    int samples_per_edge = 8; // A_pixel<=0 时的固定采样路径
};

struct OverlapHit {
    uint64_t target_ipix = 0; // NESTED
    double a_jp = 0.0;        // [px^2]
};

struct OverlapRow {
    double drop_area = 0.0;   // 球面 drop 多边形面积 [px^2]
    double A_pixel = 0.0;     // 该源像素 pixfrac=1 面积 [px^2]
    double sum_a_jp = 0.0;    // Sum_p a_jp
    double closure_rel = 0.0; // (sum_a_jp - pixfrac^2 A_pixel) / (pixfrac^2 A_pixel)
    std::vector<OverlapHit> hits; // 去重、按 target_ipix 排序
    // DRZ-PF-CORRECT-01 (S1 第 19 条): 本行候选里交叠面积**失效**（非有限或
    // <0，因而被剔除、其面积不进 sum_a_jp）的 target 像素数。修复前该分支是
    // 静默 continue ⇒ 面积亏损既不计数也无人知道。
    // a == 0 是**良性**的"该候选与该 drop 不相交"（保守候选查询的必然产物），
    // 只计入 n_zero_overlap，不判失败。
    std::uint64_t n_area_rejected = 0;
    std::uint64_t n_zero_overlap = 0;
};

// 容许的「候选交叠面积失效」像素数上限（默认 0 = 一个都不容许）。
// 失效面积必然使该源像素的几何闭合亏损（rel<0），故默认 fail-closed。
constexpr std::uint64_t kMaxInvalidAreaHits = 0;

// 构建算子的几何诊断（供产品 provenance 登记；修复前这些量全部不可见）。
struct OverlapDiagnostics {
    std::uint64_t n_area_rejected = 0;    // 失效面积候选总数（非有限/负）
    std::uint64_t n_zero_overlap = 0;     // 良性零交叠候选数（保守查询超集）
    std::uint64_t n_sources = 0;          // 处理过的源像素数
    double max_abs_closure_rel = 0.0;     // 全体源像素 |闭合偏差| 最大值
};

// 计算单个源 drop 的球面 overlap 行（真实球面几何）。
// closure_rel_tol: 几何闭合容差（判据取绝对值；超出即具名失败）。
// 失败面: 面积失效候选数 > kMaxInvalidAreaHits ⇒ DrzError::overlap_area_invalid;
//         |closure_rel| > closure_rel_tol ⇒ overlap_exceeds_drop / overlap_area_deficit。
DrzError compute_overlap_row(const ::healpix::HealpixCore& hp,
                             const SourceDropSpec& src,
                             spherical::PixelToSkyFn pixel_to_sky, void* user_data,
                             double closure_rel_tol,
                             OverlapRow* out);

// 由真实球面 overlap 构建 v6 Drizzle 算子。所有源必须同一 pixfrac。
// target_ipix_out（可选）：局部 target 索引 -> 全局 NESTED ipix。
// diag_out（可选）：几何诊断（失效面积计数 / 最大 |闭合偏差|），供 provenance。
DrzError build_operator_from_sources(const ::healpix::HealpixCore& hp,
                                     const std::vector<SourceDropSpec>& sources,
                                     spherical::PixelToSkyFn pixel_to_sky,
                                     void* user_data,
                                     double closure_rel_tol,
                                     DrizzleOperator& out,
                                     std::vector<uint64_t>* target_ipix_out = nullptr,
                                     std::vector<OverlapRow>* rows_out = nullptr,
                                     OverlapDiagnostics* diag_out = nullptr);

} // namespace drizzle
} // namespace v6
} // namespace astrocs

#endif // ASTROCS_v6_SPHERICAL_OVERLAP_H
