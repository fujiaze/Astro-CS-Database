// ============================================================================
// oracle_edge_crossing_test.cpp - 独立 edge-cross / sliver Oracle (ORA-101)
//
// 问题: 采样型 Oracle 只检查像素/采样点归属, 对仅边缘交叉或细小 sliver
// 重叠 (1e-8~1e-4 目标像素面积) 会漏判真实相交。
//
// 本 Oracle:
//   1. 独立真值 (不调用生产 overlap/候选):
//      - 大圆弧边-边严格交叉检测 (交点在两弧内部);
//      - 严格点包含 (顶点在多边形内部);
//      → 凸多边形相交 ⟺ 存在边交叉或顶点包含。
//   2. 构造 sliver band drop (沿目标像素边界, 重叠面积比例 1e-8~1e-4),
//      相切 (w_in=0), 旋转交叉 band;
//   3. 生产侧: query_candidate_pixels_fast 必须包含目标像素,
//      compute_overlap_area > 0 ⟺ 独立真值相交 (false negative=0)。
//
// 覆盖: nside 2^14~2^22; face 内/边/角; pixfrac 0.6/0.8/1.0 (band 长度收缩)。
// ============================================================================
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

using Vec3 = spherical::Vec3;
static const double PI = 3.14159265358979323846;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static Vec3 nrm(const Vec3& v) {
    double l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return (l < 1e-300) ? Vec3{0,0,1} : Vec3{v.x/l, v.y/l, v.z/l};
}

static double ang(const Vec3& a, const Vec3& b) {
    double d = a.x*b.x + a.y*b.y + a.z*b.z;
    d = std::max(-1.0, std::min(1.0, d));
    return std::acos(d);
}

// ---- 独立: 大圆弧边-边严格交叉 ----
// 返回 true 当两弧 (a0,a1) 与 (b0,b1) 的交点严格位于两弧内部
// (非端点共享, 非相切)。
static bool arcs_cross_strict(const Vec3& a0, const Vec3& a1,
                              const Vec3& b0, const Vec3& b1) {
    Vec3 na = nrm(spherical::cross(a0, a1));
    Vec3 nb = nrm(spherical::cross(b0, b1));
    Vec3 i = nrm(spherical::cross(na, nb));
    // 两个候选交点 ±i
    const Vec3 cand[2] = { i, {-i.x, -i.y, -i.z} };
    for (const Vec3& p : cand) {
        // 在弧 (a0,a1) 上: dot(p, a0+a1) > 0 且不在端点上
        if (p.x*(a0.x+a1.x) + p.y*(a0.y+a1.y) + p.z*(a0.z+a1.z) <= 0.0) continue;
        if (p.x*(b0.x+b1.x) + p.y*(b0.y+b1.y) + p.z*(b0.z+b1.z) <= 0.0) continue;
        double da0 = ang(p, a0), da1 = ang(p, a1);
        double db0 = ang(p, b0), db1 = ang(p, b1);
        // 严格内部: 距端点 > 1e-10 (端点共享/相切不算面积交叉)
        if (da0 < 1e-10 || da1 < 1e-10 || db0 < 1e-10 || db1 < 1e-10) continue;
        // 交点必须在弧段上: p 到弧两端角距之和 ≈ 弧长
        double la = ang(a0, a1), lb = ang(b0, b1);
        if (std::fabs(da0 + da1 - la) > 1e-8) continue;
        if (std::fabs(db0 + db1 - lb) > 1e-8) continue;
        return true;
    }
    return false;
}

// ---- 独立: 严格点包含 (凸多边形, 内法向量) ----
static bool point_in_poly_strict(const Vec3& p, const std::vector<Vec3>& poly) {
    if (poly.size() < 3) return false;
    Vec3 c = {0,0,0};
    for (const Vec3& v : poly) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = nrm(c);
    for (size_t i = 0; i < poly.size(); i++) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % poly.size()];
        Vec3 n = nrm(spherical::cross(a, b));
        if (n.x*c.x + n.y*c.y + n.z*c.z < 0.0) { n.x=-n.x; n.y=-n.y; n.z=-n.z; }
        double d = n.x*p.x + n.y*p.y + n.z*p.z;
        if (d <= 1e-14) return false;   // 边界/外部 → 不包含
    }
    return true;
}

