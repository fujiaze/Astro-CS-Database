/* weight_chain_selfcheck.cpp — 权重链独立合成 Oracle（C++ 侧）
 *
 * 独立性边界:
 *   - 期望值在本 TU 内**独立复算**（走 sigma_F 路径 1/sigma_F^2、独立写的双线性、
 *     独立写的自然边界三次样条——后者用**稠密高斯消元**解三对角系统，
 *     不调用被测实现的 Thomas 消元与求值路径），只调用其公开 API 对拍；
 *   - Python 侧另有 oracle/weight_chain_oracle.py 以不同语言独立复算同一公式，
 *     oracle/recon_exp04_parity.py 与实验单元 EXP-04 的算子实现逐像素对拍。
 *
 * 正例: 注入已知 SNR ⇒ 权重可复算（相对容差 1e-12）；重建算子 ⇒ 与独立复算一致。
 * 负例: SNR 缺失/非有限/非正、F_ref 非法、稀疏层损坏/越界/几何错位/算子未识别、
 *       SNR 语义冒充、legacy_allow_weight_fallback ⇒ 全部 fail-closed 且不得静默退化。
 *
 * 构建（前台统一）: 见 oracle/CMakeLists.txt。
 */
#include "astrocs/v6/weight_chain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using astrocs::v6::p2weight::FrameSnrKind;
using astrocs::v6::p2weight::FrameWeightInput;
using astrocs::v6::p2weight::SparseReconstruction;
using astrocs::v6::p2weight::SparseReconOperator;
using astrocs::v6::p2weight::SparseSnrLayer;
using astrocs::v6::p2weight::SparseSnrPoint;
using astrocs::v6::p2weight::SparseSnrReconstructor;
using astrocs::v6::p2weight::WeightChainPolicy;
using astrocs::v6::p2weight::WeightClosure;
using astrocs::v6::p2weight::WeightChainResult;

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool cond, const std::string& what) {
  if (cond) { ++g_pass; std::printf("  [PASS] %s\n", what.c_str()); }
  else { ++g_fail; std::printf("  [FAIL] %s\n", what.c_str()); }
}

