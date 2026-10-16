// ACSD PSF — 空间 Moffat4 模型验证（PSF.md §4 基础完成模型）
//
// 判据（expected 不由被测实现反推）:
//  1. 规范化恒等: (sx,sy,θ) 与 (sy,sx,θ+π/2) 给出**同一**二次型 ⇒ 两种输入
//     的逐像元响应逐位级一致（PSF.md §2.2 旋转简并）。
//  2. 顶点命中: 查询 = 顶点位置 ⇒ λ=(1,0,0), 响应 == 该顶点单体响应。
//  3. 质心混合: 查询 = 三角形质心 ⇒ λ=(1/3,1/3,1/3), 响应 == 三顶点响应的
//     算术平均（凸组合线性）。
//  4. 壳外不外推: 凸包外查询 ⇒ outside_hull 具名拒绝（正本明示）。
//  5. 留一法（真实 p1_sources 518 星顶点）: 从顶点集移除一颗留出星, 在其位置
//     求值, 与该星自身单体响应对比 ⇒ 空间插值误差的实测分布（PSF.md §4
//     「必须用空间留出星及注入回收量化模板失配」）。
//  6. 顶点的规范化不改变模型值（θ mod π 与轴交换后二次型相同）。
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
    const double d = std::abs(a[i] - b[i]);
    const double s = std::max(1e-14, std::max(std::abs(a[i]), std::abs(b[i])));
    m = std::max(m, d / s);
  }
  return m;
}

// 网格像元集（以 (cx,cy) 为中心）
static void grid_pixels(double cx, double cy, int half, std::vector<double>& px,
                        std::vector<double>& py) {
  px.clear();
  py.clear();
  for (int y = static_cast<int>(std::floor(cy)) - half;
       y <= static_cast<int>(std::floor(cy)) + half; ++y)
    for (int x = static_cast<int>(std::floor(cx)) - half;
         x <= static_cast<int>(std::floor(cx)) + half; ++x) {
      px.push_back(x + 0.0);
      py.push_back(y + 0.0);
    }
}

