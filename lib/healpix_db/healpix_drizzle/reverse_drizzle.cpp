// ============================================================================
// reverse_drizzle.cpp - Sphere -> Plane 球面面积 Drizzle (签字修正)
//
// 冻结语义 (wiki/Reverse_Drizzle.md + control/REVERSE_DRIZZLE_IMPLEMENTATION.md):
//   - HEALPix leaf 球面边界 (自适应细分);
//   - pixfrac 球面 slerp 收缩;
//   - target pixel 球面 footprint (WCS/SIP 自适应边细分);
//   - 球面 overlap (drop ∩ target 球面面积, fan triangulation + S-H);
//   - signal 按球面面积比例分配 (禁止 2D 投影面积权重);
//   - support 以均匀覆盖假设影响 coverage;
//   - FP32→FP32 真实 float 累计; FP64→FP64 double 累计;
//   - 输入严格校验 (REV-105), 统计字段全部填充 (REV-106)。
// ============================================================================
#include "reverse_drizzle.h"
#include "wcs_sip.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace drizzle {

namespace {

using Vec3 = spherical::Vec3;

// ---- WCS pixelToSky 回调 (build_drop_polygon_adaptive 使用) ----
struct WcsCallbackCtx {
    const WcsSip* wcs;
};

static bool wcs_pixel_to_sky(double px, double py, double& ra, double& dec,
                             void* user_data) {
    auto* ctx = static_cast<WcsCallbackCtx*>(user_data);
    ctx->wcs->pixelToSky(px, py, ra, dec);
    return std::isfinite(ra) && std::isfinite(dec);
}

// ---- 球面 slerp (从 a 到 b, t∈[0,1]; 小角度退化为线性插值归一化) ----
static Vec3 slerp(const Vec3& a, const Vec3& b, double t) {
    double d = a.x * b.x + a.y * b.y + a.z * b.z;
    d = std::max(-1.0, std::min(1.0, d));
    double theta = std::acos(d);
    if (theta < 1e-12) {
        // 退化: 线性插值 + 归一化
        Vec3 v = { a.x + t * (b.x - a.x),
                   a.y + t * (b.y - a.y),
                   a.z + t * (b.z - a.z) };
        return spherical::normalize(v);
    }
    double s0 = std::sin((1.0 - t) * theta) / std::sin(theta);
    double s1 = std::sin(t * theta) / std::sin(theta);
    Vec3 v = { a.x * s0 + b.x * s1,
               a.y * s0 + b.y * s1,
               a.z * s0 + b.z * s1 };
    return spherical::normalize(v);
}

// ---- 球面多边形-多边形重叠面积 (目标多边形扇形剖分 + drop 球面 S-H 裁剪) ----
// target 为平面像素球面 footprint (凸, 由 build_drop_polygon_adaptive 生成);
// 扇形剖分使每个三角形凸, S-H 裁剪误差不跨三角形累积 (与 compute_overlap_area 同模式)。
static double overlap_drop_target(const std::vector<Vec3>& drop,
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
        // 三角形质心 (内部测试点; 扇心 c 是退化点, dot(cross(p0,p1), c)=0 无法判定)
        Vec3 m = { (c.x + a.x + b.x) / 3.0,
                   (c.y + a.y + b.y) / 3.0,
                   (c.z + a.z + b.z) / 3.0 };
        m = spherical::normalize(m);
        std::vector<Vec3> normals;
        normals.reserve(3);
        for (int e = 0; e < 3; e++) {
            const Vec3& p0 = tri[e];
            const Vec3& p1 = tri[(e + 1) % 3];
            Vec3 n = spherical::normalize(spherical::cross(p0, p1));
            // 内法向量: 指向三角形内部 (质心 m 与 n 同侧)
            if (spherical::dot(n, m) < 0.0) n = {-n.x, -n.y, -n.z};
            normals.push_back(n);
        }
        std::vector<Vec3> clipped =
            spherical::sutherland_hodgman_spherical(drop, normals);
        total += spherical::spherical_polygon_area(clipped);
    }
    return total;
}

