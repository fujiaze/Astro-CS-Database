#include "reverse_drizzle.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <functional>

using namespace drizzle;
using Vec3 = spherical::Vec3;
static const double PI = 3.14159265358979323846;

static WcsParams make_wcs(double ra0, double dec0, double scale_arcsec, int size) {
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = ra0; w.crval[1] = dec0;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = scale_arcsec / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;
    return w;
}

struct Ctx { const WcsSip* wcs; };
static bool cb(double px, double py, double& ra, double& dec, void* ud) {
    static_cast<Ctx*>(ud)->wcs->pixelToSky(px, py, ra, dec);
    return true;
}

static double overlap_drop_target_probe(const std::vector<Vec3>& drop,
                                        const std::vector<Vec3>& target) {
    if (drop.size() < 3 || target.size() < 3) return 0.0;
    Vec3 c = {0.0, 0.0, 0.0};
    for (const Vec3& v : target) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = spherical::normalize(c);
    double total = 0.0;
    for (size_t i = 0; i < target.size(); i++) {
        const Vec3& a = target[i];
        const Vec3& b = target[(i + 1) % target.size()];
        const Vec3 tri[3] = { c, a, b };
        Vec3 m = { (c.x + a.x + b.x) / 3.0,
                   (c.y + a.y + b.y) / 3.0,
                   (c.z + a.z + b.z) / 3.0 };
        m = spherical::normalize(m);
        std::vector<Vec3> normals;
        for (int e = 0; e < 3; e++) {
            const Vec3& p0 = tri[e];
            const Vec3& p1 = tri[(e + 1) % 3];
            Vec3 n = spherical::normalize(spherical::cross(p0, p1));
            if (spherical::dot(n, m) < 0.0) n = {-n.x, -n.y, -n.z};
            normals.push_back(n);
        }
        std::vector<Vec3> clipped =
            spherical::sutherland_hodgman_spherical(drop, normals);
        total += spherical::spherical_polygon_area(clipped);
    }
    return total;
}

