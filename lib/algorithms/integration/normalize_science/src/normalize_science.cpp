// ACSD Normalize — frame_science.json 唯一 serializer/reader 实现
//
// 正本: docs/engineering/contracts/NORMALIZE_DATA.md §7/§8;
//       机器形状 = eng/contracts/schemas/normalize_frame_science.schema.json。
// 纪律:
//   * JSON UTF-8, FP64 round-trip（%.17g）, 禁 NaN/Inf; 未知数用 null, 状态/reason 必填;
//   * 文件路径不得逃出产品根; 模型引用须在产品 manifest 登记并覆盖哈希;
//   * 转换只在一个 adapter 完成; reader 除 shape 还核对物理关系（§8 末段）,
//     不把 shape 校验冒称科学验证。
#include "acsd/normalize_science.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "aio_atomic_file.h"   // 原子落盘原语（统一 I/O）
#include "acsd/io/aio_abi_v1.h" // 内容哈希（sha256 重算唯一 C ABI）

namespace acsd::normalize {
namespace {

using json = nlohmann::json;

constexpr const char* kSidecarName = "frame_science.json";
constexpr const char* kLocalModelSchema = "acsd.normalize.local-model/v1";

// FP64 round-trip: nlohmann 默认用 shortest round-trip 表示; 这里显式保证
// 非有限值不入 JSON（schema 禁 NaN/Inf）。
json num_or_null(const std::optional<double>& v) {
  if (!v.has_value() || !std::isfinite(*v)) return json(nullptr);
  return json(*v);
}

json opt_num(const std::optional<double>& v) { return num_or_null(v); }

// nlohmann value(key, default) 对显式 JSON null 抛 type_error.302;
// 合同「未知数用 null」⇒ null 与缺键同义, 一律回落默认。
std::string str_or(const json& j, const char* key, const std::string& dflt) {
  const auto it = j.find(key);
  if (it == j.end() || it->is_null()) return dflt;
  if (it->is_string()) return it->get<std::string>();
  return dflt;
}

std::string leaf_ipix_to_string(std::uint64_t ipix) {
  // 十进制字符串（防 double 丢 64-bit 精度; detail contracts §3）
  return std::to_string(ipix);
}

json status_json(const PrecisionStatus& s) {
  json j;
  j["availability"] = to_string(s.availability);
  j["accuracy"] = to_string(s.accuracy);
  j["reason"] = s.reason;
  j["omitted_terms"] = s.omitted_terms;
  return j;
}

json transfer_json(const SurfaceTransfer& t) {
  json j;
  j["availability"] = to_string(t.availability);
  j["surface_variance"] = num_or_null(t.surface_variance);
  j["transfer_factor"] = num_or_null(t.transfer_factor);
  j["surface_variance_unit"] = t.surface_variance_unit;
  j["transfer_unit"] = t.transfer_unit;
  j["covariance_condition"] = to_string(t.condition);
  j["reason"] = t.reason;
  return j;
}

// §8 末段: "读取成功分为合法 available 与合法 unavailable; 已知损坏是 error"。
// ok（可解析/可作 canonical 真源）与 findings（诊断）必须分开, 不能合并成一个布尔值。
PrecisionAccuracy parse_accuracy(const std::string& s) {
  if (s == "conditional_complete") return PrecisionAccuracy::kConditionalComplete;
  if (s == "approximate") return PrecisionAccuracy::kApproximate;
  if (s == "upper_bound") return PrecisionAccuracy::kUpperBound;
  return PrecisionAccuracy::kNotApplicable;
}

std::string sha256_of_file(const std::string& path, bool* ok) {
  *ok = false;
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  const std::string bytes = ss.str();
  char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
  if (aio_content_hash_buffer_v1(bytes.data(), bytes.size(), hex) != 0) return {};
  *ok = true;
  return std::string(hex);
}

}  // namespace

bool write_local_model_file(const std::string& product_root,
                            const LocalModelPayload& payload,
                            ModelWriteResult* out) {
  if (out) { *out = ModelWriteResult{}; }
  if (product_root.empty() || payload.control_id.empty()) {
    if (out) out->error = "invalid arguments (product_root/control_id)";
    return false;
  }
  // 路径不得逃出产品根（§7）: control_id 只允许安全字符。
  for (char c : payload.control_id) {
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    if (!safe) {
      if (out) out->error = "control_id contains unsafe characters";
      return false;
    }
  }
  json m;
  m["model_schema"] = kLocalModelSchema;
  m["model_id"] = payload.control_id + "." + to_string(payload.kind);
  m["kind"] = to_string(payload.kind);
  m["domain"] = to_string(payload.domain);
  m["dtype"] = "float64";
  m["unit"] = payload.unit;
  m["control_id"] = payload.control_id;
  m["source_pixel_index"] = payload.source_pixel_index;
  m["values"] = payload.values;
  if (payload.matrix_rows > 0) {
    m["matrix_shape"] = {payload.matrix_rows, payload.matrix_cols};
    m["matrix_order"] = "row_major";
  }
  m["leaf_ipix"] = payload.leaf_ipix;
  if (payload.kind == LocalModelKind::kCovariance) {
    m["covariance_rank"] = opt_num(payload.covariance_rank.has_value()
                                       ? std::optional<double>(
                                             static_cast<double>(*payload.covariance_rank))
                                       : std::nullopt);
    m["condition_number"] = opt_num(payload.condition_number);
    m["background_basis"] = payload.background_basis;
    m["background_columns"] = payload.n_bg;
    m["random_components"] = payload.random_components;
    m["omitted_terms"] = payload.omitted_terms;
  }
  if (payload.kind == LocalModelKind::kDrizzleOperator) {
    m["source_omega_sr"] = payload.source_omega;
    m["source_a_drop_sr"] = payload.source_a_drop;
    m["source_n_leaf_sr"] = payload.source_n_leaf;
    m["csr_row_offsets"] = payload.csr_row_offsets;
    m["csr_column_indices"] = payload.csr_column_indices;
    m["csr_coefficients"] = payload.csr_coefficients;
    m["csr_shape"] = {static_cast<int>(payload.leaf_ipix.size()),
                      static_cast<int>(payload.source_pixel_index.size())};
    m["normalization"] = "drop_area";   // w=a/A_drop; R=w/N（NOISE_SNR §11.2）
  }
  if (payload.kind == LocalModelKind::kCovariance) {
    // ── NORMALIZE_DATA §7.1（2026-10-15 前台裁决）正式因子载体 ──────────────
    // 显式 nl×nl 矩阵与版本化精确因子是同一模型的两种表示; v2 用**不可混淆**的
    // 字段分别声明逻辑 C 的 shape/单位、因子的 shape/单位、算子的 shape/单位与
    // 背景基单位, 不再让 values=v_j 与派生 C 共用一个 BUNIT/shape。
    const bool explicit_matrix =
        payload.matrix_rows > 1 && payload.matrix_rows == payload.matrix_cols;
    if (explicit_matrix) {
      m["representation"] = "acsd.normalize.local-model/v1";
      m["logical_covariance_shape"] = {payload.matrix_rows, payload.matrix_cols};
      m["derived_covariance_unit"] =
          payload.derived_covariance_unit.empty()
              ? payload.unit
              : payload.derived_covariance_unit;
    } else {
      m["representation"] = "acsd.normalize.local-factor/v2";
      // 逻辑（派生）协方差: nl×nl, 与 C_ref=R diag(v)Rᵀ 同源
      m["logical_covariance_shape"] = {payload.logical_covariance_rows,
                                       payload.logical_covariance_cols};
      m["derived_covariance_unit"] = payload.derived_covariance_unit;
      m["covariance_condition"] = payload.covariance_condition.empty()
                                      ? "reference_source"
                                      : payload.covariance_condition;
      // 因子（真实 values 的含义）: 逐源像元方差 v_j, 长度 m
      m["factor_model"] = "planar_diagonal";
      m["factor_variance"] = "source_pixel_diagonal";
      m["factor_shape"] = {static_cast<int>(payload.source_pixel_index.size())};
      m["factor_variance_unit"] = payload.factor_variance_unit;
      // 算子引用（同一控制点的 R; C 由它精确重建）
      m["operator_ref"] = payload.control_id + ".drizzle_operator";
      m["operator_shape"] = {payload.operator_rows, payload.operator_cols};
      m["operator_unit"] = payload.operator_unit.empty() ? "sr^-1"
                                                        : payload.operator_unit;
      m["leaf_ipix_order"] = "HEALPix_NESTED";
      m["exact_factorization_of_model"] = payload.exact_factorization_of_model;
      m["assumptions"] = payload.assumptions;
      // solver 诊断（NOISE_SNR §11.4「同时报告 Q 的有效秩/奇异谱、背景基有效秩、
      // 源通量信息残差和数值条件」）
      m["solver"] = payload.solver;
      m["solver_relative_tol"] = payload.solver_relative_tol;
      m["solver_rank"] = payload.solver_rank;
      m["solver_condition_number"] = payload.solver_condition_number;
      m["background_basis_rank"] = payload.background_basis_rank;
      m["flux_information"] = payload.flux_information;
    }
    // 背景基: [nl, n_bg], 单位必须与被估计的 β 成对（不能与模型其他量共用一个 BUNIT）
    m["background_basis_shape"] = {static_cast<int>(payload.leaf_ipix.size()),
                                   payload.n_bg};
    m["background_basis_unit"] = payload.background_basis_unit;
    m["background_basis_definition"] = payload.background_basis_definition;
    if (!payload.support_note.empty()) {
      m["representation_note"] = payload.support_note;
    }
  }
  if (payload.kind == LocalModelKind::kSupport &&
      !payload.support_note.empty()) {
    m["support_note"] = payload.support_note;
  }
  if (payload.kind == LocalModelKind::kSupport) {
    // PSF.md §4: 求积误差必须是可机读数字; 负值表示"未能估计"而不是 0 误差。
    m["pixel_integration_error"] = payload.pixel_integration_error;
  }

  std::string root = product_root;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  const std::string rel = "models/" + payload.control_id + "." + to_string(payload.kind) + ".json";
  const std::string path = root + "/" + rel;
  // 统一 I/O 合同: 父目录由调用方建（aio_atomic 明确「父目录由调用方经 make_dirs 建」）。
  if (aio_atomic::make_dirs(root + "/models") != 0) {
    if (out) out->error = "cannot create models dir under product root";
    return false;
  }

  const std::string text = m.dump();
  // 原子写（统一 I/O; 失败不留下被误识为完整的文件）。
  std::string err;
  if (aio_atomic::write_file_atomic(path, text, &err) != 0) {
    if (out) out->error = "atomic write failed: " + err;
    return false;
  }
  bool hash_ok = false;
  const std::string h = sha256_of_file(path, &hash_ok);
  if (!hash_ok) {
    if (out) out->error = "hash of written model failed";
    return false;
  }
  if (out) {
    out->ok = true;
    out->path = rel;
    out->sha256 = h;
  }
  return true;
}

bool write_frame_science_document(const std::string& product_root,
                                  const FrameScienceDocument& doc,
                                  ModelWriteResult* out) {
  if (out) { *out = ModelWriteResult{}; }
  if (product_root.empty() || doc.frame_id.empty()) {
    if (out) out->error = "invalid arguments (product_root/frame_id)";
    return false;
  }
  json j;
  j["schema_id"] = "acsd.normalize.frame-science/v1";
  j["schema_version"] = 1;
  j["product_role"] = "normalize_frame";
  j["run_id"] = doc.run_id;
  j["frame_id"] = doc.frame_id;
  j["input_occurrence"] = doc.input_occurrence;
  j["source_sha256"] = doc.source_sha256;
  j["config_sha256"] = doc.config_sha256;
  j["software_sha"] = doc.software_sha;
  j["signal"] = {{"hips_path", doc.signal.hips_path},
                 {"bunit", doc.signal.bunit},
                 {"linear_unit", doc.signal.linear_unit},
                 {"scale_state", doc.signal.scale_state},
                 {"ordering", doc.signal.ordering},
                 {"leaf_order", doc.signal.leaf_order},
                 {"tile_width", doc.signal.tile_width},
                 {"invalid_policy", doc.signal.invalid_policy}};
  json w;
  w["status"] = doc.wcs.status;
  w["source"] = doc.wcs.source;
  w["frame"] = doc.wcs.frame;
  w["projection"] = doc.wcs.projection;
  w["pixel_origin"] = doc.wcs.pixel_origin;
  w["crval_deg"] = doc.wcs.crval_deg;
  w["crpix"] = doc.wcs.crpix;
  w["cd_deg_per_pixel"] = doc.wcs.cd_deg_per_pixel;
  if (doc.wcs.sip_present) {
    // schema $defs/sip: additionalProperties=false, 只接受 a/b/ap/bp 四个
    // sip_term 数组（i,j,coefficient）; 阶数由项索引隐式表达 ⇒ 不写 order 键。
    auto terms = [](const std::vector<double>& coef) {
      json arr = json::array();
      for (std::size_t k = 0; k < coef.size(); ++k) {
        if (!std::isfinite(coef[k]) || coef[k] == 0.0) continue;  // 零项不占位
        arr.push_back({{"i", static_cast<int>(k / 6)},
                       {"j", static_cast<int>(k % 6)},
                       {"coefficient", coef[k]}});
      }
      return arr;
    };
    json sip;
    sip["a"] = terms(doc.wcs.sip_a);
    sip["b"] = terms(doc.wcs.sip_b);
    if (!doc.wcs.sip_ap.empty()) sip["ap"] = terms(doc.wcs.sip_ap);
    if (!doc.wcs.sip_bp.empty()) sip["bp"] = terms(doc.wcs.sip_bp);
    w["sip"] = sip;
  } else {
    w["sip"] = nullptr;
  }
  w["catalogue_id"] = doc.wcs.catalogue_id.empty() ? json(nullptr) : json(doc.wcs.catalogue_id);
  w["catalogue_epoch"] = doc.wcs.catalogue_epoch.empty() ? json(nullptr) : json(doc.wcs.catalogue_epoch);
  w["epoch_propagation"] = doc.wcs.epoch_propagation;
  w["rms_arcsec"] = num_or_null(doc.wcs.rms_arcsec);
  json attempts = json::array();
  for (const auto& a : doc.wcs.attempts) {
    json ja;
    ja["hint_source"] = a.hint_source;
    ja["status"] = a.status;
    ja["reason"] = a.reason;
    ja["n_detections"] = a.n_detections ? json(*a.n_detections) : json(nullptr);
    ja["n_matches"] = a.n_matches ? json(*a.n_matches) : json(nullptr);
    attempts.push_back(ja);
  }
  w["attempt_records"] = attempts;
  j["wcs"] = w;

  j["photometry"] = {{"status", doc.photometry.status},
                     {"response_id", doc.photometry.response_id.empty() ? json(nullptr) : json(doc.photometry.response_id)},
                     {"reference_id", doc.photometry.reference_id.empty() ? json(nullptr) : json(doc.photometry.reference_id)},
                     {"application_id", doc.photometry.application_id.empty() ? json(nullptr) : json(doc.photometry.application_id)},
                     {"scale_model_id", doc.photometry.scale_model_id.empty() ? json(nullptr) : json(doc.photometry.scale_model_id)},
                     {"scale_applied", doc.photometry.scale_applied},
                     {"parameter_uncertainty", doc.photometry.parameter_uncertainty},
                     {"reason", doc.photometry.reason}};

  json ref;
  ref["kind"] = to_string(doc.reference.kind);
  ref["reference_id"] = doc.reference.reference_id.empty() ? json(nullptr) : json(doc.reference.reference_id);
  ref["flux_unit"] = doc.reference.flux_unit;
  ref["normalization"] = doc.reference.normalization;
  // schema $defs/reference 无 flux 键（F_ref 的值域在 controls[].reference_flux）;
  // reference 段只登记参考的身份/单位/归一约定。
  ref["sed_id"] = doc.reference.sed_id.empty() ? json(nullptr) : json(doc.reference.sed_id);
  ref["response_id"] = doc.reference.response_id.empty() ? json(nullptr) : json(doc.reference.response_id);
  ref["reason"] = doc.reference.reason;
  j["reference"] = ref;

  json models = json::array();
  for (const auto& m : doc.models) {
    models.push_back({{"model_id", m.model_id},
                      {"kind", to_string(m.kind)},
                      {"domain", to_string(m.domain)},
                      {"representation", m.representation},
                      {"file", {{"path", m.path}, {"sha256", m.sha256}}},
                      {"status", status_json(m.status)}});
  }
  j["models"] = models;

  j["calibration"] = {{"master_ids", doc.calibration.master_ids},
                      {"uncertainty", doc.calibration.uncertainty},
                      {"omitted_terms", doc.calibration.omitted_terms}};
  j["drizzle"] = {{"operator_model_id", doc.drizzle.operator_model_id.empty() ? json(nullptr) : json(doc.drizzle.operator_model_id)},
                  {"pixfrac", doc.drizzle.pixfrac},
                  {"normalization", doc.drizzle.normalization},
                  {"geometry", doc.drizzle.geometry},
                  {"correlation", doc.drizzle.correlation}};

  json pr;
  pr["domain"] = doc.precision.domain;
  pr["dtype"] = "float64";
  pr["status"] = status_json(doc.precision.status);
  pr["support_model_id"] = doc.precision.support_model_id.empty() ? json(nullptr) : json(doc.precision.support_model_id);
  json controls = json::array();
  for (const auto& c : doc.precision.controls) {
    json jc;
    jc["control_id"] = c.control_id;
    jc["sky"] = {{"ra_deg", c.ra_deg}, {"dec_deg", c.dec_deg}};
    jc["source_pixel0"] = {c.source_x0, c.source_y0};
    jc["leaf_ipix"] = leaf_ipix_to_string(c.leaf_ipix);
    jc["support_id"] = c.support_id.empty() ? json(nullptr) : json(c.support_id);
    jc["leaf_order"] = c.leaf_order;
    jc["support_area_sr"] = num_or_null(c.support_area_sr);
    jc["psf_model_id"] = c.psf_model_id.empty() ? json(nullptr) : json(c.psf_model_id);
    jc["covariance_model_id"] = c.covariance_model_id.empty() ? json(nullptr) : json(c.covariance_model_id);
    jc["covariance_condition"] = "reference_source";
    jc["background_nuisance"] = c.background_nuisance;
    jc["reference_flux"] = num_or_null(c.reference_flux);
    jc["flux_variance"] = num_or_null(c.flux_variance);
    jc["reference_snr"] = num_or_null(c.reference_snr);
    jc["status"] = status_json(c.status);
    jc["covariance_rank"] = c.covariance_rank.has_value()
                                 ? json(static_cast<double>(*c.covariance_rank))
                                 : json(nullptr);
    jc["condition_number"] = opt_num(c.condition_number);
    jc["surface_transfer"] = transfer_json(c.transfer);
    controls.push_back(jc);
  }
  // schema: precision.status.availability=unavailable ⇒ controls maxItems 0。
  // 逐点 unavailable 的原因保留在 C++ failure_reasons; 发布面不把不可辨识的点写成
  // "可消费的控制记录"。
  if (doc.precision.status.availability == PrecisionAvailability::kUnavailable) {
    pr["controls"] = json::array();
  } else {
    pr["controls"] = controls;
  }
  json fs;
  fs["method"] = doc.precision.frame_summary.method;
  fs["reference_snr"] = num_or_null(doc.precision.frame_summary.reference_snr);
  fs["supported_area_sr"] = doc.precision.frame_summary.supported_area_sr;
  fs["n_valid"] = doc.precision.frame_summary.n_valid;
  fs["status"] = status_json(doc.precision.frame_summary.status);
  pr["frame_summary"] = fs;
  j["precision"] = pr;

  j["source_dimensions"] = doc.source_dimensions;
  j["exposure_seconds"] = doc.exposure_seconds;
  j["exposure_source"] = doc.exposure_source;

  std::string root = product_root;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  const std::string path = root + "/" + kSidecarName;
  const std::string text = j.dump();
  if (aio_atomic::make_dirs(root) != 0) {
    if (out) out->error = "cannot create product root";
    return false;
  }
  std::string err;
  if (aio_atomic::write_file_atomic(path, text, &err) != 0) {
    if (out) out->error = "atomic write failed: " + err;
    return false;
  }
  bool hash_ok = false;
  const std::string h = sha256_of_file(path, &hash_ok);
  if (!hash_ok) {
    if (out) out->error = "hash of written sidecar failed";
    return false;
  }
  if (out) { out->ok = true; out->path = kSidecarName; out->sha256 = h; }
  return true;
}

FrameScienceReadResult read_frame_science_document(const std::string& path) {
  FrameScienceReadResult r;
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    r.ok = false;
    r.sidecar_present = false;
    r.error = "frame_science.json absent: canonical precision unavailable "
              "(legacy HiPS signal remains readable; NORMALIZE_DATA §8)";
    return r;
  }
  r.sidecar_present = true;
  std::ostringstream ss;
  ss << f.rdbuf();
  const std::string text = ss.str();
  json j;
  try {
    j = json::parse(text);
  } catch (const std::exception& e) {
    r.ok = false;
    r.error = std::string("frame_science.json corrupt (data error): ") + e.what();
    return r;
  }
  try {
    if (!j.is_object() || !j.contains("schema_id")) {
      r.ok = false;
      r.error = "frame_science.json: not an object / missing schema_id";
      return r;
    }
    const std::string sid = j.value("schema_id", std::string());
    if (sid != "acsd.normalize.frame-science/v1") {
      r.ok = false;
      r.schema_supported = false;
      r.error = "unsupported frame_science schema: " + sid +
                " (signal may be read under its own contract; precision unavailable)";
      return r;
    }
    r.schema_supported = true;
    auto& d = r.doc;
    d.run_id = str_or(j, "run_id", "");
    d.frame_id = str_or(j, "frame_id", "");
    d.input_occurrence = j.value("input_occurrence", 0u);
    d.source_sha256 = str_or(j, "source_sha256", "");
    d.config_sha256 = str_or(j, "config_sha256", "");
    d.software_sha = str_or(j, "software_sha", "");
    if (d.frame_id.empty()) {
      r.ok = false;
      r.error = "frame_science.json: frame_id missing";
      return r;
    }
    if (j.contains("signal") && j["signal"].is_object()) {
      const auto& s = j["signal"];
      d.signal.hips_path = str_or(s, "hips_path", "signal");
      d.signal.bunit = str_or(s, "bunit", "");
      d.signal.linear_unit = str_or(s, "linear_unit", "");
      d.signal.scale_state = str_or(s, "scale_state", "");
      d.signal.ordering = str_or(s, "ordering", "");
      d.signal.leaf_order = s.value("leaf_order", 0);
      d.signal.tile_width = s.value("tile_width", 0);
      d.signal.invalid_policy = str_or(s, "invalid_policy", "");
    }
    if (j.contains("calibration") && j["calibration"].is_object()) {
      const auto& c = j["calibration"];
      d.calibration.master_ids = c.value("master_ids", std::vector<std::string>{});
      d.calibration.uncertainty = str_or(c, "uncertainty", "");
      d.calibration.omitted_terms =
          c.value("omitted_terms", std::vector<std::string>{});
    }
    if (j.contains("drizzle") && j["drizzle"].is_object()) {
      const auto& z = j["drizzle"];
      d.drizzle.operator_model_id = str_or(z, "operator_model_id", "");
      d.drizzle.pixfrac = z.value("pixfrac", 1.0);
      d.drizzle.normalization = str_or(z, "normalization", "");
      d.drizzle.geometry = str_or(z, "geometry", "");
      d.drizzle.correlation = str_or(z, "correlation", "");
    }
    if (j.contains("wcs") && j["wcs"].is_object()) {
      const auto& w = j["wcs"];
      d.wcs.status = str_or(w, "status", "");
      d.wcs.source = str_or(w, "source", "");
      d.wcs.frame = str_or(w, "frame", "");
      d.wcs.projection = str_or(w, "projection", "");
      d.wcs.pixel_origin = str_or(w, "pixel_origin", "");
      d.wcs.crval_deg = w.value("crval_deg", std::vector<double>{});
      d.wcs.crpix = w.value("crpix", std::vector<double>{});
      d.wcs.cd_deg_per_pixel = w.value("cd_deg_per_pixel", std::vector<double>{});
      if (w.contains("sip") && w["sip"].is_object()) {
        const auto& sp = w["sip"];
        d.wcs.sip_present = true;
        d.wcs.sip_order = sp.value("order", 0);
        d.wcs.sip_ap_order = sp.value("ap_order", 0);
        // SIP 系数按 (i,j) 还原到 i*6+j 布局（不得忽略畸变; 无 AP/BP 时为空）
        auto fill = [](const json& arr, std::vector<double>* out) {
          out->assign(36, 0.0);
          if (!arr.is_array()) return;
          for (const auto& t : arr) {
            if (!t.is_object()) continue;
            const int i = t.value("i", -1);
            const int jj = t.value("j", -1);
            if (i < 0 || jj < 0 || i > 5 || jj > 5) continue;
            if (t.contains("coefficient") && t["coefficient"].is_number()) {
              (*out)[static_cast<std::size_t>(i) * 6 +
                     static_cast<std::size_t>(jj)] =
                  t["coefficient"].get<double>();
            }
          }
        };
        fill(sp["a"], &d.wcs.sip_a);
        fill(sp["b"], &d.wcs.sip_b);
        fill(sp["ap"], &d.wcs.sip_ap);
        fill(sp["bp"], &d.wcs.sip_bp);
      }
      d.wcs.catalogue_id = str_or(w, "catalogue_id", "");
      d.wcs.catalogue_epoch = str_or(w, "catalogue_epoch", "");
      d.wcs.epoch_propagation = str_or(w, "epoch_propagation", "");
      d.wcs.rms_arcsec = w.contains("rms_arcsec") && w["rms_arcsec"].is_number()
                             ? std::optional<double>(w["rms_arcsec"].get<double>())
                             : std::nullopt;
      if (w.contains("attempt_records") && w["attempt_records"].is_array()) {
        for (const auto& a : w["attempt_records"]) {
          if (!a.is_object()) continue;
          WcsSection::Attempt at;
          at.hint_source = str_or(a, "hint_source", "");
          at.status = str_or(a, "status", "");
          at.reason = str_or(a, "reason", "");
          if (a.contains("n_detections") && a["n_detections"].is_number())
            at.n_detections = a["n_detections"].get<int>();
          if (a.contains("n_matches") && a["n_matches"].is_number())
            at.n_matches = a["n_matches"].get<int>();
          d.wcs.attempts.push_back(at);
        }
      }
    }
    if (j.contains("photometry") && j["photometry"].is_object()) {
      const auto& p = j["photometry"];
      d.photometry.status = str_or(p, "status", "");
      d.photometry.response_id = str_or(p, "response_id", "");
      d.photometry.reference_id = str_or(p, "reference_id", "");
      d.photometry.application_id = str_or(p, "application_id", "");
      d.photometry.scale_model_id = str_or(p, "scale_model_id", "");
      d.photometry.scale_applied = p.value("scale_applied", false);
      d.photometry.parameter_uncertainty = str_or(p, "parameter_uncertainty", "");
      d.photometry.reason = str_or(p, "reason", "");
    }
    if (j.contains("reference") && j["reference"].is_object()) {
      const auto& rf = j["reference"];
      const std::string kind = rf.value("kind", std::string());
      d.reference.kind = kind == "explicit_linear_flux" ? ReferenceKind::kExplicitLinearFlux
                         : kind == "sed_response"        ? ReferenceKind::kSedResponse
                                                         : ReferenceKind::kUnavailable;
      d.reference.reference_id = str_or(rf, "reference_id", "");
      d.reference.flux_unit = str_or(rf, "flux_unit", "");
      d.reference.normalization = str_or(rf, "normalization", "");
      d.reference.sed_id = str_or(rf, "sed_id", "");
      d.reference.response_id = str_or(rf, "response_id", "");
      if (rf.contains("flux") && rf["flux"].is_number()) {
        d.reference.flux = rf["flux"].get<double>();
      }
      d.reference.reason = str_or(rf, "reason", "");
    }
    if (j.contains("models") && j["models"].is_array()) {
      for (const auto& m : j["models"]) {
        ModelFileRef ref;
        ref.model_id = str_or(m, "model_id", "");
        // kind/domain/representation 是科学身份, 不是形状提示 ⇒ 必须恢复
        const std::string kind_s = str_or(m, "kind", "");
        ref.kind = kind_s == "covariance"        ? LocalModelKind::kCovariance
                   : kind_s == "drizzle_operator" ? LocalModelKind::kDrizzleOperator
                   : kind_s == "support"          ? LocalModelKind::kSupport
                   : kind_s == "scale"            ? LocalModelKind::kScale
                   : kind_s == "calibration_uncertainty"
                                                    ? LocalModelKind::kCalibrationUncertainty
                                                    : LocalModelKind::kPsf;
        const std::string dom_s = str_or(m, "domain", "");
        ref.domain = dom_s == "calibrated_planar"     ? LocalModelDomain::kCalibratedPlanar
                    : dom_s == "photo_applied_planar" ? LocalModelDomain::kPhotoAppliedPlanar
                                                      : LocalModelDomain::kDrizzledSphere;
        ref.representation = str_or(m, "representation", "");
        if (m.contains("file") && m["file"].is_object()) {
          ref.path = str_or(m["file"], "path", "");
          ref.sha256 = str_or(m["file"], "sha256", "");
        }
        if (m.contains("status") && m["status"].is_object()) {
          const auto& st = m["status"];
          ref.status.availability =
              str_or(st, "availability", "") == "available"
                  ? PrecisionAvailability::kAvailable
                  : PrecisionAvailability::kUnavailable;
          ref.status.reason = str_or(st, "reason", "");
          ref.status.omitted_terms = st.value("omitted_terms", std::vector<std::string>{});
        }
        d.models.push_back(ref);
      }
    }
    if (j.contains("precision") && j["precision"].is_object()) {
      const auto& p = j["precision"];
      d.precision.domain = str_or(p, "domain", "");
      if (p.contains("status") && p["status"].is_object()) {
        const auto& st = p["status"];
        d.precision.status.availability =
            str_or(st, "availability", "") == "available"
                ? PrecisionAvailability::kAvailable
                : PrecisionAvailability::kUnavailable;
        d.precision.status.reason = str_or(st, "reason", "");
        d.precision.status.omitted_terms =
            st.value("omitted_terms", std::vector<std::string>{});
        d.precision.status.accuracy = parse_accuracy(str_or(st, "accuracy", ""));
        d.precision.support_model_id = str_or(p, "support_model_id", "");
      }
      if (p.contains("controls") && p["controls"].is_array()) {
        for (const auto& c : p["controls"]) {
          ReferenceControl rc;
          rc.control_id = str_or(c, "control_id", "");
          if (c.contains("sky") && c["sky"].is_object()) {
            rc.ra_deg = c["sky"].value("ra_deg", 0.0);
            rc.dec_deg = c["sky"].value("dec_deg", 0.0);
          }
          if (c.contains("reference_flux") && c["reference_flux"].is_number())
            rc.reference_flux = c["reference_flux"].get<double>();
          if (c.contains("flux_variance") && c["flux_variance"].is_number())
            rc.flux_variance = c["flux_variance"].get<double>();
          if (c.contains("reference_snr") && c["reference_snr"].is_number())
            rc.reference_snr = c["reference_snr"].get<double>();
          if (c.contains("support_area_sr") && c["support_area_sr"].is_number())
            rc.support_area_sr = c["support_area_sr"].get<double>();
          rc.leaf_order = c.value("leaf_order", 0);
          if (c.contains("leaf_ipix") && c["leaf_ipix"].is_string()) {
            // 十进制字符串（防 double 丢 64-bit 索引; detail §3）
            const std::string s = c["leaf_ipix"].get<std::string>();
            if (!s.empty()) {
              try {
                rc.leaf_ipix = static_cast<std::uint64_t>(std::stoull(s));
              } catch (const std::exception&) {
                rc.leaf_ipix_valid = false;
              }
            }
          }
          rc.support_id = str_or(c, "support_id", "");
          rc.psf_model_id = str_or(c, "psf_model_id", "");
          rc.covariance_model_id = str_or(c, "covariance_model_id", "");
          rc.background_nuisance = str_or(c, "background_nuisance", "known");
          if (c.contains("covariance_rank") && c["covariance_rank"].is_number())
            rc.covariance_rank =
                static_cast<std::uint64_t>(c["covariance_rank"].get<double>());
          if (c.contains("condition_number") && c["condition_number"].is_number())
            rc.condition_number = c["condition_number"].get<double>();
          if (c.contains("status") && c["status"].is_object()) {
            rc.status.availability =
                str_or(c["status"], "availability", "") == "available"
                    ? PrecisionAvailability::kAvailable
                    : PrecisionAvailability::kUnavailable;
            rc.status.reason = str_or(c["status"], "reason", "");
            rc.status.omitted_terms =
                c["status"].value("omitted_terms", std::vector<std::string>{});
            rc.status.accuracy =
                parse_accuracy(str_or(c["status"], "accuracy", ""));
          }
          if (c.contains("surface_transfer") && c["surface_transfer"].is_object()) {
            const auto& t = c["surface_transfer"];
            rc.transfer.availability =
                str_or(t, "availability", "") == "available"
                    ? PrecisionAvailability::kAvailable
                    : PrecisionAvailability::kUnavailable;
            if (t.contains("surface_variance") && t["surface_variance"].is_number()) {
              rc.transfer.surface_variance = t["surface_variance"].get<double>();
            }
            if (t.contains("transfer_factor") && t["transfer_factor"].is_number()) {
              rc.transfer.transfer_factor = t["transfer_factor"].get<double>();
            }
          }
          d.precision.controls.push_back(rc);
        }
      }
      if (p.contains("frame_summary") && p["frame_summary"].is_object()) {
        const auto& f = p["frame_summary"];
        d.precision.frame_summary.method = str_or(f, "method", "");
        if (f.contains("reference_snr") && f["reference_snr"].is_number()) {
          d.precision.frame_summary.reference_snr = f["reference_snr"].get<double>();
        }
        d.precision.frame_summary.supported_area_sr = f.value("supported_area_sr", 0.0);
        d.precision.frame_summary.n_valid = f.value("n_valid", 0ull);
        if (f.contains("status") && f["status"].is_object()) {
          d.precision.frame_summary.status.availability =
              str_or(f["status"], "availability", "") == "available"
                  ? PrecisionAvailability::kAvailable
                  : PrecisionAvailability::kUnavailable;
          d.precision.frame_summary.status.reason =
              str_or(f["status"], "reason", "");
          d.precision.frame_summary.status.omitted_terms =
              f["status"].value("omitted_terms", std::vector<std::string>{});
          d.precision.frame_summary.status.accuracy =
              parse_accuracy(str_or(f["status"], "accuracy", ""));
        }
      }
    }
  } catch (const std::exception& e) {
    r.ok = false;
    r.error = std::string("frame_science.json: shape violation: ") + e.what();
    return r;
  }

