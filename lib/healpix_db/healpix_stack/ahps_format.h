#ifndef AHPS_FORMAT_H
#define AHPS_FORMAT_H

// ============================================================================
// .ahps 稀疏堆栈文件格式常量
//
// 文件布局:
//   [固定头 38B][压缩 JSON 头][像素索引块][波段0统计块][波段1统计块]...
//
// 固定头 (38 字节, 小端序):
//   Magic(4) + Version(2) + Nside(4) + TileNside(4) + TileIpix(8) +
//   PixelCount(4) + BandCount(4) + HeaderSize(4) + HeaderCompSize(4)
//
// 像素索引块:
//   PixelCount 个 uint64 (HEALpix 像素号, 升序)
//   分块压缩: 每 CHUNK_SIZE 个像素一个独立压缩块
//
// 波段统计块 (每波段一个):
//   PixelCount 个 PixelStats (count/sum/sumSq/weightSum)
//   分块压缩: 每 CHUNK_SIZE 个像素一个独立压缩块
//
// JSON 头包含: bands 名称列表, 各数据块的 chunk 索引 (offset/size/codec)
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace ahps {

// 文件 Magic 与版本
constexpr char MAGIC[4] = {'A', 'H', 'P', 'S'};
constexpr uint16_t VERSION = 1;

// 文件头固定部分大小 (bytes)
// Magic(4) + Version(2) + Nside(4) + TileNside(4) + TileIpix(8) +
// PixelCount(4) + BandCount(4) + HeaderSize(4) + HeaderCompSize(4) = 38
constexpr size_t HEADER_FIXED_SIZE = 38;

// 分块压缩: 每 CHUNK_SIZE 个像素一个独立压缩块
// 每个压缩块独立压缩, 便于随机访问与并行解压
constexpr uint32_t CHUNK_SIZE = 4096;

// 压缩编码
enum class Codec : uint8_t {
    NONE = 0,   // 不压缩
    ZSTD = 1,   // Zstd (默认, 高压缩率)
    LZ4  = 2    // LZ4 (快速)
};

// 单波段统计量结构 (每像素 14 字节)
// 用于 sigma-clip 加权堆栈的累积量
struct PixelStats {
    uint16_t count;      // 帧计数 (N, 通过 sigma-clip 后保留的帧数)
    float    sum;        // 加权和 (Σ value * weight)
    float    sumSq;      // 加权平方和 (Σ value² * weight)
    float    weightSum;  // 权重累加 (Σ weight)
};

// 数据块索引 (记录一个压缩块在文件中的位置)
struct ChunkIndex {
    uint64_t offset;     // 文件内偏移
    uint64_t size;       // 压缩后大小
    uint32_t rawCount;   // 原始元素数 (像素数, 末块可能 < CHUNK_SIZE)
    uint8_t  codec;      // Codec 枚举值
};

} // namespace ahps

#endif // AHPS_FORMAT_H
