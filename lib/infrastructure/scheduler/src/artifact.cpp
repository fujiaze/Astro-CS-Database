// CORE-002 DataArtifact + Provenance 实现
#include "astrocs/core/artifact.h"

#include <cstdio>
#include <cstring>

namespace astrocs::core {

namespace {

// 最小 JSON 编码 (无外部依赖; 仅用于 artifact descriptor, 非通用 JSON)
std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c;
    }
  }
  return out;
}

std::string key(const char* k, const std::string& v) {
  return std::string("\"") + k + "\":\"" + json_escape(v) + "\"";
}

}  // namespace

std::string Provenance::to_json() const {
  std::string out = "{";
  out += key("source_commit", source_commit) + ",";
  out += key("pipeline_hash", pipeline_hash) + ",";
  out += key("module_build_id", module_build_id) + ",";
  out += key("config_hash", config_hash) + ",";
  out += key("input_hash", input_hash) + ",";
  out += key("created_utc", created_utc) + ",";
  out += key("platform", platform) + ",";
  out += key("data_schema_id", data_schema_id) + ",";
  out += "\"schema_version\":" + std::to_string(schema_version);
  out += "}";
  return out;
}

// FNV-1a 64 稳定 hash (仅用于科学 payload 语义; 顺序固定)
std::string Provenance::science_hash() const {
  uint64_t h = 1469598103934665603ULL;
  auto mix = [&h](const std::string& s) {
    for (char ch : s) { h ^= static_cast<unsigned char>(ch); h *= 1099511628211ULL; }
    h ^= 0xff;
    h *= 1099511628211ULL;
  };
  mix(source_commit);
  mix(pipeline_hash);
  mix(module_build_id);
  mix(config_hash);
  mix(input_hash);
  mix(created_utc);
  mix(platform);
  mix(data_schema_id);
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
  return std::string(buf);
}

bool DataArtifactDescriptor::validate(std::string* err) const {
  if (id.id.empty()) {
    if (err) *err = "artifact id empty";
    return false;
  }
  if (data_schema_id.size() < 5 || data_schema_id.rfind("DATA-", 0) != 0) {
    if (err) *err = "data_schema_id must start with DATA-: " + data_schema_id;
    return false;
  }
  if (shape.dims.empty()) {
    if (err) *err = "shape empty";
    return false;
  }
  if (shape.num_elements() == 0) {
    if (err) *err = "shape zero elements";
    return false;
  }
  if (unit == UnitId::UNKNOWN && coordinate != CoordinateFrame::PIXEL) {
    // PIXEL 坐标允许 UNKNOWN; 天球/科学坐标必须有单位
    if (err) *err = "science coordinate requires unit";
    return false;
  }
  return true;
}

bool DataArtifactDescriptor::to_json(std::string* out) const {
  std::string s = "{";
  s += key("id", id.id) + ",";
  s += key("data_schema_id", data_schema_id) + ",";
  s += "\"schema_version\":" + std::to_string(schema_version) + ",";
  s += key("scalar", scalar_type_name(scalar)) + ",";
  s += key("unit", unit_name(unit)) + ",";
  s += key("coordinate", coordinate == CoordinateFrame::PIXEL ? "pixel"
       : coordinate == CoordinateFrame::ICRS ? "icrs"
       : coordinate == CoordinateFrame::CONTROL_CELL ? "control_cell" : "healpix") + ",";
  s += "\"shape\":[";
  for (size_t i = 0; i < shape.dims.size(); ++i) {
    if (i) s += ",";
    s += std::to_string(shape.dims[i]);
  }
  s += "],";
  s += key("ownership", ownership == Ownership::BORROWED ? "borrowed"
       : ownership == Ownership::SHARED ? "shared"
       : ownership == Ownership::UNIQUE ? "unique" : "persisted") + ",";
  s += key("storage", storage) + ",";
  s += "\"provenance\":" + provenance.to_json();
  s += "}";
  if (out) *out = s;
  return true;
}

// 最小 JSON 解析: 提取 key 的字符串/数字值 (不依赖外部库)
namespace {
bool extract_string(const std::string& in, const char* k, std::string* out) {
  std::string pat = std::string("\"") + k + "\":\"";
  auto pos = in.find(pat);
  if (pos == std::string::npos) return false;
  pos += pat.size();
  auto end = in.find('"', pos);
  if (end == std::string::npos) return false;
  *out = in.substr(pos, end - pos);
  return true;
}
bool extract_uint(const std::string& in, const char* k, uint64_t* out) {
  std::string pat = std::string("\"") + k + "\":";
  auto pos = in.find(pat);
  if (pos == std::string::npos) return false;
  pos += pat.size();
  auto end = in.find_first_of(",}", pos);
  if (end == std::string::npos) return false;
  try { *out = std::stoull(in.substr(pos, end - pos)); } catch (...) { return false; }
  return true;
}
bool extract_shape(const std::string& in, Shape* out) {
  auto pos = in.find("\"shape\":[");
  if (pos == std::string::npos) return false;
  pos += 9;
  auto end = in.find(']', pos);
  if (end == std::string::npos) return false;
  std::string body = in.substr(pos, end - pos);
  size_t i = 0;
  while (i < body.size()) {
    while (i < body.size() && (body[i] == ',' || body[i] == ' ')) ++i;
    if (i >= body.size()) break;
    auto j = body.find(',', i);
    std::string num = (j == std::string::npos) ? body.substr(i) : body.substr(i, j - i);
    try { out->dims.push_back(std::stoull(num)); } catch (...) { return false; }
    i = (j == std::string::npos) ? body.size() : j + 1;
  }
  return !out->dims.empty();
}
}  // namespace

bool DataArtifactDescriptor::from_json(const std::string& in, DataArtifactDescriptor* out,
                                       std::string* err) {
  if (!out) { if (err) *err = "null out"; return false; }
  DataArtifactDescriptor d;
  std::string s;
  uint64_t u = 0;
  if (!extract_string(in, "id", &d.id.id)) { if (err) *err = "missing id"; return false; }
  if (!extract_string(in, "data_schema_id", &d.data_schema_id)) { if (err) *err = "missing data_schema_id"; return false; }
  if (extract_uint(in, "schema_version", &u)) d.schema_version = u;
  if (!extract_string(in, "scalar", &s)) { if (err) *err = "missing scalar"; return false; }
  if (s == "f32") d.scalar = ScalarType::F32; else if (s == "f64") d.scalar = ScalarType::F64;
  else if (s == "u16") d.scalar = ScalarType::U16; else { if (err) *err = "bad scalar " + s; return false; }
  if (!extract_string(in, "unit", &s)) { if (err) *err = "missing unit"; return false; }
  if (s == "ADU") d.unit = UnitId::ADU; else if (s == "electron") d.unit = UnitId::ELECTRON;
  else if (s == "ADU^2") d.unit = UnitId::ADU2; else if (s == "1/variance") d.unit = UnitId::INVERSE_VARIANCE;
  else if (s == "deg") d.unit = UnitId::DEGREE; else if (s == "1") d.unit = UnitId::DIMENSIONLESS;
  else if (s == "UNKNOWN") d.unit = UnitId::UNKNOWN; else { if (err) *err = "bad unit " + s; return false; }
  if (!extract_shape(in, &d.shape)) { if (err) *err = "missing shape"; return false; }
  if (extract_string(in, "storage", &d.storage)) {
    // d.storage 已由 extract_string 填入
  }
  std::string serr;
  if (!d.validate(&serr)) { if (err) *err = serr; return false; }
  *out = std::move(d);
  return true;
}

}  // namespace astrocs::core
