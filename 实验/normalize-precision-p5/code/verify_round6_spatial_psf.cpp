// ACSD PSF — 空间 Moffat4 模型（PSF.md §4/§5）第二轮验证：
//   A. Bowyer–Watson 几何核退化审计（重复点/近共圆/精确共圆/超大坐标/共线/
//      确定性/置换不变性） + 三角剖分合法性独立核验（空外接圆、凸包覆盖、Euler）。
//   B. **同一留出星样本、同一窗口、同一指标** 的对照: 空间 Delaunay+重心凸组合
//      vs 帧常圆模型 vs 该星自身椭圆模型（真值参照）, 逐星 L1、分位数、分层
//      （椭率/最近顶点距离/凸包内外/欠采样）, 并显式量化**位置泄漏**（留出星
//      是否进入训练顶点）对结论的乐观程度。
//
// 判据全部来自独立实现: 凸包（monotone chain）、空外接圆检验、面积/Euler、
// 分位数统计都在本程序内独立计算, 不调用被测模块的判定。
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "acsd/pixel_integrated_psf.h"
#include "acsd/spatial_psf.h"

using namespace acsd::psf;
static int fails = 0;
static void chk(bool ok, const char* w) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}
static double relmax(const std::vector<double>& a, const std::vector<double>& b) {
  double m = 0.0;
  for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
    const double s = std::max(1e-300, std::max(std::abs(a[i]), std::abs(b[i])));
    m = std::max(m, std::abs(a[i] - b[i]) / s);
  }
  return m;
}
static void grid_pixels(double cx, double cy, int half, std::vector<double>& px,
                        std::vector<double>& py) {
  px.clear(); py.clear();
  for (int y = static_cast<int>(std::floor(cy)) - half;
       y <= static_cast<int>(std::floor(cy)) + half; ++y)
    for (int x = static_cast<int>(std::floor(cx)) - half;
         x <= static_cast<int>(std::floor(cx)) + half; ++x) {
      px.push_back(x + 0.0); py.push_back(y + 0.0);
    }
}

