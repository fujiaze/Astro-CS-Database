// ACSD Core — ARCH-501 命名块与块生命周期实现
// 依据：ASTROCS_DESIGN.md §8.2；CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md
#include "astrocs/core/block_frame.h"

#include <algorithm>
#include <set>

namespace astrocs::core {
namespace {

// 冻结上限（BLOCKER-DF-001 同源口径）：单块 4 GiB、单帧 16 GiB
constexpr std::size_t kMaxBlockBytes = static_cast<std::size_t>(1) << 32;
constexpr std::size_t kMaxFrameBytes = static_cast<std::size_t>(1) << 34;

bool valid_block_name(const std::string& n) {
  if (n.empty() || n.size() > 63) return false;
  if (!(n[0] >= 'a' && n[0] <= 'z')) return false;
  for (char c : n) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) return false;
  }
  return true;
}

}  // namespace

std::size_t dtype_size(BlockDtype d) {
  switch (d) {
    case BlockDtype::F64: return 8;
    case BlockDtype::F32: return 4;
    case BlockDtype::I32: return 4;
    case BlockDtype::U8:  return 1;
  }
  return 0;
}

const char* dtype_name(BlockDtype d) {
  switch (d) {
    case BlockDtype::F64: return "f64";
    case BlockDtype::F32: return "f32";
    case BlockDtype::I32: return "i32";
    case BlockDtype::U8:  return "u8";
  }
  return "?";
}

// ── DAG 校验：合同 §2 的四条非法图判据 + 名字/节点合法性 ────────────────────
std::vector<BlockGraphIssue> BlockDagValidator::validate(const BlockGraph& graph) {
  std::vector<BlockGraphIssue> issues;
  std::set<std::string> nodes(graph.node_ids.begin(), graph.node_ids.end());
  std::map<std::string, int> producer_count;

  for (const BlockMeta& b : graph.blocks) {
    if (!valid_block_name(b.name)) {
      issues.push_back({BlockGraphError::EMPTY_NAME, b.name,
                        "块名非法（要求小写蛇形、1..63 字符）"});
      continue;
    }
    producer_count[b.name] += 1;
    // 生产者要么为空（= 阶段外部输入，必须 optional，见下方 CONSUME_MISSING 判据），
    // 要么必须是本阶段节点集合内的真实节点。
    //
    // 修复记录（ARCH-505 实测）：原实现把「空生产者」无条件判 UNKNOWN_NODE，
    // 与同一函数下方「非 optional 块缺生产者（外部输入必须声明 optional）」条款
    // 自相矛盾 —— 结果是**阶段外部输入根本无法表达**，块流规格里 frames / hips /
    // calibrated 这类上游磁盘产品一律被判非法图，ARCH-505 的命名块装配无法落地。
    // 现按下方条款的语义统一：空生产者 + optional = 合法外部输入；
    // 空生产者 + 非 optional = CONSUME_MISSING（缺生产者且未声明可缺）。
    if (!b.producer.empty() && nodes.count(b.producer) == 0) {
      issues.push_back({BlockGraphError::UNKNOWN_NODE, b.name,
                        "生产者不在节点集合内: " + b.producer});
    }
    for (const std::string& c : b.consumers) {
      if (nodes.count(c) == 0) {
        issues.push_back({BlockGraphError::UNKNOWN_NODE, b.name,
                          "消费者不在节点集合内: " + c});
      }
    }
    // 生命周期与实际消费者跨度一致性：
    //   SHORT ⇒ 必须恰有 1 个消费者（节点内即时消耗）；
    //   RUN   ⇒ 必须至少有 1 个消费者（否则是终态块，应为 FRAME）；
    //   终态块（consumers 为空）⇒ 必须是 FRAME 或 RUN。
    if (b.lifecycle == BlockLifecycle::SHORT && b.consumers.size() != 1) {
      issues.push_back({BlockGraphError::LIFECYCLE_MISMATCH, b.name,
                        "short 生命周期要求恰有 1 个消费者，实际 " +
                            std::to_string(b.consumers.size())});
    }
    if (b.lifecycle == BlockLifecycle::RUN && b.consumers.empty()) {
      issues.push_back({BlockGraphError::LIFECYCLE_MISMATCH, b.name,
                        "run 生命周期要求至少 1 个消费者"});
    }
    // 重复消费者（同一节点重复声明 ⇒ 引用计数语义会被破坏）
    std::set<std::string> uniq(b.consumers.begin(), b.consumers.end());
    if (uniq.size() != b.consumers.size()) {
      issues.push_back({BlockGraphError::DUPLICATE_PRODUCER, b.name,
                        "consumers 含重复节点"});
    }
  }

  // 重复生产
  for (const auto& kv : producer_count) {
    if (kv.second > 1) {
      issues.push_back({BlockGraphError::DUPLICATE_PRODUCER, kv.first,
                        "同一块被 " + std::to_string(kv.second) + " 处生产"});
    }
  }

  // 消费不存在的块：消费者声明的输入块必须有生产者
  std::set<std::string> produced;
  for (const BlockMeta& b : graph.blocks) produced.insert(b.name);
  for (const BlockMeta& b : graph.blocks) {
    for (const std::string& c : b.consumers) {
      (void)c;  // 消费者是节点，不是块；本循环保留以对齐合同条款结构
    }
  }
  // 反向：每个节点声明的入参块由调用方通过 blocks 表达 ⇒ 无生产者即非法
  // （此处以「非 optional 且无生产者」表达；optional 块允许缺生产者=外部输入）
  for (const BlockMeta& b : graph.blocks) {
    if (b.producer.empty() && !b.optional) {
      issues.push_back({BlockGraphError::CONSUME_MISSING, b.name,
                        "非 optional 块缺生产者（外部输入必须声明 optional）"});
    }
  }
  return issues;
}

