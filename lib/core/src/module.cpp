// CORE-003 ModuleDescriptor + Registry 实现
#include "astrocs/core/module.h"

#include <cstdio>

namespace astrocs::core {

bool ModuleDescriptor::validate(std::string* err) const {
  if (module_id.empty()) { if (err) *err = "module_id empty"; return false; }
  if (module_id.rfind("astrocs.", 0) != 0) {
    if (err) *err = "module_id must be namespaced astrocs.*: " + module_id; return false;
  }
  if (version.empty()) { if (err) *err = "version empty"; return false; }
  if (abi != "c++17" && abi != "c") { if (err) *err = "abi must be c++17 or c"; return false; }
  if (ports.empty()) { if (err) *err = "ports empty"; return false; }
  bool has_input = false, has_output = false;
  for (const auto& p : ports) {
    if (p.data_schema_id.rfind("DATA-", 0) != 0) {
      if (err) *err = "port " + p.name + " bad data_schema_id"; return false;
    }
    if (p.is_input) has_input = true; else has_output = true;
  }
  if (!has_input || !has_output) { if (err) *err = "module needs both input and output ports"; return false; }
  if (sci_id.rfind("SCI-", 0) != 0 && !sci_id.empty()) { if (err) *err = "bad sci_id"; return false; }
  return true;
}

Result<void> ModuleRegistry::register_module(const ModuleDescriptor& desc) {
  std::string err;
  if (!desc.validate(&err)) return Result<void>::fail(Error(ErrorDomain::DATA, err));
  if (modules_.count(desc.module_id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "duplicate module id: " + desc.module_id));
  }
  modules_[desc.module_id] = desc;
  return Result<void>::success();
}

const ModuleDescriptor* ModuleRegistry::find(const std::string& module_id) const {
  auto it = modules_.find(module_id);
  return it == modules_.end() ? nullptr : &it->second;
}

std::vector<std::string> ModuleRegistry::module_ids() const {
  std::vector<std::string> out;
  out.reserve(modules_.size());
  for (const auto& [k, v] : modules_) out.push_back(k);
  return out;
}

bool ModuleRegistry::export_index_json(std::string* out) const {
  std::string s = "{\"schema\":\"astrocs.module-index/v1\",\"modules\":[";
  bool first = true;
  for (const auto& [id, m] : modules_) {
    if (!first) s += ",";
    first = false;
    s += "{\"module_id\":\"" + id + "\",\"version\":\"" + m.version +
         "\",\"abi\":\"" + m.abi + "\",\"execution_class\":\"" + m.execution_class +
         "\",\"sci_id\":\"" + m.sci_id + "\",\"test_id\":\"" + m.test_id + "\"}";
  }
  s += "]}";
  if (out) *out = s;
  return true;
}

}  // namespace astrocs::core
