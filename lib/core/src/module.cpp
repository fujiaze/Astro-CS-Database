// CORE-003 / RT-005 ModuleDescriptor + Registry 实现
// registry 支持可执行模块工厂；index 用 JSON library 正确转义。
#include "astrocs/core/module.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <set>

namespace astrocs::core {

using nlohmann::json;

bool ModuleDescriptor::validate(std::string* err) const {
  if (module_id.empty()) {
    if (err) *err = "module_id empty";
    return false;
  }
  if (module_id.rfind("astrocs.", 0) != 0) {
    if (err) *err = "module_id must be namespaced astrocs.*: " + module_id;
    return false;
  }
  if (version.empty()) {
    if (err) *err = "version empty";
    return false;
  }
  if (abi != "c++17" && abi != "c") {
    if (err) *err = "abi must be c++17 or c";
    return false;
  }
  if (ports.empty()) {
    if (err) *err = "ports empty";
    return false;
  }
  bool has_input = false, has_output = false;
  std::set<std::string> port_names;
  for (const auto& p : ports) {
    if (p.data_schema_id.rfind("DATA-", 0) != 0) {
      if (err) *err = "port " + p.name + " bad data_schema_id";
      return false;
    }
    if (!port_names.insert(p.name).second) {
      if (err) *err = "duplicate port name: " + p.name;
      return false;
    }
    if (p.is_input) has_input = true; else has_output = true;
  }
  if (!has_input || !has_output) {
    if (err) *err = "module needs both input and output ports";
    return false;
  }
  // heavy+serial 拒绝（RT-005: cpu_heavy 必须 parallel_ok）
  if (execution_class == "cpu_heavy" && !parallel_ok) {
    if (err) *err = "cpu_heavy module must be parallel_ok (heavy+serial rejected)";
    return false;
  }
  // ACR production 拒绝（ACR 不接生产）
  if (module_id.rfind("astrocs.acr.", 0) == 0) {
    if (err) *err = "ACR module cannot be registered for production: " + module_id;
    return false;
  }
  if (sci_id.rfind("SCI-", 0) != 0 && !sci_id.empty()) {
    if (err) *err = "bad sci_id";
    return false;
  }
  if (alg_id.rfind("ALG-", 0) != 0 && !alg_id.empty()) {
    if (err) *err = "bad alg_id";
    return false;
  }
  if (data_id.rfind("DATA-", 0) != 0 && !data_id.empty()) {
    if (err) *err = "bad data_id";
    return false;
  }
  if (api_id.rfind("API-", 0) != 0 && !api_id.empty()) {
    if (err) *err = "bad api_id";
    return false;
  }
  if (test_id.rfind("TEST-", 0) != 0 && !test_id.empty()) {
    if (err) *err = "bad test_id";
    return false;
  }
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

Result<void> ModuleRegistry::register_factory(
    const std::string& module_id,
    std::function<std::unique_ptr<IModule>()> factory) {
  if (!factory) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "register_factory: null factory for " + module_id));
  }
  if (!modules_.count(module_id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "register_factory: module not registered first: " + module_id));
  }
  if (factories_.count(module_id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "register_factory: duplicate factory for " + module_id));
  }
  // 立即实例化一次以验证工厂可用（拒绝只注册 metadata 的假模块）
  auto probe = factory();
  if (!probe) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "register_factory: factory returned null for " + module_id));
  }
  if (probe->descriptor().module_id != module_id) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "register_factory: factory descriptor module_id mismatch: " + module_id));
  }
  factories_[module_id] = std::move(factory);
  return Result<void>::success();
}

Result<std::unique_ptr<IModule>> ModuleRegistry::create(
    const std::string& module_id) const {
  auto it = factories_.find(module_id);
  if (it == factories_.end()) {
    return Result<std::unique_ptr<IModule>>::fail(Error(ErrorDomain::DATA,
        "create: no factory for module " + module_id));
  }
  auto m = it->second();
  if (!m) {
    return Result<std::unique_ptr<IModule>>::fail(Error(ErrorDomain::DATA,
        "create: factory failed for " + module_id));
  }
  return Result<std::unique_ptr<IModule>>::ok(std::move(m));
}

bool ModuleRegistry::has_factory(const std::string& module_id) const {
  return factories_.count(module_id) > 0;
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
  json j;
  j["schema"] = "astrocs.module-index/v1";
  j["modules"] = json::array();
  for (const auto& [id, m] : modules_) {
    json mj;
    mj["module_id"] = id;
    mj["version"] = m.version;
    mj["abi"] = m.abi;
    mj["execution_class"] = m.execution_class;
    mj["parallel_ok"] = m.parallel_ok;
    mj["sci_id"] = m.sci_id;
    mj["alg_id"] = m.alg_id;
    mj["data_id"] = m.data_id;
    mj["api_id"] = m.api_id;
    mj["test_id"] = m.test_id;
    mj["executable"] = has_factory(id);
    j["modules"].push_back(std::move(mj));
  }
  if (out) *out = j.dump();
  return true;
}

}  // namespace astrocs::core
