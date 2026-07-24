#ifndef AHPS_WRITER_H
#define AHPS_WRITER_H

#include "ahps_format.h"

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace ahps {

// ============================================================================
// AhpsWriter - .ahps 稀疏堆栈文件写入器
//
// 用法:
//   AhpsWriter writer;
//   writer.setNside(32768);
//   writer.setTileNside(512);
//   writer.setTileIpix(123);
//   writer.setBandCount(6);
//   writer.setHeaderJson("{...}");
//   writer.setPixelIndices(indices);           // 必须升序
//   writer.setBandStats(0, statsBand0);
//   writer.setBandStats(1, statsBand1);
//   writer.write("tile_123.ahps", 5);
// ============================================================================

class AhpsWriter {
public:
    AhpsWriter();
    ~AhpsWriter();

    // 设置头信息
    void setNside(int nside);
    void setTileNside(int tileNside);
    void setTileIpix(int64_t tileIpix);
    void setBandCount(int bandCount);
    void setHeaderJson(const std::string& json);

    // 设置像素索引 (必须升序排序)
    void setPixelIndices(const std::vector<uint64_t>& indices);

    // 设置指定波段的统计量
    // bandIndex: 0..BandCount-1
    void setBandStats(int bandIndex, const std::vector<PixelStats>& stats);

    // 写入文件
    // zstdLevel: 1-22, 推荐 5
    bool write(const std::string& path, int zstdLevel = 5);

private:
    int         m_nside;
    int         m_tileNside;
    int64_t     m_tileIpix;
    int         m_bandCount;
    std::string m_headerJson;
    std::vector<uint64_t> m_pixelIndices;
    std::map<int, std::vector<PixelStats>> m_bandStats;

    // 将一段原始数据分块压缩写入文件, 返回各 chunk 索引
    // fp: 已打开的文件指针
    // src: 原始数据
    // elemSize: 单元素字节数
    // totalElems: 元素总数
    // zstdLevel: 压缩级别
    // outChunks: 输出 chunk 索引列表
    bool writeChunked(FILE* fp, const void* src, size_t elemSize,
                      size_t totalElems, int zstdLevel,
                      std::vector<ChunkIndex>& outChunks);

    // 构建 JSON 头 (含 bands 列表与 chunk 索引)
    std::string buildHeaderJson(const std::vector<ChunkIndex>& pixelChunks,
                                const std::vector<std::vector<ChunkIndex>>& bandChunks,
                                int zstdLevel);

    // UTF-8 路径文件打开
    FILE* openFile(const std::string& path, const char* mode);
};

} // namespace ahps

#endif // AHPS_WRITER_H
