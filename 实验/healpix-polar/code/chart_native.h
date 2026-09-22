// ============================================================================
// chart_native.h — 面坐标卡原生交叠分配（自实现基线，机制与 INNOV-04-01 的
//                  face_native.h 同源；本文件为 EXP-07-POLAR 自包含复刻）
//
// 原理: HEALPix 面坐标卡 (face,u,v) 是**精确等面积**的（|J| = pi/3 常数，见
// polar_common.h 注释与报告 §3.1 的解析证明），且叶边界在 chart 里是**精确的
// 轴对齐直线段**。因此叶侧零近似：a_jp = J * |D_chart ∩ cell_(i,j)| / nside^2。
//
// 唯一的近似在 drop 侧：drop 边界是球面大圆弧，在 chart 里是曲线，需要弦折线
// 自适应细分（refine_arc_chart，弦偏差 <= subdiv_rel * diag）。
//
// 角点退化: drop 包含 face 角点（极点/三角点）时，drop 的 chart 原像是**楔形**，
// 直接连 drop 顶点折线会自交 ⇒ 需要"角点路径"（沿 drop 边界重采样 + 沿方格边
// 回到角点）。本文件复刻该机制以便量化其精度/代价。
// ============================================================================
#ifndef POLAR_CHART_NATIVE_H
#define POLAR_CHART_NATIVE_H

#include "polar_common.h"
#include <vector>
#include <array>
#include <algorithm>

