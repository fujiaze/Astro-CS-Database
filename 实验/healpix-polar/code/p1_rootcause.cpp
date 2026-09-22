// ============================================================================
// p1_rootcause.cpp — EXP-07-POLAR 实验 1：极区破门根因定位
//
// 四组测量（全部纯几何/可复算，无 oracle 依赖的只有 T1-T3）：
//   T1 坐标卡自检：等面积性、与生产 pix2ang_nest 的一致性、正逆映射往返
//   T2 叶边界"4 角 + 大圆弧"逼近误差：真实边界曲线到弦的最大角偏差 / hp_res
//   T3 叶角点构造精度：acos(z) 路径 vs 稳定路径（相对 hp_res）
//   T4 极点 V0 破门复现 + 消融：A0(production) / A1(稳定角点) / A2(真实曲线 K 段)
//      / O(oracle, K=256 + Richardson)
//
// 固定 seed：本实验无随机数（全确定性格点）。
// ============================================================================
#include "polar_common.h"
#include "variants.h"
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

// 生产 ipix -> (face, i, j)  [NESTED]
static void ipix_to_fij(uint64_t ipix, uint32_t Ns, int& face, uint32_t& i, uint32_t& j) {
    uint64_t per = (uint64_t)Ns * (uint64_t)Ns;
    face = (int)(ipix / per);
    uint64_t rem = ipix % per;
    uint32_t xv = 0, yv = 0;
    for (int b = 0; b < 32; ++b) {
        xv |= (uint32_t)((rem >> (2*b)) & 1ull) << b;
        yv |= (uint32_t)((rem >> (2*b+1)) & 1ull) << b;
    }
    i = xv; j = yv;
}

// ---------------------------------------------------------------------------
// T1 坐标卡自检
// ---------------------------------------------------------------------------
static void t1_chart_selfcheck() {
    printf("\n## T1 坐标卡自检\n");
    // 数值 Jacobian |dOmega/dudv|（用单位向量叉积模长），对 12 face x 网格
    double jmin = 1e30, jmax = -1e30;
    for (int f = 0; f < 12; ++f) {
        for (int a = 1; a <= 9; ++a) for (int b = 1; b <= 9; ++b) {
            double u = a / 10.0, v = b / 10.0;
            const double h = 1e-5;
            V3 p1 = chart_uv_to_xyz(f, u + h, v, 1), p2 = chart_uv_to_xyz(f, u - h, v, 1);
            V3 q1 = chart_uv_to_xyz(f, u, v + h, 1), q2 = chart_uv_to_xyz(f, u, v - h, 1);
            V3 du{(p1.x-p2.x)/(2*h), (p1.y-p2.y)/(2*h), (p1.z-p2.z)/(2*h)};
            V3 dv{(q1.x-q2.x)/(2*h), (q1.y-q2.y)/(2*h), (q1.z-q2.z)/(2*h)};
            double J = norm3(cross3(du, dv));
            jmin = std::min(jmin, J); jmax = std::max(jmax, J);
        }
    }
    printf("数值 Jacobian |dOmega/dudv|: min=%.15f max=%.15f  解析 pi/3=%.15f  最大相对偏差=%.3e\n",
           jmin, jmax, kJacobian, std::max(std::fabs(jmin-kJacobian), std::fabs(jmax-kJacobian))/kJacobian);

    // 与生产 pix2ang_nest 一致性（叶中心）
    uint32_t Ns = 262144u;
    healpix::HealpixCore hp((int)Ns);
    double worst_ang = 0.0;
    for (int f = 0; f < 12; ++f) {
        for (int a = 1; a <= 5; ++a) for (int b = 1; b <= 5; ++b) {
            uint32_t i = (uint32_t)((double)a/6.0*Ns), j = (uint32_t)((double)b/6.0*Ns);
            uint64_t ipix = (uint64_t)f * (uint64_t)Ns * (uint64_t)Ns + ((uint64_t)i << 0);
            // morton 交织
            uint64_t loc = 0;
            for (int bit = 0; bit < 32; ++bit) {
                loc |= (uint64_t)((i >> bit) & 1u) << (2*bit);
                loc |= (uint64_t)((j >> bit) & 1u) << (2*bit+1);
            }
            ipix = (uint64_t)f * (uint64_t)Ns * (uint64_t)Ns + loc;
            double ra, dec; hp.pix2radec((int64_t)ipix, &ra, &dec);
            V3 a1 = radec_to_vec(ra, dec);
            V3 a2 = leaf_center_xyz(f, Ns, i, j, 1);
            // 用 atan2(|cross|, dot) 测角：acos(dot) 在夹角 ~1e-8 rad 时被
            // 1 ulp 的 dot 舍入主导（acos(1-2e-16)=2.1e-8），不可用作判据。
            V3 cr = cross3(a1, a2);
            double ang = std::atan2(norm3(cr), dot3(a1, a2));
            worst_ang = std::max(worst_ang, ang);
        }
    }
    printf("本实现叶中心 vs 生产 pix2ang_nest: 最大角差=%.3e rad (=%.3e x hp_res)\n",
           worst_ang, worst_ang / std::sqrt(kPi/3.0/Ns/Ns));

    // 正逆映射往返（极点附近）
    double worst_rt = 0.0;
    for (int f = 0; f < 4; ++f) {
        for (int a = 0; a <= 20; ++a) {
            double s = std::pow(10.0, -7.0 + 0.25*a);      // 1e-7 .. ~1e-2 (face 单位)
            for (int b = 0; b < 8; ++b) {
                double phit = (b + 0.5) / 8.0 * kHalfPi;
                double u = 1.0 - s + 2.0*s*phit/kPi, v = 1.0 - 2.0*s*phit/kPi;
                V3 p = chart_uv_to_xyz(f, u, v, 1);
                double u2, v2;
                xyz_to_chart_forced(f, p, u2, v2, 1);
                worst_rt = std::max(worst_rt, std::max(std::fabs(u2-u), std::fabs(v2-v)));
            }
        }
    }
    printf("正逆映射往返 (极冠, s in [1e-7,1e-2], face 单位): 最大 |du|,|dv| = %.3e\n", worst_rt);
}

