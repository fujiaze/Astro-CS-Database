// eng/tests/unit/v6_p3_rsmp/p3_rsmp_core_test.cpp
// IMPL-P3-RSMP-001 正向科学测试：R 行归一 / S 列归一 / C_y=R C_x Rᵀ / π=S p / Q/W 输出帧重算 /
// BUNIT 二次律 / 跨 tile 边界 fail-closed。真值来自独立 Oracle（p3_rsmp_oracle.h）。
#include <cmath>
#include <cstdio>
#include <vector>

#include "p3_rsmp.h"
#include "p3_rsmp_oracle.h"
#include "p3_rsmp_scenarios.h"
#include "p3_rsmp_test_util.h"

using astrocs::p3rsmp::apply_operator;
using astrocs::p3rsmp::DenseMatrix;
using astrocs::p3rsmp::GateConfig;
using astrocs::p3rsmp::Geometry;
using astrocs::p3rsmp::InputGrid2D;
using astrocs::p3rsmp::GateResult;
using astrocs::p3rsmp::NeighborhoodSet;
using astrocs::p3rsmp::PointSourceInput;
using astrocs::p3rsmp::SparseOperator;
using astrocs::p3rsmp::Status;
using astrocs::p3rsmp::SurfaceBrightnessInput;
using astrocs::p3rsmp::TileMask;

namespace {

p3oracle::Mat to_oracle(const DenseMatrix& m) {
  p3oracle::Mat r(m.rows, m.cols);
  for (int i = 0; i < m.rows; ++i)
    for (int j = 0; j < m.cols; ++j) r(i, j) = m(i, j);
  return r;
}

void test_row_col_normalization() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  const SparseOperator S = p3scen::chain_S(c);
  const std::vector<double> rs = R.row_sums();
  const std::vector<double> cs = S.col_sums();
  P3_CHECK(R.row_normalized && !R.column_normalized);
  P3_CHECK(!S.row_normalized && S.column_normalized);
  for (double v : rs) P3_CHECK_NEAR(v, 1.0, 1e-14);
  for (double v : cs) P3_CHECK_NEAR(v, 1.0, 1e-14);
  P3_CHECK(R.full_coverage(1e-14));

  // 非单位立体角（Omega_in=Omega_out=2，重叠面积=2）：行/列归一都必须除以各自面积元，
  // 而非直接用原始重叠面积；R=S=1（原始重叠=2 会暴露漏归一）。
  Geometry g2;
  g2.omega_in_sr = {2.0};
  g2.omega_out_sr = {2.0};
  std::vector<astrocs::p3rsmp::OverlapEntry> e2;
  {
    astrocs::p3rsmp::OverlapEntry a;
    a.out_index = 0;
    a.in_index = 0;
    a.overlap_sr = 2.0;
    e2.push_back(a);
  }
  const SparseOperator R2 =
      astrocs::p3rsmp::operator_from_entries_row(e2, g2, 1, 1, "bilinear_area_overlap_exact");
  const SparseOperator S2 =
      astrocs::p3rsmp::operator_from_entries_col(e2, g2, 1, 1, "bilinear_area_overlap_exact");
  for (double v : R2.row_sums()) P3_CHECK_NEAR(v, 1.0, 1e-15);
  for (double v : S2.col_sums()) P3_CHECK_NEAR(v, 1.0, 1e-15);
  P3_CHECK_NEAR(R2.at(0, 0), 1.0, 1e-15);  // 2.0/Omega'_i=2
  P3_CHECK_NEAR(S2.at(0, 0), 1.0, 1e-15);  // 2.0/Omega_j=2（而非原始重叠 2.0）
  P3_CHECK_NEAR(S2.at(0, 0), R2.at(0, 0) * g2.omega_out_sr[0] / g2.omega_in_sr[0], 1e-15);
}

void test_R_S_identity() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  const SparseOperator S = p3scen::chain_S(c);
  // S_ij = R_ij * Omega'_i / Omega_j
  for (int o = 0; o < c.n_out; ++o) {
    for (int i = 0; i < c.n_in; ++i) {
      const double expect = R.at(o, i) * c.omega_out[static_cast<std::size_t>(o)] /
                            c.omega_in[static_cast<std::size_t>(i)];
      P3_CHECK_NEAR(S.at(o, i), expect, 1e-15);
    }
  }
  P3_CHECK_NEAR(R.at(0, 0), 0.5, 1e-15);
  P3_CHECK_NEAR(S.at(0, 0), 1.0, 1e-15);
}

