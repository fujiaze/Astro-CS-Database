// AstroCS Core Contracts — CORE-003 ModuleDescriptor + Registry
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace astrocs::core {

// 模块端口 (MOD 合同: 输入输出端口)
struct PortDescriptor {
  std::string name;
  std::string data_schema_id;  // DATA-xxx
  bool is_input = true;
  UnitId unit = UnitId::UNKNOWN;
  CoordinateFrame coordinate = CoordinateFrame::PIXEL;
};

// 模块描述 (MOD: 唯一 ID/版本/ABI/端口/config/执行模型/合同引用)
struct ModuleDescriptor {
  std::string module_id;      // "astrocs.phase1.calibration"
  std::string version;        // "1.x"
  std::string abi;            // "c++17" | "c"
  std::vector<PortDescriptor> ports;
  std::string config_schema;  // JSON schema 引用或内嵌
  std::string execution_class;  // cpu_heavy | io | light
  bool parallel_ok = false;
  std::string sci_id;   // SCI-xxx
  std::string alg_id;   // ALG-xxx
  std::string data_id;  // DATA-xxx
  std::string api_id;   // API-xxx
  std::string test_id;  // TEST-xxx

  bool validate(std::string* err) const;
};

// ModuleRegistry: 模块唯一注册, duplicate/ABI/合同校验
class ModuleRegistry {
 public:
  Result<void> register_module(const ModuleDescriptor& desc);
  const ModuleDescriptor* find(const std::string& module_id) const;
  size_t size() const { return modules_.size(); }
  std::vector<std::string> module_ids() const;

  // 机器可读索引 (CORE-003: 可导出 machine index)
  bool export_index_json(std::string* out) const;

 private:
  std::map<std::string, ModuleDescriptor> modules_;
};

}  // namespace astrocs::core
