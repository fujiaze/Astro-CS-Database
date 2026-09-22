// ============================================================================
// p4_hst.cpp — EXP-07-POLAR 实验 4：HST 真实信号模板（WCS 旋转到极点）
//
// 数据: testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f502n_v1_drz.fits 头部
//   NAXIS=8000x8400, CRPIX=(4000,4200), CRVAL=(274.721587,-13.841549),
//   CD=[[-9.1016831619991e-06,-6.3731918728008e-06],
//       [-6.3730803948928e-06, 9.10186123591135e-06]]  (0.0400"/px, ORIENTAT=-35deg)
//   CTYPE=RA---TAN/DEC--TAN, 无 SIP（drz 已去畸变）
// 变换: 保持 CD 与 CRPIX，把 CRVAL 旋到极点 (0,90) ⇒ "真实帧旋转到极点"。
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
static const double HST_CD[2][2] = {
    {-9.1016831619991e-06, -6.3731918728008e-06},
    {-6.3730803948928e-06,  9.10186123591135e-06}};

struct Cfg { double crval_ra, crval_dec, crpix1, crpix2, rot_deg, scale_arcsec; };

// 说明: 探针的 make_wcs 以 (0,0) 为切点像元; HST 真实 CRPIX=(4000,4200),
// 因此传入的 (x,y) 是"相对 CRPIX 的偏移", 帧中心 = (0,0) = 切点。
static int scan(const Cfg& cf, uint32_t Ns, double lo, double hi, double step, const char* tag) {
    healpix::HealpixCore hp((int)Ns);
    double hp_res_rad = hp.pixelResolutionArcsec() * kArcsec2Rad;
    TanWcs w = make_wcs(cf.crval_ra, cf.crval_dec, cf.scale_arcsec, cf.rot_deg);
    printf("\n## T16 HST 模板 %s: CRVAL=(%.6f,%.6f) %.4f\"/px rot=%.1f  nside=%u\n",
           tag, cf.crval_ra, cf.crval_dec, cf.scale_arcsec, cf.rot_deg, Ns);
    printf("  %-18s %-13s %-13s %-13s %-13s %-13s\n",
           "源像素(x,y)", "离极点\"", "V0 闭合", "REC-1 闭合", "oracle 闭合", "drop面积比");
    int n = 0, f0 = 0, f1 = 0;
    double w0 = 0, w1 = 0, wo = 0, wa = 0;
    for (double x = lo; x <= hi + 1e-9; x += step)
    for (double y = lo; y <= hi + 1e-9; y += step) {
        std::vector<V3> d; if (!build_drop(w, x, y, 1.0, 1, d)) continue;
        DropGeom g; build_drop_geom(d, g);
        const double adrop = vos_area_rotated(d);
        // 3D 精确构造（同一切点/尺度/取向）作为 drop 面积基准
        TanWcs3D w3 = make_wcs3d(cf.crval_ra, cf.crval_dec, cf.scale_arcsec, cf.rot_deg);
        std::vector<V3> d3; build_drop3d(w3, x, y, 1.0, 1, d3);
        const double a3 = vos_area_rotated(d3);
        double rpole = 1e9;
        for (const auto& p : d3) rpole = std::min(rpole, std::atan2(std::hypot(p.x,p.y), std::fabs(p.z)));
        std::vector<spherical::Vec3> dd; for (auto& p : d) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> cands; spherical::query_candidate_pixels<double>(dd, hp, cands);
        double s0 = 0, s1 = 0, so = 0;
        for (uint64_t ipix : cands) {
            int f; uint32_t i, j; ipix_to_fij(ipix, Ns, f, i, j);
            s0 += overlap_variant(g, Ns, f, i, j, 0, 1, hp_res_rad, true);
            s1 += overlap_adaptive(g, Ns, f, i, j, 1e-6*adrop, 18, nullptr);
            so += oracle_overlap(f, Ns, i, j, d, 96, 1);
        }
        double r0 = rel(s0, adrop), r1 = rel(s1, adrop), ro = rel(so, adrop);
        double ra_ = std::fabs(adrop/a3 - 1.0);
        ++n; w0 = std::max(w0,r0); w1 = std::max(w1,r1); wo = std::max(wo,ro); wa = std::max(wa,ra_);
        if (r0 > 1e-6) ++f0; if (r1 > 1e-6) ++f1;
        if (n <= 12 || r0 > 1e-6)
            printf("  (%7.1f,%7.1f)   %-13.4f %-13.3e %-13.3e %-13.3e %-13.3e\n",
                   x, y, rpole*kRad2Arcsec, r0, r1, ro, ra_);
    }
    printf("  ---- 汇总 n=%d : V0 破门=%d 最坏=%.3e | REC-1 破门=%d 最坏=%.3e | oracle 最坏=%.3e | drop面积比最坏=%.3e\n",
           n, f0, w0, f1, w1, wo, wa);
    return (f1 == 0) ? 0 : 1;
}

int main(int argc, char** argv) {
    uint32_t Ns = 2097152u;
    const char* which = (argc > 1) ? argv[1] : "all";
    if (argc > 2) Ns = (uint32_t)strtoul(argv[2], nullptr, 10);
    int bad = 0;
    Cfg hst_pole{0.0, 90.0, 4000.0, 4200.0, -35.0, 0.0400};
    Cfg hst_orig{274.721587, -13.841549, 4000.0, 4200.0, -35.0, 0.0400};
    if (std::strcmp(which, "center") == 0 || std::strcmp(which, "all") == 0)
        bad += scan(hst_pole, Ns, -10.0, 10.0, 2.0, "极点(帧中心 +-10px, 2px 栅格)");
    if (std::strcmp(which, "grid") == 0 || std::strcmp(which, "all") == 0)
        bad += scan(hst_pole, Ns, -4000.0, 4000.0, 1000.0, "极点(全帧 9x9 栅格)");
    if (std::strcmp(which, "orig") == 0 || std::strcmp(which, "all") == 0)
        bad += scan(hst_orig, Ns, -10.0, 10.0, 5.0, "原始指向 M16(负对照)");
    printf("\n# SUMMARY T16: %s\n", bad == 0 ? "PASS" : "FAIL");
    return bad;
}
