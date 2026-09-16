// CORE-007 / RT-007 Checkpoint 实现: 节点产物原子提交后才可恢复
// RT-007 扩展:
//   - phase scope 隔离: begin(run_id, scope); 不同 scope 的 checkpoint 互不可见;
//   - resume 输入门: 恢复只接受 ArtifactStore 中 hash/schema 匹配的已发布 artifact。
#include "astrocs/core/checkpoint.h"

namespace astrocs::core {

Result<void> CheckpointStore::begin(const std::string& run_id) {
  return begin(run_id, "");
}

Result<void> CheckpointStore::begin(const std::string& run_id,
                                    const std::string& phase_scope) {
  if (run_id.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "checkpoint: run_id empty"));
  }
  if (!entries_.empty()) {
    if (run_id_ != run_id) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "checkpoint: run_id changed mid-run (" + run_id_ + " -> " + run_id + ")"));
    }
    if (scope_ != phase_scope) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "checkpoint: phase scope changed mid-run (" +
          (scope_.empty() ? "<none>" : scope_) + " -> " +
          (phase_scope.empty() ? "<none>" : phase_scope) + ")"));
    }
  }
  run_id_ = run_id;
  scope_ = phase_scope;
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
  e.scope = scope_;
  e.seq = entries_.size() + 1;
  entries_.push_back(std::move(e));
  index_[key(node_id, scope_)] = entries_.size() - 1;
  return Result<void>::success();
}

bool CheckpointStore::is_committed(const std::string& node_id) const {
  // 兼容既有单 run 用法: 无 scope 时按 node_id 直接查;
  // 有 scope 时按当前 scope 隔离（不同 scope 提交互不可见）。
  return is_committed_in_scope(node_id, scope_);
}

bool CheckpointStore::is_committed_in_scope(const std::string& node_id,
                                            const std::string& phase_scope) const {
  return index_.count(key(node_id, phase_scope)) > 0;
}

std::vector<std::string> CheckpointStore::committed_nodes() const {
  std::vector<std::string> out;
  for (const auto& e : entries_) {
    if (e.scope == scope_) out.push_back(e.node_id);
  }
  return out;
}

Result<void> CheckpointStore::replay_same_run(const std::string& node_id,
                                              const std::vector<std::string>& artifact_ids) {
  // 幂等: 同 run_id + 同 scope 重复提交 -> 覆盖为同一结果
  const std::string k = key(node_id, scope_);
  if (index_.count(k)) {
    auto& e = entries_[index_[k]];
    e.artifact_ids = artifact_ids;
    e.run_id = run_id_;
    e.scope = scope_;
    return Result<void>::success();
  }
  return commit_node(node_id, artifact_ids);
}

// ── resume 输入门 ──
Result<void> validate_resume_inputs(const ArtifactStore& store,
                                    const std::vector<ResumeBinding>& want,
                                    const std::string& consumer_node) {
  for (const auto& w : want) {
    ArtifactDescriptor d;
    if (!store.get(w.artifact_id, &d)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: artifact not published in store: " + w.artifact_id +
          " (consumer " + consumer_node + ")"));
    }
    std::string err;
    if (!d.validate(&err)) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: " + w.artifact_id + " incomplete: " + err +
          " (consumer " + consumer_node + ")"));
    }
    if (w.expected_schema_id.empty()) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: expected schema required for " + w.artifact_id));
    }
    if (d.data_schema_id != w.expected_schema_id ||
        d.schema_version != w.expected_schema_version) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: " + w.artifact_id + " schema " + d.data_schema_id + " v" +
          std::to_string(d.schema_version) + " != expected " + w.expected_schema_id +
          " v" + std::to_string(w.expected_schema_version) +
          " (consumer " + consumer_node + ")"));
    }
    if (w.expected_sha256.empty()) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: expected sha256 required for " + w.artifact_id));
    }
    if (d.content_sha256 != w.expected_sha256) {
      return Result<void>::fail(Error(ErrorDomain::DATA,
          "resume: " + w.artifact_id + " content hash mismatch (consumer " +
          consumer_node + ")"));
    }
  }
  return Result<void>::success();
}

}  // namespace astrocs::core
