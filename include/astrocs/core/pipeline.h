// AstroCS Core Contracts — CORE-004 Pipeline IR + 静态验证
#pragma once

#include "astrocs/core/contracts.h"

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
  std::string config_json;  // 原始 config 子串(校验用)
};

struct PipelineIR {
  std::string pipeline_id;
  std::string version;
  std::vector<PipelineNode> nodes;
  std::map<std::string, std::string> outputs;  // port -> "artifact:..."

  std::string to_json() const;
};

enum class IrError : uint8_t {
  NONE = 0,
  PARSE = 1,           // JSON 结构/必填缺失
  UNKNOWN_MODULE = 2,  // 模块未注册
  MISSING_PORT = 3,    // 端口缺失/不匹配
  DATA_MISMATCH = 4,   // DATA schema 不兼容
  UNIT_MISMATCH = 5,   // 单位/坐标系冲突
  DUPLICATE_PRODUCER = 6,  // 重复 producer
  CYCLE = 7,           // DAG 环
  SERIAL_HEAVY = 8,    // cpu_heavy 声明 serial
  UNCONSUMED = 9,      // 未消费的必需产物
};

struct IrIssue {
  IrError kind = IrError::NONE;
  std::string node_id;
  std::string detail;
};

// PipelineIRParser: 解析 canonical IR JSON (schema: astrocs.pipeline/v1)
class PipelineIRParser {
 public:
  // 解析 + 基础结构校验 (schema 字段/必填/资源 class)
  Result<PipelineIR> parse(const std::string& json_text) const;

  // 静态验证: 需要模块注册表(未知模块/端口) + 依赖图(环/producer)
  std::vector<IrIssue> validate(const PipelineIR& ir,
                                const std::vector<std::string>& known_modules) const;
};

}  // namespace astrocs::core
