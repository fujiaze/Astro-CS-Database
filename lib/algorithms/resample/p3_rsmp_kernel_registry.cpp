// lib/algorithms/resample/p3_rsmp_kernel_registry.cpp
// 采样核 registry（FZ-P3-KERNEL-REGISTRY；ALG-P3-001_KERNEL_REGISTRY §1-§5）。
// 冻结：核是产品语义；未注册/未验证核进生产即 REJECT；nearest 仅 mask/诊断/显式选择；
//       bilinear_4quad 须独立 Oracle + 误差界 + 边界定义；高阶核各自注册带 Oracle。
#include "p3_rsmp.h"

namespace astrocs {
namespace p3rsmp {

const char* to_string(KernelStatus v) {
  switch (v) {
    case KernelStatus::RegisteredWithOracle: return "registered_with_oracle";
    case KernelStatus::RegisteredRestricted: return "registered_restricted";
    case KernelStatus::RequiresRegistration: return "requires_registration";
  }
  return "unknown";
}
const char* to_string(KernelUse v) {
  switch (v) {
    case KernelUse::ContinuousField: return "continuous_field";
    case KernelUse::DiscreteMask: return "discrete_mask";
    case KernelUse::Diagnostics: return "diagnostics";
    case KernelUse::ExplicitUserSelection: return "explicit_user_selection";
  }
  return "unknown";
}
const char* to_string(BoundaryPolicy v) {
  switch (v) {
    case BoundaryPolicy::MissingIsNaN: return "missing_is_nan";
    case BoundaryPolicy::Undefined: return "undefined";
  }
  return "unknown";
}
const char* to_string(OracleKind v) {
  switch (v) {
    case OracleKind::None: return "none";
    case OracleKind::Structural: return "structural";
    case OracleKind::Analytic: return "analytic";
    case OracleKind::MonteCarlo: return "monte_carlo";
  }
  return "unknown";
}

bool KernelDescriptor::allows(KernelUse u) const {
  for (KernelUse v : allowed_uses) {
    if (v == u) return true;
  }
  return false;
}

void KernelRegistry::add(const KernelDescriptor& k) { kernels_.push_back(k); }

const KernelDescriptor* KernelRegistry::find(const std::string& kernel_id) const {
  for (const auto& k : kernels_) {
    if (k.kernel_id == kernel_id) return &k;
  }
  return nullptr;
}

GateResult KernelRegistry::validate_registration(const KernelDescriptor& k) const {
  // 注册前置：唯一 id / 用途 / 语义 / 误差界 / 边界 / 独立 Oracle / 负向 mutation。
  if (k.kernel_id.empty()) {
    return GateResult{Status::Reject, "G-P3-KRN-03", "kernel_id_empty"};
  }
  if (k.status == KernelStatus::RequiresRegistration) {
    return GateResult{Status::Reject, "G-P3-KRN-01",
                      "kernel_not_registered:" + k.kernel_id};
  }
  if (k.allowed_uses.empty()) {
    return GateResult{Status::Reject, "G-P3-KRN-03", "kernel_missing_allowed_uses"};
  }
  if (k.normalization == KernelNormalization::Discrete) {
    // 离散核不要求连续场误差界，但边界必须定义。
    if (!k.boundary_defined || k.boundary_definition.empty()) {
      return GateResult{Status::Reject, "G-P3-KRN-03",
                        "registered_kernel_missing_boundary_definition:" + k.kernel_id};
    }
    return GateResult{Status::Ok, "", ""};
  }
  if (!k.error_bound_present || k.error_bound_expression.empty()) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "registered_kernel_missing_error_bound:" + k.kernel_id};
  }
  if (!k.boundary_defined || k.boundary_definition.empty() ||
      k.boundary_policy == BoundaryPolicy::Undefined) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "registered_kernel_missing_boundary_definition:" + k.kernel_id};
  }
  const KernelOracleEvidence& o = k.oracle;
  if (o.kind == OracleKind::None || !o.ok || !o.independent || o.oracle_id.empty()) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "registered_kernel_missing_independent_oracle:" + k.kernel_id};
  }
  // 连续场核必须给出解析/ MC 数值 Oracle（结构化仅限无权重算术的离散核）。
  if (o.kind == OracleKind::Structural) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "continuous_kernel_requires_analytic_or_mc_oracle:" + k.kernel_id};
  }
  if (!(o.max_interp_err <= o.analytic_bound)) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "kernel_oracle_error_exceeds_bound:" + k.kernel_id};
  }
  if (!o.boundary_fail_closed) {
    return GateResult{Status::Reject, "G-P3-KRN-03",
                      "kernel_oracle_boundary_not_fail_closed:" + k.kernel_id};
  }
  return GateResult{Status::Ok, "", ""};
}

GateResult KernelRegistry::admit(const std::string& kernel_id, KernelUse use, P3Mode mode) const {
  const KernelDescriptor* k = find(kernel_id);
  if (k == nullptr) {
    return GateResult{Status::Reject, "G-P3-KRN-01", "unknown_kernel_id:" + kernel_id};
  }
  if (k->status == KernelStatus::RequiresRegistration) {
    return GateResult{Status::Reject, "G-P3-KRN-01",
                      "kernel_requires_registration:" + kernel_id};
  }
  // 注册结构门（缺 Oracle/误差界/边界）
  GateResult reg = validate_registration(*k);
  if (!reg.ok()) return reg;

  // nearest 不得作连续科学场默认/科学采样（G-P3-KRN-02）
  if (kernel_id == "nearest" && use == KernelUse::ContinuousField) {
    return GateResult{Status::Reject, "G-P3-KRN-02",
                      "nearest_not_allowed_for_continuous_field"};
  }
  if (!k->allows(use)) {
    return GateResult{Status::Reject, "G-P3-KRN-02",
                      "kernel_use_not_allowed:" + kernel_id + ":" + to_string(use)};
  }
  // 语义匹配：SB 需行归一能力；point_source_flux 需列归一能力。
  if (mode == P3Mode::SurfaceBrightness && !k->supports_row_normalized) {
    return GateResult{Status::Reject, "G-P3-KRN-05",
                      "kernel_lacks_row_normalized_semantics:" + kernel_id};
  }
  if (mode == P3Mode::PointSourceFlux && !k->supports_column_normalized) {
    return GateResult{Status::Reject, "G-P3-KRN-05",
                      "kernel_lacks_column_normalized_semantics:" + kernel_id};
  }
  return GateResult{Status::Ok, "", ""};
}

