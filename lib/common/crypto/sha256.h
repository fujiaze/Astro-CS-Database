// lib/common/crypto/sha256.h — 共享自包含 SHA-256（FIPS 180-4，公开算法）
// 由 astro_image_io（AIO UPM 容器）与 phase2（UPM 模型哈希）共用，单一实现。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace astrocs::crypto {

// 计算输入字节的 SHA-256 摘要并输出 hex 字符串（64 字符）。
std::string sha256_hex(const void* data, std::size_t len);

} // namespace astrocs::p2
