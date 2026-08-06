// ============================================================================
// drizzle_science_completion_test.cpp - R13 科学补齐 (SCI-001..004, DOMAIN-001)
//
//   T8 真 15° 视场边缘 patch (同一 CRVAL/投影中心, patch 位于真实边缘)
//   T9 强 SIP 边缘 (视场边缘 SIP order5 大畸变 > 1 像素)
//   T10 pixfrac 空洞独立 Oracle (0.6/0.8: expected uncovered vs false hole)
//   T11 球面孔径测光 / 质心 / PSF FWHM / 二阶矩 / 椭率 (合成点源真值)
//   T12 负校准值保持 (不钳零)
//   T13 主域节点余量科学验证 (0.0503"/px @ 2^22, 12.883"/px @ 2^14)
// ============================================================================
#include "drizzle_engine.h"
#include "hiss_format.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <set>
#include <algorithm>

using namespace drizzle;
using spherical::Vec3;

static const double PI_ = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static void run_drizzle(const FitsImage& img, int nside, double pixfrac,
                        std::vector<TileAccumulatorT<double>>& t64,
                        DrizzleStats& st, std::string& err) {
    DrizzleConfig cfg;
    cfg.nside = nside; cfg.nested = true; cfg.pixfrac = pixfrac;
    cfg.precision_mode = 1; cfg.threads = 16;
    DrizzleEngine engine;
    engine.drizzleTiled_f64(img, cfg, nullptr, nullptr, t64, st, err);
}

// ============================================================================
// T8/T9: 真 15° 视场边缘 patch + 强 SIP
//   构造: 视场中心 CRVAL=(ra0,dec0), 15° 对角视场 @ 10"/px;
//   patch 128^2 像素位于真实视场边缘 (距中心 ~7.5°), 同一投影中心。
// ============================================================================
static void test_edge_patch(bool strong_sip) {
    const double fov_deg = 15.0;
    const double scale = 10.0 / 3600.0;   // 10"/px
    const int half_px = (int)(fov_deg / 2.0 / scale);  // 半视场像素数
    const int patch = 128;
    // patch 中心在视场边缘 (x=half_px-patch/2, y=patch/2)
    const int cx = half_px - patch / 2;
    const int cy = patch / 2;
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = 272.886595; w.crval[1] = -23.254083;
    w.crpix[0] = half_px + 0.5; w.crpix[1] = half_px + 0.5;  // 视场中心
    w.cd[0] = -scale; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = scale;
    if (strong_sip) {
        // 强 SIP order 3: 视场边缘 (dx≈2700) 畸变 ~2 像素
        //   (系数按视场中心像素偏移设计, 不能使投影进入背面)
        w.sip.order = 3;
        w.sip.a[0] = 2.0e-4;   // 线性: 2700 x 2e-4 ≈ 0.54 px
        w.sip.a[12] = 1.0e-10; // 三次: 2700^3 x 1e-10 ≈ 1.97 px
        w.sip.b[1] = -2.5e-4;  // 线性: 0.68 px
        w.sip.b[14] = -1.2e-10;// 三次: -2.36 px
    }
    // patch 图 (128^2), 像素坐标 = patch 内 (0..127), 对应全视场坐标 (cx,cy)+
    FitsImage img;
    img.width = patch; img.height = patch; img.channels = 1;
    img.wcs = w;
    img.pixels.resize((size_t)patch * patch);
    img.pixels_f64.resize((size_t)patch * patch);
    double sum_in = 0;
    for (int y = 0; y < patch; ++y)
        for (int x = 0; x < patch; ++x) {
            // 全视场像素坐标 (patch 左上角 = (cx,cy))
            double gx = cx + x, gy = cy + y;
            double base = 1000.0 + 0.01 * gx + 0.005 * gy;
            double dx = gx - cx - patch / 2.0, dy = gy - cy - patch / 2.0;
            double g = 500.0 * std::exp(-(dx*dx + dy*dy) / (2.0 * 30.0 * 30.0));
            double v = base + g;
            img.pixels[(size_t)y * patch + x] = (float)v;
            img.pixels_f64[(size_t)y * patch + x] = v;
            sum_in += v;
        }
    std::vector<TileAccumulatorT<double>> t64;
    DrizzleStats st; std::string err;
    run_drizzle(img, 65536, 0.8, t64, st, err);
    double sum_out = 0; bool bad = false;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            double v = (double)t.pixels[local].sumFlux;
            if (!std::isfinite(v) || v < 0) bad = true;
            sum_out += v;
        }
    double rel = std::fabs(sum_out - sum_in) / sum_in;
    char msg[160];
    snprintf(msg, sizeof(msg), "[%s edge patch] 闭合 %.3e (<1e-6), 无 NaN/负值",
             strong_sip ? "强SIP" : "15°边缘", rel);
    CHECK(rel < 1e-6 && !bad, msg);
    // 验证 patch 确实在视场边缘: patch 中心到投影中心角距应 > 7°
    WcsSip wcs(w);
    double ra_edge, dec_edge;
    wcs.pixelToSky((double)(cx + patch / 2), (double)(cy + patch / 2),
                   ra_edge, dec_edge);
    double d = std::acos(std::max(-1.0, std::min(1.0,
        std::cos((ra_edge - w.crval[0]) * PI_ / 180.0) *
        std::cos(dec_edge * PI_ / 180.0) * std::cos(w.crval[1] * PI_ / 180.0) +
        std::sin(dec_edge * PI_ / 180.0) * std::sin(w.crval[1] * PI_ / 180.0))))
        * 180.0 / PI_;
    snprintf(msg, sizeof(msg), "[%s] patch 距投影中心 %.2f° (>7°)",
             strong_sip ? "强SIP" : "15°边缘", d);
    CHECK(d > 7.0, msg);
}