bool close(double a, double b, double rtol = 1e-12) {
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  const double d = std::fabs(a - b);
  return d <= rtol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

/* ---- 独立复算路径（不调用被测实现） ---- */
double oracle_weight_from_snr(double snr, double fref) {
  const double sigma_f = fref / snr;      /* sigma_F = F_ref / SNR */
  return 1.0 / (sigma_f * sigma_f);       /* w = 1/sigma_F^2 */
}
double oracle_bilinear(const SparseSnrLayer& L, double x, double y) {
  const double gx = (x - L.x0) / L.dx;
  const double gy = (y - L.y0) / L.dy;
  int i0 = (int)std::floor(gx);
  int j0 = (int)std::floor(gy);
  if (i0 > L.nx - 2) i0 = L.nx - 2;
  if (j0 > L.ny - 2) j0 = L.ny - 2;
  if (i0 < 0) i0 = 0;
  if (j0 < 0) j0 = 0;
  const double fx = gx - i0, fy = gy - j0;
  const std::size_t nxl = (std::size_t)L.nx;
  const double v00 = L.points[(std::size_t)j0 * nxl + (std::size_t)i0].snr;
  const double v10 = L.points[(std::size_t)j0 * nxl + (std::size_t)i0 + 1].snr;
  const double v01 = L.points[(std::size_t)(j0 + 1) * nxl + (std::size_t)i0].snr;
  const double v11 = L.points[(std::size_t)(j0 + 1) * nxl + (std::size_t)i0 + 1].snr;
  return (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 + (1 - fx) * fy * v01 + fx * fy * v11;
}

/* 自然边界三次样条：**独立实现**（稠密高斯消元 + 部分主元），刻意不复用被测
 * 实现的 Thomas 消元；节点等距 h=1。 */
std::vector<double> oracle_spline_M(const std::vector<double>& y) {
  const int n = (int)y.size();
  std::vector<double> M((std::size_t)n, 0.0);
  if (n < 3) return M;
  const int m = n - 2;
  std::vector<std::vector<double>> A((std::size_t)m, std::vector<double>((std::size_t)m, 0.0));
  std::vector<double> b((std::size_t)m, 0.0);
  for (int r = 0; r < m; ++r) {
    A[(std::size_t)r][(std::size_t)r] = 4.0;
    if (r > 0) A[(std::size_t)r][(std::size_t)(r - 1)] = 1.0;
    if (r < m - 1) A[(std::size_t)r][(std::size_t)(r + 1)] = 1.0;
    b[(std::size_t)r] = 6.0 * (y[(std::size_t)(r + 2)] - 2.0 * y[(std::size_t)(r + 1)] +
                               y[(std::size_t)r]);
  }
  for (int c = 0; c < m; ++c) {
    int piv = c;
    for (int r = c + 1; r < m; ++r)
      if (std::fabs(A[(std::size_t)r][(std::size_t)c]) >
          std::fabs(A[(std::size_t)piv][(std::size_t)c])) piv = r;
    std::swap(A[(std::size_t)c], A[(std::size_t)piv]);
    std::swap(b[(std::size_t)c], b[(std::size_t)piv]);
    const double d = A[(std::size_t)c][(std::size_t)c];
    for (int r = c + 1; r < m; ++r) {
      const double f = A[(std::size_t)r][(std::size_t)c] / d;
      for (int k = c; k < m; ++k)
        A[(std::size_t)r][(std::size_t)k] -= f * A[(std::size_t)c][(std::size_t)k];
      b[(std::size_t)r] -= f * b[(std::size_t)c];
    }
  }
  std::vector<double> sol((std::size_t)m, 0.0);
  for (int r = m - 1; r >= 0; --r) {
    double s = b[(std::size_t)r];
    for (int k = r + 1; k < m; ++k) s -= A[(std::size_t)r][(std::size_t)k] * sol[(std::size_t)k];
    sol[(std::size_t)r] = s / A[(std::size_t)r][(std::size_t)r];
  }
  for (int r = 0; r < m; ++r) M[(std::size_t)(r + 1)] = sol[(std::size_t)r];
  return M;
}

double oracle_spline_eval(const std::vector<double>& y, const std::vector<double>& M, double pos) {
  const int n = (int)y.size();
  if (n == 1) return y[0];
  int i = (int)std::floor(pos);
  if (i < 0) i = 0;
  if (i > n - 2) i = n - 2;
  double h = pos - (double)i;
  if (h < 0.0) h = 0.0;
  if (h > 1.0) h = 1.0;
  const double y0 = y[(std::size_t)i], y1 = y[(std::size_t)(i + 1)];
  const double m0 = M[(std::size_t)i], m1 = M[(std::size_t)(i + 1)];
  const double bb = (y1 - y0) - (2.0 * m0 + m1) / 6.0;
  return y0 + bb * h + m0 * h * h / 2.0 + (m1 - m0) * h * h * h / 6.0;
}

/* 独立复算的「自然样条 + 值域钳制」2D 重建（控制网格 ctrl 行主序 ny*nx）。 */
double oracle_spline_clip(const std::vector<double>& ctrl, int nx, int ny, double x0, double y0,
                          double dx, double dy, double x, double y) {
  const double gx = (x - x0) / dx;
  const double gy = (y - y0) / dy;
  const double cx = std::min(std::max(gx, 0.0), (double)(nx - 1));
  const double cy = std::min(std::max(gy, 0.0), (double)(ny - 1));
  std::vector<double> row((std::size_t)nx, 0.0);
  for (int i = 0; i < nx; ++i) {
    std::vector<double> col((std::size_t)ny, 0.0);
    for (int j = 0; j < ny; ++j) col[(std::size_t)j] = ctrl[(std::size_t)j * (std::size_t)nx + (std::size_t)i];
    const std::vector<double> M = oracle_spline_M(col);
    row[(std::size_t)i] = oracle_spline_eval(col, M, cy);
  }
  const std::vector<double> Mx = oracle_spline_M(row);
  double v = oracle_spline_eval(row, Mx, cx);
  double lo = ctrl[0], hi = ctrl[0];
  for (double t : ctrl) { lo = std::min(lo, t); hi = std::max(hi, t); }
  v = std::min(std::max(v, lo), hi);
  return v;
}

/* 独立复算的 3x3 mesh 中值（边界 replicate，无条件替换）。 */
std::vector<double> oracle_median3(const std::vector<double>& g, int nx, int ny) {
  std::vector<double> out(g.size(), 0.0);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      std::vector<double> w;
      for (int dj = -1; dj <= 1; ++dj)
        for (int di = -1; di <= 1; ++di) {
          int jj = std::min(std::max(j + dj, 0), ny - 1);
          int ii = std::min(std::max(i + di, 0), nx - 1);
          w.push_back(g[(std::size_t)jj * (std::size_t)nx + (std::size_t)ii]);
        }
      std::sort(w.begin(), w.end());
      out[(std::size_t)j * (std::size_t)nx + (std::size_t)i] = w[4];
    }
  }
  return out;
}

FrameWeightInput mk(const std::string& id, double snr) {
  FrameWeightInput f;
  f.frame_id = id;
  f.kind = FrameSnrKind::kFluxTypeUnweightedSnr;
  f.has_frame_snr = true;
  f.frame_snr = snr;
  return f;
}

