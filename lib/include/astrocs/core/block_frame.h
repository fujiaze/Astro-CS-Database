// AstroCS Core — ARCH-501 阶段内命名块与块生命周期（内存管线）
//
// 依据：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器）、§8.2（阶段内命名块
//       内存管线与块生命周期）；ENGINEERING_SPEC.md §4.1（管线纪律）；
//       CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md（块元数据 9 字段、
//       创建-消费-销毁状态机、DAG 四条非法图判据、provenance 流转、显式降级）。
//
// 边界：本模块**不做科学计算**，也不装配三阶段生产节点（ARCH-505）；它只提供
//       阶段内内存管线的块容器与生命周期语义。
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace astrocs::core {

// ── 块元数据（CONTRACT-501 PIPELINE_BLOCK_CONTRACT §2 的 9 字段）──────────────
enum class BlockDtype : std::uint8_t { F64 = 0, F32 = 1, I32 = 2, U8 = 3 };

// 生命周期阶段：short = 节点内即时消耗；frame = 帧内跨节点；run = 存活到导出
enum class BlockLifecycle : std::uint8_t { SHORT = 0, FRAME = 1, RUN = 2 };

// 状态机（合同 §3）：CREATED → CONSUMED → DESTROYED（取消/异常同销毁）
enum class BlockState : std::uint8_t { CREATED = 0, CONSUMED = 1, DESTROYED = 2 };

std::size_t dtype_size(BlockDtype d);
const char* dtype_name(BlockDtype d);

struct BlockMeta {
  std::string name;                     // 阶段内唯一，小写蛇形
  std::vector<std::int64_t> shape;      // 维度
  BlockDtype dtype = BlockDtype::F64;
  std::string unit;                     // 物理单位；空 = 无量纲
  bool optional = false;                // 可缺性：缺失必须显式降级，不得静默
  std::string producer;                 // 生产者节点 ID（唯一）
  std::vector<std::string> consumers;   // 消费者节点 ID 集合（可空 = 终态块）
  BlockLifecycle lifecycle = BlockLifecycle::FRAME;
};

// ── DAG 校验（合同 §2 的四条非法图判据）────────────────────────────────────
enum class BlockGraphError : std::uint8_t {
  NONE = 0,
  CONSUME_MISSING = 1,       // 消费不存在的块
  DUPLICATE_PRODUCER = 2,    // 同一块被重复生产
  LIFECYCLE_MISMATCH = 3,    // 生命周期声明与实际消费者跨度不一致
  EMPTY_NAME = 4,            // 块名为空/非法
  UNKNOWN_NODE = 5,          // 生产者/消费者不在节点集合内
};

struct BlockGraphIssue {
  BlockGraphError kind = BlockGraphError::NONE;
  std::string block;
  std::string detail;
};

struct BlockGraph {
  std::vector<std::string> node_ids;   // 本阶段节点集合
  std::vector<BlockMeta> blocks;
};

class BlockDagValidator {
 public:
  // 空返回 = 合法；非空 = 非法（构建期必须报错，不得运行期容忍）。
  static std::vector<BlockGraphIssue> validate(const BlockGraph& graph);
};

// ── 块（运行时对象）────────────────────────────────────────────────────────
class Block {
 public:
  Block(BlockMeta meta, std::size_t element_count);

  const BlockMeta& meta() const { return meta_; }
  BlockState state() const { return state_; }
  std::size_t element_count() const { return element_count_; }
  std::size_t bytes() const { return data_.size(); }
  bool is_alive() const { return state_ != BlockState::DESTROYED; }

  // 数据面：f64 视图（仅 F64 块；其他 dtype 返回 nullptr）
  double* f64();
  const double* f64() const;
  std::uint8_t* raw() { return data_.empty() ? nullptr : data_.data(); }
  const std::uint8_t* raw() const { return data_.empty() ? nullptr : data_.data(); }

  // provenance 头部 KV（随块流转，下游不得丢弃）
  void set_provenance(const std::string& key, const std::string& value);
  const std::map<std::string, std::string>& provenance() const { return provenance_; }

  // 剩余未消费的消费者集合（引用计数语义）
  const std::vector<std::string>& remaining_consumers() const { return remaining_; }

 private:
  friend class BlockFrame;
  BlockMeta meta_;
  BlockState state_ = BlockState::CREATED;
  std::size_t element_count_ = 0;
  std::vector<std::uint8_t> data_;
  std::map<std::string, std::string> provenance_;
  std::vector<std::string> remaining_;   // 尚未声明消费的消费者
};

// ── PipelineFrame：阶段内命名块容器 ────────────────────────────────────────
class BlockFrame {
 public:
  BlockFrame() = default;
  BlockFrame(const BlockFrame&) = delete;
  BlockFrame& operator=(const BlockFrame&) = delete;

  // 创建块；重名或 element_count 超上限 => nullptr（不覆盖、不静默）
  Block* create(const BlockMeta& meta, std::size_t element_count);

  Block* find(const std::string& name);
  const Block* find(const std::string& name) const;

  // 声明 consumer 用完该块：全部消费者用完 ⇒ 立即销毁并归还内存（返回 true）。
  // 块不存在 / 状态非 CREATED / consumer 未声明 ⇒ false（不静默成功）。
  bool consume(const std::string& name, const std::string& consumer);

  // 显式销毁（取消/异常路径必须走这里，保证无泄漏）
  bool destroy(const std::string& name);

  // 缺块降级：optional=true 的块缺失时消费方须显式登记（合同 §4）
  void mark_degraded(const std::string& block, const std::string& reason);
  const std::vector<std::pair<std::string, std::string>>& degradations() const {
    return degradations_;
  }

  std::size_t bytes_alive() const;
  std::size_t blocks_alive() const;
  std::size_t blocks_total_created() const { return created_; }

  // 阶段结束/取消：销毁全部存活块，返回归还字节数
  std::size_t destroy_all();

 private:
  std::map<std::string, std::unique_ptr<Block>> blocks_;
  std::vector<std::pair<std::string, std::string>> degradations_;
  std::size_t created_ = 0;
};

}  // namespace astrocs::core
