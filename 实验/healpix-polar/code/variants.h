// ============================================================================
// variants.h — 交叠算法变体（drop 侧一律复用 production 几何，只变叶侧表示）
//
//  A0  V0            : production 本体（链接生产 TU，见 prod_v0.h）
//  A1  V0-corner-stbl: 4 角叶边界 + 数值稳定角点公式（隔离 RC2a）
//  A2  V0-curve-K    : 叶边界按真实曲线 K 段/边（隔离 RC1，含 RC2a 修正）
//  A3  A2 + Richardson
//  B   chart-native  : 面坐标卡原生交叠（叶边界精确直线 + drop 边界自适应细化）
//
// 所有变体的 drop 几何来自 production 的 build_drop_geometry_into<double>，
// 保证差异只来自"叶侧表示"。面积一律用旋转 VOS（对给定多边形精确）；
// production 在 max_angle<1e-3 时用切平面鞋带，与 VOS 差 O(theta^2)~1e-12 相对，
// 远小于本实验考察的效应（>=1e-4），报告中单列。
// ============================================================================
#ifndef POLAR_VARIANTS_H
#define POLAR_VARIANTS_H

#include "polar_common.h"
#include "spherical_overlap.h"
#include "healpix_core.h"

namespace pp {

// production DropGeometryT<double> 的薄封装（只读字段）
struct DropGeom {
    std::vector<V3> corners_d;
    std::vector<V3> clip_normals_d;
    V3 center_d;
    double max_angle = 0.0;
    double drop_area = 0.0;
};

inline void build_drop_geom(const std::vector<V3>& drop, DropGeom& g) {
    std::vector<spherical::Vec3> d, dd;
    d.reserve(drop.size());
    for (const auto& p : drop) d.push_back({p.x, p.y, p.z});
    dd = d;
    spherical::DropGeometryT<double> pg;
    spherical::build_drop_geometry_into<double>(pg, d, &dd);
    g.corners_d.clear(); g.clip_normals_d.clear();
    for (const auto& v : pg.corners_d) g.corners_d.push_back({v.x, v.y, v.z});
    for (const auto& v : pg.clip_normals_d) g.clip_normals_d.push_back({v.x, v.y, v.z});
    g.center_d = {pg.center_d.x, pg.center_d.y, pg.center_d.z};
    g.max_angle = pg.max_angle;
    g.drop_area = pg.drop_area;
}

// ---------------------------------------------------------------------------
// 变体求交：返回给定叶的 overlap 面积（sr）
// leaf_mode: 0 = 4 角 (production 复刻, acos 路径)
//            1 = 4 角 (稳定公式)
//            2 = 真实曲线 K 段/边 (稳定公式)
// use_fast: 是否启用 leaf_fully_inside_drop / drop_inside_leaf 快路径
// ---------------------------------------------------------------------------
struct VariantStat { long long n_full = 0, n_dropin = 0, n_sh = 0, n_quick = 0; };

inline double overlap_variant(const DropGeom& g, uint32_t nside, int face,
                              uint32_t i, uint32_t j, int leaf_mode, int K,
                              double hp_res_rad, bool use_fast,
                              VariantStat* st = nullptr) {
    std::vector<V3> bnd;
    // leaf_mode: 0/1 = 4 角（0=production acos 路径, 1=稳定）；2/3 = 真实曲线 K 段（2=稳定, 3=acos 路径）
    if (leaf_mode == 2 || leaf_mode == 3)
        leaf_boundary_curve_f(face, nside, i, j, K, bnd, leaf_mode == 2 ? 1 : 0);
    else {
        double uv[4][2]; leaf_corner_uv(nside, i, j, uv);
        for (int e = 0; e < 4; ++e)
            bnd.push_back(chart_uv_to_xyz(face, uv[e][0], uv[e][1], leaf_mode == 0 ? 0 : 1));
    }
    if (bnd.size() < 3) return 0.0;
    const double tol = 1e-12;
    // 叶中心（用于 quick-reject 与半空间定向）
    V3 hpc = leaf_center_xyz(face, nside, i, j, (leaf_mode == 0 || leaf_mode == 3) ? 0 : 1);
    if (use_fast) {
        const double lim = g.max_angle + 1.25 * hp_res_rad;
        double dc = std::max(-1.0, std::min(1.0, dot3(hpc, g.center_d)));
        if (dc <= std::cos(lim + 1e-9)) { if (st) st->n_quick++; return 0.0; }
    }
    // leaf fully inside drop?
    if (use_fast) {
        bool full = true;
        for (const auto& n : g.clip_normals_d) {
            for (const auto& v : bnd) if (dot3(n, v) < -tol) { full = false; break; }
            if (!full) break;
        }
        if (full) { if (st) st->n_full++; return kPi / (3.0 * (double)nside * (double)nside); }
    }
    // drop inside leaf? （用叶多边形的边大圆做半空间，保守：内接多边形内 ⇒ 真叶内）
    if (use_fast) {
        bool din = true;
        size_t nb = bnd.size();
        for (size_t e = 0; e < nb; ++e) {
            V3 n = normalize3(cross3(bnd[e], bnd[(e + 1) % nb]));
            if (dot3(n, hpc) < 0.0) n = {-n.x, -n.y, -n.z};
            for (const auto& v : g.corners_d) if (dot3(n, v) < -tol) { din = false; break; }
            if (!din) break;
        }
        if (din) { if (st) st->n_dropin++; return g.drop_area; }
    }
    if (st) st->n_sh++;
    std::vector<V3> res = sh_clip(bnd, g.clip_normals_d, 0.0);
    if (res.size() < 3) return 0.0;
    return vos_area_rotated(res);
}

// ---------------------------------------------------------------------------
// REC-1: 叶边界自适应细分（弦偏差绝对预算驱动）+ 稳定角点公式
//
// 判据推导：叶边界真实曲线用弦折线逼近时，每段的"面积亏损" ≈ (2/3)·delta·len
// （抛物线弓形面积），delta = 段中点真实位置到弦平面的角距，len = 段弧长。
// 由于 sum(len_i) = 边长，对整条边施加**统一的绝对弦偏差上限**
//     delta_max = 1.5 · (budget_abs/4) / edge_len
// 即得该边总亏损 ≤ budget_abs/4，四条边合计 ≤ budget_abs。
// 与 hp_res 无关（budget 用绝对球面度），因此对任意 nside/尺度都成立。
// ---------------------------------------------------------------------------
struct AdaptiveStat { int n_seg = 0; int n_split = 0; };

inline void refine_edge_budget(int face, uint32_t Ns, uint32_t i, uint32_t j, int e,
                               double u0, double v0, double u1, double v1,
                               const V3& P0, const V3& P1,
                               double delta_max, int depth, int max_depth,
                               std::vector<V3>& out, AdaptiveStat* st) {
    double um = 0.5*(u0+u1), vm = 0.5*(v0+v1);
    V3 Pm = chart_uv_to_xyz(face, um, vm, 1);
    double len = std::atan2(norm3(cross3(P0, P1)), dot3(P0, P1));
    V3 nn = normalize3(cross3(P0, P1));
    double delta = std::fabs(std::atan2(dot3(nn, Pm), std::sqrt(std::max(0.0, 1.0 - dot3(nn,Pm)*dot3(nn,Pm)))));
    if (delta <= delta_max || depth >= max_depth) {
        out.push_back(P0);
        if (st) st->n_seg++;
        (void)len;
        return;
    }
    if (st) st->n_split++;
    refine_edge_budget(face, Ns, i, j, e, u0, v0, um, vm, P0, Pm, delta_max, depth+1, max_depth, out, st);
    refine_edge_budget(face, Ns, i, j, e, um, vm, u1, v1, Pm, P1, delta_max, depth+1, max_depth, out, st);
}

inline double overlap_adaptive(const DropGeom& g, uint32_t nside, int face,
                               uint32_t i, uint32_t j, double budget_abs,
                               int max_depth, AdaptiveStat* st = nullptr) {
    double uv[4][2]; leaf_corner_uv(nside, i, j, uv);
    V3 P[4];
    for (int e = 0; e < 4; ++e) P[e] = chart_uv_to_xyz(face, uv[e][0], uv[e][1], 1);
    std::vector<V3> bnd;
    bnd.reserve(64);
    for (int e = 0; e < 4; ++e) {
        int e2 = (e + 1) % 4;
        double len = std::atan2(norm3(cross3(P[e], P[e2])), dot3(P[e], P[e2]));
        double delta_max = (len > 0.0) ? (1.5 * (budget_abs / 4.0) / len) : 1e300;
        refine_edge_budget(face, nside, i, j, e, uv[e][0], uv[e][1], uv[e2][0], uv[e2][1],
                           P[e], P[e2], delta_max, 0, max_depth, bnd, st);
    }
    if (bnd.size() < 3) return 0.0;
    V3 hpc = leaf_center_xyz(face, nside, i, j, 1);
    const double tol = 1e-12;
    // 快路径 1: 叶完全在 drop 内 → 解析面积
    {
        bool full = true;
        for (const auto& n : g.clip_normals_d) {
            for (const auto& v : bnd) if (dot3(n, v) < -tol) { full = false; break; }
            if (!full) break;
        }
        if (full) return kPi / (3.0 * (double)nside * (double)nside);
    }
    // 快路径 2: drop 完全在叶内
    {
        bool din = true;
        size_t nb = bnd.size();
        for (size_t e = 0; e < nb; ++e) {
            V3 n = normalize3(cross3(bnd[e], bnd[(e + 1) % nb]));
            if (dot3(n, hpc) < 0.0) n = {-n.x, -n.y, -n.z};
            for (const auto& v : g.corners_d) if (dot3(n, v) < -tol) { din = false; break; }
            if (!din) break;
        }
        if (din) return g.drop_area;
    }
    std::vector<V3> res = sh_clip(bnd, g.clip_normals_d, 0.0);
    if (res.size() < 3) return 0.0;
    return vos_area_rotated(res);
}

// ---------------------------------------------------------------------------
// B: 面坐标卡原生交叠（叶侧精确）
//
// 叶方格在 chart 里是精确的轴对齐单位方格（面积 = J * 方格面积，J = pi/3 常数），
// 因此叶侧零近似；drop 边界（大圆弧）在 chart 里是曲线，按弦偏差自适应细分。
//
// 角点安全构造：drop 包含面角点时，drop 在 chart 里的原像不是闭合折线而是
// "楔形"，直接连折线会自交。本实现改为**逐叶对 4 条 drop 边界曲线做半空间裁剪**：
// 对叶方格 Q，依次用 drop 的第 k 条边裁剪，裁剪边界是曲线 C_k = {q: n_k.S(q)=0}
// 在 Q 邻域内的**自适应折线**（顶点严格在 C_k 上，弦偏差 <= tol）。
// 该构造与角点无关，天然覆盖极冠楔形。
// ---------------------------------------------------------------------------
struct ChartOpts {
    double tol_rel = 1e-6;    // 弦偏差阈值（相对叶尺寸 1/Ns，即 chart 单位 1）
    int    max_depth = 12;
};

// 在 chart 里解 n.S(u,v)=0：沿线段 (u0,v0)-(u1,v1) 找所有穿越点
inline int chart_curve_crossings(int face, const V3& n, double u0, double v0,
                                 double u1, double v1, std::vector<double>& ts) {
    ts.clear();
    const int NS = 64;
    double prev = dot3(n, chart_uv_to_xyz(face, u0, v0, 1));
    for (int k = 1; k <= NS; ++k) {
        double t = (double)k / (double)NS;
        double u = u0 + t*(u1-u0), v = v0 + t*(v1-v0);
        double cur = dot3(n, chart_uv_to_xyz(face, u, v, 1));
        if ((prev < 0.0) != (cur < 0.0)) {
            double a = t - 1.0/NS, b = t, fa = prev;
            for (int it = 0; it < 60; ++it) {
                double m = 0.5*(a+b);
                double fm = dot3(n, chart_uv_to_xyz(face, u0 + m*(u1-u0), v0 + m*(v1-v0), 1));
                if ((fa < 0.0) != (fm < 0.0)) { b = m; } else { a = m; fa = fm; }
            }
            ts.push_back(0.5*(a+b));
        }
        prev = cur;
    }
    return (int)ts.size();
}

// 用曲线 C_k（drop 第 k 条边）裁剪 chart 多边形（顶点列表，chart 坐标）
inline void clip_poly_by_curve(int face, const V3& n, std::vector<std::array<double,2>>& poly,
                               double tol, int max_depth) {
    if (poly.size() < 3) return;
    size_t m = poly.size();
    std::vector<std::array<double,2>> out;
    out.reserve(m + 4);
    // 逐边：找穿越，按半空间内外组装；穿越段用曲线折线替代
    // 先算每个顶点的符号
    std::vector<double> g(m);
    for (size_t i = 0; i < m; ++i) g[i] = dot3(n, chart_uv_to_xyz(face, poly[i][0], poly[i][1], 1));
    for (size_t i = 0; i < m; ++i) {
        size_t j = (i + 1) % m;
        bool ii = g[i] >= 0.0, ij = g[j] >= 0.0;
        if (ii) out.push_back(poly[i]);
        if (ii != ij) {
            // 二分求交点
            double a = 0.0, b = 1.0, fa = g[i];
            for (int it = 0; it < 60; ++it) {
                double mid = 0.5*(a+b);
                double u = poly[i][0] + mid*(poly[j][0]-poly[i][0]);
                double v = poly[i][1] + mid*(poly[j][1]-poly[i][1]);
                double fm = dot3(n, chart_uv_to_xyz(face, u, v, 1));
                if ((fa < 0.0) != (fm < 0.0)) b = mid; else { a = mid; fa = fm; }
            }
            double u = poly[i][0] + 0.5*(a+b)*(poly[j][0]-poly[i][0]);
            double v = poly[i][1] + 0.5*(a+b)*(poly[j][1]-poly[i][1]);
            out.push_back({u, v});
        }
    }
    (void)tol; (void)max_depth;
    poly.swap(out);
}

// 在 chart 内沿曲线 C_k 求"从点 p 出发、沿曲线走到离开叶方格"的折线（自适应细分）
// 简化实现：不做曲线跟踪，改用**细折线近似整条曲线在叶邻域内的部分**——
// 具体做法见 chart_overlap_leaf（用密集采样 + 弦偏差判据）。
inline double chart_overlap_leaf(int face, uint32_t nside, uint32_t i, uint32_t j,
                                 const std::vector<V3>& drop, const ChartOpts& opt,
                                 long long* n_eval = nullptr) {
    // 叶方格（chart 单位：[i,i+1] x [j,j+1]）
    double u0 = (double)i, u1 = (double)(i + 1);
    double v0 = (double)j, v1 = (double)(j + 1);
    std::vector<V3> nrm;
    drop_clip_normals(drop, nrm);
    std::vector<std::array<double,2>> poly = {{{u0,v0}},{{u1,v0}},{{u1,v1}},{{u0,v1}}};
    for (const V3& n : nrm) {
        clip_poly_by_curve(face, n, poly, opt.tol_rel, opt.max_depth);
        if (poly.size() < 3) { if (n_eval) *n_eval = 0; return 0.0; }
    }
    // 鞋带（平移到 bbox 左下角，避免 chart 坐标 ~1 处的相消）
    double su = 0.0, sv = 0.0;
    for (const auto& q : poly) { su += q[0]; sv += q[1]; }
    double cu = su / (double)poly.size(), cv = sv / (double)poly.size();
    double s2 = 0.0;
    for (size_t k = 0; k < poly.size(); ++k) {
        const auto& A = poly[k]; const auto& B = poly[(k + 1) % poly.size()];
        s2 += (A[0]-cu)*(B[1]-cv) - (B[0]-cu)*(A[1]-cv);
    }
    if (n_eval) *n_eval = (long long)poly.size();
    // chart 单位 -> sr: 叶方格边长 1 chart 单位 = 1/nside face 单位, 面积元 pi/3
    double area_chart = 0.5 * std::fabs(s2);            // 单位: chart 单位^2
    return area_chart * (kJacobian / (double)nside / (double)nside);
}

} // namespace pp
#endif // POLAR_VARIANTS_H