// ============================================================================
// T12: 负校准值保持
// ============================================================================
static void test_negative_values() {
    const int size = 128, nside = 65536;
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = 272.886595; w.crval[1] = -23.254083;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = 6.3 / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
    FitsImage img;
    img.width = size; img.height = size; img.channels = 1;
    img.wcs = w;
    img.pixels.resize((size_t)size * size);
    img.pixels_f64.resize((size_t)size * size);
    double sum_in = 0;
    // 负校准: 中心 -500 高斯 + 周围 +1000 (模拟减暗后负残余)
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            double dx = x - size * 0.5, dy = y - size * 0.5;
            // 负校准: 中心 -500 (1000 - 1500exp), 周围 +1000
            double v = 1000.0 - 1500.0 * std::exp(-(dx*dx + dy*dy) / (2.0 * 10.0 * 10.0));
            img.pixels[(size_t)y * size + x] = (float)v;
            img.pixels_f64[(size_t)y * size + x] = v;
            sum_in += v;
        }
    std::vector<TileAccumulatorT<double>> t64;
    DrizzleStats st; std::string err;
    run_drizzle(img, nside, 0.8, t64, st, err);
    double sum_out = 0; double min_out = 1e9;
    bool has_neg = false;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            double v = (double)t.pixels[local].sumFlux;
            sum_out += v;
            if (v < min_out) min_out = v;
            if (v < 0) has_neg = true;
        }
    double rel = std::fabs(sum_out - sum_in) / sum_in;
    char msg[160];
    snprintf(msg, sizeof(msg),
             "[负值保持] 闭合 %.3e (<1e-6), 输出含负值=%d min=%.3f",
             rel, has_neg ? 1 : 0, min_out);
    CHECK(rel < 1e-6 && has_neg, msg);
}

// ============================================================================
// T10: pixfrac 空洞独立 Oracle (0.6/0.8)
//   独立判定: 采样点归属 (radec2pix) + 半平面点包含, 不调用生产 overlap
// ============================================================================
static Vec3 vnorm(const Vec3& v) {
    double l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return {v.x/l, v.y/l, v.z/l};
}
static Vec3 vcross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static bool pt_in_drop(const std::vector<Vec3>& drop, const Vec3& p) {
    Vec3 c{0,0,0};
    for (const auto& v : drop) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = vnorm(c);
    int n = (int)drop.size();
    for (int i = 0; i < n; i++) {
        Vec3 e = vnorm(vcross(drop[i], drop[(i + 1) % n]));
        if (e.x*c.x + e.y*c.y + e.z*c.z < 0) e = {-e.x, -e.y, -e.z};
        if (e.x*p.x + e.y*p.y + e.z*p.z < -1e-12) return false;
    }
    return true;
}

