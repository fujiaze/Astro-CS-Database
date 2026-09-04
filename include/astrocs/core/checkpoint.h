// AstroCS Core Contracts — CORE-007 Checkpoint 与幂等恢复
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/artifact_store.h"
#include "astrocs/core/contracts.h"

#include <map>
#include <string>
#include <vector>

namespace astrocs::core {

// Checkpoint 记录: 节点产物必须在 Artifact 原子提交后才写入 (API-001 §4)
// RT-007: scope = 阶段/运行作用域隔离键（如 "phase2"/"phase3"）。不同 scope 的
// checkpoint 互不可见（阶段 run 不能误读另一阶段 checkpoint）; 空 scope 兼容
// 既有单 run 用法。
struct CheckpointEntry {
  std::string node_id;
  std::vector<std::string> artifact_ids;  // 已提交 artifact
  std::string run_id;
  std::string scope;                      // RT-007: 阶段作用域（空 = 无作用域）
  uint64_t seq = 0;
};

// CheckpointStore: 故障注入后只重跑未提交节点; 不接受半成品
// RT-007 恢复边界:
//   - scope 隔离: begin(run_id, scope) 后 commit 的节点只在该 scope 可见;
//     不同 scope 读/恢复互不可见;
//   - run 边界: 同 store 中途切换 run_id 或 scope → 拒绝（防止误读/串写）。
class CheckpointStore {
 public:
  CheckpointStore() = default;

  // 既有单参数 begin（scope 空，兼容 CORE-007 既有用法）
  Result<void> begin(const std::string& run_id);
  // RT-007: 带阶段 scope 的 begin。scope 中途切换（有已提交节点时）→ 拒绝。
  Result<void> begin(const std::string& run_id, const std::string& phase_scope);
  Result<void> commit_node(const std::string& node_id,
                           const std::vector<std::string>& artifact_ids);
  // RT-007: 仅当节点属于给定 scope 才视为已提交（跨 scope 误读检测）。
  bool is_committed(const std::string& node_id) const;
  bool is_committed_in_scope(const std::string& node_id,
                             const std::string& phase_scope) const;
  std::vector<std::string> committed_nodes() const;
  const std::string& run_id() const { return run_id_; }
  const std::string& scope() const { return scope_; }
  size_t size() const { return entries_.size(); }

  // 幂等: 相同 run_id 重复 commit 同一节点 -> 覆盖为同一结果(不报错)
  Result<void> replay_same_run(const std::string& node_id,
                               const std::vector<std::string>& artifact_ids);

 private:
  std::string run_id_;
  std::string scope_;                                    // RT-007: 当前作用域
  std::vector<CheckpointEntry> entries_;
  std::map<std::string, size_t> index_;  // scope + "\x1f" + node_id -> entries_ idx

  std::string key(const std::string& node_id, const std::string& sc) const {
    return sc + "\x1f" + node_id;
  }
};

// ── RT-007 resume 门: resume 只使用 hash/schema 匹配的已发布 artifact ──
// 恢复/重放节点输入前, 用已发布 ArtifactStore 逐项校验: id 存在、role/完整、
// schema id/version 与期望一致、content_sha256 与期望一致（任何不一致 →
// DATA 错误, 拒绝 resume）。禁止仅凭 id/文件名猜测内容后恢复。
struct ResumeBinding {
  std::string artifact_id;          // store 中的稳定 id
  std::string expected_schema_id;   // 期望 DATA-xxx
  uint64_t expected_schema_version = 0;
  std::string expected_sha256;      // 64 hex; 与 store 内容一致才放行
};

Result<void> validate_resume_inputs(const ArtifactStore& store,
                                    const std::vector<ResumeBinding>& want,
                                    const std::string& consumer_node);

}  // namespace astrocs::core
