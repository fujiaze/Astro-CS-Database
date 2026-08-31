// AstroCS Core Contracts — RT-007 类型化 ArtifactStore + 跨阶段绑定
// Artifact descriptor 含 ID、role、schema/version、unit、coordinate、dtype/shape、validity、
// path/URI、size/hash、producer node/module/version、source commit、input IDs/hashes、created UTC。
// 消费前验证完整；禁止从文件名/目录猜角色。
// P1 output 必须成为 P2 input，P2 HiPS 成为 P3 input；篡改 path/hash/unit/schema/换 producer 硬失败。
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs::core {

// 产物角色（跨阶段绑定语义；禁止从文件名猜）
enum class ArtifactRole : uint8_t {
  UNKNOWN = 0,
  P1_CALIBRATED_FRAME = 1,  // P1 输出 → P2 输入
  P2_HIPS = 2,              // P2 输出（HiPS）→ P3 输入
  P3_TILE = 3,              // P3 输出（FITS tile）
  INPUT_RAW = 4,            // 外部输入（raw frames / HiPS source）
  METADATA = 5,             // 配置/清单
};

constexpr const char* artifact_role_name(ArtifactRole r) noexcept {
  switch (r) {
    case ArtifactRole::UNKNOWN: return "UNKNOWN";
    case ArtifactRole::P1_CALIBRATED_FRAME: return "p1_calibrated_frame";
    case ArtifactRole::P2_HIPS: return "p2_hips";
    case ArtifactRole::P3_TILE: return "p3_tile";
    case ArtifactRole::INPUT_RAW: return "input_raw";
    case ArtifactRole::METADATA: return "metadata";
  }
  return "UNKNOWN";
}

// 完整产物描述（RT-007）
struct ArtifactDescriptor {
  ArtifactId id;                    // 稳定 ID
  ArtifactRole role = ArtifactRole::UNKNOWN;
  std::string data_schema_id;       // DATA-xxx
  uint64_t schema_version = 0;
  ScalarType scalar = ScalarType::F32;
  UnitId unit = UnitId::UNKNOWN;
  CoordinateFrame coordinate = CoordinateFrame::PIXEL;
  Shape shape;
  InvalidValuePolicy invalids = InvalidValuePolicy::NAN_INVALID;
  std::string path_or_uri;          // FITS/HiPS 路径或 URI
  uint64_t size_bytes = 0;
  std::string content_sha256;       // 科学 payload hash
  // producer 溯源
  std::string producer_node;        // node_id
  std::string producer_module;      // module_id
  std::string producer_version;     // module version
  std::string source_commit;        // 源码 commit
  std::string input_ids_hash;       // 输入集合 id+hash 的稳定 hash
  std::string created_utc;          // UTC 时间

  // 完整性校验（消费前必须通过；任何缺失/非法 → false）
  bool validate(std::string* err) const;

  // 稳定序列化（roundtrip）
  bool to_json(std::string* out) const;
  static bool from_json(const std::string& in, ArtifactDescriptor* out, std::string* err);
};

// 类型化 ArtifactStore：跨阶段唯一绑定（RT-007）
// - 唯一 producer：同 id 二次写入 → 硬失败
// - role 绑定：P2 input 必须是 P1_CALIBRATED_FRAME 或 INPUT_RAW；P3 input 必须是 P2_HIPS
// - 篡改检测：content_sha256/path/unit/schema/producer 不一致 → 硬失败
// - 并发安全（RT-003 语义）
class ArtifactStore {
 public:
  ArtifactStore() = default;
  ArtifactStore(const ArtifactStore&) = delete;
  ArtifactStore& operator=(const ArtifactStore&) = delete;

  // 写入（唯一 producer；duplicate → DATA error）
  Result<void> store(const ArtifactDescriptor& desc);

  // 读取（消费前调用 validate 全检；id 不存在 → false）
  bool get(const std::string& id, ArtifactDescriptor* out) const;

  // 消费前绑定检查：确认 role 符合预期；篡改硬失败
  Result<void> bind_as_input(const std::string& id, ArtifactRole expected_role,
                             const std::string& consumer_node) const;

  std::vector<std::string> ids() const;

  // P1→P2 绑定辅助：claim P2 input（P1_CALIBRATED_FRAME）
  Result<void> bind_p1_to_p2(const std::string& cal_frame_id,
                             const std::string& consumer_node) const;
  // P2→P3 绑定辅助：claim P3 input（P2_HIPS）
  Result<void> bind_p2_to_p3(const std::string& hips_id,
                             const std::string& consumer_node) const;

  size_t size() const { return store_.size(); }

 private:
  std::map<std::string, ArtifactDescriptor> store_;
  mutable std::mutex mu_;
};

}  // namespace astrocs::core
