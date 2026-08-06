// ============================================================================
// reverse_drizzle_science_test.cpp - 反向 Drizzle 解析球面科学矩阵 (签字修正)
//
// 覆盖 (templates/REVERSE_SCIENCE_RESULT.json):
//   cases: constant_sphere, gradient_sphere, compact_source, negative_field,
//          partial_support, cross_face, ra0, polar, rotated_cd, sip_edge
//   pixfrac: 0.6 / 0.8 / 1.0
//   模式: fp32_to_fp32 (真实 float 累计), fp64_to_fp64
//   指标: false_hole, false_fill, signal_relative_error, coverage_relative_error
//
// 解析真值 (独立于生产 overlap 机制):
//   - pf=1.0 且像素处于图像内区 (距边缘 ≥ M px): 源 leaf 铺满该区,
//     truth_j = ∫_{pixel_j} B(Ω) dΩ (常数场 = B0×A_pixel; 一般场用独立
//     球面三角形扇细分积分 depth=7);
//   - pf<1.0: drop 收缩产生固有空隙, 逐像素解析真值不可行 → 门为
//     (a) 总通量守恒上界 (total_out ≤ total_in), (b) 表面亮度自洽
//     signal_j ≈ B0×A_pixel_j×coverage_j (常数场, coverage>0.05),
//     (c) coverage∈[0,1], (d) fp32/fp64 一致性。
// ============================================================================
#include "reverse_drizzle.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <functional>

using namespace drizzle;
using Vec3 = spherical::Vec3;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static const double PI = 3.14159265358979323846;

static Vec3 nrm(const Vec3& v) {
    double l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return (l < 1e-300) ? Vec3{0,0,1} : Vec3{v.x/l, v.y/l, v.z/l};
}

// ---- 独立球面几何 (Eriksson 面积 + 向量面积) ----
static double tri_area_ref(const Vec3& a, const Vec3& b, const Vec3& c) {
    double det = a.x * (b.y * c.z - b.z * c.y) +
                 a.y * (b.z * c.x - b.x * c.z) +
                 a.z * (b.x * c.y - b.y * c.x);
    double denom = 1.0 + (a.x*b.x + a.y*b.y + a.z*b.z) +
                           (b.x*c.x + b.y*c.y + b.z*c.z) +
                           (c.x*a.x + c.y*a.y + c.z*a.z);
    return std::fabs(2.0 * std::atan2(det, denom));
}

static double integrate_tri(const Vec3& a, const Vec3& b, const Vec3& c,
                            const std::function<double(const Vec3&)>& B,
                            int depth) {
    if (depth == 0) {
        Vec3 m = nrm({(a.x+b.x+c.x)/3.0, (a.y+b.y+c.y)/3.0, (a.z+b.z+c.z)/3.0});
        return B(m) * tri_area_ref(a, b, c);
    }
    Vec3 ab = nrm({(a.x+b.x)/2.0, (a.y+b.y)/2.0, (a.z+b.z)/2.0});
    Vec3 bc = nrm({(b.x+c.x)/2.0, (b.y+c.y)/2.0, (b.z+c.z)/2.0});
    Vec3 ca = nrm({(c.x+a.x)/2.0, (c.y+a.y)/2.0, (c.z+a.z)/2.0});
    return integrate_tri(a, ab, ca, B, depth-1) +
           integrate_tri(b, bc, ab, B, depth-1) +
           integrate_tri(c, ca, bc, B, depth-1) +
           integrate_tri(ab, bc, ca, B, depth-1);
}

static double integrate_poly(const std::vector<Vec3>& poly,
                             const std::function<double(const Vec3&)>& B,
                             int depth) {
    if (poly.size() < 3) return 0.0;
    Vec3 c = {0,0,0};
    for (const Vec3& v : poly) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = nrm(c);
    double total = 0.0;
    for (size_t i = 0; i < poly.size(); i++)
        total += integrate_tri(c, poly[i], poly[(i+1)%poly.size()], B, depth);
    return total;
}

