// eng/tests/unit/v6_p3_rsmp/p3_rsmp_scenarios.h
// 共享合成场景（生产类型 + 独立 Oracle 输入同源）。1D 链：
//   4 输入像素（Omega_in=1）→ 2 输出像素（Omega_out=2，各覆盖 2 个输入，重叠面积 1.0）
//   R_ij = 0.5（行和 1）；S_ij = 1.0（列和 1）；S_ij = R_ij*Omega'_i/Omega_j。
#ifndef P3_RSMP_SCENARIOS_H
#define P3_RSMP_SCENARIOS_H

#include <cmath>
#include <vector>

#include "p3_rsmp.h"
#include "p3_rsmp_oracle.h"

namespace p3scen {

struct Chain {
  int n_in = 4;
  int n_out = 2;
  std::vector<double> omega_in;
  std::vector<double> omega_out;
  std::vector<double> x;    // SB 输入
  std::vector<double> psf;  // 输入 effective PSF，Σ=1
  astrocs::p3rsmp::Geometry geom;
  std::vector<astrocs::p3rsmp::OverlapEntry> entries;
  std::vector<p3oracle::Ov> ov;
  astrocs::p3rsmp::DenseMatrix cx;
};

inline Chain make_chain(double rho = 0.19) {
  Chain c;
  c.omega_in = {1.0, 1.0, 1.0, 1.0};
  c.omega_out = {2.0, 2.0};
  c.x = {1.5, 2.0, 2.5, 3.0};
  c.psf = {0.4, 0.3, 0.2, 0.1};
  c.geom.omega_in_sr = c.omega_in;
  c.geom.omega_out_sr = c.omega_out;
  const int pairs[2][2] = {{0, 1}, {2, 3}};
  for (int o = 0; o < c.n_out; ++o) {
    for (int k = 0; k < 2; ++k) {
      astrocs::p3rsmp::OverlapEntry e;
      e.out_index = o;
      e.in_index = pairs[o][k];
      e.overlap_sr = 1.0;
      c.entries.push_back(e);
      p3oracle::Ov q;
      q.o = o;
      q.i = pairs[o][k];
      q.area = 1.0;
      c.ov.push_back(q);
    }
  }
  c.cx = astrocs::p3rsmp::DenseMatrix(c.n_in, c.n_in);
  for (int i = 0; i < c.n_in; ++i) {
    for (int j = 0; j < c.n_in; ++j) {
      c.cx(i, j) = std::pow(rho, std::abs(i - j));
    }
  }
  return c;
}

inline astrocs::p3rsmp::SparseOperator chain_R(const Chain& c) {
  return astrocs::p3rsmp::operator_from_entries_row(c.entries, c.geom, c.n_out, c.n_in,
                                                    "bilinear_area_overlap_exact");
}
inline astrocs::p3rsmp::SparseOperator chain_S(const Chain& c) {
  return astrocs::p3rsmp::operator_from_entries_col(c.entries, c.geom, c.n_out, c.n_in,
                                                    "bilinear_area_overlap_exact");
}

}  // namespace p3scen

#endif  // P3_RSMP_SCENARIOS_H