namespace pp {

// face 邻接（数值求取，同 face_native.h 口径）
inline int neighbor_across(int face, int edge) {
    double us[2], vs[2];
    if (edge == 0 || edge == 1) {
        double uu = (edge == 0) ? 0.0 : 1.0;
        us[0] = uu; vs[0] = 0.3; us[1] = uu; vs[1] = 0.7;
    } else {
        double vv = (edge == 2) ? 0.0 : 1.0;
        us[0] = 0.3; vs[0] = vv; us[1] = 0.7; vs[1] = vv;
    }
    const double eps = 1e-7;
    int nb = -1;
    for (int i = 0; i < 2; ++i) {
        double u = us[i], v = vs[i];
        if (edge == 0) u -= eps; else if (edge == 1) u += eps;
        else if (edge == 2) v -= eps; else v += eps;
        V3 p = chart_uv_to_xyz(face, u, v, 1);
        int f2 = natural_face(p);
        if (i == 0) nb = f2; else if (nb != f2) return -2;
    }
    return nb;
}

// 2D 多边形工具
struct P2 { std::vector<std::array<double,2>> p; };
// 鞋带面积。**必须先平移到 bbox 左下角**：chart 坐标 ~0.5 处的鞋带项是 ~0.25，
// 而待求面积可小到 ~1e-12（2 个叶），1e-16 的绝对相消噪声会放大成 ~1e-4 相对误差
// ——这是 chart 原生路径必须踩过的一个纯数值坑（与几何无关）。
inline double area2(const P2& q) {
    if (q.p.size() < 3) return 0.0;
    double umin = 1e300, vmin = 1e300;
    for (const auto& a : q.p) { umin = std::min(umin, a[0]); vmin = std::min(vmin, a[1]); }
    double s = 0.0;
    for (size_t i = 0; i < q.p.size(); ++i) {
        const auto& a = q.p[i]; const auto& b = q.p[(i+1)%q.p.size()];
        double ax = a[0]-umin, ay = a[1]-vmin, bx = b[0]-umin, by = b[1]-vmin;
        s += ax*by - bx*ay;
    }
    return 0.5 * std::fabs(s);
}
// 标准 Sutherland-Hodgman：对 4 条半平面逐次裁剪
inline P2 clip_half(const P2& in, int axis, double val, bool keep_ge) {
    P2 out; out.p.reserve(in.p.size() + 2);
    if (in.p.empty()) return out;
    size_t m = in.p.size();
    auto f = [&](const std::array<double,2>& q) { return keep_ge ? (q[axis] - val) : (val - q[axis]); };
    for (size_t i = 0; i < m; ++i) {
        const auto& A = in.p[(i + m - 1) % m];
        const auto& B = in.p[i];
        double fa = f(A), fb = f(B);
        bool ia = fa >= 0.0, ib = fb >= 0.0;
        if (ib) {
            if (!ia) {
                double t = fa / (fa - fb);
                out.p.push_back({A[0]+t*(B[0]-A[0]), A[1]+t*(B[1]-A[1])});
            }
            out.p.push_back(B);
        } else if (ia) {
            double t = fa / (fa - fb);
            out.p.push_back({A[0]+t*(B[0]-A[0]), A[1]+t*(B[1]-A[1])});
        }
    }
    return out;
}
inline P2 clip_box(const P2& in, double u0, double v0, double u1, double v1) {
    P2 q = clip_half(in, 0, u0, true);
    q = clip_half(q, 0, u1, false);
    q = clip_half(q, 1, v0, true);
    q = clip_half(q, 1, v1, false);
    return q;
}
inline bool point_in_poly(const P2& q, double u, double v) {
    bool in = false;
    for (size_t i = 0, j = q.p.size()-1; i < q.p.size(); j = i++) {
        if (((q.p[i][1] > v) != (q.p[j][1] > v)) &&
            (u < (q.p[j][0]-q.p[i][0])*(v-q.p[i][1])/(q.p[j][1]-q.p[i][1]) + q.p[i][0])) in = !in;
    }
    return in;
}

// chart 内把大圆弧 A->B 自适应细分（弦偏差 <= tol，chart 单位）
inline void refine_arc_chart(int face, const V3& A, const V3& B, double tol,
                             int depth, int max_depth, std::vector<V3>& out) {
    V3 M = normalize3({A.x+B.x, A.y+B.y, A.z+B.z});
    double ua, va, ub, vb, um, vm;
    bool ok = xyz_to_chart_forced(face, A, ua, va, 1) &&
              xyz_to_chart_forced(face, B, ub, vb, 1) &&
              xyz_to_chart_forced(face, M, um, vm, 1);
    double dev = ok ? std::hypot(um - 0.5*(ua+ub), vm - 0.5*(va+vb)) : 0.0;
    if (depth >= max_depth || dev <= tol) { out.push_back(M); return; }
    refine_arc_chart(face, A, M, tol, depth+1, max_depth, out);
    refine_arc_chart(face, M, B, tol, depth+1, max_depth, out);
}

// drop 是否包含某个 face 角点
inline bool drop_contains_any_face_corner(const std::vector<V3>& drop,
                                          const std::vector<V3>& corners) {
    std::vector<V3> nrm; drop_clip_normals(drop, nrm);
    for (const auto& c : corners) {
        bool in = true;
        for (const auto& n : nrm) if (dot3(n, c) < 0.0) { in = false; break; }
        if (in) return true;
    }
    return false;
}

struct ChartAlloc {
    double sum_area = 0.0;
    double chart_area = 0.0;      // 单位: face 单位^2
    int faces = 0;
    int cells_clipped = 0;
    long long n_curve_pts = 0;
    bool corner_path = false;
};

struct ChartOpts2 {
    double subdiv_rel = 1e-6;     // 弦偏差阈值（相对 chart 折线包围盒对角线）
    int max_depth = 12;
    bool adaptive = true;
    bool use_corner_path = true;
    int corner_samples = 512;
    int max_face_visits = 12;
};

// 角点路径（复刻 INNOV-04-01 机制）：沿 drop 边界按弧长重采样 N 点 → 取落在
// [0,1]^2 内的最长连续段 → 两端二分细化 → 用方格边经角点闭合 → 鞋带
inline P2 corner_region_polygon(int f, const std::vector<V3>& drop, int N, double& arc_frac) {
    arc_frac = 0.0;
    std::vector<V3> s; std::vector<std::array<double,2>> uv;
    s.reserve(N+1); uv.reserve(N+1);
    size_t ne = drop.size();
    double total = 0.0;
    for (size_t e = 0; e < ne; ++e)
        total += std::atan2(norm3(cross3(drop[e], drop[(e+1)%ne])), dot3(drop[e], drop[(e+1)%ne]));
    if (!(total > 0.0)) return P2{};
    for (size_t e = 0; e < ne; ++e) {
        const V3& A = drop[e]; const V3& B = drop[(e+1)%ne];
        double d = std::atan2(norm3(cross3(A,B)), dot3(A,B));
        int k = std::max(1, (int)std::llround(N * d / total));
        for (int i = 0; i < k; ++i) {
            double t = (double)i / (double)k;
            V3 p;
            if (d < 1e-12) p = A;
            else {
                double so = std::sin(d);
                double w1 = std::sin((1.0-t)*d)/so, w2 = std::sin(t*d)/so;
                p = normalize3({A.x*w1+B.x*w2, A.y*w1+B.y*w2, A.z*w1+B.z*w2});
            }
            double u, v; xyz_to_chart_forced(f, p, u, v, 1);
            s.push_back(p); uv.push_back({u, v});
        }
    }
    size_t n = s.size();
    if (n < 8) return P2{};
    auto inside = [](const std::array<double,2>& q) {
        return q[0] >= 0.0 && q[0] <= 1.0 && q[1] >= 0.0 && q[1] <= 1.0;
    };
    size_t best_start = 0, best_len = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!inside(uv[i])) continue;
        size_t j = i;
        while (inside(uv[(j+1)%n]) && (j+1-i) < n) ++j;
        size_t len = j - i + 1;
        if (len > best_len) { best_len = len; best_start = i; }
        i = j;
    }
    if (best_len == 0) return P2{};
    arc_frac = (double)best_len / (double)n;
    auto refine = [&](size_t i_in, size_t i_out) {
        V3 a = s[i_in], b = s[i_out];
        for (int it = 0; it < 60; ++it) {
            V3 mid = normalize3({0.5*(a.x+b.x), 0.5*(a.y+b.y), 0.5*(a.z+b.z)});
            double u, v; xyz_to_chart_forced(f, mid, u, v, 1);
            if (inside({u,v})) a = mid; else b = mid;
        }
        return a;
    };
    size_t i_first = best_start, i_last = (best_start + best_len - 1) % n;
    V3 E_in  = refine(i_first, (i_first + n - 1) % n);
    V3 E_out = refine(i_last, (i_last + 1) % n);
    std::vector<V3> nrm; drop_clip_normals(drop, nrm);
    auto in_drop = [&](const V3& p) {
        for (const auto& nn : nrm) if (dot3(nn, p) < -1e-15) return false;
        return true;
    };
    const double cs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    std::vector<std::array<double,2>> c_inside;
    for (auto& cc : cs) {
        V3 p = chart_uv_to_xyz(f, cc[0], cc[1], 1);
        if (in_drop(normalize3(p))) c_inside.push_back({cc[0], cc[1]});
    }
    if (c_inside.empty()) return P2{};
    P2 out;
    double u, v;
    xyz_to_chart_forced(f, E_in, u, v, 1); out.p.push_back({u, v});
    for (size_t k = 0; k < best_len; ++k) out.p.push_back(uv[(best_start+k)%n]);
    xyz_to_chart_forced(f, E_out, u, v, 1); out.p.push_back({u, v});
    std::array<double,2> eo{u, v}, ei;
    xyz_to_chart_forced(f, E_in, ei[0], ei[1], 1);
    auto on_edge = [](const std::array<double,2>& q, double tol) {
        if (std::fabs(q[0]) < tol) return 0;
        if (std::fabs(q[0]-1.0) < tol) return 1;
        if (std::fabs(q[1]) < tol) return 2;
        if (std::fabs(q[1]-1.0) < tol) return 3;
        return -1;
    };
    int edge_out = on_edge(eo, 1e-9), edge_in = on_edge(ei, 1e-9);
    std::array<double,2> corner = c_inside[0];
    if (edge_out >= 0 && edge_in >= 0 && edge_out != edge_in) {
        auto ec = [](int e) {
            std::array<std::array<double,2>,2> r;
            if (e == 0) r = {{{0,0},{0,1}}};
            else if (e == 1) r = {{{1,0},{1,1}}};
            else if (e == 2) r = {{{0,0},{1,0}}};
            else r = {{{0,1},{1,1}}};
            return r;
        };
        auto a = ec(edge_out), b = ec(edge_in);
        bool found = false;
        for (auto& x : a) for (auto& y : b)
            if (std::fabs(x[0]-y[0]) < 1e-12 && std::fabs(x[1]-y[1]) < 1e-12)
                for (auto& ci : c_inside)
                    if (std::fabs(ci[0]-x[0]) < 1e-12 && std::fabs(ci[1]-y[1]) < 1e-12) { corner = x; found = true; }
        if (!found) return P2{};
    }
    out.p.push_back(corner);
    return out;
}

