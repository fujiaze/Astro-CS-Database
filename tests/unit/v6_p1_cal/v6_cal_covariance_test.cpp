/* ============================================================================
 * tests/unit/v6_p1_cal/v6_cal_covariance_test.cpp
 * IMPL-P1-CAL-001 — Phase1 校准 covariance 共址单元测试
 *
 * 覆盖（冻结节点 → 用例）:
 *   T-CAL-01 CAL-COV-FORMULA   : per-pixel variance == J C_in J^T（dense 独立 Oracle）
 *   T-CAL-02 CAL-COV-FORMULA   : 同一 bias master_id 折叠为 (1-alpha)^2（负例：重复计）
 *   T-CAL-04 CAL-UNIT          : variance == signal^2 / ivar == 1/variance
 *   T-CAL-05 CAL-COV-FORMULA   : 缺 gain/read_noise/master_id、f<=0 fail-closed
 *   T-CAL-06 CAL-NO-CLIP       : 保留负值、不加 pedestal、floor 区域语义
 *   T-CAL-07 CAL-COV-REPRESENTATION : lowrank / correlation_kernel / common_master_id
 *   ADJ-OBS-01-SHARED           : 联合/朴素方差比 > 1 必检出（负例：按独立处理）
 *   FZ-FORMULA-COV-PROP         : covariance 记录门（禁止权重反推 variance）
 *   负向注入: 8 条注入缺陷必须使对应门变红（不 skIP、零用例即失败）
 * ==========================================================================*/

#include "astrocs/calibration/v6_calibration_covariance.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "v6_cal_oracle.hpp"
#include "v6_cal_test_support.hpp"

using astrocs::calibration::v6::CalConfig;
using astrocs::calibration::v6::CalPixelInput;
using astrocs::calibration::v6::CalReason;
using astrocs::calibration::v6::CalResult;
using astrocs::calibration::v6::CalStatus;
using astrocs::calibration::v6::CovarianceRecord;
using astrocs::calibration::v6::DarkOption;
using astrocs::calibration::v6::MasterIdentity;
using astrocs::calibration::v6::SharedCorrelationKernel;
using astrocs::calibration::v6::SharedLowRank;
using astrocs::calibration::v6::SharedCommonMaster;
using astrocs::calibration::v6::SharedRepresentationKind;
using astrocs::calibration::v6::SharedSystematic;
using astrocs::calibration::v6::UnitLaw;
using astrocs::calibration::v6::VarianceSource;