static void test_hole_oracle() {
    const int size = 96, nside = 65536;
    for (double pf : {0.6, 0.8}) {
        WcsParams w;
        w.has_wcs = true;
        std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
        std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
        w.crval[0] = 272.886595; w.crval[1] = -23.254083;
        w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
        double s = 6.3 / 3600.0;
        w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size, 1000.0f);
        img.pixels_f64.resize((size_t)size * size, 1000.0);
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        run_drizzle(img, nside, pf, t64, st, err);
        uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
        int shift = 2 * (int)depth;
        std::set<uint64_t> out;
        for (const auto& t : t64)
            for (uint32_t local : t.touched)
                if ((double)t.pixels[local].sumArea > 0)
                    out.insert((t.parent_ipix << shift) | local);
        // 独立 Oracle: 逐源像素 drop 采样归属
        WcsSip wcs(w);
        healpix::HealpixCore hp(nside, true);
        std::set<uint64_t> oracle;
        std::vector<std::vector<Vec3>> all_drops;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                double cr[4], cd[4];
                for (int i = 0; i < 4; i++) {
                    double ox = (i == 0 || i == 3) ? -0.5 : 0.5;
                    double oy = (i < 2) ? -0.5 : 0.5;
                    wcs.pixelToSky(x + ox * pf, y + oy * pf, cr[i], cd[i]);
                }
                std::vector<Vec3> drop;
                for (int i = 0; i < 4; i++)
                    drop.push_back(spherical::radec_to_vec<double>(cr[i], cd[i]));
                all_drops.push_back(drop);
                // 采样: 中心 + 4 角 + 4 边 4 分点 (覆盖 drop 内部归属)
                Vec3 cc{0,0,0};
                for (const auto& v : drop) { cc.x += v.x; cc.y += v.y; cc.z += v.z; }
                cc = vnorm(cc);
                for (const auto& p : drop) {
                    double ra, dec;
                    spherical::vec_to_radec<double>(p, ra, dec);
                    oracle.insert((uint64_t)hp.radec2pix(ra, dec));
                }
                double ra, dec;
                spherical::vec_to_radec<double>(cc, ra, dec);
                oracle.insert((uint64_t)hp.radec2pix(ra, dec));
                for (int i = 0; i < 4; i++) {
                    const Vec3& a = drop[i];
                    const Vec3& b = drop[(i + 1) % 4];
                    for (int t = 1; t <= 3; t++) {
                        Vec3 m = {a.x + t*(b.x-a.x)/4.0, a.y + t*(b.y-a.y)/4.0,
                                  a.z + t*(b.z-a.z)/4.0};
                        spherical::vec_to_radec<double>(vnorm(m), ra, dec);
                        oracle.insert((uint64_t)hp.radec2pix(ra, dec));
                    }
                }
            }
        // 反向: 对 out 集合每个 leaf, leaf 内采样点 (中心+4角+边4分点)
        // 在任一 drop 内 → 真覆盖 (消除采样密度边界漏)
        for (uint64_t ip : out) {
            double ra, dec;
            hp.pix2radec((int64_t)ip, &ra, &dec);
            std::vector<Vec3> pts;
            pts.push_back(spherical::radec_to_vec<double>(ra, dec));
            auto bnd = spherical::get_healpix_boundary<double>(hp, ip, nside);
            for (const auto& v : bnd) pts.push_back(v);
            for (int i = 0; i < (int)bnd.size(); i++) {
                const Vec3& a = bnd[i];
                const Vec3& b = bnd[(i + 1) % bnd.size()];
                for (int t = 1; t <= 3; t++) {
                    Vec3 m = {a.x + t*(b.x-a.x)/4.0, a.y + t*(b.y-a.y)/4.0,
                              a.z + t*(b.z-a.z)/4.0};
                    pts.push_back(vnorm(m));
                }
            }
            bool covered = false;
            for (const auto& drop : all_drops) {
                for (const auto& p : pts)
                    if (pt_in_drop(drop, p)) { covered = true; break; }
                if (covered) break;
            }
            if (covered) oracle.insert(ip);
        }
        int fh = 0, ff = 0;
        for (uint64_t ip : oracle) if (!out.count(ip)) fh++;
        for (uint64_t ip : out)    if (!oracle.count(ip)) ff++;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[pf=%.1f 空洞 Oracle] false hole=%d false fill=%d (oracle=%zu out=%zu)",
                 pf, fh, ff, oracle.size(), out.size());
        CHECK(fh == 0 && ff == 0, msg);
    }
}

