#include "model/hash.hpp"

#include <cstdio>

namespace acsd::model {
namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

}  // namespace

Sha256::Sha256() : bit_length_(0), buffered_(0) {
  state_[0] = 0x6a09e667u; state_[1] = 0xbb67ae85u; state_[2] = 0x3c6ef372u;
  state_[3] = 0xa54ff53au; state_[4] = 0x510e527fu; state_[5] = 0x9b05688cu;
  state_[6] = 0x1f83d9abu; state_[7] = 0x5be0cd19u;
  std::memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::transform(const std::uint8_t* block) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (std::uint32_t(block[4 * i]) << 24) | (std::uint32_t(block[4 * i + 1]) << 16) |
           (std::uint32_t(block[4 * i + 2]) << 8) | std::uint32_t(block[4 * i + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i -  2], 17) ^ rotr(w[i -  2], 19) ^ (w[i -  2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
  std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
  for (int i = 0; i < 64; ++i) {
    const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ (~e & g);
    const std::uint32_t temp1 = h + S1 + ch + kK[i] + w[i];
    const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = S0 + maj;
    h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
  }
  state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
  state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(std::span<const std::byte> bytes) {
  const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
  std::size_t remaining = bytes.size();
  bit_length_ += static_cast<std::uint64_t>(remaining) * 8u;
  if (buffered_ > 0) {
    const std::size_t take = std::min(remaining, std::size_t{64} - buffered_);
    std::memcpy(buffer_ + buffered_, data, take);
    buffered_ += take; data += take; remaining -= take;
    if (buffered_ == 64) { transform(buffer_); buffered_ = 0; }
  }
  while (remaining >= 64) { transform(data); data += 64; remaining -= 64; }
  if (remaining > 0) { std::memcpy(buffer_ + buffered_, data, remaining); buffered_ += remaining; }
}

void Sha256::update(std::string_view text) {
  update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()));
}

void Sha256::update_u64(std::uint64_t value) {
  std::uint8_t bytes[8];
  for (int i = 0; i < 8; ++i) bytes[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
  update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes), 8));
}

void Sha256::update_i64(std::int64_t value) {
  update_u64(static_cast<std::uint64_t>(value));
}

void Sha256::update_f64(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  update_u64(bits);
}

void Sha256::update_bool(bool value) { update_u64(value ? 1u : 0u); }

void Sha256::field(std::string_view key, std::string_view value) {
  update_u64(key.size());
  update(key);
  update_u64(value.size());
  update(value);
}

void Sha256::field(std::string_view key, std::uint64_t value) {
  update_u64(key.size());
  update(key);
  update_u64(value);
}

void Sha256::field_f64(std::string_view key, double value) {
  update_u64(key.size());
  update(key);
  update_f64(value);
}

std::array<std::uint8_t, 32> Sha256::digest() const {
  Sha256 copy(*this);
  std::uint64_t bits = copy.bit_length_;
  const std::uint8_t pad = 0x80;
  copy.update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&pad), 1));
  // update() adjusted bit_length_; restore the original message length for the tail.
  copy.bit_length_ = bits;
  while (copy.buffered_ != 56) {
    const std::uint8_t zero = 0;
    copy.update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&zero), 1));
    copy.bit_length_ = bits;
  }
  std::uint8_t tail[8];
  for (int i = 0; i < 8; ++i) tail[i] = static_cast<std::uint8_t>((bits >> (56 - 8 * i)) & 0xffu);
  copy.update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(tail), 8));
  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[4 * i + 0] = static_cast<std::uint8_t>((copy.state_[i] >> 24) & 0xffu);
    out[4 * i + 1] = static_cast<std::uint8_t>((copy.state_[i] >> 16) & 0xffu);
    out[4 * i + 2] = static_cast<std::uint8_t>((copy.state_[i] >> 8) & 0xffu);
    out[4 * i + 3] = static_cast<std::uint8_t>(copy.state_[i] & 0xffu);
  }
  return out;
}

std::string Sha256::hex() const {
  const auto d = digest();
  static const char* kHex = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (std::uint8_t byte : d) { out.push_back(kHex[byte >> 4]); out.push_back(kHex[byte & 0x0f]); }
  return out;
}

std::string sha256_hex(std::span<const std::byte> bytes) {
  Sha256 h; h.update(bytes); return h.hex();
}

std::string sha256_hex(std::string_view text) {
  Sha256 h; h.update(text); return h.hex();
}

std::string content_id(std::string_view hex_digest) { return "sha256:" + std::string(hex_digest); }

bool is_content_id(std::string_view id) {
  if (id.size() != 71) return false;
  if (id.compare(0, 7, "sha256:") != 0) return false;
  for (std::size_t i = 7; i < id.size(); ++i) {
    const char c = id[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

void append_f64_le(std::vector<std::byte>& out, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::byte>((bits >> (8 * i)) & 0xffu));
}

void append_u64_le(std::vector<std::byte>& out, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xffu));
}

void append_i64_le(std::vector<std::byte>& out, std::int64_t value) {
  append_u64_le(out, static_cast<std::uint64_t>(value));
}

void append_u32_le(std::vector<std::byte>& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xffu));
}

}  // namespace acsd::model