int main() {
    const int W = 64, H = 64, NSIDE = 1024;
    fprintf(stderr, "A make_wcs\n"); fflush(stderr);
    WcsParams w = make_wcs(272.886595, -23.254083, 30.0, W);
    WcsSip wcs(w);
    fprintf(stderr, "B queryDisc\n"); fflush(stderr);
    healpix::HealpixCore hp(NSIDE, true);
    std::vector<int64_t> cand = hp.queryDisc(272.886595, -23.254083, 2024.0);
    fprintf(stderr, "C queryDisc n=%zu\n", cand.size()); fflush(stderr);
    for (int64_t p : cand) {
        std::vector<Vec3> bnd = spherical::get_healpix_boundary_sampled<double>(
            hp, (uint64_t)p, NSIDE, 8);
        if (bnd.size() < 3) { fprintf(stderr, "C1 empty bnd\n"); fflush(stderr); return 1; }
    }
    fprintf(stderr, "D boundaries ok\n"); fflush(stderr);
    // 单个像素 footprint + overlap
    Ctx ctx{&wcs};
    double scale = 0.5 * (std::sqrt(w.cd[0]*w.cd[0]+w.cd[2]*w.cd[2]) +
                          std::sqrt(w.cd[1]*w.cd[1]+w.cd[3]*w.cd[3])) * PI / 180.0;
    std::vector<Vec3> fp = spherical::build_drop_polygon_adaptive<double>(
        48.0, 48.0, 1.0, cb, &ctx, scale);
    fprintf(stderr, "E footprint verts=%zu\n", fp.size()); fflush(stderr);
    // 直接测量 TAN 边中点对大圆弧平面的偏差
    {
        double x0 = 47.5, y0 = 47.5, x1 = 48.5, y1 = 47.5;
        double ra0, dec0, ra1, dec1, ram, decm;
        wcs.pixelToSky(x0, y0, ra0, dec0);
        wcs.pixelToSky(x1, y1, ra1, dec1);
        wcs.pixelToSky(0.5*(x0+x1), 0.5*(y0+y1), ram, decm);
        Vec3 p0v = spherical::radec_to_vec<double>(ra0, dec0);
        Vec3 p1v = spherical::radec_to_vec<double>(ra1, dec1);
        Vec3 pmv = spherical::radec_to_vec<double>(ram, decm);
        Vec3 n = spherical::normalize(spherical::cross(p0v, p1v));
        double d = n.x*pmv.x + n.y*pmv.y + n.z*pmv.z;
        fprintf(stderr, "E2 tan-edge dev=|asin(%.17g)|=%.3e (eps=1e-15)\n",
                d, std::fabs(std::asin(std::max(-1.0, std::min(1.0, d))))); fflush(stderr);
    }
    Vec3 c = {0,0,0};
    for (auto& v : fp) { c.x+=v.x; c.y+=v.y; c.z+=v.z; }
    c = spherical::normalize(c);
    double pa = spherical::spherical_polygon_area(fp);
    fprintf(stderr, "F pixel area=%.6e\n", pa); fflush(stderr);
    // 恒等重叠: drop = 同一 footprint → 重叠应 ≈ pixel area
    {
        double ov = overlap_drop_target_probe(fp, fp);
        fprintf(stderr, "F2 identity overlap=%.6e area=%.6e ratio=%.4f\n",
                ov, pa, pa > 0 ? ov / pa : 0.0); fflush(stderr);
    }
    // 中心 leaf drop vs 中心像素 footprint
    {
        std::vector<Vec3> bnd = spherical::get_healpix_boundary_sampled<double>(
            hp, (uint64_t)cand[0], NSIDE, 8);
        double ra, dec;
        hp.pix2radec((int64_t)cand[0], &ra, &dec);
        Vec3 center = spherical::radec_to_vec<double>(ra, dec);
        std::vector<Vec3> drop;
        for (const Vec3& v : bnd)
            drop.push_back({v.x, v.y, v.z});
        double ov = overlap_drop_target_probe(drop, fp);
        fprintf(stderr, "F3 center-leaf overlap=%.6e drop_area=%.6e\n",
                ov, spherical::spherical_polygon_area(drop)); fflush(stderr);
        // 直接: 用整个 target 多边形内法向量裁剪 drop
        Vec3 fc = {0,0,0};
        for (const Vec3& v : fp) { fc.x += v.x; fc.y += v.y; fc.z += v.z; }
        fc = spherical::normalize(fc);
        std::vector<Vec3> normals;
        for (size_t i = 0; i < fp.size(); i++) {
            const Vec3& p0 = fp[i];
            const Vec3& p1 = fp[(i + 1) % fp.size()];
            Vec3 n = spherical::normalize(spherical::cross(p0, p1));
            if (spherical::dot(n, fc) < 0.0) n = {-n.x, -n.y, -n.z};
            normals.push_back(n);
        }
        std::vector<Vec3> clipped =
            spherical::sutherland_hodgman_spherical(drop, normals);
        fprintf(stderr, "F4 whole-poly clip area=%.6e verts=%zu\n",
                spherical::spherical_polygon_area(clipped), clipped.size()); fflush(stderr);
        // drop 顶点投影到平面像素坐标
        for (const Vec3& v : drop) {
            double ra, dec, x, y;
            spherical::vec_to_radec<double>(v, ra, dec);
            wcs.skyToPixel(ra, dec, x, y);
            fprintf(stderr, "F5 drop vertex -> (%.2f, %.2f)\n", x, y); fflush(stderr);
        }
    }
    // reverse run: 2 leaves
    std::vector<uint64_t> ipix = {(uint64_t)cand[0], (uint64_t)cand[1]};
    std::vector<double> sig = {1.0, 1.0};
    ReverseDrizzleInput rin;
    rin.nside = NSIDE; rin.nested = true;
    rin.leaf_ipix = ipix; rin.leaf_signal = sig;
    rin.wcs = w; rin.target_width = W; rin.target_height = H;
    rin.pixfrac = 1.0; rin.output_fp64 = true;
    ReverseDrizzle rdz; ReverseDrizzleOutput ro; std::string err;
    fprintf(stderr, "G run\n"); fflush(stderr);
    bool ok = rdz.run(rin, ro, err);
    fprintf(stderr, "H ok=%d err=%s touched=%lld\n", ok, err.c_str(),
            (long long)ro.n_target_pixel_touched); fflush(stderr);
    // 病态 WCS (crval=1e300): 验证不应卡死
    {
        std::vector<uint64_t> all_ipix;
        std::vector<double> all_sig;
        for (int64_t p : cand) {
            double ra, dec;
            hp.pix2radec((int64_t)p, &ra, &dec);
            double d = std::acos(std::max(-1.0, std::min(1.0,
                std::sin(dec*PI/180.0) * std::sin(-23.254083*PI/180.0) +
                std::cos(dec*PI/180.0) * std::cos(-23.254083*PI/180.0) *
                std::cos((ra - 272.886595)*PI/180.0))));
            if (d > (1200.0/3600.0*PI/180.0)) continue;
            all_ipix.push_back((uint64_t)p);
            all_sig.push_back(1.0);
        }
        WcsParams badw = w;
        badw.crval[1] = 91.0;   // DEC 越界 → 应快速拒绝
        ReverseDrizzleInput b;
        b.nside = NSIDE; b.nested = true;
        b.leaf_ipix = all_ipix; b.leaf_signal = all_sig;
        b.wcs = badw; b.target_width = W; b.target_height = H;
        b.pixfrac = 0.8; b.output_fp64 = true;
        auto t0 = std::chrono::steady_clock::now();
        bool bok = rdz.run(b, ro, err);
        auto t1 = std::chrono::steady_clock::now();
        fprintf(stderr, "I bad-wcs ok=%d elapsed=%.2fs err=%s\n", bok,
                std::chrono::duration<double>(t1 - t0).count(),
                err.c_str()); fflush(stderr);
    }
    return 0;
}
