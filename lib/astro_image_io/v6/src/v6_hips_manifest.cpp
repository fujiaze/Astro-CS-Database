/* v6_hips_manifest.cpp — HiPS properties / manifest 实现。 */
#include "astro/aio/v6_hips_manifest.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace astrocs {
namespace aio {
namespace {

using nlohmann::json;

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

bool is_hex64(const std::string& s) {
  if (s.size() != 64) return false;
  for (char c : s) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

const std::set<std::string>& allowed_roles() {
  static const std::set<std::string> kRoles = {
      "signal", "variance", "ivar", "coverage", "validity", "support",
      "rejection", "point_information", "psf", "manifest", "properties", "tile"};
  return kRoles;
}

bool allowed_tile_format(const std::string& f) {
  return f == "fits" || f == "png" || f == "jpeg" || f == "jpg";
}

bool path_is_safe(const std::string& p) {
  if (p.empty() || p[0] == '/') return false;
  if (p.find("..") != std::string::npos) return false;
  return true;
}

}  // namespace

std::string render_hips_properties(const HipsProperties& p) {
  std::ostringstream os;
  auto emit = [&os](const std::string& k, const std::string& v) {
    os << k << " = " << v << "\n";
  };
  emit("creator_did", p.creator_did);
  emit("obs_collection", p.obs_collection);
  emit("dataproduct_type", p.dataproduct_type);
  emit("hips_version", p.hips_version);
  emit("hips_release_date", p.release_date);
  emit("hips_status", p.hips_status);
  emit("hips_frame", p.frame);
  emit("hips_order", std::to_string(p.order));
  if (p.order_min >= 0) emit("hips_order_min", std::to_string(p.order_min));
  emit("hips_tile_width", std::to_string(p.tile_width));
  emit("hips_tile_format", p.tile_format);
  if (p.has_initial_position) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10g", p.initial_ra);
    emit("hips_initial_ra", buf);
    std::snprintf(buf, sizeof(buf), "%.10g", p.initial_dec);
    emit("hips_initial_dec", buf);
    std::snprintf(buf, sizeof(buf), "%.10g", p.initial_fov);
    emit("hips_initial_fov", buf);
  }
  return os.str();
}

ValidationReport validate_hips_properties(const std::string& text) {
  ValidationReport r;
  std::map<std::string, std::string> kv;
  std::istringstream is(text);
  std::string line;
  while (std::getline(is, line)) {
    const std::string t = trim(line);
    if (t.empty() || t[0] == '#') continue;
    const std::size_t eq = t.find('=');
    if (eq == std::string::npos) {
      r.add("G-HIPS-PROPERTIES", "ALG-P3-008", "properties line without '=': " + t);
      continue;
    }
    kv[trim(t.substr(0, eq))] = trim(t.substr(eq + 1));
  }
  const char* kRequired[] = {"creator_did",       "obs_collection",
                             "dataproduct_type",  "hips_version",
                             "hips_order",        "hips_tile_width",
                             "hips_tile_format",  "hips_frame"};
  for (const char* k : kRequired) {
    if (kv.find(k) == kv.end() || kv[k].empty()) {
      r.add("G-HIPS-PROPERTIES", "ALG-P3-008",
            std::string("missing HiPS key: ") + k);
    }
  }
  if (kv.count("hips_order")) {
    const long o = std::strtol(kv["hips_order"].c_str(), nullptr, 10);
    if (o < 0) r.add("G-HIPS-PROPERTIES", "ALG-P3-008", "hips_order must be >=0");
    if (kv.count("hips_order_min")) {
      const long om = std::strtol(kv["hips_order_min"].c_str(), nullptr, 10);
      if (om > o) r.add("G-HIPS-PROPERTIES", "ALG-P3-008", "hips_order_min > hips_order");
    }
  }
  if (kv.count("hips_tile_width")) {
    const long w = std::strtol(kv["hips_tile_width"].c_str(), nullptr, 10);
    if (w <= 0) r.add("G-HIPS-PROPERTIES", "ALG-P3-008", "hips_tile_width must be >0");
  }
  if (kv.count("hips_tile_format") && !allowed_tile_format(kv["hips_tile_format"])) {
    r.add("G-HIPS-PROPERTIES", "ALG-P3-008",
          "hips_tile_format not in {fits,png,jpeg}");
  }
  return r;
}

json hips_manifest_to_json(const HipsManifest& m) {
  json j;
  j["manifest_schema"] = m.manifest_schema;
  j["schema_version"] = m.schema_version;
  j["product_type_id"] = m.product_type_id;
  j["order"] = m.order;
  if (m.order_min >= 0) j["order_min"] = m.order_min;
  j["tile_format"] = m.tile_format;
  j["frame"] = m.frame;
  j["tile_count"] = m.tile_count;
  json files = json::array();
  for (const auto& f : m.files) {
    json fj = {{"relative_path", f.relative_path},
               {"size_bytes", f.size_bytes},
               {"sha256", f.sha256_hex},
               {"role", f.role}};
    if (!f.hdu_extname.empty()) fj["hdu_extname"] = f.hdu_extname;
    files.push_back(fj);
  }
  j["files"] = files;
  j["provenance_sha256"] = m.provenance_sha256;
  j["output_hash"] = m.output_hash;
  j["generated_utc"] = m.generated_utc;
  return j;
}

std::string render_hips_manifest_json(const HipsManifest& m) {
  return hips_manifest_to_json(m).dump(2) + "\n";
}

ValidationReport validate_hips_manifest_json(const json& j) {
  ValidationReport r;
  if (!j.is_object()) {
    r.add("G-HIPS-MANIFEST", "ALG-P3-008", "manifest not an object");
    return r;
  }
  const char* kRequired[] = {"manifest_schema", "schema_version", "product_type_id",
                             "order", "tile_format", "frame", "tile_count",
                             "files", "provenance_sha256", "output_hash",
                             "generated_utc"};
  for (const char* k : kRequired) {
    if (!j.contains(k) || (j[k].is_string() && j[k].get<std::string>().empty())) {
      r.add("G-HIPS-MANIFEST", "ALG-P3-008", std::string("missing manifest key: ") + k);
    }
  }
  if (j.contains("tile_format") && j["tile_format"].is_string() &&
      !allowed_tile_format(j["tile_format"].get<std::string>())) {
    r.add("G-HIPS-MANIFEST", "ALG-P3-008", "tile_format invalid");
  }
  if (j.contains("order") && j["order"].is_number() && j["order"].get<long long>() < 0) {
    r.add("G-HIPS-MANIFEST", "ALG-P3-008", "order must be >=0");
  }
  if (j.contains("files") && j["files"].is_array()) {
    if (j.contains("tile_count") &&
        j["tile_count"].get<std::uint64_t>() != j["files"].size()) {
      r.add("G-HIPS-MANIFEST", "ALG-P3-008",
            "tile_count != files.size() (" +
                std::to_string(j["tile_count"].get<std::uint64_t>()) + " vs " +
                std::to_string(j["files"].size()) + ")");
    }
    for (const auto& f : j["files"]) {
      if (!f.is_object()) {
        r.add("G-HIPS-MANIFEST", "ALG-P3-008", "file record not an object");
        continue;
      }
      const std::string p = f.value("relative_path", std::string());
      const std::string sha = f.value("sha256", std::string());
      const std::string role = f.value("role", std::string());
      if (!path_is_safe(p)) {
        r.add("G-HIPS-MANIFEST", "ALG-P3-008", "unsafe relative_path: " + p);
      }
      if (!is_hex64(sha)) {
        r.add("G-HIPS-MANIFEST", "ALG-P3-008", "sha256 must be 64 hex: " + p);
      }
      if (allowed_roles().count(role) == 0) {
        r.add("G-HIPS-MANIFEST", "ALG-P3-008", "invalid role '" + role + "' for " + p);
      }
      if (!f.contains("size_bytes")) {
        r.add("G-HIPS-MANIFEST", "ALG-P3-008", "missing size_bytes for " + p);
      }
    }
  } else {
    r.add("G-HIPS-MANIFEST", "ALG-P3-008", "files must be a non-empty array");
  }
  if (j.contains("provenance_sha256")) {
    const std::string p = j.value("provenance_sha256", std::string());
    if (!is_hex64(p)) {
      r.add("G-HIPS-MANIFEST", "ALG-P3-008", "provenance_sha256 must be 64 hex");
    }
  }
  return r;
}

}  // namespace aio
}  // namespace astrocs