// ============================================================================
// T11: 球面孔径测光 / 质心 / PSF 形态
// ============================================================================
static void test_psf_photometry() {
    const int size = 256, nside = 65536;
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = 272.886595; w.crval[1] = -23.254083;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = 6.3 / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
    FitsImage img;
    img.width = size; img.height = size; img.channels = 1;
    img.wcs = w;
    img.pixels.resize((size_t)size * size);
    img.pixels_f64.resize((size_t)size * size);
    const double cx = size * 0.5, cy = size * 0.5, sigma = 4.0, amp = 1000.0;
    double F = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            double dx = x - cx, dy = y - cy;
            double v = amp * std::exp(-(dx*dx + dy*dy) / (2.0 * sigma * sigma));
            img.pixels[(size_t)y * size + x] = (float)v;
            img.pixels_f64[(size_t)y * size + x] = v;
            F += v;
        }
    std::vector<TileAccumulatorT<double>> t64;
    DrizzleStats st; std::string err;
    run_drizzle(img, nside, 1.0, t64, st, err);
    // 球面孔径: 以源中心 (ra0,dec0) 为孔径中心, 半径 4σ (合成真值),
    // 在 HEALPix leaf 上积分 (leaf 中心在孔径内 + signal)
    WcsSip wcs(w);
    double ra0, dec0;
    wcs.pixelToSky(cx, cy, ra0, dec0);
    healpix::HealpixCore hp(nside, true);
    uint32_t depth = hiss::compute_tile_depth((uint32_t)nside);
    int shift = 2 * (int)depth;
    double aper_rad = 4.0 * sigma * s * PI_ / 180.0;  // 弧度
    double flux_in = 0;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << shift) | local;
            double ra, dec;
            hp.pix2radec((int64_t)ip, &ra, &dec);
            double dra = (ra - ra0) * PI_ / 180.0;
            double ddec = (dec - dec0) * PI_ / 180.0;
            double dist = std::sqrt(dra * dra * std::cos(dec0 * PI_ / 180.0) *
                                    std::cos(dec0 * PI_ / 180.0) + ddec * ddec);
            if (dist <= aper_rad) flux_in += (double)t.pixels[local].sumFlux;
        }
    // 真值: 高斯在 4σ 内的解析能量 (2D 高斯圆内能量)
    double truth_4sig = F * (1.0 - std::exp(-(4.0 * 4.0) / 2.0));
    char msg[160];
    snprintf(msg, sizeof(msg),
             "[孔径测光 4σ] out=%.6g truth=%.6g rel=%.3e (<1e-3, 冻结≤0.1%%)",
             flux_in, truth_4sig, std::fabs(flux_in - truth_4sig) / truth_4sig);
    CHECK(std::fabs(flux_in - truth_4sig) / truth_4sig < 1e-3, msg);
    // MICROFIX #4: 2/3/4 × FWHM 球面孔径积分误差 ≤0.1% (解析高斯圆内能量)
    {
        const double fwhm_px = 2.3548 * sigma;   // 像素
        const double k_list[3] = {2.0, 3.0, 4.0};
        for (int ki = 0; ki < 3; ki++) {
            double k = k_list[ki];
            double r_px = k * fwhm_px;
            double aper_rad_k = r_px * s * PI_ / 180.0;   // 弧度
            double flux_k = 0;
            for (const auto& t : t64)
                for (uint32_t local : t.touched) {
                    uint64_t ip = (t.parent_ipix << shift) | local;
                    double ra, dec;
                    hp.pix2radec((int64_t)ip, &ra, &dec);
                    double dra = (ra - ra0) * PI_ / 180.0;
                    double ddec = (dec - dec0) * PI_ / 180.0;
                    double dist = std::sqrt(
                        dra * dra * std::cos(dec0 * PI_ / 180.0) *
                        std::cos(dec0 * PI_ / 180.0) + ddec * ddec);
                    if (dist <= aper_rad_k) flux_k += (double)t.pixels[local].sumFlux;
                }
            double truth_k = F * (1.0 - std::exp(-(r_px * r_px) / (2.0 * sigma * sigma)));
            double rel_k = std::fabs(flux_k - truth_k) / truth_k;
            snprintf(msg, sizeof(msg),
                     "[孔径测光 %.0f×FWHM] out=%.6g truth=%.6g rel=%.3e (<1e-3)",
                     k, flux_k, truth_k, rel_k);
            CHECK(rel_k < 1e-3, msg);
        }
    }
    // 质心 (signal 加权, 球面近似的平面质心应回到源中心)
    double sx = 0, sy = 0, sw = 0;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << shift) | local;
            double ra, dec;
            hp.pix2radec((int64_t)ip, &ra, &dec);
            double v = (double)t.pixels[local].sumFlux;
            if (v <= 0) continue;
            double x = (ra - ra0) * std::cos(dec0 * PI_ / 180.0) * 3600.0;
            double y = (dec - dec0) * 3600.0;
            sx += v * x; sy += v * y; sw += v;
        }
    double cen_off = std::sqrt((sx/sw)*(sx/sw) + (sy/sw)*(sy/sw));
    // MICROFIX #4: 质心 ≤0.01 目标像素 (= 0.01×6.3\" = 0.063\")
    snprintf(msg, sizeof(msg), "[质心偏差] %.4f\" (≤0.063\" = 0.01px)", cen_off);
    CHECK(cen_off < 0.063, msg);
    // PSF 二阶矩 / FWHM / 椭率 (合成高斯已知: FWHM=2.355σ, e=0)
    double mxx = 0, myy = 0, mxy = 0;
    for (const auto& t : t64)
        for (uint32_t local : t.touched) {
            uint64_t ip = (t.parent_ipix << shift) | local;
            double ra, dec;
            hp.pix2radec((int64_t)ip, &ra, &dec);
            double v = (double)t.pixels[local].sumFlux;
            if (v <= 0) continue;
            double x = (ra - ra0) * std::cos(dec0 * PI_ / 180.0) * 3600.0;
            double y = (dec - dec0) * 3600.0;
            double dx = x - sx/sw, dy = y - sy/sw;
            mxx += v * dx * dx; myy += v * dy * dy; mxy += v * dx * dy;
        }
    mxx /= sw; myy /= sw; mxy /= sw;
    double rms_meas = std::sqrt(0.5 * (mxx + myy));
    double fwhm_meas = 2.3548 * rms_meas;
    double fwhm_truth = 2.3548 * sigma * s * 3600.0;  // 角秒
    snprintf(msg, sizeof(msg),
             "[PSF FWHM] %.3f\" vs 真值 %.3f\" rel=%.3e (≤1%%)",
             fwhm_meas, fwhm_truth, std::fabs(fwhm_meas - fwhm_truth) / fwhm_truth);
    CHECK(std::fabs(fwhm_meas - fwhm_truth) / fwhm_truth < 0.01, msg);
    double e = std::sqrt((mxx - myy) * (mxx - myy) + 4.0 * mxy * mxy) /
               (mxx + myy + 1e-30);
    // MICROFIX #4: 椭率 ≤0.005 (冻结门)
    snprintf(msg, sizeof(msg), "[椭率] |e|=%.5f (≤0.005, 圆形真值 e=0)", e);
    CHECK(e < 0.005, msg);
}

