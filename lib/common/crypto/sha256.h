// lib/common/crypto/sha256.h — 共享自包含 SHA-256（FIPS 180-4，公开算法）
// 由 astro_image_io（AIO UPM 容器）与 phase2（UPM 模型哈希）共用，单一实现。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace astrocs::crypto {

// 计算输入字节的 SHA-256 摘要并输出 hex 字符串（64 字符）。
std::string sha256_hex(const void* data, std::size_t len);

// 计算文件字节的 SHA-256 摘要并输出 hex 字符串（分块读取，不整体载入）。
std::string sha256_file(const char* path);

// 增量 SHA-256（大文件/流式 checksum；分块 update，禁止整体读入内存）。
class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    // 结束并输出 hex 摘要；final 后不可再 update。
    std::string final_hex();

private:
    std::uint32_t h_[8];
    std::uint64_t total_bits_ = 0;
    unsigned char block_[64];
    std::size_t block_len_ = 0;
    bool finalized_ = false;
    void process_block(const unsigned char* p);
};

} // namespace astrocs::crypto