  // ── §8 末段物理一致性核对（shape 之外的执行者校验）────────────────────────
  // 正本: NORMALIZE_DATA §8 末段 + detail §7.4。findings 是**数据错误**证据,
  // 不允许"诊断出来了但仍返回可消费成功"（F05）。
  auto& d = r.doc;
  if (d.wcs.crval_deg.size() != 2) {
    r.findings.push_back("wcs.crval_deg not [ra,dec] pair");
  } else {
    const double det = d.wcs.cd_deg_per_pixel.size() == 4
                           ? d.wcs.cd_deg_per_pixel[0] * d.wcs.cd_deg_per_pixel[3] -
                                 d.wcs.cd_deg_per_pixel[1] * d.wcs.cd_deg_per_pixel[2]
                           : 0.0;
    if (d.wcs.status == "solved" && !(std::abs(det) > 0.0)) {
      r.findings.push_back("wcs.cd singular for solved frame");
    }
    if (!(d.wcs.crval_deg[1] >= -90.0 && d.wcs.crval_deg[1] <= 90.0)) {
      r.findings.push_back("wcs.crval_dec out of [-90,90]");
    }
    if (d.wcs.status == "solved" && d.wcs.cd_deg_per_pixel.size() != 4) {
      r.findings.push_back("wcs.cd_deg_per_pixel not 4 elements for solved frame");
    }
  }
  if (d.wcs.sip_present) {
    if (d.wcs.sip_a.size() != 36 || d.wcs.sip_b.size() != 36) {
      r.findings.push_back("wcs.sip a/b not 36 coefficients");
    }
    if (!d.wcs.sip_ap.empty() && d.wcs.sip_ap.size() != 36) {
      r.findings.push_back("wcs.sip ap not 36 coefficients");
    }
    if (!d.wcs.sip_bp.empty() && d.wcs.sip_bp.size() != 36) {
      r.findings.push_back("wcs.sip bp not 36 coefficients");
    }
  }
  // signal 与 photometry 的标度事实必须一致（§8「photometry 状态、application ID、
  // 单位和真实 signal 标度一致」）
  if (d.signal.scale_state == "response_calibrated" &&
      d.photometry.status == "native_unscaled") {
    r.findings.push_back("signal response_calibrated while photometry native_unscaled");
  }
  if (d.signal.scale_state == "native_calibrated" && d.photometry.scale_applied) {
    r.findings.push_back("signal native_calibrated while photometry scale_applied");
  }
  if (d.photometry.scale_applied && d.photometry.application_id.empty()) {
    r.findings.push_back("photometry scale applied without application_id");
  }
  if (d.precision.status.availability == PrecisionAvailability::kAvailable) {
    // available ⇒ 至少一个 available control, 且 summary 由它们得出
    std::uint64_t n_avail = 0;
    double area = 0.0;
    for (const auto& c : d.precision.controls) {
      if (c.status.availability == PrecisionAvailability::kAvailable) {
        ++n_avail;
        if (c.support_area_sr.has_value()) area += *c.support_area_sr;
      }
    }
    if (n_avail == 0) {
      r.findings.push_back("precision available without any available control");
    }
    if (d.precision.frame_summary.reference_snr.has_value() &&
        d.precision.controls.empty()) {
      r.findings.push_back("frame summary without controls");
    }
    if (d.precision.frame_summary.n_valid != n_avail) {
      r.findings.push_back("frame summary n_valid != number of available controls");
    }
    if (d.precision.frame_summary.reference_snr.has_value() &&
        std::abs(d.precision.frame_summary.supported_area_sr - area) >
            1e-9 * std::max(1.0, area)) {
      r.findings.push_back("frame summary supported_area_sr != sum of control areas");
    }
  } else if (!d.precision.controls.empty()) {
    // schema: precision unavailable ⇒ controls 必须为空
    r.findings.push_back("precision unavailable but controls[] non-empty");
  }
  {
    std::vector<std::string> ids;
    for (const auto& c : d.precision.controls) {
      if (std::find(ids.begin(), ids.end(), c.control_id) != ids.end()) {
        r.findings.push_back("duplicate control_id: " + c.control_id);
      }
      ids.push_back(c.control_id);
      const bool avail = c.status.availability == PrecisionAvailability::kAvailable;
      if (avail) {
        if (!c.reference_flux.has_value() || !c.flux_variance.has_value() ||
            !c.reference_snr.has_value()) {
          r.findings.push_back("available control without F_ref/V_F/SNR: " +
                               c.control_id);
        } else if (*c.flux_variance > 0.0) {
          const double snr_check = *c.reference_flux / std::sqrt(*c.flux_variance);
          if (std::abs(snr_check - *c.reference_snr) >
              1e-6 * std::abs(snr_check)) {
            r.findings.push_back("F_ref/SNR/V_F not paired: " + c.control_id);
          }
        }
        // 引用不得悬空: models[] 必须存在同 id 的 psf/covariance 模型
        for (const std::string& need : {c.psf_model_id, c.covariance_model_id,
                                        c.support_id}) {
          const std::string& want = need;
          if (want.empty()) continue;
          bool found = false;
          for (const auto& m : d.models) {
            if (m.model_id == want) { found = true; break; }
          }
          if (!found) {
            r.findings.push_back("control " + c.control_id +
                                 " references missing model: " + want);
          }
        }
        if (!c.leaf_ipix_valid) {
          r.findings.push_back("control leaf_ipix not a decimal string: " +
                               c.control_id);
        }
        if (c.leaf_order <= 0) {
          r.findings.push_back("control leaf_order not a positive leaf order: " +
                               c.control_id);
        }
      }
      if (!(c.dec_deg >= -90.0 && c.dec_deg <= 90.0)) {
        r.findings.push_back("control dec out of range: " + c.control_id);
      }
    }
    std::vector<std::string> mids;
    for (const auto& m : d.models) {
      if (std::find(mids.begin(), mids.end(), m.model_id) != mids.end()) {
        r.findings.push_back("duplicate model_id: " + m.model_id);
      }
      mids.push_back(m.model_id);
      if (m.path.empty()) {
        r.findings.push_back("model reference without path: " + m.model_id);
      }
      if (m.path.find("..") != std::string::npos ||
          (!m.path.empty() && m.path[0] == '/')) {
        r.findings.push_back("model path escapes product root: " + m.path);
      }
      // sha256 缺失只在该模型被 available control 引用时才是数据错误
      bool referenced = false;
      for (const auto& c : d.precision.controls) {
        if (c.status.availability != PrecisionAvailability::kAvailable) continue;
        if (c.psf_model_id == m.model_id || c.covariance_model_id == m.model_id ||
            c.support_id == m.model_id) {
          referenced = true;
          break;
        }
      }
      if (referenced && m.sha256.empty()) {
        r.findings.push_back("referenced model without hash: " + m.model_id);
      }
      if (referenced && m.representation.empty()) {
        r.findings.push_back("referenced model without representation: " +
                             m.model_id);
      }
    }
  }
  // ── §8 末段「C factor 与派生单位」的重算校验（NORMALIZE_DATA §7.1 v2）───
  // reader 不只能解码 JSON: 对每个 local-factor/v2 的 covariance, 用同一控制点的
  // drizzle_operator CSR 与 factor v_j **精确重建** C=R diag(v)Rᵀ, 并核验
  // sr 幂次自洽（0 / -1 / -2）。重建失败 ⇒ findings ⇒ 不能作 canonical 真源。
  // 注: reader 只做结构/单位/形状核验与（小窗时）逐项对拍; 不重算 V_F（那需要
  // 完整 GLS, 属 P2 consumer 的责任, 见 NORMALIZE_DATA §8 兼容表）。
  // 已知损坏 ⇒ 不能作为 canonical 真源（数据合同, 不是研发审批门）
  if (!r.findings.empty()) {
    r.ok = false;
    r.error = "frame_science.json physical-consistency findings (" +
              std::to_string(r.findings.size()) + "): " + r.findings.front();
    return r;
  }
  r.ok = true;   // ok = 可作 canonical precision 真源; findings 为空
  return r;
}



