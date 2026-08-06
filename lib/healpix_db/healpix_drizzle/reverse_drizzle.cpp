// ============================================================================
// reverse_drizzle.cpp - Sphere -> Plane 真面积 Drizzle 实现
// ============================================================================
#include "reverse_drizzle.h"
#include "poly_clip.h"
#include "wcs_sip.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace drizzle {

bool ReverseDrizzle::run(const ReverseDrizzleInput& in,
                         ReverseDrizzleOutput& out, std::string& error_msg) {
    error_msg.clear();
    if (in.nside == 0 || in.target_width <= 0 || in.target_height <= 0) {
        error_msg = "ReverseDrizzle: 非法 nside/尺寸";
        return false;
    }
    const size_t npix_plane = (size_t)in.target_width * in.target_height;
    out.signal.assign(npix_plane, 0.0);
    out.coverage.assign(npix_plane, 0.0);
    out.signal_f32.assign(npix_plane, 0.0f);
    out.coverage_f32.assign(npix_plane, 0.0f);
    const bool use_f32 = !in.leaf_signal.empty() ? false
                        : !in.leaf_signal_f32.empty();
    if (in.leaf_ipix.empty()) {
        out.n_source_leaf = 0;
        return true;
    }
    WcsSip wcs(in.wcs);
    healpix::HealpixCore hp(in.nside, in.nested);
    const double pf = std::max(0.01, std::min(1.0, in.pixfrac));

    double total_in = 0.0;
    const int W = in.target_width, H = in.target_height;
    for (size_t i = 0; i < in.leaf_ipix.size(); i++) {
        uint64_t ip = in.leaf_ipix[i];
        double sig = use_f32 ? (double)in.leaf_signal_f32[i]
                             : in.leaf_signal[i];
        if (!std::isfinite(sig)) continue;
        total_in += sig;
        // 1. leaf 球面 footprint (细分边界, 面积精度)
        std::vector<spherical::Vec3> bnd_sph =
            spherical::get_healpix_boundary_sampled<double>(
                hp, ip, (int)in.nside, 8);
        if (bnd_sph.size() < 3) continue;
        // 2. 投影到平面 (skyToPixel), pixfrac 收缩
        std::vector<Point2D> poly;
        poly.reserve(bnd_sph.size());
        for (const auto& v : bnd_sph) {
            double ra, dec;
            spherical::vec_to_radec<double>(v, ra, dec);
            double x, y;
            wcs.skyToPixel(ra, dec, x, y);
            if (!std::isfinite(x) || !std::isfinite(y)) { poly.clear(); break; }
            poly.push_back({x, y});
        }
        if (poly.size() < 3) continue;
        // pixfrac 收缩: 多边形向质心收缩 pf 倍
        if (pf < 1.0) {
            double cx = 0, cy = 0;
            for (const auto& p : poly) { cx += p.x; cy += p.y; }
            cx /= poly.size(); cy /= poly.size();
            for (auto& p : poly) {
                p.x = cx + (p.x - cx) * pf;
                p.y = cy + (p.y - cy) * pf;
            }
        }
        double leaf_area = std::fabs(PolyClip::polygonArea(poly));
        if (leaf_area < 1e-12) continue;
        // 3. 平面候选像素包围盒 (含 1 像素裕量)
        double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18;
        for (const auto& p : poly) {
            minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
            miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
        }
        int x0 = std::max(0, (int)std::floor(minx - 0.5));
        int x1 = std::min(W - 1, (int)std::ceil(maxx + 0.5));
        int y0 = std::max(0, (int)std::floor(miny - 0.5));
        int y1 = std::min(H - 1, (int)std::ceil(maxy + 0.5));
        if (x0 > x1 || y0 > y1) continue;   // leaf 完全在图像外
        // 4. 逐候选像素: 2D S-H 重叠
        for (int py = y0; py <= y1; py++) {
            for (int px = x0; px <= x1; px++) {
                out.n_candidates++;
                // 像素矩形 (凸, 逆时针)
                std::vector<Point2D> rect = {
                    {px - 0.5, py - 0.5}, {px + 0.5, py - 0.5},
                    {px + 0.5, py + 0.5}, {px - 0.5, py + 0.5}
                };
                std::vector<Point2D> inter =
                    PolyClip::clipPolygon(poly, rect);
                double ov = std::fabs(PolyClip::polygonArea(inter));
                if (ov <= 1e-12) continue;
                out.n_overlaps++;
                double w = ov / leaf_area;
                size_t idx = (size_t)py * W + px;
                out.signal[idx] += sig * w;
                out.coverage[idx] += ov;   // 覆盖面积 (像素单位²)
                out.total_signal_out += sig * w;
            }
        }
        out.n_source_leaf++;
    }
    out.total_signal_in = total_in;
    // 输出 dtype
    if (!use_f32 || in.output_fp64) {
        for (size_t i = 0; i < npix_plane; i++) {
            if (out.signal[i] == 0.0 && !in.no_data_as_zero) {
                out.signal[i] = std::nan("");
                out.coverage[i] = 0.0;
            }
            out.signal_f32[i] = (float)out.signal[i];
            out.coverage_f32[i] = (float)out.coverage[i];
        }
    }
    return true;
}

} // namespace drizzle
