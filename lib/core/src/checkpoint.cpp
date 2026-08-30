// CORE-007 Checkpoint 实现: 节点产物原子提交后才可恢复
#include "astrocs/core/checkpoint.h"

namespace astrocs::core {

Result<void> CheckpointStore::begin(const std::string& run_id) {
  if (run_id.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "checkpoint: run_id empty"));
  }
  if (!entries_.empty() && run_id_ != run_id) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "checkpoint: run_id changed mid-run (" + run_id_ + " -> " + run_id + ")"));
  }
  run_id_ = run_id;
  return Result<void>::success();
}

Result<void> CheckpointStore::commit_node(const std::string& node_id,
                                          const std::vector<std::string>& artifact_ids) {
  if (run_id_.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "checkpoint: begin() not called"));
  }
  if (node_id.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "checkpoint: node_id empty"));
  }
  // 半成品拒绝: 必须至少一个已提交 artifact
  if (artifact_ids.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "checkpoint: node " + node_id + " commit with no artifacts (half-done rejected)"));
  }
  CheckpointEntry e;
  e.node_id = node_id;
  e.artifact_ids = artifact_ids;
  e.run_id = run_id_;
  e.seq = entries_.size() + 1;
  entries_.push_back(std::move(e));
  index_[node_id] = entries_.size() - 1;
  return Result<void>::success();
}

bool CheckpointStore::is_committed(const std::string& node_id) const {
  return index_.count(node_id) > 0;
}

std::vector<std::string> CheckpointStore::committed_nodes() const {
  std::vector<std::string> out;
  for (const auto& e : entries_) out.push_back(e.node_id);
  return out;
}

Result<void> CheckpointStore::replay_same_run(const std::string& node_id,
                                              const std::vector<std::string>& artifact_ids) {
  // 幂等: 同 run_id 重复提交 -> 覆盖为同一结果
  if (is_committed(node_id)) {
    auto& e = entries_[index_[node_id]];
    e.artifact_ids = artifact_ids;
    e.run_id = run_id_;
    return Result<void>::success();
  }
  return commit_node(node_id, artifact_ids);
}

}  // namespace astrocs::core
