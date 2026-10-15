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
    m["csr_row_offsets"] = payload.csr_row_offsets;
    m["csr_column_indices"] = payload.csr_column_indices;
    m["csr_coefficients"] = payload.csr_coefficients;
    m["csr_shape"] = {static_cast<int>(payload.leaf_ipix.size()),
                      static_cast<int>(payload.source_pixel_index.size())};
  }
  if (payload.kind == LocalModelKind::kSupport && !payload.support_note.empty()) {
    m["support_note"] = payload.support_note;
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
    jc["support_area_sr"] = num_or_null(c.support_area_sr);
    jc["psf_model_id"] = c.psf_model_id.empty() ? json(nullptr) : json(c.psf_model_id);
    jc["covariance_model_id"] = c.covariance_model_id.empty() ? json(nullptr) : json(c.covariance_model_id);
    jc["covariance_condition"] = "reference_source";
    jc["background_nuisance"] = c.background_nuisance;
    jc["reference_flux"] = num_or_null(c.reference_flux);
    jc["flux_variance"] = num_or_null(c.flux_variance);
    jc["reference_snr"] = num_or_null(c.reference_snr);
    jc["status"] = status_json(c.status);
    jc["covariance_rank"] = opt_num(c.covariance_rank.has_value()
                                        ? std::optional<double>(static_cast<double>(*c.covariance_rank))
                                        : std::nullopt);
    jc["condition_number"] = opt_num(c.condition_number);
    jc["surface_transfer"] = transfer_json(c.transfer);
    controls.push_back(jc);
  }
  pr["controls"] = controls;
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
      d.signal.leaf_order = s.value("leaf_order", 0);
    }
    if (j.contains("wcs") && j["wcs"].is_object()) {
      const auto& w = j["wcs"];
      d.wcs.status = str_or(w, "status", "");
      d.wcs.source = str_or(w, "source", "");
      d.wcs.crval_deg = w.value("crval_deg", std::vector<double>{});
      d.wcs.crpix = w.value("crpix", std::vector<double>{});
      d.wcs.cd_deg_per_pixel = w.value("cd_deg_per_pixel", std::vector<double>{});
      if (w.contains("sip") && w["sip"].is_object()) {
        d.wcs.sip_present = true;
        d.wcs.sip_order = w["sip"].value("order", 0);
        d.wcs.sip_ap_order = w["sip"].value("ap_order", 0);
      }
      d.wcs.rms_arcsec = w.contains("rms_arcsec") && w["rms_arcsec"].is_number()
                             ? std::optional<double>(w["rms_arcsec"].get<double>())
                             : std::nullopt;
    }
    if (j.contains("photometry") && j["photometry"].is_object()) {
      const auto& p = j["photometry"];
      d.photometry.status = str_or(p, "status", "");
      d.photometry.application_id = str_or(p, "application_id", "");
      d.photometry.scale_applied = p.value("scale_applied", false);
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
      if (rf.contains("flux") && rf["flux"].is_number()) {
        d.reference.flux = rf["flux"].get<double>();
      }
      d.reference.reason = str_or(rf, "reason", "");
    }
    if (j.contains("models") && j["models"].is_array()) {
      for (const auto& m : j["models"]) {
        ModelFileRef ref;
        ref.model_id = str_or(m, "model_id", "");
        ref.kind = LocalModelKind::kPsf;   // shape 校验不推断科学身份; kind 由读者按需取
        if (m.contains("file") && m["file"].is_object()) {
          ref.path = str_or(m["file"], "path", "");
          ref.sha256 = str_or(m["file"], "sha256", "");
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
            st.value("availability", std::string()) == "available"
                ? PrecisionAvailability::kAvailable
                : PrecisionAvailability::kUnavailable;
        d.precision.status.reason = str_or(st, "reason", "");
        d.precision.status.omitted_terms = st.value("omitted_terms", std::vector<std::string>{});
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
          if (c.contains("status") && c["status"].is_object()) {
            rc.status.availability =
                str_or(c["status"], "availability", "") == "available"
                    ? PrecisionAvailability::kAvailable
                    : PrecisionAvailability::kUnavailable;
            rc.status.reason = str_or(c["status"], "reason", "");
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
      }
    }
  } catch (const std::exception& e) {
    r.ok = false;
    r.error = std::string("frame_science.json: shape violation: ") + e.what();
    return r;
  }

  // ── §8 末段物理一致性核对（shape 之外的执行者校验）────────────────────────
  auto& d = r.doc;
  if (d.wcs.crval_deg.size() == 2) {
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
  } else {
    r.findings.push_back("wcs.crval_deg not [ra,dec] pair");
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
          r.findings.push_back("available control without F_ref/V_F/SNR: " + c.control_id);
        } else if (*c.flux_variance > 0.0) {
          const double snr_check = *c.reference_flux / std::sqrt(*c.flux_variance);
          if (std::abs(snr_check - *c.reference_snr) > 1e-6 * std::abs(snr_check)) {
            r.findings.push_back("F_ref/SNR/V_F not paired: " + c.control_id);
          }
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
      if (m.path.empty() || m.sha256.empty()) {
        r.findings.push_back("model reference without path/hash: " + m.model_id);
      }
    }
    // 摘要必须由有效 controls 得出
    if (d.precision.frame_summary.reference_snr.has_value() && d.precision.controls.empty()) {
      r.findings.push_back("frame summary without controls");
    }
  }
  r.ok = true;   // ok = 可作 canonical precision 真源; findings 供复核（非静默接受）
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

bool looks_like_phase1_frame_product(const std::string& frame_product_root) {
  std::string root = frame_product_root;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  std::ifstream f(root + "/signal/properties");
  return f.good();
}

}  // namespace acsd::normalize
