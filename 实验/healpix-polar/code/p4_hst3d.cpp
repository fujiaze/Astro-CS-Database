// ============================================================================
// p4_hst3d.cpp — P3-02 订正探针：HST 真实指向的闭合度量基准修正
//
// 背景（审查 P3-02）: 原 p4_hst.cpp T16 用 2D TAN（经 (ra,dec) 度往返）构造
//   drop，同时用其 VOS 面积做闭合分母。在 HST 原始指向（切点远离极点）处，
//   2D 构造的像素→天球映射与 3D 精确构造不一致（实测 drop 面积比差到 4.2e-9），
//   使闭合分母失真；REC-1 的快路径 2 直接返回 g.drop_area（2D 面积）⇒ 产生
//   5.0e+01 量级的假失败读数。
// 本探针: 一律用 3D 精确 TAN（p = normalize(T + eta*e1 + xi*e2)）构造 drop，
//   面积基准 = 该 3D drop 的 VOS 面积；并输出 a2d/a3d − 1 作为"2D 基准是否可
//   用"的自检量（>1e-12 即该指向下 2D 基准不可用）。
// ============================================================================
#include "polar_common.h"
#include "variants.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace pp;
static double rel(double a, double b) { return (b > 0.0) ? std::fabs(a - b) / b : 0.0; }
static void ipix_to_fij(uint64_t ipix, uint32_t Ns, int& face, uint32_t& i, uint32_t& j) {
    uint64_t per = (uint64_t)Ns * (uint64_t)Ns; face = (int)(ipix / per); uint64_t rem = ipix % per;
    uint32_t xv=0,yv=0; for (int b=0;b<32;++b){xv|=(uint32_t)((rem>>(2*b))&1ull)<<b;yv|=(uint32_t)((rem>>(2*b+1))&1ull)<<b;}
    i=xv;j=yv;
}

struct Cfg { const char* tag; double ra, dec; };

// pixfrac 参数化: 与生产一致
static void scan(const Cfg& cf, uint32_t Ns, double scale_as, double rot,
                 double lo, double hi, double step, double pixfrac, int verbose_rows) {
    healpix::HealpixCore hp((int)Ns);
    const double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs3D w3 = make_wcs3d(cf.ra, cf.dec, scale_as, rot);
    TanWcs   w2 = make_wcs(cf.ra, cf.dec, scale_as, rot);
    printf("\n## T16b(3D 基准) %-22s CRVAL=(%.6f,%.6f) %.4f\"/px rot=%.1f pixfrac=%.2f nside=%u\n",
           cf.tag, cf.ra, cf.dec, scale_as, rot, pixfrac, Ns);
    printf("  %-14s %-11s %-11s %-11s %-11s %-11s %-11s\n",
           "源像素(x,y)", "a2d/a3d-1", "V0 闭合", "REC1 闭合", "oracle 闭合", "REC1L 闭合", "drop[sr]");
    int n=0, f0=0, f1=0, fo=0, f1l=0, f2d=0, fsh=0;
    double w0=0, w1=0, wo=0, w1l=0, w2d=0;
    for (double x = lo; x <= hi + 1e-9; x += step)
    for (double y = lo; y <= hi + 1e-9; y += step) {
        std::vector<V3> d3; if (!build_drop3d(w3, x, y, pixfrac, 1, d3)) continue;
        std::vector<V3> d2; build_drop(w2, x, y, pixfrac, 1, d2);
        const double a3 = vos_area_rotated(d3);
        const double a2 = d2.size() >= 3 ? vos_area_rotated(d2) : 0.0;
        const double r2d = (a3 > 0.0 && a2 > 0.0) ? std::fabs(a2/a3 - 1.0) : 1.0;
        DropGeom g; build_drop_geom(d3, g);
        std::vector<spherical::Vec3> dd; for (auto& p : d3) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
        double s0=0, s1=0, so=0, s1l=0;
        for (uint64_t ipix : cands) {
            int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
            s0  += overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true);
            s1  += overlap_adaptive(g, Ns, f, i, j, 1e-6*a3, 18, nullptr);
            // REC1L: 同 REC-1 但判据预算收紧到 1e-12·A（≥9 层细分余量），
            // 用于分离"预算不可达"与"实现上限"两种解释
            s1l += overlap_adaptive(g, Ns, f, i, j, 1e-12*a3, 18, nullptr);
            so  += oracle_overlap(f, Ns, i, j, d3, 96, 1);
        }
        const double r0 = rel(s0,a3), r1 = rel(s1,a3), ro = rel(so,a3), r1l = rel(s1l,a3);
        ++n;
        w0=std::max(w0,r0); w1=std::max(w1,r1); wo=std::max(wo,ro); w1l=std::max(w1l,r1l); w2d=std::max(w2d,r2d);
        if (r0 > 1e-6) ++f0; if (r1 > 1e-6) ++f1; if (ro > 1e-6) ++fo;
        if (r1l > 1e-6) ++f1l; if (r2d > 1e-12) ++f2d;
        if (n <= verbose_rows || r1 > 1e-6)
            printf("  (%8.1f,%7.1f) %-11.3e %-11.3e %-11.3e %-11.3e %-11.3e %-11.3e\n",
                   x, y, r2d, r0, r1, ro, r1l, a3);
    }
    printf("  ---- 汇总 n=%d : V0 破门=%d 最坏=%.3e | REC1 破门=%d 最坏=%.3e | REC1L(无快路径) 破门=%d 最坏=%.3e |"
           " oracle 破门=%d 最坏=%.3e | 2D基准不可用格=%d 最坏 a2d/a3d-1=%.3e\n",
           n, f0, w0, f1, w1, f1l, w1l, fo, wo, f2d, w2d);
}

int main(int argc, char** argv) {
    uint32_t Ns = (argc > 1) ? (uint32_t)strtoul(argv[1], nullptr, 10) : 2097152u;
    const char* which = (argc > 2) ? argv[2] : "all";
    const char* pf = (argc > 3) ? argv[3] : "1.0";
    const double pixfrac = atof(pf);
    const Cfg pole{"极点(帧中心)",   0.000000,  90.000000};
    const Cfg pole_off{"极点邻域",  30.000000,  89.999000};
    const Cfg orig{"HST 原始指向", 274.721587, -13.841549};
    int bad = 0;
    if (!strcmp(which,"pole") || !strcmp(which,"all"))
        scan(pole, Ns, 0.0400, -35.0, -10.0, 10.0, 5.0, pixfrac, 6);
    if (!strcmp(which,"pole9") || !strcmp(which,"all")) {
        // 全帧 9x9（同原 T16 grid 行, 但基准换 3D）
        healpix::HealpixCore hp((int)Ns);
        (void)hp;
        scan(pole, Ns, 0.0400, -35.0, -4000.0, 4000.0, 1000.0, pixfrac, 0);
    }
    if (!strcmp(which,"orig") || !strcmp(which,"all"))
        scan(orig, Ns, 0.0400, -35.0, -10.0, 10.0, 5.0, pixfrac, 6);
    if (!strcmp(which,"poleoff") || !strcmp(which,"all"))
        scan(pole_off, Ns, 0.0400, -35.0, -10.0, 10.0, 5.0, pixfrac, 6);
    (void)bad;
    return 0;
}
