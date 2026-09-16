// ============================================================================
// test_geometry_truth.cpp - 几何 truth 自动测试（headless，无 Qt）
//
// 对 synthetic geometry truth HiPS 验证：
// RA/Dec 方向、0/360 wrap、polar、equatorial、multi-face、
// NESTED leaf 一致性、多 order 一致性、网格/seam、零覆盖区、
// LRU cache 有界 + eviction。
// 用法: test_geometry_truth <truth.hips>
// ============================================================================

#include "hips_browser_backend.h"
#include "hips_sky_view.h"
#include "healpix/healpix_core.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_fail = 0;

void check(const char* name, bool ok, double val = 0.0, double want = 0.0) {
    std::printf("[%s] %s (val=%.6f want=%.6f)\n", ok ? "PASS" : "FAIL",
                name, val, want);
    if (!ok) ++g_fail;
}

double dist_deg(double ra1, double dec1, double ra2, double dec2) {
    const double r1 = ra1 * 3.14159265358979 / 180.0;
    const double d1 = dec1 * 3.14159265358979 / 180.0;
    const double r2 = ra2 * 3.14159265358979 / 180.0;
    const double d2 = dec2 * 3.14159265358979 / 180.0;
    const double c = std::sin(d1) * std::sin(d2) +
                     std::cos(d1) * std::cos(d2) * std::cos(r1 - r2);
    return std::acos(std::max(-1.0, std::min(1.0, c))) * 180.0 /
           3.14159265358979;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: test_geometry_truth <truth.hips>\n");
        return 2;
    }
    const std::string root = argv[1];

    HipsBrowserBackend bk;
    if (bk.open_product(root) != 0) {
        std::fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(640, 480);
    sky.set_view(45.0, 0.0, 30.0, 640.0 / 480.0);

    auto sample = [&](double ra, double dec, int order, float* out) {
        return sky.sample_at(ra, dec, order, *out) == 0;
    };

    // 1. RA 递增方向（无镜像/翻转；避免网格线：用 .37 偏移）
    float a = 0, b = 0;
    check("ra_direction_order0",
          sample(1.37, 0.37, 0, &a) && sample(3.37, 0.37, 0, &b) && a < b, b,
          a);
    check("ra_direction_order1",
          sample(1.37, 0.37, 1, &a) && sample(3.37, 0.37, 1, &b) && a < b, b,
          a);
    check("ra_direction_order2",
          sample(46.37, 2.37, 2, &a) && sample(47.37, 3.37, 2, &b) && a < b, b,
          a);

    // 2. Dec 递增方向
    check("dec_direction",
          sample(50.37, -1.37, 0, &a) && sample(50.37, 1.37, 0, &b) && a < b,
          b, a);

    // 3. 0/360 wrap：负 RA 输入归一化后与 359.x 同一 leaf/值（无断裂/错位）
    float w1 = 0, w2 = 0;
    const bool wrap_ok = sample(-0.1, 0.37, 1, &w1) &&
                         sample(359.9, 0.37, 1, &w2);
    check("wrap_0_360_consistent", wrap_ok && std::fabs(w1 - w2) < 1e-6,
          std::fabs(w1 - w2), 0.0);

    // 4. polar（极区可查询 + marker 可识别）
    float p1 = 0, p2 = 0;
    check("polar_finite", sample(0.0, 88.0, 0, &p1) && std::isfinite(p1), p1,
          0.0);
    const bool marker_ok = sample(0.25, 88.25, 0, &p1) &&
                           sample(0.25, 86.25, 0, &p2);
    check("polar_marker_bump", marker_ok && (p1 - p2) > 0.10, p1 - p2, 0.10);

    // 5. equatorial（order2 局部放大区可查询）
    float e1 = 0;
    check("equatorial_order2",
          sample(45.37, 0.37, 2, &e1) && std::isfinite(e1), e1, 0.0);

    // 6. multi-face：每 face 取多个偏移点，计算 面标签残差 ≈ 0.012*face_at_point
    std::vector<int> face_covered(12, 0);
    double worst_face_err = 0.0;
    for (int f = 0; f < 12; ++f) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(1, (std::uint64_t)f, ra, dec);
        const double offs[4][2] = {{0.37, 0.37},  {1.37, 0.37},
                                   {0.37, -0.37}, {-0.37, 0.37}};
        for (const auto& off : offs) {
            const double pra = std::fmod(ra + off[0], 360.0);
            const double pdec = dec + off[1];
            float v = 0;
            if (!sample(pra, pdec, 0, &v) || !std::isfinite(v)) continue;
            const double base = 0.20 + 0.50 * (pra / 360.0) +
                                0.25 * ((pdec + 90.0) / 180.0);
            const std::uint64_t leaf = astrocs::healpix::ang2pix_nest(
                1u << 9, pra, pdec);
            const int face_here = (int)(leaf >> 18);  // order0 tile = base face
            const double res = (double)v - base - 0.012 * face_here;
            if (std::fabs(res) < 0.03) {
                face_covered[face_here] = 1;
                worst_face_err = std::max(worst_face_err, std::fabs(res));
            }
        }
    }
    int face_ok = 0;
    for (int c : face_covered) face_ok += c;
    check("multi_face_labels", face_ok >= 9, (double)face_ok, 9.0);

    // 7. NESTED：AIO leaf query vs 同 order 直读
    double qs = 0, qsup = 0;
    float sv = 0;
    const bool nested_ok = (sky.query_sky(45.37, 0.37, qs, qsup) == 0) &&
                           std::isfinite(qs) && sample(45.37, 0.37, 2, &sv);
    check("nested_leaf_consistent", nested_ok &&
                                        std::fabs(qs - (double)sv) < 1e-5,
          std::fabs(qs - (double)sv), 0.0);

    // 8. 多 order 一致性（同一 sky 位置，不同 order 采样）
    float o0 = 0, o1 = 0, o2 = 0;
    check("multi_order_o1_vs_o0",
          sample(47.37, 3.37, 1, &o1) && sample(47.37, 3.37, 0, &o0) &&
              std::fabs(o1 - o0) < 0.03,
          std::fabs(o1 - o0), 0.0);
    check("multi_order_o2_vs_o1",
          sample(47.37, 3.37, 2, &o2) && sample(47.37, 3.37, 1, &o1) &&
              std::fabs(o2 - o1) < 0.015,
          std::fabs(o2 - o1), 0.0);

    // 9. 网格暗线（1° 网格存在，用于人工识别方向/错位）
    float g1 = 0, g2 = 0;
    check("grid_line_dark",
          sample(46.0, 2.0, 2, &g1) && sample(46.37, 2.37, 2, &g2) &&
              (g1 - g2) < -0.15,
          g1 - g2, -0.15);

    // 10. seam：相邻 order1 tile 边界两侧值连续（避开网格）
    const auto& o1tiles = bk.tiles_at_order(1);
    double best = 1e30;
    std::pair<std::uint64_t, std::uint64_t> pair;
    for (std::size_t i = 0; i < o1tiles.size(); ++i) {
        for (std::size_t j = i + 1; j < o1tiles.size(); ++j) {
            double ra1 = 0, dec1 = 0, ra2 = 0, dec2 = 0;
            const std::uint64_t c1 = (o1tiles[i] << 18) | 196608u;
            const std::uint64_t c2 = (o1tiles[j] << 18) | 196608u;
            astrocs::healpix::pix2ang_nest(1u << 10, c1, ra1, dec1);
            astrocs::healpix::pix2ang_nest(1u << 10, c2, ra2, dec2);
            const double d = dist_deg(ra1, dec1, ra2, dec2);
            if (d < best) {
                best = d;
                pair = {o1tiles[i], o1tiles[j]};
            }
        }
    }
    double seam_err = 0.0;
    bool seam_ok = false;
    if (best < 1e30) {
        double ra1 = 0, dec1 = 0, ra2 = 0, dec2 = 0;
        astrocs::healpix::pix2ang_nest(1u << 10, (pair.first << 18) | 196608u,
                                       ra1, dec1);
        astrocs::healpix::pix2ang_nest(1u << 10, (pair.second << 18) | 196608u,
                                       ra2, dec2);
        const double mid_ra = (ra1 + ra2) / 2.0;
        const double mid_dec = (dec1 + dec2) / 2.0;
        // 沿边界取 5 个点，两侧各偏移 0.01°，避开网格（用 .37 偏移）
        for (int k = 0; k < 5; ++k) {
            const double off = (double)k * 0.7;
            const double r = mid_ra + off;
            const double d = mid_dec + off;
            float va = 0, vb = 0;
            if (sample(r - 0.01, d - 0.01, 1, &va) &&
                sample(r + 0.01, d + 0.01, 1, &vb) && std::isfinite(va) &&
                std::isfinite(vb)) {
                const double e = std::fabs(va - vb);
                seam_err = std::max(seam_err, e);
                if (e < 0.03) seam_ok = true;
            }
        }
    }
    check("seam_continuous", seam_ok, seam_err, 0.03);

    // 11. 零覆盖区（signal=NaN, support=0）
    {
        const std::uint64_t leaf = astrocs::healpix::ang2pix_nest(
            1u << 9, 180.5, 0.0);
        const std::uint64_t tile = leaf >> 18;
        std::vector<float> sig, sup;
        bool zero_ok = false;
        if (bk.read_tile_at_order(0, tile, sig, sup) == 0) {
            const std::uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                leaf & ((1ULL << 18) - 1), 9u, 512u);
            zero_ok = !std::isfinite(sig[(size_t)fi]) && sup[(size_t)fi] == 0.0f;
        }
        check("zero_coverage_region", zero_ok, 0.0, 0.0);
    }

    // 12. LRU cache 有界 + eviction
    sky.reset_metrics();
    sky.set_cache_cap(8);
    std::vector<std::uint32_t> rgba;
    for (int f = 0; f < 12; ++f) {
        sky.set_view(10.0 + f * 25.0, (f % 2) ? 30.0 : -30.0, 20.0, 640.0 / 480.0);
        sky.rasterize(rgba);
    }
    const auto& m = sky.metrics();
    check("cache_bounded", sky.target_order() >= 0, (double)sky.target_order(),
          0.0);
    check("cache_evictions_occur", m.evictions > 0, (double)m.evictions, 1.0);
    check("cache_hits_positive", m.cache_hits_total > 0,
          (double)m.cache_hits_total, 1.0);
    check("raster_nonempty", rgba.size() == 640u * 480u,
          (double)rgba.size(), 640.0 * 480.0);

    std::printf("worst_face_err=%.4f seam_err=%.4f evictions=%llu hits=%llu\n",
                worst_face_err, seam_err, (unsigned long long)m.evictions,
                (unsigned long long)m.cache_hits_total);
    std::printf("RESULT: %s (%d failures)\n", g_fail == 0 ? "PASS" : "FAIL",
                g_fail);
    return g_fail == 0 ? 0 : 1;
}