namespace {

constexpr double kAlpha = 0.6;
constexpr double kGain = 1.5;
constexpr double kReadNoise = 3.0;
constexpr double kQAdu = 1.0;
constexpr double kFlat = 2.0;
constexpr double kR = 20.0;
constexpr double kBias = 2.0;
constexpr double kDark = 12.0;

MasterIdentity make_master(const std::string& id, const std::string& units,
                           int n, double v_single) {
  MasterIdentity m;
  m.master_id = id;
  m.normalization_version = "flatnorm_v1";
  m.units = units;
  m.combine_rule = "mean";
  m.n_combined = n;
  m.v_single_frame = v_single;
  m.common_mode_declared = false;
  m.common_mode_variance = 0.0;
  return m;
}

CalConfig baseline_config(void) {
  CalConfig cfg;
  cfg.dark_opt = DarkOption::kExplicitBiasDark;
  cfg.alpha = kAlpha;
  cfg.detector.has_gain = true;
  cfg.detector.gain = kGain;
  cfg.detector.has_read_noise = true;
  cfg.detector.read_noise_e = kReadNoise;
  cfg.detector.quantum_declared = true;
  cfg.detector.q_adu = kQAdu;
  cfg.detector.variance_source = VarianceSource::kPhysical;
  cfg.bias_light_master = make_master("bias_master_A", "ADU", 10, 0.4);
  cfg.bias_dark_master = make_master("bias_master_A", "ADU", 10, 0.4);
  cfg.dark_master = make_master("dark_master_A", "ADU", 10, 0.9);
  cfg.flat_master = make_master("flat_master_A", "1", 1, 0.0004);
  return cfg;
}

CalPixelInput baseline_pixel(void) {
  CalPixelInput px;
  px.r = kR;
  px.has_bias_light = true;
  px.bias_light = kBias;
  px.has_bias_dark = true;
  px.bias_dark = kBias;
  px.has_dark = true;
  px.dark = kDark;
  px.has_flat = true;
  px.flat = kFlat;
  return px;
}

double expected_y(const CalConfig& cfg, const CalPixelInput& px) {
  const double f = px.has_flat ? (px.flat < 0.1 ? 0.1 : px.flat) : 1.0;
  if (cfg.dark_opt == DarkOption::kExplicitBiasDark) {
    const double b = px.has_bias_light ? px.bias_light : 0.0;
    const double d = px.has_dark ? px.dark : 0.0;
    const double bd = px.has_bias_dark ? px.bias_dark : b;
    return (px.r - b - cfg.alpha * (d - bd)) / f;
  }
  const double d = px.has_dark ? px.dark : 0.0;
  return (px.r - d) / f;
}

/* ── T-CAL-01: J C_in J^T（dense 独立 Oracle） ───────────────────────── */
void test_formula_dense_oracle(void) {
  const CalConfig cfg = baseline_config();
  const CalPixelInput px = baseline_pixel();
  const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(r.status == CalStatus::kOk, "T-CAL-01 status ok");
  V6_CHECK_CLOSE(r.y, expected_y(cfg, px), 1e-12, "T-CAL-01 signal y");

  double J[4];
  astrocs::calibration::v6::oracle::analytic_jacobian(kAlpha, kFlat, r.y, J);
  for (int i = 0; i < 4; ++i) {
    V6_CHECK_CLOSE(r.jacobian[i], J[i], 1e-12,
                   "T-CAL-01 jacobian[" + std::to_string(i) + "]");
  }
  const double v_r = r.terms.light_frame();
  const double v_b = cfg.bias_light_master.self_variance();
  const double v_d = cfg.dark_master.self_variance() + r.terms.dark_photon;
  const double v_f = cfg.flat_master.self_variance();
  const double expected = astrocs::calibration::v6::oracle::full_pixel_variance(
      kAlpha, kFlat, r.y, kReadNoise, kGain, kQAdu, kR, kDark, v_b,
      cfg.dark_master.self_variance(), v_f);
  V6_CHECK_CLOSE(r.v_cal, expected, 1e-12, "T-CAL-01 v_cal dense oracle");

  const double V[4] = {v_r, v_b, v_d, v_f};
  const double closed =
      astrocs::calibration::v6::diagonal_quadratic_variance(J, v_r, v_b, v_d, v_f);
  V6_CHECK_CLOSE(closed, astrocs::calibration::v6::oracle::dense_jcj(J, V), 1e-12,
                 "T-CAL-01 diagonal quadratic == dense J C J^T");
  V6_CHECK_CLOSE(r.sigma_cal * r.sigma_cal, r.v_cal, 1e-12, "T-CAL-01 sigma^2 = v");
}

/* ── T-CAL-02: 同一 bias master 只计一次（(1-alpha)^2 折叠） ────────── */
void test_same_master_folding(void) {
  const CalConfig cfg = baseline_config();
  const CalPixelInput px = baseline_pixel();
  const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(r.same_bias_master_folded, "T-CAL-02 same master detected");

  const double v_r = r.terms.light_frame();
  const double v_b = cfg.bias_light_master.self_variance();
  const double v_d = cfg.dark_master.self_variance() + r.terms.dark_photon;
  const double v_f = cfg.flat_master.self_variance();
  const double bias_term =
      r.v_cal - (r.jacobian[0] * r.jacobian[0] * v_r +
                 r.jacobian[2] * r.jacobian[2] * v_d +
                 r.jacobian[3] * r.jacobian[3] * v_f);
  /* 同 master 折叠: j_b = -(1-alpha)/f => (0.2)^2 * 0.04 = 0.0016 */
  const double folded = std::pow(-(1.0 - kAlpha) / kFlat, 2.0) * v_b;
  V6_CHECK_CLOSE(bias_term, folded, 1e-12, "T-CAL-02 folded (1-alpha)^2 term");
  /* 负向对照: 设计式重复计 (1/f^2 + alpha^2/f^2) V_b = 0.0136 必须不等 */
  const double double_counted =
      (1.0 / (kFlat * kFlat) + kAlpha * kAlpha / (kFlat * kFlat)) * v_b;
  V6_CHECK(std::fabs(bias_term - double_counted) > 1e-6,
           "T-CAL-02 folded != double-counted design form");
  /* master_ids 只登记一次共享 bias */
  int bias_records = 0;
  for (const auto& m : r.master_ids) {
    if (m.role == "bias_shared" || m.role == "bias_light") bias_records += 1;
  }
  V6_CHECK(bias_records == 1, "T-CAL-02 same bias master counted once in records");

  /* 仅提供光路 bias（暗路 bias 缺失）：同一物理 master 用于两处 -> 仍折叠 */
  {
    CalPixelInput only_light = baseline_pixel();
    only_light.has_bias_dark = false;
    const CalResult r2 = astrocs::calibration::v6::calibrate_pixel(cfg, only_light);
    V6_CHECK(r2.same_bias_master_folded, "T-CAL-02 single bias present folds");
    V6_CHECK_CLOSE(r2.y, (kR - kBias - kAlpha * (kDark - kBias)) / kFlat, 1e-12,
                   "T-CAL-02 single bias signal folded");
    const double v_b = cfg.bias_light_master.self_variance();
    const double bias_term2 =
        r2.v_cal -
        (r2.jacobian[0] * r2.jacobian[0] * r2.terms.light_frame() +
         r2.jacobian[2] * r2.jacobian[2] *
             (cfg.dark_master.self_variance() + r2.terms.dark_photon) +
         r2.jacobian[3] * r2.jacobian[3] * cfg.flat_master.self_variance());
    V6_CHECK_CLOSE(bias_term2, std::pow(-(1.0 - kAlpha) / kFlat, 2.0) * v_b, 1e-12,
                   "T-CAL-02 single bias folded variance");
  }
  /* 仅提供暗路 bias（光路 bias 缺失）：同样折叠 */
  {
    CalPixelInput only_dark = baseline_pixel();
    only_dark.has_bias_light = false;
    const CalResult r3 = astrocs::calibration::v6::calibrate_pixel(cfg, only_dark);
    V6_CHECK(r3.same_bias_master_folded, "T-CAL-02 dark-path-only bias folds");
    V6_CHECK_CLOSE(r3.y, (kR - kBias - kAlpha * (kDark - kBias)) / kFlat, 1e-12,
                   "T-CAL-02 dark-path-only signal folded");
    const double v_b = cfg.bias_dark_master.self_variance();
    const double bias_term3 =
        r3.v_cal -
        (r3.jacobian[0] * r3.jacobian[0] * r3.terms.light_frame() +
         r3.jacobian[2] * r3.jacobian[2] *
             (cfg.dark_master.self_variance() + r3.terms.dark_photon) +
         r3.jacobian[3] * r3.jacobian[3] * cfg.flat_master.self_variance());
    V6_CHECK_CLOSE(bias_term3, std::pow(-(1.0 - kAlpha) / kFlat, 2.0) * v_b, 1e-12,
                   "T-CAL-02 dark-path-only folded variance");
  }
}

/* 不同 master_id: 两个独立项 V_b + alpha^2 V_b（DESIGN §4.2 情形） */
void test_distinct_master(void) {
  CalConfig cfg = baseline_config();
  cfg.bias_dark_master = make_master("bias_master_B", "ADU", 10, 0.4);
  const CalPixelInput px = baseline_pixel();
  const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(!r.same_bias_master_folded, "T-CAL-02b distinct master not folded");
  const double v_r = r.terms.light_frame();
  const double v_b = cfg.bias_light_master.self_variance();
  const double v_d = cfg.dark_master.self_variance() + r.terms.dark_photon;
  const double v_f = cfg.flat_master.self_variance();
  const double bias_term =
      r.v_cal - (r.jacobian[0] * r.jacobian[0] * v_r +
                 r.jacobian[2] * r.jacobian[2] * v_d +
                 r.jacobian[3] * r.jacobian[3] * v_f);
  const double design =
      (1.0 / (kFlat * kFlat) + kAlpha * kAlpha / (kFlat * kFlat)) * v_b;
  V6_CHECK_CLOSE(bias_term, design, 1e-12,
                 "T-CAL-02b distinct masters == DESIGN V_b + alpha^2 V_b");
}

/* ── T-CAL-04: 单位律 ───────────────────────────────────────────────── */
void test_unit_law(void) {
  const UnitLaw law = astrocs::calibration::v6::calibration_unit_law();
  V6_CHECK(law.signal_unit == "ADU", "T-CAL-04 signal unit ADU");
  V6_CHECK(law.variance_unit == "ADU^2", "T-CAL-04 variance unit ADU^2");
  V6_CHECK(law.ivar_unit == "ADU^-2", "T-CAL-04 ivar unit ADU^-2");
  V6_CHECK(astrocs::calibration::v6::unit_law_consistent(law),
           "T-CAL-04 variance == signal^2");
  UnitLaw bad = law;
  bad.variance_unit = "ADU";
  V6_CHECK(!astrocs::calibration::v6::unit_law_consistent(bad),
           "T-CAL-04 negative unit law rejects ADU variance");
}

/* ── T-CAL-06: 不裁剪 / 不加 pedestal / floor 语义 ──────────────────── */
void test_no_clip_and_floor(void) {
  CalConfig cfg = baseline_config();
  CalPixelInput px = baseline_pixel();
  px.r = -5.0;
  px.bias_light = 0.0;
  px.bias_dark = 0.0;
  px.dark = 0.0;
  px.has_dark = false;
  const CalResult neg = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(neg.status == CalStatus::kOk, "T-CAL-06 negative light status ok");
  V6_CHECK(neg.y < 0.0, "T-CAL-06 negative value preserved (no clamp)");
  V6_CHECK_CLOSE(neg.y, -5.0 / kFlat, 1e-12, "T-CAL-06 negative value exact");

  /* f=0.05 -> floor 0.1 生效但 f>0 不 REJECT */
  px.r = 5.0;
  px.flat = 0.05;
  const CalResult floored = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(floored.status == CalStatus::kOk, "T-CAL-06 tiny flat status ok");
  V6_CHECK(floored.flat_floor_applied, "T-CAL-06 flat floor applied");
  V6_CHECK_CLOSE(floored.y, 5.0 / 0.1, 1e-12, "T-CAL-06 floored division");

  /* f<=0 / 非有限 -> REJECT（不得静默 floor 造值） */
  px.flat = 0.0;
  const CalResult zero_flat = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(zero_flat.status == CalStatus::kRejected &&
               zero_flat.reason == CalReason::kNonPositiveFlat,
           "T-CAL-06 f=0 rejected (no silent floor)");
  px.flat = -1.0;
  const CalResult neg_flat = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(neg_flat.status == CalStatus::kRejected,
           "T-CAL-06 f<0 rejected");
  px.flat = std::nan("");
  const CalResult nan_flat = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK(nan_flat.status == CalStatus::kRejected,
           "T-CAL-06 f=NaN rejected");
}

/* ── T-CAL-05 / §2.4: fail-closed ───────────────────────────────────── */
void test_fail_closed(void) {
  {
    CalConfig cfg = baseline_config();
    cfg.detector.has_gain = false;
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable &&
                 r.reason == CalReason::kMissingGainOrReadNoise,
             "T-CAL-05 missing gain -> unavailable");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.detector.has_read_noise = false;
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable,
             "T-CAL-05 missing read_noise -> unavailable");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.detector.has_gain = false;
    cfg.detector.has_read_noise = false;
    cfg.detector.variance_source = VarianceSource::kEmpiricalMadFallback;
    CalPixelInput px = baseline_pixel();
    px.has_empirical_variance = true;
    px.empirical_variance = 0.25;
    const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
    V6_CHECK(r.status == CalStatus::kOk &&
                 r.variance_source == VarianceSource::kEmpiricalMadFallback,
             "T-CAL-05 declared empirical_mad_fallback accepted");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.bias_light_master.master_id = "";
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable &&
                 r.reason == CalReason::kMissingMasterIdentity,
             "T-CAL-05 missing bias master_id -> unavailable");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.flat_master.normalization_version = "";
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable,
             "T-CAL-05 missing normalization_version -> unavailable");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.dark_master.units = "";
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable,
             "T-CAL-05 missing master units -> unavailable");
  }
  {
    /* 给了 V_single_frame 却无 N_combined -> 自身方差不可判 -> REJECT */
    CalConfig cfg = baseline_config();
    cfg.bias_light_master.n_combined = 0;
    cfg.bias_dark_master.n_combined = 0;
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kRejected &&
                 r.reason == CalReason::kInvalidConfig,
             "T-CAL-05 V_single_frame without N_combined -> reject");
  }
  {
    CalConfig cfg = baseline_config();
    cfg.shared.kind = SharedRepresentationKind::kLowRank;
    cfg.shared.low_rank.rank = 0; /* 不可表示 */
    const CalResult r =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r.status == CalStatus::kUnavailable &&
                 r.reason == CalReason::kSharedUnrepresentable,
             "T-CAL-05 unrepresentable shared -> unavailable");
    cfg.shared.system_error_budget_declared = true;
    cfg.shared.system_error_budget_variance = 0.03;
    const CalResult r2 =
        astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
    V6_CHECK(r2.status == CalStatus::kOk,
             "T-CAL-05 unrepresentable shared + budget accepted");
  }
}