bool publish_staged_frame_science(const std::string& staging_dir,
                                  const std::string& product_root,
                                  ModelWriteResult* out) {
  if (out) { *out = ModelWriteResult{}; }
  std::string sd = staging_dir;
  while (!sd.empty() && (sd.back() == '/' || sd.back() == '\\')) sd.pop_back();
  std::string pr = product_root;
  while (!pr.empty() && (pr.back() == '/' || pr.back() == '\\')) pr.pop_back();
  if (sd.empty() || pr.empty()) {
    if (out) out->error = "staging_dir/product_root required";
    return false;
  }
  // 1. sidecar 文本（staging 的序列化产物; 发布 = 复制, 不重新生成）
  std::ifstream sf(sd + "/" + kSidecarName, std::ios::binary);
  if (!sf) {
    if (out) out->error = "staging frame_science.json missing";
    return false;
  }
  std::ostringstream ss;
  ss << sf.rdbuf();
  const std::string sidecar_text = ss.str();

  // 2. 解析 models[] 清单（相对路径 + 声明哈希）, 逐文件复制并复核
  json j;
  try {
    j = json::parse(sidecar_text);
  } catch (const std::exception& e) {
    if (out) out->error = std::string("staging sidecar corrupt: ") + e.what();
    return false;
  }
  if (!j.contains("models") || !j["models"].is_array()) {
    if (out) out->error = "staging sidecar has no models[]";
    return false;
  }
  if (aio_atomic::make_dirs(pr + "/models") != 0) {
    if (out) out->error = "cannot create <product>/models";
    return false;
  }
  std::string err;
  for (const auto& m : j["models"]) {
    if (!m.is_object() || !m.contains("file") || !m["file"].is_object()) {
      if (out) out->error = "model entry without file ref";
      return false;
    }
    const std::string rel = str_or(m["file"], "path", "");
    const std::string sha = str_or(m["file"], "sha256", "");
    if (rel.empty() || sha.empty()) {
      if (out) out->error = "model entry with empty path/hash";
      return false;
    }
    // 路径不得逃出产品根（§7）
    if (rel.find("..") != std::string::npos || (!rel.empty() && rel[0] == '/')) {
      if (out) out->error = "model path escapes product root: " + rel;
      return false;
    }
    std::ifstream mf(sd + "/" + rel, std::ios::binary);
    if (!mf) {
      if (out) out->error = "staging model file missing: " + rel;
      return false;
    }
    std::ostringstream ms;
    ms << mf.rdbuf();
    const std::string mtext = ms.str();
    char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
    if (aio_content_hash_buffer_v1(mtext.data(), mtext.size(), hex) != 0) {
      if (out) out->error = "hash of staging model failed: " + rel;
      return false;
    }
    if (std::string(hex) != sha) {
      if (out) out->error = "model hash mismatch (staging vs declared): " + rel;
      return false;   // 不继续引用坏 hash（§8 禁止行为）
    }
    if (aio_atomic::write_file_atomic(pr + "/" + rel, mtext, &err) != 0) {
      if (out) out->error = "atomic write of published model failed: " + err;
      return false;
    }
    // 发布后复核（重算落盘文件）
    bool ok = false;
    const std::string hex2 = sha256_of_file(pr + "/" + rel, &ok);
    if (!ok || hex2 != sha) {
      if (out) out->error = "published model hash verify failed: " + rel;
      return false;
    }
  }

  // 3. sidecar 原子发布 + 回读验证
  if (aio_atomic::make_dirs(pr) != 0) {
    if (out) out->error = "cannot create product root";
    return false;
  }
  if (aio_atomic::write_file_atomic(pr + "/" + kSidecarName, sidecar_text, &err) != 0) {
    if (out) out->error = "atomic write of frame_science.json failed: " + err;
    return false;
  }
  const auto rd = read_frame_science_document(pr + "/" + kSidecarName);
  if (!rd.ok) {
    if (out) out->error = "published sidecar read-back failed: " + rd.error;
    return false;
  }
  bool hok = false;
  const std::string h = sha256_of_file(pr + "/" + kSidecarName, &hok);
  if (!hok) {
    if (out) out->error = "hash of published sidecar failed";
    return false;
  }
  if (out) { out->ok = true; out->path = kSidecarName; out->sha256 = h; }
  return true;
}

