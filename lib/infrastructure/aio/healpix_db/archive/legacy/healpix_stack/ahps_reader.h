#ifndef AHPS_READER_H
#define AHPS_READER_H

#include "ahps_format.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace ahps {

// ============================================================================
// AhpsReader - .ahps 稀疏堆栈文件读取器
//
// 用法:
//   AhpsReader reader;
//   if (!reader.open("tile_123.ahps")) return;
//   auto indices = reader.readPixelIndices();
//   auto stats   = reader.readBandStats(0);
//   reader.close();
// ============================================================================

class AhpsReader {
public:
    AhpsReader();
    ~AhpsReader();

    // 打开文件, 解析头. 成功返回 true
    bool open(const std::string& path);
    void close();

    // 头信息访问
    int              getNside() const;
    int              getTileNside() const;
    int64_t          getTileIpix() const;
    int64_t          getPixelCount() const;
    int              getBandCount() const;
    const std::string& getHeaderJson() const;

    // 读取像素索引数组 (PixelCount 个 uint64, 升序)
    std::vector<uint64_t> readPixelIndices();

    // 读取指定波段的统计量 (PixelCount 个 PixelStats)
    // bandIndex: 0..BandCount-1
    std::vector<PixelStats> readBandStats(int bandIndex);

    // 读取指定波段的最终值 (value = sum/weightSum) 与方差
    // variance = sumSq/weightSum - value² (加权方差)
    // weightSum==0 时 value=0, variance=0
    bool readBandValues(int bandIndex,
                        std::vector<float>& outValues,
                        std::vector<float>& outVariance);

private:
    std::string m_path;
    FILE*       m_fp;
    int         m_nside;
    int         m_tileNside;
    int64_t     m_tileIpix;
    int64_t     m_pixelCount;
    int         m_bandCount;
    uint32_t    m_headerSize;
    uint32_t    m_headerCompSize;
    std::string m_headerJson;

    // 像素索引块的 chunk 索引
    std::vector<ChunkIndex> m_pixelChunks;
    // 各波段统计块的 chunk 索引
    std::vector<std::vector<ChunkIndex>> m_bandChunks;

    bool parseHeader();
    bool readRaw(uint64_t offset, uint64_t size, void* dst);

    // 读取并解压一个 chunk, 返回原始字节
    std::vector<uint8_t> readChunk(const ChunkIndex& chunk);

    // UTF-8 路径文件打开
    FILE* openFile(const std::string& path, const char* mode);
};

} // namespace ahps

#endif // AHPS_READER_H