const KernelRegistry& KernelRegistry::frozen() {
  static const KernelRegistry reg = [] {
    KernelRegistry r;

    // --- nearest：registered_restricted（仅离散 mask/诊断/显式选择）---
    {
      KernelDescriptor k;
      k.kernel_id = "nearest";
      k.status = KernelStatus::RegisteredRestricted;
      k.allowed_uses = {KernelUse::DiscreteMask, KernelUse::Diagnostics,
                        KernelUse::ExplicitUserSelection};
      k.normalization = KernelNormalization::Discrete;
      k.supports_row_normalized = false;   // 无插值，不制造连续科学场
      k.supports_column_normalized = false;
      k.error_bound_present = false;
      k.error_bound_expression = "";
      k.boundary_defined = true;
      k.boundary_definition =
          "缺 tile/越界 -> NaN + coverage=0，禁零填（无插值权重算术）";
      k.boundary_policy = BoundaryPolicy::MissingIsNaN;
      k.oracle.kind = OracleKind::Structural;
      k.oracle.ok = true;
      k.oracle.independent = true;
      k.oracle.oracle_id = "ALG-P3-001-KERNEL-ORACLE/nearest-structural";
      k.oracle.boundary_fail_closed = true;
      k.production_science_default = false;
      r.add(k);
    }

    // --- bilinear_4quad：registered_with_oracle（连续场；生产默认=false）---
    {
      KernelDescriptor k;
      k.kernel_id = "bilinear_4quad";
      k.status = KernelStatus::RegisteredWithOracle;
      k.allowed_uses = {KernelUse::ContinuousField, KernelUse::ExplicitUserSelection};
      k.normalization = KernelNormalization::UnitSum;
      k.supports_row_normalized = true;
      k.supports_column_normalized = true;
      k.error_bound_present = true;
      k.error_bound_expression = "(h^2/8)*(max|Fxx| + max|Fyy|)";
      k.boundary_defined = true;
      k.boundary_definition =
          "四象限最近中心双线性；缺 tile/越界 -> NaN，禁零填";
      k.boundary_policy = BoundaryPolicy::MissingIsNaN;
      k.oracle.kind = OracleKind::Analytic;
      k.oracle.ok = true;
      k.oracle.independent = true;
      k.oracle.oracle_id = "ALG-P3-001-KERNEL-ORACLE/bilinear_4quad";
      k.oracle.max_interp_err = 0.027395522883651657;
      k.oracle.analytic_bound = 1.0 / 36.0;  // (h^2/8)(max|Fxx|+max|Fyy|)=0.0277777...
      k.oracle.err_over_bound = 0.9862388238114597;
      k.oracle.max_weight_sum_dev = 0.0;
      k.oracle.boundary_fail_closed = true;
      k.oracle.zero_fill_error = 1.3223;
      k.production_science_default = false;
      r.add(k);
    }

    // --- bilinear_area_overlap_exact：registered_with_oracle（生产科学默认）---
    {
      KernelDescriptor k;
      k.kernel_id = "bilinear_area_overlap_exact";
      k.status = KernelStatus::RegisteredWithOracle;
      k.allowed_uses = {KernelUse::ContinuousField, KernelUse::ExplicitUserSelection};
      k.normalization = KernelNormalization::ExactOverlap;
      k.supports_row_normalized = true;
      k.supports_column_normalized = true;
      k.error_bound_present = true;
      k.error_bound_expression = "0（解析重叠，无插值近似）";
      k.boundary_defined = true;
      k.boundary_definition = "重叠 0 贡献；无覆盖 -> NaN";
      k.boundary_policy = BoundaryPolicy::MissingIsNaN;
      k.oracle.kind = OracleKind::Analytic;
      k.oracle.ok = true;
      k.oracle.independent = true;
      k.oracle.oracle_id = "ALG-P3-001-KERNEL-ORACLE/area_overlap_exact";
      k.oracle.max_interp_err = 0.0;
      k.oracle.analytic_bound = 0.0;
      k.oracle.err_over_bound = 1.0;  // 0/0 -> 约定为 ≤ 上界
      k.oracle.max_weight_sum_dev = 0.0;
      k.oracle.boundary_fail_closed = true;
      k.oracle.zero_fill_error = 0.0;
      k.production_science_default = true;
      r.add(k);
    }

    // --- 高阶核：requires_registration（进生产即 REJECT）---
    for (const char* id : {"bicubic", "lanczos"}) {
      KernelDescriptor k;
      k.kernel_id = id;
      k.status = KernelStatus::RequiresRegistration;
      k.allowed_uses = {KernelUse::ContinuousField};
      k.normalization = KernelNormalization::UnitSum;
      k.supports_row_normalized = true;
      k.supports_column_normalized = true;
      k.error_bound_present = false;
      k.boundary_defined = false;
      k.boundary_policy = BoundaryPolicy::Undefined;
      k.production_science_default = false;
      r.add(k);
    }
    return r;
  }();
  return reg;
}

}  // namespace p3rsmp
}  // namespace astrocs
