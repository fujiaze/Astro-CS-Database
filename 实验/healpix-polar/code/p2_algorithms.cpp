// ============================================================================
// p2_algorithms.cpp — EXP-07-POLAR 实验 2：候选算法精度/代价对照
//
//  T8  面坐标卡原生交叠：drop 边界在 chart 里的曲率、精度/代价 vs subdiv_rel、
//      角点路径精度 vs 采样数 N（定位 1e-5 触底根因）
//  T9  REC-1（叶边界自适应细分 + 稳定角点公式）：精度/代价 vs 面积误差预算
//  T10 计时：ns/drop 与 ns/leaf
// ============================================================================
#include "polar_common.h"
#include "variants.h"
#include "chart_native.h"
#include "spherical_overlap.h"
#include "healpix_core.h"

#include <chrono>
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

// ---------------------------------------------------------------------------
// T8a drop 边界在 chart 里的弦偏差（曲率度量）
// ---------------------------------------------------------------------------
static void t8a_chart_curvature(uint32_t Ns, double ra0, double dec0, const char* tag) {
    TanWcs w = make_wcs(ra0, dec0, 0.2, 23.5);
    std::vector<V3> drop; build_drop(w, 0.0, 0.0, 1.0, 1, drop);
    V3 c{0,0,0}; for (auto& p : drop) { c.x+=p.x;c.y+=p.y;c.z+=p.z; }
    c = normalize3(c);
    int f = natural_face(c);
    double hp_res_leaf = 1.0;   // 叶尺寸 = 1 chart 单位 (u,v 以 face 为单位时叶 = 1/Ns)
    (void)hp_res_leaf;
    printf("\n## T8a drop 边界在 chart 里的弦偏差 (%s, face=%d)\n", tag, f);
    // 整条边（1 段）的偏差，以及二分 k 次后的最大偏差
    for (int e = 0; e < (int)drop.size(); ++e) {
        const V3& A = drop[e]; const V3& B = drop[(e+1)%drop.size()];
        double ua,va,ub,vb; xyz_to_chart_forced(f,A,ua,va,1); xyz_to_chart_forced(f,B,ub,vb,1);
        double chord = std::hypot(ub-ua, vb-va);
        double worst = 0.0;
        for (int k = 1; k < 200; ++k) {
            double t = k/200.0;
            V3 M = normalize3({A.x+(B.x-A.x)*t, A.y+(B.y-A.y)*t, A.z+(B.z-A.z)*t});
            double um,vm; xyz_to_chart_forced(f,M,um,vm,1);
            double d = std::hypot(um - (ua+(ub-ua)*t), vm - (va+(vb-va)*t));
            worst = std::max(worst, d);
        }
        printf("  边%d: 弦长=%.4f chart单位  最大弦偏差=%.4e (=%.4e x 弦长, =%.4e x 叶尺寸)\n",
               e, chord, worst, worst/chord, worst);
    }
}