// ---------------------------------------------------------------------------
// T2 叶边界弦偏差（真实曲线 vs 4 角大圆弧）
// ---------------------------------------------------------------------------
// 真实曲线在参数 t 处到弦平面的角偏差（弦 = 两端点大圆弧）
static double chord_dev_at(int face, uint32_t Ns, uint32_t i, uint32_t j,
                           int edge, double t, double& arc_len) {
    double uv[4][2]; leaf_corner_uv(Ns, i, j, uv);
    int e2 = (edge + 1) % 4;
    auto pt = [&](double tt) {
        double u = uv[edge][0] + tt*(uv[e2][0]-uv[edge][0]);
        double v = uv[edge][1] + tt*(uv[e2][1]-uv[edge][1]);
        return chart_uv_to_xyz(face, u, v, 1);
    };
    V3 A = pt(0.0), B = pt(1.0), M = pt(t);
    V3 n = normalize3(cross3(A, B));
    arc_len = std::acos(std::max(-1.0, std::min(1.0, dot3(A, B))));
    return std::asin(std::max(-1.0, std::min(1.0, dot3(n, M))));
}

struct LocSpec { const char* name; int face; uint32_t i, j; };
static void t2_leaf_boundary_dev(uint32_t Ns) {
    printf("\n## T2 叶边界 '4 角 + 大圆弧' 逼近误差 (nside=%u, hp_res=%.4f\")\n",
           Ns, std::sqrt(kPi/3.0/Ns/Ns)*kRad2Arcsec);
    double hp_res = std::sqrt(kPi/3.0/(double)Ns/(double)Ns);
    std::vector<LocSpec> locs;
    locs.push_back({"北极叶(face0 极冠角)", 0, Ns-1, Ns-1});
    locs.push_back({"北极叶邻格",          0, Ns-2, Ns-1});
    locs.push_back({"三角点叶(face0 u=v=0)", 0, 0, 0});
    locs.push_back({"face0 中心",          0, Ns/2, Ns/2});
    locs.push_back({"赤道面 face4 中心",    4, Ns/2, Ns/2});
    locs.push_back({"极冠/赤道交界 z=2/3",  0, Ns/2, Ns-1});
    for (const auto& L : locs) {
        double worst = 0.0; int we = -1; double wt = 0.0;
        for (int e = 0; e < 4; ++e) {
            for (int k = 1; k < 200; ++k) {
                double t = k / 200.0, alen;
                double d = std::fabs(chord_dev_at(L.face, Ns, L.i, L.j, e, t, alen));
                if (d > worst) { worst = d; we = e; wt = t; }
            }
        }
        printf("  %-24s face=%2d (i,j)=(%u,%u): 最大弦偏差=%.3e rad = %.4f x hp_res  (edge %d, t=%.2f)\n",
               L.name, L.face, L.i, L.j, worst, worst/hp_res, we, wt);
    }
    // 与 nside 的关系（极点叶）
    printf("  极点叶弦偏差/hp_res 随 nside: ");
    for (uint32_t ns : {1024u, 4096u, 16384u, 65536u, 262144u, 2097152u}) {
        double w = 0.0;
        for (int e = 0; e < 4; ++e)
            for (int k = 1; k < 100; ++k) { double alen; w = std::max(w, std::fabs(chord_dev_at(0, ns, ns-1, ns-1, e, k/100.0, alen))); }
        printf("%u:%.4f ", ns, w/std::sqrt(kPi/3.0/ns/ns));
    }
    printf("\n");
}