// 球面多边形向量面积 V = (1/2)Σ v_i × v_{i+1} (大圆弧边精确);
// |V| = 球面面积 (凸多边形含于半球); ∫_poly (c·n) dΩ = c·V (线性场精确)。
// 符号规范: V 与外法向一致 (V·质心 > 0); 顶点绕序可能因 WCS 方向反转
// (如 cd[0]<0) 而变为顺时针, 叉积和符号随绕序变化, 必须规范化。
static void vector_area(const std::vector<Vec3>& poly, Vec3& V) {
    V = {0.0, 0.0, 0.0};
    const size_t n = poly.size();
    for (size_t i = 0; i < n; i++) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % n];
        V.x += a.y * b.z - a.z * b.y;
        V.y += a.z * b.x - a.x * b.z;
        V.z += a.x * b.y - a.y * b.x;
    }
    V.x *= 0.5; V.y *= 0.5; V.z *= 0.5;
    Vec3 c = {0,0,0};
    for (const Vec3& v : poly) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = nrm(c);
    if (V.x*c.x + V.y*c.y + V.z*c.z < 0.0) {
        V.x = -V.x; V.y = -V.y; V.z = -V.z;
    }
}

// 线性场 B(v) = b0 + c·v 的多边形积分: b0×A + c·V (解析精确)
static double linear_poly_integral(const std::vector<Vec3>& poly,
                                   double b0, const Vec3& c) {
    Vec3 V;
    vector_area(poly, V);
    double A = std::sqrt(V.x*V.x + V.y*V.y + V.z*V.z);
    return b0 * A + (c.x*V.x + c.y*V.y + c.z*V.z);
}

// ---- WCS 回调 ----
struct Ctx { const WcsSip* wcs; };
static bool cb(double px, double py, double& ra, double& dec, void* ud) {
    static_cast<Ctx*>(ud)->wcs->pixelToSky(px, py, ra, dec);
    return true;
}

static WcsParams make_wcs(double ra0, double dec0, double scale_arcsec,
                          int size, double rot_deg = 0.0) {
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = ra0; w.crval[1] = dec0;
    w.crpix[0] = size / 2.0 + 0.5; w.crpix[1] = size / 2.0 + 0.5;
    double s = scale_arcsec / 3600.0;
    double cs = std::cos(rot_deg * PI / 180.0);
    double sn = std::sin(rot_deg * PI / 180.0);
    w.cd[0] = -s * cs; w.cd[1] =  s * sn;
    w.cd[2] =  s * sn; w.cd[3] =  s * cs;
    return w;
}

// ---- 图像球面覆盖圆 (边/角采样) ----
static void image_cover_circle(const WcsSip& wcs, int W, int H,
                               Vec3& center, double& radius) {
    Ctx ctx{&wcs};
    std::vector<Vec3> pts;
    auto add = [&](double x, double y) {
        double ra, dec;
        wcs.pixelToSky(x, y, ra, dec);
        pts.push_back(spherical::radec_to_vec<double>(ra, dec));
    };
    add(0,0); add(W-1,0); add(W-1,H-1); add(0,H-1);
    const int N = 16;
    for (int i = 1; i < N; i++) {
        double t = (double)i / N;
        add(t*(W-1), 0); add(t*(W-1), H-1); add(0, t*(H-1)); add(W-1, t*(H-1));
    }
    Vec3 c = {0,0,0};
    for (const Vec3& v : pts) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = nrm(c);
    double r = 0.0;
    for (const Vec3& v : pts) {
        double d = std::acos(std::max(-1.0, std::min(1.0,
            c.x*v.x + c.y*v.y + c.z*v.z)));
        if (d > r) r = d;
    }
    center = c; radius = r;
}