void test_constant_sb_field() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  const double B0 = 3.7;
  std::vector<double> xb(static_cast<std::size_t>(c.n_in), B0);
  const std::vector<double> y = apply_operator(R, xb);
  for (double v : y) P3_CHECK_NEAR(v, B0, 1e-14);
  // Oracle 交叉：常量场不变量
  const p3oracle::Mat Rm = p3oracle::build_R(c.ov, c.omega_out, c.n_out, c.n_in);
  const std::vector<double> yo = p3oracle::matvec(Rm, xb);
  for (int i = 0; i < c.n_out; ++i) {
    P3_CHECK_NEAR(y[static_cast<std::size_t>(i)], yo[static_cast<std::size_t>(i)], 1e-14);
  }
}

void test_flux_conservation() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator S = p3scen::chain_S(c);
  std::vector<double> d(static_cast<std::size_t>(c.n_in), 0.0);
  for (int j = 0; j < c.n_in; ++j) {
    d[static_cast<std::size_t>(j)] = c.x[static_cast<std::size_t>(j)] * c.omega_in[static_cast<std::size_t>(j)];
  }
  const std::vector<double> f = apply_operator(S, d);
  double sf = 0.0;
  double sd = 0.0;
  for (double v : f) sf += v;
  for (double v : d) sd += v;
  P3_CHECK_NEAR(sf, sd, 1e-13);  // 列归一 ⇒ 点源总通量守恒
}

void test_covariance_vs_oracle() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  const DenseMatrix cy = astrocs::p3rsmp::propagate_covariance(R, c.cx);
  const p3oracle::Mat Rm = p3oracle::build_R(c.ov, c.omega_out, c.n_out, c.n_in);
  const p3oracle::Mat cyo = p3oracle::covariance(Rm, to_oracle(c.cx));
  P3_CHECK(cy.rows == c.n_out && cy.cols == c.n_out);
  for (int i = 0; i < c.n_out; ++i) {
    for (int j = 0; j < c.n_out; ++j) {
      P3_CHECK_NEAR(cy(i, j), cyo(i, j), 1e-13);
    }
  }
}

void test_diagonal_input_variance() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  std::vector<double> v = {0.04, 0.09, 0.16, 0.25};
  const DenseMatrix cx = DenseMatrix::diagonal(v);
  const DenseMatrix cy = astrocs::p3rsmp::propagate_covariance(R, cx);
  for (int o = 0; o < c.n_out; ++o) {
    double expect = 0.0;  // Σ_j R_oj² v_j（对角输入时严格）
    for (int i = 0; i < c.n_in; ++i) {
      expect += R.at(o, i) * R.at(o, i) * v[static_cast<std::size_t>(i)];
    }
    P3_CHECK_NEAR(cy(o, o), expect, 1e-15);
  }
}

void test_correlation_deficit_detected() {
  const p3scen::Chain c = p3scen::make_chain(0.19);
  const SparseOperator R = p3scen::chain_R(c);
  const DenseMatrix cy = astrocs::p3rsmp::propagate_covariance(R, c.cx);
  // 对角省略公式 Σ c²u 系统性低估（F3-02 / C-P3-PROP-8）
  for (int o = 0; o < c.n_out; ++o) {
    double naive = 0.0;
    for (int i = 0; i < c.n_in; ++i) {
      naive += R.at(o, i) * R.at(o, i) * c.cx(i, i);
    }
    const double deficit = 1.0 - naive / cy(o, o);
    P3_CHECK(deficit > 0.10);  // 相关核必须显式给出，否则 α 面 fail-closed
  }
}

