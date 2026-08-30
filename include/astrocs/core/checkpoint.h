// AstroCS Core Contracts — CORE-007 Checkpoint 与幂等恢复
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <map>
#include <string>
#include <vector>

namespace astrocs::core {

// Checkpoint 记录: 节点产物必须在 Artifact 原子提交后才写入 (API-001 §4)
struct CheckpointEntry {
  std::string node_id;
  std::vector<std::string> artifact_ids;  // 已提交 artifact
  std::string run_id;
  uint64_t seq = 0;
};

// CheckpointStore: 故障注入后只重跑未提交节点; 不接受半成品
class CheckpointStore {
 public:
  CheckpointStore() = default;

  Result<void> begin(const std::string& run_id);
  Result<void> commit_node(const std::string& node_id,
                           const std::vector<std::string>& artifact_ids);
  bool is_committed(const std::string& node_id) const;
  std::vector<std::string> committed_nodes() const;
  const std::string& run_id() const { return run_id_; }
  size_t size() const { return entries_.size(); }

  // 幂等: 相同 run_id 重复 commit 同一节点 -> 覆盖为同一结果(不报错)
  Result<void> replay_same_run(const std::string& node_id,
                               const std::vector<std::string>& artifact_ids);

 private:
  std::string run_id_;
  std::vector<CheckpointEntry> entries_;
  std::map<std::string, size_t> index_;  // node_id -> entries_ idx
};

}  // namespace astrocs::core
