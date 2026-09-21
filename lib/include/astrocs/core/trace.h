// AstroCS Core Contracts — RT-006 真实运行 trace 存储与 JSONL 重放
//
// 角色（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-006 +
//       14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §4/§5）：
//   TraceStore 是运行时唯一 trace 汇：executor/scheduler/module(adapter)/
//   provider 观测在真实运行点把 TraceEvent（contracts.h 定义）写入 store；
//   run 完成后 export_jsonl() 输出单行 JSONL（一事件一行，seq 递增）；
//   from_jsonl + replay 把 JSONL 还原成可按图渲染的节点/调用/产物摘要
//   （JSONL 可重放到图契约，LOG-003 渲染器消费同形态）。
//   禁止 config 值冒充观测：workers/provider/duration/artifact hash 等
//   观测字段一律由执行层在运行点填写（见 trace.cpp 各写入点注释）。
//
// 所有权/线程：TraceStore 进程内每 Runtime 一个（RT-001 唯一调度入口）；
// record 线程安全；snapshot/export 返回拷贝。TraceStore 定义在本头（不侵入
// contracts.h 布局）；contracts.h 仅前向声明 class TraceStore。
#pragma once

#include "astrocs/core/contracts.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs::core {

// ── TraceStore: 运行 trace 事件汇（线程安全；RT-006 冻结语义） ──
class TraceStore {
 public:
  TraceStore() = default;
  TraceStore(const TraceStore&) = delete;
  TraceStore& operator=(const TraceStore&) = delete;

  // 记录一条事件（线程安全）。自动填 ts_utc（若空）与全局递增 seq。
  // ts 由观测调用方先填亦可（保留观测时刻原样；seq 恒由 store 赋）。
  uint64_t record(TraceEvent e);

  // 快照（按 seq 升序拷贝）；线程安全。
  std::vector<TraceEvent> snapshot() const;

  // 全部事件 JSONL（每行一事件；nlohmann dump 正确转义）。
  std::string export_jsonl() const;

  size_t size() const { return events_.size(); }
  uint64_t next_seq() const { return seq_; }
  void clear();

  // ── 隐藏 session / 重复调用检测 ──
  // 语义（RT-006 验收）：同一次 run 内若同一真实 entry（如 *session_run*）被
  // ≥2 个不同 node_id 的 MODULE_CALL 命中，即为"隐藏 session 扇出/重复调用"
  // （多个节点重复包装同一 Session → 约束 F.1 禁止形态），返回违规描述列表。
  // 每次 run 以 run_id 为界；空 run_id 视为同一次未命名运行。
  std::vector<std::string> detect_repeated_calls() const;

 private:
  mutable std::mutex mu_;
  std::vector<TraceEvent> events_;
  uint64_t seq_ = 0;
};

// ── JSONL 可重放节点摘要（LOG-003 图渲染输入契约；RT-006 本层提供实现） ──
struct TraceNodeReplay {
  std::string node_id;
  std::string status;             // 最后终态
  std::string module_id;          // 观察到的 module（首个 MODULE_CALL）
  std::string entry;              // 观察到的真实入口（首个 MODULE_CALL）
  uint32_t call_count = 0;        // 该节点 MODULE_CALL 计数
  std::string provider;           // 最后观察 provider
  std::vector<std::string> artifact_ids;  // 发布 artifact（出现顺序去重）
  double wall_ms = 0.0;           // NODE_START→NODE_END 观测耗时
  uint64_t first_seq = 0;
};

// 从 JSONL 文本重放为按 node 聚合的可渲染摘要（schema:
// {"replay_schema":"astrocs.trace-replay/v1","nodes":[...]}）。
// 非法/不可解析行 → 跳过并计入 skipped_lines（返回值含该计数）；空输入合法。
// 永不 throw；输入任意文本安全。
struct TraceReplayResult {
  bool ok = true;                 // 至少可解析一行 → true
  std::vector<TraceNodeReplay> nodes;   // 按 node_id 字典序
  uint64_t parsed_lines = 0;
  uint64_t skipped_lines = 0;
  std::string error;              // 完全不可解析时的说明
};
TraceReplayResult trace_replay_from_jsonl(const std::string& jsonl);

// 便捷：JSONL → replay 摘要 JSON 文本（LOG-003 直接消费）。
std::string trace_replay_nodes_json(const std::string& jsonl);

// 便捷工厂（owner=调用者独占）：创建空 TraceStore。
Result<std::shared_ptr<TraceStore>> create_trace_store() noexcept;

}  // namespace astrocs::core
