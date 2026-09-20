#ifndef AIO_AHPX_READER_H
#define AIO_AHPX_READER_H

#include "../../include/aio_ahpx_format.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace aio::ahpx {

// ============================================================================
// AhpxReader - .ahpx 单帧存储格式读取器
//
// 数据面见 aio_ahpx_format.h (变更 AHPX-WEIGHT-RETIRE-20260920): 只读
// "pixel" 与 "snr" 两类块, 本格式不承载任何权重。
//
// 旧格式显式拒绝: 头 JSON 携带已作废的 "weight" 字段 (权重模式载体) 或存在
// 同名数据块时, open() 返回 false 且 getRejectReason() 给出原因 — 禁静默忽略。
//
// 用法:
// AhpxReader reader;
// if (!reader.open("frame.ahpx")) return;
// printf("header: %s\n", reader.getHeaderJson.c_str);
// auto pixels = reader.readPixels;
// reader.close;
// ============================================================================

class AhpxReader {
public:
    AhpxReader();
    ~AhpxReader();

    // 打开文件, 解析头. 成功返回 true
    // 旧格式 (含已作废 weight 字段/块) ⇒ 返回 false, 原因见 getRejectReason()
    bool open(const std::string& path);

    // 获取 JSON 头 (已解压). open 成功后有效
    const std::string& getHeaderJson() const;

    // 获取拒绝原因. open 失败时非空 (含旧格式显式拒绝的判据)
    const std::string& getRejectReason() const;

    // 获取数据块索引列表
    const std::vector<BlockIndex>& getBlocks() const;

    // 按 id 查找块. 未找到返回 nullptr
    const BlockIndex* findBlock(const std::string& id) const;

    // 读取指定块数据 (解压后)
    // 返回: 解压后数据 (vector<uint8_t>), 失败返回空 vector
    std::vector<uint8_t> readBlock(const std::string& id);

    // 读取像素数据为 float32 数组
    // width/height/channels 从 JSON 头的 image 字段解析
    // 失败返回空 vector
    std::vector<float> readPixels();

    // 读取 SNR 图 (W×H float32)
    // 失败返回空 vector
    std::vector<float> readSnr();

    // 关闭文件, 释放资源
    void close();

    // 获取图像几何信息 (从 JSON 头解析)
    // 成功返回 true
    bool getImageInfo(int* width, int* height, int* channels) const;

private:
    std::string             m_path;        // 文件路径
    FILE*                   m_fp;          // 文件指针 (二进制模式)
    uint16_t                m_version;     // 格式版本
    uint32_t                m_headerSize;  // JSON 原始长度
    uint32_t                m_headerCompSize; // JSON 压缩后长度
    uint32_t                m_blockCount;  // 块数量
    std::string             m_headerJson;  // 解压后的 JSON 头
    std::string             m_rejectReason; // 拒绝原因 (open 失败时非空)
    std::vector<BlockIndex> m_blocks;      // 块索引列表

    // 解析 JSON 头, 填充 m_blocks; 旧格式 ⇒ 置 m_rejectReason 并返回 false
    bool parseHeader();

    // 从 JSON 头解析图像几何信息
    bool parseImageInfo(int* width, int* height, int* channels) const;

    // 从文件指定偏移读取指定长度的原始字节
    bool readRaw(uint64_t offset, uint64_t size, void* dst);

    // 读取并解压一个块 (内部使用 BlockIndex 指针)
    std::vector<uint8_t> readBlockByIndex(const BlockIndex* blk);
};

} // namespace aio::ahpx

#endif // AIO_AHPX_READER_H