/* ── T-CAL-07 + ADJ-OBS-01-SHARED: 三通道 + 联合/朴素比 ─────────────── */
void test_shared_channels(void) {
  const int n = 4;
  const std::vector<double> c(n, 0.25);
  const std::vector<double> diag(n, 0.05);
  const double v_shared = 0.02;
  const double s = std::sqrt(v_shared);
  const double naive = 0.0125;
  const double expected_joint = naive + v_shared; /* (Σc)^2 v_shared */
  const std::vector<double> alpha(n, 1.0);

  /* 无共享项 */
  {
    SharedSystematic sh;
    const auto g = astrocs::calibration::v6::evaluate_shared_gate(
        c, diag, sh, alpha, {});
    V6_CHECK(g.ok && !g.shared_present, "T-CAL-07 no shared term -> ok");
  }
  /* lowrank 通道 */
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kLowRank;
    sh.low_rank.factor_ref = "L#1";
    sh.low_rank.rank = 1;
    sh.low_rank.factor.assign(n, s);
    const auto g = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, {});
    V6_CHECK(g.ok && g.shared_present, "T-CAL-07 lowrank gate ok");
    V6_CHECK_CLOSE(g.naive_variance, naive, 1e-12, "T-CAL-07 lowrank naive");
    V6_CHECK_CLOSE(g.joint_variance, expected_joint, 1e-12, "T-CAL-07 lowrank joint");
    V6_CHECK(g.ratio > 1.0 + 1e-9, "T-CAL-07 lowrank ratio>1");
    const double dense = astrocs::calibration::v6::oracle::low_rank_dense(
        c, sh.low_rank.factor, n, 1);
    V6_CHECK_CLOSE(g.shared_variance, dense, 1e-12, "T-CAL-07 lowrank vs dense oracle");
  }
  /* common_master_id 通道 */
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kCommonMasterId;
    sh.common_master.master_id = "bias_master_A";
    sh.common_master.alpha_m = 1.0;
    sh.common_master.master_variance = v_shared;
    const auto g = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, {});
    V6_CHECK(g.ok && g.shared_present, "T-CAL-07 common_master gate ok");
    V6_CHECK_CLOSE(g.joint_variance, expected_joint, 1e-12, "T-CAL-07 common_master joint");
  }
  /* correlation_kernel 通道（显式物化 K；核形式 DI-03 OPEN 不发明） */
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kCorrelationKernel;
    sh.correlation_kernel.kernel_id = "kernel_v1";
    sh.correlation_kernel.kernel_version = "1";
    sh.correlation_kernel.scale = s;
    std::vector<double> K(n * n, 1.0);
    const auto g = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, K);
    V6_CHECK(g.ok && g.shared_present, "T-CAL-07 kernel gate ok");
    V6_CHECK_CLOSE(g.joint_variance, expected_joint, 1e-12, "T-CAL-07 kernel joint");
    /* 未物化 K 且无预算 -> unavailable（DI-03 fail-closed） */
    const auto g2 = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, {});
    V6_CHECK(!g2.ok && g2.reason == CalReason::kSharedUnrepresentable,
             "T-CAL-07 unmaterialized kernel -> unavailable");
    sh.system_error_budget_declared = true;
    sh.system_error_budget_variance = v_shared;
    const auto g3 = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, {});
    V6_CHECK(g3.ok, "T-CAL-07 unmaterialized kernel + budget ok");
  }
  /* lowrank 秩/尺寸不匹配 -> 不可表示 */
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kLowRank;
    sh.low_rank.factor_ref = "L#2";
    sh.low_rank.rank = 2;
    sh.low_rank.factor.assign(n, s); /* 尺寸应为 n*2 */
    const auto g = astrocs::calibration::v6::evaluate_shared_gate(c, diag, sh, alpha, {});
    V6_CHECK(!g.ok && g.reason == CalReason::kSharedUnrepresentable,
             "T-CAL-07 lowrank size mismatch -> unavailable");
  }
}