// ---------------------------------------------------------------------------
// T8b chart 原生精度/代价 vs subdiv_rel + 角点路径精度 vs N
// ---------------------------------------------------------------------------
struct Pos { const char* tag; double ra, dec; };
static void t8b_chart_native(uint32_t Ns, const Pos& pos, double lo, double hi, double step) {
    printf("\n## T8b chart 原生交叠 (%s ra=%.4f dec=%.4f, nside=%u)\n", pos.tag, pos.ra, pos.dec, Ns);
    TanWcs w = make_wcs(pos.ra, pos.dec, 0.2, 23.5);
    const std::vector<V3>& C = corners14();
    double worst_naive = 0, worst_1e6 = 0, worst_1e8 = 0, worst_1e10 = 0;
    long long pts_1e6 = 0, pts_1e8 = 0, pts_1e10 = 0;
    int ncorner = 0, n = 0;
    double worst_cp_N[4] = {0,0,0,0};   // N = 64,128,256,512
    for (double dx = lo; dx <= hi + 1e-9; dx += step)
    for (double dy = lo; dy <= hi + 1e-9; dy += step) {
        std::vector<V3> drop; if (!build_drop(w, dx, dy, 1.0, 1, drop)) continue;
        const double adrop = vos_area_rotated(drop);
        ++n;
        ChartOpts2 o;
        o.use_corner_path = false; o.adaptive = false;
        ChartAlloc a0 = chart_allocate(drop, Ns, o, C);
        o.adaptive = true;
        o.subdiv_rel = 1e-6;  ChartAlloc a6  = chart_allocate(drop, Ns, o, C);
        o.subdiv_rel = 1e-8;  ChartAlloc a8  = chart_allocate(drop, Ns, o, C);
        o.subdiv_rel = 1e-10; ChartAlloc a10 = chart_allocate(drop, Ns, o, C);
        worst_naive = std::max(worst_naive, rel(a0.sum_area, adrop));
        worst_1e6  = std::max(worst_1e6,  rel(a6.sum_area, adrop));
        worst_1e8  = std::max(worst_1e8,  rel(a8.sum_area, adrop));
        worst_1e10 = std::max(worst_1e10, rel(a10.sum_area, adrop));
        pts_1e6 += a6.n_curve_pts; pts_1e8 += a8.n_curve_pts; pts_1e10 += a10.n_curve_pts;
        bool cc = drop_contains_any_face_corner(drop, C);
        if (cc) {
            ++ncorner;
            const int Ns4[4] = {64,128,256,512};
            for (int q = 0; q < 4; ++q) {
                ChartOpts2 oc; oc.use_corner_path = true; oc.corner_samples = Ns4[q];
                ChartAlloc ac = chart_allocate(drop, Ns, oc, C);
                worst_cp_N[q] = std::max(worst_cp_N[q], rel(ac.sum_area, adrop));
            }
        }
    }
    printf("  配置=%d  含角点=%d\n", n, ncorner);
    printf("  朴素(1段/边, 无角点路径)      : 最坏闭合=%.4e\n", worst_naive);
    printf("  自适应 subdiv_rel=1e-6        : 最坏闭合=%.4e  平均折线点/配置=%.1f\n", worst_1e6, (double)pts_1e6/n);
    printf("  自适应 subdiv_rel=1e-8        : 最坏闭合=%.4e  平均折线点/配置=%.1f\n", worst_1e8, (double)pts_1e8/n);
    printf("  自适应 subdiv_rel=1e-10       : 最坏闭合=%.4e  平均折线点/配置=%.1f\n", worst_1e10, (double)pts_1e10/n);
    printf("  角点路径 N=64/128/256/512     : 最坏闭合=%.4e / %.4e / %.4e / %.4e\n",
           worst_cp_N[0], worst_cp_N[1], worst_cp_N[2], worst_cp_N[3]);
}

// ---------------------------------------------------------------------------
// T9 REC-1 精度/代价 vs 面积误差预算
// ---------------------------------------------------------------------------
static void t9_rec1(uint32_t Ns, const Pos& pos, double lo, double hi, double step) {
    printf("\n## T9 REC-1 叶边界自适应细分 (%s ra=%.4f dec=%.4f, nside=%u)\n", pos.tag, pos.ra, pos.dec, Ns);
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs w = make_wcs(pos.ra, pos.dec, 0.2, 23.5);
    const double budgets[4] = {1e-6, 1e-7, 1e-8, 1e-9};
    double worst[4] = {0,0,0,0};
    long long seg[4] = {0,0,0,0};
    long long nleaf[4] = {0,0,0,0};
    int n = 0;
    double t_oracle = 0;
    for (double dx = lo; dx <= hi + 1e-9; dx += step)
    for (double dy = lo; dy <= hi + 1e-9; dy += step) {
        std::vector<V3> drop; if (!build_drop(w, dx, dy, 1.0, 1, drop)) continue;
        DropGeom g; build_drop_geom(drop, g);
        const double adrop = vos_area_rotated(drop);
        std::vector<spherical::Vec3> dd; for (auto& p : drop) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
        ++n;
        for (int q = 0; q < 4; ++q) {
            double s = 0.0;
            for (uint64_t ipix : cands) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                AdaptiveStat st;
                s += overlap_adaptive(g, Ns, f, i, j, budgets[q] * adrop, 18, &st);
                seg[q] += st.n_seg; ++nleaf[q];
            }
            worst[q] = std::max(worst[q], rel(s, adrop));
        }
        (void)t_oracle;
    }
    printf("  配置=%d\n", n);
    for (int q = 0; q < 4; ++q)
        printf("  budget=%0.0e x A_drop : 最坏闭合=%.4e  平均段数/叶=%.2f  破门=%s\n",
               budgets[q], worst[q], (double)seg[q]/(double)std::max(1LL,nleaf[q]),
               (worst[q] > 1e-6) ? "是" : "否");
}

