#pragma once
// SHA-256 and canonical content hashing for ACSD v2 objects.
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace acsd::model {

class Sha256 {
 public:
  Sha256();
  void update(std::span<const std::byte> bytes);
  void update(std::string_view text);
  void update_u64(std::uint64_t value);
  void update_i64(std::int64_t value);
  void update_f64(double value);  // bit pattern of the binary64 value
  void update_bool(bool value);
  // Appends a length-prefixed field so that concatenations cannot alias.
  void field(std::string_view key, std::string_view value);
  void field(std::string_view key, std::uint64_t value);
  void field_f64(std::string_view key, double value);
  std::array<std::uint8_t, 32> digest() const;
  std::string hex() const;

 private:
  void transform(const std::uint8_t* block);
  std::uint32_t state_[8];
  std::uint64_t bit_length_;
  std::uint8_t buffer_[64];
  std::size_t buffered_;
};

std::string sha256_hex(std::span<const std::byte> bytes);
std::string sha256_hex(std::string_view text);
// Content id form required by contracts.hpp: "sha256:<64 lowercase hex>".
std::string content_id(std::string_view hex_digest);
bool is_content_id(std::string_view id);

// Canonical little-endian encoding helpers used for block hashing.
void append_f64_le(std::vector<std::byte>& out, double value);
void append_u64_le(std::vector<std::byte>& out, std::uint64_t value);
void append_i64_le(std::vector<std::byte>& out, std::int64_t value);
void append_u32_le(std::vector<std::byte>& out, std::uint32_t value);

}  // namespace acsd::model
