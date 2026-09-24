/* ============================================================================
 * ACSD V6 Phase1 calibration covariance 实现 — ALG-P1-CAL-COV-001
 * 头文件: lib/algorithms/calibration/include/astrocs/calibration/v6_calibration_covariance.h
 *
 * 纪律: 不接线 Phase session；不改 legacy ac_* 实现；不发明 DI-03（共享低秩/
 *       相关核数据面实例化）未冻结的核函数或秩上限；所有 fail-closed 按
 *       docs/contracts/DATA_SEMANTICS.md 实现。
 *
 * 测试用故障注入: 仅当环境变量 ASTROCS_V6_CAL_FAULT 被显式设置时改变行为，
 *       默认（未设置）为严格正确实现。对齐仓库既有先例 ASTROCS_RT001_FAULT。
 * ==========================================================================*/

#include "astrocs/calibration/v6_calibration_covariance.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace astrocs {
namespace calibration {
namespace v6 {

namespace {

/* ── 测试用故障注入钩子（默认无） ─────────────────────────────────────── */
std::string fault_mode() {
  const char* v = std::getenv("ASTROCS_V6_CAL_FAULT");
  return (v != nullptr) ? std::string(v) : std::string();
}
bool fault_is(const char* name) { return fault_mode() == name; }

bool is_finite(double x) { return std::isfinite(x); }

bool is_allowed_representation(const std::string& r) {
  return r == "diagonal_variance" ||
         r == "diagonal_variance_plus_correlation_kernel" ||
         r == "low_rank_factors" || r == "common_master" ||
         r == "unavailable";
}

bool is_allowed_unavailable_reason(const std::string& r) {
  return r == "shared_systematic_unrepresentable" ||
         r == "input_covariance_missing" || r == "correlation_kernel_missing" ||
         r == "diagonal_deficit_gate_failed" ||
         r == "operator_not_reconstructable";
}

bool is_allowed_variance_from(const std::string& v) {
  return v == "combination_coefficients" ||
         v == "linear_combination_coefficients" ||
         v == "actual_combination_coefficients";
}

/* 退役对象（PSFSW-RETIRE-01；负责人裁决「只要纯净信号/噪声的信噪比……绝对标定」）：
 * psfsw_robust_weight **不是现行对象**（ASTROCS_DESIGN.md §3.1 订正后；
 * docs/design/UNIFIED_MODEL.md:58；统一对象 14→13，CHG-2026-09-20-PSFSW-RETIRE）。
 * 旧产品若在 variance_from 声明该对象 ⇒ 显式拒绝 + 迁移提示，不得静默接受，
 * 也不得再把它当作"在役的相对复合权重"（它连对象都不存在了）。 */
bool is_retired_canonical_object(const std::string& token) {
  return token == "psfsw_robust_weight";
}

/* 冻结的 flat 归一除法下限（FZ-CAL-FLOOR，继承 SCI-CAL-001）。 */
constexpr double kFlatFloor = 0.1;
/* 共享项检出阈值：joint/naive 必须严格 > 1（ADJ-OBS-01-SHARED）。 */
constexpr double kSharedDetectionEps = 1e-9;

}  // namespace

const char* calibration_covariance_contract_id() { return "ALG-P1-CAL-COV-001"; }
const char* fault_injection_env_var() { return "ASTROCS_V6_CAL_FAULT"; }

const char* to_string(CalStatus status) {
  switch (status) {
    case CalStatus::kOk: return "ok";
    case CalStatus::kRejected: return "rejected";
    case CalStatus::kUnavailable: return "unavailable";
  }
  return "unknown";
}

const char* to_string(CalReason reason) {
  switch (reason) {
    case CalReason::kNone: return "none";
    case CalReason::kMissingGainOrReadNoise: return "missing_gain_or_read_noise";
    case CalReason::kMissingMasterIdentity: return "missing_master_identity";
    case CalReason::kNonPositiveFlat: return "non_positive_flat";
    case CalReason::kNonFiniteInput: return "non_finite_input";
    case CalReason::kSharedUnrepresentable: return "shared_systematic_unrepresentable";
    case CalReason::kSharedNotDetected: return "shared_systematic_not_detected";
    case CalReason::kInvalidConfig: return "invalid_config";
  }
  return "unknown";
}

const char* to_string(VarianceSource source) {
  switch (source) {
    case VarianceSource::kPhysical: return "physical";
    case VarianceSource::kEmpiricalMadFallback: return "empirical_mad_fallback";
  }
  return "unknown";
}

const char* to_string(SharedRepresentationKind kind) {
  switch (kind) {
    case SharedRepresentationKind::kNone: return "none";
    case SharedRepresentationKind::kLowRank: return "lowrank";
    case SharedRepresentationKind::kCorrelationKernel: return "correlation_kernel";
    case SharedRepresentationKind::kCommonMasterId: return "common_master_id";
  }
  return "unknown";
}

/* ───────────────────────────── master 方差 ───────────────────────────── */

double MasterIdentity::self_variance() const {
  if (v_single_frame < 0.0 || common_mode_variance < 0.0 || n_combined < 0) {
    return -1.0;
  }
  /* V(master)=V_single_frame/N_combined + common mode：给了 V_single_frame
   * 就必须记录 N_combined（ALG-P1-001 §2.3），否则自身方差不可判。 */
  if (v_single_frame > 0.0 && n_combined <= 0) {
    return -1.0;
  }
  double v = 0.0;
  if (n_combined > 0) {
    v = v_single_frame / static_cast<double>(n_combined);
  }
  if (common_mode_declared) {
    v += common_mode_variance;
  }
  return v;
}

bool MasterIdentity::identity_complete() const {
  return !master_id.empty() && !normalization_version.empty() && !units.empty();
}

double RandomTermVariances::light_frame() const {
  return read_noise + photon_light + quantization;
}

RandomTermVariances independent_random_terms(double r_adu, double d_adu,
                                             const DetectorMetadata& md) {
  RandomTermVariances t;
  const double q_adu = md.quantum_declared ? md.q_adu : 1.0; /* 缺省 1 ADU */
  t.quantization = q_adu * q_adu / 12.0;
  if (fault_is("quantum_zero")) {
    t.quantization = 0.0;
  }
  if (md.has_gain && md.gain > 0.0) {
    if (md.has_read_noise) {
      const double rn = md.read_noise_e / md.gain;
      t.read_noise = rn * rn;
    }
    t.photon_light = (r_adu > 0.0 ? r_adu : 0.0) / md.gain;
    t.dark_photon = (d_adu > 0.0 ? d_adu : 0.0) / md.gain;
  }
  return t;
}

/* ──────────────────────────── 共享系统项 ─────────────────────────────── */

bool SharedSystematic::has_shared_term() const {
  return kind != SharedRepresentationKind::kNone;
}

bool SharedSystematic::structurally_representable() const {
  switch (kind) {
    case SharedRepresentationKind::kNone:
      return false;
    case SharedRepresentationKind::kLowRank:
      return low_rank.rank > 0 && !low_rank.factor.empty();
    case SharedRepresentationKind::kCorrelationKernel:
      return !correlation_kernel.kernel_id.empty() &&
             !correlation_kernel.kernel_version.empty() &&
             correlation_kernel.scale > 0.0;
    case SharedRepresentationKind::kCommonMasterId:
      return !common_master.master_id.empty() &&
             common_master.master_variance >= 0.0 && is_finite(common_master.alpha_m);
  }
  return false;
}

/* ───────────────────────────── 校准主入口 ───────────────────────────── */

double diagonal_quadratic_variance(const double jacobian[4], double v_r,
                                   double v_b, double v_d, double v_f) {
  /* 只做二次型；不裁剪、不加 pedestal（CAL-NO-CLIP）。 */
  return jacobian[0] * jacobian[0] * v_r + jacobian[1] * jacobian[1] * v_b +
         jacobian[2] * jacobian[2] * v_d + jacobian[3] * jacobian[3] * v_f;
}

CalResult calibrate_pixel(const CalConfig& cfg, const CalPixelInput& px) {
  CalResult out;
  out.variance_source = cfg.detector.variance_source;

  /* ── 输入有限性检查 ──────────────────────────────────────────────── */
  if (!is_finite(px.r) || !is_finite(px.bias_light) || !is_finite(px.bias_dark) ||
      !is_finite(px.dark)) {
    out.status = CalStatus::kRejected;
    out.reason = CalReason::kNonFiniteInput;
    return out;
  }

  /* ── flat：f<=0 或非有限 -> REJECT（不得静默 floor 造值） ──────────── */
  double denom = 1.0;
  bool flat_used = px.has_flat;
  if (flat_used) {
    if (!is_finite(px.flat) || px.flat <= 0.0) {
      out.status = CalStatus::kRejected;
      out.reason = CalReason::kNonPositiveFlat;
      return out;
    }
    denom = px.flat;
    if (!fault_is("remove_floor") && px.flat < kFlatFloor) {
      denom = kFlatFloor; /* FZ-CAL-FLOOR: max(f_p, 0.1) */
      out.flat_floor_applied = true;
    }
  }

  /* ── master 身份（master_id / 归一版本 / 单位） ───────────────────── */
  const bool dark_opt_explicit = (cfg.dark_opt == DarkOption::kExplicitBiasDark);
  /* BIAS-001：bias 在**两分支都进入算术**——标准式 (r-b-alpha*d)/f 只含光路 bias；
   * 兼容式 (r-b-alpha*(d-b))/f 同时含光路与暗路 bias（可折叠为同一共享 master）。 */
  const bool uses_bias_light = px.has_bias_light;
  const bool uses_bias_dark = dark_opt_explicit && px.has_bias_dark;
  const bool uses_dark = px.has_dark;
  const bool uses_flat = flat_used;

  auto require_identity = [&out](const MasterIdentity& m, const char* role) -> bool {
    if (!m.identity_complete()) {
      out.status = CalStatus::kUnavailable;
      out.reason = CalReason::kMissingMasterIdentity;
      (void)role;
      return false;
    }
    return true;
  };
  if (uses_bias_light && !require_identity(cfg.bias_light_master, "bias_light")) return out;
  if (uses_bias_dark && !require_identity(cfg.bias_dark_master, "bias_dark")) return out;
  if (uses_dark && !require_identity(cfg.dark_master, "dark")) return out;
  if (uses_flat && !require_identity(cfg.flat_master, "flat")) return out;

  /* ── 共享项不可表示且无系统误差预算 -> unavailable ─────────────────── */
  if (cfg.shared.has_shared_term() && !cfg.shared.structurally_representable() &&
      !cfg.shared.system_error_budget_declared) {
    out.status = CalStatus::kUnavailable;
    out.reason = CalReason::kSharedUnrepresentable;
    out.shared_systematic = cfg.shared;
    return out;
  }

  /* ── 探测器响应：缺 gain/read_noise 且未声明 fallback -> unavailable ── */
  const bool gain_ok = cfg.detector.has_gain && cfg.detector.gain > 0.0;
  const bool read_ok = cfg.detector.has_read_noise;
  const bool fallback = (cfg.detector.variance_source == VarianceSource::kEmpiricalMadFallback);
  if (!gain_ok || !read_ok) {
    if (!fallback || !px.has_empirical_variance ||
        !is_finite(px.empirical_variance) || px.empirical_variance < 0.0) {
      out.status = CalStatus::kUnavailable;
      out.reason = CalReason::kMissingGainOrReadNoise;
      return out;
    }
  }
  if (cfg.detector.has_gain && cfg.detector.gain <= 0.0) {
    out.status = CalStatus::kRejected;
    out.reason = CalReason::kInvalidConfig;
    return out;
  }
  if (cfg.detector.quantum_declared && cfg.detector.q_adu <= 0.0) {
    out.status = CalStatus::kRejected;
    out.reason = CalReason::kInvalidConfig;
    return out;
  }
  out.quantum_default_applied = !cfg.detector.quantum_declared;

  /* ── 信号（原样继承 SCI-CAL-001 §5 口径；不裁剪/不加 pedestal） ─────── */
  double y = 0.0;
  if (dark_opt_explicit) {
    /* 只有一个 bias（光路或暗路）时按同一共享 master 用于两处（折叠）；两个
     * 且 master_id 不同时为独立项（OI-02 精确形式）。 */
    const double b_lin = px.has_bias_light ? px.bias_light
                         : (px.has_bias_dark ? px.bias_dark : 0.0);
    const double b_dk = px.has_bias_dark ? px.bias_dark : b_lin;
    const double d = px.has_dark ? px.dark : 0.0;
    double num = px.r - b_lin;
    if (px.has_dark) {
      num -= cfg.alpha * (d - b_dk);
    }
    y = num / denom;
  } else {
    /* 标准式（BIAS-001）：master_dark 已减 bias -> y = (r - b - alpha*d) / f */
    const double d = px.has_dark ? px.dark : 0.0;
    const double b = px.has_bias_light ? px.bias_light : 0.0;
    y = (px.r - b - cfg.alpha * d) / denom;
  }
  if (fault_is("clip_negative") && y < 0.0) {
    y = 0.0;
  }
  if (fault_is("add_pedestal")) {
    y += 100.0;
  }
  out.y = y;

  /* ── 方向导数（BIAS-001 订正后）─────────────────────────────────────
   *   标准式 (dark_opt=0): J = [ 1/f,   -1/f,     -alpha/f, -y/f ]
   *   兼容式 (dark_opt=1): J = [ 1/f, -(1-alpha)/f, -alpha/f, -y/f ]（同 master 折叠）
   * 缺省 master 的系数为 0（不使用即不进入 J 与方差）。 */
  double jac[4] = {0.0, 0.0, 0.0, 0.0};
  const bool bias_present_any = uses_bias_light || uses_bias_dark;
  const bool same_bias_master =
      (uses_bias_light && uses_bias_dark)
          ? (cfg.bias_light_master.master_id == cfg.bias_dark_master.master_id)
          : bias_present_any;
  out.same_bias_master_folded = dark_opt_explicit && same_bias_master;

  const double j_r = 1.0 / denom;
  double j_b_folded = 0.0; /* 折叠后 bias 系数（同 master） */
  double j_b_light = 0.0;
  double j_b_dark = 0.0;
  if (dark_opt_explicit) {
    if (same_bias_master) {
      j_b_folded = -(1.0 - cfg.alpha) / denom;
    } else {
      if (uses_bias_light) j_b_light = -1.0 / denom;
      if (uses_bias_dark) j_b_dark = cfg.alpha / denom;
    }
  } else if (uses_bias_light) {
    /* 标准式：bias 只出现一次（光路），系数 -1/f；dark 视作已减 bias */
    j_b_light = -1.0 / denom;
  }
  /* K(=alpha) 在两分支都作用于 dark（BIAS-001：旧实现标准式强制 K=1） */
  const double j_d = uses_dark ? -cfg.alpha / denom : 0.0;
  double j_f = 0.0;
  if (flat_used && !out.flat_floor_applied) {
    /* 只在未触发 floor 区域 y 才显式依赖 f；floor 区域 d y / d f = 0。 */
    j_f = -y / px.flat;
  }
  jac[0] = j_r;
  jac[1] = same_bias_master ? j_b_folded : j_b_light;
  jac[2] = j_d;
  jac[3] = j_f;
  for (int i = 0; i < 4; ++i) out.jacobian[i] = jac[i];

  /* ── 独立随机项 ──────────────────────────────────────────────────── */
  out.terms = independent_random_terms(px.r, px.dark, cfg.detector);
  double v_r = out.terms.light_frame();
  if (fallback && !gain_ok) {
    v_r = px.empirical_variance; /* empirical_mad_fallback 替代物理光路项 */
  }

  /* ── master 自身方差 ─────────────────────────────────────────────── */
  double v_b_light = 0.0, v_b_dark = 0.0, v_dark = 0.0, v_flat = 0.0;
  if (uses_bias_light) {
    v_b_light = cfg.bias_light_master.self_variance();
    if (v_b_light < 0.0) {
      out.status = CalStatus::kRejected;
      out.reason = CalReason::kInvalidConfig;
      return out;
    }
  }
  if (uses_bias_dark) {
    v_b_dark = cfg.bias_dark_master.self_variance();
    if (v_b_dark < 0.0) {
      out.status = CalStatus::kRejected;
      out.reason = CalReason::kInvalidConfig;
      return out;
    }
  }
  if (uses_dark) {
    v_dark = cfg.dark_master.self_variance();
    if (v_dark < 0.0) {
      out.status = CalStatus::kRejected;
      out.reason = CalReason::kInvalidConfig;
      return out;
    }
  }
  if (uses_flat) {
    v_flat = cfg.flat_master.self_variance();
    if (v_flat < 0.0) {
      out.status = CalStatus::kRejected;
      out.reason = CalReason::kInvalidConfig;
      return out;
    }
  }

  /* ── C_cal = J C_in J^T（对角情形逐像素逐项） ────────────────────── */
  double v_cal = 0.0;
  v_cal += j_r * j_r * v_r;
  double v_independent = j_r * j_r * v_r;
  if (dark_opt_explicit) {
    const double v_b_folded = uses_bias_light ? v_b_light : v_b_dark;
    if (same_bias_master) {
      if (fault_is("double_bias_master")) {
        /* 注入缺陷：同 master 重复计为两个独立项（M-C2）。 */
        v_cal += (1.0 / (denom * denom)) * v_b_folded;
        v_cal += (cfg.alpha * cfg.alpha / (denom * denom)) * v_b_folded;
      } else {
        v_cal += j_b_folded * j_b_folded * v_b_folded;
      }
    } else {
      v_cal += j_b_light * j_b_light * v_b_light;
      v_cal += j_b_dark * j_b_dark * v_b_dark;
    }
  } else if (uses_bias_light) {
    /* 标准式：bias 独立项系数 -1/f（无折叠，bias 只出现一次） */
    v_cal += j_b_light * j_b_light * v_b_light;
  }
  if (uses_dark) {
    v_cal += j_d * j_d * v_dark;
    v_cal += j_d * j_d * out.terms.dark_photon;
    v_independent += j_d * j_d * out.terms.dark_photon;
  }
  if (flat_used) {
    v_cal += j_f * j_f * v_flat;
  }
  out.v_cal = v_cal;
  out.v_cal_independent = v_independent;
  out.v_cal_shared_diagonal = v_cal - v_independent;
  out.sigma_cal = std::sqrt(v_cal);

  /* ── master_ids 记录（含 combine 规则与 N_combined） ─────────────── */
  auto push_master = [&out](const char* role, const MasterIdentity& m, bool folded) {
    MasterIdRecord r;
    r.role = role;
    r.master_id = m.master_id;
    r.combine_rule = m.combine_rule;
    r.n_combined = m.n_combined;
    r.self_variance = m.self_variance();
    r.folded_same_master = folded;
    out.master_ids.push_back(r);
  };
  const bool bias_folded = dark_opt_explicit && same_bias_master;
  if (bias_folded && bias_present_any) {
    /* 同一 master 只登记一次（ADJ-OBS-01：共享 master 只进一次通道）。 */
    const MasterIdentity& m =
        uses_bias_light ? cfg.bias_light_master : cfg.bias_dark_master;
    push_master("bias_shared", m, true);
  } else {
    if (uses_bias_light) push_master("bias_light", cfg.bias_light_master, false);
    if (uses_bias_dark) push_master("bias_dark", cfg.bias_dark_master, false);
  }
  if (uses_dark) push_master("dark", cfg.dark_master, false);
  if (uses_flat) push_master("flat", cfg.flat_master, false);
  out.shared_systematic = cfg.shared;

  out.status = CalStatus::kOk;
  out.reason = CalReason::kNone;
  return out;
}

std::vector<CalResult> calibrate_pixels(const CalConfig& cfg,
                                        const std::vector<CalPixelInput>& pixels) {
  std::vector<CalResult> out;
  out.reserve(pixels.size());
  for (const auto& px : pixels) {
    out.push_back(calibrate_pixel(cfg, px));
  }
  return out;
}

/* ────────────────────────── 共享项二次型 ─────────────────────────────── */

double naive_diagonal_variance(const std::vector<double>& c,
                               const std::vector<double>& diagonal_variance) {
  if (c.size() != diagonal_variance.size()) return -1.0;
  double v = 0.0;
  for (std::size_t i = 0; i < c.size(); ++i) {
    v += c[i] * c[i] * diagonal_variance[i];
  }
  return v;
}

double low_rank_variance(const std::vector<double>& c,
                         const std::vector<double>& L, int n_pix, int rank) {
  if (n_pix <= 0 || rank <= 0) return -1.0;
  if (static_cast<int>(c.size()) != n_pix) return -1.0;
  if (static_cast<std::size_t>(n_pix) * static_cast<std::size_t>(rank) != L.size()) {
    return -1.0;
  }
  double total = 0.0;
  for (int r = 0; r < rank; ++r) {
    double acc = 0.0;
    for (int p = 0; p < n_pix; ++p) {
      acc += c[static_cast<std::size_t>(p)] *
             L[static_cast<std::size_t>(p) * static_cast<std::size_t>(rank) +
               static_cast<std::size_t>(r)];
    }
    total += acc * acc;
  }
  return total;
}

double common_master_variance(const std::vector<double>& c,
                              const std::vector<double>& alpha_per_pixel,
                              double master_variance) {
  if (c.size() != alpha_per_pixel.size()) return -1.0;
  if (master_variance < 0.0) return -1.0;
  double acc = 0.0;
  for (std::size_t i = 0; i < c.size(); ++i) {
    acc += c[i] * alpha_per_pixel[i];
  }
  return acc * acc * master_variance;
}

double correlation_kernel_variance(const std::vector<double>& c,
                                   const std::vector<double>& K, int n_pix,
                                   double scale) {
  if (n_pix <= 0 || scale <= 0.0) return -1.0;
  if (static_cast<int>(c.size()) != n_pix) return -1.0;
  if (K.size() != static_cast<std::size_t>(n_pix) * static_cast<std::size_t>(n_pix)) {
    return -1.0;
  }
  double qf = 0.0;
  for (int p = 0; p < n_pix; ++p) {
    for (int q = 0; q < n_pix; ++q) {
      qf += c[static_cast<std::size_t>(p)] * c[static_cast<std::size_t>(q)] *
            K[static_cast<std::size_t>(p) * static_cast<std::size_t>(n_pix) +
              static_cast<std::size_t>(q)];
    }
  }
  return scale * scale * qf;
}

SharedGateResult evaluate_shared_gate(const std::vector<double>& c,
                                      const std::vector<double>& diagonal_variance,
                                      const SharedSystematic& shared,
                                      const std::vector<double>& alpha_per_pixel,
                                      const std::vector<double>& correlation_matrix) {
  SharedGateResult res;
  const int n_pix = static_cast<int>(c.size());
  const double naive = naive_diagonal_variance(c, diagonal_variance);
  res.naive_variance = (naive < 0.0) ? 0.0 : naive;
  res.joint_variance = res.naive_variance;
  res.ratio = 1.0;
  res.ok = true;
  res.reason = CalReason::kNone;

  if (!shared.has_shared_term()) {
    res.shared_present = false;
    return res;
  }
  res.shared_present = true;

  if (fault_is("drop_shared")) {
    /* 注入缺陷：共享系统项按独立项处理（M-C1）。 */
    res.shared_variance = 0.0;
    res.joint_variance = res.naive_variance;
    res.ratio = 1.0;
    res.ok = false;
    res.reason = CalReason::kSharedNotDetected;
    return res;
  }

  double shared_variance = 0.0;
  bool representable = shared.structurally_representable();
  if (representable) {
    switch (shared.kind) {
      case SharedRepresentationKind::kLowRank:
        shared_variance = low_rank_variance(c, shared.low_rank.factor, n_pix,
                                            shared.low_rank.rank);
        break;
      case SharedRepresentationKind::kCommonMasterId:
        shared_variance = common_master_variance(c, alpha_per_pixel,
                                                 shared.common_master.master_variance);
        break;
      case SharedRepresentationKind::kCorrelationKernel:
        if (!correlation_matrix.empty()) {
          shared_variance = correlation_kernel_variance(
              c, correlation_matrix, n_pix, shared.correlation_kernel.scale);
        } else {
          /* DI-03 OPEN：核函数数据面未冻结，生产不发明核。 */
          shared_variance = -1.0;
        }
        break;
      case SharedRepresentationKind::kNone:
        shared_variance = 0.0;
        break;
    }
    if (shared_variance < 0.0) representable = false;
  }

  if (!representable) {
    if (shared.system_error_budget_declared) {
      /* 无法表示但已声明系统误差预算：以预算方差兜底（FZ-PROV-SHARED-SYSTEMATIC）。 */
      shared_variance = shared.system_error_budget_variance;
    } else {
      res.ok = false;
      res.reason = CalReason::kSharedUnrepresentable;
      res.shared_variance = 0.0;
      return res;
    }
  }

  res.shared_variance = shared_variance;
  res.joint_variance = res.naive_variance + shared_variance;
  res.ratio = (res.naive_variance > 0.0)
                  ? res.joint_variance / res.naive_variance
                  : std::numeric_limits<double>::infinity();
  if (!(res.ratio > 1.0 + kSharedDetectionEps)) {
    /* 共享项存在却未使联合方差严格大于独立求和 -> 按独立处理，REJECT。 */
    res.ok = false;
    res.reason = CalReason::kSharedNotDetected;
  }
  return res;
}

/* ─────────────────────────── 单位与记录 ──────────────────────────────── */

UnitLaw calibration_unit_law(void) {
  UnitLaw law;
  law.signal_unit = "ADU";
  law.variance_unit = "ADU^2";
  law.ivar_unit = "ADU^-2";
  if (fault_is("variance_unit_not_squared")) {
    law.variance_unit = "ADU"; /* 注入缺陷：M-C4 */
  }
  return law;
}

bool unit_law_consistent(const UnitLaw& law) {
  if (law.signal_unit == "ADU") {
    return law.variance_unit == "ADU^2" && law.ivar_unit == "ADU^-2";
  }
  if (law.signal_unit == "ADU/sr") {
    return law.variance_unit == "ADU^2/sr^2" && law.ivar_unit == "sr^2/ADU^2";
  }
  if (law.signal_unit == "ADU/px") {
    return law.variance_unit == "ADU^2/px^2" && law.ivar_unit == "px^2/ADU^2";
  }
  return false;
}

CovarianceRecord make_calibration_covariance_record(const CalResult& result) {
  CovarianceRecord rec;
  rec.propagation = "C_out = R C_in R^T";
  rec.variance_from = "actual_combination_coefficients";
  rec.input_covariance = "declared";
  rec.combination_coefficients.assign(result.jacobian, result.jacobian + 4);
  rec.avail = (result.status == CalStatus::kOk) ? "available" : "unavailable";

  switch (result.shared_systematic.kind) {
    case SharedRepresentationKind::kNone:
      rec.representation = "diagonal_variance";
      break;
    case SharedRepresentationKind::kLowRank:
      rec.representation = "low_rank_factors";
      break;
    case SharedRepresentationKind::kCommonMasterId:
      rec.representation = "common_master";
      break;
    case SharedRepresentationKind::kCorrelationKernel:
      rec.representation = "diagonal_variance_plus_correlation_kernel";
      break;
  }
  if (fault_is("drop_representation")) {
    rec.representation = "unsupported_none"; /* 注入缺陷：M-S13 */
  }

  if (result.status != CalStatus::kOk) {
    rec.representation = "unavailable";
    switch (result.reason) {
      case CalReason::kSharedUnrepresentable:
        rec.unavailable_reason = "shared_systematic_unrepresentable";
        break;
      case CalReason::kMissingGainOrReadNoise:
      case CalReason::kMissingMasterIdentity:
        rec.unavailable_reason = "input_covariance_missing";
        break;
      default:
        rec.unavailable_reason = "operator_not_reconstructable";
        break;
    }
  }
  return rec;
}

bool is_forbidden_variance_source(const std::string& token) {
  // PSFSW-RETIRE-01：其中 "psfsw_robust_weight" / "psfsw" 是**退役对象的拒绝面**
  // （旧产品声明该对象 ⇒ REJECT），不是"在役的相对复合权重"；两项不得按
  // "残留清理"删除（删除即变成静默接受）。退役对象另有更可诊断的拒绝路径，
  // 见 is_retired_canonical_object / validate_covariance_record。
  static const char* kForbidden[] = {
      "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
      "support", "support_area", "coverage", "coverage_area", "fwhm",
      "psf_fwhm", "median_fwhm", "source_fwhm", "residual", "psf_residual",
      "psf_fit_residual", "fit_residual", "weight", "psfsw_robust_weight",
      "psfsw", "effective_psf", "source_snr", "ivar", "inverse_variance",
      "fisher", "fisher_information"};
  for (const char* t : kForbidden) {
    if (token == t) return true;
  }
  return false;
}

bool validate_covariance_record(const CovarianceRecord& rec, std::string* error) {
  auto fail = [error](const std::string& msg) {
    if (error != nullptr) *error = msg;
    return false;
  };
  if (rec.propagation != "C_out = R C_in R^T") {
    return fail("propagation must be C_out = R C_in R^T");
  }
  if (is_retired_canonical_object(rec.variance_from)) {
    // 退役对象优先报出（可诊断 + 迁移提示），仍是 fail-closed 的拒绝。
    return fail("variance_from declares retired canonical object 'psfsw_robust_weight' "
                "(not a current object: ASTROCS_DESIGN.md 3.1; UNIFIED_MODEL.md:58); "
                "migration: variance_from=actual_combination_coefficients with "
                "C_out = R C_in R^T");
  }
  if (!is_allowed_variance_from(rec.variance_from) ||
      is_forbidden_variance_source(rec.variance_from)) {
    return fail("variance_from invalid or forbidden");
  }
  if (!is_allowed_representation(rec.representation)) {
    return fail("representation not in covariance.v1 enum");
  }
  if (rec.avail != "available" && rec.avail != "unavailable") {
    return fail("avail must be available|unavailable");
  }
  if (rec.representation == "unavailable" && rec.avail != "unavailable") {
    return fail("representation=unavailable requires avail=unavailable");
  }
  if (rec.avail == "unavailable" && !is_allowed_unavailable_reason(rec.unavailable_reason)) {
    return fail("unavailable_reason not in whitelist");
  }
  if (rec.representation != "unavailable" && rec.combination_coefficients.empty() &&
      !(rec.operator_reconstructable && !rec.operator_summary_ref.empty())) {
    return fail("empty combination_coefficients without operator descriptor");
  }
  if (rec.input_covariance != "provided" && rec.input_covariance != "declared" &&
      rec.input_covariance != "unavailable") {
    return fail("input_covariance must be provided|declared|unavailable");
  }
  if (rec.input_covariance == "unavailable") {
    return fail("calibration requires declared/provided input_covariance");
  }
  if (error != nullptr) error->clear();
  return true;
}

}  // namespace v6
}  // namespace calibration
}  // namespace astrocs
