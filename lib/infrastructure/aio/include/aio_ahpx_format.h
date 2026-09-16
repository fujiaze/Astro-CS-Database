#ifndef AIO_AHPX_FORMAT_H
#define AIO_AHPX_FORMAT_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace aio::ahpx {

// .ahpx 文件格式常量
constexpr char MAGIC[4] = {'A', 'H', 'P', 'X'};
constexpr uint16_t VERSION = 1;

// 文件头固定部分大小 (bytes)
// Magic(4) + Version(2) + HeaderSize(4) + HeaderCompSize(4) + BlockCount(4) = 18
constexpr size_t HEADER_FIXED_SIZE = 18;

// 权重模式
enum class WeightMode : uint8_t {
    SCALAR = 0,   // 整图统一权重 (float32 标量)
    GRID   = 1,   // 分块网格权重 (gw×gh float32)
    PIXEL  = 2    // 逐像素权重 (W×H float32)
};

// 压缩编码
enum class Codec : uint8_t {
    NONE = 0,
    ZSTD = 1,
    LZ4  = 2,
    JPEG = 3   // 仅用于缩略图(保留，当前不使用)
};

// 数据块索引
struct BlockIndex {
    char     id[32];        // 块标识 (如 "pixel", "snr", "weight")
    uint64_t offset;        // 文件内偏移
    uint64_t size;          // 压缩后大小
    uint8_t  codec;         // Codec 枚举值
    int      level;         // 压缩级别 (zstd 1-22, lz4 忽略)
};

// 权重模式信息
struct WeightInfo {
    WeightMode mode;
    uint16_t   grid_w;      // 仅 GRID 模式
    uint16_t   grid_h;      // 仅 GRID 模式
};

} // namespace aio::ahpx

#endif // AIO_AHPX_FORMAT_H
