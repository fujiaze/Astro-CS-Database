// ============================================================================
// v6 球面 overlap 适配层实现 — 见 v6_spherical_overlap.h
// 复用既有球面几何（spherical_overlap.{h,cpp}），不改其数值路径。
// ============================================================================
#include "v6_spherical_overlap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>

namespace astrocs {
namespace v6 {
namespace drizzle {

namespace {
constexpr double kArcsecToRad = 3.14159265358979323846 / 180.0 / 3600.0;

double drop_src_scale_rad(const SourceDropSpec& src) {
    if (src.A_pixel > 0.0) return std::sqrt(src.A_pixel);
    return 1e-6; // 保守初值；A_pixel<=0 时走固定采样路径，不依赖自适应阈值
}
} // namespace

DrzError compute_overlap_row(const ::healpix::HealpixCore& hp,
                             const SourceDropSpec& src,
                             spherical::PixelToSkyFn pixel_to_sky, void* user_data,
                             double closure_rel_tol,
                             OverlapRow* out) {
    if (!out || !pixel_to_sky) return DrzError::invalid_argument;
    if (validate_pixfrac(src.pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    *out = OverlapRow{};
    // ENGINEERING_SPEC §8 可执行负例（故障注入面）: 把首个候选的交叠面积置为
    // NaN，复现「一个面积无效像素」被静默 continue 吞掉的旧行为 ⇒ 本函数必须
    // 以具名错误 overlap_area_invalid 失败（门 v6_p1_drz_area_invalid 判红）。
    const char* v6_fault = std::getenv("ASTROCS_V6_DRZ_FAULT");
    const bool inject_invalid_area =
        (v6_fault && std::string(v6_fault) == "invalid_area");
    bool injected = false;

    std::vector<spherical::Vec3> corners;
    if (src.A_pixel > 0.0) {
        corners = spherical::build_drop_polygon_adaptive<double>(
            src.px, src.py, src.pixfrac, pixel_to_sky, user_data,
            drop_src_scale_rad(src));
    } else {
        const int n = std::max(1, src.samples_per_edge);
        corners = spherical::build_drop_polygon_sampled<double>(
            src.px, src.py, src.pixfrac, pixel_to_sky, user_data, n);
    }
    if (corners.empty()) return DrzError::missing_wcs;

    spherical::DropGeometryT<double> geom =
        spherical::build_drop_geometry<double>(corners);
    out->drop_area = geom.drop_area;
    if (!std::isfinite(out->drop_area) || out->drop_area <= 0.0) {
        return DrzError::missing_wcs;
    }
    out->A_pixel = (src.A_pixel > 0.0)
                       ? src.A_pixel
                       : out->drop_area / (src.pixfrac * src.pixfrac);
    if (!std::isfinite(out->A_pixel) || out->A_pixel <= 0.0) {
        return DrzError::invalid_source_area;
    }

    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels_fast<double>(corners, hp, candidates);
    const double hp_res_rad = hp.pixelResolutionArcsec() * kArcsecToRad;

    out->hits.reserve(candidates.size());
    double sum = 0.0;
    for (uint64_t ipix : candidates) {
        double a = spherical::compute_overlap_area_g_ctx<double>(
            geom, hp, ipix, hp_res_rad);
        if (inject_invalid_area && !injected) {
            a = std::numeric_limits<double>::quiet_NaN();
            injected = true;
        }
        // DRZ-PF-CORRECT-01 (S1 第 19 条): **面积失效必须计数并具名失败**。
        // 语义分层（不可混为一谈）:
        //   * a == 0  = 该候选与该 drop **真的不相交**（候选查询是保守超集,
        //     见 spherical_overlap.cpp 的 quick-reject 返回 Scalar(0)）⇒ 良性,
        //     只计数 n_zero_overlap, 不判失败;
        //   * a < 0 或非有限 = **几何失效**（裁剪多边形面积不可能为负/NaN）⇒
        //     该 target 的面积既不进 sum_a_jp 也不留痕, 使 sum_a_jp 偏小、
        //     闭合亏损 (rel<0)。修复前这里是同一句裸 continue ⇒ 面积亏损全程
        //     静默进产品。现在计数并在超阈值时具名失败。
        if (!std::isfinite(a) || a < 0.0) {
            ++out->n_area_rejected;
            continue;
        }
        if (a == 0.0) {
            ++out->n_zero_overlap;
            continue;
        }
        out->hits.push_back(OverlapHit{ipix, a});
        sum += a;
    }
    std::sort(out->hits.begin(), out->hits.end(),
              [](const OverlapHit& x, const OverlapHit& y) {
                  return x.target_ipix < y.target_ipix;
              });
    // 去重（同一 target 只允许一条 (src,dst) 记录；保守候选查询可能重复）。
    out->hits.erase(std::unique(out->hits.begin(), out->hits.end(),
                                [](const OverlapHit& x, const OverlapHit& y) {
                                    return x.target_ipix == y.target_ipix;
                                }),
                    out->hits.end());
    sum = 0.0;
    for (const OverlapHit& h : out->hits) sum += h.a_jp;
    out->sum_a_jp = sum;
    if (out->n_area_rejected > kMaxInvalidAreaHits) {
        // 具名失败并回传非零（不再用 1e300 当"求值器"把判据短路掉）。
        out->closure_rel = 0.0;
        return DrzError::overlap_area_invalid;
    }
    // 闭合判据用**真实容差**且取绝对值（亏损侧同样具名失败）。
    const DrzError ce = validate_overlap_closure(sum, out->A_pixel, src.pixfrac,
                                                closure_rel_tol, &out->closure_rel);
    if (ce != DrzError::ok) return ce;
    return DrzError::ok;
}

DrzError build_operator_from_sources(const ::healpix::HealpixCore& hp,
                                     const std::vector<SourceDropSpec>& sources,
                                     spherical::PixelToSkyFn pixel_to_sky,
                                     void* user_data,
                                     double closure_rel_tol,
                                     DrizzleOperator& out,
                                     std::vector<uint64_t>* target_ipix_out,
                                     std::vector<OverlapRow>* rows_out,
                                     OverlapDiagnostics* diag_out) {
    if (sources.empty()) return DrzError::invalid_argument;
    const double pixfrac = sources.front().pixfrac;
    if (validate_pixfrac(pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    for (const SourceDropSpec& s : sources) {
        if (s.pixfrac != pixfrac) return DrzError::invalid_argument;
    }
    // DRZ-PF-CORRECT-01 (S1 第 19 条): 几何诊断按源累加，供产品 provenance。
    OverlapDiagnostics diag;
    diag.n_sources = sources.size();

    std::vector<OverlapRow> rows(sources.size());
    std::unordered_map<uint64_t, uint32_t> dst_index;
    std::vector<uint64_t> target_ipix;
    std::vector<OperatorEntry> overlaps;
    std::vector<OperatorSource> op_sources(sources.size());

    for (size_t j = 0; j < sources.size(); ++j) {
        DrzError e = compute_overlap_row(hp, sources[j], pixel_to_sky, user_data,
                                         closure_rel_tol, &rows[j]);
        // 先归集本行诊断再判失败 —— 失败路径也必须把"被吞掉几个"回传，
        // 否则该计数在产品 provenance 上不可见（静默的另一种形态）。
        diag.n_area_rejected += rows[j].n_area_rejected;
        diag.n_zero_overlap += rows[j].n_zero_overlap;
        if (e != DrzError::ok) {
            if (diag_out) *diag_out = diag;
            return e;
        }
        diag.max_abs_closure_rel =
            std::max(diag.max_abs_closure_rel, std::fabs(rows[j].closure_rel));
        double rel = 0.0;
        // 同一判据再核一遍（闭合取绝对值，亏损/超额分别具名）。
        e = validate_overlap_closure(rows[j].sum_a_jp, rows[j].A_pixel, pixfrac,
                                     closure_rel_tol, &rel);
        if (e != DrzError::ok) return e;
        op_sources[j].A_pixel = rows[j].A_pixel;
        for (const OverlapHit& h : rows[j].hits) {
            auto it = dst_index.find(h.target_ipix);
            uint32_t dst;
            if (it == dst_index.end()) {
                dst = static_cast<uint32_t>(target_ipix.size());
                dst_index.emplace(h.target_ipix, dst);
                target_ipix.push_back(h.target_ipix);
            } else {
                dst = it->second;
            }
            OperatorEntry oe;
            oe.src = static_cast<uint32_t>(j);
            oe.dst = dst;
            oe.a_jp = h.a_jp;
            overlaps.push_back(oe);
        }
    }

    const size_t n_dst = target_ipix.size();
    if (n_dst == 0) return DrzError::coverage_mismatch;
    if (n_dst > 0xFFFFFFFFull || sources.size() > 0xFFFFFFFFull) {
        return DrzError::invalid_argument;
    }

    DrzError e = DrizzleOperator::build(
        static_cast<uint32_t>(sources.size()), static_cast<uint32_t>(n_dst),
        op_sources, pixfrac, NormalizationKind::drop_area, overlaps, out);
    if (e != DrzError::ok) return e;

    if (target_ipix_out) *target_ipix_out = std::move(target_ipix);
    if (rows_out) *rows_out = std::move(rows);
    if (diag_out) *diag_out = diag;
    return DrzError::ok;
}

} // namespace drizzle
} // namespace v6
} // namespace astrocs
