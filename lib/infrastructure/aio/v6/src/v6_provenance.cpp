/* v6_provenance.cpp — provenance 最小集校验实现。 */
#include "astro/aio/v6_provenance.h"

#include <cmath>
#include <cctype>
#include <set>

#include <nlohmann/json.hpp>

#include "astro/aio/v6_bunit.h"

namespace astrocs {
namespace aio {
namespace {

using nlohmann::json;

bool is_nonempty(const json& j) {
  if (j.is_null()) return false;
  if (j.is_string()) return !j.get_ref<const std::string&>().empty();
  if (j.is_array() || j.is_object()) return !j.empty();
  if (j.is_boolean()) return true;
  if (j.is_number()) return true;
  return false;
}

bool is_hex40(const std::string& s) {
  if (s.size() != 40) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool is_placeholder_reason(const std::string& raw) {
  std::string s;
  for (char c : raw) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  static const std::set<std::string> kPlaceholders = {
      "", "tbd", "todo", "unknown", "n/a", "na", "none", "placeholder", "?",
      "-", "null", "unavailable", "not_applicable_reason"};
  return kPlaceholders.count(s) > 0;
}

const char* kAllowedRepresentations[] = {"correlation_kernel", "low_rank_factors",
                                         "common_master", "full_matrix_unavailable"};

}  // namespace

std::vector<std::string> provenance_required_keys() {
  return {
      "provenance_schema",   "schema_version",       "product",
      "software_sha",        "run_id",               "input_product_hashes",
      "config_hash",         "units",                "coordinate",
      "pixel_semantics",     "sampling",             "algorithm_ids",
      "module",              "provider",             "approximations",
      "degradations",        "normalization_version","weight_mode_version",
      "correlation_summary", "flux_conservation_factor", "k_corr",
      "generated_utc",       "output_hash"};
}

json provenance_to_json(const Provenance& p) {
  json j;
  j["provenance_schema"] = p.provenance_schema;
  j["schema_version"] = p.schema_version;
  j["product"] = {{"type_id", p.product.type_id},
                  {"schema_version", p.product.schema_version}};
  j["software_sha"] = p.software_sha;
  j["run_id"] = p.run_id;
  j["input_product_hashes"] = p.input_product_hashes;
  j["config_hash"] = p.config_hash;
  json units = {{"bunit", p.units.bunit},
                {"pixel_semantics", p.units.pixel_semantics},
                {"pixel_area_power", p.units.pixel_area_power}};
  if (p.units.has_target_pixel_area) units["target_pixel_area"] = p.units.target_pixel_area;
  j["units"] = units;
  json coord = {{"frame", p.coordinate_frame}};
  if (!p.coordinate_epoch.empty()) coord["epoch"] = p.coordinate_epoch;
  j["coordinate"] = coord;
  j["pixel_semantics"] = p.pixel_semantics;
  json sampling = json::object();
  if (!p.sampling.kernel_id.empty()) sampling["kernel_id"] = p.sampling.kernel_id;
  if (p.sampling.has_pixfrac) sampling["pixfrac"] = p.sampling.pixfrac;
  if (sampling.empty()) sampling["kernel_id"] = "unspecified";
  j["sampling"] = sampling;
  j["algorithm_ids"] = p.algorithm_ids;
  json module = json::object();
  module["module_id"] = p.module.module_id;
  module["build_id"] = p.module.build_id;
  j["module"] = module;
  j["provider"] = p.provider;
  j["approximations"] = p.approximations;
  json degs = json::array();
  for (const auto& d : p.degradations) {
    json dj = {{"kind", d.kind},
               {"reason", d.reason},
               {"p05", d.p05},
               {"p50", d.p50},
               {"p95", d.p95},
               {"max_systematic_deviation", d.max_systematic_deviation},
               {"sampling_coverage", d.sampling_coverage},
               {"model_error", d.model_error},
               {"domain", d.domain}};
    json gates = json::object();
    gates["spatial_residual"] = d.gate_spatial_residual;
    gates["power_loss"] = d.gate_power_loss;
    dj["gates"] = gates;
    degs.push_back(dj);
  }
  j["degradations"] = degs;
  j["normalization_version"] = p.normalization_version;
  j["weight_mode_version"] = p.weight_mode_version;
  json cs = json::object();
  cs["representation"] = p.correlation_summary.representation;
  if (!p.correlation_summary.kernel_id.empty()) cs["kernel_id"] = p.correlation_summary.kernel_id;
  if (p.correlation_summary.has_scale) cs["scale"] = p.correlation_summary.scale;
  if (p.correlation_summary.has_mean_abs_rho) cs["mean_abs_rho"] = p.correlation_summary.mean_abs_rho;
  if (p.correlation_summary.has_max_abs_rho) cs["max_abs_rho"] = p.correlation_summary.max_abs_rho;
  if (!p.correlation_summary.low_rank_ref.empty()) cs["low_rank_ref"] = p.correlation_summary.low_rank_ref;
  if (!p.correlation_summary.master_id.empty()) cs["master_id"] = p.correlation_summary.master_id;
  if (p.correlation_summary.has_alpha_m) cs["alpha_m"] = p.correlation_summary.alpha_m;
  j["correlation_summary"] = cs;
  if (p.has_flux_conservation_factor) {
    j["flux_conservation_factor"] = p.flux_conservation_factor;
  } else {
    j["flux_conservation_factor"] = nullptr;
  }
  if (p.has_k_corr) {
    json kc = {{"definition", p.k_corr.definition},
               {"value", p.k_corr.value},
               {"domain", {{"geometry", p.k_corr.domain.geometry},
                           {"pixfrac", p.k_corr.domain.pixfrac},
                           {"patch_size", p.k_corr.domain.patch_size},
                           {"estimator", p.k_corr.domain.estimator},
                           {"spherical", p.k_corr.domain.spherical}}},
               {"calibration", {{"script", p.k_corr.calibration.script},
                                {"seed", p.k_corr.calibration.seed},
                                {"calibration_run_id", p.k_corr.calibration.calibration_run_id}}}};
    if (!p.k_corr.lookup_table_ref.empty()) kc["lookup_table_ref"] = p.k_corr.lookup_table_ref;
    j["k_corr"] = kc;
  } else {
    j["k_corr"] = nullptr;
  }
  j["unavailable"] = {{"flag", p.unavailable_flag},
                      {"reason", p.unavailable_reason},
                      {"scope", p.unavailable_scope}};
  j["generated_utc"] = p.generated_utc;
  j["output_hash"] = p.output_hash;
  return j;
}

ValidationReport validate_bunit_law(const ProvenanceUnits& units,
                                    const std::string& signal_unit,
                                    const std::string& variance_unit,
                                    const std::string& ivar_unit) {
  ValidationReport r;
  if (units.bunit.empty()) {
    r.add("G-BUNIT-SEMANTICS", "FZ-BUNIT-SEMANTICS", "units.bunit missing");
  } else {
    const BunitCheck bc = bunit_dimension_decidable(
        units.bunit, units.pixel_semantics, units.pixel_area_power,
        units.has_target_pixel_area, units.target_pixel_area);
    if (!bc.decidable) {
      r.add("G-BUNIT-SEMANTICS", "FZ-BUNIT-SEMANTICS",
            "BUNIT not dimensionally decidable: " + bc.reason);
    }
  }
  if (units.pixel_semantics == "surface_brightness" && units.pixel_area_power != -2) {
    r.add("G-PIXEL-AREA-POWER", "FZ-BUNIT-SEMANTICS",
          "surface_brightness pixel_area_power must be -2, got " +
              std::to_string(units.pixel_area_power));
  }
  if (!signal_unit.empty()) {
    std::string why;
    if (!quadratic_law_holds(signal_unit, variance_unit, ivar_unit, &why)) {
      r.add("G-BUNIT-QUADRATIC", "FZ-P3-BUNIT-QUADRATIC", why);
    }
  }
  if (!units.bunit.empty() && !signal_unit.empty()) {
    UnitExponents a, b;
    if (parse_unit_exponents(units.bunit, &a) &&
        parse_unit_exponents(signal_unit, &b) &&
        (a.adu_power != b.adu_power || a.px_power != b.px_power)) {
      r.add("G-BUNIT-SEMANTICS", "FZ-BUNIT-SEMANTICS",
            "units.bunit '" + units.bunit + "' != signal_unit '" + signal_unit + "'");
    }
  }
  return r;
}

ValidationReport validate_provenance_json(
    const json& j, const std::vector<std::string>& required) {
  ValidationReport r;
  if (!j.is_object()) {
    r.add("G-PROV-MINIMAL-SET", "FZ-PROV-MINIMAL-SET", "provenance is not an object");
    return r;
  }
  for (const auto& key : required) {
    // 最小集语义: 键必须存在且非 null; 字符串非空; 对象非空;
    // input_product_hashes / algorithm_ids 至少 1 项 (schema minItems=1);
    // approximations / degradations 允许为空数组 (schema 无 minItems)。
    bool bad = false;
    if (!j.contains(key) || j.at(key).is_null()) {
      bad = true;
    } else {
      const json& v = j.at(key);
      if (v.is_string() && v.get_ref<const std::string&>().empty()) bad = true;
      if (v.is_object() && v.empty()) bad = true;
      if ((key == "input_product_hashes" || key == "algorithm_ids") &&
          (!v.is_array() || v.empty())) {
        bad = true;
      }
    }
    if (bad) {
      r.add("G-PROV-MINIMAL-SET", "FZ-PROV-MINIMAL-SET",
            "missing/empty required key: " + key);
    }
  }
  if (j.contains("software_sha") && j["software_sha"].is_string()) {
    if (!is_hex40(j["software_sha"].get<std::string>())) {
      r.add("G-PROV-MINIMAL-SET", "FZ-PROV-MINIMAL-SET",
            "software_sha must be full 40-hex SHA");
    }
  }
  // unavailable 显式登记 (ADJ-GEN-03)。
  if (j.contains("unavailable") && j["unavailable"].is_object()) {
    const json& u = j["unavailable"];
    const bool flag = u.value("flag", false);
    const std::string reason = u.value("reason", std::string());
    const std::string scope = u.value("scope", std::string());
    if (reason.empty() || scope.empty()) {
      r.add("G-UNAVAILABLE-REASON", "ADJ-GEN-03",
            "unavailable.reason/scope must be explicitly registered");
    } else if (flag && is_placeholder_reason(reason)) {
      r.add("G-UNAVAILABLE-REASON", "ADJ-GEN-03",
            "unavailable.reason is a placeholder: " + reason);
    }
  } else {
    r.add("G-UNAVAILABLE-REASON", "ADJ-GEN-03", "unavailable block missing");
  }
  // pixel 语义一致性。
  if (j.contains("units") && j["units"].is_object()) {
    const json& u = j["units"];
    const std::string ps = u.value("pixel_semantics", std::string());
    const std::string top_ps = j.value("pixel_semantics", std::string());
    if (ps != top_ps) {
      r.add("G-PROV-MINIMAL-SET", "FZ-PROV-MINIMAL-SET",
            "units.pixel_semantics != top-level pixel_semantics");
    }
  }
  // flux_conservation_factor (FZ-COND-FLUX-CONSERV)。
  bool has_pixfrac = false;
  double pixfrac = 1.0;
  if (j.contains("sampling") && j["sampling"].is_object() &&
      j["sampling"].contains("pixfrac")) {
    has_pixfrac = true;
    pixfrac = j["sampling"]["pixfrac"].is_number()
                  ? j["sampling"]["pixfrac"].get<double>()
                  : 1.0;
  }
  if (has_pixfrac && pixfrac > 0.0 && pixfrac < 1.0) {
    const bool fcf_ok = j.contains("flux_conservation_factor") &&
                        j["flux_conservation_factor"].is_number() &&
                        j["flux_conservation_factor"].get<double>() > 0.0;
    if (!fcf_ok) {
      r.add("G-FLUX-CONSERV-FACTOR", "FZ-COND-FLUX-CONSERV",
            "pixfrac<1 requires positive flux_conservation_factor");
    }
  }
  // k_corr (FZ-PROV-KCORR)。
  if (j.contains("k_corr") && j["k_corr"].is_object()) {
    const json& kc = j["k_corr"];
    const std::string def = kc.value("definition", std::string());
    const double value = kc.value("value", 0.0);
    if (def.empty()) {
      r.add("G-KCORR-DOMAIN", "FZ-PROV-KCORR", "k_corr.definition missing");
    }
    if (!(value > 0.0)) {
      r.add("G-KCORR-DOMAIN", "FZ-PROV-KCORR", "k_corr.value must be positive");
    } else if (std::fabs(value - 1.0) <= 1e-12) {
      r.add("G-KCORR-DOMAIN", "FZ-PROV-KCORR",
            "k_corr=1 ignores correlation (REJECT)");
    }
    if (!kc.contains("domain") || !kc["domain"].is_object() ||
        !is_nonempty(kc["domain"])) {
      r.add("G-KCORR-DOMAIN", "FZ-PROV-KCORR", "k_corr.domain missing");
    }
    if (!kc.contains("calibration") || !kc["calibration"].is_object()) {
      r.add("G-KCORR-CALIBRATION", "FZ-PROV-KCORR",
            "k_corr.calibration missing");
    } else {
      const json& c = kc["calibration"];
      if (c.value("script", std::string()).empty() ||
          c.value("calibration_run_id", std::string()).empty()) {
        r.add("G-KCORR-CALIBRATION", "FZ-PROV-KCORR",
              "k_corr.calibration.script/calibration_run_id required");
      }
      if (!c.contains("seed")) {
        r.add("G-KCORR-CALIBRATION", "FZ-PROV-KCORR",
              "k_corr.calibration.seed (fixed seed) required");
      }
    }
  } else {
    r.add("G-KCORR-DOMAIN", "FZ-PROV-KCORR", "k_corr block missing");
  }
  // correlation_summary (FZ-PROV-SHARED-SYSTEMATIC)。
  if (j.contains("correlation_summary") && j["correlation_summary"].is_object()) {
    const std::string rep =
        j["correlation_summary"].value("representation", std::string());
    bool ok = false;
    for (const char* a : kAllowedRepresentations) {
      if (rep == a) ok = true;
    }
    if (!ok) {
      r.add("G-SHARED-SYSTEMATIC", "FZ-PROV-SHARED-SYSTEMATIC",
            "correlation_summary.representation invalid: '" + rep + "'");
    }
  }
  // degradations (FZ-DEGRADE-SCALAR)。
  if (j.contains("degradations") && j["degradations"].is_array()) {
    int idx = 0;
    for (const auto& d : j["degradations"]) {
      const std::string where = "degradation[" + std::to_string(idx) + "]";
      ++idx;
      if (!d.is_object()) {
        r.add("G-DEGRADE-SCALAR", "FZ-DEGRADE-SCALAR", where + " not an object");
        continue;
      }
      if (!(d.value("p05", 0.0) <= d.value("p50", 0.0) &&
            d.value("p50", 0.0) <= d.value("p95", 0.0))) {
        r.add("G-DEGRADE-SCALAR", "FZ-DEGRADE-SCALAR",
              where + " requires p05<=p50<=p95");
      }
      if (!d.contains("gates") || !d["gates"].is_object() || d["gates"].empty()) {
        r.add("G-DEGRADE-SCALAR", "FZ-DEGRADE-SCALAR",
              where + " gates (spatial residual + power loss) required");
      }
    }
  }
  return r;
}

ValidationReport validate_provenance(const Provenance& p) {
  ValidationReport r = validate_provenance_json(provenance_to_json(p),
                                                provenance_required_keys());
  r.merge(validate_bunit_law(p.units, p.signal_unit, p.variance_unit, p.ivar_unit));
  if (p.diagonal_variance_only &&
      p.correlation_summary.representation == "full_matrix_unavailable") {
    r.add("G-SHARED-SYSTEMATIC", "FZ-GATE-PARENT-VAR",
          "diagonal-only variance without correlation_kernel/low_rank/common_master");
  }
  return r;
}

}  // namespace aio
}  // namespace astrocs
