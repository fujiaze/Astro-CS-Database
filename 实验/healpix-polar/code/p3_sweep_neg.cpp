// ============================================================================
// p3_sweep_neg.cpp — EXP-07-POLAR 实验 3：全天扫描 + 判据非退化（负例注入）
//
//  T11 叶边界弦偏差的空间分布（极冠图 + 关键位置），给出"适用域"
//  T12 全天扫描：14 个 face 角点邻域，V0 / REC-1 / oracle 闭合对照
//  T13 判据非退化：真值无效应 ⇒ 归零；错误算法注入 ⇒ 判红
// ============================================================================
#include "polar_common.h"
#include "variants.h"
#include "chart_native.h"
#include "spherical_overlap.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

using namespace pp;
static double rel(double a, double b) { return (b > 0.0) ? std::fabs(a - b) / b : 0.0; }
static void ipix_to_fij(uint64_t ipix, uint32_t Ns, int& face, uint32_t& i, uint32_t& j) {
    uint64_t per = (uint64_t)Ns * (uint64_t)Ns; face = (int)(ipix / per); uint64_t rem = ipix % per;
    uint32_t xv=0,yv=0; for (int b=0;b<32;++b){xv|=(uint32_t)((rem>>(2*b))&1ull)<<b;yv|=(uint32_t)((rem>>(2*b+1))&1ull)<<b;}
    i=xv;j=yv;
}
static const std::vector<V3>& corners14() {
    static std::vector<V3> v = [] {
        std::vector<V3> out;
        for (int f = 0; f < 12; ++f) {
            const double cs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
            for (auto& c : cs) {
                V3 p = normalize3(chart_uv_to_xyz(f, c[0], c[1], 1));
                bool dup = false;
                for (const auto& q : out) if (dot3(p, q) > 1.0 - 1e-12) { dup = true; break; }
                if (!dup) out.push_back(p);
            }
        }
        return out;
    }();
    return v;
}

// 叶边界最大弦偏差（真实曲线 vs 4 角大圆弧），单位 hp_res
static double leaf_chord_dev(int face, uint32_t Ns, uint32_t i, uint32_t j) {
    double uv[4][2]; leaf_corner_uv(Ns, i, j, uv);
    double worst = 0.0;
    for (int e = 0; e < 4; ++e) {
        int e2 = (e + 1) % 4;
        V3 A = chart_uv_to_xyz(face, uv[e][0], uv[e][1], 1);
        V3 B = chart_uv_to_xyz(face, uv[e2][0], uv[e2][1], 1);
        V3 nn = normalize3(cross3(A, B));
        for (int k = 1; k < 24; ++k) {
            double t = k / 24.0;
            double u = uv[e][0] + t*(uv[e2][0]-uv[e][0]);
            double v = uv[e][1] + t*(uv[e2][1]-uv[e][1]);
            V3 M = chart_uv_to_xyz(face, u, v, 1);
            double d = std::fabs(std::asin(std::max(-1.0, std::min(1.0, dot3(nn, M)))));
            worst = std::max(worst, d);
        }
    }
    return worst;
}