// ---- 生成输入 leaf: 图像覆盖圆 + 裕量内的全部 leaf ----
// sig 计算: 线性场用解析向量面积; 否则数值积分 (depth=4)。
static void make_leaves(const WcsParams& w, int W, int H, int nside,
                        double pixfrac,
                        const std::function<double(const Vec3&)>& B,
                        bool linear, double b0, const Vec3& c,
                        std::vector<uint64_t>& ipix,
                        std::vector<double>& sig,
                        std::vector<double>& support) {
    WcsSip wcs(w);
    healpix::HealpixCore hp(nside, true);
    Vec3 cc; double cover_r;
    image_cover_circle(wcs, W, H, cc, cover_r);
    double hp_res = hp.pixelResolutionArcsec() * (PI / (180.0*3600.0));
    double qr = cover_r + 4.0 * hp_res;
    double ra_c, dec_c;
    spherical::vec_to_radec<double>(cc, ra_c, dec_c);
    std::vector<int64_t> cand = hp.queryDisc(ra_c, dec_c, qr * (180.0*3600.0/PI));
    for (int64_t p : cand) {
        uint64_t ip = (uint64_t)p;
        std::vector<Vec3> bnd = spherical::get_healpix_boundary_sampled<double>(
            hp, ip, nside, 8);
        if (bnd.size() < 3) continue;
        double ra, dec;
        hp.pix2radec((int64_t)ip, &ra, &dec);
        Vec3 center = spherical::radec_to_vec<double>(ra, dec);
        std::vector<Vec3> drop;
        drop.reserve(bnd.size());
        for (const Vec3& v : bnd)
            drop.push_back(pixfrac >= 1.0 ? v
                : nrm({center.x + pixfrac*(v.x-center.x),
                       center.y + pixfrac*(v.y-center.y),
                       center.z + pixfrac*(v.z-center.z)}));
        double dc = std::acos(std::max(-1.0, std::min(1.0,
            cc.x*center.x + cc.y*center.y + cc.z*center.z)));
        double drop_r = 0.0;
        for (const Vec3& v : drop) {
            double d = std::acos(std::max(-1.0, std::min(1.0,
                center.x*v.x + center.y*v.y + center.z*v.z)));
            if (d > drop_r) drop_r = d;
        }
        if (dc > cover_r + drop_r + 2.0*hp_res) continue;  // 不可能触及图像
        double s = linear ? linear_poly_integral(drop, b0, c)
                          : integrate_poly(drop, B, 4);
        ipix.push_back(ip);
        sig.push_back(s);
        support.push_back(1.0);
    }
}

// ---- 独立目标像素真值 (pf=1 内区) ----
// 线性场: b0×A + c·V (解析); 一般场: 数值积分 depth=4。
static void reference_field(const WcsParams& w, int W, int H,
                            const std::function<double(const Vec3&)>& B,
                            bool linear, double b0, const Vec3& c,
                            std::vector<double>& truth) {
    WcsSip wcs(w);
    Ctx ctx{&wcs};
    double scale = 0.5 * (std::sqrt(w.cd[0]*w.cd[0]+w.cd[2]*w.cd[2]) +
                          std::sqrt(w.cd[1]*w.cd[1]+w.cd[3]*w.cd[3])) * PI / 180.0;
    truth.assign((size_t)W*H, 0.0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            std::vector<Vec3> fp = spherical::build_drop_polygon_adaptive<double>(
                (double)x, (double)y, 1.0, cb, &ctx, scale);
            if (fp.size() < 3) continue;
            if (linear) {
                truth[(size_t)y*W + x] = linear_poly_integral(fp, b0, c);
            } else {
                truth[(size_t)y*W + x] = integrate_poly(fp, B, 4);
            }
        }
}

// ---- 单个 case 运行 ----
struct CaseResult {
    double signal_rel_err = 0.0;
    double coverage_rel_err = 0.0;
    double sb_err = 0.0;            // 表面亮度自洽 (pf<1 常数场)
    int false_hole = 0;
    int false_fill = 0;
    double fp32_max_rel = 0.0;
    double total_in = 0.0, total_out = 0.0;
    double total_covered_area_out = 0.0;
    int inner_pixels = 0;
};

