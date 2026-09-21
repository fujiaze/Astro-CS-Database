/* v6_atomic_publish.h — 原子发布原语 (tmp + fsync + rename + 重开验证)
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/infrastructure/aio/v6/。
 * 语义锚 (ALG-P3-008 §7.3, 宪章 §4.3/§14.4):
 *   1. 写临时文件（与目标同目录，保证 rename 原子）
 *   2. flush / close / fsync
 *   3. (DATASUM/CHECKSUM 由 v6_fits 写入 tmp 内容)
 *   4. 原子 rename 到目标
 *   5. 重开并独立验证（由 verify_fn 提供，失败即撤销发布）
 *   失败或取消：删除 tmp，rename 不发生 -> 目标根无可见半成品。
 *
 * 状态码 0..15/70/71 数值与 lib/algorithms/drizzle/hips/include/astrocs/hips/publish.h
 * aio_publish_status_v1、lib/include/astrocs/io/aio_abi_v1.h 对齐（编译期 static_assert）。
 */
#ifndef ASTROCS_V6_AIO_ATOMIC_PUBLISH_H
#define ASTROCS_V6_AIO_ATOMIC_PUBLISH_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace astrocs {
namespace aio {

enum class PublishStatus : int {
  kOk = 0,
  kErrParam = 1,
  kErrAbiMismatch = 2,
  kErrNomem = 3,
  kErrIo = 4,
  kErrUnsupported = 5,
  kErrCancelled = 6,
  kErrState = 7,
  kErrTruncated = 8,
  kErrBadHeader = 9,
  kErrMismatch = 10,
  kErrChecksum = 11,
  kErrNanInf = 12,
  kErrDiskfull = 13,
  kErrDeclInvalid = 14,
  kErrHashMismatch = 15,
  kErrInternal = 70,
  kStatusCount = 71,
};

const char* publish_status_name(PublishStatus s);

// 取消探针：返回 true 表示调用方要求取消。
using CancelFn = std::function<bool()>;
// 内容写出器：向 fd 顺序写数据；返回 false 表示失败（不得静默）。
using FileWriterFn =
    std::function<bool(int fd, const CancelFn& cancel, std::string* err)>;
// staging 目录构造器：在 stage_dir 下生成完整目录树。
using DirBuilderFn = std::function<bool(const std::string& stage_dir,
                                        const CancelFn& cancel,
                                        std::string* err)>;
// 发布后重开独立验证：返回 false 使本次发布被撤销。
using VerifyFn = std::function<bool(const std::string& path, std::string* err)>;

struct PublishOptions {
  bool verify_after_rename = true;
  bool fsync_directory = true;
  // 验证失败时撤销已 rename 的产物（保证无可见半成品）。
  bool remove_on_verify_failure = true;
};

struct PublishResult {
  PublishStatus status = PublishStatus::kErrInternal;
  std::string target;
  std::string tmp;
  std::string sha256_hex;
  std::uint64_t bytes_written = 0;
  std::string message;
  bool renamed = false;
  bool tmp_residue = false;  // 失败后 tmp 是否残留（必须恒为 false）
};

// 原子发布单个文件。
PublishResult atomic_publish_file(const std::string& target,
                                  const FileWriterFn& writer,
                                  const VerifyFn& verify,
                                  const PublishOptions& opts,
                                  const CancelFn& cancel);

// 便捷：一次性写出字节。
PublishResult atomic_write_bytes(const std::string& target,
                                 const std::string& bytes,
                                 const VerifyFn& verify,
                                 const PublishOptions& opts,
                                 const CancelFn& cancel);

// 原子发布整目录（HiPS 树）：staging 目录同父 -> rename 整树原子出现。
PublishResult atomic_publish_directory(const std::string& target_dir,
                                       const DirBuilderFn& builder,
                                       const VerifyFn& verify,
                                       const PublishOptions& opts,
                                       const CancelFn& cancel);

// 工具：顺序写满 fd（处理部分写/取消）。
bool write_all_fd(int fd, const void* data, std::size_t len,
                  const CancelFn& cancel, std::string* err);
// 工具：文件 SHA-256 十六进制（失败返回 false）。
bool sha256_file_hex(const std::string& path, std::string* hex);
// 工具：递归删除目录（不跟随符号链接）；路径不存在 => true。
bool remove_tree(const std::string& path);
// 工具：递归 fsync 目录树 (文件先, 目录后序)。
bool fsync_tree(const std::string& path, std::uint64_t* n_files,
                std::string* err);

static_assert(static_cast<int>(PublishStatus::kOk) == 0 &&
                  static_cast<int>(PublishStatus::kErrParam) == 1 &&
                  static_cast<int>(PublishStatus::kErrIo) == 4 &&
                  static_cast<int>(PublishStatus::kErrState) == 7 &&
                  static_cast<int>(PublishStatus::kErrDiskfull) == 13 &&
                  static_cast<int>(PublishStatus::kErrInternal) == 70 &&
                  static_cast<int>(PublishStatus::kStatusCount) == 71,
              "V6 AIO publish status 数值域与 AIO-001/AIO-002 冻结一致");

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_ATOMIC_PUBLISH_H
