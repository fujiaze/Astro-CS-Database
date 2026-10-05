/* atomic_publish.h — 原子发布原语 (tmp + fsync + rename + 重开验证)
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/infrastructure/aio/product_io/。
 * 语义锚 (ALG-P3-008 §7.3, 宪章 §4.3/§14.4)。
 * **步序正本唯一** = docs/engineering/contracts/ATOMIC_PUBLISH.md §4（生产者协议
 * 6 步序）+ §4.1（机制层步序与"两序关系"的唯一声明处）—— 本文件**不复写步序表**，
 * 只记机制层的实际顺序与调用方约束：
 *   写 tmp(同目录) -> flush/close/fsync(+DATASUM/CHECKSUM 由 fits 写进 tmp) ->
 *   **原子 rename 到目标** -> **重开已发布对象并由 verify_fn 独立验证** -> 失败即撤销。
 * 与 §4 生产者序（校验先于 rename）的关系见 §4.1：内容面等价；机制层**检测面更强**
 * （能检出 rename 之后才发生的损坏），但**可见性更弱**（有未验证对象短暂可见的窗口、
 * 撤销会删除已替换的目标）⇒ 调用方必须按下面的 durability 三态处置。
 * 失败或取消：删除 tmp；rename 未发生 ⇒ 目标根无可见半成品（见 PublishResult::durability
 * 的 kNotPublished）。
 *
 * P-174：发布终态**恰有三态**，由 PublishResult::durability 表达（与 status 正交，
 * 不动冻结的 PublishStatus 数值域）——旧口径「失败/取消 ⇒ 无正式产品」只覆盖首尾两态，
 * 缺「已发布但持久化未确认」：
 *   ① kNotPublished  = 未发布：目标根无本次发布的正式产品（rename 未发生，或 rename
 *      后验证失败且已撤销成功）；
 *   ② kDurable       = 已发布且持久化已确认：目标可见，且 rename 后的**目录 fsync 成功**
 *      （目录项确认落盘）；判据 = rename 已生效 + 目录 fsync 成功；
 *   ③ kNotDurable    = 已发布但持久化未确认：目标**已可见且不可回滚**，而目录项持久化
 *      无证据 —— rename 后目录 fsync 失败（status 仍 fail-closed 为 kErrIo），或本平台
 *      无目录 fsync 等价物 / 调用方显式关闭 opts.fsync_directory（此时 status 可为 kOk，
 *      但不得据此声称「已确认落盘」）。
 *   自洽判据：renamed == (durability != kNotPublished)，且已观测路径上目标确实存在，
 *   见 publish_result_consistent()（外部项来自 rename 后那次读盘，不是自证）。
 *   调用方处置：kNotDurable **不得回滚删除**（产品已可见，删了就毁掉已发布对象）、
 *   **不得静默当成功**（该产品面在崩溃后可能消失），须显式可见（日志/manifest 标注
 *   「持久化未确认」）并允许对父目录重跑 fsync 确认；只有 kNotPublished 才按
 *   「无正式产品」清理/重发。
 *
 * 状态码 0..15/70/71 数值与 lib/algorithms/drizzle/hips/include/acsd/hips/publish.h
 * aio_publish_status_v1、lib/include/acsd/io/aio_abi_v1.h 对齐（编译期 static_assert）。
 */
#ifndef ACSD_V6_AIO_ATOMIC_PUBLISH_H
#define ACSD_V6_AIO_ATOMIC_PUBLISH_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace acsd {
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

// P-174 三终态：发布终态的**持久化事实**，与 status 正交。数值域独立于冻结的
// PublishStatus（0..15/70/71，见文件末尾 static_assert），因此不触碰
// lib/include/acsd/io/aio_abi_v1.h 与 lib/algorithms/drizzle/hips/include/acsd/hips/publish.h。
enum class PublishDurability : int {
  kDurable = 0,       // 目标可见 + 目录项持久化已确认（rename 后目录 fsync 成功）
  kNotDurable = 1,    // 目标可见（已发布、不可回滚）+ 持久化未确认 => 第三态
  kNotPublished = 2,  // 目标根无正式产品（默认）
};

const char* publish_durability_name(PublishDurability d);

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
  // renamed 语义（P-174 明确）：目标位置此刻是否由本次发布的对象占据 ——
  // rename 已生效且未被撤销 ⇒ true；rename 未发生，或 rename 后验证失败且撤销
  // 成功（目标根已无该产品）⇒ false。恒等式：renamed == (durability !=
  // kNotPublished)，由 publish_result_consistent() 判定。
  bool renamed = false;
  // 外部观测: 本次发布是否**读盘看过**目标 (target_checked), 以及读到目标是否
  // 真的在盘上 (target_present)。只在 rename 已生效的路径上置位 —— 早退路径上
  // 的「目标存在」可能来自上一次发布, 拿它当判据会造出恒红门。
  bool target_checked = false;
  bool target_present = false;
  bool tmp_residue = false;  // 失败后 tmp 是否残留（必须恒为 false）
  // P-174 第三态：只描述「正式产品面的持久化事实」，与 status 正交（status 仍是
  // 调用方的主判据）。默认 kNotPublished；详见文件头三态说明。
  PublishDurability durability = PublishDurability::kNotPublished;
};

// P-174 终态自洽判据：存在正式产品（kDurable | kNotDurable）⇔ renamed；
// 并且在已读盘观测的路径上，正式产品**真的在盘上**。
// 第二项不可省：只比 renamed 与 durability 这两个**由同一控制流成对赋值**的字段
// 是一条恒等式 —— 本库三个生产者对自产出的每一个 PublishResult 都返回 true，
// 八个生产点拿它当红灯门却永不触发，恒真门第 ③ 型（往返自证）。外部项的期望量
// 来自文件系统 (rename 返回 0 ⇒ 目标必然在盘上), 与被检验量不同源, 谓词可假。
inline bool publish_result_consistent(const PublishResult& r) {
  const bool published = r.durability != PublishDurability::kNotPublished;
  if (r.renamed != published) return false;
  if (!r.target_checked) return true;   // 未到读盘观测点：只判内部不变量
  return r.target_present == published;
}

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
}  // namespace acsd

#endif  // ACSD_V6_AIO_ATOMIC_PUBLISH_H