inline void distribute_cells(ChartAlloc& res, int f, const P2& region, uint32_t nside, double cell);

// 面坐标卡原生分配主入口
inline ChartAlloc chart_allocate(const std::vector<V3>& drop, uint32_t nside,
                                 const ChartOpts2& opt,
                                 const std::vector<V3>& corners) {
    ChartAlloc res;
    if (drop.size() < 3) return res;
    V3 c{0,0,0};
    for (const auto& p : drop) { c.x += p.x; c.y += p.y; c.z += p.z; }
    c = normalize3(c);
    int f0 = natural_face(c);
    bool corner_case = opt.use_corner_path && drop_contains_any_face_corner(drop, corners);
    res.corner_path = corner_case;
    std::vector<int> stack{f0};
    std::vector<char> visited(12, 0);
    int visits = 0;
    const double cell = 1.0 / (double)nside;
    while (!stack.empty() && visits < opt.max_face_visits) {
        int f = stack.back(); stack.pop_back();
        if (visited[f]) continue;
        visited[f] = 1; ++visits;
        if (corner_case) {
            double arc_frac = 0.0;
            P2 region = corner_region_polygon(f, drop, opt.corner_samples, arc_frac);
            if (region.p.size() >= 3) {
                distribute_cells(res, f, region, nside, cell);
                for (int e = 0; e < 4; ++e) {
                    int nb = neighbor_across(f, e);
                    if (nb >= 0 && !visited[nb]) stack.push_back(nb);
                }
                continue;
            }
        }
        // 快速路径
        std::vector<std::array<double,2>> raw;
        bool finite = true;
        for (const auto& p : drop) {
            double u, v;
            if (!xyz_to_chart_forced(f, p, u, v, 1) || !std::isfinite(u) || !std::isfinite(v) ||
                std::fabs(u) > 8.0 || std::fabs(v) > 8.0) { finite = false; break; }
            raw.push_back({u, v});
        }
        if (!finite || raw.size() < 3) continue;
        double umin=1e30,umax=-1e30,vmin=1e30,vmax=-1e30;
        for (auto& q : raw) { umin=std::min(umin,q[0]);umax=std::max(umax,q[0]);
                              vmin=std::min(vmin,q[1]);vmax=std::max(vmax,q[1]); }
        double diag = std::hypot(umax-umin, vmax-vmin);
        P2 poly;
        if (opt.adaptive && diag > 0.0) {
            double tol = opt.subdiv_rel * diag;
            size_t ne = drop.size();
            std::vector<V3> tmp;
            for (size_t e = 0; e < ne; ++e) {
                const V3& A = drop[e]; const V3& B = drop[(e+1)%ne];
                double ua, va; xyz_to_chart_forced(f, A, ua, va, 1);
                poly.p.push_back({ua, va});
                tmp.clear();
                refine_arc_chart(f, A, B, tol, 0, opt.max_depth, tmp);
                for (const auto& m : tmp) {
                    double um, vm; xyz_to_chart_forced(f, m, um, vm, 1);
                    poly.p.push_back({um, vm});
                }
            }
            res.n_curve_pts += (long long)poly.p.size();
        } else {
            poly.p = raw;
        }
        if (poly.p.size() < 3) continue;
        for (const auto& q : poly.p) {
            if (q[0] < 0.0) { int nb = neighbor_across(f, 0); if (nb >= 0 && !visited[nb]) stack.push_back(nb); }
            if (q[0] > 1.0) { int nb = neighbor_across(f, 1); if (nb >= 0 && !visited[nb]) stack.push_back(nb); }
            if (q[1] < 0.0) { int nb = neighbor_across(f, 2); if (nb >= 0 && !visited[nb]) stack.push_back(nb); }
            if (q[1] > 1.0) { int nb = neighbor_across(f, 3); if (nb >= 0 && !visited[nb]) stack.push_back(nb); }
        }
        P2 clipped = clip_box(poly, 0.0, 0.0, 1.0, 1.0);
        distribute_cells(res, f, clipped, nside, cell);
    }
    // chart 坐标 u,v 以 face 为单位 ([0,1])，该坐标卡下 |dOmega/dudv| = pi/3 常数，
    // 与 nside 无关（叶方格面积 = 1/nside^2 face 单位^2 ⇒ 叶面积 = (pi/3)/nside^2）。
    res.sum_area = kJacobian * res.chart_area;
    return res;
}

