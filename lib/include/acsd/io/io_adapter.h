// ACSD I/O Adapter — IO-001 Artifact 事务 + I/O 边界
#pragma once

#include "acsd/core/artifact.h"
#include "acsd/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace acsd::io {
using acsd::core::Error;
using acsd::core::ErrorDomain;
using acsd::core::Result;

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

  // close + verify (写错误/长度/校验和) -> rename; 失败时清理临时文件并给确定错误
  // (verify 对 tmp 全量重算 FNV-1a 校验和, 防"长度一致但内容损坏"的假成功)
  Result<void> commit();

  // 放弃: 删除临时文件
  void abort();

  bool active() const { return active_; }

 private:
  // FNV-1a/64 参数: 偏移基 0xcbf29ce484222325 = 14695981039346656037; 乘数
  // 1099511628211 (0x100000001b3) 见 io_adapter.cpp 的 fnv_update。两者取自 FNV
  // 参考实现而非本仓自定: 用其公布的测试向量 (""→0xcbf29ce484222325、"a"→
  // 0xaf63dc4c8601ec8c、"foobar"→0x85944171f73967e8) 可逐位复算。
  // 只此一处声明: 该值只服务于 commit() 的写后读回比对, 期望侧 (write 收到的内存
  // 载荷) 与实测侧 (从 tmp 文件读回的落盘字节) 物理不同源, 两端共用同一参数;
  // 故它是有效的完整性判据, 且改动不改变任何落盘读数 (checksum_ 不出本类)。
  // 收成单一声明是为排除 .h 与 .cpp 各写一份字面量而漂移 —— 两者曾各自写成少一位的
  // 转录值 (标准偏移基整除 10), 三处全同才没被当场发现。
  static constexpr uint64_t kFnv1a64OffsetBasis = 14695981039346656037ULL;
  std::string target_;
  std::string tmp_;
  uint64_t written_ = 0;
  uint64_t checksum_ = kFnv1a64OffsetBasis;
  bool write_failed_ = false;  // 任一次 write 失败即锁存, commit 时按合同报错
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

}  // namespace acsd::io