// ---------------------------------------------------------------------------
// T3 叶角点构造精度（mode0 vs mode1）
// ---------------------------------------------------------------------------
static void t3_corner_precision(uint32_t Ns) {
    printf("\n## T3 叶角点构造精度 (nside=%u, 极点叶)\n", Ns);
    double hp_res = std::sqrt(kPi/3.0/(double)Ns/(double)Ns);
    // 参考：用高精度 (long double) 计算 s -> theta 真值
    double uv[4][2]; leaf_corner_uv(Ns, Ns-1, Ns-1, uv);
    for (int e = 0; e < 4; ++e) {
        double u = uv[e][0], v = uv[e][1];
        double s = 2.0 - u - v;
        long double sl = (long double)2.0L - (long double)u - (long double)v;
        long double th_ref = 2.0L * asinl(sl / sqrtl(6.0L));
        V3 a0 = chart_uv_to_xyz(0, u, v, 0);
        V3 a1 = chart_uv_to_xyz(0, u, v, 1);
        double th0 = std::acos(std::max(-1.0, std::min(1.0, a0.z)));
        double th1 = std::asin(std::max(-1.0, std::min(1.0, std::sqrt(a1.x*a1.x + a1.y*a1.y))));
        printf("  角%d (u=%.9f,v=%.9f) s=%.6e: mode0 theta 误差=%.3e rad (%.3e x hp_res)  mode1 误差=%.3e (%.3e x hp_res)\n",
               e, u, v, s, std::fabs((double)((long double)th0 - th_ref)), std::fabs((double)((long double)th0-th_ref))/hp_res,
               std::fabs((double)((long double)th1 - th_ref)), std::fabs((double)((long double)th1-th_ref))/hp_res);
    }
    // 径向位置误差（横向模长）——决定面积误差的直接量
    for (int e = 0; e < 4; ++e) {
        double u = uv[e][0], v = uv[e][1];
        long double sl = (long double)2.0L - (long double)u - (long double)v;
        long double th_ref = 2.0L * asinl(sl / sqrtl(6.0L));
        long double r_ref = sinl(th_ref);
        V3 a0 = chart_uv_to_xyz(0, u, v, 0);
        V3 a1 = chart_uv_to_xyz(0, u, v, 1);
        double r0 = std::sqrt(a0.x*a0.x + a0.y*a0.y);
        double r1 = std::sqrt(a1.x*a1.x + a1.y*a1.y);
        printf("  角%d 横向半径: mode0 相对误差=%.3e   mode1 相对误差=%.3e\n",
               e, std::fabs((double)((long double)r0 - r_ref))/(double)r_ref,
               std::fabs((double)((long double)r1 - r_ref))/(double)r_ref);
    }
}