// ---------------------------------------------------------------------------
// T11 弦偏差空间分布
// ---------------------------------------------------------------------------
static void t11_distribution(uint32_t Ns) {
    double hp_res = std::sqrt(kPi/3.0/(double)Ns/(double)Ns);
    printf("\n## T11 叶边界弦偏差 / hp_res 的空间分布 (nside=%u, hp_res=%.4f\")\n", Ns, hp_res*kRad2Arcsec);
    printf("  北极冠 (face0, 距极点 d 个叶, 方位 k*45deg):\n");
    for (int d = 0; d <= 12; ++d) {
        double w = 0.0; int n = 0;
        for (int a = 0; a < 8; ++a) {
            double ang = a * kPi / 4.0;
            // 极冠内 s = (d+0.5)/Ns, phi_t 由 a 决定 -> (u,v)
            double s = (d + 0.5) / (double)Ns;
            double phit = ang;                       // 以 face0 为参考
            if (phit > kHalfPi) continue;
            double u = 1.0 - s + 2.0*s*phit/kPi;
            double v = 1.0 - 2.0*s*phit/kPi;
            if (u >= 1.0 || v >= 1.0 || u < 0.0 || v < 0.0) continue;
            uint32_t i = (uint32_t)(u * Ns), j = (uint32_t)(v * Ns);
            if (i >= Ns || j >= Ns) continue;
            w = std::max(w, leaf_chord_dev(0, Ns, i, j)/hp_res); ++n;
        }
        printf("    d=%2d 叶: 最大弦偏差=%.5f hp_res (采样 %d)\n", d, w, n);
    }
    printf("  其他区域:\n");
    struct L { const char* n; int f; double u, v; };
    std::vector<L> ls = {
        {"face0 极冠/赤道交界 (u+v=1)", 0, 0.5, 0.5},
        {"face0 三角点角 (0,0)",        0, 0.0, 0.0},
        {"face0 三角点角 (1,0)",        0, 1.0, 0.0},
        {"face0 赤道三角内部",          0, 0.2, 0.2},
        {"face4 赤道面中心",            4, 0.5, 0.5},
        {"face4 z=0 角 (1,0)",          4, 1.0, 0.0},
        {"face4 极冠边界 (u+v=1)",      4, 0.5, 0.5},
    };
    for (const auto& x : ls) {
        uint32_t i = (uint32_t)(x.u * Ns), j = (uint32_t)(x.v * Ns);
        if (i >= Ns) i = Ns - 1; if (j >= Ns) j = Ns - 1;
        printf("    %-30s 弦偏差=%.6f hp_res\n", x.n, leaf_chord_dev(x.f, Ns, i, j)/hp_res);
    }
}

// ---------------------------------------------------------------------------
// T12 全天扫描（14 个 face 角点邻域）
// ---------------------------------------------------------------------------
static int t12_sweep(uint32_t Ns, double scale, double lo, double hi, double step,
                     double budget_rel, const char* csv) {
    printf("\n## T12 全天 14 角点邻域扫描 (nside=%u, %.2f\"/px, |d|<=%.1f px step %.2f)\n",
           Ns, scale, hi, step);
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    const std::vector<V3>& C = corners14();
    FILE* fp = csv ? std::fopen(csv, "w") : nullptr;
    if (fp) std::fprintf(fp, "corner,ra,dec,n,worst_v0,worst_rec1,worst_oracle,"
                             "n_fail_v0,n_fail_rec1,n_fail_oracle,seg_per_leaf\n");
    int bad = 0;
    for (size_t ci = 0; ci < C.size(); ++ci) {
        double ra0, dec0; vec_to_radec(C[ci], ra0, dec0);
        TanWcs w = make_wcs(ra0, dec0, scale, 23.5);
        double w0 = 0, w1 = 0, wo = 0;
        int n = 0, f0 = 0, f1 = 0, fo = 0;
        long long seg = 0, nl = 0;
        for (double dx = lo; dx <= hi + 1e-9; dx += step)
        for (double dy = lo; dy <= hi + 1e-9; dy += step) {
            std::vector<V3> drop; if (!build_drop(w, dx, dy, 1.0, 1, drop)) continue;
            DropGeom g; build_drop_geom(drop, g);
            const double adrop = vos_area_rotated(drop);
            std::vector<spherical::Vec3> dd; for (auto& p : drop) dd.push_back({p.x,p.y,p.z});
            std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
            double s0 = 0, s1 = 0, so = 0;
            for (uint64_t ipix : cands) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                s0 += overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true);
                AdaptiveStat st;
                s1 += overlap_adaptive(g, Ns, f, i, j, budget_rel*adrop, 18, &st);
                seg += st.n_seg; ++nl;
                so += oracle_overlap(f, Ns, i, j, drop, 96, 1);
            }
            ++n;
            double r0 = rel(s0, adrop), r1 = rel(s1, adrop), ro = rel(so, adrop);
            w0 = std::max(w0, r0); w1 = std::max(w1, r1); wo = std::max(wo, ro);
            if (r0 > 1e-6) ++f0;
            if (r1 > 1e-6) ++f1;
            if (ro > 1e-6) ++fo;
        }
        printf("  角点%02zu (dec=%+.4f): n=%d | V0 破门=%d 最坏=%.3e | REC-1 破门=%d 最坏=%.3e | oracle 破门=%d 最坏=%.3e | 段/叶=%.1f\n",
               ci, dec0, n, f0, w0, f1, w1, fo, wo, (double)seg/(double)std::max(1LL,nl));
        if (fp) std::fprintf(fp, "%zu,%.6f,%.6f,%d,%.6e,%.6e,%.6e,%d,%d,%d,%.2f\n",
                             ci, ra0, dec0, n, w0, w1, wo, f0, f1, fo,
                             (double)seg/(double)std::max(1LL,nl));
        if (f1 != 0) ++bad;
        if (fo != 0) ++bad;
    }
    if (fp) std::fclose(fp);
    printf("# SUMMARY T12: %s\n", bad == 0 ? "PASS" : "FAIL");
    return bad == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// T13 判据非退化（负例注入）