void test_effective_psf() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator S = p3scen::chain_S(c);
  const std::vector<double> pi = apply_operator(S, c.psf);
  double s = 0.0;
  for (double v : pi) s += v;
  P3_CHECK_NEAR(s, 1.0, 5.1e-12);  // Σπ=1（ALG Oracle 实测 0.9999999999949）
  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  const std::vector<double> pio = p3oracle::matvec(Sm, c.psf);
  for (int i = 0; i < c.n_out; ++i) {
    P3_CHECK_NEAR(pi[static_cast<std::size_t>(i)], pio[static_cast<std::size_t>(i)], 1e-15);
  }
  P3_CHECK_NEAR(pi[0], 0.7, 1e-15);
  P3_CHECK_NEAR(pi[1], 0.3, 1e-15);
  P3_CHECK(pi[1] > 0.2);  // 非单像素 δ
}

PointSourceInput make_psf_input(const p3scen::Chain& c, const SparseOperator& S, double a) {
  PointSourceInput in;
  in.s_op = S;
  in.x = c.x;
  in.c_x = c.cx;
  in.geom = c.geom;
  in.x_is_surface_brightness = true;
  in.psf_p = c.psf;
  in.psf_present = true;
  in.point_information_present = true;
  in.photometric_scale_present = true;
  in.a = a;
  in.effective_psf_present = true;
  in.effective_psf_normalization_declared = true;
  return in;
}

void test_qw_output_frame_vs_oracle() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator S = p3scen::chain_S(c);
  const double a = 2.0;
  const auto res = astrocs::p3rsmp::propagate_point_source_flux(make_psf_input(c, S, a), GateConfig{});
  P3_CHECK(res.status == Status::Ok);
  P3_CHECK(res.frame_is_output_recompute);

  // Oracle：独立稠密 C_y 与高斯消元解
  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  std::vector<double> d(static_cast<std::size_t>(c.n_in), 0.0);
  for (int j = 0; j < c.n_in; ++j) {
    d[static_cast<std::size_t>(j)] = c.x[static_cast<std::size_t>(j)] * c.omega_in[static_cast<std::size_t>(j)];
  }
  const std::vector<double> f_or = p3oracle::matvec(Sm, d);
  DenseMatrix cd(c.n_in, c.n_in);
  for (int i = 0; i < c.n_in; ++i)
    for (int j = 0; j < c.n_in; ++j)
      cd(i, j) = c.omega_in[static_cast<std::size_t>(i)] * c.cx(i, j) *
                 c.omega_in[static_cast<std::size_t>(j)];
  const p3oracle::Mat cyo = p3oracle::covariance(Sm, to_oracle(cd));
  const std::vector<double> pio = p3oracle::matvec(Sm, c.psf);
  std::vector<double> q;
  P3_CHECK(p3oracle::solve(cyo, pio, &q));
  const double W_or = a * a * p3oracle::dot(q, pio);
  const double Q_or = a * p3oracle::dot(q, f_or);
  for (int i = 0; i < c.n_out; ++i) {
    P3_CHECK_NEAR(res.f[static_cast<std::size_t>(i)], f_or[static_cast<std::size_t>(i)], 1e-14);
    P3_CHECK_NEAR(res.pi[static_cast<std::size_t>(i)], pio[static_cast<std::size_t>(i)], 1e-15);
  }
  P3_CHECK_NEAR(res.W, W_or, 1e-12);
  P3_CHECK_NEAR(res.Q, Q_or, 1e-12);
  P3_CHECK_NEAR(res.F_hat, res.Q / res.W, 1e-15);
  P3_CHECK_NEAR(res.var_F_hat, 1.0 / res.W, 1e-15);
}

void test_qw_vs_monte_carlo() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator S = p3scen::chain_S(c);
  const double a = 2.0;
  const auto res = astrocs::p3rsmp::propagate_point_source_flux(make_psf_input(c, S, a), GateConfig{});
  P3_CHECK(res.status == Status::Ok);
  DenseMatrix cd(c.n_in, c.n_in);
  for (int i = 0; i < c.n_in; ++i)
    for (int j = 0; j < c.n_in; ++j) cd(i, j) = c.cx(i, j);  // omega=1
  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  const std::vector<double> pio = p3oracle::matvec(Sm, c.psf);  // Oracle 自算 π
  double rel = 0.0;
  const double var_mc = p3oracle::mc_var_Fhat(Sm, pio, to_oracle(cd), a, 200000, nullptr, &rel);
  P3_CHECK(std::isfinite(var_mc));
  P3_CHECK(rel < 0.01);  // 固定种子 MC（SEED=20260915）与 1/W 一致
  P3_CHECK_NEAR(res.var_F_hat, var_mc, 0.05 * var_mc);
}