// ---------------------------------------------------------------------------
// T4 极点 V0 破门复现 + 消融
// ---------------------------------------------------------------------------
struct SumOut { double sum = 0; double closure = 0; int nleaf = 0; };

// 12 face 的角点去重集合（与既有 face_native.h 同一构造）
static const std::vector<V3>& face_corner_vectors() {
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
// T7 生产 V0 直调交叉校验：本文件的 A0 是否忠实复刻 production
// ---------------------------------------------------------------------------
static int t7_prod_crosscheck(uint32_t Ns, double scale, double rot, double pf, int m,
                              double lo, double hi, double step, const char* out_csv) {
    printf("\n## T7 生产 V0 直调 vs 本文件 A0 复刻 (nside=%u, %.2f\"/px, 中心 dec=90)\n", Ns, scale);
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs w = make_wcs(0.0, 90.0, scale, rot);
    spherical::TargetGeomCache cache(8192);
    FILE* fp = out_csv ? std::fopen(out_csv, "w") : nullptr;
    if (fp) std::fprintf(fp, "dx,dy,adrop,sum_prod,sum_a0,rel_prod,rel_a0,bitdiff_leaves\n");
    int n = 0, nfail_prod = 0, nfail_a0 = 0, n_mismatch = 0;
    double wp = 0, wa = 0, worst_dsum = 0.0;
    for (double dx = lo; dx <= hi + 1e-9; dx += step) {
        for (double dy = lo; dy <= hi + 1e-9; dy += step) {
            std::vector<V3> drop;
            if (!build_drop(w, dx, dy, pf, m, drop)) continue;
            DropGeom g; build_drop_geom(drop, g);
            const double adrop = vos_area_rotated(drop);
            // ---- 生产 V0 直调 ----
            std::vector<spherical::Vec3> d, dd;
            for (const auto& p : drop) d.push_back({p.x, p.y, p.z});
            dd = d;
            spherical::DropGeometryT<double> pg;
            spherical::build_drop_geometry_into<double>(pg, d, &dd);
            std::vector<uint64_t> cands;
            spherical::query_candidate_pixels_fast<double>(dd, hp, cands);
            std::vector<uint64_t> cands2;
            spherical::query_candidate_pixels<double>(dd, hp, cands2);
            if (cands != cands2) ++n_mismatch;
            double sp = 0.0;
            for (uint64_t ipix : cands2)
                sp += spherical::compute_overlap_area_g_ctx_cached<double>(pg, hp, ipix, hp_res_rad, cache);
            // ---- 本文件 A0 ----
            double sa = 0.0;
            for (uint64_t ipix : cands2) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                sa += overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true);
            }
            ++n;
            double rp = rel(sp, adrop), ra = rel(sa, adrop);
            if (rp > 1e-6) ++nfail_prod;
            if (ra > 1e-6) ++nfail_a0;
            wp = std::max(wp, rp); wa = std::max(wa, ra);
            worst_dsum = std::max(worst_dsum, std::fabs(sp - sa) / adrop);
            if (fp) std::fprintf(fp, "%.2f,%.2f,%.6e,%.12e,%.12e,%.4e,%.4e,%d\n",
                                 dx, dy, adrop, sp, sa, rp, ra, (cands != cands2) ? 1 : 0);
        }
    }
    if (fp) std::fclose(fp);
    printf("配置=%d  候选集合 fast/cons 不一致=%d\n", n, n_mismatch);
    printf("  生产 V0 直调      : 破门=%d/%d 最坏闭合=%.4e\n", nfail_prod, n, wp);
    printf("  本文件 A0 复刻    : 破门=%d/%d 最坏闭合=%.4e\n", nfail_a0, n, wa);
    printf("  |sum_prod - sum_A0| / A_drop 最坏 = %.3e  (复刻保真度判据: <=1e-9)\n", worst_dsum);
    return (worst_dsum <= 1e-9) ? 0 : 1;
}