/* 2x2 规则网格: 行主序 j*nx+i。dx=dy=1 ⇒ cell 中心 = origin + (dx-1)/2 = 0
   ⇒ x0=y0=0 本身就是 cell 中心（Δ=1 时节点落在整数像素中心）。 */
SparseSnrLayer grid2x2() {
  SparseSnrLayer L;
  L.present = true;
  L.regular_grid = true;
  L.nx = 2; L.ny = 2;
  L.x0 = 0.0; L.y0 = 0.0; L.dx = 1.0; L.dy = 1.0;
  L.points = {{0, 0, 1.0}, {1, 0, 1.2}, {0, 1, 0.8}, {1, 1, 1.0}};
  return L;
}

/* 4x4 规则网格（Δ=8，origin=0 ⇒ 节点在 cell 中心 x0 = (8-1)/2 = 3.5）。
   控制值取固定常数表（无随机数 ⇒ 逐位可复现）。 */
SparseSnrLayer grid4x4() {
  SparseSnrLayer L;
  L.present = true;
  L.regular_grid = true;
  L.nx = 4; L.ny = 4;
  L.dx = 8.0; L.dy = 8.0;
  L.x0 = 3.5; L.y0 = 3.5;
  L.grid_origin_x = 0.0; L.grid_origin_y = 0.0;
  const double v[16] = {1.0,  1.1, 0.9,  1.05,
                        0.8,  1.4, 1.2,  0.7,
                        1.3,  0.6, 1.5,  1.0,
                        0.95, 1.25, 0.85, 1.35};
  for (int j = 0; j < 4; ++j)
    for (int i = 0; i < 4; ++i)
      L.points.push_back(SparseSnrPoint{3.5 + 8.0 * i, 3.5 + 8.0 * j, v[j * 4 + i]});
  return L;
}

std::vector<double> ctrl_of(const SparseSnrLayer& L) {
  std::vector<double> c((std::size_t)L.nx * (std::size_t)L.ny, 0.0);
  for (std::size_t k = 0; k < L.points.size(); ++k) c[k] = L.points[k].snr;
  return c;
}

}  /* namespace */