/* ── 独立/共享对角划分一致性（避免共享项重复计） ───────────────────── */
void test_independent_shared_split(void) {
  const CalConfig cfg = baseline_config();
  const CalPixelInput px = baseline_pixel();
  const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
  V6_CHECK_CLOSE(r.v_cal, r.v_cal_independent + r.v_cal_shared_diagonal, 1e-12,
                 "SPLIT v_cal = independent + shared diagonal");
  /* 独立部分不含 master 自方差；共享对角 == jb^2 Vb + jd^2 Vd + jf^2 Vf */
  const double v_b = cfg.bias_light_master.self_variance();
  const double v_d = cfg.dark_master.self_variance();
  const double v_f = cfg.flat_master.self_variance();
  const double expected_shared = r.jacobian[1] * r.jacobian[1] * v_b +
                                 r.jacobian[2] * r.jacobian[2] * v_d +
                                 r.jacobian[3] * r.jacobian[3] * v_f;
  V6_CHECK_CLOSE(r.v_cal_shared_diagonal, expected_shared, 1e-12,
                 "SPLIT shared diagonal == sum j_m^2 V_m");
  /* 经共同 master_id 通道逐 master 复算对角贡献，须与 shared diagonal 一致 */
  std::vector<double> c(1, 1.0);
  std::vector<double> ind(1, r.v_cal_independent);
  double via_channels = 0.0;
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kCommonMasterId;
    sh.common_master.master_id = cfg.bias_light_master.master_id;
    sh.common_master.alpha_m = 1.0;
    sh.common_master.master_variance = v_b;
    via_channels += astrocs::calibration::v6::evaluate_shared_gate(
                        c, ind, sh, std::vector<double>(1, r.jacobian[1]), {})
                        .shared_variance;
  }
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kCommonMasterId;
    sh.common_master.master_id = cfg.dark_master.master_id;
    sh.common_master.alpha_m = 1.0;
    sh.common_master.master_variance = v_d;
    via_channels += astrocs::calibration::v6::evaluate_shared_gate(
                        c, ind, sh, std::vector<double>(1, r.jacobian[2]), {})
                        .shared_variance;
  }
  {
    SharedSystematic sh;
    sh.kind = SharedRepresentationKind::kCommonMasterId;
    sh.common_master.master_id = cfg.flat_master.master_id;
    sh.common_master.alpha_m = 1.0;
    sh.common_master.master_variance = v_f;
    via_channels += astrocs::calibration::v6::evaluate_shared_gate(
                        c, ind, sh, std::vector<double>(1, r.jacobian[3]), {})
                        .shared_variance;
  }
  V6_CHECK_CLOSE(via_channels, r.v_cal_shared_diagonal, 1e-12,
                 "SPLIT channel sum == shared diagonal (no double count)");
}