static int scan_at(const char* tag, uint32_t Ns, double ra0, double dec0,
                   double scale, double rot, double pf, int m,
                   double lo, double hi, double step, const char* out_csv) {
    printf("\n## %s (nside=%u, %.2f\"/px, rot=%.1f, pf=%.2f, m=%d, WCS 中心 ra=%.4f dec=%.4f, dx,dy in [%.1f,%.1f] step %.2f px)\n",
           tag, Ns, scale, rot, pf, m, ra0, dec0, lo, hi, step);
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs w = make_wcs(ra0, dec0, scale, rot);
    FILE* fp = out_csv ? std::fopen(out_csv, "w") : nullptr;
    if (fp) std::fprintf(fp, "dx,dy,adrop,"
                             "s0_4c_acos,s1_4c_stbl,s3_curve128_acos,s2_curve8,s2_curve32,s2_curve128,s2_curve512,oracle,"
                             "r0,r1,r3,r2k8,r2k32,r2k128,r2k512,roracle,leafmax_err\n");
    int n = 0, nfail0 = 0, nfail1 = 0, nfail2 = 0, nfailo = 0, nfail3 = 0;
    double w0 = 0, w1 = 0, w2 = 0, wo = 0, w3 = 0;
    double worst_leaf = 0.0;
    for (double dx = lo; dx <= hi + 1e-9; dx += step) {
        for (double dy = lo; dy <= hi + 1e-9; dy += step) {
            std::vector<V3> drop;
            if (!build_drop(w, dx, dy, pf, m, drop)) continue;
            DropGeom g; build_drop_geom(drop, g);
            const double adrop = vos_area_rotated(drop);
            std::vector<spherical::Vec3> dd;
            for (const auto& p : drop) dd.push_back({p.x, p.y, p.z});
            std::vector<uint64_t> cands;
            spherical::query_candidate_pixels<double>(dd, hp, cands);
            double s0 = 0, s1 = 0, s3 = 0, s2a = 0, s2b = 0, s2c = 0, s2d = 0, so = 0;
            double lmax = 0.0;
            for (uint64_t ipix : cands) {
                int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
                VariantStat st;
                s0 += overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true, &st);
                s1 += overlap_variant(g, Ns, f, i, j, 1, 1, hp_res_rad, true, &st);
                s3 += overlap_variant(g, Ns, f, i, j, 3, 128, hp_res_rad, true);   // 曲线 + acos 路径
                double a8   = overlap_variant(g, Ns, f, i, j, 2, 8,   hp_res_rad, true);
                double a32  = overlap_variant(g, Ns, f, i, j, 2, 32,  hp_res_rad, true);
                double a128 = overlap_variant(g, Ns, f, i, j, 2, 128, hp_res_rad, true);
                double a512 = overlap_variant(g, Ns, f, i, j, 2, 512, hp_res_rad, true);
                s2a += a8; s2b += a32; s2c += a128; s2d += a512;
                double ao = oracle_overlap(f, Ns, i, j, drop, 128, 1);
                so += ao;
                // 叶级绝对差归一化到**叶面积**（对细条叶用自身 overlap 归一化无意义）
                double leaf_area = kPi / (3.0 * (double)Ns * (double)Ns);
                lmax = std::max(lmax, std::fabs(a512 - ao) / leaf_area);
            }
            worst_leaf = std::max(worst_leaf, lmax);
            double r0 = rel(s0, adrop), r1 = rel(s1, adrop), r3 = rel(s3, adrop),
                   r2a = rel(s2a, adrop), r2b = rel(s2b, adrop), r2c = rel(s2c, adrop),
                   r2d = rel(s2d, adrop), ro = rel(so, adrop);
            ++n;
            if (r0 > 1e-6) ++nfail0;
            if (r1 > 1e-6) ++nfail1;
            if (r3 > 1e-6) ++nfail3;
            if (r2c > 1e-6) ++nfail2;
            if (ro > 1e-6) ++nfailo;
            w0 = std::max(w0, r0); w1 = std::max(w1, r1); w3 = std::max(w3, r3);
            w2 = std::max(w2, r2c); wo = std::max(wo, ro);
            if (fp) std::fprintf(fp, "%.2f,%.2f,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,"
                                     "%.3e,%.3e,%.3e,%.3e,%.3e,%.3e,%.3e,%.3e,%.3e\n",
                                 dx, dy, adrop, s0, s1, s3, s2a, s2b, s2c, s2d, so,
                                 r0, r1, r3, r2a, r2b, r2c, r2d, ro, lmax);
        }
    }
    if (fp) std::fclose(fp);
    printf("配置数=%d\n", n);
    printf("  A0 production 4角 + acos(z)        : 破门=%d/%d  最坏闭合=%.4e\n", nfail0, n, w0);
    printf("  A1 4角 + 稳定公式                  : 破门=%d/%d  最坏闭合=%.4e\n", nfail1, n, w1);
    printf("  A3 真实曲线K=128 + acos(z) 路径    : 破门=%d/%d  最坏闭合=%.4e   <- 隔离浮点相消(RC2a)\n", nfail3, n, w3);
    printf("  A2 真实曲线K=128 + 稳定公式        : 破门=%d/%d  最坏闭合=%.4e   <- 隔离大圆弧弦逼近(RC1)\n", nfail2, n, w2);
    printf("  O  oracle K=128+Richardson         : 破门=%d/%d  最坏闭合=%.4e  <- 必须接近 0（判据非退化）\n", nfailo, n, wo);
    printf("  A2(K=512) 逐叶 vs oracle 最大相对差 : %.3e\n", worst_leaf);
    return 0;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "all";
    const char* csv = (argc > 2) ? argv[2] : nullptr;
    uint32_t Ns = 2097152u;
    if (std::strcmp(which, "t1") == 0 || std::strcmp(which, "all") == 0) t1_chart_selfcheck();
    if (std::strcmp(which, "t2") == 0 || std::strcmp(which, "all") == 0) {
        t2_leaf_boundary_dev(65536u);
        t2_leaf_boundary_dev(2097152u);
    }
    if (std::strcmp(which, "t3") == 0 || std::strcmp(which, "all") == 0) t3_corner_precision(Ns);
    double lo = (argc > 3) ? atof(argv[3]) : -2.0;
    double hi = (argc > 4) ? atof(argv[4]) :  2.0;
    double st = (argc > 5) ? atof(argv[5]) :  0.5;
    double ra0 = (argc > 6) ? atof(argv[6]) : 0.0;
    double dec0 = (argc > 7) ? atof(argv[7]) : 90.0;
    if (std::strcmp(which, "t4") == 0 || std::strcmp(which, "all") == 0)
        scan_at("T4 极点破门复现 + 消融", Ns, 0.0, 90.0, 0.2, 23.5, 1.0, 1, lo, hi, st, csv);
    if (std::strcmp(which, "t7") == 0) {
        int rc = t7_prod_crosscheck(Ns, 0.2, 23.5, 1.0, 1, lo, hi, st, csv);
        printf("# SUMMARY T7: %s\n", rc == 0 ? "PASS" : "FAIL");
        return rc;
    }
    if (std::strcmp(which, "t5") == 0)
        scan_at("T5 任意天区扫描 + 消融", Ns, ra0, dec0, 0.2, 23.5, 1.0, 1, lo, hi, st, csv);
    return 0;
}