int main() {
  std::printf("== weight_chain selfcheck (independent C++ oracle) ==\n");

  /* ---------- 正例 1: 标量换算 w = SNR^2/F_ref^2 = 1/sigma_F^2 ---------- */
  {
    std::printf("[positive] scalar w = SNR^2/F_ref^2\n");
    const double fref = 1000.0;
    const double snrs[3] = {200.0, 100.0, 50.0};
    for (double s : snrs) {
      double w = 0.0; std::string e;
      const bool ok = astrocs::v6::p2weight::weight_from_snr(s, fref, &w, &e);
      check(ok, "weight_from_snr accepted finite positive SNR");
      check(close(w, oracle_weight_from_snr(s, fref)),
            "w matches independent 1/sigma_F^2 oracle");
      check(close(w, (s * s) / (fref * fref)), "w matches SNR^2/F_ref^2 form");
    }
  }

  /* ---------- 正例 2: 无稀疏层多帧权重 ---------- */
  {
    std::printf("[positive] multi-frame without sparse layer\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0), mk("f1", 100.0), mk("f2", 50.0)};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(r.ok && r.weight_chain_closed && r.production_allowed,
          "chain closed and production allowed");
    check(r.closure == WeightClosure::kClosed, "closure token = closed");
    check(r.weight_source == "frame_snr", "weight_source = frame_snr");
    check(r.weights.size() == 3, "one weight per input frame");
    bool all = true;
    for (std::size_t i = 0; i < frames.size(); ++i)
      all = all && close(r.weights[i], oracle_weight_from_snr(frames[i].frame_snr, fref));
    check(all, "all weights match independent oracle");
    /* SNR_combined^2 = Sum SNR_k^2 (independent frames) */
    double sum_snr2 = 0.0;
    for (const auto& f : frames) sum_snr2 += f.frame_snr * f.frame_snr;
    double sum_w = 0.0;
    for (double w : r.weights) sum_w += w;
    check(close(sum_w * fref * fref, sum_snr2, 1e-12),
          "SNR_combined^2 = Sum SNR_k^2 identity holds");
  }

  /* ---------- 正例 3: 稀疏层 帧级×帧内（默认算子 = 自然样条 + 钳制） ---------- */
  {
    std::printf("[positive] sparse layer composition (frame x intra, default operator)\n");
    const double fref = 1000.0;
    SparseSnrLayer L = grid2x2();
    std::vector<FrameWeightInput> frames;
    FrameWeightInput a = mk("f0", 200.0); a.sparse = &L; a.x = 0.5; a.y = 0.5;
    FrameWeightInput b = mk("f1", 100.0); b.sparse = &L; b.x = 0.25; b.y = 0.5;
    frames = {a, b};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(r.ok && r.weight_chain_closed, "sparse chain closed");
    check(r.weight_source == "frame_snr_x_sparse_snr", "weight_source = frame_snr_x_sparse_snr");
    /* 2x2 控制网格上自然样条退化为双线性（M ≡ 0）⇒ 期望值仍可用独立双线性复算。
       注意：默认算子已从 bilinear_regular_grid_v1 改为
       natural_bicubic_spline_clip_v1（实验 EXP-04 §4.1 推荐），本行断言随之更新。 */
    const double intra0 = oracle_bilinear(L, 0.5, 0.5);
    const double intra1 = oracle_bilinear(L, 0.25, 0.5);
    check(close(r.intra_snr[0], intra0), "intra[0] matches independent oracle");
    check(close(r.intra_snr[1], intra1), "intra[1] matches independent oracle");
    check(close(r.actual_snr[0], 200.0 * intra0), "actual SNR = frame x intra (f0)");
    check(close(r.weights[0], oracle_weight_from_snr(200.0 * intra0, fref)),
          "weight[0] from composed SNR matches oracle");
    check(r.sparse_operator_ids[0] == "natural_bicubic_spline_clip_v1",
          "default reconstruction operator id recorded (natural_bicubic_spline_clip_v1)");
    check(r.sparse_node_residual[0] <= 1e-9, "node reproduction residual ~ 0");
  }

  /* ---------- 正例 4: 4x4 网格上默认算子 = 独立复算的自然样条 + 钳制 ---------- */
  {
    std::printf("[positive] default operator vs independent natural-spline oracle (4x4)\n");
    SparseSnrLayer L = grid4x4();
    const std::vector<double> c = ctrl_of(L);
    SparseSnrReconstructor rec;
    std::string err;
    const bool ok = rec.prepare(L, &err);
    check(ok, std::string("prepare succeeded: ") + err);
    check(rec.operator_id() == std::string("natural_bicubic_spline_clip_v1"),
          "effective operator = default natural_bicubic_spline_clip_v1");
    check(rec.mesh_median_applied() == false, "mesh median filter OFF by default");
    check(rec.value_range_clipped() == true, "value-range clip ON for the default operator");
    check(rec.cell_center_offset_max_abs() <= 1e-9, "control points are at cell centers");
    check(rec.n_invalid_control_points_filled() == 0, "no invalid control point to fill");
    bool all = true;
    double worst = 0.0;
    const double qs[6][2] = {{3.5, 3.5}, {11.5, 3.5}, {3.5, 27.5}, {20.0, 12.0}, {0.0, 0.0},
                             {31.0, 31.0}};
    for (const auto& q : qs) {
      double v = 0.0;
      SparseReconstruction info;
      const bool e2 = rec.eval(q[0], q[1], &v, &info, &err);
      const double want =
          oracle_spline_clip(c, L.nx, L.ny, L.x0, L.y0, L.dx, L.dy, q[0], q[1]);
      all = all && e2 && close(v, want, 1e-11);
      worst = std::max(worst, std::fabs(v - want));
    }
    check(all, "all query points match the independent spline+clip oracle");
    std::printf("        (max abs deviation vs independent oracle: %.3e)\n", worst);
    /* 非退化：4x4 网格上样条必须与双线性**不同**（否则"换默认算子"无实质变化）。 */
    double vb = 0.0;
    L.reconstruction_operator = "bilinear_regular_grid_v1";
    SparseSnrReconstructor recb;
    std::string eb;
    check(recb.prepare(L, &eb), "bilinear operator still available (retained)");
    check(recb.eval(20.0, 12.0, &vb, nullptr, &eb), "bilinear eval ok");
    check(std::fabs(vb - oracle_spline_clip(c, L.nx, L.ny, L.x0, L.y0, L.dx, L.dy, 20.0, 12.0)) >
              1e-3,
          "spline default differs from bilinear on a 4x4 grid (non-vacuous default change)");
  }

  /* ---------- 正例 5: 高对比档 = mesh 中值 + 自然样条 + 钳制 ---------- */
  {
    std::printf("[positive] mesh-median operator vs independent oracle\n");
    SparseSnrLayer L = grid4x4();
    L.reconstruction_operator = "natural_bicubic_spline_clip_mesh_median_v1";
    const std::vector<double> c = ctrl_of(L);
    SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), std::string("prepare ok: ") + err);
    check(rec.mesh_median_applied() == true, "mesh median filter applied when declared");
    check(rec.operator_id() == std::string("natural_bicubic_spline_clip_mesh_median_v1"),
          "operator id = ..._mesh_median_v1");
    const std::vector<double> cm = oracle_median3(c, L.nx, L.ny);
    bool all = true;
    const double qs[4][2] = {{3.5, 3.5}, {20.0, 12.0}, {11.5, 27.5}, {31.0, 0.0}};
    for (const auto& q : qs) {
      double v = 0.0;
      const bool e2 = rec.eval(q[0], q[1], &v, nullptr, &err);
      const double want =
          oracle_spline_clip(cm, L.nx, L.ny, L.x0, L.y0, L.dx, L.dy, q[0], q[1]);
      all = all && e2 && close(v, want, 1e-11);
    }
    check(all, "mesh-median operator matches independent oracle (median then spline then clip)");
    /* 非退化：4x4 上带/不带滤波必须给出不同的场。 */
    SparseSnrLayer L0 = grid4x4();
    SparseSnrReconstructor rec0;
    std::string e0;
    double v0 = 0.0, v1 = 0.0;
    rec0.prepare(L0, &e0);
    rec0.eval(20.0, 12.0, &v0, nullptr, &e0);
    rec.eval(20.0, 12.0, &v1, nullptr, &err);
    check(std::fabs(v0 - v1) > 1e-6, "mesh filter changes the reconstructed field (non-vacuous)");
  }

  /* ---------- 正例 6: 值域钳制使输出有界且严格为正 ---------- */
  {
    std::printf("[positive] value-range clip keeps output bounded and strictly positive\n");
    /* 病态控制网格：相邻节点大幅交替（光滑插值类在不加钳制时会失控/给出负值）。 */
    SparseSnrLayer L = grid4x4();
    const double patho[16] = {0.05, 9.0, 0.06, 8.0,
                              8.5, 0.07, 7.5, 0.08,
                              0.09, 7.0, 0.10, 6.5,
                              6.0, 0.11, 5.5, 0.12};
    for (std::size_t k = 0; k < L.points.size(); ++k) L.points[k].snr = patho[k];
    SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), std::string("prepare ok: ") + err);
    const double lo = rec.clip_low(), hi = rec.clip_high();
    bool in_range = true, positive = true;
    for (int j = 0; j <= 31; ++j)
      for (int i = 0; i <= 31; ++i) {
        double v = 0.0;
        if (!rec.eval((double)i, (double)j, &v, nullptr, &err)) { in_range = false; continue; }
        if (!(v > 0.0)) positive = false;
        if (v < lo - 1e-12 || v > hi + 1e-12) in_range = false;
      }
    check(positive, "reconstructed field is strictly positive everywhere (no negative sigma)");
    check(in_range, "reconstructed field stays inside the valid control-value range");
    /* 同网格上不加钳制的样条会给出负值 ⇒ 钳制是必需的（不是可选优化）。 */
    double raw_min = 1e300;
    const std::vector<double> c = ctrl_of(L);
    for (int j = 0; j <= 31; ++j)
      for (int i = 0; i <= 31; ++i) {
        const double gx = std::min(std::max(((double)i - L.x0) / L.dx, 0.0), 3.0);
        const double gy = std::min(std::max(((double)j - L.y0) / L.dy, 0.0), 3.0);
        std::vector<double> row(4, 0.0);
        for (int k = 0; k < 4; ++k) {
          std::vector<double> col(4, 0.0);
          for (int jj = 0; jj < 4; ++jj) col[(std::size_t)jj] = c[(std::size_t)jj * 4 + (std::size_t)k];
          const std::vector<double> M = oracle_spline_M(col);
          row[(std::size_t)k] = oracle_spline_eval(col, M, gy);
        }
        const std::vector<double> Mx = oracle_spline_M(row);
        raw_min = std::min(raw_min, oracle_spline_eval(row, Mx, gx));
      }
    check(raw_min < 0.0,
          "unclipped natural spline goes negative on this grid (clip is mandatory)");
    std::printf("        (unclipped min = %.6f, clipped min = %.6f)\n", raw_min,
                rec.clip_low());
  }

  /* ---------- 正例 7: NaN（schema 声明的 invalid）按最近有效控制点填充 ---------- */
  {
    std::printf("[positive] NaN control points filled by nearest valid (declared invalid)\n");
    SparseSnrLayer L = grid4x4();
    L.points[5].snr = std::nan("");
    L.points[6].snr = std::nan("");
    SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), std::string("prepare ok with NaN nodes: ") + err);
    check(rec.n_invalid_control_points_filled() == 2, "fill count reported (2)");
    /* 最近有效邻居在索引空间等距并列时取平均（SExtractor bad-mesh 语义）：
       节点 (1,1) 的三个等距邻居 = (0,1)/(1,0)/(1,2)；节点 (2,1) = (3,1)/(2,0)/(2,2)。 */
    SparseSnrLayer Lf = grid4x4();
    Lf.points[5].snr = (Lf.points[4].snr + Lf.points[1].snr + Lf.points[9].snr) / 3.0;
    Lf.points[6].snr = (Lf.points[7].snr + Lf.points[2].snr + Lf.points[10].snr) / 3.0;
    SparseSnrReconstructor recf;
    std::string ef;
    check(recf.prepare(Lf, &ef), std::string("reference filled layer prepares: ") + ef);
    bool same = true;
    for (int q = 0; q < 8; ++q) {
      double a = 0.0, b = 0.0;
      const double x = 3.5 + 3.0 * q, y = 3.5 + 2.0 * q;
      rec.eval(x, y, &a, nullptr, &err);
      recf.eval(x, y, &b, nullptr, &ef);
      same = same && close(a, b, 1e-12);
    }
    check(same, "filled field equals explicit nearest-valid-filled reference field");
    /* 全无效 ⇒ fail-closed */
    SparseSnrLayer Lall = grid4x4();
    for (auto& p : Lall.points) p.snr = std::nan("");
    SparseSnrReconstructor reca;
    std::string ea;
    check(!reca.prepare(Lall, &ea), "all-NaN layer is fail-closed");
  }

  /* ---------- 正例 8: 预置路径与单次调用逐位一致 + 1/N worker 逐位一致 ---------- */
  {
    std::printf("[positive] prepared path == one-shot call; 1/N worker bitwise identity\n");
    SparseSnrLayer L = grid4x4();
    SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), "prepare ok");
    bool same = true;
    for (int j = 0; j <= 16; ++j)
      for (int i = 0; i <= 16; ++i) {
        double a = 0.0, b = 0.0;
        rec.eval((double)i, (double)j, &a, nullptr, &err);
        astrocs::v6::p2weight::reconstruct_sparse_snr(L, (double)i, (double)j, &b, nullptr, &err);
        same = same && (a == b);   /* 逐位 */
      }
    check(same, "prepared eval and one-shot reconstruct are bitwise identical");
    const int nq = 4096;
    std::vector<double> seq((std::size_t)nq, 0.0), par((std::size_t)nq, 0.0);
    for (int k = 0; k < nq; ++k)
      rec.eval((double)(k % 32), (double)(k / 32), &seq[(std::size_t)k], nullptr, &err);
    const int nthreads = 8;
    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; ++t) {
      th.emplace_back([&, t]() {
        std::string le;
        for (int k = t; k < nq; k += nthreads)
          rec.eval((double)(k % 32), (double)(k / 32), &par[(std::size_t)k], nullptr, &le);
      });
    }
    for (auto& x : th) x.join();
    bool bitwise = true;
    for (int k = 0; k < nq; ++k) bitwise = bitwise && (seq[(std::size_t)k] == par[(std::size_t)k]);
    check(bitwise, "1-thread and 8-thread evaluation are bitwise identical (reentrant)");
  }

  /* ---------- 负例 1: 帧级 SNR 缺失 ---------- */
  {
    std::printf("[negative] missing frame SNR -> fail-closed\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].has_frame_snr = false;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "missing SNR is fail-closed");
    check(r.closure == WeightClosure::kUnclosedMissingFrameSnr, "closure = missing_frame_snr");
    check(r.weights.empty(), "no weights emitted on failure");
    check(r.error.find("missing") != std::string::npos, "explicit reason reported");
  }

  /* ---------- 负例 2: SNR 非有限/非正 ---------- */
  {
    std::printf("[negative] non-finite / non-positive SNR -> fail-closed\n");
    const double fref = 1000.0;
    const double bad[3] = {std::nan(""), INFINITY, -3.0};
    for (double b : bad) {
      std::vector<FrameWeightInput> frames = {mk("f0", b)};
      const WeightChainResult r =
          astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
      check(!r.ok && r.closure == WeightClosure::kUnclosedInvalidFrameSnr,
            "invalid SNR is fail-closed (invalid_frame_snr)");
    }
  }

  /* ---------- 负例 3: F_ref 非法 ---------- */
  {
    std::printf("[negative] invalid F_ref -> fail-closed\n");
    const double bad[3] = {0.0, -1.0, std::nan("")};
    for (double b : bad) {
      std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
      const WeightChainResult r =
          astrocs::v6::p2weight::compute_inverse_variance_weights(frames, b);
      check(!r.ok && r.closure == WeightClosure::kUnclosedInvalidReferenceFlux,
            "invalid F_ref is fail-closed (invalid_reference_flux)");
    }
  }

  /* ---------- 负例 4: 稀疏层存在但损坏/空 ---------- */
  {
    std::printf("[negative] corrupt/empty sparse layer -> fail-closed, no frame fallback\n");
    const double fref = 1000.0;
    SparseSnrLayer empty;
    empty.present = true; /* present but empty */
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].sparse = &empty; frames[0].x = 0.5; frames[0].y = 0.5;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok, "present-but-empty layer is fail-closed");
    check(r.closure == WeightClosure::kUnclosedSparseLayerUnreconstructible,
          "closure = sparse_layer_unreconstructible");
    check(r.weights.empty(), "no silent fallback to frame-level weights");
  }

  /* ---------- 负例 5: 稀疏层越界 ---------- */
  {
    std::printf("[negative] sparse layer out-of-domain -> fail-closed\n");
    const double fref = 1000.0;
    SparseSnrLayer L = grid2x2();
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].sparse = &L; frames[0].x = 5.0; frames[0].y = 0.5;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && r.closure == WeightClosure::kUnclosedSparseLayerUnreconstructible,
          "out-of-domain is fail-closed");
    check(r.error.find("fallback") != std::string::npos,
          "explicitly states frame-level fallback forbidden");
  }

  /* ---------- 负例 6: SNR 语义冒充 ---------- */
  {
    std::printf("[negative] wrong SNR semantics (quality weight) -> fail-closed\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].kind = FrameSnrKind::kRelativeQualityWeight;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && r.closure == WeightClosure::kUnclosedWrongSnrSemantics,
          "relative quality weight rejected");
  }

  /* ---------- 负例 7: legacy_allow_weight_fallback 显式请求仍 fail-closed ---------- */
  {
    std::printf("[negative] legacy_allow_weight_fallback -> explicit unclosed, no false green\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].has_frame_snr = false;
    WeightChainPolicy p;
    p.legacy_allow_weight_fallback = true;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref, p);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "legacy fallback never reports success");
    check(r.closure == WeightClosure::kUnclosedLegacyFallbackRejected,
          "closure = legacy_fallback_rejected");
    check(r.error.find("NOT closed") != std::string::npos ||
              r.error.find("未闭合") != std::string::npos,
          "explicit 'weight chain not closed' message");
    check(r.weights.empty() && r.legacy_equal_weight_used,
          "no scientific weights; diagnostic equal weights flagged only");
  }

  /* ---------- 负例 8: 显式等权基线不得冒充闭合 ---------- */
  {
    std::printf("[negative] explicit equal-weight baseline is not a closure\n");
    const WeightChainResult r = astrocs::v6::p2weight::make_equal_weight_baseline(3);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "baseline never reports a closed chain");
    check(r.closure == WeightClosure::kBaselineEqualWeight, "closure = baseline_equal_weight");
    check(r.weights.size() == 3, "baseline still exposes diagnostic weights");
  }

  /* ---------- 负例 9: 未识别的重建算子 token ---------- */
  {
    std::printf("[negative] unknown reconstruction_operator token -> fail-closed\n");
    SparseSnrLayer L = grid4x4();
    L.reconstruction_operator = "spline_natural_v1";   /* 未钳制的旧名：不得回退 */
    SparseSnrReconstructor rec;
    std::string err;
    check(!rec.prepare(L, &err), "unknown token rejected");
    check(err.find("unknown reconstruction_operator") != std::string::npos,
          "explicit unknown-token reason");
  }

  /* ---------- 负例 10: 几何门——角点锚定（半 cell 相位偏移） ---------- */
  {
    std::printf("[negative] corner-anchored grid (half-cell shift) -> fail-closed\n");
    SparseSnrLayer L = grid4x4();
    L.x0 = 0.0;      /* 角点锚定：应为 grid_origin + (dx-1)/2 = 3.5 */
    L.y0 = 0.0;
    for (std::size_t k = 0; k < L.points.size(); ++k) {
      L.points[k].x = 0.0 + 8.0 * (double)(k % 4);
      L.points[k].y = 0.0 + 8.0 * (double)(k / 4);
    }
    SparseSnrReconstructor rec;
    std::string err;
    check(!rec.prepare(L, &err), "corner-anchored grid rejected");
    check(err.find("cell centers") != std::string::npos, "explicit cell-center reason");
    /* 反例：同一网格改成 cell 中心锚定必须通过（判红来自相位而非网格本身）。 */
    SparseSnrLayer L2 = grid4x4();
    SparseSnrReconstructor rec2;
    std::string e2;
    check(rec2.prepare(L2, &e2), "same grid with cell centers accepted");
    check(rec2.cell_center_offset_max_abs() <= 1e-9, "cell-center offset ~ 0");
  }

  /* ---------- 负例 11: 算子与层形态不匹配 ---------- */
  {
    std::printf("[negative] operator/layer-shape mismatch -> fail-closed\n");
    SparseSnrLayer L = grid4x4();
    L.reconstruction_operator = "nearest_control_point_v1";
    SparseSnrReconstructor r1;
    std::string e1;
    check(!r1.prepare(L, &e1), "nearest_control_point_v1 on a regular grid rejected");
    SparseSnrLayer S;
    S.present = true;
    S.regular_grid = false;
    S.max_radius_px = 4.0;
    S.points = {{0.0, 0.0, 2.0}, {5.0, 5.0, 3.0}};
    S.reconstruction_operator = "natural_bicubic_spline_clip_v1";
    SparseSnrReconstructor r2;
    std::string e2;
    check(!r2.prepare(S, &e2), "regular-grid operator on a scattered layer rejected");
    S.reconstruction_operator.clear();
    SparseSnrReconstructor r3;
    std::string e3;
    check(r3.prepare(S, &e3), "scattered layer default = nearest_control_point_v1");
    check(r3.operator_id() == std::string("nearest_control_point_v1"),
          "scattered default operator id");
  }

  /* ---------- 负例 12: 非法控制值（0 / 负 / +inf）与全 NaN ---------- */
  {
    std::printf("[negative] illegal control values and all-NaN layer -> fail-closed\n");
    const double bad[3] = {0.0, -1.0, INFINITY};
    for (double b : bad) {
      SparseSnrLayer L = grid4x4();
      L.points[7].snr = b;
      SparseSnrReconstructor rec;
      std::string err;
      check(!rec.prepare(L, &err), "illegal control value rejected");
    }
  }

  /* ---------- 负例 13: 定义域边界（cell 并集之外） ---------- */
  {
    std::printf("[negative] outside the cell-union domain -> fail-closed\n");
    SparseSnrLayer L = grid4x4();   /* cell 并集 = [-0.5, 31.5]（x/y 同） */
    SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), "prepare ok");
    double v = 0.0;
    SparseReconstruction info;
    check(rec.eval(-0.4, 10.0, &v, &info, &err), "just inside the domain is accepted");
    check(!rec.eval(-0.6, 10.0, &v, &info, &err) && info.out_of_domain,
          "just outside the domain is fail-closed with out_of_domain=true");
    check(!rec.eval(31.6, 10.0, &v, &info, &err), "upper edge outside is fail-closed");
  }

  /* ---------- 非空真检查: 等权 != 逆方差权重 ---------- */
  {
    std::printf("[non-vacuity] equal weight differs from inverse-variance weight\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0), mk("f1", 50.0)};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    bool differs = false;
    for (double w : r.weights) differs = differs || !close(w, 1.0, 1e-9);
    check(differs, "inverse-variance weights are not all 1.0 (test is non-vacuous)");
  }

  /* ---------- 非空真检查: 算子词表与滤波/钳制绑定自洽 ---------- */
  {
    std::printf("[non-vacuity] operator token table is self-consistent\n");
    SparseReconOperator op;
    check(astrocs::v6::p2weight::parse_sparse_recon_operator(
              "natural_bicubic_spline_clip_v1", &op) &&
              !astrocs::v6::p2weight::sparse_recon_operator_uses_mesh_median(op) &&
              astrocs::v6::p2weight::sparse_recon_operator_clips_to_ctrl_range(op),
          "default token: clip ON, mesh median OFF");
    check(astrocs::v6::p2weight::parse_sparse_recon_operator(
              "natural_bicubic_spline_clip_mesh_median_v1", &op) &&
              astrocs::v6::p2weight::sparse_recon_operator_uses_mesh_median(op) &&
              astrocs::v6::p2weight::sparse_recon_operator_clips_to_ctrl_range(op),
          "mesh-median token: clip ON, mesh median ON");
    check(astrocs::v6::p2weight::parse_sparse_recon_operator(
              "bilinear_regular_grid_v1", &op) &&
              !astrocs::v6::p2weight::sparse_recon_operator_uses_mesh_median(op) &&
              !astrocs::v6::p2weight::sparse_recon_operator_clips_to_ctrl_range(op),
          "bilinear token: clip OFF, mesh median OFF (legacy behaviour preserved)");
    check(std::string(astrocs::v6::p2weight::sparse_recon_operator_default_token()) ==
              std::string("natural_bicubic_spline_clip_v1"),
          "frozen default token = natural_bicubic_spline_clip_v1");
    check(!astrocs::v6::p2weight::parse_sparse_recon_operator("nn", &op),
          "unknown token is not parsed");
  }

  std::printf("\n== summary: %d passed, %d failed ==\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
