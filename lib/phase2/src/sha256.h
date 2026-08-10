// lib/phase2/src/sha256.h — 自包含 SHA-256（FIPS 180-4，公开算法）
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace astrocs::p2 {

// 计算输入字节的 SHA-256 摘要并输出 hex 字符串（64 字符）。
std::string sha256_hex(const void* data, std::size_t len);

} // namespace astrocs::p2
