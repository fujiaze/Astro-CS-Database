// ============================================================================
// v6 球面 overlap 适配层实现 — 见 v6_spherical_overlap.h
// 复用既有球面几何（spherical_overlap.{h,cpp}），不改其数值路径。
// ============================================================================
#include "v6_spherical_overlap.h"

#include <algorithm>
#include <cmath>
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
                             OverlapRow* out) {
    if (!out || !pixel_to_sky) return DrzError::invalid_argument;
    if (validate_pixfrac(src.pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    *out = OverlapRow{};

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
        const double a = spherical::compute_overlap_area_g_ctx<double>(
            geom, hp, ipix, hp_res_rad);
        if (!std::isfinite(a) || a <= 0.0) continue;
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
    (void)validate_overlap_closure(sum, out->A_pixel, src.pixfrac, 1e300,
                                   &out->closure_rel);
    return DrzError::ok;
}

DrzError build_operator_from_sources(const ::healpix::HealpixCore& hp,
                                     const std::vector<SourceDropSpec>& sources,
                                     spherical::PixelToSkyFn pixel_to_sky,
                                     void* user_data,
                                     double closure_rel_tol,
                                     DrizzleOperator& out,
                                     std::vector<uint64_t>* target_ipix_out,
                                     std::vector<OverlapRow>* rows_out) {
    if (sources.empty()) return DrzError::invalid_argument;
    const double pixfrac = sources.front().pixfrac;
    if (validate_pixfrac(pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    for (const SourceDropSpec& s : sources) {
        if (s.pixfrac != pixfrac) return DrzError::invalid_argument;
    }

    std::vector<OverlapRow> rows(sources.size());
    std::unordered_map<uint64_t, uint32_t> dst_index;
    std::vector<uint64_t> target_ipix;
    std::vector<OperatorEntry> overlaps;
    std::vector<OperatorSource> op_sources(sources.size());

    for (size_t j = 0; j < sources.size(); ++j) {
        DrzError e = compute_overlap_row(hp, sources[j], pixel_to_sky, user_data, &rows[j]);
        if (e != DrzError::ok) return e;
        double rel = 0.0;
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
        op_sources, pixfrac, NormalizationKind::sb_a_pixel, overlaps, out);
    if (e != DrzError::ok) return e;

    if (target_ipix_out) *target_ipix_out = std::move(target_ipix);
    if (rows_out) *rows_out = std::move(rows);
    return DrzError::ok;
}

} // namespace drizzle
} // namespace v6
} // namespace astrocs