/* ── ADJ-OBS-01-SHARED: 定种子 MC 独立真值 ─────────────────────────── */
void test_shared_mc(void) {
  const int n = 4;
  const std::vector<double> c(n, 0.25);
  const double v_ind = 0.05;
  const double v_shared = 0.02;
  const double analytic = astrocs::calibration::v6::naive_diagonal_variance(
                              c, std::vector<double>(n, v_ind)) +
                          v_shared;
  const double mc = astrocs::calibration::v6::oracle::shared_variance_mc(
      c, v_ind, v_shared, 200000, 20260915ULL);
  V6_CHECK_CLOSE(mc, analytic, 0.03, "ADJ-OBS-01-SHARED MC vs analytic joint");
  V6_CHECK(mc > astrocs::calibration::v6::naive_diagonal_variance(
                    c, std::vector<double>(n, v_ind)) * 1.0,
           "ADJ-OBS-01-SHARED MC detects ratio>1");
}

/* ── FZ-FORMULA-COV-PROP: covariance 记录门 ─────────────────────────── */
void test_covariance_record(void) {
  const CalConfig cfg = baseline_config();
  const CalResult r =
      astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
  std::string err;
  const CovarianceRecord rec =
      astrocs::calibration::v6::make_calibration_covariance_record(r);
  V6_CHECK(rec.propagation == "C_out = R C_in R^T", "COV-PROP canonical propagation");
  V6_CHECK(rec.variance_from == "actual_combination_coefficients",
           "COV-PROP variance_from actual coefficients");
  V6_CHECK(astrocs::calibration::v6::validate_covariance_record(rec, &err),
           "COV-PROP baseline record valid");

  CovarianceRecord bad = rec;
  bad.variance_from = "psfsw_robust_weight";
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP rejects weight-derived variance");
  bad = rec;
  bad.variance_from = "median_source_snr";
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP rejects median SNR source");
  bad = rec;
  bad.propagation = "C_out = C_in";
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP rejects wrong propagation");
  bad = rec;
  bad.representation = "bogus_enum";
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP rejects unknown representation");
  bad = rec;
  bad.avail = "unavailable";
  bad.unavailable_reason.clear();
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP unavailable needs whitelisted reason");
  bad = rec;
  bad.combination_coefficients.clear();
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP empty coefficients rejected");
  bad = rec;
  bad.input_covariance = "unavailable";
  V6_CHECK(!astrocs::calibration::v6::validate_covariance_record(bad, &err),
           "COV-PROP calibration requires declared input covariance");

  /* 三通道记录 representation 映射 */
  CalResult r2 = r;
  r2.shared_systematic.kind = SharedRepresentationKind::kLowRank;
  V6_CHECK(astrocs::calibration::v6::make_calibration_covariance_record(r2).representation ==
               "low_rank_factors",
           "COV-PROP lowrank representation mapping");
  r2.shared_systematic.kind = SharedRepresentationKind::kCommonMasterId;
  V6_CHECK(astrocs::calibration::v6::make_calibration_covariance_record(r2).representation ==
               "common_master",
           "COV-PROP common_master representation mapping");
  r2.shared_systematic.kind = SharedRepresentationKind::kCorrelationKernel;
  V6_CHECK(astrocs::calibration::v6::make_calibration_covariance_record(r2).representation ==
               "diagonal_variance_plus_correlation_kernel",
           "COV-PROP kernel representation mapping");
}

