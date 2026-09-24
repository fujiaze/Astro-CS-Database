// ACSD Core — ARCH-505 阶段块流执行器（生产节点命名块装配）
//
// 依据：ASTROCS_DESIGN.md §8.2（阶段内命名块内存管线与块生命周期）、§8.3（三阶段调度器）、
//       §10.3；ENGINEERING_SPEC.md §2；CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md；
//       块流规格唯一事实源：eng/contracts/block_flow/stage_block_flow.json（由注册表派生，
//       机器门 eng/tools/quality/check_block_flow_spec.py 校验，禁止手改漂移）。
//
// 本执行器把「生产节点 = 命名块读写 + 调度器编排」变成可运行、可证伪的合同：
//   - 节点只能读写它在规格里声明的块（未声明读/写 ⇒ fail-closed，不静默）；
//   - 块生命周期由执行器掌管（节点不得自行销毁块，避免绕过引用计数）；
//     SHORT = 最后一个声明消费者用完即销毁；FRAME = 活到本单元结束；RUN = 随产品到导出；
//   - 单元结束不得残留 SHORT 块（short_residual_bytes 必须为 0）；
//   - 构建期用 BlockDagValidator 校验块图（四条非法图判据），非法图不得进入运行期；
//   - 探针记录每节点耗时与块生灭，供审计与 PERF-501 调优。
//
// 边界：本模块**不做科学计算**；节点执行体由装配方注入（ARCH-505 生产装配）。
#pragma once

#include "astrocs/core/block_frame.h"
#include "astrocs/core/normalize_workflow.h"   // ProbeSink / ProbeEvent

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace astrocs::core {

// 块在块流规格里的角色（与 BlockLifecycle 的映射见 lifecycle_of）
enum class BlockRole : std::uint8_t {
  EXTERNAL_IN = 0,    // 阶段输入（来自上游磁盘产品），由调用方注入
  EXTERNAL_OUT = 1,   // 阶段对外产品，必须发布
  STAGE = 2,          // 阶段内跨节点存活（活到单元结束）
  SHORT = 3,          // 单消费者，最后一次消费即销毁
};

const char* block_role_name(BlockRole r);
bool parse_block_role(const std::string& s, BlockRole* out);
BlockLifecycle lifecycle_of(BlockRole r);

// 一个生产节点的块流声明 + 执行体
struct BlockFlowNode {
  std::string module_id;                 // 与 module_ports.registry.json 一致
  std::string stage;                     // normalize / mosaic / export
  std::string operation;
  std::string entry;
  std::vector<std::string> reads;
  std::vector<std::string> writes;
  // 执行体：在 frame 内读已声明的块、创建已声明的块；返回 false = 节点失败。
  std::function<bool(BlockFrame&)> run;
};

struct StageBlockFlowSpec {
  std::string stage;
  std::vector<BlockFlowNode> nodes;                  // 必须已按拓扑序
  std::map<std::string, BlockRole> roles;            // block -> role
  std::map<std::string, std::vector<std::string>> consumers;   // block -> 消费者节点集合
  std::map<std::string, std::string> producer;       // block -> 生产者节点（EXTERNAL_IN 为空）
  // 声明式元数据（可选；缺省按 F64/空单位）
  std::map<std::string, BlockDtype> dtypes;
  std::map<std::string, std::string> units;
};

struct BlockFlowOutcome {
  bool ok = false;
  std::string error;                     // 失败原因（含节点名与块名，便于定位）
  std::uint64_t checksum = 0;            // 产品块内容哈希（按块名升序、逐字节）
  std::size_t peak_bytes = 0;            // 单元内在途块字节峰值
  std::size_t blocks_created = 0;
  std::size_t blocks_destroyed = 0;
  std::size_t short_residual_bytes = 0;  // 单元结束残留的 SHORT 块字节（必须为 0）
  std::map<std::string, std::size_t> product_bytes;   // EXTERNAL_OUT 块名 → 字节
  double wall_seconds = 0.0;
};

class StageBlockFlow {
 public:
  StageBlockFlow(StageBlockFlowSpec spec, ProbeSink* sink);

  // 构建期块图校验：非空 = 规格非法（不得进入运行期）
  const std::vector<BlockGraphIssue>& graph_issues() const { return graph_issues_; }
  bool spec_valid() const { return graph_issues_.empty(); }

  // 注入 EXTERNAL_IN 块（单元开始前调用；重名覆盖 = 拒绝）
  bool set_input(const std::string& name, std::size_t element_count);
  bool set_input_f64(const std::string& name, const std::vector<double>& values);

  // 运行一个单元（P1/P3 = 一帧；P2 = 一个天球窗口）
  BlockFlowOutcome run_unit(const std::string& unit_id);

  // 读取产品块（run_unit 之后有效；未声明的名字返回 nullptr）
  const Block* product(const std::string& name) const;
  const std::vector<std::string>& product_names() const { return product_names_; }

  const StageBlockFlowSpec& spec() const { return spec_; }

 private:
  StageBlockFlowSpec spec_;
  ProbeSink* sink_ = nullptr;
  std::vector<BlockGraphIssue> graph_issues_;
  std::map<std::string, std::size_t> inputs_;
  std::map<std::string, std::vector<double>> input_values_;
  std::map<std::string, BlockFrame> products_;   // 每个产品块保留一个独立 frame 以延长寿命
  std::vector<std::string> product_names_;
};

// 从块流规格 JSON（eng/contracts/block_flow/stage_block_flow.json）解析一个阶段的规格。
// 只解析声明，不解析执行体（执行体由装配方注入）。返回空 error 表示成功。
bool parse_stage_block_flow_spec(const std::string& json_text, const std::string& stage,
                                 StageBlockFlowSpec* out, std::string* error);

}  // namespace astrocs::core