void test_qw_non_commutation() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator S = p3scen::chain_S(c);
  const SparseOperator R = p3scen::chain_R(c);
  const auto res = astrocs::p3rsmp::propagate_point_source_flux(make_psf_input(c, S, 2.0), GateConfig{});
  P3_CHECK(res.status == Status::Ok);
  // W_naive = Σ_i (行归一核) W_in,i（禁路径），与输出帧重算不可交换
  double w_naive = 0.0;
  for (int o = 0; o < c.n_out; ++o) {
    for (int i = 0; i < c.n_in; ++i) {
      const double w_in = c.psf[static_cast<std::size_t>(i)] * c.psf[static_cast<std::size_t>(i)];
      w_naive += R.at(o, i) * w_in;
    }
  }
  const double rel = std::fabs(w_naive - res.W) / res.W;
  P3_CHECK(rel > 0.2);  // ALG Oracle 实测 91.76%；不同场景仍必须显著不可交换
}

void test_sb_propagation_positive() {
  const p3scen::Chain c = p3scen::make_chain();
  const SparseOperator R = p3scen::chain_R(c);
  SurfaceBrightnessInput in;
  in.x = c.x;
  in.c_in = c.cx;
  in.measurement_capable = true;
  in.uncertainty_available = true;
  in.covariance = astrocs::p3rsmp::CovarianceRepresentation::ExactFull;
  const auto res = astrocs::p3rsmp::propagate_surface_brightness(R, in, GateConfig{});
  P3_CHECK(res.status == Status::Ok);
  P3_CHECK(res.valid.size() == static_cast<std::size_t>(c.n_out));
  for (std::size_t i = 0; i < res.valid.size(); ++i) P3_CHECK(res.valid[i]);
  const DenseMatrix cy = astrocs::p3rsmp::propagate_covariance(R, c.cx);
  for (int i = 0; i < c.n_out; ++i) {
    P3_CHECK_NEAR(res.variance[static_cast<std::size_t>(i)], cy(i, i), 1e-15);
    double expect = 0.0;
    for (int j = 0; j < c.n_in; ++j) expect += R.at(i, j) * c.x[static_cast<std::size_t>(j)];
    P3_CHECK_NEAR(res.y[static_cast<std::size_t>(i)], expect, 1e-14);
  }
}

void test_bunit_quadratic() {
  using astrocs::p3rsmp::is_inverse_pair;
  using astrocs::p3rsmp::is_quadratic_variance;
  using astrocs::p3rsmp::units::sb_ivar_out;
  using astrocs::p3rsmp::units::sb_variance_out;
  using astrocs::p3rsmp::units::signal_sb;
  P3_CHECK(is_quadratic_variance(signal_sb, sb_variance_out));
  P3_CHECK(is_inverse_pair(sb_variance_out, sb_ivar_out));
  // canonical **产品 BUNIT 串** = 冻结单位表逐字串（docs/contracts/DATA_SEMANTICS.md
  // §31.1 单位表 + §31.1a「单位串与内部幂次编码的对应」: pixel_area_power = -2 ⇔ 串含
  // "/sr"、-4 ⇔ "/sr^2"、+4 ⇔ "sr^2/…"；**canonical 产品串一律写 sr**，写侧只出 sr）。
  P3_CHECK(signal_sb.canonical() == "ADU/sr");
  P3_CHECK(sb_variance_out.canonical() == "ADU^2/sr^2");
  P3_CHECK(sb_ivar_out.canonical() == "sr^2/ADU^2");
  // 读侧/写侧同一口径: canonical 串必须解析回同一内部幂次（§31.1a 同一映射的逆）。
  {
    using astrocs::p3rsmp::BunitProvenance;
    using astrocs::p3rsmp::resolve_bunit;
    const BunitProvenance no_prov{};
    const auto rs = resolve_bunit(signal_sb.canonical(), no_prov);
    P3_CHECK(rs.resolvable && rs.resolved == signal_sb);
    const auto rv = resolve_bunit(sb_variance_out.canonical(), no_prov);
    P3_CHECK(rv.resolvable && rv.resolved == sb_variance_out);
    const auto ri = resolve_bunit(sb_ivar_out.canonical(), no_prov);
    P3_CHECK(ri.resolvable && ri.resolved == sb_ivar_out);
    // legacy 旧串 px 幂次映射到同一立体角维（§31.1a「读侧兼容旧串 px / pixel」）。
    const auto rl = resolve_bunit("ADU/px^2", no_prov);
    P3_CHECK(rl.resolvable && rl.resolved == signal_sb);
    P3_CHECK(rl.resolved.canonical() == "ADU/sr");
  }
  // 一次幂（mutation）必须被检出
  P3_CHECK(!is_quadratic_variance(signal_sb, signal_sb));
  using astrocs::p3rsmp::Bunit;
  P3_CHECK(!is_quadratic_variance(signal_sb, Bunit{1, -2}));
}