static CaseResult run_case(const WcsParams& w, int W, int H, int nside,
                           double pixfrac, double B0,
                           const std::function<double(const Vec3&)>& B,
                           bool linear, double b0, const Vec3& c,
                           bool sb_check,
                           const std::vector<double>* support_override = nullptr) {
    CaseResult cr;
    std::vector<uint64_t> ipix;
    std::vector<double> sig, support;
    make_leaves(w, W, H, nside, pixfrac, B, linear, b0, c, ipix, sig, support);
    if (support_override) support = *support_override;
    std::vector<double> truth;
    reference_field(w, W, H, B, linear, b0, c, truth);

    const int M = 8;  // 内区裕量 (px): 2×leaf 半径 + 像素半径
    auto inner = [&](int x, int y) {
        return x >= M && y >= M && x < W - M && y < H - M;
    };

    ReverseDrizzleInput rin;
    rin.nside = nside; rin.nested = true;
    rin.leaf_ipix = ipix; rin.leaf_signal = sig;
    rin.leaf_support = support;
    rin.wcs = w; rin.target_width = W; rin.target_height = H;
    rin.pixfrac = pixfrac; rin.output_fp64 = true;
    ReverseDrizzle rdz; ReverseDrizzleOutput ro64; std::string err;
    if (!rdz.run(rin, ro64, err)) {
        printf("    fp64 失败: %s\n", err.c_str());
        cr.signal_rel_err = 1.0; cr.coverage_rel_err = 1.0;
        return cr;
    }
    ReverseDrizzleInput rin32 = rin;
    rin32.leaf_signal.clear();
    rin32.leaf_signal_f32.assign(sig.begin(), sig.end());
    rin32.output_fp64 = false;
    ReverseDrizzleOutput ro32;
    if (!rdz.run(rin32, ro32, err)) {
        printf("    fp32 失败: %s\n", err.c_str());
        cr.signal_rel_err = 1.0; cr.coverage_rel_err = 1.0;
        return cr;
    }

    double denom_s = 0, err_s = 0, denom_c = 0, err_c = 0;
    double denom_sb = 0, err_sb = 0;
    double max_v = 0.0;
    // 像素球面面积 (独立参考, 常数场 SB 自洽用)
    std::vector<double> pixel_area;
    if (pixfrac < 1.0) {
        WcsSip wcs(w);
        Ctx ctx{&wcs};
        double scale = 0.5 * (std::sqrt(w.cd[0]*w.cd[0]+w.cd[2]*w.cd[2]) +
                              std::sqrt(w.cd[1]*w.cd[1]+w.cd[3]*w.cd[3])) * PI / 180.0;
        pixel_area.resize((size_t)W*H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                auto fp = spherical::build_drop_polygon_adaptive<double>(
                    (double)x, (double)y, 1.0, cb, &ctx, scale);
                pixel_area[(size_t)y*W + x] = fp.size() >= 3
                    ? spherical::spherical_polygon_area(fp) : 0.0;
            }
    }

    // 预扫描 inner 像素 max|v| (fill 相对阈值基准)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (!inner(x, y)) continue;
            double vv = ro64.signal[(size_t)y*W + x];
            if (std::fabs(vv) > max_v) max_v = std::fabs(vv);
        }

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            size_t i = (size_t)y*W + x;
            double t = truth[i];
            double v = ro64.signal[i];
            double cv = ro64.coverage[i];
            if (!inner(x, y)) continue;
            cr.inner_pixels++;
            if (pixfrac >= 1.0) {
                if (t > 0) { denom_s += t; err_s += std::fabs(v - t); }
                double at = std::fabs(t);
                if (at > 0 && t <= 0 && v > 1e-9 * at) ++cr.false_fill;
                if (t > 0 && v <= 1e-9 * t) ++cr.false_hole;
                double ct = t > 0 ? 1.0 : 0.0;
                denom_c += ct; err_c += std::fabs(cv - ct);
            } else {
                // 表面亮度自洽: signal ≈ B0 × A_pixel × coverage
                double sb_expected = 0.0;
                if (sb_check) {
                    double ap = pixel_area[i];
                    sb_expected = B0 * ap * cv;
                    if (cv > 0.05 && ap > 0) {
                        denom_sb += sb_expected;
                        err_sb += std::fabs(v - sb_expected);
                    }
                }
                if (cv > 1e-3 && v <= 1e-12) ++cr.false_hole;
                if (cv <= 1e-6 && v > 1e-6 * max_v)
                    ++cr.false_fill;
            }
            if (cv < 0.0 || cv > 1.0 + 1e-9) { ++cr.false_hole; }
            double scale = std::max(std::fabs(v), 1e-9);
            double r = std::fabs((double)ro32.signal_f32[i] - v) / scale;
            if (r > cr.fp32_max_rel) cr.fp32_max_rel = r;
        }
    cr.signal_rel_err = denom_s > 0 ? err_s / denom_s : 0.0;
    cr.coverage_rel_err = denom_c > 0 ? err_c / denom_c : 0.0;
    cr.sb_err = denom_sb > 0 ? err_sb / denom_sb : 0.0;
    cr.total_in = ro64.total_signal_in;
    cr.total_out = ro64.total_signal_out;
    cr.total_covered_area_out = ro64.total_covered_area_out;
    return cr;
}