// ---- 估算目标像素球面 footprint 在平面上的最大投影半径 (候选 bbox 裕量) ----
static double estimate_footprint_radius_px(const WcsSip& wcs, int W, int H,
                                           double src_scale_rad) {
    WcsCallbackCtx ctx{&wcs};
    const int stride = std::max(1, std::min(W, H) / 32);
    double max_r = 0.0;
    auto sample = [&](int px, int py) {
        std::vector<Vec3> fp = spherical::build_drop_polygon_adaptive<double>(
            (double)px, (double)py, 1.0, wcs_pixel_to_sky, &ctx, src_scale_rad);
        if (fp.size() < 3) return;
        for (const Vec3& v : fp) {
            double ra, dec, x, y;
            spherical::vec_to_radec<double>(v, ra, dec);
            wcs.skyToPixel(ra, dec, x, y);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            double r = std::hypot(x - px, y - py);
            if (r > max_r) max_r = r;
        }
    };
    sample(0, 0); sample(W - 1, 0); sample(0, H - 1); sample(W - 1, H - 1);
    sample(W / 2, H / 2);
    for (int y = 0; y < H; y += stride)
        for (int x = 0; x < W; x += stride) sample(x, y);
    return max_r;
}

// ---- 累计器 (T=float 真实 FP32 路径; T=double FP64 路径) ----
template <typename T>
struct ReverseAccum {
    std::vector<T> signal;
    std::vector<T> coverage;
    std::vector<uint8_t> touched;
};

template <typename T>
static bool run_typed(const ReverseDrizzleInput& in, ReverseDrizzleOutput& out,
                      const WcsSip& wcs, const healpix::HealpixCore& hp,
                      double src_scale_rad, double footprint_radius_px,
                      std::string& error_msg) {
    const int W = in.target_width, H = in.target_height;
    const size_t npix = (size_t)W * H;
    ReverseAccum<T> acc;
    acc.signal.assign(npix, T(0));
    acc.coverage.assign(npix, T(0));
    acc.touched.assign(npix, 0);

    const bool use_f32 = !in.leaf_signal_f32.empty();
    const double pf = in.pixfrac;
    WcsCallbackCtx ctx{&wcs};

    for (size_t i = 0; i < in.leaf_ipix.size(); i++) {
        const uint64_t ip = in.leaf_ipix[i];
        const double sig = use_f32 ? (double)in.leaf_signal_f32[i]
                                   : in.leaf_signal[i];
        const double support = in.leaf_support.empty() ? 1.0
                                                       : in.leaf_support[i];
        if (!std::isfinite(sig)) { ++out.n_nonfinite; continue; }
        if (ip >= (uint64_t)hp.getNpix()) { ++out.n_invalid_ipix; continue; }

        // 1. leaf 球面边界 (自适应细分; 高 NSIDE 自动退化为 4 角)
        std::vector<Vec3> bnd =
            spherical::get_healpix_boundary_sampled<double>(
                hp, ip, (int)in.nside, 8);
        if (bnd.size() < 3) { ++out.n_invalid_ipix; continue; }

        // 2. pixfrac 球面 slerp 收缩
        double ra_c, dec_c;
        hp.pix2radec((int64_t)ip, &ra_c, &dec_c);
        Vec3 center = spherical::radec_to_vec<double>(ra_c, dec_c);
        std::vector<Vec3> drop;
        drop.reserve(bnd.size());
        for (const Vec3& v : bnd) drop.push_back(pf >= 1.0 ? v : slerp(center, v, pf));

        const double drop_area = spherical::spherical_polygon_area(drop);
        if (drop_area <= 0.0 || !std::isfinite(drop_area)) {
            ++out.n_invalid_ipix;
            continue;
        }
        ++out.n_source_leaf;
        out.total_signal_in += sig;
        out.total_covered_area_in += drop_area * support;
        if (sig == 0.0 && support <= 0.0) continue;

        // 3. 投影 bbox + 裕量 (候选可使用 skyToPixel bbox, 面积分配仍为球面)
        double minx = 1e300, maxx = -1e300, miny = 1e300, maxy = -1e300;
        bool any_proj = false;
        for (const Vec3& v : drop) {
            double ra, dec, x, y;
            spherical::vec_to_radec<double>(v, ra, dec);
            wcs.skyToPixel(ra, dec, x, y);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            any_proj = true;
            minx = std::min(minx, x); maxx = std::max(maxx, x);
            miny = std::min(miny, y); maxy = std::max(maxy, y);
        }
        if (!any_proj) { ++out.n_skipped_outside; continue; }
        const double m = footprint_radius_px + 2.0;
        int x0 = std::max(0, (int)std::floor(minx - m));
        int x1 = std::min(W - 1, (int)std::ceil(maxx + m));
        int y0 = std::max(0, (int)std::floor(miny - m));
        int y1 = std::min(H - 1, (int)std::ceil(maxy + m));
        if (x0 > x1 || y0 > y1) { ++out.n_skipped_outside; continue; }

        // 4. 逐候选像素: target 球面 footprint + 球面 overlap + 面积分配
        for (int py = y0; py <= y1; py++) {
            for (int px = x0; px <= x1; px++) {
                ++out.n_candidates;
                std::vector<Vec3> fp = spherical::build_drop_polygon_adaptive<double>(
                    (double)px, (double)py, 1.0, wcs_pixel_to_sky, &ctx,
                    src_scale_rad);
                if (fp.size() < 3) continue;
                const double ov = overlap_drop_target(drop, fp);
                if (ov <= 1e-20 || !std::isfinite(ov)) continue;
                const size_t idx = (size_t)py * W + px;
                const double pixel_area = spherical::spherical_polygon_area(fp);
                if (pixel_area <= 0.0 || !std::isfinite(pixel_area)) continue;
                const double w = ov / drop_area;
                acc.signal[idx] += T(sig * w);
                acc.coverage[idx] += T(support * ov / pixel_area);
                acc.touched[idx] = 1;
                ++out.n_overlaps;
                out.total_signal_out += sig * w;
                out.total_covered_area_out += support * ov;
            }
        }
    }

    // 5. 输出
    for (size_t i = 0; i < npix; i++) {
        double s = (double)acc.signal[i];
        double c = (double)acc.coverage[i];
        if (c > 1.0) c = 1.0;
        if (!in.no_data_as_zero && acc.touched[i] == 0) {
            s = std::nan("");
            c = 0.0;
        }
        out.signal[i] = s;
        out.coverage[i] = c;
        out.signal_f32[i] = (float)s;
        out.coverage_f32[i] = (float)c;
        if (acc.touched[i]) ++out.n_target_pixel_touched;
    }
    return true;
}

} // namespace

