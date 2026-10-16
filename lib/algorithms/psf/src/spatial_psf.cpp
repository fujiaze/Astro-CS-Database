// ACSD PSF — 空间 Moffat4 模型实现（PSF.md §4 基础完成模型）
#include "acsd/spatial_psf.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace acsd {
namespace psf {

namespace {

// ── 2D 几何基元（局部辅助; 不暴露） ────────────────────────────────────────
constexpr double kPi = 3.14159265358979323846;

double cross2(double ax, double ay, double bx, double by) {
  return ax * by - ay * bx;
}

// 点在三角形内（含边界的保守判定）; 返回 false 时 lambda 无意义。
// eps 为相对容差: λ >= -eps·scale。
bool barycentric(double px, double py, double x1, double y1, double x2,
                 double y2, double x3, double y3, double eps,
                 double* lambda) {
  const double den = cross2(x2 - x1, y2 - y1, x3 - x1, y3 - y1);
  if (!(std::abs(den) > 0.0)) return false;   // 退化（零面积）三角形
  const double l1 = cross2(x2 - px, y2 - py, x3 - px, y3 - py) / den;
  const double l2 = cross2(x3 - px, y3 - py, x1 - px, y1 - py) / den;
  const double l3 = 1.0 - l1 - l2;
  const double scale = std::max({std::abs(l1), std::abs(l2), std::abs(l3), 1.0});
  if (l1 < -eps * scale || l2 < -eps * scale || l3 < -eps * scale) return false;
  lambda[0] = l1;
  lambda[1] = l2;
  lambda[2] = l3;
  return true;
}

bool in_circumcircle(double px, double py, double x1, double y1, double x2,
                     double y2, double x3, double y3) {
  // 行列式形式的 in-circle 测试（与面积同号取向敏感; 统一为先定向 CCW）。
  double ax = x1, ay = y1, bx = x2, by = y2, cx = x3, cy = y3;
  if (cross2(bx - ax, by - ay, cx - ax, cy - ay) < 0.0) {
    std::swap(bx, cx);
    std::swap(by, cy);
  }
  const double m[3][3] = {{ax - px, ay - py, (ax - px) * (ax - px) + (ay - py) * (ay - py)},
                          {bx - px, by - py, (bx - px) * (bx - px) + (by - py) * (by - py)},
                          {cx - px, cy - py, (cx - px) * (cx - px) + (cy - py) * (cy - py)}};
  const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                     m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                     m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  return det > 0.0;   // 严格内（边界由 eps 收缩另行处理）
}

}  // namespace

bool canonicalize_vertex(PsfVertex* v, std::string* why) {
  if (v == nullptr) {
    if (why) *why = "null_vertex";
    return false;
  }
  if (!std::isfinite(v->x) || !std::isfinite(v->y) || !std::isfinite(v->sx) ||
      !std::isfinite(v->sy) || !std::isfinite(v->theta)) {
    if (why) *why = "non_finite_vertex";
    return false;
  }
  if (!(v->sx > 0.3) || !(v->sy > 0.3)) {
    // PSF.md §5.2: 拟合后 sx/sy <= 0.3 判为无效参数 ⇒ 不作顶点。
    if (why) *why = "sigma_below_fit_lower_bound";
    return false;
  }
  // theta 规范化: 模型对 theta 的周期是 pi（轴固定时）。先归约到 [0,pi)。
  double th = std::fmod(v->theta, kPi);
  if (th < 0.0) th += kPi;
  // 轴约定 sx >= sy（PSF.md §4 参数表）: sx < sy 时交换两轴并 theta += pi/2。
  // 这是**重参数化**, 模型值逐点不变（二次型同一）, 不改变可辨识性。
  if (v->sx < v->sy) {
    std::swap(v->sx, v->sy);
    th += 0.5 * kPi;
    if (th >= kPi) th -= kPi;
  }
  v->theta = th;
  return true;
}

bool build_spatial_model(const std::vector<PsfVertex>& in,
                         DelaunayTriangulation* out,
                         double coincident_tol) {
  if (out == nullptr) return false;
  *out = DelaunayTriangulation{};
  std::vector<PsfVertex> pts;
  pts.reserve(in.size());
  for (const auto& v0 : in) {
    PsfVertex v = v0;
    std::string why;
    if (!canonicalize_vertex(&v, &why)) {
      // 无效顶点被剔除（登记原因不逐条返回; 调用方按质量守卫生成顶点集）。
      continue;
    }
    // 重合点剔除（与已接受顶点的距离 < coincident_tol）。
    bool dup = false;
    for (const auto& p : pts) {
      if (std::abs(p.x - v.x) < coincident_tol &&
          std::abs(p.y - v.y) < coincident_tol) {
        dup = true;
        break;
      }
    }
    if (!dup) pts.push_back(v);
  }
  if (pts.size() < 3) {
    out->reject = "fewer_than_3_valid_vertices";
    return false;
  }
  // 全部共线 ⇒ 无三角剖分（行列式恒零 ⇒ Bowyer-Watson 不产生内部三角形）。
  {
    bool all_collinear = true;
    const double x0 = pts[0].x, y0 = pts[0].y;
    for (std::size_t i = 2; i < pts.size(); ++i) {
      const double d = cross2(pts[1].x - x0, pts[1].y - y0,
                              pts[i].x - x0, pts[i].y - y0);
      if (std::abs(d) > 0.0) {
        all_collinear = false;
        break;
      }
    }
    if (all_collinear) {
      out->reject = "all_vertices_collinear";
      return false;
    }
  }

  // 超级三角形（覆盖全部点; 典型探测器坐标量级 1e4 ⇒ 1e8 足够远且不溢出）。
  double minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
  for (const auto& p : pts) {
    minx = std::min(minx, p.x);
    maxx = std::max(maxx, p.x);
    miny = std::min(miny, p.y);
    maxy = std::max(maxy, p.y);
  }
  const double dx = maxx - minx, dy = maxy - miny;
  const double dmax = std::max(dx, dy);
  const double cx = 0.5 * (minx + maxx), cy = 0.5 * (miny + maxy);
  // 超级三角形顶点放在 pts 之外（下标 n, n+1, n+2）。
  const std::size_t n = pts.size();
  PsfVertex s0{cx - 20.0 * dmax, cy - dmax, 1.0, 1.0, 0.0, 0};
  PsfVertex s1{cx + 20.0 * dmax, cy - dmax, 1.0, 1.0, 0.0, 0};
  PsfVertex s2{cx, cy + 20.0 * dmax, 1.0, 1.0, 0.0, 0};
  std::vector<PsfVertex> all = pts;
  all.push_back(s0);
  all.push_back(s1);
  all.push_back(s2);

  // Bowyer–Watson: 初始只有一个超级三角形。
  std::vector<std::array<int, 3>> tris;
  tris.push_back({static_cast<int>(n), static_cast<int>(n + 1),
                  static_cast<int>(n + 2)});
  for (std::size_t ip = 0; ip < n; ++ip) {
    // 收集外接圆包含该点的三角形（"bad"集）与边界边。
    std::vector<std::array<int, 3>> bad;
    std::vector<std::pair<int, int>> edges;   // 有向边 (a,b)
    for (const auto& t : tris) {
      const PsfVertex& a = all[static_cast<std::size_t>(t[0])];
      const PsfVertex& b = all[static_cast<std::size_t>(t[1])];
      const PsfVertex& c = all[static_cast<std::size_t>(t[2])];
      if (in_circumcircle(all[ip].x, all[ip].y, a.x, a.y, b.x, b.y, c.x, c.y)) {
        bad.push_back(t);
        edges.emplace_back(t[0], t[1]);
        edges.emplace_back(t[1], t[2]);
        edges.emplace_back(t[2], t[0]);
      }
    }
    if (bad.empty()) continue;   // 数值退化: 不 Insert（点被忽略, 查询仍可用其余剖分）
    // 从 tris 删除 bad。
    std::vector<std::array<int, 3>> keep;
    keep.reserve(tris.size());
    for (const auto& t : tris) {
      bool is_bad = false;
      for (const auto& b : bad)
        if (t == b) {
          is_bad = true;
          break;
        }
      if (!is_bad) keep.push_back(t);
    }
    // 腔体边界: 只出现一次的边。
    std::vector<std::pair<int, int>> boundary;
    for (const auto& e : edges) {
      int cnt = 0;
      for (const auto& f : edges) {
        if (e == f || (e.first == f.second && e.second == f.first)) ++cnt;
      }
      if (cnt == 1) boundary.push_back(e);
    }
    // 重新连接: 新点与每条边界边成三角形。
    for (const auto& e : boundary) {
      keep.push_back({e.first, e.second, static_cast<int>(ip)});
    }
    tris.swap(keep);
  }
  // 移除接触超级顶点的三角形。
  std::vector<std::array<int, 3>> final_tris;
  for (const auto& t : tris) {
    if (t[0] >= static_cast<int>(n) || t[1] >= static_cast<int>(n) ||
        t[2] >= static_cast<int>(n))
      continue;
    final_tris.push_back(t);
  }
  out->vertices = pts;
  out->tris = final_tris;
  out->ok = !final_tris.empty();
  if (!out->ok) out->reject = "empty_triangulation_after_super_removal";
  return out->ok;
}

bool spatial_pixel_response(const DelaunayTriangulation& model,
                            const SpatialResponseRequest& req,
                            std::vector<double>* out_p,
                            PixelIntegrationDiagnostics* diag,
                            SpatialResponseDiagnostics* sdiag) {
  if (out_p == nullptr) return false;
  out_p->clear();
  if (diag) *diag = PixelIntegrationDiagnostics{};
  if (sdiag) *sdiag = SpatialResponseDiagnostics{};
  if (!model.ok || model.tris.empty()) {
    if (sdiag) sdiag->reject = "model_not_built";
    return false;
  }
  if (req.pixel_x == nullptr || req.pixel_y == nullptr || req.n_pixels == 0 ||
      !(req.order > 0)) {
    if (sdiag) sdiag->reject = "invalid_request";
    return false;
  }
  // 定位三角形（重心坐标; 容差 eps 允许边界命中）。
  const double eps = 1e-12;
  int hit = -1;
  double lam[3] = {0.0, 0.0, 0.0};
  for (std::size_t ti = 0; ti < model.tris.size(); ++ti) {
    const auto& t = model.tris[ti];
    const PsfVertex& a = model.vertices[static_cast<std::size_t>(t[0])];
    const PsfVertex& b = model.vertices[static_cast<std::size_t>(t[1])];
    const PsfVertex& c = model.vertices[static_cast<std::size_t>(t[2])];
    double l[3];
    if (barycentric(req.cx, req.cy, a.x, a.y, b.x, b.y, c.x, c.y, eps, l)) {
      hit = static_cast<int>(ti);
      lam[0] = l[0];
      lam[1] = l[1];
      lam[2] = l[2];
      if (sdiag) {
        sdiag->tri = hit;
        sdiag->lambda[0] = lam[0];
        sdiag->lambda[1] = lam[1];
        sdiag->lambda[2] = lam[2];
        sdiag->star_ids[0] = a.star_id;
        sdiag->star_ids[1] = b.star_id;
        sdiag->star_ids[2] = c.star_id;
        sdiag->max_vertex_sigma =
            std::max({std::sqrt(a.sx * a.sy), std::sqrt(b.sx * b.sy),
                      std::sqrt(c.sx * c.sy)});
        sdiag->min_vertex_sigma =
            std::min({std::sqrt(a.sx * a.sy), std::sqrt(b.sx * b.sy),
                      std::sqrt(c.sx * c.sy)});
      }
      break;
    }
  }
  if (hit < 0) {
    if (sdiag) sdiag->reject = "outside_hull";
    return false;
  }
  // 逐顶点: 单位总通量模型平移到同一参考中心 u, 同一像元边界求积。
  const auto& t = model.tris[static_cast<std::size_t>(hit)];
  std::vector<double> mix(req.n_pixels, 0.0);
  double enc = 0.0;
  double err_max = -1.0;
  int order_seen = 0;
  for (int s = 0; s < 3; ++s) {
    const PsfVertex& v = model.vertices[static_cast<std::size_t>(t[s])];
    if (lam[s] <= 0.0) continue;   // 边界上的点: 零权重顶点不求积（省算力）
    PixelIntegrationRequest r;
    r.sx = v.sx;
    r.sy = v.sy;
    r.theta = v.theta;
    r.cx = req.cx;   // 顶点模型平移到查询中心
    r.cy = req.cy;
    r.pixel_x = req.pixel_x;
    r.pixel_y = req.pixel_y;
    r.n_pixels = req.n_pixels;
    r.order = req.order;
    std::vector<double> ps;
    PixelIntegrationDiagnostics ds;
    if (!pixel_integrated_moffat4_profile(r, &ps, &ds)) {
      if (sdiag) sdiag->reject = "vertex_integration_failed";
      return false;
    }
    for (std::size_t i = 0; i < req.n_pixels; ++i)
      mix[i] += lam[s] * ps[i];
    enc += lam[s] * ds.f_enclosed;
    if (ds.estimated_error >= 0.0)
      err_max = std::max(err_max, ds.estimated_error);
    order_seen = ds.order;
  }
  // 混合后 f_enc = Σ λ_s f_enc,s（线性组合保持正性与归一; ΣP 仍不重归一）。
  *out_p = mix;
  if (diag) {
    diag->method = PixelIntegrationMethod::kGaussLegendreTensor;
    diag->order = order_seen;
    diag->estimated_error = err_max;
    diag->f_enclosed = enc;
    diag->f_out = 1.0 - enc;
    diag->n_pixels = static_cast<int>(req.n_pixels);
  }
  if (sdiag) sdiag->ok = true;
  return true;
}

}  // namespace psf
}  // namespace acsd