// ---------------------------------------------------------------------------
// T10 计时（单叶 / 单 drop）
// ---------------------------------------------------------------------------
static void t10_timing(uint32_t Ns) {
    printf("\n## T10 单叶/单 drop 计时 (nside=%u, 极点配置)\n", Ns);
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs w = make_wcs(0.0, 90.0, 0.2, 23.5);
    std::vector<V3> drop; build_drop(w, 1.0, -0.5, 1.0, 1, drop);
    DropGeom g; build_drop_geom(drop, g);
    const double adrop = vos_area_rotated(drop);
    std::vector<spherical::Vec3> dd; for (auto& p : drop) dd.push_back({p.x,p.y,p.z});
    std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
    printf("  候选叶=%zu\n", cands.size());
    auto bench = [&](const char* name, int reps, auto fn) {
        auto t0 = std::chrono::steady_clock::now();
        volatile double acc = 0;
        for (int r = 0; r < reps; ++r) {
            for (uint64_t ipix : cands) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                acc += fn(f, i, j);
            }
        }
        auto t1 = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        printf("  %-34s %8.2f us/drop   %7.1f ns/leaf   (acc=%.3e)\n",
               name, us/reps, us*1000.0/(reps*(double)cands.size()), (double)acc);
    };
    bench("V0 4角 (production 路径)", 2000, [&](int f, uint32_t i, uint32_t j) {
        return overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true); });
    bench("REC-1 budget=1e-6 A_drop", 300, [&](int f, uint32_t i, uint32_t j) {
        return overlap_adaptive(g, Ns, f, i, j, 1e-6*adrop, 18, nullptr); });
    bench("REC-1 budget=1e-7 A_drop", 300, [&](int f, uint32_t i, uint32_t j) {
        return overlap_adaptive(g, Ns, f, i, j, 1e-7*adrop, 18, nullptr); });
    bench("REC-1 budget=1e-8 A_drop", 200, [&](int f, uint32_t i, uint32_t j) {
        return overlap_adaptive(g, Ns, f, i, j, 1e-8*adrop, 18, nullptr); });
    bench("oracle K=128+Richardson", 20, [&](int f, uint32_t i, uint32_t j) {
        return oracle_overlap(f, Ns, i, j, drop, 128, 1); });
    const std::vector<V3>& C = corners14();
    bench("chart-native subdiv_rel=1e-6", 200, [&](int, uint32_t, uint32_t) {
        ChartOpts2 o; o.subdiv_rel = 1e-6;
        return chart_allocate(drop, Ns, o, C).sum_area; });
    bench("chart-native subdiv_rel=1e-8", 100, [&](int, uint32_t, uint32_t) {
        ChartOpts2 o; o.subdiv_rel = 1e-8;
        return chart_allocate(drop, Ns, o, C).sum_area; });
}

int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "all";
    uint32_t Ns = 2097152u;
    std::vector<Pos> poss = {
        {"极点",     0.0, 90.0},
        {"三角点",   0.0, 41.8103},
        {"赤道角点", 0.0, 0.0},
        {"赤道一般", 17.0, 0.0},
    };
    if (std::strcmp(which, "t8a") == 0 || std::strcmp(which, "all") == 0) {
        for (const auto& p : poss) t8a_chart_curvature(Ns, p.ra, p.dec, p.tag);
    }
    if (std::strcmp(which, "t8b") == 0 || std::strcmp(which, "all") == 0) {
        for (const auto& p : poss) t8b_chart_native(Ns, p, -3.0, 3.0, 1.0);
    }
    if (std::strcmp(which, "t9") == 0 || std::strcmp(which, "all") == 0) {
        for (const auto& p : poss) t9_rec1(Ns, p, -3.0, 3.0, 1.0);
    }
    if (std::strcmp(which, "t10") == 0 || std::strcmp(which, "all") == 0) t10_timing(Ns);
    return 0;
}