// ============================================================================
// ReverseDrizzle::run — 输入校验 + 调度
// ============================================================================
bool ReverseDrizzle::run(const ReverseDrizzleInput& in,
                         ReverseDrizzleOutput& out, std::string& error_msg) {
    error_msg.clear();
    out = ReverseDrizzleOutput();

    // ---- REV-105 严格输入校验 (不静默裁剪) ----
    const uint32_t ns = in.nside;
    if (ns == 0 || (ns & (ns - 1)) != 0 || ns > (1u << 22)) {
        error_msg = "ReverseDrizzle: nside 必须是 2 的幂且在 [1, 2^22]";
        return false;
    }
    if (!in.nested) {
        error_msg = "ReverseDrizzle: 仅支持 NESTED ordering";
        return false;
    }
    if (in.target_width <= 0 || in.target_height <= 0 ||
        (size_t)in.target_width * in.target_height > (size_t)1 << 26) {
        error_msg = "ReverseDrizzle: 输出尺寸非法";
        return false;
    }
    if (!(in.pixfrac > 0.0 && in.pixfrac <= 1.0)) {
        error_msg = "ReverseDrizzle: pixfrac 必须在 (0, 1]";
        return false;
    }
    if (!in.wcs.has_wcs) {
        error_msg = "ReverseDrizzle: WCS 无效 (has_wcs=false)";
        return false;
    }
    const double det = in.wcs.cd[0] * in.wcs.cd[3] - in.wcs.cd[1] * in.wcs.cd[2];
    if (!std::isfinite(det) || std::fabs(det) < 1e-30 ||
        !std::isfinite(in.wcs.crval[0]) || !std::isfinite(in.wcs.crval[1]) ||
        !std::isfinite(in.wcs.crpix[0]) || !std::isfinite(in.wcs.crpix[1])) {
        error_msg = "ReverseDrizzle: WCS/CD 矩阵无效";
        return false;
    }
    // 物理范围检查 (防病态有限值: 如 crval=1e300 会使 TAN 投影数值爆炸,
    //   目标 footprint 自适应细分永不收敛 → 卡死)
    if (in.wcs.crval[1] < -90.0 || in.wcs.crval[1] > 90.0) {
        error_msg = "ReverseDrizzle: crval DEC 超出 [-90, 90]";
        return false;
    }
    for (int k = 0; k < 4; k++) {
        if (!std::isfinite(in.wcs.cd[k]) || std::fabs(in.wcs.cd[k]) > 1.0) {
            error_msg = "ReverseDrizzle: CD 矩阵元素超出物理范围 (±1 deg/px)";
            return false;
        }
    }
    const size_t n = in.leaf_ipix.size();
    const bool has_f64 = !in.leaf_signal.empty();
    const bool has_f32 = !in.leaf_signal_f32.empty();
    if (has_f64 == has_f32) {
        error_msg = "ReverseDrizzle: 必须且只能提供一种 signal (f32 或 f64)";
        return false;
    }
    if (has_f64 && in.leaf_signal.size() != n) {
        error_msg = "ReverseDrizzle: leaf_signal 长度与 leaf_ipix 不一致";
        return false;
    }
    if (has_f32 && in.leaf_signal_f32.size() != n) {
        error_msg = "ReverseDrizzle: leaf_signal_f32 长度与 leaf_ipix 不一致";
        return false;
    }
    if (!in.leaf_support.empty()) {
        if (in.leaf_support.size() != n) {
            error_msg = "ReverseDrizzle: leaf_support 长度与 leaf_ipix 不一致";
            return false;
        }
        for (double s : in.leaf_support) {
            if (!std::isfinite(s) || s < 0.0 || s > 1.0 + 1e-9) {
                error_msg = "ReverseDrizzle: support 必须在 [0,1]";
                return false;
            }
        }
    }
    const int64_t npix_total = 12LL * (int64_t)ns * (int64_t)ns;
    std::vector<uint64_t> seen;
    seen.reserve(n);
    if (n > 0) {
        seen = in.leaf_ipix;
        std::sort(seen.begin(), seen.end());
        for (size_t i = 1; i < seen.size(); i++) {
            if (seen[i] == seen[i - 1]) {
                error_msg = "ReverseDrizzle: leaf_ipix 重复 (未定义语义, 拒绝)";
                return false;
            }
        }
        if (seen.back() >= (uint64_t)npix_total) {
            error_msg = "ReverseDrizzle: leaf_ipix 越界 (>= 12*nside^2)";
            return false;
        }
    }

    const size_t npix = (size_t)in.target_width * in.target_height;
    out.signal.assign(npix, 0.0);
    out.coverage.assign(npix, 0.0);
    out.signal_f32.assign(npix, 0.0f);
    out.coverage_f32.assign(npix, 0.0f);
    if (n == 0) return true;

    WcsSip wcs(in.wcs);
    healpix::HealpixCore hp((int)ns, true);
    // 目标像素角尺度 (弧度) — 用于 WCS 自适应细分阈值
    const double s1 = std::sqrt(in.wcs.cd[0] * in.wcs.cd[0] +
                                in.wcs.cd[2] * in.wcs.cd[2]);
    const double s2 = std::sqrt(in.wcs.cd[1] * in.wcs.cd[1] +
                                in.wcs.cd[3] * in.wcs.cd[3]);
    const double src_scale_rad = 0.5 * (s1 + s2) * (3.14159265358979323846 / 180.0);
    const double fp_radius = estimate_footprint_radius_px(
        wcs, in.target_width, in.target_height, src_scale_rad);

    // 真实数据面路径:
    //   fp32 输入 → fp32 输出: float 累计 (真实 FP32, 非 double 伪装)
    //   其余组合: double 累计 (输出按 dtype 转换)
    const bool true_fp32 = has_f32 && !in.output_fp64;
    bool ok;
    if (true_fp32) {
        ok = run_typed<float>(in, out, wcs, hp, src_scale_rad, fp_radius, error_msg);
    } else {
        ok = run_typed<double>(in, out, wcs, hp, src_scale_rad, fp_radius, error_msg);
    }
    return ok;
}

} // namespace drizzle