// ── Block ─────────────────────────────────────────────────────────────────
Block::Block(BlockMeta meta, std::size_t element_count)
    : meta_(std::move(meta)), element_count_(element_count) {
  const std::size_t bytes = element_count_ * dtype_size(meta_.dtype);
  data_.assign(bytes, 0);
  remaining_ = meta_.consumers;
}

double* Block::f64() {
  if (meta_.dtype != BlockDtype::F64 || data_.empty()) return nullptr;
  return reinterpret_cast<double*>(data_.data());
}

const double* Block::f64() const {
  if (meta_.dtype != BlockDtype::F64 || data_.empty()) return nullptr;
  return reinterpret_cast<const double*>(data_.data());
}

void Block::set_provenance(const std::string& key, const std::string& value) {
  provenance_[key] = value;
}

// ── BlockFrame ────────────────────────────────────────────────────────────
Block* BlockFrame::create(const BlockMeta& meta, std::size_t element_count) {
  if (!valid_block_name(meta.name)) return nullptr;
  if (blocks_.count(meta.name) != 0) return nullptr;   // 不覆盖
  const std::size_t bytes = element_count * dtype_size(meta.dtype);
  if (bytes > kMaxBlockBytes) return nullptr;
  if (bytes_alive() + bytes > kMaxFrameBytes) return nullptr;
  auto blk = std::unique_ptr<Block>(new Block(meta, element_count));
  Block* raw = blk.get();
  blocks_[meta.name] = std::move(blk);
  ++created_;
  return raw;
}

Block* BlockFrame::find(const std::string& name) {
  auto it = blocks_.find(name);
  if (it == blocks_.end()) return nullptr;
  return it->second->is_alive() ? it->second.get() : nullptr;
}

const Block* BlockFrame::find(const std::string& name) const {
  auto it = blocks_.find(name);
  if (it == blocks_.end()) return nullptr;
  return it->second->is_alive() ? it->second.get() : nullptr;
}

bool BlockFrame::declare_consumers(const std::string& name,
                                     const std::vector<std::string>& consumers) {
  auto it = blocks_.find(name);
  if (it == blocks_.end() || !it->second) return false;
  Block& b = *it->second;
  if (!b.remaining_.empty()) {
    // 节点自行声明了消费者：必须与规格一致，否则视为越权（生命周期归执行器）
    std::vector<std::string> a = b.remaining_, c = consumers;
    std::sort(a.begin(), a.end());
    std::sort(c.begin(), c.end());
    return a == c;
  }
  b.remaining_ = consumers;
  return true;
}

bool BlockFrame::consume(const std::string& name, const std::string& consumer) {
  auto it = blocks_.find(name);
  if (it == blocks_.end()) return false;
  Block* b = it->second.get();
  if (b->state_ != BlockState::CREATED) return false;
  auto rit = std::find(b->remaining_.begin(), b->remaining_.end(), consumer);
  if (rit == b->remaining_.end()) return false;   // 未声明的消费者不得消费
  b->remaining_.erase(rit);
  if (b->remaining_.empty()) {
    // 全部声明消费者用完 ⇒ 立即销毁、内存归还（合同 §3）
    b->state_ = BlockState::CONSUMED;
    destroy(name);
    return true;
  }
  return false;
}

bool BlockFrame::destroy(const std::string& name) {
  auto it = blocks_.find(name);
  if (it == blocks_.end()) return false;
  if (it->second->state_ == BlockState::DESTROYED) return false;
  it->second->state_ = BlockState::DESTROYED;
  it->second->data_.clear();
  it->second->data_.shrink_to_fit();
  it->second->remaining_.clear();
  blocks_.erase(it);   // 释放块对象本身
  return true;
}

void BlockFrame::mark_degraded(const std::string& block, const std::string& reason) {
  degradations_.emplace_back(block, reason);
}

std::vector<std::string> BlockFrame::names() const {
  std::vector<std::string> out;
  out.reserve(blocks_.size());
  for (const auto& kv : blocks_)
    if (kv.second && kv.second->is_alive()) out.push_back(kv.first);
  return out;   // std::map 迭代已按 key 升序
}

std::size_t BlockFrame::bytes_alive() const {
  std::size_t total = 0;
  for (const auto& kv : blocks_) {
    if (kv.second->is_alive()) total += kv.second->bytes();
  }
  return total;
}

std::size_t BlockFrame::blocks_alive() const {
  std::size_t n = 0;
  for (const auto& kv : blocks_) {
    if (kv.second->is_alive()) ++n;
  }
  return n;
}

std::size_t BlockFrame::destroy_all() {
  std::size_t freed = bytes_alive();
  for (auto& kv : blocks_) {
    kv.second->state_ = BlockState::DESTROYED;
    kv.second->data_.clear();
    kv.second->data_.shrink_to_fit();
    kv.second->remaining_.clear();
  }
  blocks_.clear();
  return freed;
}

}  // namespace astrocs::core
