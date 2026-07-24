#ifndef AIO_AHPX_WRITER_H
#define AIO_AHPX_WRITER_H

#include "../../include/aio_ahpx_format.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace aio::ahpx {

// ============================================================================
// AhpxWriter - .ahpx 单帧存储格式写入器
//
// 用法:
//   AhpxWriter writer;
//   writer.setMetadata(jsonStr);
//   writer.setPixels(pixelData, w, h, c);
//   writer.setSnr(snrData, w, h);
//   writer.setWeightScalar(1.0f);
//   writer.write("output.ahpx");
// ============================================================================

// 写入配置
struct AhpxWriteConfig {
    int        zstdLevel = 5;               // ZSTD 压缩级别 (1-22), 默认 5
    WeightMode weightMode = WeightMode::SCALAR; // 权重模式
    uint16_t   gridW = 0;                   // 仅 GRID 模式: 网格宽度
    uint16_t   gridH = 0;                   // 仅 GRID 模式: 网格高度
};

class AhpxWriter {
public:
    AhpxWriter();
    ~AhpxWriter();

    // 设置元数据 JSON (调用方构建完整 JSON 字符串)
    // 必须包含 image/wcs/observation/calibration 字段
    void setMetadata(const std::string& json);

    // 设置图像数据 (float32, HWC 或 CHW 排列由调用方约定, 这里只存原始字节)
    void setPixels(const float* data, int width, int height, int channels);

    // 设置 SNR 图 (W×H float32)
    void setSnr(const float* data, int width, int height);

    // 设置权重 - 标量模式 (整图统一权重)
    void setWeightScalar(float scalar);

    // 设置权重 - 网格模式 (gw×gh float32)
    void setWeightGrid(const float* grid, uint16_t gw, uint16_t gh);

    // 设置权重 - 逐像素模式 (W×H float32)
    void setWeightPixel(const float* data, int width, int height);

    // 写入文件
    // config.zstdLevel: JSON 头和数据块的压缩级别
    // 成功返回 true
    bool write(const std::string& path, const AhpxWriteConfig& config = {});

private:
    std::string         m_metadataJson;   // 元数据 JSON
    std::vector<float>  m_pixels;         // 像素数据
    int                 m_width;          // 图像宽度
    int                 m_height;         // 图像高度
    int                 m_channels;       // 通道数
    std::vector<float>  m_snr;            // SNR 图
    bool                m_hasSnr;         // 是否设置了 SNR
    WeightMode          m_weightMode;     // 权重模式
    std::vector<float>  m_weightData;     // 权重数据
    uint16_t            m_gridW;          // 网格宽度 (GRID 模式)
    uint16_t            m_gridH;          // 网格高度 (GRID 模式)

    // 压缩并写入一个数据块, 返回块索引
    // fp: 已打开的文件指针
    // id: 块标识
    // srcData: 原始数据指针
    // srcSize: 原始数据字节数
    // codec: 压缩编码 (0=NONE, 1=ZSTD, 2=LZ4)
    // level: 压缩级别
    // outOffset: 输出块在文件中的偏移
    // outCompSize: 输出压缩后大小
    bool compressAndWriteBlock(FILE* fp, const char* id,
                                const void* srcData, size_t srcSize,
                                uint8_t codec, int level,
                                uint64_t& outOffset, uint64_t& outCompSize);

    // 构建 blocks 索引 JSON 片段
    std::string buildBlocksJson(const std::vector<BlockIndex>& blocks);

    // 将 blocks 数组和 weight 信息注入到元数据 JSON 中
    std::string injectBlocksIntoJson(const std::vector<BlockIndex>& blocks,
                                     const AhpxWriteConfig& config);

    // UTF-8 路径文件打开
    FILE* openFile(const std::string& path, const char* mode);
};

} // namespace aio::ahpx

#endif // AIO_AHPX_WRITER_H