/* ── 负向注入: 违反冻结的实现必须使对应门变红 ───────────────────────── */
void test_fault_injections(void) {
  /* 每条: 先验证基线正确，再注入缺陷并断言检测到（门红）。 */
  struct FaultCase {
    const char* mode;
    const char* name;
  };

  /* 1. M-C2 同 master 重复计 */
  {
    auto correct = []() {
      const CalConfig cfg = baseline_config();
      const CalResult r =
          astrocs::calibration::v6::calibrate_pixel(cfg, baseline_pixel());
      const double folded = 0.04 * std::pow(-(1.0 - kAlpha) / kFlat, 2.0);
      const double bias_term =
          r.v_cal - (r.jacobian[0] * r.jacobian[0] * r.terms.light_frame() +
                     r.jacobian[2] * r.jacobian[2] *
                         (cfg.dark_master.self_variance() + r.terms.dark_photon) +
                     r.jacobian[3] * r.jacobian[3] * cfg.flat_master.self_variance());
      return std::fabs(bias_term - folded) < 1e-12;
    };
    V6_CHECK(correct(), "INJ baseline M-C2 folded");
    v6test::set_fault("double_bias_master");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ M-C2 double_bias_master -> gate red");
  }
  /* 2. M-S14 裁剪负值 */
  {
    auto correct = []() {
      CalConfig cfg = baseline_config();
      CalPixelInput px = baseline_pixel();
      px.r = -5.0;
      px.has_dark = false;
      px.has_bias_light = true;
      px.bias_light = 0.0;
      px.has_bias_dark = false;
      const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
      return r.y < 0.0;
    };
    V6_CHECK(correct(), "INJ baseline no-clip");
    v6test::set_fault("clip_negative");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ M-S14 clip_negative -> gate red");
  }
  /* 3. 加未声明 pedestal */
  {
    auto correct = []() {
      const CalResult r = astrocs::calibration::v6::calibrate_pixel(
          baseline_config(), baseline_pixel());
      return std::fabs(r.y - 6.0) < 1e-12;
    };
    V6_CHECK(correct(), "INJ baseline no-pedestal");
    v6test::set_fault("add_pedestal");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ add_pedestal -> gate red");
  }
  /* 4. FZ-CAL-FLOOR 删除 floor */
  {
    auto correct = []() {
      CalConfig cfg = baseline_config();
      CalPixelInput px = baseline_pixel();
      px.flat = 0.05;
      const CalResult r = astrocs::calibration::v6::calibrate_pixel(cfg, px);
      const double y_expected = (px.r - px.bias_light -
                                 cfg.alpha * (px.dark - px.bias_dark)) / 0.1;
      return r.flat_floor_applied && std::fabs(r.y - y_expected) < 1e-12;
    };
    V6_CHECK(correct(), "INJ baseline floor");
    v6test::set_fault("remove_floor");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ FZ-CAL-FLOOR remove_floor -> gate red");
  }
  /* 5. FZ-CAL-QUANTUM-DEFAULT 量化项为 0 */
  {
    auto correct = []() {
      const CalResult r = astrocs::calibration::v6::calibrate_pixel(
          baseline_config(), baseline_pixel());
      return std::fabs(r.terms.quantization - kQAdu * kQAdu / 12.0) < 1e-15;
    };
    V6_CHECK(correct(), "INJ baseline quantum");
    v6test::set_fault("quantum_zero");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ FZ-CAL-QUANTUM-DEFAULT quantum_zero -> gate red");
  }
  /* 6. M-C1 共享项当独立 */
  {
    const int n = 4;
    const std::vector<double> c(n, 0.25);
    const std::vector<double> diag(n, 0.05);
    const std::vector<double> alpha(n, 1.0);
    auto correct = []() {
      SharedSystematic sh;
      sh.kind = SharedRepresentationKind::kCommonMasterId;
      sh.common_master.master_id = "bias_master_A";
      sh.common_master.alpha_m = 1.0;
      sh.common_master.master_variance = 0.02;
      const auto g = astrocs::calibration::v6::evaluate_shared_gate(
          std::vector<double>(4, 0.25), std::vector<double>(4, 0.05), sh,
          std::vector<double>(4, 1.0), {});
      return g.ok && g.ratio > 1.0;
    };
    V6_CHECK(correct(), "INJ baseline shared detected");
    v6test::set_fault("drop_shared");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ M-C1 drop_shared -> gate red");
    (void)c;
    (void)diag;
    (void)alpha;
  }
  /* 7. M-S13 丢弃共享表示 */
  {
    auto correct = []() {
      const CalResult r = astrocs::calibration::v6::calibrate_pixel(
          baseline_config(), baseline_pixel());
      const CovarianceRecord rec =
          astrocs::calibration::v6::make_calibration_covariance_record(r);
      std::string err;
      return astrocs::calibration::v6::validate_covariance_record(rec, &err);
    };
    V6_CHECK(correct(), "INJ baseline representation");
    v6test::set_fault("drop_representation");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ M-S13 drop_representation -> gate red");
  }
  /* 8. M-C4 方差单位非 signal 平方 */
  {
    auto correct = []() {
      return astrocs::calibration::v6::unit_law_consistent(
          astrocs::calibration::v6::calibration_unit_law());
    };
    V6_CHECK(correct(), "INJ baseline unit law");
    v6test::set_fault("variance_unit_not_squared");
    const bool red = !correct();
    v6test::clear_fault();
    V6_CHECK(red, "INJ M-C4 variance_unit_not_squared -> gate red");
  }
}

}  // namespace

int main(void) {
  std::printf("== IMPL-P1-CAL-001 v6 calibration covariance tests ==\n");
  V6_CHECK(std::string(astrocs::calibration::v6::calibration_covariance_contract_id()) ==
               "ALG-P1-CAL-COV-001",
           "contract id ALG-P1-CAL-COV-001");
  test_formula_dense_oracle();
  test_same_master_folding();
  test_distinct_master();
  test_unit_law();
  test_no_clip_and_floor();
  test_fail_closed();
  test_independent_shared_split();
  test_shared_channels();
  test_shared_mc();
  test_covariance_record();
  test_fault_injections();

  const auto& l = v6test::ledger();
  std::printf("== executed=%d failed=%d ==\n", l.executed, l.failed);
  if (l.executed == 0) {
    std::printf("ZERO CASES -> FAIL\n");
    return 2;
  }
  if (l.failed != 0) {
    for (const auto& f : l.failures) std::printf("  - %s\n", f.c_str());
    return 1;
  }
  return 0;
}