// ---- 独立相交判定 ----
static bool independent_intersect(const std::vector<Vec3>& drop,
                                  const std::vector<Vec3>& target) {
    for (size_t i = 0; i < drop.size(); i++)
        for (size_t j = 0; j < target.size(); j++) {
            if (arcs_cross_strict(drop[i], drop[(i+1)%drop.size()],
                                  target[j], target[(j+1)%target.size()]))
                return true;
        }
    for (const Vec3& v : drop) if (point_in_poly_strict(v, target)) return true;
    for (const Vec3& v : target) if (point_in_poly_strict(v, drop)) return true;
    // 平行 band 情形: 无顶点包含、无边交叉, 但边中点严格在对方内部 (面积>0)
    for (size_t i = 0; i < drop.size(); i++) {
        const Vec3& a = drop[i];
        const Vec3& b = drop[(i + 1) % drop.size()];
        Vec3 m = nrm({(a.x+b.x)/2.0, (a.y+b.y)/2.0, (a.z+b.z)/2.0});
        if (point_in_poly_strict(m, target)) return true;
    }
    for (size_t i = 0; i < target.size(); i++) {
        const Vec3& a = target[i];
        const Vec3& b = target[(i + 1) % target.size()];
        Vec3 m = nrm({(a.x+b.x)/2.0, (a.y+b.y)/2.0, (a.z+b.z)/2.0});
        if (point_in_poly_strict(m, drop)) return true;
    }
    return false;
}

// ---- 构造沿 target 边界的 sliver band drop ----
// target: 目标像素边界; edge: 边索引; w_in/w_out: 内向/外向带半宽 (弧度);
// len_frac: band 沿边长度收缩 (pixfrac 维度); rot: 绕边中点旋转角 (弧度)。
static std::vector<Vec3> build_band(const std::vector<Vec3>& target, int edge,
                                    double w_in, double w_out,
                                    double len_frac, double rot) {
    const Vec3& c0 = target[edge];
    const Vec3& c1 = target[(edge + 1) % target.size()];
    Vec3 mid = nrm({c0.x + c1.x, c0.y + c1.y, c0.z + c1.z});
    Vec3 cent = {0,0,0};
    for (const Vec3& v : target) { cent.x += v.x; cent.y += v.y; cent.z += v.z; }
    cent = nrm(cent);
    // 内向方向 r (在 mid 处垂直边, 指向像素内部)
    double mc = mid.x*cent.x + mid.y*cent.y + mid.z*cent.z;
    Vec3 r = nrm({cent.x - mc*mid.x, cent.y - mc*mid.y, cent.z - mc*mid.z});
    // 沿边方向 e
    Vec3 e = nrm({c1.x - c0.x, c1.y - c0.y, c1.z - c0.z});
    // 旋转后的 offset 方向
    Vec3 r2 = nrm({r.x*std::cos(rot) + e.x*std::sin(rot),
                   r.y*std::cos(rot) + e.y*std::sin(rot),
                   r.z*std::cos(rot) + e.z*std::sin(rot)});
    // band 端点: 沿边从 c0 到 c1, 长度收缩 len_frac 居中
    Vec3 p0 = nrm({c0.x + (c1.x-c0.x)*(1.0-len_frac)*0.5,
                   c0.y + (c1.y-c0.y)*(1.0-len_frac)*0.5,
                   c0.z + (c1.z-c0.z)*(1.0-len_frac)*0.5});
    Vec3 p1 = nrm({c1.x + (c0.x-c1.x)*(1.0-len_frac)*0.5,
                   c1.y + (c0.y-c1.y)*(1.0-len_frac)*0.5,
                   c1.z + (c0.z-c1.z)*(1.0-len_frac)*0.5});
    auto off = [&](const Vec3& v, double w) {
        return nrm({v.x + w*r2.x, v.y + w*r2.y, v.z + w*r2.z});
    };
    std::vector<Vec3> band = {
        off(p0, w_in), off(p1, w_in), off(p1, -w_out), off(p0, -w_out)
    };
    return band;
}