inline void distribute_cells(ChartAlloc& res, int f, const P2& region, uint32_t nside, double cell) {
    if (region.p.size() < 3) return;
    double umin=1e30,umax=-1e30,vmin=1e30,vmax=-1e30;
    for (const auto& q : region.p) { umin=std::min(umin,q[0]);umax=std::max(umax,q[0]);
                                     vmin=std::min(vmin,q[1]);vmax=std::max(vmax,q[1]); }
    long long i0 = (long long)std::floor(umin * nside); if (i0 < 0) i0 = 0;
    long long i1 = (long long)std::floor(std::nextafter(umax * nside, -1e300));
    if (i1 > (long long)nside - 1) i1 = (long long)nside - 1;
    long long j0 = (long long)std::floor(vmin * nside); if (j0 < 0) j0 = 0;
    long long j1 = (long long)std::floor(std::nextafter(vmax * nside, -1e300));
    if (j1 > (long long)nside - 1) j1 = (long long)nside - 1;
    if (i1 < i0 || j1 < j0) return;
    res.faces++;
    double ca = area2(region);
    if (!(ca > 0.0)) return;
    res.chart_area += ca;
    for (long long i = i0; i <= i1; ++i) {
        for (long long j = j0; j <= j1; ++j) {
            P2 q = clip_box(region, (double)i*cell, (double)j*cell,
                            (double)(i+1)*cell, (double)(j+1)*cell);
            if (q.p.size() < 3) continue;
            double a2v = area2(q);
            if (!(a2v > 0.0)) continue;
            res.cells_clipped++;
        }
    }
}

} // namespace pp
#endif // POLAR_CHART_NATIVE_H
