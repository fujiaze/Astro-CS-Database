// lib/phase3_rsmp/p3_rsmp_operator.cpp
// 统一线性模型：R（行归一）/ S（列归一）算子与跨 tile 邻域（ALG-P3-002/003）。
#include "p3_rsmp.h"

#include <algorithm>
#include <cmath>

namespace astrocs {
namespace p3rsmp {

bool TileMask::present_at(int ix, int iy) const {
  if (ix < 0 || iy < 0 || ix >= tiles_x || iy >= tiles_y) return false;
  const std::size_t idx = static_cast<std::size_t>(iy) * tiles_x + ix;
  if (idx >= present.size()) return false;
  return present[idx] != 0;
}

double SparseOperator::at(int o, int i) const {
  for (std::size_t e = 0; e < weight.size(); ++e) {
    if (out_idx[e] == o && in_idx[e] == i) return weight[e];
  }
  return 0.0;
}

std::vector<double> SparseOperator::row_sums() const {
  std::vector<double> s(static_cast<std::size_t>(n_out), 0.0);
  for (std::size_t e = 0; e < weight.size(); ++e) {
    s[static_cast<std::size_t>(out_idx[e])] += weight[e];
  }
  return s;
}

std::vector<double> SparseOperator::col_sums() const {
  std::vector<double> s(static_cast<std::size_t>(n_in), 0.0);
  for (std::size_t e = 0; e < weight.size(); ++e) {
    s[static_cast<std::size_t>(in_idx[e])] += weight[e];
  }
  return s;
}

bool SparseOperator::full_coverage(double tol) const {
  for (std::size_t i = 0; i < valid_out.size(); ++i) {
    if (!valid_out[i]) return false;
    if (std::fabs(coverage[i] - 1.0) > tol) return false;
  }
  return true;
}

namespace {
SparseOperator from_entries(const std::vector<OverlapEntry>& e, const Geometry& g, int n_out,
                            int n_in, const std::string& kernel_id, bool row) {
  SparseOperator op;
  op.n_out = n_out;
  op.n_in = n_in;
  op.kernel_id = kernel_id;
  op.row_normalized = row;
  op.column_normalized = !row;
  op.valid_out.assign(static_cast<std::size_t>(n_out), false);
  op.coverage.assign(static_cast<std::size_t>(n_out), 0.0);
  std::vector<double> area_sum(static_cast<std::size_t>(n_out), 0.0);
  for (const OverlapEntry& en : e) {
    if (en.out_index < 0 || en.out_index >= n_out) continue;
    if (en.in_index < 0 || en.in_index >= n_in) continue;
    if (!(en.overlap_sr > 0.0)) continue;  // 非正重叠不贡献（不零填）
    double w = 0.0;
    if (row) {
      const double oo = g.omega_out_sr.empty() ? kNaN
                                               : g.omega_out_sr[static_cast<std::size_t>(en.out_index)];
      if (!(oo > 0.0)) continue;
      w = en.overlap_sr / oo;
    } else {
      const double oi = g.omega_in_sr.empty() ? kNaN
                                              : g.omega_in_sr[static_cast<std::size_t>(en.in_index)];
      if (!(oi > 0.0)) continue;
      w = en.overlap_sr / oi;
    }
    op.out_idx.push_back(en.out_index);
    op.in_idx.push_back(en.in_index);
    op.weight.push_back(w);
    area_sum[static_cast<std::size_t>(en.out_index)] += en.overlap_sr;
    op.valid_out[static_cast<std::size_t>(en.out_index)] = true;
  }
  for (int i = 0; i < n_out; ++i) {
    const double oo = g.omega_out_sr.empty() ? kNaN : g.omega_out_sr[static_cast<std::size_t>(i)];
    op.coverage[static_cast<std::size_t>(i)] =
        (oo > 0.0) ? (area_sum[static_cast<std::size_t>(i)] / oo) : 0.0;
  }
  return op;
}
}  // namespace

SparseOperator operator_from_entries_row(const std::vector<OverlapEntry>& e, const Geometry& g,
                                         int n_out, int n_in, const std::string& kernel_id) {
  return from_entries(e, g, n_out, n_in, kernel_id, true);
}
SparseOperator operator_from_entries_col(const std::vector<OverlapEntry>& e, const Geometry& g,
                                         int n_out, int n_in, const std::string& kernel_id) {
  return from_entries(e, g, n_out, n_in, kernel_id, false);
}

SparseOperator build_row_normalized(const NeighborhoodSet& nb, const std::string& kernel_id) {
  const int n_out = static_cast<int>(nb.geom.omega_out_sr.size());
  const int n_in = static_cast<int>(nb.geom.omega_in_sr.size());
  SparseOperator op;
  op.n_out = n_out;
  op.n_in = n_in;
  op.kernel_id = kernel_id;
  op.row_normalized = true;
  op.column_normalized = false;
  op.valid_out.assign(static_cast<std::size_t>(n_out), false);
  op.coverage.assign(static_cast<std::size_t>(n_out), 0.0);
  for (const OutputNeighborhood& n : nb.outputs) {
    if (n.out_index < 0 || n.out_index >= n_out) continue;
    // 缺 tile/越界：fail-closed，输出 NaN，且**不零填**（C-P3-PROP-6）
    if (n.missing_contributors > 0) continue;
    if (n.in_index.empty()) continue;
    if (!(n.omega_out_sr > 0.0)) continue;
    double area = 0.0;
    for (double a : n.overlap_sr) {
      if (a > 0.0) area += a;
    }
    if (!(area > 0.0)) continue;
    for (std::size_t k = 0; k < n.in_index.size(); ++k) {
      const int j = n.in_index[k];
      if (j < 0 || j >= n_in) continue;
      const double a = n.overlap_sr[k];
      if (!(a > 0.0)) continue;
      op.out_idx.push_back(n.out_index);
      op.in_idx.push_back(j);
      op.weight.push_back(a / n.omega_out_sr);  // R_ij = a_ij / Omega'_i
    }
    op.valid_out[static_cast<std::size_t>(n.out_index)] = true;
    op.coverage[static_cast<std::size_t>(n.out_index)] = area / n.omega_out_sr;
  }
  return op;
}

SparseOperator build_column_normalized(const NeighborhoodSet& nb, const std::string& kernel_id) {
  const int n_out = static_cast<int>(nb.geom.omega_out_sr.size());
  const int n_in = static_cast<int>(nb.geom.omega_in_sr.size());
  SparseOperator op;
  op.n_out = n_out;
  op.n_in = n_in;
  op.kernel_id = kernel_id;
  op.row_normalized = false;
  op.column_normalized = true;
  op.valid_out.assign(static_cast<std::size_t>(n_out), false);
  op.coverage.assign(static_cast<std::size_t>(n_out), 0.0);
  for (const OutputNeighborhood& n : nb.outputs) {
    if (n.out_index < 0 || n.out_index >= n_out) continue;
    if (n.missing_contributors > 0) continue;
    if (n.in_index.empty()) continue;
    if (!(n.omega_out_sr > 0.0)) continue;
    double area = 0.0;
    for (double a : n.overlap_sr) {
      if (a > 0.0) area += a;
    }
    if (!(area > 0.0)) continue;
    for (std::size_t k = 0; k < n.in_index.size(); ++k) {
      const int j = n.in_index[k];
      if (j < 0 || j >= n_in) continue;
      const double a = n.overlap_sr[k];
      if (!(a > 0.0)) continue;
      const double oi = nb.geom.omega_in_sr[static_cast<std::size_t>(j)];
      if (!(oi > 0.0)) continue;
      op.out_idx.push_back(n.out_index);
      op.in_idx.push_back(j);
      op.weight.push_back(a / oi);  // S_ij = a_ij / Omega_j
    }
    op.valid_out[static_cast<std::size_t>(n.out_index)] = true;
    op.coverage[static_cast<std::size_t>(n.out_index)] = area / n.omega_out_sr;
  }
  return op;
}

std::vector<double> apply_operator(const SparseOperator& op, const std::vector<double>& x) {
  std::vector<double> y(static_cast<std::size_t>(op.n_out), 0.0);
  for (std::size_t e = 0; e < op.weight.size(); ++e) {
    const int i = op.in_idx[e];
    if (i < 0 || static_cast<std::size_t>(i) >= x.size()) continue;
    y[static_cast<std::size_t>(op.out_idx[e])] += op.weight[e] * x[static_cast<std::size_t>(i)];
  }
  for (int o = 0; o < op.n_out; ++o) {
    if (!op.valid_out[static_cast<std::size_t>(o)]) y[static_cast<std::size_t>(o)] = kNaN;
  }
  return y;
}

std::vector<double> apply_operator_transpose(const SparseOperator& op,
                                             const std::vector<double>& v) {
  std::vector<double> y(static_cast<std::size_t>(op.n_in), 0.0);
  for (std::size_t e = 0; e < op.weight.size(); ++e) {
    const int o = op.out_idx[e];
    if (!op.valid_out[static_cast<std::size_t>(o)]) continue;  // 无效输出不贡献
    y[static_cast<std::size_t>(op.in_idx[e])] += op.weight[e] * v[static_cast<std::size_t>(o)];
  }
  return y;
}

// ---------------------------------------------------------------------------
// 2D 邻域生成（跨 tile）
// ---------------------------------------------------------------------------
namespace {
struct Sample {
  int ix = 0;
  int iy = 0;
  double w = 0.0;
};

void accumulate(std::vector<Sample>* samples, const InputGrid2D& in, const TileMask& tiles,
                int ix, int iy, double w, int* missing) {
  if (ix < 0 || iy < 0 || ix >= in.width || iy >= in.height) {
    ++(*missing);
    return;
  }
  const int tx = ix / tiles.tile_px;
  const int ty = iy / tiles.tile_px;
  if (!tiles.present_at(tx, ty)) {
    ++(*missing);
    return;
  }
  samples->push_back(Sample{ix, iy, w});
}
}  // namespace

NeighborhoodSet make_bilinear_4quad_neighborhood(const InputGrid2D& in, const TileMask& tiles,
                                                 const GridPlan& plan, double omega_in_sr,
                                                 double omega_out_sr) {
  NeighborhoodSet nb;
  nb.geom.omega_in_sr.assign(static_cast<std::size_t>(in.width) * in.height, omega_in_sr);
  nb.geom.omega_out_sr.assign(static_cast<std::size_t>(plan.out_width) * plan.out_height,
                              omega_out_sr);
  for (int oy = 0; oy < plan.out_height; ++oy) {
    for (int ox = 0; ox < plan.out_width; ++ox) {
      OutputNeighborhood n;
      n.out_index = oy * plan.out_width + ox;
      n.omega_out_sr = omega_out_sr;
      const double cx = plan.out_origin_x + ox * plan.out_step;
      const double cy = plan.out_origin_y + oy * plan.out_step;
      const double u = cx - 0.5;
      const double v = cy - 0.5;
      const int j0 = static_cast<int>(std::floor(u));
      const int k0 = static_cast<int>(std::floor(v));
      const double fx = u - j0;
      const double fy = v - k0;
      const double w00 = (1.0 - fx) * (1.0 - fy);
      const double w10 = fx * (1.0 - fy);
      const double w01 = (1.0 - fx) * fy;
      const double w11 = fx * fy;
      std::vector<Sample> s;
      accumulate(&s, in, tiles, j0, k0, w00, &n.missing_contributors);
      accumulate(&s, in, tiles, j0 + 1, k0, w10, &n.missing_contributors);
      accumulate(&s, in, tiles, j0, k0 + 1, w01, &n.missing_contributors);
      accumulate(&s, in, tiles, j0 + 1, k0 + 1, w11, &n.missing_contributors);
      for (const Sample& sm : s) {
        n.in_index.push_back(sm.iy * in.width + sm.ix);
        n.overlap_sr.push_back(sm.w * omega_out_sr);  // R_ij = w_ij，Σw=1
      }
      nb.outputs.push_back(n);
    }
  }
  return nb;
}

NeighborhoodSet make_nearest_neighborhood(const InputGrid2D& in, const TileMask& tiles,
                                          const GridPlan& plan, double omega_in_sr,
                                          double omega_out_sr) {
  NeighborhoodSet nb;
  nb.geom.omega_in_sr.assign(static_cast<std::size_t>(in.width) * in.height, omega_in_sr);
  nb.geom.omega_out_sr.assign(static_cast<std::size_t>(plan.out_width) * plan.out_height,
                              omega_out_sr);
  for (int oy = 0; oy < plan.out_height; ++oy) {
    for (int ox = 0; ox < plan.out_width; ++ox) {
      OutputNeighborhood n;
      n.out_index = oy * plan.out_width + ox;
      n.omega_out_sr = omega_out_sr;
      const double cx = plan.out_origin_x + ox * plan.out_step;
      const double cy = plan.out_origin_y + oy * plan.out_step;
      const int ix = static_cast<int>(std::floor(cx));
      const int iy = static_cast<int>(std::floor(cy));
      std::vector<Sample> s;
      accumulate(&s, in, tiles, ix, iy, 1.0, &n.missing_contributors);
      for (const Sample& sm : s) {
        n.in_index.push_back(sm.iy * in.width + sm.ix);
        n.overlap_sr.push_back(sm.w * omega_out_sr);
      }
      nb.outputs.push_back(n);
    }
  }
  return nb;
}

}  // namespace p3rsmp
}  // namespace astrocs
