#ifndef AHPX_COMPRESSOR_H
#define AHPX_COMPRESSOR_H

#include <cstdint>
#include <cstddef>

namespace ahpx {

// ============================================================================
// 压缩层接口
// 封装 zstd 和 lz4 两种压缩算法
// 无库可用时 fallback 到不压缩 (返回 0 表示未压缩, 调用方需处理)
// ============================================================================

// -------- Zstd --------
// 压缩: 返回压缩后字节数, 失败返回 0
// level: 1-22, 默认 5, 越高压缩率越好但越慢
size_t compressZstd(const void* src, size_t srcSize,
                    void* dst, size_t dstCapacity, int level);

// 解压: 返回解压后字节数, 失败返回 0
size_t decompressZstd(const void* src, size_t srcSize,
                      void* dst, size_t dstCapacity);

// 获取 zstd 压缩后最大可能大小 (用于分配输出缓冲区)
size_t compressBoundZstd(size_t srcSize);

// -------- LZ4 --------
// 压缩: 返回压缩后字节数, 失败返回 0
size_t compressLz4(const void* src, size_t srcSize,
                   void* dst, size_t dstCapacity);

// 解压: 返回解压后字节数, 失败返回 0
// 注意: LZ4 解压需要知道原始大小, 由调用方提供 dstCapacity
size_t decompressLz4(const void* src, size_t srcSize,
                     void* dst, size_t dstCapacity);

// 获取 lz4 压缩后最大可能大小
size_t compressBoundLz4(size_t srcSize);

// -------- 编译特性查询 --------
// 返回是否编译时支持 zstd
bool hasZstdSupport();
// 返回是否编译时支持 lz4
bool hasLz4Support();

} // namespace ahpx

#endif // AHPX_COMPRESSOR_H