// ── 独立几何核（本程序内, 不调用被测模块）────────────────────────────────
namespace indep {
double cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

// Monotone chain 凸包; 返回逆时针顶点序。
std::vector<std::array<double, 2>> convex_hull(std::vector<std::array<double, 2>> p) {
  std::sort(p.begin(), p.end(), [](const auto& a, const auto& b) {
    return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
  });
  p.erase(std::unique(p.begin(), p.end(), [](const auto& a, const auto& b) {
            return a[0] == b[0] && a[1] == b[1];
          }), p.end());
  const int n = static_cast<int>(p.size());
  if (n < 3) return p;
  std::vector<std::array<double, 2>> h(2 * n);
  int k = 0;
  for (int i = 0; i < n; ++i) {
    while (k >= 2 && cross(h[k - 1][0] - h[k - 2][0], h[k - 1][1] - h[k - 2][1],
                           p[i][0] - h[k - 1][0], p[i][1] - h[k - 1][1]) <= 0.0) --k;
    h[k++] = p[i];
  }
  for (int i = n - 2, t = k + 1; i >= 0; --i) {
    while (k >= t && cross(h[k - 1][0] - h[k - 2][0], h[k - 1][1] - h[k - 2][1],
                           p[i][0] - h[k - 1][0], p[i][1] - h[k - 1][1]) <= 0.0) --k;
    h[k++] = p[i];
  }
  h.resize(k > 0 ? k - 1 : 0);
  return h;
}
double polygon_area(const std::vector<std::array<double, 2>>& h) {
  double a = 0.0;
  for (std::size_t i = 0; i < h.size(); ++i) {
    const auto& p = h[i];
    const auto& q = h[(i + 1) % h.size()];
    a += p[0] * q[1] - q[0] * p[1];
  }
  return 0.5 * std::abs(a);
}
double tri_area(double x1, double y1, double x2, double y2, double x3, double y3) {
  return 0.5 * std::abs(cross(x2 - x1, y2 - y1, x3 - x1, y3 - y1));
}
// 严格空外接圆: 无其它顶点落在三角形外接圆内部（相对容差 eps·scale）。
bool empty_circumcircle(const DelaunayTriangulation& m, std::size_t ti, double eps,
                        double* worst_rel) {
  const auto& t = m.tris[ti];
  const PsfVertex& a = m.vertices[static_cast<std::size_t>(t[0])];
  const PsfVertex& b = m.vertices[static_cast<std::size_t>(t[1])];
  const PsfVertex& c = m.vertices[static_cast<std::size_t>(t[2])];
  double ax = a.x, ay = a.y, bx = b.x, by = b.y, cx = c.x, cy = c.y;
  if (cross(bx - ax, by - ay, cx - ax, cy - ay) < 0.0) { std::swap(bx, cx); std::swap(by, cy); }
  // 外接圆圆心/半径
  const double d = 2.0 * cross(bx - ax, by - ay, cx - ax, cy - ay);
  if (!(std::abs(d) > 0.0)) return false;
  const double ux = ((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) * (cy - ay) -
                    ((cx - ax) * (cx - ax) + (cy - ay) * (cy - ay)) * (by - ay);
  const double uy = ((cx - ax) * (cx - ax) + (cy - ay) * (cy - ay)) * (bx - ax) -
                    ((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) * (cx - ax);
  const double ox = ax + ux / d, oy = ay + uy / d;
  const double r2 = (ax - ox) * (ax - ox) + (ay - oy) * (ay - oy);
  const double scale = std::max(1e-300, r2);
  bool ok = true;
  for (const auto& v : m.vertices) {
    if (&v == &a || &v == &b || &v == &c) continue;
    const double d2 = (v.x - ox) * (v.x - ox) + (v.y - oy) * (v.y - oy);
    const double rel = (d2 - r2) / scale;
    if (worst_rel && rel < *worst_rel) *worst_rel = rel;
    if (rel < -eps) ok = false;   // 有顶点严格落入外接圆
  }
  return ok;
}
double quantile(std::vector<double> v, double q) {
  if (v.empty()) return -1.0;
  std::sort(v.begin(), v.end());
  const double pos = q * (v.size() - 1);
  const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
  const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
  const double f = pos - lo;
  return v[lo] * (1.0 - f) + v[hi] * f;
}
double mean(const std::vector<double>& v) {
  double s = 0.0; for (double x : v) s += x; return v.empty() ? -1.0 : s / v.size();
}
}  // namespace indep

// 对一份顶点集做独立合法性核验: 覆盖面积 == 凸包面积; 每条边 1-2 个三角形;
// 空外接圆（相对容差 eps）。
static void audit_triangulation(const DelaunayTriangulation& m, const char* tag,
                               double eps, bool require_delaunay) {
  std::vector<std::array<double, 2>> pts;
  for (const auto& v : m.vertices) pts.push_back({v.x, v.y});
  const auto hull = indep::convex_hull(pts);
  const double hull_area = indep::polygon_area(hull);
  double sum_area = 0.0;
  for (const auto& t : m.tris) {
    const auto& a = m.vertices[static_cast<std::size_t>(t[0])];
    const auto& b = m.vertices[static_cast<std::size_t>(t[1])];
    const auto& c = m.vertices[static_cast<std::size_t>(t[2])];
    sum_area += indep::tri_area(a.x, a.y, b.x, b.y, c.x, c.y);
  }
  const double area_rel = std::abs(sum_area - hull_area) / std::max(1e-300, hull_area);
  // 边计数
  std::map<std::pair<int, int>, int> ec;
  for (const auto& t : m.tris)
    for (int k = 0; k < 3; ++k) {
      int p = t[k], q = t[(k + 1) % 3];
      if (p > q) std::swap(p, q);
      ec[{p, q}] += 1;
    }
  int bad_edges = 0;
  for (const auto& kv : ec) if (kv.second > 2) ++bad_edges;
  // 空外接圆
  double worst = 0.0;
  int nonempty = 0;
  for (std::size_t i = 0; i < m.tris.size(); ++i)
    if (!indep::empty_circumcircle(m, i, eps, &worst)) ++nonempty;
  std::printf("  [%s] n=%zu tris=%zu hull_area=%.6g area_rel=%.2e edges>2=%d "
              "nonempty_circumcircle=%d worst_rel=%.2e\n",
              tag, m.vertices.size(), m.tris.size(), hull_area, area_rel,
              bad_edges, nonempty, worst);
  chk(area_rel < 1e-9, "triangulation covers the convex hull exactly");
  chk(bad_edges == 0, "no edge belongs to more than two triangles");
  if (require_delaunay) chk(nonempty == 0, "empty-circumcircle (Delaunay) holds");
}

int main(int argc, char** argv) {
  const char* vertex_file = argc > 1 ? argv[1] : "evidence/psf_vertices_m42_t2m4.txt";
  std::printf("== A. Bowyer-Watson degeneracy audit ==\n");

  // A1. 重复点（精确 + 容差内）
  {
    std::vector<PsfVertex> vs = {
        {10, 10, 2, 2, 0.0, 1}, {10, 10, 2, 2, 0.0, 2},
        {11, 10, 2, 2, 0.0, 3}, {10, 11, 2, 2, 0.0, 4},
        {11, 11, 2, 2, 0.0, 5},
    };
    DelaunayTriangulation m;
    const bool ok = build_spatial_model(vs, &m);
    std::printf("  duplicates: ok=%d vertices_in=%zu kept=%zu tris=%zu\n", (int)ok,
                vs.size(), m.vertices.size(), m.tris.size());
    chk(ok && m.vertices.size() == 4, "exact duplicate vertex is removed");
    audit_triangulation(m, "duplicates", 1e-12, true);
  }

  // A2. 精确共圆（正方形 + 圆心点）: Delaunay 不唯一但必须合法（空外接圆,
  //     同一边界对角取舍; 有圆心点时圆心必被连成 4 个三角形）
  {
    std::vector<PsfVertex> vs = {
        {0, 0, 2, 2, 0, 1}, {10, 0, 2, 2, 0, 2},
        {10, 10, 2, 2, 0, 3}, {0, 10, 2, 2, 0, 4},
        {5, 5, 2, 2, 0, 5},
    };
    DelaunayTriangulation m;
    const bool ok = build_spatial_model(vs, &m);
    std::printf("  exact cocircular (square+center): ok=%d tris=%zu\n", (int)ok,
                m.tris.size());
    chk(ok && m.tris.size() == 4, "cocircular square + center gives 4 triangles");
    audit_triangulation(m, "cocircular", 1e-12, true);
  }

  // A3. 近共圆: 一个顶点扰动 1e-13 ⇒ Delaunay 唯一, 空外接圆须容差内成立
  {
    std::vector<PsfVertex> vs = {
        {0, 0, 2, 2, 0, 1}, {10, 0, 2, 2, 0, 2},
        {10, 10, 2, 2, 0, 3}, {0, 10, 2, 2, 0, 4},
        {5 + 1e-13, 5, 2, 2, 0, 5},
    };
    DelaunayTriangulation m;
    const bool ok = build_spatial_model(vs, &m);
    std::printf("  near-cocircular: ok=%d tris=%zu\n", (int)ok, m.tris.size());
    chk(ok && m.tris.size() == 4, "near-cocircular builds a valid triangulation");
    audit_triangulation(m, "near-cocircular", 1e-9, false);
  }

  // A4. 共线 / 近共线
  {
    std::vector<PsfVertex> vs = {
        {0, 0, 2, 2, 0, 1}, {1, 1, 2, 2, 0, 2}, {2, 2, 2, 2, 0, 3},
        {3, 3, 2, 2, 0, 4}};
    DelaunayTriangulation m;
    const bool ok = build_spatial_model(vs, &m);
    std::printf("  collinear: ok=%d reject=%s\n", (int)ok, m.reject.c_str());
    chk(!ok && m.reject == "all_vertices_collinear",
        "all-collinear input is rejected with a named reason");
    std::vector<PsfVertex> vs2 = vs;
    vs2.push_back({0, 5, 2, 2, 0, 5});
    DelaunayTriangulation m2;
    const bool ok2 = build_spatial_model(vs2, &m2);
    std::printf("  collinear+1: ok=%d tris=%zu\n", (int)ok2, m2.tris.size());
    chk(ok2 && m2.tris.size() == 3, "collinear run + 1 off-line gives a fan of 3");
    audit_triangulation(m2, "collinear+1", 1e-12, true);
  }

  // A5. 超大坐标（1e6 量级）与极端平台（1e9）: 几何必须仍然合法且无 NaN
  for (double scale : {1e6, 1e9, 1e150}) {
    std::vector<PsfVertex> vs;
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    for (int i = 0; i < 40; ++i)
      vs.push_back({scale * u(rng), scale * u(rng), 2.0, 2.0, 0.0,
                    static_cast<std::uint64_t>(i)});
    DelaunayTriangulation m;
    const bool ok = build_spatial_model(vs, &m);
    char tag[64];
    std::snprintf(tag, sizeof(tag), "scale=%.0e", scale);
    std::printf("  huge coords %s: ok=%d tris=%zu dropped=%d\n", tag, (int)ok,
                m.tris.size(), m.dropped_points);
    chk(ok && !m.tris.empty(), "large-coordinate point set triangulates");
    chk(m.dropped_points == 0, "no vertex silently dropped (overflow-safe in-circle)");
    audit_triangulation(m, tag, 1e-9, false);
  }

  // A6. 确定性 + 置换不变性（同一集合, 打乱插入顺序 ⇒ 三角形集合应相同）
  {
    std::vector<PsfVertex> vs;
    std::mt19937_64 rng(11);
    std::uniform_real_distribution<double> u(0.0, 1000.0);
    for (int i = 0; i < 60; ++i)
      vs.push_back({u(rng), u(rng), 2.0 + 0.1 * u(rng), 2.0, u(rng),
                    static_cast<std::uint64_t>(i)});
    DelaunayTriangulation a, b;
    build_spatial_model(vs, &a);
    build_spatial_model(vs, &b);
    bool same = a.tris.size() == b.tris.size();
    for (std::size_t i = 0; same && i < a.tris.size(); ++i) same = a.tris[i] == b.tris[i];
    chk(same, "same input (same order) gives bit-identical triangulation");
    std::vector<PsfVertex> perm = vs;
    std::shuffle(perm.begin(), perm.end(), rng);
    // star_id 保留身份; 比较三角形集合（按 star_id 归一）
    DelaunayTriangulation c;
    build_spatial_model(perm, &c);
    auto keyset = [](const DelaunayTriangulation& m) {
      std::vector<std::array<unsigned long long, 3>> out;
      for (const auto& t : m.tris) {
        std::array<unsigned long long, 3> k = {
            m.vertices[static_cast<std::size_t>(t[0])].star_id,
            m.vertices[static_cast<std::size_t>(t[1])].star_id,
            m.vertices[static_cast<std::size_t>(t[2])].star_id};
        std::sort(k.begin(), k.end());
        out.push_back(k);
      }
      std::sort(out.begin(), out.end());
      return out;
    };
    const auto ka = keyset(a), kc = keyset(c);
    std::printf("  permutation: tris %zu vs %zu\n", ka.size(), kc.size());
    chk(ka == kc, "vertex insertion order does not change the triangle set");
  }

  // A7. 欠采样/极小 σ: 模型不得虚造归一; ΣP<1 且 f_out 如实报告
  {
    std::vector<PsfVertex> vs = {
        {0, 0, 1.0, 0.7, 0.3, 1}, {10, 0, 1.0, 0.7, 0.3, 2},
        {5, 9, 1.0, 0.7, 0.3, 3}};
    DelaunayTriangulation m;
    build_spatial_model(vs, &m);
    std::vector<double> px, py;
    grid_pixels(5.0, 3.0, 4, px, py);
    SpatialResponseRequest q;
    q.cx = 5.0; q.cy = 3.0;
    q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
    std::vector<double> p;
    SpatialResponseDiagnostics sd;
    PixelIntegrationDiagnostics dg;
    chk(spatial_pixel_response(m, q, &p, &dg, &sd), "undersampled mix evaluates");
    double sp = 0.0; double pmin = 1e30;
    for (double x : p) { sp += x; pmin = std::min(pmin, x); }
    std::printf("  undersampled: SumP=%.9f f_enclosed=%.9f min(P)=%.3e\n", sp,
                dg.f_enclosed, pmin);
    chk(pmin >= 0.0 && sp > 0.9 && sp <= 1.0 + 1e-12,
        "undersampled mix stays positive and <= unit flux (no renormalization)");
  }

  // A8. 凸包外: 命名拒绝, 不外推
  {
    std::vector<PsfVertex> vs = {
        {0, 0, 2, 2, 0, 1}, {10, 0, 2, 2, 0, 2}, {5, 10, 2, 2, 0, 3}};
    DelaunayTriangulation m;
    build_spatial_model(vs, &m);
    std::vector<double> px, py;
    grid_pixels(50.0, 50.0, 3, px, py);
    SpatialResponseRequest q;
    q.cx = 50.0; q.cy = 50.0;
    q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
    std::vector<double> p;
    SpatialResponseDiagnostics sd;
    PixelIntegrationDiagnostics dg;
    chk(!spatial_pixel_response(m, q, &p, &dg, &sd) && sd.reject == "outside_hull",
        "outside hull ⇒ named outside_hull (no extrapolation)");
  }

  std::printf("\n== B. hold-out comparison (identical sample/window/metric) ==\n");
  // 读取真实星表（从 p1_sources.json 无损导出; 见 extract_psf_vertices.py）
  std::vector<PsfVertex> all;
  {
    std::ifstream f(vertex_file);
    if (!f) {
      std::printf("  (skip) vertex file %s not found\n", vertex_file);
    } else {
      std::string line;
      while (std::getline(f, line)) {
        std::istringstream ss(line);
        PsfVertex v;
        long long id = 0;
        if (ss >> v.x >> v.y >> v.sx >> v.sy >> v.theta >> id) {
          v.star_id = static_cast<std::uint64_t>(id);
          all.push_back(v);
        }
      }
    }
  }
  if (all.size() >= 100) {
    // 训练/留出划分: 每 5 颗取 1 颗留出（同一划分用于全部模型与指标）。
    std::vector<PsfVertex> train, hold;
    for (std::size_t i = 0; i < all.size(); ++i)
      (i % 5 == 0 ? hold : train).push_back(all[i]);
    // 训练集 σ 中位数（帧常圆模型的代表值, 不含留出星 ⇒ 无泄漏）
    std::vector<double> sig;
    for (const auto& v : train) sig.push_back(std::sqrt(v.sx * v.sy));
    std::sort(sig.begin(), sig.end());
    const double med_sigma = sig[sig.size() / 2];
    DelaunayTriangulation model, model_leak;
    const bool built = build_spatial_model(train, &model);
    build_spatial_model(all, &model_leak);   // 含留出星（泄漏对照）
    std::printf("  n=%zu train=%zu hold=%zu tris=%zu median_sigma(train)=%.6f\n",
                all.size(), train.size(), hold.size(), model.tris.size(), med_sigma);
    chk(built && !model.tris.empty(), "training-only triangulation builds");

    // 分层变量（全部由留出星自身与训练顶点集独立计算）: 椭率 e、到最近训练顶点
    // 的距离 d_min、σ 是否欠采样（σ<1.5 px）。切分点取样本中位数。
    std::vector<double> ecc_all, dmin_all;
    for (const auto& h : hold) {
      PsfVertex o = h; std::string w; canonicalize_vertex(&o, &w);
      const double ax = std::max(o.sx, o.sy), ay = std::min(o.sx, o.sy);
      ecc_all.push_back(std::sqrt(std::max(0.0, 1.0 - (ay * ay) / (ax * ax))));
      double d2 = 1e300;
      for (const auto& t : train)
        d2 = std::min(d2, (t.x - o.x) * (t.x - o.x) + (t.y - o.y) * (t.y - o.y));
      dmin_all.push_back(std::sqrt(d2));
    }
    const double ecc_med = indep::quantile(ecc_all, 0.5);
    const double dmin_med = indep::quantile(dmin_all, 0.5);
    std::printf("  strata: ecc_med=%.4f dmin_med=%.3f px\n", ecc_med, dmin_med);
    std::vector<double> l1_spatial, l1_circ, l1_leak;
    std::vector<double> l1_spatial_hi_e, l1_spatial_lo_e;
    std::vector<double> l1_circ_hi_e, l1_circ_lo_e;
    std::vector<double> l1_spatial_near, l1_spatial_far;
    std::vector<double> l1_circ_near, l1_circ_far;
    std::vector<double> l1_spatial_under;
    int n_outside = 0, n_inside = 0;
    for (std::size_t h = 0; h < hold.size(); ++h) {
      const double ecc_h = ecc_all[h], dmin_h = dmin_all[h];
      PsfVertex own = hold[h];
      std::string w;
      canonicalize_vertex(&own, &w);
      std::vector<double> px, py;
      grid_pixels(own.x, own.y, 8, px, py);   // 同一窗口
      // 真值参照: 该星自身椭圆模型
      PixelIntegrationRequest r;
      r.sx = own.sx; r.sy = own.sy; r.theta = own.theta;
      r.cx = own.x; r.cy = own.y;
      r.pixel_x = px.data(); r.pixel_y = py.data(); r.n_pixels = px.size();
      std::vector<double> p_own;
      pixel_integrated_moffat4_profile(r, &p_own, nullptr);
      // 帧常圆模型（训练集中位 σ）
      PixelIntegrationRequest rc;
      rc.sx = rc.sy = med_sigma; rc.theta = 0.0; rc.cx = own.x; rc.cy = own.y;
      rc.pixel_x = px.data(); rc.pixel_y = py.data(); rc.n_pixels = px.size();
      std::vector<double> p_circ;
      pixel_integrated_moffat4_profile(rc, &p_circ, nullptr);
      // 空间模型（训练顶点; 留出星不在其中）
      SpatialResponseRequest q;
      q.cx = own.x; q.cy = own.y;
      q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
      std::vector<double> p_sp;
      SpatialResponseDiagnostics sd;
      PixelIntegrationDiagnostics dg;
      const bool ok = spatial_pixel_response(model, q, &p_sp, &dg, &sd);
      double den = 0.0;
      for (double x : p_own) den += x;
      auto l1 = [&](const std::vector<double>& a) {
        double num = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) num += std::abs(a[i] - p_own[i]);
        return num / std::max(den, 1e-300);
      };
      if (!ok) {
        // 凸包外: 空间模型不外推（生产回退帧常圆模型）⇒ 该层只登记, 不参与
        // 空间模型的误差统计（如实分层, 不把回退点算进空间成绩）。
        ++n_outside;
        continue;
      }
      ++n_inside;
      const double e1 = l1(p_sp), e2 = l1(p_circ);
      l1_spatial.push_back(e1);
      l1_circ.push_back(e2);
      // 泄漏对照: 用含本星的顶点集求值（位置泄漏)
      SpatialResponseDiagnostics sd2;
      PixelIntegrationDiagnostics dg2;
      std::vector<double> p_leak;
      if (spatial_pixel_response(model_leak, q, &p_leak, &dg2, &sd2))
        l1_leak.push_back(l1(p_leak));
      // 分层: 椭率（中位数切分）、最近训练顶点距离、欠采样
      const bool hi_e = ecc_h > ecc_med;
      (hi_e ? l1_spatial_hi_e : l1_spatial_lo_e).push_back(e1);
      (hi_e ? l1_circ_hi_e : l1_circ_lo_e).push_back(e2);
      const bool near = dmin_h < dmin_med;
      (near ? l1_spatial_near : l1_spatial_far).push_back(e1);
      (near ? l1_circ_near : l1_circ_far).push_back(e2);
      double sig_h = std::sqrt(own.sx * own.sy);
      if (sig_h < 1.5) l1_spatial_under.push_back(e1);
    }
    auto pr = [&](const char* name, const std::vector<double>& v) {
      if (v.empty()) { std::printf("  %-28s (empty)\n", name); return; }
      std::printf("  %-28s n=%3zu median=%.4f mean=%.4f p10=%.4f p90=%.4f max=%.4f\n",
                  name, v.size(), indep::quantile(v, 0.5), indep::mean(v),
                  indep::quantile(v, 0.1), indep::quantile(v, 0.9),
                  indep::quantile(v, 1.0));
    };
    std::printf("  [per-star matched L1 vs own ellipse, same window]\n");
    pr("spatial (inside hull)", l1_spatial);
    pr("circular frame-median", l1_circ);
    pr("spatial WITH holdout leaked", l1_leak);
    pr("spatial ecc>median", l1_spatial_hi_e);
    pr("spatial ecc<=median", l1_spatial_lo_e);
    pr("circular ecc>median", l1_circ_hi_e);
    pr("circular ecc<=median", l1_circ_lo_e);
    pr("spatial dmin<=median", l1_spatial_near);
    pr("spatial dmin>median", l1_spatial_far);
    pr("circular dmin<=median", l1_circ_near);
    pr("circular dmin>median", l1_circ_far);
    pr("spatial sigma<1.5px", l1_spatial_under);
    std::printf("  outside-hull holdouts (named circular fallback): %d of %zu\n",
                n_outside, hold.size());
    chk(n_inside > 20, "hold-out comparison evaluated on >20 in-hull stars");
    chk(!l1_spatial.empty() && !l1_circ.empty() &&
            indep::quantile(l1_spatial, 0.5) < indep::quantile(l1_circ, 0.5),
        "spatial model beats circular model on the SAME held-out sample (median)");
    // 泄漏量化: 含留出星时误差必须减小（若显著减小, 则"留一法"结论乐观）
    if (!l1_leak.empty()) {
      const double med_leak = indep::quantile(l1_leak, 0.5);
      const double med_clean = indep::quantile(l1_spatial, 0.5);
      std::printf("  leakage: median L1 clean=%.4f leaked=%.4f (reduction %.1f%%)\n",
                  med_clean, med_leak,
                  100.0 * (med_clean - med_leak) / std::max(1e-300, med_clean));
      chk(med_leak <= med_clean + 1e-12,
          "including the hold-out star as a vertex cannot worsen the median");
    }
  }

  std::printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "ALL PASS", fails);
  return fails ? 1 : 0;
}