// ---------------------------------------------------------------------------
static int t13_negatives(uint32_t Ns) {
    printf("\n## T13 判据非退化（负例注入）\n");
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    const double gate = 1e-6;
    int bad = 0;
    auto closure_of = [&](const std::vector<V3>& drop, int mode, int K) {
        DropGeom g; build_drop_geom(drop, g);
        const double adrop = vos_area_rotated(drop);
        std::vector<spherical::Vec3> dd; for (auto& p : drop) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
        double s = 0;
        for (uint64_t ipix : cands) {
            int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
            s += overlap_variant(g, Ns, f, i, j, mode, K, hp_res_rad, true);
        }
        return rel(s, adrop);
    };
    auto oracle_closure = [&](const std::vector<V3>& drop) {
        const double adrop = vos_area_rotated(drop);
        std::vector<spherical::Vec3> dd; for (auto& p : drop) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
        double s = 0;
        for (uint64_t ipix : cands) {
            int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
            s += oracle_overlap(f, Ns, i, j, drop, 96, 1);
        }
        return rel(s, adrop);
    };

    // N1 真值无效应 A: 天区移到赤道（无 face 角点邻域）
    {
        TanWcs w = make_wcs(17.0, 0.0, 0.2, 23.5);
        std::vector<V3> d; build_drop(w, 0, 0, 1.0, 1, d);
        double c = closure_of(d, 0, 1);
        printf("  N1 赤道一般位置 V0 闭合=%.3e  -> %s (须 <1e-6: 度量归零)\n", c, c < gate ? "绿 OK" : "红 FAIL");
        if (!(c < gate)) ++bad;
    }
    // N2 真值无效应 B: drop 缩为极小（仍在 (ra,dec) 表示域内）
    {
        TanWcs w = make_wcs(0.0, 90.0, 0.2, 23.5);
        std::vector<V3> d; build_drop(w, 0, 0, 5e-2, 1, d);
        TanWcs w1 = make_wcs(0.0, 90.0, 0.2, 23.5);
        std::vector<V3> d1; build_drop(w1, 0, 0, 1.0, 1, d1);
        double c = closure_of(d, 0, 1);
        double cor = oracle_closure(d);
        printf("  N2 极点极小 drop (pf=0.05, 面积比=%.3e) V0 闭合=%.3e oracle=%.3e -> %s\n",
               vos_area_rotated(d)/vos_area_rotated(d1), c, cor,
               (c < gate && cor < gate) ? "绿 OK" : "红 FAIL");
        if (!(c < gate && cor < gate)) ++bad;
    }
    // N6 (ra,dec) 表示域边界：drop 角半径 < ~1.5e-8 rad 时 dec 被舍入到 90.000000
    //    ⇒ 离极点偏移整体丢失，drop 退化为 6.1e-17 rad 的小圈。必须可检测。
    {
        printf("  N6 (ra,dec) 表示域：\n");
        for (double pf : {1.0, 0.2, 5e-2, 1e-2, 1e-3}) {
            TanWcs w = make_wcs(0.0, 90.0, 0.2, 23.5);
            std::vector<V3> d; build_drop(w, 0, 0, pf, 1, d);
            double r = 0.0;
            for (const auto& p : d) r = std::max(r, std::atan2(std::hypot(p.x,p.y), p.z));
            double a = vos_area_rotated(d);
            double a_expect = pf*pf*vos_area_rotated([]{
                TanWcs ww = make_wcs(0.0, 90.0, 0.2, 23.5);
                std::vector<V3> dd; build_drop(ww, 0, 0, 1.0, 1, dd); return dd; }());
            printf("    pf=%.0e: drop 角半径=%.3e rad (=%.4f\")  面积/期望=%.4f\n",
                   pf, r, r*kRad2Arcsec, a/a_expect);
        }
        printf("    -> 角半径 < ~1.5e-8 rad (0.003\") 时 dec 舍入到 90 度，面积塌缩；\n");
        printf("       生产路径须对此 fail-closed（本实验只登记，不改生产）。\n");
    }
    // T14 (ra,dec) 往返 vs 直接 3D 构造：极点邻域的 drop 面积保真度
    {
        printf("\n## T14 极点邻域 drop 构造保真度：(ra,dec) 往返 vs 直接 3D\n");
        printf("  %-10s %-14s %-14s %-14s\n", "中心偏移", "drop角半径", "面积比(2D/3D)", "闭合(2D)/闭合(3D)");
        for (double off_px : {0.0, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0}) {
            for (double pf : {1.0, 0.2, 0.05}) {
                TanWcs w2 = make_wcs(0.0, 90.0, 0.2, 23.5);
                TanWcs3D w3 = make_wcs3d(0.0, 90.0, 0.2, 23.5);
                std::vector<V3> d2, d3;
                build_drop(w2, off_px, 0.0, pf, 1, d2);
                build_drop3d(w3, off_px, 0.0, pf, 1, d3);
                double a2 = vos_area_rotated(d2), a3 = vos_area_rotated(d3);
                double r = 0.0;
                for (const auto& p : d3) r = std::max(r, std::atan2(std::hypot(p.x,p.y), p.z));
                double c2 = closure_of(d2, 0, 1), c3 = closure_of(d3, 0, 1);
                printf("  %-10.1f %-14.4e %-14.6f %-6.2e / %.2e\n", off_px, r, a2/a3, c2, c3);
                (void)pf;
            }
        }
        printf("\n## T15 极点邻域 drop 面积的三种构造 (double radec / long-double radec / 直接 3D)\n");
        printf("  %-8s %-13s %-13s %-13s %-13s\n", "中心偏移px", "drop半径rad", "|A_dbl/A_3D-1|", "|A_ld/A_3D-1|", "模型 2.2e-16/r^2");
        for (double off_px : {0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0}) {
            TanWcs w2 = make_wcs(0.0, 90.0, 0.2, 23.5);
            TanWcs3D w3 = make_wcs3d(0.0, 90.0, 0.2, 23.5);
            std::vector<V3> d2, d3;
            build_drop(w2, off_px, 0.0, 1.0, 1, d2);
            build_drop3d(w3, off_px, 0.0, 1.0, 1, d3);
            // long-double radec 路径
            std::vector<V3> dl;
            {
                double half = 0.5;
                double c[4][2] = {{off_px-half,-half},{off_px+half,-half},{off_px+half,half},{off_px-half,half}};
                for (int e = 0; e < 4; ++e) {
                    double ra, dec; w2.pixelToSky(c[e][0], c[e][1], ra, dec);
                    dl.push_back(radec_to_vec_ld(ra, dec));
                }
            }
            double r = 0.0;
            for (const auto& p : d3) r = std::max(r, std::atan2(std::hypot(p.x,p.y), p.z));
            double a2 = vos_area_rotated(d2), a3 = vos_area_rotated(d3), al = vos_area_rotated(dl);
            printf("  %-8.2f %-13.4e %-13.3e %-13.3e %-13.3e\n", off_px, r,
                   std::fabs(a2/a3-1.0), std::fabs(al/a3-1.0), 2.2e-16/(r*r));
        }
    }

    // N3 错误算法注入: 极点用 V0（4 角 + 大圆弧）必须判红
    {
        TanWcs w = make_wcs(0.0, 90.0, 0.2, 23.5);
        double worst = 0; int nfail = 0, n = 0;
        for (double dx = -8; dx <= 8.0001; dx += 0.5)
        for (double dy = -8; dy <= 8.0001; dy += 0.5) {
            std::vector<V3> d; if (!build_drop(w, dx, dy, 1.0, 1, d)) continue;
            double c = closure_of(d, 0, 1); ++n;
            worst = std::max(worst, c); if (c > gate) ++nfail;
        }
        printf("  N3 极点注入 V0: 破门=%d/%d 最坏=%.3e -> %s (须判红)\n", nfail, n, worst,
               nfail > 0 ? "红 OK（判据有效）" : "FAIL（判据退化）");
        if (!(nfail > 0)) ++bad;
    }
    // N4 错误算法注入: 极点用 REC-1（预算 1e-7）必须全绿
    {
        TanWcs w = make_wcs(0.0, 90.0, 0.2, 23.5);
        double worst = 0; int nfail = 0, n = 0;
        for (double dx = -8; dx <= 8.0001; dx += 0.5)
        for (double dy = -8; dy <= 8.0001; dy += 0.5) {
            std::vector<V3> d; if (!build_drop(w, dx, dy, 1.0, 1, d)) continue;
            DropGeom g; build_drop_geom(d, g);
            const double adrop = vos_area_rotated(d);
            std::vector<spherical::Vec3> dd; for (auto& p : d) dd.push_back({p.x,p.y,p.z});
            std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
            double s = 0;
            for (uint64_t ipix : cands) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                s += overlap_adaptive(g, Ns, f, i, j, 1e-7*adrop, 18, nullptr);
            }
            double c = rel(s, adrop); ++n;
            worst = std::max(worst, c); if (c > gate) ++nfail;
        }
        printf("  N4 极点 REC-1(1e-7): 破门=%d/%d 最坏=%.3e -> %s (须全绿)\n", nfail, n, worst,
               nfail == 0 ? "绿 OK" : "FAIL");
        if (!(nfail == 0)) ++bad;
    }
    // N5 oracle 自洽（必须在所有位置 <1e-8）
    {
        double worst = 0;
        for (auto pr : {std::pair<double,double>{0.0,90.0}, {0.0,41.8103}, {0.0,0.0}, {17.0,0.0}}) {
            TanWcs w = make_wcs(pr.first, pr.second, 0.2, 23.5);
            for (double dx = -2; dx <= 2.0001; dx += 1.0)
            for (double dy = -2; dy <= 2.0001; dy += 1.0) {
                std::vector<V3> d; if (!build_drop(w, dx, dy, 1.0, 1, d)) continue;
                worst = std::max(worst, oracle_closure(d));
            }
        }
        printf("  N5 oracle 自身闭合（4 位置 x 25 配置）最坏=%.3e -> %s\n", worst,
               worst < 1e-8 ? "绿 OK" : "FAIL");
        if (!(worst < 1e-8)) ++bad;
    }
    printf("# SUMMARY T13: %s\n", bad == 0 ? "PASS" : "FAIL");
    return bad == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "all";
    uint32_t Ns = 2097152u;
    if (std::strcmp(which, "t11") == 0 || std::strcmp(which, "all") == 0) t11_distribution(Ns);
    if (std::strcmp(which, "t12") == 0) {
        const char* csv = (argc > 2) ? argv[2] : nullptr;
        double lo = (argc > 3) ? atof(argv[3]) : -8.0;
        double hi = (argc > 4) ? atof(argv[4]) :  8.0;
        double st = (argc > 5) ? atof(argv[5]) :  0.5;
        double bg = (argc > 6) ? atof(argv[6]) : 1e-7;
        int rc = t12_sweep(Ns, 0.2, lo, hi, st, bg, csv);
        return rc;
    }
    if (std::strcmp(which, "t13") == 0 || std::strcmp(which, "all") == 0) {
        int rc = t13_negatives(Ns);
        return rc;
    }
    return 0;
}
