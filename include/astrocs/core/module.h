// AstroCS Core Contracts — CORE-003 ModuleDescriptor + Registry
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace astrocs::core {

class RunContext;  // 前向声明（context.h 依赖本头；execute 只取引用）

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

// ── ModulePlan: 模块执行前的资源/工作量计划 (RT-005) ──
// plan() 不得修改输入；只输出工作量、内存、I/O、并行轴与 kernel 请求。
struct ModulePlan {
  std::string node_id;                  // 计划所属 node（非空）
  uint64_t work_units = 0;              // 工作量（tile/row-band/样本数）
  uint64_t estimated_memory_bytes = 0;  // 峰值内存估计（NodePlan 预算用）
  std::vector<std::string> parallel_axes;  // 并行轴（"tile"/"row"/"sample"…）
  std::vector<std::string> kernel_ids;     // 请求的 provider kernel IDs
  bool cpu_heavy = false;               // execution class 判定
};

// ── IModule: 模块生命周期接口 (RT-005) ──
// 生命周期: describe → validate_config → plan → create → execute → inspect → destroy。
// execute 只使用 Runtime 提供的 lease/context/artifacts；inspect 不重执行科学计算。
class IModule {
 public:
  virtual ~IModule() = default;

  // 静态描述（descriptor；非空、不得与注册合同矛盾）
  virtual const ModuleDescriptor& descriptor() const noexcept = 0;

  // 校验 config JSON（input: config schema JSON 文本, 非空, 拥有方=调用者,
  // lifetime=本次调用有效; output err 可空, 由调用者持有）
  virtual Result<void> validate_config(const std::string& config_json) = 0;

  // 生成执行计划（input: node_id, config; output: ModulePlan）。
  // 禁止修改任何输入；线程安全：可并发调用。
  virtual Result<ModulePlan> plan(const std::string& node_id,
                                  const std::string& config_json) = 0;

  // 执行（input: RunContext& 引用, 拥有方=Runtime, 非空, 仅当前线程调用；
  // 输出 Result<void>；取消/异常必须归还 lease）
  virtual Result<void> execute(RunContext& ctx) = 0;

  // 结构化诊断（output JSON 文本；不重执行科学计算；线程安全：可并发）
  virtual Result<std::string> inspect() = 0;
};

// ModuleRegistry: 模块唯一注册, duplicate/ABI/合同校验 + 工厂创建（RT-005 可执行）
class ModuleRegistry {
 public:
  Result<void> register_module(const ModuleDescriptor& desc);
  const ModuleDescriptor* find(const std::string& module_id) const;
  size_t size() const { return modules_.size(); }
  std::vector<std::string> module_ids() const;

  // RT-005: 注册模块工厂（可执行模块；校验与 descriptor 一致）
  // factory: 每次调用返回新的 IModule 实例（owner=调用者）；nullptr → 拒绝注册。
  Result<void> register_factory(const std::string& module_id,
                                std::function<std::unique_ptr<IModule>()> factory);

  // 创建模块实例；未注册工厂 → Error(DATA, "no factory")
  Result<std::unique_ptr<IModule>> create(const std::string& module_id) const;

  // 机器可读索引 (CORE-003: 可导出 machine index; RT-005: JSON library 正确转义)
  bool export_index_json(std::string* out) const;

  // 该模块是否可执行（有工厂）
  bool has_factory(const std::string& module_id) const;

 private:
  std::map<std::string, ModuleDescriptor> modules_;
  std::map<std::string, std::function<std::unique_ptr<IModule>()>> factories_;
};

}  // namespace astrocs::core
