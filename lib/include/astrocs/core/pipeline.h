// AstroCS Core Contracts — CORE-004 Pipeline IR + 静态验证
// RT-004: 使用真正 JSON parser (nlohmann) + schema 驱动校验；validate 接受完整 ModuleRegistry。
#pragma once

#include "astrocs/core/contracts.h"
#include "astrocs/core/module.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace astrocs::core {

// Pipeline 节点 (03 §4)
struct PipelineNode {
  std::string node_id;
  std::string module_id;
  std::string module_api;
  std::map<std::string, std::string> inputs;   // port -> "artifact:..."
  std::map<std::string, std::string> outputs;  // port -> "artifact:..."
  std::string resource_class;  // metadata|io|cpu_light|cpu_heavy
  bool parallel = false;
  std::string config_json;             // inline config 原始 JSON（若有）
  std::string config_ref_schema;       // config_ref.schema（若有）
  std::string config_ref_sha256;       // config_ref.sha256（若有）
};

struct PipelineIR {
  std::string schema;         // "astrocs.pipeline/v1" | "astrocs.pipeline/v2"
  std::string pipeline_id;
  std::string version;
  std::vector<PipelineNode> nodes;
  std::map<std::string, std::string> outputs;  // port -> "artifact:..."

  std::string to_json() const;
};

// 错误种类（数值冻结，追加不改 ABI）
enum class IrError : uint8_t {
  NONE = 0,
  PARSE = 1,             // JSON 语法/结构/必填缺失
  UNKNOWN_MODULE = 2,    // 模块未注册
  MISSING_PORT = 3,      // 端口缺失/不匹配
  DATA_MISMATCH = 4,     // DATA schema 不兼容
  UNIT_MISMATCH = 5,     // 单位冲突
  DUPLICATE_PRODUCER = 6,    // 重复 producer
  CYCLE = 7,             // DAG 环
  SERIAL_HEAVY = 8,      // cpu_heavy 声明 serial
  UNCONSUMED = 9,        // 产出但从未被消费的内部产物
  COORDINATE_MISMATCH = 10,  // 坐标系冲突
  UNPRODUCED_OUTPUT = 11,    // pipeline outputs 未被任何节点产出
};

struct IrIssue {
  IrError kind = IrError::NONE;
  std::string node_id;
  std::string detail;
};

// PipelineIRParser: 解析 canonical IR JSON（schema 驱动校验）
class PipelineIRParser {
 public:
  // 解析 + 结构/schema 校验（nlohmann 解析；必填字段；cpu_heavy→parallel）。
  Result<PipelineIR> parse(const std::string& json_text) const;

  // 静态验证：接受完整 ModuleRegistry（未知模块/端口/schema/unit/coord 冲突），
  // 依赖图（环/重复 producer），以及 pipeline outputs 未产出检测。
  std::vector<IrIssue> validate(const PipelineIR& ir,
                                const ModuleRegistry& registry) const;
};

}  // namespace astrocs::core