NeighborhoodSet bilinear_scene(bool all_tiles) {
  InputGrid2D grid;
  grid.width = 8;
  grid.height = 8;
  grid.value.assign(64, 5.0);
  TileMask tiles;
  tiles.tile_px = 4;
  tiles.tiles_x = 2;
  tiles.tiles_y = 2;
  tiles.present.assign(4, 1);
  if (!all_tiles) tiles.present[1] = 0;  // tile (1,0) 缺失
  astrocs::p3rsmp::GridPlan plan;
  plan.out_width = 4;
  plan.out_height = 4;
  plan.out_origin_x = 4.0;
  plan.out_origin_y = 4.0;
  plan.out_step = 1.0;
  return astrocs::p3rsmp::make_bilinear_4quad_neighborhood(grid, tiles, plan, 1.0, 1.0);
}

void test_cross_tile_boundary_no_zero_fill() {
  const NeighborhoodSet nb_all = bilinear_scene(true);
  const SparseOperator R_all = astrocs::p3rsmp::build_row_normalized(nb_all, "bilinear_4quad");
  P3_CHECK(R_all.full_coverage(1e-12));
  P3_CHECK_NEAR(R_all.row_sums()[0], 1.0, 1e-14);
  std::vector<double> xb(64, 5.0);
  const std::vector<double> y_all = apply_operator(R_all, xb);
  for (double v : y_all) P3_CHECK_NEAR(v, 5.0, 1e-12);

  const NeighborhoodSet nb_missing = bilinear_scene(false);
  const SparseOperator R_missing =
      astrocs::p3rsmp::build_row_normalized(nb_missing, "bilinear_4quad");
  const std::vector<double> y_missing = apply_operator(R_missing, xb);
  int n_nan = 0;
  int n_zero = 0;
  for (std::size_t i = 0; i < y_missing.size(); ++i) {
    if (std::isnan(y_missing[i])) ++n_nan;
    if (y_missing[i] == 0.0) ++n_zero;
  }
  P3_CHECK(n_nan > 0);    // 缺 tile → NaN
  P3_CHECK(n_zero == 0);  // 禁零填
  bool saw_invalid = false;
  for (int i = 0; i < R_missing.n_out; ++i) {
    if (!R_missing.valid_out[static_cast<std::size_t>(i)]) {
      saw_invalid = true;
      P3_CHECK_NEAR(R_missing.coverage[static_cast<std::size_t>(i)], 0.0, 1e-15);
    }
  }
  P3_CHECK(saw_invalid);
}

}  // namespace

int main() {
  test_row_col_normalization();
  test_R_S_identity();
  test_constant_sb_field();
  test_flux_conservation();
  test_covariance_vs_oracle();
  test_diagonal_input_variance();
  test_correlation_deficit_detected();
  test_effective_psf();
  test_qw_output_frame_vs_oracle();
  test_qw_vs_monte_carlo();
  test_qw_non_commutation();
  test_sb_propagation_positive();
  test_bunit_quadratic();
  test_cross_tile_boundary_no_zero_fill();
  return p3test::finish("p3_rsmp_core");
}