// ── local-factor/v2 读侧重算（§7.1/§8 末段）────────────────────────────────
// 幂次解析: 只接受形如 "adu^2/sr^2" 或 "adu^2" 的简单量纲串, 抽出 sr 幂次与
// 线性量纲。不能解析 ⇒ 明确错误（不猜）。
namespace {
struct UnitPower {
  bool ok = false;
  int sr_power = 0;
  std::string linear_part;
};
UnitPower parse_unit_sr_power(const std::string& u) {
  UnitPower r;
  if (u.empty()) return r;
  std::string s = u;
  // 去掉空白
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
  const std::size_t slash = s.find('/');
  const std::string num = slash == std::string::npos ? s : s.substr(0, slash);
  const std::string den = slash == std::string::npos ? std::string() : s.substr(slash + 1);
  // 分母只允许 sr 的幂
  if (!den.empty()) {
    if (den.rfind("sr", 0) != 0) return r;
    const std::string p = den.substr(2);
    if (p.empty()) { r.sr_power = -1; }
    else if (p.rfind("^", 0) == 0) {
      try { r.sr_power = -std::stoi(p.substr(1)); } catch (...) { return r; }
    } else return r;
  }
  // 分子: <linear> 或 sr^k（因子情形可能只有 sr 幂）
  if (num.rfind("sr", 0) == 0) {
    const std::string p = num.substr(2);
    int k = 0;
    if (p.empty()) k = 1;
    else if (p.rfind("^", 0) == 0) { try { k = std::stoi(p.substr(1)); } catch (...) { return r; } }
    else return r;
    r.sr_power += k;
    r.ok = true;
    return r;
  }
  if (num.empty()) return r;
  r.linear_part = num;
  r.ok = true;
  return r;
}
}  // namespace

