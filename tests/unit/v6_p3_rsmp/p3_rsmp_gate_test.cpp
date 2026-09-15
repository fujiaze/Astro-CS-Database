// tests/unit/v6_p3_rsmp/p3_rsmp_gate_test.cpp
// IMPL-P3-RSMP-001 fail-closed 门测试：三模式 12 门 + kernel/covariance/QW/epsf/provenance 扩展门。
// 每条负向 mutation 注入一个违反冻结的记录/注册，断言门变红（对应冻结条款的 negative_mutation）。
#include <string>
#include <vector>

#include "p3_rsmp.h"
#include "p3_rsmp_test_util.h"

using namespace astrocs::p3rsmp;

namespace {

ProductRecord positive_sb() {
  ProductRecord r;
  r.mode_declared = true;
  r.mode = P3Mode::SurfaceBrightness;
  r.measurement_capable = true;
  r.uncertainty_available = true;
  r.covariance = CovarianceRepresentation::ExactFull;
  r.signal_bunit = units::signal_sb;
  r.variance_bunit = units::sb_variance_out;
  r.ivar_bunit = units::sb_ivar_out;
  r.bunit_quadratic_law_ok = true;
  return r;
}

ProductRecord positive_psf() {
  ProductRecord r;
  r.mode_declared = true;
  r.mode = P3Mode::PointSourceFlux;
  r.psf_present = true;
  r.psf_sum = 1.0;
  r.point_information_present = true;
  r.photometric_scale_present = true;
  r.photometric_scale = 2.0;
  r.effective_psf_present = true;
  r.effective_psf_normalization_declared = true;
  r.covariance = CovarianceRepresentation::ExactFull;
  return r;
}

ProductRecord positive_vis() {
  ProductRecord r;
  r.mode_declared = true;
  r.mode = P3Mode::Visualization;
  r.measurement_capable = false;
  return r;
}

void expect_ok(const ProductRecord& r) {
  const std::vector<GateResult> all = check_failclosed_all(r, GateConfig{});
  P3_CHECK_NO_VIOLATION(all);
}

void expect_code(const ProductRecord& r, const std::string& code) {
  const std::vector<GateResult> all = check_failclosed_all(r, GateConfig{});
  P3_CHECK_CODE(all, code);
}

void test_positive_records() {
  expect_ok(positive_sb());
  expect_ok(positive_psf());
  expect_ok(positive_vis());
}

void test_mode_gate() {
  ProductRecord r = positive_sb();
  r.mode_declared = false;
  expect_code(r, "G-P3-MODE-01");
  const GateResult g = check_failclosed(r, GateConfig{});
  P3_CHECK(g.status == Status::Reject);
  P3_CHECK(g.code == "G-P3-MODE-01");
  P3_CHECK(parse_mode("surface_brightness", nullptr) == Status::InvalidArgument);
  P3Mode m = P3Mode::Visualization;
  P3_CHECK(parse_mode("surface_brightness", &m) == Status::Ok && m == P3Mode::SurfaceBrightness);
  P3_CHECK(parse_mode("point_source_flux", &m) == Status::Ok && m == P3Mode::PointSourceFlux);
  P3_CHECK(parse_mode("visualization", &m) == Status::Ok && m == P3Mode::Visualization);
  P3_CHECK(parse_mode("auto", &m) == Status::Reject);            // legacy（FZ-P3-MODES）
  P3_CHECK(parse_mode("0", &m) == Status::Reject);               // legacy 整数
  P3_CHECK(parse_mode("support_x_snr2", &m) == Status::Reject);  // legacy
  P3_CHECK(parse_mode("psf_snr_power", &m) == Status::Reject);   // DEFERRED（C-004.1）
}

void test_sb_gates() {
  {  // G-P3-SB-01 无逐像素 Omega 做 flux 换算
    ProductRecord r = positive_sb();
    r.flux_conversion = true;
    r.omega_per_pixel = false;
    expect_code(r, "G-P3-SB-01");
  }
  {  // G-P3-SB-02 声明测量却 uncertainty unavailable 且无原因
    ProductRecord r = positive_sb();
    r.measurement_capable = true;
    r.uncertainty_available = false;
    r.uncertainty_unavailable_reason.clear();
    expect_code(r, "G-P3-SB-02");
    r.uncertainty_unavailable_reason = "upstream_covariance_unavailable";
    expect_ok(r);
  }
  {  // G-P3-SB-03 variance BUNIT 一次幂
    ProductRecord r = positive_sb();
    r.variance_bunit = units::signal_sb;
    expect_code(r, "G-P3-SB-03");
    ProductRecord r2 = positive_sb();
    r2.ivar_bunit = units::sb_variance_out;
    expect_code(r2, "G-P3-SB-03");
  }
  {  // G-P3-SB-04 只出对角且无相关核
    ProductRecord r = positive_sb();
    r.variance_diagonal_only = true;
    r.correlation_kernel_present = false;
    expect_code(r, "G-P3-SB-04");
    expect_code(r, "G-P3-COV-01");
  }
}

void test_psf_gates() {
  {
    ProductRecord r = positive_psf();
    r.psf_present = false;
    expect_code(r, "G-P3-PSF-01");
  }
  {
    ProductRecord r = positive_psf();
    r.psf_sum = 1.05;
    expect_code(r, "G-P3-PSF-02");
  }
  {
    ProductRecord r = positive_psf();
    r.point_information_present = false;
    r.rebuildable_from_frames = false;
    expect_code(r, "G-P3-PSF-03");
    r.rebuildable_from_frames = true;
    expect_ok(r);
  }
  {
    ProductRecord r = positive_psf();
    r.variance_diagonal_only = true;
    r.correlation_kernel_present = false;
    expect_code(r, "G-P3-PSF-04");
  }
  {
    ProductRecord r = positive_psf();
    r.photometric_scale_present = false;
    expect_code(r, "G-P3-PSF-05");
  }
  {
    ProductRecord r = positive_psf();
    r.effective_psf_present = false;
    expect_code(r, "G-P3-PSF-06");
    ProductRecord r2 = positive_psf();
    r2.effective_psf_fwhm_only = true;
    expect_code(r2, "G-P3-PSF-06");
    expect_code(r2, "G-P3-EPSF-01");
  }
}

void test_vis_gate() {
  for (int variant = 0; variant < 4; ++variant) {
    ProductRecord r = positive_vis();
    if (variant == 0) r.measurement_capable = true;
    if (variant == 1) r.writes_variance = true;
    if (variant == 2) r.writes_ivar = true;
    if (variant == 3) r.writes_point_information = true;
    expect_code(r, "G-P3-VIS-01");
  }
}

void test_global_and_covariance_gates() {
  {
    ProductRecord r = positive_sb();
    r.weight_sources = {"median_source_snr"};
    expect_code(r, "G-P3-GLB-01");
  }
  {
    ProductRecord r = positive_sb();
    r.variance_from = "psfsw_robust_weight";
    expect_code(r, "G-P3-GLB-01");
  }
  {
    ProductRecord r = positive_sb();
    r.uses_relative_weight_as_ivar = true;
    expect_code(r, "G-P3-GLB-01");
    expect_code(r, "G-P3-COV-03");
  }
  {
    ProductRecord r = positive_sb();
    r.covariance = CovarianceRepresentation::WeightDerived;
    expect_code(r, "G-P3-COV-02");
  }
  {
    ProductRecord r = positive_sb();
    r.covariance = CovarianceRepresentation::ApproximateCorrelation;
    r.correlation_kernel_present = true;
    r.correlation_approx_error_available = true;
    r.correlation_approx_error = 0.01;
    const std::vector<GateResult> all = check_failclosed_all(r, GateConfig{});
    P3_CHECK_CODE(all, "CF-T-P3-CORR-EPSILON");  // OPEN 项 → fail-closed
    GateConfig signed_cfg;
    signed_cfg.epsilon_corr_ratified = true;
    signed_cfg.epsilon_corr = kNaN;  // 本任务不自定值
    const std::vector<GateResult> opened = check_failclosed_all(r, signed_cfg);
    P3_CHECK(!p3test::has_code(opened, "CF-T-P3-CORR-EPSILON"));
  }
}

void test_qw_gates() {
  {
    ProductRecord r = positive_psf();
    r.input_qw_resampled = true;
    expect_code(r, "G-P3-QW-01");
  }
  {
    ProductRecord r = positive_psf();
    r.w_from_sum_input = true;
    expect_code(r, "G-P3-QW-02");
  }
  {
    ProductRecord r = positive_psf();
    r.upstream_w_recomputed = true;
    expect_code(r, "G-P3-QW-03");
  }
  {
    ProductRecord r = positive_psf();
    r.covariance = CovarianceRepresentation::DiagonalOnly;
    r.correlation_kernel_present = true;  // 相关核在，避免 COV-01
    r.optimism_audit_performed = false;
    expect_code(r, "G-P3-QW-04");
    r.optimism_audit_performed = true;
    r.diagonal_optimism_detected = true;
    expect_code(r, "G-P3-QW-04");
    r.diagonal_optimism_detected = false;
    const std::vector<GateResult> clean = check_failclosed_all(r, GateConfig{});
    P3_CHECK(!p3test::has_code(clean, "G-P3-QW-04"));
  }
}

void test_epsf_and_provenance_gates() {
  {
    ProductRecord r = positive_sb();
    r.effective_psf_fwhm_only = true;
    expect_code(r, "G-P3-EPSF-01");
  }
  {
    ProductRecord r = positive_sb();
    r.provenance_complete = false;
    expect_code(r, "G-P3-PROV-01");
  }
  {
    ProductRecord r = positive_sb();
    r.provenance_missing_keys = {"flux_conservation_factor", "k_corr"};
    expect_code(r, "G-P3-PROV-01");
  }
}

void test_forbidden_token_table() {
  P3_CHECK(!forbidden_weight_source_tokens().empty());
  for (const std::string& t : forbidden_weight_source_tokens()) {
    P3_CHECK(token_is_forbidden_weight_source(t));
    ProductRecord r = positive_sb();
    r.variance_from = t;
    const std::vector<GateResult> all = check_failclosed_all(r, GateConfig{});
    P3_CHECK(p3test::has_code(all, "G-P3-GLB-01"));
  }
  // 合法来源不是禁止项
  P3_CHECK(!token_is_forbidden_weight_source("psf"));
  P3_CHECK(!token_is_forbidden_weight_source("combination_coefficients"));
  P3_CHECK(!forbidden_psfsw_product_keys().empty());
}

void test_kernel_registry() {
  const KernelRegistry& reg = KernelRegistry::frozen();
  const KernelDescriptor* nearest = reg.find("nearest");
  const KernelDescriptor* bil = reg.find("bilinear_4quad");
  const KernelDescriptor* area = reg.find("bilinear_area_overlap_exact");
  P3_CHECK(nearest != nullptr && bil != nullptr && area != nullptr);
  P3_CHECK(nearest->status == KernelStatus::RegisteredRestricted);
  P3_CHECK(bil->status == KernelStatus::RegisteredWithOracle);
  P3_CHECK(area->status == KernelStatus::RegisteredWithOracle);
  P3_CHECK(area->production_science_default);
  P3_CHECK(!bil->production_science_default);  // 注册但非生产默认（ALG-P3-003）
  P3_CHECK(bil->oracle.ok && bil->oracle.independent);
  P3_CHECK(area->oracle.ok && area->oracle.independent);

  // 生产准入
  P3_CHECK(reg.admit("bilinear_4quad", KernelUse::ContinuousField, P3Mode::SurfaceBrightness).ok());
  P3_CHECK(reg.admit("bilinear_4quad", KernelUse::ContinuousField, P3Mode::PointSourceFlux).ok());
  P3_CHECK(reg.admit("bilinear_area_overlap_exact", KernelUse::ContinuousField,
                     P3Mode::SurfaceBrightness).ok());
  {
    const GateResult g = reg.admit("nearest", KernelUse::ContinuousField, P3Mode::SurfaceBrightness);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-02");  // nearest 禁科学默认
  }
  P3_CHECK(reg.admit("nearest", KernelUse::DiscreteMask, P3Mode::Visualization).ok());
  {
    const GateResult g = reg.admit("bicubic", KernelUse::ContinuousField, P3Mode::SurfaceBrightness);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-01");  // 未注册核进生产
  }
  {
    const GateResult g = reg.admit("lanczos", KernelUse::ContinuousField, P3Mode::SurfaceBrightness);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-01");
  }
  {
    const GateResult g = reg.admit("nonexistent_kernel", KernelUse::ContinuousField,
                                   P3Mode::SurfaceBrightness);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-01");
  }
  // 注册结构门：缺 Oracle / 缺边界 / 非独立证据 → 红
  {
    KernelDescriptor k = *bil;
    k.oracle.kind = OracleKind::None;
    const GateResult g = reg.validate_registration(k);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-03");
  }
  {
    KernelDescriptor k = *bil;
    k.oracle.independent = false;
    const GateResult g = reg.validate_registration(k);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-03");
  }
  {
    KernelDescriptor k = *bil;
    k.boundary_defined = false;
    k.boundary_definition.clear();
    const GateResult g = reg.validate_registration(k);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-03");
  }
  {
    KernelDescriptor k = *bil;
    k.error_bound_present = false;
    k.error_bound_expression.clear();
    const GateResult g = reg.validate_registration(k);
    P3_CHECK(g.status == Status::Reject && g.code == "G-P3-KRN-03");
  }
  P3_CHECK(reg.validate_registration(*bil).ok());
  P3_CHECK(reg.validate_registration(*area).ok());
  // 全表：registered 核必须有误差界/边界/独立 Oracle
  for (const KernelDescriptor& k : reg.all()) {
    if (k.status == KernelStatus::RequiresRegistration) continue;
    P3_CHECK(k.boundary_defined);
    P3_CHECK(!k.boundary_definition.empty());
    if (k.normalization != KernelNormalization::Discrete) {
      P3_CHECK(k.error_bound_present);
      P3_CHECK(!k.error_bound_expression.empty());
    }
    P3_CHECK(k.oracle.ok && k.oracle.independent);
  }
}

}  // namespace

int main() {
  test_positive_records();
  test_mode_gate();
  test_sb_gates();
  test_psf_gates();
  test_vis_gate();
  test_global_and_covariance_gates();
  test_qw_gates();
  test_epsf_and_provenance_gates();
  test_forbidden_token_table();
  test_kernel_registry();
  return p3test::finish("p3_rsmp_gate");
}
