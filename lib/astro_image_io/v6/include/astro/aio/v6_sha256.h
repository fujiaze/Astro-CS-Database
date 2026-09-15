/* v6_sha256.h — AstroCS V6 科学产品 I/O: SHA-256 完整性摘要
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/astro_image_io/v6/。
 * 语义锚: FZ-PROV-MINIMAL-SET (output_hash / input_product_hashes)、宪章 §4.3。
 *
 * 独立性: 本实现不依赖 OpenSSL；标准 FIPS 180-4 SHA-256，仅用于完整性锚，
 * 不是任何科学量。真值由独立 Oracle (python hashlib) 交叉校验。
 */
#ifndef ASTROCS_V6_AIO_SHA256_H
#define ASTROCS_V6_AIO_SHA256_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace astrocs {
namespace aio {

class Sha256 {
 public:
  Sha256();
  void update(const void* data, std::size_t len);
  // 返回 32 字节摘要；调用后对象仍可继续 update（不重置）。
  std::array<std::uint8_t, 32> digest() const;
  // 64 位小写十六进制摘要。
  std::string hex() const;

  static std::string hex_of(const void* data, std::size_t len);
  static std::string hex_of_string(const std::string& s);

 private:
  void transform(const std::uint8_t block[64]);
  std::uint32_t state_[8];
  std::uint64_t bitlen_;
  std::uint8_t buffer_[64];
  std::size_t buffer_len_;
};

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_SHA256_H