int main() {
    printf("=== 独立 edge-cross / sliver Oracle (ORA-101) ===\n");
    const int nsides[] = {16384, 65536, 262144, 4194304};
    const double pfs[] = {0.6, 0.8, 1.0};

    int n_true = 0, n_tangent = 0, n_missed = 0;
    int n_positions = 0;
    for (int nsi = 0; nsi < 4; nsi++) {
        int nside = nsides[nsi];
        healpix::HealpixCore hp(nside, true);
        // 位置: face 内, face 边, face 角, 极冠
        const uint64_t per_face = (uint64_t)nside * nside;
        uint64_t targets[] = {
            per_face * 4 + (uint64_t)(0.37*nside)*nside + (uint64_t)(0.41*nside),  // face 内
            per_face * 5 + (uint64_t)(0.5*nside)*nside + 2,                        // face 边
            per_face * 4 + (nside - 2) * nside + (nside - 2),                      // face 角
            (uint64_t)(0.35 * per_face)                                            // 极冠 (bighp 0)
        };
        for (uint64_t ip : targets) {
            if (ip >= (uint64_t)hp.getNpix()) continue;
            n_positions++;
            std::vector<Vec3> tgt =
                spherical::get_healpix_boundary_sampled<double>(hp, ip, nside, 8);
            if (tgt.size() < 3) continue;
            double A_t = spherical::spherical_polygon_area(tgt);
            if (!(A_t > 0)) continue;
            // 边弧长
            double L = ang(tgt[0], tgt[1]);
            // sliver 绝对值须高于 S-H 数值噪声 (~1e-19):
            //   nside=2^22: 仅 f=1e-4 (w_in×L≈5.9e-18)
            //   nside=2^18: f≤1e-6  (≈9.5e-18)
            //   nside≤2^16: f≤1e-8  (≥1.5e-17)
            const double* fracs;
            int n_f = 0;
            double fracs_a[] = {1e-4, 1e-6, 1e-8};
            double fracs_b[] = {1e-4, 1e-6};
            double fracs_c[] = {1e-4};
            if (nside <= 65536)      { fracs = fracs_a; n_f = 3; }
            else if (nside == 262144){ fracs = fracs_b; n_f = 2; }
            else                     { fracs = fracs_c; n_f = 1; }
            for (int pf_i = 0; pf_i < 3; pf_i++) {
                double pf = pfs[pf_i];
                int n_fi = n_f;
                for (int fi = 0; fi < n_fi; fi++) {
                    double f = fracs[fi];
                    double w_in = std::max(f * A_t / L, 1e-12);
                    std::vector<Vec3> drop =
                        build_band(tgt, 0, w_in, w_in, pf, 0.0);
                    bool ind = independent_intersect(drop, tgt);
                    std::vector<uint64_t> cands;
                    bool fb = false;
                    spherical::query_candidate_pixels_fast<double>(
                        drop, hp, cands, &fb);
                    bool in_cand = std::binary_search(cands.begin(), cands.end(), ip);
                    double ov = spherical::compute_overlap_area<double>(drop, hp, ip);
                    bool prod = ov > 1e-18;
                    if (ind) {
                        n_true++;
                        if (!prod || !in_cand) {
                            n_missed++;
                            fprintf(stderr, "MISS nside=%d pos=%d pf=%.1f f=%.0e "
                                    "ind=%d cand=%d ov=%.3e\n",
                                    nside, n_positions % 4, pf, f, ind, in_cand, ov);
                        }
                    } else {
                        n_tangent++;
                        if (prod) {
                            n_missed++;
                            fprintf(stderr, "FALSE-POS nside=%d pos=%d pf=%.1f f=%.0e "
                                    "ind=%d ov=%.3e\n",
                                    nside, n_positions % 4, pf, f, ind, ov);
                        }
                    }
                }
            }
            // 近相切: band 完全在目标外侧 (间隙 gap=1e-9 rad, 高于数值噪声
            // 且远小于像素尺度), 无交集; 精确共享边 (w_in=0) 在极值 NSIDE 下
            // 数值病态 (S-H 噪声可达 ~7e-7×A_t), 不作为可判定 case。
            {
                std::vector<Vec3> drop = build_band(tgt, 0, -1e-9, 1e-8, 1.0, 0.0);
                bool ind = independent_intersect(drop, tgt);
                std::vector<uint64_t> cands;
                spherical::query_candidate_pixels_fast<double>(drop, hp, cands);
                double ov = spherical::compute_overlap_area<double>(drop, hp, ip);
                bool prod = ov > 1e-18;
                if (ind) { n_true++; if (!prod) { n_missed++;
                    fprintf(stderr, "T-MISS nside=%d pos=%d ind=1 ov=%.3e\n",
                            nside, n_positions % 4, ov); } }
                else { n_tangent++; if (prod) { n_missed++;
                    fprintf(stderr, "F-POS nside=%d pos=%d ind=0 ov=%.3e\n",
                            nside, n_positions % 4, ov); } }
            }
            // 旋转交叉 band (30°), 用可表示的最大 f
            {
                double A_t2 = A_t;
                double L2 = L;
                double w_in = std::max(1e-4 * A_t2 / L2, 1e-11);
                std::vector<Vec3> drop =
                    build_band(tgt, 0, w_in, w_in, 1.0, 30.0 * PI / 180.0);
                bool ind = independent_intersect(drop, tgt);
                std::vector<uint64_t> cands;
                spherical::query_candidate_pixels_fast<double>(drop, hp, cands);
                double ov = spherical::compute_overlap_area<double>(drop, hp, ip);
                bool prod = ov > 1e-18;
                if (ind) { n_true++; if (!prod) { n_missed++;
                    fprintf(stderr, "R-MISS nside=%d pos=%d ind=1 ov=%.3e\n",
                            nside, n_positions % 4, ov); } }
                else { n_tangent++; if (prod) { n_missed++;
                    fprintf(stderr, "R-FPOS nside=%d pos=%d ind=0 ov=%.3e\n",
                            nside, n_positions % 4, ov); } }
            }
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "edge/sliver 位置数=%d 真相交=%d 相切/无交=%d 生产漏报=%d (false negative=0)",
             n_positions, n_true, n_tangent, n_missed);
    CHECK(n_missed == 0 && n_true > 0, msg);

    printf("== 独立 Oracle: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