int main() {
    printf("=== 反向 Drizzle 解析球面科学矩阵 ===\n");
    const int W = 96, H = 96, NSIDE = 8192;
    const double B0 = 1000.0;

    struct Scene {
        const char* name;
        WcsParams w;
        std::function<double(const Vec3&)> B;
        bool linear;      // true: B(v)=b0 + c·v (解析向量面积)
        double b0;
        Vec3 c;
        int nside;
    };
    auto B_const = [](const Vec3&) -> double { return 1000.0; };
    auto B_grad = [](const Vec3& v) -> double {
        return 1000.0 + 300.0 * (0.6*v.x + 0.8*v.z);
    };
    auto B_neg = [](const Vec3& v) -> double {
        return -500.0 + 1500.0 * (v.z > 0 ? v.z : 0.0);
    };
    // compact_source: 高斯源位于场景 CRVAL 方向 (capture)
    auto make_bump = [](double ra0, double dec0) {
        Vec3 dir = spherical::radec_to_vec<double>(ra0, dec0);
        double sigma = 0.20 * PI / 180.0;  // 0.2° = 12' ≈ 47× leaf (nside=8192),
                                           // 面积分摊 smearing ~(leaf/σ)²≈4.5e-4 < 1e-3
        return [dir, sigma](const Vec3& v) -> double {
            double d = std::acos(std::max(-1.0, std::min(1.0,
                dir.x*v.x + dir.y*v.y + dir.z*v.z)));
            return 1000.0 + 50000.0 * std::exp(-(d*d) / (2.0*sigma*sigma));
        };
    };

    WcsParams w_c = make_wcs(272.886595, -23.254083, 6.3, W);
    WcsParams w_sip = w_c;
    w_sip.sip.order = 3; w_sip.sip.ap_order = 3;
    w_sip.sip.a[3*6+0] = 5e-5;
    w_sip.sip.b[0*6+3] = -4e-5;
    w_sip.sip.ap[3*6+0] = -5e-5;
    w_sip.sip.bp[0*6+3] = 4e-5;

    Scene scenes[] = {
        {"constant_sphere", w_c, B_const, true, 1000.0, Vec3{0,0,0}, 8192},
        {"gradient_sphere", w_c, B_grad, true, 1000.0, Vec3{180.0, 0.0, 240.0}, 8192},
        {"compact_source",  make_wcs(272.886595, -10.0, 6.3, W),
                            make_bump(272.886595, -10.0), false, 0.0, Vec3{0,0,0}, 32768},
        {"negative_field",  make_wcs(272.886595, 30.0, 6.3, W),
                            B_neg, true, -500.0, Vec3{0,0,1500.0}, 8192},
        {"cross_face",      make_wcs(273.7, -41.8, 6.3, W), B_const, true, 1000.0, Vec3{0,0,0}, 8192},
        {"ra0",             make_wcs(0.0, 0.0, 6.3, W), B_const, true, 1000.0, Vec3{0,0,0}, 8192},
        {"polar",           make_wcs(0.0, 89.5, 6.3, W), B_const, true, 1000.0, Vec3{0,0,0}, 8192},
        {"rotated_cd",      make_wcs(272.886595, -23.254083, 6.3, W, 30.0),
                            B_const, true, 1000.0, Vec3{0,0,0}, 8192},
        {"sip_edge",        w_sip, B_const, true, 1000.0, Vec3{0,0,0}, 8192},
    };

    const double pixfracs[] = {0.6, 0.8, 1.0};
    const char* pf_names[] = {"pf=0.6", "pf=0.8", "pf=1.0"};
    bool all_ok = true;

    for (const Scene& sc : scenes) {
        for (int pi = 0; pi < 3; pi++) {
            double pf = pixfracs[pi];
            fprintf(stderr, "[probe] start %s %s\n", sc.name, pf_names[pi]);
            bool sb_check = (sc.c.x == 0.0 && sc.c.y == 0.0 && sc.c.z == 0.0);
            CaseResult cr = run_case(sc.w, W, H, sc.nside, pf, sc.b0, sc.B,
                                     sc.linear, sc.b0, sc.c, sb_check);
            fprintf(stderr, "[probe] done  %s %s leaves_in=%.3e\n",
                    sc.name, pf_names[pi], cr.total_in);
            bool smooth = (std::strcmp(sc.name, "constant_sphere") == 0 ||
                           std::strcmp(sc.name, "cross_face") == 0 ||
                           std::strcmp(sc.name, "ra0") == 0 ||
                           std::strcmp(sc.name, "polar") == 0 ||
                           std::strcmp(sc.name, "rotated_cd") == 0 ||
                           std::strcmp(sc.name, "sip_edge") == 0);
            double tol = smooth ? 1e-5 : 1e-3;
            bool ok;
            char msg[512];
            if (pf >= 1.0) {
                ok = cr.signal_rel_err < tol &&
                     cr.coverage_rel_err < 2e-3 &&
                     cr.false_hole == 0 && cr.false_fill == 0 &&
                     cr.fp32_max_rel < 5e-4;
                snprintf(msg, sizeof(msg),
                         "[%s %s] signal_rel=%.3e(<%.0e) cov_rel=%.3e hole=%d "
                         "fill=%d fp32rel=%.2e inner=%d",
                         sc.name, pf_names[pi], cr.signal_rel_err, tol,
                         cr.coverage_rel_err, cr.false_hole, cr.false_fill,
                         cr.fp32_max_rel, cr.inner_pixels);
            } else {
                ok = cr.sb_err < 2e-3 &&
                     cr.false_hole == 0 && cr.false_fill == 0 &&
                     cr.total_out <= cr.total_in * (1.0 + 1e-12) &&
                     cr.fp32_max_rel < 5e-4;
                snprintf(msg, sizeof(msg),
                         "[%s %s] sb_err=%.3e(<2e-3) hole=%d fill=%d "
                         "out/in=%.4f fp32rel=%.2e inner=%d",
                         sc.name, pf_names[pi], cr.sb_err, cr.false_hole,
                         cr.false_fill,
                         cr.total_in > 0 ? cr.total_out / cr.total_in : 0.0,
                         cr.fp32_max_rel, cr.inner_pixels);
            }
            CHECK(ok, msg);
            if (!ok) all_ok = false;
        }
    }

    // ---- partial_support: HISS signal 已含覆盖 (sumFlux), support 按均匀
    //      覆盖假设仅影响 coverage; 同输入 support=1 vs 0.5 → 覆盖面积减半,
    //      signal 总量不变 (与冻结 Wiki 语义一致) ----
    {
        CaseResult full = run_case(w_c, W, H, NSIDE, 1.0, B0, B_const,
                                   true, 1000.0, Vec3{0,0,0}, true);
        std::vector<uint64_t> ipix; std::vector<double> sig, sup;
        make_leaves(w_c, W, H, NSIDE, 1.0, B_const, true, 1000.0, Vec3{0,0,0},
                    ipix, sig, sup);
        std::vector<double> sup_half(sup.size(), 1.0);
        for (size_t i = 0; i < sup_half.size(); i++) sup_half[i] = 0.5;
        CaseResult half = run_case(w_c, W, H, NSIDE, 1.0, B0, B_const,
                                   true, 1000.0, Vec3{0,0,0}, true, &sup_half);
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[partial_support] in_ratio_err=%.3e (signal 不含 support, 应=1) "
                 "cov_ratio=%.4f (应≈0.5)",
                 std::fabs(half.total_in / full.total_in - 1.0),
                 full.total_covered_area_out > 0
                     ? half.total_covered_area_out / full.total_covered_area_out : 0.0);
        CHECK(std::fabs(half.total_in / full.total_in - 1.0) < 1e-6 &&
              std::fabs((half.total_covered_area_out /
                         full.total_covered_area_out) - 0.5) < 1e-3,
              msg);
    }

    // ---- REV-104 回归: FP32 输入 → FP32 输出非全零 ----
    {
        std::vector<uint64_t> ipix; std::vector<double> sig, sup;
        make_leaves(w_c, W, H, NSIDE, 0.8, B_const, true, 1000.0, Vec3{0,0,0},
                    ipix, sig, sup);
        ReverseDrizzleInput rin;
        rin.nside = NSIDE; rin.nested = true;
        rin.leaf_ipix = ipix;
        rin.leaf_signal_f32.assign(sig.begin(), sig.end());
        rin.leaf_support = sup;
        rin.wcs = w_c; rin.target_width = W; rin.target_height = H;
        rin.pixfrac = 0.8; rin.output_fp64 = false;
        ReverseDrizzle rdz; ReverseDrizzleOutput ro; std::string e2;
        bool ok = rdz.run(rin, ro, e2);
        double sum32 = 0;
        for (float v : ro.signal_f32) sum32 += v;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[fp32_to_fp32 真实路径] run=%d Σsignal_f32=%.6g touched=%lld",
                 ok ? 1 : 0, sum32, (long long)ro.n_target_pixel_touched);
        CHECK(ok && sum32 > 0 && ro.n_target_pixel_touched > 0, msg);
    }

    // ---- REV-105: 输入严格校验 (6 类非法输入必须拒绝) ----
    {
        ReverseDrizzleInput rin;
        rin.nside = NSIDE; rin.nested = true;
        rin.leaf_ipix = {0}; rin.leaf_signal = {1.0};
        rin.wcs = w_c; rin.target_width = W; rin.target_height = H;
        rin.pixfrac = 1.0;
        ReverseDrizzle rdz; ReverseDrizzleOutput ro; std::string e3;
        int fails = 0;
        ReverseDrizzleInput a = rin; a.pixfrac = 0.0;
        if (!rdz.run(a, ro, e3)) fails++;
        ReverseDrizzleInput b = rin; b.leaf_signal_f32 = {1.0f};
        if (!rdz.run(b, ro, e3)) fails++;
        ReverseDrizzleInput c = rin; c.leaf_support = {1.5};
        if (!rdz.run(c, ro, e3)) fails++;
        ReverseDrizzleInput d = rin; d.leaf_ipix = {12LL*NSIDE*NSIDE};
        if (!rdz.run(d, ro, e3)) fails++;
        ReverseDrizzleInput f = rin; f.leaf_ipix = {1, 1};
        f.leaf_signal = {1.0, 2.0};
        if (!rdz.run(f, ro, e3)) fails++;
        ReverseDrizzleInput g = rin; g.wcs.has_wcs = false;
        if (!rdz.run(g, ro, e3)) fails++;
        char msg[256];
        snprintf(msg, sizeof(msg), "[输入严格校验] 6/6 非法输入被拒绝 (%d/6)", fails);
        CHECK(fails == 6, msg);
    }

    printf("== 反向 Drizzle 科学矩阵: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    (void)all_ok;
    return g_fail == 0 ? 0 : 1;
}
