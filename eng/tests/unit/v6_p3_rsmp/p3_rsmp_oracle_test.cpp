// eng/tests/unit/v6_p3_rsmp/p3_rsmp_oracle_test.cpp
// IMPL-P3-RSMP-001 独立 Oracle 测试：
//   * bilinear_4quad 误差界/权重归一/边界（不调用生产实现）
//   * 最近邻替代双线性 → 超界（mutation 必红）
//   * C_y 与 Q/W 的生产实现 vs 独立稠密转写 + 固定种子 MC
//   * 对角化过度乐观可检出（G-P3-QW-04 证据）
#include <cmath>
#include <cstdio>
#include <vector>

#include "p3_rsmp.h"
#include "p3_rsmp_oracle.h"
#include "p3_rsmp_scenarios.h"
#include "p3_rsmp_test_util.h"

using astrocs::p3rsmp::DenseMatrix;
using astrocs::p3rsmp::GateConfig;
using astrocs::p3rsmp::KernelDescriptor;
using astrocs::p3rsmp::KernelRegistry;
using astrocs::p3rsmp::PointSourceInput;
using astrocs::p3rsmp::SparseOperator;
using astrocs::p3rsmp::Status;

namespace {

p3oracle::Mat to_oracle(const DenseMatrix& m) {
  p3oracle::Mat r(m.rows, m.cols);
  for (int i = 0; i < m.rows; ++i)
    for (int j = 0; j < m.cols; ++j) r(i, j) = m(i, j);
  return r;
}

void test_kernel_oracle_independent() {
  const p3oracle::KernelOracle ko = p3oracle::run_kernel_oracle();
  P3_CHECK(ko.max_interp_err <= ko.analytic_bound);          // 误差 ≤ 解析上界
  P3_CHECK(ko.err_over_bound > 0.9 && ko.err_over_bound <= 1.0);
  P3_CHECK_NEAR(ko.max_weight_sum_dev, 0.0, 1e-15);          // Σw=1
  P3_CHECK(ko.nearest_over_bound > 1.0);                     // 最近邻替代双线性 → 超界
  P3_CHECK(ko.zero_fill_error > 0.5);                        // 零填显著有害
  P3_CHECK(ko.boundary_fail_closed);

  const KernelRegistry& reg = KernelRegistry::frozen();
  const KernelDescriptor* bil = reg.find("bilinear_4quad");
  P3_CHECK(bil != nullptr);
  // 注册证据必须与独立 Oracle 复算一致（非自证）
  P3_CHECK_NEAR(bil->oracle.max_interp_err, ko.max_interp_err, 1e-6);
  P3_CHECK_NEAR(bil->oracle.analytic_bound, ko.analytic_bound, 1e-9);
  P3_CHECK_NEAR(bil->oracle.err_over_bound, ko.err_over_bound, 1e-4);
  P3_CHECK_NEAR(bil->oracle.max_weight_sum_dev, ko.max_weight_sum_dev, 1e-15);
  P3_CHECK(bil->oracle.ok && bil->oracle.independent);
  P3_CHECK(bil->oracle.boundary_fail_closed);
  P3_CHECK(bil->boundary_policy == astrocs::p3rsmp::BoundaryPolicy::MissingIsNaN);
  P3_CHECK(bil->oracle.zero_fill_error > 0.5);
  // 无 Oracle 证据的注册 → 结构门必须红（FZ-P3-KERNEL-REGISTRY negative_mutation）
  KernelDescriptor k = *bil;
  k.oracle = astrocs::p3rsmp::KernelOracleEvidence{};
  const auto g = reg.validate_registration(k);
  P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-03");
}

void test_covariance_cross_check() {
  const p3scen::Chain c = p3scen::make_chain(0.19);
  const SparseOperator R = p3scen::chain_R(c);
  const SparseOperator S = p3scen::chain_S(c);
  const p3oracle::Mat Rm = p3oracle::build_R(c.ov, c.omega_out, c.n_out, c.n_in);
  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  const p3oracle::Mat cxm = to_oracle(c.cx);
  const DenseMatrix cy_r = astrocs::p3rsmp::propagate_covariance(R, c.cx);
  const DenseMatrix cy_s = astrocs::p3rsmp::propagate_covariance(S, c.cx);
  const p3oracle::Mat cy_r_o = p3oracle::covariance(Rm, cxm);
  const p3oracle::Mat cy_s_o = p3oracle::covariance(Sm, cxm);
  for (int i = 0; i < c.n_out; ++i) {
    for (int j = 0; j < c.n_out; ++j) {
      P3_CHECK_NEAR(cy_r(i, j), cy_r_o(i, j), 1e-13);
      P3_CHECK_NEAR(cy_s(i, j), cy_s_o(i, j), 1e-13);
    }
  }
  const double rho = astrocs::p3rsmp::max_abs_offdiag_correlation(cy_s);
  P3_CHECK(rho > 0.0);  // 重采样制造相邻相关（C-P3-PROP-9）
}

PointSourceInput make_input(const p3scen::Chain& c, const SparseOperator& S, double a) {
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

void test_qw_oracle_and_mc() {
  const p3scen::Chain c = p3scen::make_chain(0.19);
  const SparseOperator S = p3scen::chain_S(c);
  const double a = 2.0;
  const auto res = astrocs::p3rsmp::propagate_point_source_flux(make_input(c, S, a), GateConfig{});
  P3_CHECK(res.status == Status::Ok);

  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  const p3oracle::Mat cdm = to_oracle(c.cx);  // omega=1
  const p3oracle::Mat cym = p3oracle::covariance(Sm, cdm);
  const std::vector<double> pio = p3oracle::matvec(Sm, c.psf);
  std::vector<double> d(static_cast<std::size_t>(c.n_in), 0.0);
  for (int j = 0; j < c.n_in; ++j) d[static_cast<std::size_t>(j)] = c.x[static_cast<std::size_t>(j)];
  const std::vector<double> fo = p3oracle::matvec(Sm, d);
  std::vector<double> q;
  P3_CHECK(p3oracle::solve(cym, pio, &q));
  P3_CHECK_NEAR(res.W, a * a * p3oracle::dot(q, pio), 1e-12);
  P3_CHECK_NEAR(res.Q, a * p3oracle::dot(q, fo), 1e-12);

  double rel = 0.0;
  const double var_mc = p3oracle::mc_var_Fhat(Sm, pio, cdm, a, 400000, nullptr, &rel);
  P3_CHECK(std::isfinite(var_mc));
  P3_CHECK(rel < 0.01);
  P3_CHECK_NEAR(res.var_F_hat, var_mc, 0.05 * var_mc);

  // 对角化过度乐观（G-P3-QW-04）：1/W_diag < 真实方差，必须被检出
  double diag_only_quad = 0.0;
  for (int i = 0; i < c.n_out; ++i) diag_only_quad += pio[static_cast<std::size_t>(i)] *
                                                       pio[static_cast<std::size_t>(i)] /
                                                       cym(i, i);
  const double claimed_var = 1.0 / (a * a * diag_only_quad);
  const double optimism = (var_mc - claimed_var) / var_mc;
  P3_CHECK(optimism > 0.02);
}

void test_effective_psf_delta_bias() {
  const p3scen::Chain c = p3scen::make_chain(0.19);
  const SparseOperator S = p3scen::chain_S(c);
  const p3oracle::Mat Sm = p3oracle::build_S(c.ov, c.omega_in, c.n_out, c.n_in);
  const p3oracle::Mat cdm = to_oracle(c.cx);
  const p3oracle::Mat cym = p3oracle::covariance(Sm, cdm);
  const std::vector<double> pio = p3oracle::matvec(Sm, c.psf);
  std::vector<double> q_true;
  P3_CHECK(p3oracle::solve(cym, pio, &q_true));
  const double w_true = p3oracle::dot(q_true, pio);
  std::vector<double> delta = {1.0, 0.0};  // 把输出 PSF 当单像素 δ（禁）
  std::vector<double> q_delta;
  P3_CHECK(p3oracle::solve(cym, delta, &q_delta));
  const double w_delta = p3oracle::dot(q_delta, delta);
  P3_CHECK(std::fabs(w_delta - w_true) / w_true > 0.5);  // δ 近似大偏差
}

}  // namespace

int main() {
  test_kernel_oracle_independent();
  test_covariance_cross_check();
  test_qw_oracle_and_mc();
  test_effective_psf_delta_bias();
  return p3test::finish("p3_rsmp_oracle");
}
