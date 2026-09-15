/* v6_provenance.h — provenance 最小集对象 + fail-closed 门
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/astro_image_io/v6/。
 * 语义锚:
 *   FZ-PROV-MINIMAL-SET   最小集缺键/单位不可判/unavailable 无原因 -> REJECT
 *   FZ-BUNIT-SEMANTICS    BUNIT 必须量纲可判
 *   FZ-P3-BUNIT-QUADRATIC variance = signal^2; ivar = 1/variance
 *   FZ-COND-FLUX-CONSERV  pixfrac<1 缺 flux_conservation_factor -> 不可用于绝对通量
 *   FZ-PROV-KCORR         k_corr 定义+适用域+值+标定；k_corr=1 忽略相关 -> REJECT
 *   FZ-PROV-SHARED-SYSTEMATIC / FZ-GATE-PARENT-VAR  相关核摘要
 *   FZ-DEGRADE-SCALAR     标量降级分位数与双门
 *   ADJ-GEN-03            unavailable.{flag,reason,scope} 必填
 *
 * 机器形状与 contracts/proposals/v6/data/astrocs.v6.provenance.v1.schema.json
 * required 集一致；required 列表由独立 Oracle 从 schema 反查交叉校验。
 */
#ifndef ASTROCS_V6_AIO_PROVENANCE_H
#define ASTROCS_V6_AIO_PROVENANCE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "astro/aio/v6_validation.h"

namespace astrocs {
namespace aio {

struct ProductRef {
  std::string type_id;
  int schema_version = 1;
};

struct ProvenanceUnits {
  std::string bunit;
  std::string pixel_semantics;  // surface_brightness | integrated_flux
  int pixel_area_power = 0;
  bool has_target_pixel_area = false;
  double target_pixel_area = 0.0;
};

struct Sampling {
  std::string kernel_id;
  bool has_pixfrac = false;
  double pixfrac = 1.0;
};

struct ModuleRef {
  std::string module_id;
  std::string build_id;
};

struct Degradation {
  std::string kind;
  std::string reason;
  double p05 = 0.0, p50 = 0.0, p95 = 0.0;
  double max_systematic_deviation = 0.0;
  double sampling_coverage = 0.0;
  double model_error = 0.0;
  std::string domain;
  bool gate_spatial_residual = false;
  bool gate_power_loss = false;
};

struct CorrelationSummary {
  std::string representation;  // correlation_kernel | low_rank_factors | common_master | full_matrix_unavailable
  std::string kernel_id;
  bool has_scale = false;
  double scale = 0.0;
  bool has_mean_abs_rho = false;
  double mean_abs_rho = 0.0;
  bool has_max_abs_rho = false;
  double max_abs_rho = 0.0;
  std::string low_rank_ref;
  std::string master_id;
  bool has_alpha_m = false;
  double alpha_m = 0.0;
};

struct KCorrDomain {
  std::string geometry;
  double pixfrac = 0.0;
  int patch_size = 0;
  std::string estimator;
  bool spherical = false;
};

struct KCorrCalibration {
  std::string script;
  long long seed = 0;
  std::string calibration_run_id;
};

struct KCorr {
  std::string definition = "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]";
  double value = 0.0;
  KCorrDomain domain;
  KCorrCalibration calibration;
  std::string lookup_table_ref;
};

struct Provenance {
  std::string provenance_schema = "astrocs.v6.provenance/v1";
  int schema_version = 1;
  ProductRef product;
  std::string software_sha;
  std::string run_id;
  std::vector<std::string> input_product_hashes;
  std::string config_hash;
  ProvenanceUnits units;
  std::string coordinate_frame;
  std::string coordinate_epoch;
  std::string pixel_semantics;
  Sampling sampling;
  std::vector<std::string> algorithm_ids;
  ModuleRef module;
  std::string provider;
  std::vector<std::string> approximations;
  std::vector<Degradation> degradations;
  std::string normalization_version;
  std::string weight_mode_version;
  CorrelationSummary correlation_summary;
  bool has_flux_conservation_factor = false;
  double flux_conservation_factor = 0.0;
  KCorr k_corr;
  bool has_k_corr = false;
  bool unavailable_flag = false;
  std::string unavailable_reason;
  std::string unavailable_scope;
  std::string generated_utc;
  std::string output_hash;
  // 声明方差仅对角（用于 FZ-GATE-PARENT-VAR / FZ-PROV-SHARED-SYSTEMATIC）。
  bool diagonal_variance_only = false;
  // 实际平面单位（用于 FZ-P3-BUNIT-QUADRATIC）。
  std::string signal_unit;
  std::string variance_unit;
  std::string ivar_unit;
};

// provenance 最小集必须键（与 schema required 一致；Oracle 独立反查校验）。
std::vector<std::string> provenance_required_keys();

nlohmann::json provenance_to_json(const Provenance& p);

// 对 JSON 形状/门做 fail-closed 校验（required 由调用方给出，可注入变异）。
ValidationReport validate_provenance_json(
    const nlohmann::json& j, const std::vector<std::string>& required);

ValidationReport validate_provenance(const Provenance& p);

// 单位/BUNIT 门集合：可判性 + 像素幂次 + 二次律 + 冻结单位表。
ValidationReport validate_bunit_law(
    const ProvenanceUnits& units, const std::string& signal_unit,
    const std::string& variance_unit, const std::string& ivar_unit);

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_PROVENANCE_H
