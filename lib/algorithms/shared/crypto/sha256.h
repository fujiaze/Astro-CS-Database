// lib/algorithms/shared/crypto/sha256.h — 共享自包含 SHA-256（FIPS 180-4，公开算法）
// 由 astro_image_io（AIO UPM 容器）与 phase2（UPM 模型哈希）共用，单一实现。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace astrocs::crypto {

// 计算输入字节的 SHA-256 摘要并输出 hex 字符串（64 字符）。
std::string sha256_hex(const void* data, std::size_t len);

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 文件级摘要
// 不属本模块（纯算法, 不持文件通道）; 唯一实现 = aio_file::sha256_hex
// (lib/infrastructure/aio/src/aio_file_io.h)。原 sha256_file 全仓零调用者,
// 已随 CLEAN-403 删除。

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