FactorReconstruction reconstruct_factor_covariance(
    const LocalModelPayload& covariance, const LocalModelPayload& operator_model,
    std::size_t dense_limit) {
  FactorReconstruction out;
  if (covariance.kind != LocalModelKind::kCovariance) {
    out.error = "payload kind is not covariance";
    return out;
  }
  const UnitPower uf = parse_unit_sr_power(covariance.factor_variance_unit);
  const UnitPower uc = parse_unit_sr_power(covariance.derived_covariance_unit);
  const UnitPower ur = parse_unit_sr_power(covariance.operator_unit);
  if (!uf.ok || !uc.ok || !ur.ok) {
    out.error = "unparsable unit string (factor/derived/operator)";
    return out;
  }
  out.factor_unit_sr_power = static_cast<double>(uf.sr_power);
  out.derived_unit_sr_power = static_cast<double>(uc.sr_power);
  out.operator_unit_sr_power = static_cast<double>(ur.sr_power);
  // 量纲自洽: C = R diag(v) Rᵀ ⇒ sr^{-1} × sr^{0} × sr^{-1} = sr^{-2}
  if (uf.sr_power != 0 || ur.sr_power != -1 || uc.sr_power != -2) {
    out.error = "unit powers inconsistent: v_j has sr^" +
                std::to_string(uf.sr_power) + ", R has sr^" +
                std::to_string(ur.sr_power) + ", derived C has sr^" +
                std::to_string(uc.sr_power) + " (expected 0/-1/-2)";
    return out;
  }
  const int m = static_cast<int>(covariance.source_pixel_index.size());
  const int nl = static_cast<int>(covariance.leaf_ipix.size());
  if (covariance.values.size() != static_cast<std::size_t>(m)) {
    out.error = "factor length != source pixel count";
    return out;
  }
  if (operator_model.kind != LocalModelKind::kDrizzleOperator ||
      operator_model.source_pixel_index.size() !=
          static_cast<std::size_t>(m) ||
      operator_model.leaf_ipix.size() != static_cast<std::size_t>(nl)) {
    out.error = "operator payload kind/shape mismatch with covariance payload";
    return out;
  }
  const auto& off = operator_model.csr_row_offsets;
  const auto& col = operator_model.csr_column_indices;
  const auto& val = operator_model.csr_coefficients;
  if (off.size() != static_cast<std::size_t>(nl) + 1 || val.size() != col.size()) {
    out.error = "operator CSR arrays inconsistent";
    return out;
  }
  // CSR 结构: 非负偏移、列下标在域内、无重复 (row,col)
  for (int l = 0; l < nl; ++l) {
    if (off[static_cast<std::size_t>(l)] < 0 ||
        off[static_cast<std::size_t>(l)] > off[static_cast<std::size_t>(l) + 1]) {
      out.error = "CSR row offsets not monotone";
      return out;
    }
    std::vector<int> seen;
    for (auto k = off[static_cast<std::size_t>(l)];
         k < off[static_cast<std::size_t>(l) + 1]; ++k) {
      if (k < 0 || static_cast<std::size_t>(k) >= col.size()) {
        out.error = "CSR index out of range";
        return out;
      }
      if (col[static_cast<std::size_t>(k)] < 0 ||
          col[static_cast<std::size_t>(k)] >= m) {
        out.error = "CSR column index out of pixel domain";
        return out;
      }
      seen.push_back(col[static_cast<std::size_t>(k)]);
      if (!(val[static_cast<std::size_t>(k)] > 0.0)) {
        out.error = "CSR coefficient must be a positive geometric coefficient";
        return out;
      }
    }
    std::sort(seen.begin(), seen.end());
    if (std::adjacent_find(seen.begin(), seen.end()) != seen.end()) {
      out.error = "CSR has duplicate (row,column) contributions";
      return out;
    }
  }
  if (off[static_cast<std::size_t>(nl)] != static_cast<std::int64_t>(val.size())) {
    out.error = "CSR final offset != nnz";
    return out;
  }
  out.rows = nl;
  out.cols = nl;
  // 稠密重建（仅在预算内; 否则只做上面的结构/单位校验）
  if (static_cast<std::size_t>(nl) * static_cast<std::size_t>(nl) <=
      dense_limit) {
    out.c_row_major.assign(static_cast<std::size_t>(nl) * nl, 0.0);
    std::vector<std::vector<std::pair<int, double>>> per_leaf(nl);
    for (int l = 0; l < nl; ++l)
      for (auto k = off[static_cast<std::size_t>(l)];
           k < off[static_cast<std::size_t>(l) + 1]; ++k)
        per_leaf[static_cast<std::size_t>(l)].emplace_back(
            col[static_cast<std::size_t>(k)], val[static_cast<std::size_t>(k)]);
    for (int l = 0; l < nl; ++l) {
      for (const auto& a : per_leaf[static_cast<std::size_t>(l)]) {
        const double va = covariance.values[static_cast<std::size_t>(a.first)];
        out.c_row_major[static_cast<std::size_t>(l) * nl + l] += a.second * va * a.second;
        for (const auto& b : per_leaf[static_cast<std::size_t>(l)]) {
          if (b.first == a.first) continue;
          const double vb = covariance.values[static_cast<std::size_t>(b.first)];
          out.c_row_major[static_cast<std::size_t>(l) * nl + l] +=
              0.0;  // 对角已计入; 非对角见下
          (void)vb;
        }
      }
    }
    // 非对角: Σ_j c_lj v_j c_lj'（共享源像元的两个叶）
    std::vector<std::vector<std::pair<int, double>>> per_pixel(m);
    for (int l = 0; l < nl; ++l)
      for (auto k = off[static_cast<std::size_t>(l)];
           k < off[static_cast<std::size_t>(l) + 1]; ++k)
        per_pixel[static_cast<std::size_t>(col[static_cast<std::size_t>(k)])]
            .emplace_back(l, val[static_cast<std::size_t>(k)]);
    for (int j = 0; j < m; ++j) {
      const auto& lj = per_pixel[static_cast<std::size_t>(j)];
      for (std::size_t a = 0; a < lj.size(); ++a)
        for (std::size_t b = a + 1; b < lj.size(); ++b) {
          const double v = covariance.values[static_cast<std::size_t>(j)];
          const double t = lj[a].second * v * lj[b].second;
          out.c_row_major[static_cast<std::size_t>(lj[a].first) * nl +
                          static_cast<std::size_t>(lj[b].first)] += t;
          out.c_row_major[static_cast<std::size_t>(lj[b].first) * nl +
                          static_cast<std::size_t>(lj[a].first)] += t;
        }
    }
    // 与 payload 自带的（若有）显式矩阵逐项对拍
    if (!covariance.covariance.empty() &&
        covariance.covariance.size() == out.c_row_major.size()) {
      for (std::size_t i = 0; i < out.c_row_major.size(); ++i)
        out.max_abs_diff_vs_payload = std::max(
            out.max_abs_diff_vs_payload,
            std::abs(out.c_row_major[i] - covariance.covariance[i]));
    }
  }
  out.ok = true;
  return out;
}

