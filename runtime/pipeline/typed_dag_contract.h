// AstroCS Runtime — RT-001 typed DAG node 合同（冻结头，编译期合同）
//
// 本头是 RT-001「类型化运行图」node 定义的 C++ 合同形态；权威 JSON 形态为
// runtime/pipeline/typed_dag.schema.json，语义执行形态为 runtime/pipeline/typed_dag.py
// （compiler: phase scope / 类型 / 单位 / 坐标 / shape / 必需端口 / 无环 /
//  无重复 producer / 每节点唯一 operation / 无跨 Phase edge）。
//
// 冻结语义（AUD-001 IRF-0007 修复：IR 节点 id 与 CLI 收集端 / 真实执行符号不匹配）:
//  1. 每个 DAG 节点绑定唯一 (module_id, operation)；operation 对应一个真实入口
//     (entry)；模块不得以多个节点重复包装同一 Session（aggregate Session module
//     不入 typed DAG —— module_ports.registry.json 强制每 module 恰一个 operation）。
//  2. 数据边以 ArtifactRef("artifact:<id>") 显式表达；禁止隐式文件路径。
//  3. 端口类型元数据 (data_schema_id/unit/coordinate/scalar/shape_hint) 由注册表
//     提供，compiler 沿边做类型/单位/坐标/shape 一致性检查。
//  4. 单 Phase 图：所有 node 的 module.phase 必须等于图顶层 phase；跨 Phase 数据
//     边在图内禁止（阶段间仅磁盘产品交换，见约束 A.6）。
//
// 所有权/线程: 本头为纯数据结构合同（无资源）；TypedDagNode 值语义，可拷贝。
// 本任务为合同 + 骨架：执行由 RT-002..007 接线真实 scheduler/Runtime。
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace astrocs::runtime {

// Phase 标识（约束 A.3；与 artifact manifest run.phase 枚举一致）
enum class PhaseId : uint8_t { PHASE1 = 1, PHASE2 = 2, PHASE3 = 3 };

// 资源类（与 PipelineIR resources.class 枚举一致）
enum class ResourceClass : uint8_t {
  METADATA = 0,
  IO = 1,
  CPU_LIGHT = 2,
  CPU_HEAVY = 3,
};

// 端口方向
enum class PortDirection : uint8_t { INPUT = 0, OUTPUT = 1 };

// 端口类型元数据（DATA-001 typed 字段子集；单位/坐标枚举与 astrocs::core::UnitId/
// CoordinateFrame 数值一致，见 include/astrocs/core/artifact.h）
struct TypedPort {
  std::string name;             // 端口名（与 registry 端口一致）
  PortDirection direction = PortDirection::INPUT;
  std::string data_schema_id;   // "DATA-xxx"
  std::string unit;             // "ADU" | "electron" | "1/variance" | "1" | "deg" ...
  std::string coordinate;       // "PIXEL" | "ICRS" | "HEALPIX" | "CONTROL_CELL"
  std::string scalar;           // "f32" | "f64" | "u16" | "i16" | "u8" | "i32"
  std::string shape_hint;       // "[H,W]" | "[N,2]" | "[]" ...
};

// 边引用：显式 artifact 引用（禁隐式文件路径）
struct ArtifactRef {
  std::string artifact_id;      // "sha256:..." 或 "artifact:<id>" 语义 id
  bool valid() const { return !artifact_id.empty(); }
};

// RT-001 node 合同：id/module_id/operation/ports/config/resource class/phase。
// 冻结字段顺序不得重排（ABI 冒烟测试锁定 layout 断言）。
struct TypedDagNode {
  std::string node_id;          // 图内唯一
  std::string module_id;        // "astrocs.phase2.coverage"（registry 登记）
  std::string operation;        // 唯一真实 operation（registry 每 module 恰一）
  std::string entry;            // 真实入口符号名（注册表声明；DLL 绑定属 ABI-00x）
  PhaseId phase = PhaseId::PHASE2;
  ResourceClass resource_class = ResourceClass::CPU_HEAVY;
  std::vector<TypedPort> ports; // input + output 端口类型元数据
  // port -> ArtifactRef（inputs 为消费边；outputs 为产出边）
  std::map<std::string, ArtifactRef> inputs;
  std::map<std::string, ArtifactRef> outputs;
  std::string config_json;      // 内联 config JSON（原样保留）
};

// 编译通过的类型化计划图（供 scheduler 消费；本轮骨架）。
struct TypedPlanGraph {
  std::string schema;           // "astrocs.plan-graph/v1"
  std::string pipeline_id;
  PhaseId phase = PhaseId::PHASE2;
  std::string version;
  std::vector<TypedDagNode> nodes;
  bool acyclic = false;
  bool single_operation_per_node = false;
};

// 编译结果（供 CLI/Runtime 在 RT-002+ 接线时消费；本轮冻结错误码枚举）。
enum class DagIssueKind : uint8_t {
  NONE = 0,
  STRUCT = 1,
  PHASE_SCOPE = 2,
  UNKNOWN_MODULE = 3,
  UNKNOWN_OPERATION = 4,
  AMBIGUOUS_OPERATION = 5,
  MISSING_PORT = 6,
  UNKNOWN_PORT = 7,
  DATA_MISMATCH = 8,
  TYPE_MISMATCH = 9,
  UNIT_MISMATCH = 10,
  COORDINATE_MISMATCH = 11,
  SHAPE_MISMATCH = 12,
  IMPLICIT_PATH = 13,
  CYCLE = 14,
  DUPLICATE_PRODUCER = 15,
  CROSS_PHASE = 16,
  MODULE_REUSED = 17,
  UNPRODUCED_OUTPUT = 18,
  CONFIG_INVALID = 19,
};

// 编译通过的节点名列表 + 状态（无执行；执行语义属 RT-002..007）
struct TypedDagCompileStatus {
  bool ok = false;
  std::vector<DagIssueKind> issues;   // 失败时的错误码
  TypedPlanGraph graph;               // ok=true 时的计划图
};

// 编译期函数（骨架：typed_dag.py 为执行语义；此头冻结 C++ 侧视图）
// 实现接线（CLI / Runtime 编译入口）在 RT-002 由运行时 owner 接入；本任务只冻结合同。

}  // namespace astrocs::runtime
