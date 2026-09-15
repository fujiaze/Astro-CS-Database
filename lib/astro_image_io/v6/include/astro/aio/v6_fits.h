/* v6_fits.h — 原生 FITS 流式写出 + DATASUM/CHECKSUM + 重开独立验证
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/astro_image_io/v6/。
 * 语义锚 (ALG-P3-008 §7):
 *   1. 写临时文件（同目录，保证 rename 原子）   -> v6_atomic_publish.h
 *   2. flush / close / fsync                    -> v6_atomic_publish.h
 *   3. 计算并写 DATASUM/CHECKSUM                -> 本文件
 *   4. 原子 rename 到目标                        -> v6_atomic_publish.h
 *   5. 重开并独立验证（shape/WCS/层完整性/单位） -> 本文件 verify_fits_file()
 *   DATASUM/CHECKSUM 采用 FITS 4.0 §4.4.2.5 / Rob Seaman 32-bit 1 补码算法
 *   (与 CFITSIO ffesum/ffdsum 同构，本实现独立转写；astropy 为外部真值)。
 *
 * HDU 布局: PRIMARY (SIMPLE=T) + 扩展 (XTENSION='IMAGE', PCOUNT/GCOUNT)；
 * 每个 HDU 头/数据段 2880 对齐；DATASUM/CHECKSUM 为 END 前最后两个关键字。
 */
#ifndef ASTROCS_V6_AIO_FITS_H
#define ASTROCS_V6_AIO_FITS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "astro/aio/v6_validation.h"

namespace astrocs {
namespace aio {

// ── 关键字 ────────────────────────────────────────────────────────────────
struct FitsCard {
  enum class Type { kString, kLogical, kInteger, kReal };
  std::string keyword;
  Type type = Type::kString;
  std::string value;    // String: 原文（不含引号）；Logical: "T"/"F"；数值: 文本
  std::string comment;  // 允许为空

  static FitsCard make_string(const std::string& k, const std::string& v,
                              const std::string& c = "") {
    return FitsCard{k, Type::kString, v, c};
  }
  static FitsCard make_logical(const std::string& k, bool v,
                               const std::string& c = "") {
    return FitsCard{k, Type::kLogical, v ? "T" : "F", c};
  }
  static FitsCard make_integer(const std::string& k, long long v,
                               const std::string& c = "") {
    return FitsCard{k, Type::kInteger, std::to_string(v), c};
  }
  static FitsCard make_real(const std::string& k, double v,
                            const std::string& c = "");
};

std::string fits_format_card(const FitsCard& card);  // 恒 80 字节

// ── 校验和原语 (FITS 4.0 §4.4.2.5) ────────────────────────────────────────
std::uint32_t fits_ones_complement_sum(const std::uint8_t* data, std::size_t n);
std::uint32_t fits_oc_add(std::uint32_t a, std::uint32_t b);
std::array<char, 16> fits_encode_checksum(std::uint32_t value);
bool fits_decode_checksum(const std::array<char, 16>& in, std::uint32_t* value);

std::size_t fits_padded_size(std::size_t n);  // 上取整到 2880 的倍数

// ── HDU 规格 ──────────────────────────────────────────────────────────────
struct FitsHduSpec {
  std::string extname;                  // 空 => PRIMARY HDU
  int bitpix = -64;                     // 8/16/32/-32/-64
  std::vector<std::size_t> naxis;       // 维度 (>=1)
  std::vector<FitsCard> cards;          // 用户关键字（BUNIT/WCS/...）
  bool with_pcount_gcount = true;       // 扩展 HDU 专用
  // PRIMARY 后存在扩展 HDU 时必须写 EXTEND=T（FITS 4.0 标准；astropy 亦据此
  // 计算 PRIMARY CHECKSUM，缺失会导致外部校验失败）。
  bool primary_has_extensions = false;
};

std::size_t fits_bytes_per_pixel(int bitpix);
std::size_t fits_data_size(const FitsHduSpec& spec);

// ── 流式写出器 ────────────────────────────────────────────────────────────
class FitsStreamWriter {
 public:
  explicit FitsStreamWriter(int fd) : fd_(fd) {}

  bool begin_hdu(const FitsHduSpec& spec, std::string* err);
  // 追加数据块；len 必须使累计字节数不超 data_size。
  bool write_data(const void* data, std::size_t len, std::string* err);
  // 补零对齐 + 计算/回写 DATASUM/CHECKSUM + 自检。
  bool end_hdu(std::string* err);
  bool fsync_now(std::string* err);

  std::size_t bytes_written() const { return file_pos_; }
  std::uint32_t last_datasum() const { return last_datasum_; }
  std::string last_checksum() const { return last_checksum_; }

 private:
  bool pwrite_all(std::uint64_t off, const void* data, std::size_t len,
                  std::string* err);
  // 累加校验和 + 写盘（不做 data_size 上限检查；供数据段补零使用）。
  bool write_raw(const void* data, std::size_t len, std::string* err);

  int fd_ = -1;
  std::uint64_t file_pos_ = 0;
  bool in_hdu_ = false;
  std::vector<std::uint8_t> header_;
  std::uint64_t hdu_start_ = 0;
  std::size_t datasum_card_off_ = 0;    // 头内偏移（0-based，80 对齐）
  std::size_t checksum_card_off_ = 0;
  std::size_t data_size_ = 0;
  std::size_t data_written_ = 0;
  std::uint32_t acc_hi_ = 0;
  std::uint32_t acc_lo_ = 0;
  std::uint64_t acc_words_ = 0;
  int acc_pending_ = -1;  // 跨块奇数字节缓存
  std::uint32_t last_datasum_ = 0;
  std::string last_checksum_;
  std::string last_datasum_text_;
};

// ── 重开验证 ──────────────────────────────────────────────────────────────
struct ExpectedHdu {
  std::string extname;  // 空 => PRIMARY
  int bitpix = 0;
  std::vector<std::size_t> naxis;
  std::string bunit;
  bool check_bunit = false;
  // 关键字逐字回读（WCS/单位等）；重开后必须完全一致。
  std::vector<std::pair<std::string, std::string>> string_cards;
};

struct FitsHduInfo {
  std::string extname;
  bool is_primary = false;
  int bitpix = 0;
  std::vector<std::size_t> naxis;
  std::string bunit;
  std::uint32_t datasum = 0;
  std::string checksum;
  std::uint64_t header_bytes = 0;
  std::uint64_t data_bytes = 0;
};

struct FitsVerifyResult {
  bool ok = false;
  std::string error;                  // 文件级错误（打不开/截断）
  std::vector<Violation> violations;  // 门违规
  std::vector<FitsHduInfo> hdus;
};

// expected 为空 => 只做 CHECKSUM/DATASUM/结构自洽验证。
FitsVerifyResult verify_fits_file(const std::string& path,
                                  const std::vector<ExpectedHdu>& expected);

// 便捷：直接对内存中的完整 FITS 字节做验证（重开路径与文件一致）。
FitsVerifyResult verify_fits_bytes(const std::vector<std::uint8_t>& bytes,
                                   const std::vector<ExpectedHdu>& expected);

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_FITS_H
