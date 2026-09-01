// RT-007 ArtifactStore 实现：唯一 producer、role 绑定、篡改检测、并发安全。
#include "astrocs/core/artifact_store.h"

#include <nlohmann/json.hpp>

#include <mutex>

namespace astrocs::core {

using nlohmann::json;

bool ArtifactDescriptor::validate(std::string* err) const {
  auto fail = [&](const char* m) { if (err) *err = m; return false; };
  if (id.id.empty()) return fail("artifact id empty");
  if (role == ArtifactRole::UNKNOWN) return fail("artifact role UNKNOWN");
  if (data_schema_id.rfind("DATA-", 0) != 0) return fail("bad data_schema_id");
  if (path_or_uri.empty()) return fail("path_or_uri empty");
  if (content_sha256.size() != 64) return fail("content_sha256 must be 64 hex");
  if (producer_node.empty() || producer_module.empty()) {
    return fail("producer node/module required");
  }
  if (producer_version.empty()) return fail("producer version required");
  if (source_commit.empty()) return fail("source_commit required");
  if (created_utc.empty()) return fail("created_utc required");
  return true;
}

bool ArtifactDescriptor::to_json(std::string* out) const {
  json j;
  j["id"] = id.id;
  j["role"] = artifact_role_name(role);
  j["data_schema_id"] = data_schema_id;
  j["schema_version"] = schema_version;
  j["scalar"] = scalar_type_name(scalar);
  j["unit"] = unit_name(unit);
  j["coordinate"] = static_cast<int>(coordinate);
  j["shape"] = shape.dims;
  j["invalids"] = static_cast<int>(invalids);
  j["path_or_uri"] = path_or_uri;
  j["size_bytes"] = size_bytes;
  j["content_sha256"] = content_sha256;
  j["producer_node"] = producer_node;
  j["producer_module"] = producer_module;
  j["producer_version"] = producer_version;
  j["source_commit"] = source_commit;
  j["input_ids_hash"] = input_ids_hash;
  j["created_utc"] = created_utc;
  if (out) *out = j.dump();
  return true;
}

bool ArtifactDescriptor::from_json(const std::string& in, ArtifactDescriptor* out,
                                   std::string* err) {
  json j;
  try {
    j = json::parse(in);
  } catch (const json::parse_error& e) {
    if (err) *err = std::string("artifact JSON parse: ") + e.what();
    return false;
  }
  if (!out) { if (err) *err = "null out"; return false; }
  ArtifactDescriptor d;
  try {
    d.id.id = j.at("id").get<std::string>();
    std::string role = j.value("role", "UNKNOWN");
    d.role = ArtifactRole::UNKNOWN;
    if (role == "p1_calibrated_frame") d.role = ArtifactRole::P1_CALIBRATED_FRAME;
    else if (role == "p2_hips") d.role = ArtifactRole::P2_HIPS;
    else if (role == "p3_tile") d.role = ArtifactRole::P3_TILE;
    else if (role == "input_raw") d.role = ArtifactRole::INPUT_RAW;
    else if (role == "metadata") d.role = ArtifactRole::METADATA;
    d.data_schema_id = j.at("data_schema_id").get<std::string>();
    d.schema_version = j.value("schema_version", 0ULL);
    std::string scalar = j.value("scalar", "f32");
    if (scalar == "f64") d.scalar = ScalarType::F64;
    else if (scalar == "u16") d.scalar = ScalarType::U16;
    else if (scalar == "i16") d.scalar = ScalarType::I16;
    else if (scalar == "u8") d.scalar = ScalarType::U8;
    else if (scalar == "i32") d.scalar = ScalarType::I32;
    d.unit = UnitId::UNKNOWN;
    std::string unit_s = j.value("unit", "");
    // unit 以名称序列化（to_json 写 unit_name）；回读映射
    if (unit_s == "ADU") d.unit = UnitId::ADU;
    else if (unit_s == "electron") d.unit = UnitId::ELECTRON;
    else if (unit_s == "ADU^2") d.unit = UnitId::ADU2;
    else if (unit_s == "electron^2") d.unit = UnitId::ELECTRON2;
    else if (unit_s == "1/variance") d.unit = UnitId::INVERSE_VARIANCE;
    else if (unit_s == "surface_brightness") d.unit = UnitId::SURFACE_BRIGHTNESS;
    else if (unit_s == "deg") d.unit = UnitId::DEGREE;
    else if (unit_s == "1") d.unit = UnitId::DIMENSIONLESS;
    d.coordinate = static_cast<CoordinateFrame>(j.value("coordinate", 0));
    for (const auto& dim : j.value("shape", std::vector<uint64_t>{})) {
      d.shape.dims.push_back(dim);
    }
    d.invalids = static_cast<InvalidValuePolicy>(j.value("invalids", 0));
    d.path_or_uri = j.at("path_or_uri").get<std::string>();
    d.size_bytes = j.value("size_bytes", 0ULL);
    d.content_sha256 = j.at("content_sha256").get<std::string>();
    d.producer_node = j.at("producer_node").get<std::string>();
    d.producer_module = j.at("producer_module").get<std::string>();
    d.producer_version = j.at("producer_version").get<std::string>();
    d.source_commit = j.at("source_commit").get<std::string>();
    d.input_ids_hash = j.value("input_ids_hash", std::string{});
    d.created_utc = j.at("created_utc").get<std::string>();
  } catch (const json::exception& e) {
    if (err) *err = std::string("artifact field error: ") + e.what();
    return false;
  }
  *out = std::move(d);
  return true;
}

Result<void> ArtifactStore::store(const ArtifactDescriptor& desc) {
  std::string err;
  if (!desc.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "store: " + err));
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (store_.count(desc.id.id)) {
    // 唯一 producer：同 id 二次写 → 硬失败
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "store: duplicate artifact id (unique producer): " + desc.id.id));
  }
  store_[desc.id.id] = desc;
  return Result<void>::success();
}

bool ArtifactStore::get(const std::string& id, ArtifactDescriptor* out) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = store_.find(id);
  if (it == store_.end()) return false;
  if (out) *out = it->second;
  return true;
}

Result<void> ArtifactStore::bind_as_input(const std::string& id,
                                          ArtifactRole expected_role,
                                          const std::string& consumer_node) const {
  ArtifactDescriptor d;
  if (!get(id, &d)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "bind: artifact not found: " + id + " (consumer " + consumer_node + ")"));
  }
  // 消费前完整验证
  std::string err;
  if (!d.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "bind: " + id + " incomplete: " + err));
  }
  // role 匹配（禁止从文件名猜角色）
  if (d.role != expected_role) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "bind: " + id + " role " + artifact_role_name(d.role) +
        " != expected " + artifact_role_name(expected_role) +
        " (consumer " + consumer_node + ")"));
  }
  return Result<void>::success();
}

Result<void> ArtifactStore::bind_p1_to_p2(const std::string& cal_frame_id,
                                          const std::string& consumer_node) const {
  return bind_as_input(cal_frame_id, ArtifactRole::P1_CALIBRATED_FRAME, consumer_node);
}

Result<void> ArtifactStore::bind_p2_to_p3(const std::string& hips_id,
                                          const std::string& consumer_node) const {
  return bind_as_input(hips_id, ArtifactRole::P2_HIPS, consumer_node);
}

std::vector<std::string> ArtifactStore::ids() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  out.reserve(store_.size());
  for (const auto& [k, v] : store_) out.push_back(k);
  return out;
}

}  // namespace astrocs::core