int main(int argc, char** argv) {
  const char* vertex_file = argc > 1 ? argv[1] : "/tmp/spatial_vertices.txt";

  // ── 1/6. 规范化恒等（旋转简并 + 轴约定） ───────────────────────────────
  {
    PsfVertex a{100.0, 200.0, 3.0, 2.0, 0.7, 1};
    PsfVertex b{100.0, 200.0, 2.0, 3.0, 0.7 + M_PI / 2, 2};
    std::string w1, w2;
    chk(canonicalize_vertex(&a, &w1) && canonicalize_vertex(&b, &w2),
        "both canonicalize ok");
    chk(std::abs(a.sx - b.sx) < 1e-15 && std::abs(a.sy - b.sy) < 1e-15 &&
            std::abs(a.theta - b.theta) < 1e-15,
        "axis swap + theta+pi/2 canonicalizes to identical parameters");
    std::vector<double> px, py, p1v, p2v;
    grid_pixels(100.0, 200.0, 8, px, py);
    PixelIntegrationRequest r1;
    r1.sx = a.sx; r1.sy = a.sy; r1.theta = a.theta; r1.cx = 100.0; r1.cy = 200.0;
    r1.pixel_x = px.data(); r1.pixel_y = py.data(); r1.n_pixels = px.size();
    pixel_integrated_moffat4_profile(r1, &p1v, nullptr);
    PixelIntegrationRequest r2;
    r2.sx = 3.0; r2.sy = 2.0; r2.theta = 0.7; r2.cx = 100.0; r2.cy = 200.0;
    r2.pixel_x = px.data(); r2.pixel_y = py.data(); r2.n_pixels = px.size();
    pixel_integrated_moffat4_profile(r2, &p2v, nullptr);
    std::printf("  degenerate-pair max rel diff = %.3e\n", relmax(p1v, p2v));
    chk(relmax(p1v, p2v) < 1e-12,
        "rotational-degenerate pair gives identical per-pixel response");
    // θ mod π 恒等
    PsfVertex c{100.0, 200.0, 3.0, 2.0, 0.7 + 17 * M_PI, 3};
    std::string w3;
    canonicalize_vertex(&c, &w3);
    chk(std::abs(c.theta - 0.7) < 1e-12, "theta mod pi reduction exact");
  }

  // ── 2/3/4. 小世界三角形: 三顶点 ────────────────────────────────────────
  {
    std::vector<PsfVertex> vs = {
        {100.0, 100.0, 2.0, 2.0, 0.0, 11},
        {103.0, 100.0, 3.0, 2.0, 0.0, 12},
        {101.0, 104.0, 2.5, 2.0, 0.0, 13},
    };
    DelaunayTriangulation m;
    chk(build_spatial_model(vs, &m), "3-vertex triangulation builds");
    chk(m.tris.size() == 1, "exactly one triangle");
    std::vector<double> px, py;
    // 顶点命中
    grid_pixels(100.0, 100.0, 8, px, py);
    SpatialResponseRequest q;
    q.cx = 100.0; q.cy = 100.0;
    q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
    q.order = 10;
    std::vector<double> pv;
    SpatialResponseDiagnostics sd;
    PixelIntegrationDiagnostics dg;
    chk(spatial_pixel_response(m, q, &pv, &dg, &sd), "vertex query inside hull");
    chk(sd.ok && (std::abs(sd.lambda[0] - 1.0) < 1e-9) &&
            sd.star_ids[0] == 11,
        "vertex query returns lambda=(1,0,0) with the vertex model");
    PixelIntegrationRequest r1;
    r1.sx = 2.0; r1.sy = 2.0; r1.theta = 0.0; r1.cx = 100.0; r1.cy = 100.0;
    r1.pixel_x = px.data(); r1.pixel_y = py.data(); r1.n_pixels = px.size();
    std::vector<double> direct;
    pixel_integrated_moffat4_profile(r1, &direct, nullptr);
    chk(relmax(pv, direct) < 1e-12,
        "vertex query response == single-vertex response");
    // 质心混合
    const double ccx = (100.0 + 103.0 + 101.0) / 3.0;
    const double ccy = (100.0 + 100.0 + 104.0) / 3.0;
    grid_pixels(ccx, ccy, 8, px, py);
    q.cx = ccx; q.cy = ccy;
    q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
    std::vector<double> pc;
    chk(spatial_pixel_response(m, q, &pc, &dg, &sd), "centroid query ok");
    chk(std::abs(sd.lambda[0] - 1.0 / 3) < 1e-9 &&
            std::abs(sd.lambda[1] - 1.0 / 3) < 1e-9,
        "centroid weights = 1/3 each");
    // 算术平均（三条独立路径: 每顶点平移到质心后求积再平均）
    std::vector<double> avg(px.size(), 0.0);
    const std::vector<PsfVertex> vv = vs;
    for (const auto& v : vv) {
      PixelIntegrationRequest r;
      r.sx = v.sx; r.sy = v.sy; r.theta = v.theta; r.cx = ccx; r.cy = ccy;
      r.pixel_x = px.data(); r.pixel_y = py.data(); r.n_pixels = px.size();
      std::vector<double> one;
      pixel_integrated_moffat4_profile(r, &one, nullptr);
      for (std::size_t i = 0; i < one.size(); ++i) avg[i] += one[i] / 3.0;
    }
    std::printf("  centroid mix max rel diff vs manual mean = %.3e\n",
                relmax(pc, avg));
    chk(relmax(pc, avg) < 1e-12,
        "barycentric mix == arithmetic mean of translated vertex responses");
    // 凸包外 ⇒ 拒绝
    grid_pixels(200.0, 300.0, 4, px, py);
    q.cx = 200.0; q.cy = 300.0;
    q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
    SpatialResponseDiagnostics sd2;
    chk(!spatial_pixel_response(m, q, &pv, &dg, &sd2) &&
            sd2.reject == "outside_hull",
        "query outside convex hull ⇒ named outside_hull rejection");
  }

  // ── 5. 真实星顶点 + 留一法 ─────────────────────────────────────────────
  {
    std::ifstream f(vertex_file);
    if (!f) {
      std::printf("  (skip) vertex file %s not found\n", vertex_file);
    } else {
      std::vector<PsfVertex> all;
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
      std::printf("  loaded %zu real star vertices\n", all.size());
      DelaunayTriangulation m;
      const bool built = build_spatial_model(all, &m);
      std::printf("  triangulation: ok=%d tris=%zu reject=%s\n", (int)m.ok,
                  m.tris.size(), m.reject.c_str());
      chk(built && m.tris.size() > 100,
          "518 real stars build a dense triangulation");
      // 留一法: 每第 37 颗星取一颗, 从顶点集移除后在其位置求值
      double worst = 0.0, sum = 0.0;
      int n_tested = 0;
      for (std::size_t k = 0; k < all.size(); k += 37) {
        std::vector<PsfVertex> subset = all;
        subset.erase(subset.begin() + static_cast<std::ptrdiff_t>(k));
        DelaunayTriangulation mm;
        if (!build_spatial_model(subset, &mm)) continue;
        const PsfVertex& held = all[k];
        std::vector<double> px, py;
        grid_pixels(held.x, held.y, 8, px, py);
        SpatialResponseRequest q;
        q.cx = held.x; q.cy = held.y;
        q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
        std::vector<double> mix, alone;
        SpatialResponseDiagnostics sd;
        PixelIntegrationDiagnostics dg;
        if (!spatial_pixel_response(mm, q, &mix, &dg, &sd)) continue;
        // 独立单体响应（该星自己的参数, 已规范化）
        PsfVertex vv = held;
        std::string w;
        canonicalize_vertex(&vv, &w);
        PixelIntegrationRequest r;
        r.sx = vv.sx; r.sy = vv.sy; r.theta = vv.theta;
        r.cx = held.x; r.cy = held.y;
        r.pixel_x = px.data(); r.pixel_y = py.data(); r.n_pixels = px.size();
        pixel_integrated_moffat4_profile(r, &alone, nullptr);
        double num = 0.0, den = 0.0;
        for (std::size_t i = 0; i < mix.size(); ++i) {
          num += std::abs(mix[i] - alone[i]);
          den += alone[i];
        }
        const double rel = num / std::max(den, 1e-30);
        worst = std::max(worst, rel);
        sum += rel;
        ++n_tested;
      }
      std::printf("  leave-one-out: n=%d  mean L1 rel diff=%.4f  worst=%.4f\n",
                  n_tested, n_tested ? sum / n_tested : -1.0, worst);
      chk(n_tested >= 10,
          "leave-one-out evaluated on >=10 held-out stars");
      // 量化门: 与圆模型逐点失配 median 0.47（留出星分析实测）相比, 空间模型
      // 的 L1 模板失配必须显著更小; 取 0.15 为本轮观测门（证据见
      // evidence/verify_round5_spatial_psf.log 与 ROUND3_FIX_REPORT §6）。
      chk(n_tested > 0 && sum / n_tested < 0.15,
          "spatial model L1 template mismatch < 0.15 (vs circular model 0.47)");
      // 查询点落在三角形顶点上时退化为单体（命中已有顶点）
      const PsfVertex& v0 = all[0];
      std::vector<double> px, py;
      grid_pixels(v0.x, v0.y, 8, px, py);
      SpatialResponseRequest q;
      q.cx = v0.x; q.cy = v0.y;
      q.pixel_x = px.data(); q.pixel_y = py.data(); q.n_pixels = px.size();
      std::vector<double> pv;
      SpatialResponseDiagnostics sd;
      PixelIntegrationDiagnostics dg;
      chk(spatial_pixel_response(m, q, &pv, &dg, &sd) && sd.ok,
          "query at an existing vertex succeeds inside hull");
    }
  }

  std::printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "ALL PASS", fails);
  return fails ? 1 : 0;
}
