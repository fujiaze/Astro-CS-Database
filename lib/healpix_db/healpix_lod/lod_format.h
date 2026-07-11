#ifndef LOD_FORMAT_H
#define LOD_FORMAT_H

// ============================================================================
// LOD (Level Of Detail) 金字塔格式常量定义
//
// LOD 金字塔维护多级降采样数据, 支持浏览器按屏幕分辨率浏览:
//   Level 0: nside=512   (lz4, 快速加载, 球面全景)
//   Level 1: nside=2048  (lz4, 导航层)
//   Level 2: nside=8192  (zstd level 3, 中精度)
//   Level 3: nside=32768 (zstd level 5, 高精度 = 数据层)
//
// LOD tile 文件格式 (.ahpl):
//   [固定头 34B][压缩数据块]
//
// 固定头 (34 字节, 小端序):
//   Magic(4) + Version(2) + Nside(4) + TileNside(4) + TileIpix(8) +
//   BandIndex(4) + PixelCount(4) + Compression(1) + Reserved(3)
//
// 压缩数据块 (整个块统一压缩):
//   像素索引: PixelCount * uint64 (8B)
//   数值:     PixelCount * float  (4B)
//   权重:     PixelCount * float  (4B)
//   计数:     PixelCount * uint16 (2B)
//   总原始大小 = PixelCount * 18
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace lod {

// --------------------------------------------------------------------------
// LOD 层级定义
// --------------------------------------------------------------------------
struct LodLevel {
    int nside;          // 该层 nside (必须为 2 的幂)
    int compression;    // 压缩方式: 0=lz4, 1=zstd3, 2=zstd5
};

// 默认压缩级别常量
constexpr int COMP_LZ4    = 0;  // LZ4 快速压缩 (低层, 快速加载)
constexpr int COMP_ZSTD_3 = 1;  // ZSTD level 3 (中层, 中等压缩率)
constexpr int COMP_ZSTD_5 = 2;  // ZSTD level 5 (高层, 高压缩率)

// --------------------------------------------------------------------------
// .ahpl LOD tile 文件格式常量
// --------------------------------------------------------------------------
constexpr char     MAGIC[4]         = {'A', 'H', 'P', 'L'};
constexpr uint16_t VERSION          = 1;

// 固定头大小 (bytes):
// Magic(4) + Version(2) + Nside(4) + TileNside(4) + TileIpix(8) +
// BandIndex(4) + PixelCount(4) + Compression(1) + Reserved(3) = 34
constexpr size_t   HEADER_FIXED_SIZE = 34;

// 压缩编码 (存储在 Compression 字段)
constexpr uint8_t  CODEC_NONE = 0;  // 不压缩
constexpr uint8_t  CODEC_ZSTD = 1;  // ZSTD 压缩
constexpr uint8_t  CODEC_LZ4  = 2;  // LZ4 压缩

// 单元素数据大小: uint64(ipix) + float(value) + float(weight) + uint16(count)
constexpr size_t   PIXEL_RECORD_SIZE = 8 + 4 + 4 + 2;  // = 18 字节/像素

} // namespace lod

#endif // LOD_FORMAT_H