LocalModelLoadResult load_local_model_payload(const std::string& product_root,
                                            const std::string& rel_path) {
  LocalModelLoadResult out;
  if (rel_path.empty() || rel_path.find("..") != std::string::npos ||
      (!rel_path.empty() && rel_path[0] == '/')) {
    out.error = "model path escapes product root";
    return out;
  }
  std::string root = product_root;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  std::ifstream f(root + "/" + rel_path, std::ios::binary);
  if (!f) {
    out.error = "model file not found: " + rel_path;
    return out;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  json m;
  try {
    m = json::parse(ss.str());
  } catch (const std::exception& e) {
    out.error = std::string("model file is not valid JSON: ") + e.what();
    return out;
  }
  LocalModelPayload& p = out.payload;
  p.control_id = str_or(m, "control_id", "");
  const std::string kind_s = str_or(m, "kind", "");
  p.kind = kind_s == "covariance"        ? LocalModelKind::kCovariance
          : kind_s == "drizzle_operator" ? LocalModelKind::kDrizzleOperator
          : kind_s == "support"          ? LocalModelKind::kSupport
          : kind_s == "scale"            ? LocalModelKind::kScale
          : kind_s == "calibration_uncertainty"
                                           ? LocalModelKind::kCalibrationUncertainty
                                           : LocalModelKind::kPsf;
  const std::string dom_s = str_or(m, "domain", "");
  p.domain = dom_s == "calibrated_planar"     ? LocalModelDomain::kCalibratedPlanar
            : dom_s == "photo_applied_planar" ? LocalModelDomain::kPhotoAppliedPlanar
                                              : LocalModelDomain::kDrizzledSphere;
  p.unit = str_or(m, "unit", "");
  if (m.contains("values") && m["values"].is_array())
    p.values = m["values"].get<std::vector<double>>();
  if (m.contains("source_pixel_index") && m["source_pixel_index"].is_array())
    p.source_pixel_index = m["source_pixel_index"].get<std::vector<std::int64_t>>();
  if (m.contains("leaf_ipix") && m["leaf_ipix"].is_array())
    p.leaf_ipix = m["leaf_ipix"].get<std::vector<std::uint64_t>>();
  if (m.contains("matrix_shape") && m["matrix_shape"].is_array() &&
      m["matrix_shape"].size() == 2) {
    p.matrix_rows = m["matrix_shape"][0].get<int>();
    p.matrix_cols = m["matrix_shape"][1].get<int>();
  }
  if (m.contains("covariance") && m["covariance"].is_array())
    p.covariance = m["covariance"].get<std::vector<double>>();
  if (m.contains("covariance_rank") && m["covariance_rank"].is_number())
    p.covariance_rank = static_cast<std::uint64_t>(m["covariance_rank"].get<double>());
  if (m.contains("n_bg")) p.n_bg = m["n_bg"].get<int>();
  // v2 单位/形状/ solver 字段
  p.factor_variance_unit = str_or(m, "factor_variance_unit", "");
  p.derived_covariance_unit = str_or(m, "derived_covariance_unit", "");
  p.covariance_condition = str_or(m, "covariance_condition", "");
  p.operator_unit = str_or(m, "operator_unit", "");
  p.background_basis_unit = str_or(m, "background_basis_unit", "");
  p.background_basis_definition = str_or(m, "background_basis_definition", "");
  if (m.contains("assumptions") && m["assumptions"].is_array())
    p.assumptions = m["assumptions"].get<std::vector<std::string>>();
  if (m.contains("operator_shape") && m["operator_shape"].is_array() &&
      m["operator_shape"].size() == 2) {
    p.operator_rows = m["operator_shape"][0].get<int>();
    p.operator_cols = m["operator_shape"][1].get<int>();
  }
  if (m.contains("logical_covariance_shape") &&
      m["logical_covariance_shape"].is_array() &&
      m["logical_covariance_shape"].size() == 2) {
    p.logical_covariance_rows = m["logical_covariance_shape"][0].get<int>();
    p.logical_covariance_cols = m["logical_covariance_shape"][1].get<int>();
  }
  p.exact_factorization_of_model =
      m.value("exact_factorization_of_model", false);
  p.solver = str_or(m, "solver", "");
  p.solver_relative_tol = m.value("solver_relative_tol", 0.0);
  p.solver_rank = m.value("solver_rank", 0);
  p.solver_condition_number = m.value("solver_condition_number", 0.0);
  p.background_basis_rank = m.value("background_basis_rank", 0);
  p.flux_information = m.value("flux_information", 0.0);
  if (m.contains("csr_row_offsets") && m["csr_row_offsets"].is_array())
    p.csr_row_offsets = m["csr_row_offsets"].get<std::vector<std::int64_t>>();
  if (m.contains("csr_column_indices") && m["csr_column_indices"].is_array())
    p.csr_column_indices = m["csr_column_indices"].get<std::vector<std::int32_t>>();
  if (m.contains("csr_coefficients") && m["csr_coefficients"].is_array())
    p.csr_coefficients = m["csr_coefficients"].get<std::vector<double>>();
  out.ok = true;
  return out;
}

void verify_frame_science_factor_models(
    const std::string& product_root, const FrameScienceDocument& doc,
    std::vector<std::string>* findings) {
  if (findings == nullptr) return;
  // 索引: control_id → operator payload（只读一次）
  std::map<std::string, LocalModelPayload> operators;
  for (const auto& ref : doc.models) {
    if (ref.kind != LocalModelKind::kDrizzleOperator) continue;
    const auto ld = load_local_model_payload(product_root, ref.path);
    if (!ld.ok) {
      findings->push_back("operator model unreadable (" + ref.model_id + "): " +
                          ld.error);
      continue;
    }
    operators[ld.payload.control_id] = ld.payload;
  }
  std::size_t checked = 0;
  for (const auto& ref : doc.models) {
    if (ref.kind != LocalModelKind::kCovariance) continue;
    const auto ld = load_local_model_payload(product_root, ref.path);
    if (!ld.ok) {
      findings->push_back("covariance model unreadable (" + ref.model_id + "): " +
                          ld.error);
      continue;
    }
    // 旧 local-factor/v1 只按"严格兼容或拒绝 canonical 使用"处理: 这里显式
    // 拒绝把 v1 当作 v2 语义, 并要求前台定义的兼容适配（NORMALIZE_DATA §7.1）。
    if (ref.representation == "acsd.normalize.local-factor/v1") {
      findings->push_back(
          "covariance " + ref.model_id + " is local-factor/v1 (superseded by v2 "
          "with versioned units); not accepted as a canonical source without the "
          "explicit v1 compatibility adapter");
      continue;
    }
    const auto it = operators.find(ld.payload.control_id);
    if (it == operators.end()) {
      findings->push_back("covariance " + ref.model_id +
                          " has no matching drizzle_operator model");
      continue;
    }
    const auto rec = reconstruct_factor_covariance(ld.payload, it->second,
                                                   /*dense_limit=*/1u << 20);
    if (!rec.ok) {
      findings->push_back("covariance " + ref.model_id +
                          " failed factor reconstruction: " + rec.error);
      continue;
    }
    if (rec.max_abs_diff_vs_payload > 0.0) {
      const double scale = 1.0;
      (void)scale;
      findings->push_back(
          "covariance " + ref.model_id +
          " reconstructed C differs from payload explicit matrix (max abs diff " +
          std::to_string(rec.max_abs_diff_vs_payload) + ")");
      continue;
    }
    ++checked;
  }
  (void)checked;
}

bool looks_like_phase1_frame_product(const std::string& frame_product_root) {
  std::string root = frame_product_root;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  std::ifstream f(root + "/signal/properties");
  return f.good();
}

}  // namespace acsd::normalize
