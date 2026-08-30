// AstroCS I/O Adapter — IO-001 Artifact 事务 + I/O 边界
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::io {
using astrocs::core::Error;
using astrocs::core::ErrorDomain;
using astrocs::core::Result;

// IO-001: I/O adapter 不 include Runtime scheduler 或模块实现 (依赖方向: core <- io)

// Artifact 事务: 同目录临时文件 -> close/verify -> rename 原子替换
// (Windows rename 失败给确定错误; 禁止直接覆盖生产文件)
class ArtifactTransaction {
 public:
  ArtifactTransaction() = default;
  ArtifactTransaction(const ArtifactTransaction&) = delete;
  ArtifactTransaction& operator=(const ArtifactTransaction&) = delete;

  // 创建临时文件 (同目录: <target>.tmp.<pid>.<seq>); 返回临时路径
  Result<std::string> begin(const std::string& target_path);

  // 写入校验数据 (可多次调用; 累计校验值)
  void write(const char* data, size_t n);

  // close + verify (长度/校验) -> rename; 失败时清理临时文件并给确定错误
  Result<void> commit();

  // 放弃: 删除临时文件
  void abort();

  bool active() const { return active_; }

 private:
  std::string target_;
  std::string tmp_;
  uint64_t written_ = 0;
  uint64_t checksum_ = 1469598103934665603ULL;  // FNV-1a seed
  bool active_ = false;
};

// I/O adapter 接口: 模块经此访问文件系统 (IO-001: 禁止模块直接路径猜测)
class IoAdapter {
 public:
  virtual ~IoAdapter() = default;

  virtual Result<std::string> read_text(const std::string& path) const = 0;
  virtual Result<void> write_text(const std::string& path, const std::string& content) const = 0;
  virtual Result<void> read_bytes(const std::string& path, std::vector<uint8_t>* out) const = 0;
  virtual bool exists(const std::string& path) const = 0;

  // artifact 事务 (原子写)
  virtual Result<void> atomic_write(const std::string& path, const std::string& content) const = 0;
};

// 文件系统实现 (POSIX; Windows rename 失败给确定错误)
class FileIoAdapter : public IoAdapter {
 public:
  Result<std::string> read_text(const std::string& path) const override;
  Result<void> write_text(const std::string& path, const std::string& content) const override;
  Result<void> read_bytes(const std::string& path, std::vector<uint8_t>* out) const override;
  bool exists(const std::string& path) const override;
  Result<void> atomic_write(const std::string& path, const std::string& content) const override;
};

}  // namespace astrocs::io