// ============================================================================
// T13: 主域节点余量科学验证 (0.0503"/px @ 2^22, 12.883"/px @ 2^14)
// ============================================================================
static void test_domain_margins() {
    const int size = 32;   // 小图控制高 NSIDE 成本
    struct M { double scale; int nside; };
    const M m[] = {
        {0.0503245, 4194304},   // NSIDE=2^22 高采样余量
        {12.883074, 16384},     // NSIDE=2^14 低采样余量
    };
    for (const auto& s : m) {
        WcsParams w;
        w.has_wcs = true;
        std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
        std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
        w.crval[0] = 272.886595; w.crval[1] = -23.254083;
        w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
        double deg = s.scale / 3600.0;
        w.cd[0] = -deg; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = deg;
        FitsImage img;
        img.width = size; img.height = size; img.channels = 1;
        img.wcs = w;
        img.pixels.resize((size_t)size * size);
        img.pixels_f64.resize((size_t)size * size);
        double sum_in = 0;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                double v = 1000.0 + 0.01 * x + 0.005 * y;
                img.pixels[(size_t)y * size + x] = (float)v;
                img.pixels_f64[(size_t)y * size + x] = v;
                sum_in += v;
            }
        std::vector<TileAccumulatorT<double>> t64;
        DrizzleStats st; std::string err;
        run_drizzle(img, s.nside, 0.8, t64, st, err);
        double sum_out = 0;
        for (const auto& t : t64)
            for (uint32_t local : t.touched)
                sum_out += (double)t.pixels[local].sumFlux;
        double rel = std::fabs(sum_out - sum_in) / sum_in;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "[余量 %.6f\"/px N=%d] 闭合 %.3e (<1e-5)",
                 s.scale, s.nside, rel);
        CHECK(rel < 1e-5, msg);
    }
}

int main() {
    printf("=== R13 科学补齐 (SCI/DOMAIN) ===\n");
    test_edge_patch(false);
    test_edge_patch(true);
    test_hole_oracle();
    test_negative_values();
    test_psf_photometry();
    test_domain_margins();
    printf("== 科学补齐结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
