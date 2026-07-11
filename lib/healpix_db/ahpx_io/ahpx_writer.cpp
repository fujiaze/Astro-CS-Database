#include "ahpx_writer.h"
#include "compressor.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ahpx {

// ============================================================================
// UTF-8 路径文件打开辅助 (Windows 下支持中文路径)
// ============================================================================
static FILE* ahpx_fopen_utf8(const char* path, const char* mode) {
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path, mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);

    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path, mode);
#endif
}

// ============================================================================
// 内部辅助: 压缩后的数据块
// ============================================================================
struct CompBlock {
    char                 id[32];       // 块标识
    std::vector<uint8_t> data;         // 压缩后数据 (或未压缩数据)
    uint64_t             offset = 0;   // 文件内偏移 (后续计算填充)
    uint64_t             compSize = 0; // 压缩后大小
    uint8_t              codec = 0;    // 实际使用的编码
    int                  level = 0;    // 压缩级别
};

// ============================================================================
// AhpxWriter 实现
// ============================================================================

AhpxWriter::AhpxWriter()
    : m_width(0)
    , m_height(0)
    , m_channels(0)
    , m_hasSnr(false)
    , m_weightMode(WeightMode::SCALAR)
    , m_gridW(0)
    , m_gridH(0) {
}

AhpxWriter::~AhpxWriter() {
}

void AhpxWriter::setMetadata(const std::string& json) {
    m_metadataJson = json;
}

void AhpxWriter::setPixels(const float* data, int width, int height, int channels) {
    if (!data || width <= 0 || height <= 0 || channels <= 0) {
        fprintf(stderr, "[ahpx][writer] setPixels: 无效参数 (data=%p w=%d h=%d c=%d)\n",
                data, width, height, channels);
        return;
    }
    m_width = width;
    m_height = height;
    m_channels = channels;
    size_t count = (size_t)width * height * channels;
    m_pixels.assign(data, data + count);
}

void AhpxWriter::setSnr(const float* data, int width, int height) {
    if (!data || width <= 0 || height <= 0) {
        fprintf(stderr, "[ahpx][writer] setSnr: 无效参数 (data=%p w=%d h=%d)\n",
                data, width, height);
        return;
    }
    size_t count = (size_t)width * height;
    m_snr.assign(data, data + count);
    m_hasSnr = true;
}

void AhpxWriter::setWeightScalar(float scalar) {
    m_weightMode = WeightMode::SCALAR;
    m_weightData.clear();
    m_weightData.push_back(scalar);
    m_gridW = 0;
    m_gridH = 0;
}

void AhpxWriter::setWeightGrid(const float* grid, uint16_t gw, uint16_t gh) {
    if (!grid || gw == 0 || gh == 0) {
        fprintf(stderr, "[ahpx][writer] setWeightGrid: 无效参数 (grid=%p gw=%u gh=%u)\n",
                grid, gw, gh);
        return;
    }
    m_weightMode = WeightMode::GRID;
    size_t count = (size_t)gw * gh;
    m_weightData.assign(grid, grid + count);
    m_gridW = gw;
    m_gridH = gh;
}

void AhpxWriter::setWeightPixel(const float* data, int width, int height) {
    if (!data || width <= 0 || height <= 0) {
        fprintf(stderr, "[ahpx][writer] setWeightPixel: 无效参数 (data=%p w=%d h=%d)\n",
                data, width, height);
        return;
    }
    m_weightMode = WeightMode::PIXEL;
    size_t count = (size_t)width * height;
    m_weightData.assign(data, data + count);
    m_gridW = 0;
    m_gridH = 0;
}

FILE* AhpxWriter::openFile(const std::string& path, const char* mode) {
    return ahpx_fopen_utf8(path.c_str(), mode);
}

bool AhpxWriter::compressAndWriteBlock(FILE* fp, const char* id,
                                        const void* srcData, size_t srcSize,
                                        uint8_t codec, int level,
                                        uint64_t& outOffset, uint64_t& outCompSize) {
    outOffset = 0;
    outCompSize = 0;

    if (!fp || !srcData || srcSize == 0) {
        fprintf(stderr, "[ahpx][writer] compressAndWriteBlock: 无效参数\n");
        return false;
    }

    std::vector<uint8_t> compBuf;
    size_t compSize = 0;
    uint8_t usedCodec = codec;

    if (codec == (uint8_t)Codec::ZSTD) {
        // ZSTD 压缩
        size_t bound = compressBoundZstd(srcSize);
        compBuf.resize(bound);
        compSize = compressZstd(srcData, srcSize, compBuf.data(), bound, level);
        if (compSize == 0) {
            fprintf(stderr, "[ahpx][writer] 块 '%s' ZSTD 压缩失败, 回退到不压缩\n", id);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        } else if (compSize >= srcSize) {
            // 压缩后比原始还大, 回退到不压缩
            fprintf(stderr, "[ahpx][writer] 块 '%s' 压缩后更大 (%zu > %zu), 回退到不压缩\n",
                    id, compSize, srcSize);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        }
    } else if (codec == (uint8_t)Codec::LZ4) {
        // LZ4 压缩
        size_t bound = compressBoundLz4(srcSize);
        compBuf.resize(bound);
        compSize = compressLz4(srcData, srcSize, compBuf.data(), bound);
        if (compSize == 0) {
            fprintf(stderr, "[ahpx][writer] 块 '%s' LZ4 压缩失败, 回退到不压缩\n", id);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        } else if (compSize >= srcSize) {
            fprintf(stderr, "[ahpx][writer] 块 '%s' LZ4 压缩后更大 (%zu > %zu), 回退到不压缩\n",
                    id, compSize, srcSize);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        }
    } else {
        // 不压缩
        usedCodec = (uint8_t)Codec::NONE;
        compSize = srcSize;
        compBuf.resize(srcSize);
        std::memcpy(compBuf.data(), srcData, srcSize);
    }

    // 获取当前文件位置作为块偏移
    long curPos = std::ftell(fp);
    if (curPos < 0) {
        fprintf(stderr, "[ahpx][writer] ftell 失败\n");
        return false;
    }
    outOffset = (uint64_t)curPos;
    outCompSize = compSize;

    // 写入压缩数据
    if (std::fwrite(compBuf.data(), 1, compSize, fp) != compSize) {
        fprintf(stderr, "[ahpx][writer] 块 '%s' 写入失败\n", id);
        return false;
    }

    fprintf(stderr, "[ahpx][writer] 块 '%s': offset=%llu compSize=%llu codec=%u (原始=%zu)\n",
            id, (unsigned long long)outOffset, (unsigned long long)outCompSize,
            usedCodec, srcSize);

    // 注意: 返回的 outCompSize 和 outOffset 正确, 但 codec 需要调用方记录
    // 这里通过修改 compBlock 在 write() 中处理
    (void)usedCodec; // 在 write() 中单独处理
    return true;
}

std::string AhpxWriter::buildBlocksJson(const std::vector<BlockIndex>& blocks) {
    std::string json = "[";
    for (size_t i = 0; i < blocks.size(); i++) {
        const BlockIndex& blk = blocks[i];
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "{\"id\":\"%s\",\"offset\":%llu,\"size\":%llu,\"codec\":%u,\"level\":%d}",
            blk.id,
            (unsigned long long)blk.offset,
            (unsigned long long)blk.size,
            blk.codec,
            blk.level);
        if (i > 0) json += ",";
        json += buf;
    }
    json += "]";
    return json;
}

std::string AhpxWriter::injectBlocksIntoJson(const std::vector<BlockIndex>& blocks,
                                              const AhpxWriteConfig& config) {
    // 构建 blocks JSON 片段
    std::string blocksJson = buildBlocksJson(blocks);

    // 构建 weight JSON 片段
    char weightJson[128];
    std::snprintf(weightJson, sizeof(weightJson),
        "\"weight\":{\"mode\":%u,\"grid_w\":%u,\"grid_h\":%u}",
        (uint8_t)config.weightMode, config.gridW, config.gridH);

    // 如果元数据 JSON 为空, 创建一个基础结构
    if (m_metadataJson.empty()) {
        std::string result = "{";
        result += "\"image\":{\"width\":" + std::to_string(m_width);
        result += ",\"height\":" + std::to_string(m_height);
        result += ",\"channels\":" + std::to_string(m_channels) + "}";
        result += ",";
        result += weightJson;
        result += ",";
        result += "\"blocks\":" + blocksJson;
        result += "}";
        return result;
    }

    // 已有 JSON: 移除末尾的 '}', 追加 blocks 和 weight 字段
    std::string result = m_metadataJson;

    // 移除末尾空白和 '}'
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t' ||
           result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '}') {
        result.pop_back();
    }

    // 检查是否已有 blocks 字段 (简单查找)
    bool hasBlocks = (result.find("\"blocks\"") != std::string::npos);
    bool hasWeight = (result.find("\"weight\"") != std::string::npos);

    // 追加 weight 字段 (如果没有)
    if (!hasWeight) {
        // 检查是否需要逗号
        while (!result.empty() && (result.back() == ' ' || result.back() == '\t' ||
               result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        if (!result.empty() && result.back() != '{') {
            result += ",";
        }
        result += weightJson;
    }

    // 追加 blocks 字段 (如果没有)
    if (!hasBlocks) {
        result += ",\"blocks\":" + blocksJson;
    }

    result += "}";
    return result;
}

bool AhpxWriter::write(const std::string& path, const AhpxWriteConfig& config) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // 使用内部记录的 weightMode/gridW/gridH, 覆盖 config 中的值
    // (实际模式由 setWeight* 调用决定, config 中的 weightMode 字段被忽略)
    AhpxWriteConfig effectiveConfig = config;
    effectiveConfig.weightMode = m_weightMode;
    effectiveConfig.gridW = m_gridW;
    effectiveConfig.gridH = m_gridH;

    // -------- 1. 验证输入 --------
    if (m_pixels.empty() || m_width <= 0 || m_height <= 0 || m_channels <= 0) {
        fprintf(stderr, "[ahpx][writer] 未设置像素数据或几何无效\n");
        return false;
    }
    if (m_weightData.empty()) {
        fprintf(stderr, "[ahpx][writer] 未设置权重数据\n");
        return false;
    }

    fprintf(stderr, "[ahpx][writer] 开始写入: %s (w=%d h=%d c=%d pixels=%zu snr=%d weight=%zu)\n",
            path.c_str(), m_width, m_height, m_channels, m_pixels.size(),
            m_hasSnr ? 1 : 0, m_weightData.size());

    // -------- 2. 压缩所有数据块 (在内存中) --------
    std::vector<CompBlock> compBlocks;

    // 2.1 压缩 pixel 块
    {
        CompBlock cb;
        std::memset(cb.id, 0, sizeof(cb.id));
        std::strncpy(cb.id, "pixel", sizeof(cb.id) - 1);
        cb.codec = (uint8_t)Codec::ZSTD;
        cb.level = effectiveConfig.zstdLevel;

        size_t srcBytes = m_pixels.size() * sizeof(float);
        size_t bound = compressBoundZstd(srcBytes);
        cb.data.resize(bound);
        size_t compSize = compressZstd(m_pixels.data(), srcBytes,
                                       cb.data.data(), bound, effectiveConfig.zstdLevel);
        if (compSize == 0 || compSize >= srcBytes) {
            // 回退到不压缩
            cb.codec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            cb.data.resize(srcBytes);
            std::memcpy(cb.data.data(), m_pixels.data(), srcBytes);
        }
        cb.compSize = compSize;
        compBlocks.push_back(std::move(cb));
        fprintf(stderr, "[ahpx][writer] pixel 块压缩完成: %zu -> %zu bytes (codec=%u)\n",
                srcBytes, compSize, compBlocks.back().codec);
    }

    // 2.2 压缩 snr 块 (如果有)
    if (m_hasSnr && !m_snr.empty()) {
        CompBlock cb;
        std::memset(cb.id, 0, sizeof(cb.id));
        std::strncpy(cb.id, "snr", sizeof(cb.id) - 1);
        cb.codec = (uint8_t)Codec::ZSTD;
        cb.level = effectiveConfig.zstdLevel;

        size_t srcBytes = m_snr.size() * sizeof(float);
        size_t bound = compressBoundZstd(srcBytes);
        cb.data.resize(bound);
        size_t compSize = compressZstd(m_snr.data(), srcBytes,
                                       cb.data.data(), bound, effectiveConfig.zstdLevel);
        if (compSize == 0 || compSize >= srcBytes) {
            cb.codec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            cb.data.resize(srcBytes);
            std::memcpy(cb.data.data(), m_snr.data(), srcBytes);
        }
        cb.compSize = compSize;
        compBlocks.push_back(std::move(cb));
        fprintf(stderr, "[ahpx][writer] snr 块压缩完成: %zu -> %zu bytes (codec=%u)\n",
                srcBytes, compSize, compBlocks.back().codec);
    }

    // 2.3 压缩 weight 块
    {
        CompBlock cb;
        std::memset(cb.id, 0, sizeof(cb.id));
        std::strncpy(cb.id, "weight", sizeof(cb.id) - 1);
        cb.codec = (uint8_t)Codec::ZSTD;
        cb.level = effectiveConfig.zstdLevel;

        size_t srcBytes = m_weightData.size() * sizeof(float);
        size_t bound = compressBoundZstd(srcBytes);
        cb.data.resize(bound);
        size_t compSize = compressZstd(m_weightData.data(), srcBytes,
                                       cb.data.data(), bound, effectiveConfig.zstdLevel);
        if (compSize == 0 || compSize >= srcBytes) {
            cb.codec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            cb.data.resize(srcBytes);
            std::memcpy(cb.data.data(), m_weightData.data(), srcBytes);
        }
        cb.compSize = compSize;
        compBlocks.push_back(std::move(cb));
        fprintf(stderr, "[ahpx][writer] weight 块压缩完成: %zu -> %zu bytes (codec=%u)\n",
                srcBytes, compSize, compBlocks.back().codec);
    }

    // -------- 3. 迭代计算偏移和压缩 JSON 头 --------
    // 由于 blocks 的 offset 依赖 headerCompSize, 而 headerCompSize 依赖 JSON 内容
    // 需要迭代直到收敛 (通常 1-2 次即可)
    std::string finalJson;
    std::vector<uint8_t> compJson;
    uint32_t headerCompSize = 0;
    uint32_t headerSize = 0;

    for (int iter = 0; iter < 5; iter++) {
        // 构建 BlockIndex 列表 (使用当前估算的偏移)
        std::vector<BlockIndex> blkIdx;
        blkIdx.reserve(compBlocks.size());
        uint64_t curOffset = HEADER_FIXED_SIZE + headerCompSize;
        for (const auto& cb : compBlocks) {
            BlockIndex bi;
            std::memset(&bi, 0, sizeof(bi));
            std::strncpy(bi.id, cb.id, sizeof(bi.id) - 1);
            bi.offset = curOffset;
            bi.size = cb.compSize;
            bi.codec = cb.codec;
            bi.level = cb.level;
            blkIdx.push_back(bi);
            curOffset += cb.compSize;
        }

        // 注入 blocks 到 JSON
        finalJson = injectBlocksIntoJson(blkIdx, effectiveConfig);
        headerSize = (uint32_t)finalJson.size();

        // 压缩 JSON 头
        if (effectiveConfig.zstdLevel > 0) {
            size_t bound = compressBoundZstd(headerSize);
            compJson.resize(bound);
            size_t compSize = compressZstd(finalJson.data(), headerSize,
                                           compJson.data(), bound, effectiveConfig.zstdLevel);
            if (compSize == 0 || compSize >= headerSize) {
                // 回退到不压缩
                compJson.assign(finalJson.begin(), finalJson.end());
                compSize = headerSize;
                fprintf(stderr, "[ahpx][writer] JSON 头压缩失败/更大, 回退到不压缩\n");
            }
            compJson.resize(compSize);
            uint32_t newHeaderCompSize = (uint32_t)compSize;

            // 检查是否收敛
            if (newHeaderCompSize == headerCompSize) {
                headerCompSize = newHeaderCompSize;
                break;
            }
            headerCompSize = newHeaderCompSize;
        } else {
            // 不压缩 JSON
            compJson.assign(finalJson.begin(), finalJson.end());
            headerCompSize = headerSize;
            break;
        }

        fprintf(stderr, "[ahpx][writer] 迭代 %d: headerCompSize=%u headerSize=%u\n",
                iter, headerCompSize, headerSize);
    }

    // 最终重新计算偏移, JSON 头不压缩以避免压缩大小振荡
    // (headerSize 依赖 offset 字符串长度, 数字位数不变即收敛, 通常 1-2 次)
    for (int iter = 0; iter < 10; iter++) {
        std::vector<BlockIndex> blkIdx;
        blkIdx.reserve(compBlocks.size());
        uint64_t curOffset = HEADER_FIXED_SIZE + headerSize;
        for (const auto& cb : compBlocks) {
            BlockIndex bi;
            std::memset(&bi, 0, sizeof(bi));
            std::strncpy(bi.id, cb.id, sizeof(bi.id) - 1);
            bi.offset = curOffset;
            bi.size = cb.compSize;
            bi.codec = cb.codec;
            bi.level = cb.level;
            blkIdx.push_back(bi);
            curOffset += cb.compSize;
        }

        finalJson = injectBlocksIntoJson(blkIdx, effectiveConfig);
        uint32_t newHeaderSize = (uint32_t)finalJson.size();
        if (newHeaderSize == headerSize) {
            break;  // 收敛
        }
        headerSize = newHeaderSize;
    }

    // JSON 头不压缩 (headerCompSize=headerSize 时 writer 写入 hdrCompField=0)
    compJson.assign(finalJson.begin(), finalJson.end());
    headerCompSize = headerSize;

    uint32_t blockCount = (uint32_t)compBlocks.size();

    fprintf(stderr, "[ahpx][writer] 头信息: headerSize=%u compSize=%u blockCount=%u\n",
            headerSize, headerCompSize, blockCount);

    // -------- 4. 写入文件 --------
    FILE* fp = openFile(path, "wb");
    if (!fp) {
        fprintf(stderr, "[ahpx][writer] 无法创建文件: %s\n", path.c_str());
        return false;
    }

    // 4.1 写入固定头 (18 字节)
    unsigned char fixedHeader[HEADER_FIXED_SIZE];
    std::memcpy(fixedHeader, MAGIC, 4);
    // Version (小端序)
    fixedHeader[4] = (unsigned char)(VERSION & 0xFF);
    fixedHeader[5] = (unsigned char)((VERSION >> 8) & 0xFF);
    // HeaderSize (小端序)
    fixedHeader[6] = (unsigned char)(headerSize & 0xFF);
    fixedHeader[7] = (unsigned char)((headerSize >> 8) & 0xFF);
    fixedHeader[8] = (unsigned char)((headerSize >> 16) & 0xFF);
    fixedHeader[9] = (unsigned char)((headerSize >> 24) & 0xFF);
    // HeaderCompSize (小端序) - 0 表示不压缩
    uint32_t hdrCompField = (headerCompSize == headerSize) ? 0 : headerCompSize;
    fixedHeader[10] = (unsigned char)(hdrCompField & 0xFF);
    fixedHeader[11] = (unsigned char)((hdrCompField >> 8) & 0xFF);
    fixedHeader[12] = (unsigned char)((hdrCompField >> 16) & 0xFF);
    fixedHeader[13] = (unsigned char)((hdrCompField >> 24) & 0xFF);
    // BlockCount (小端序)
    fixedHeader[14] = (unsigned char)(blockCount & 0xFF);
    fixedHeader[15] = (unsigned char)((blockCount >> 8) & 0xFF);
    fixedHeader[16] = (unsigned char)((blockCount >> 16) & 0xFF);
    fixedHeader[17] = (unsigned char)((blockCount >> 24) & 0xFF);

    if (std::fwrite(fixedHeader, 1, HEADER_FIXED_SIZE, fp) != HEADER_FIXED_SIZE) {
        fprintf(stderr, "[ahpx][writer] 写入固定头失败\n");
        std::fclose(fp);
        return false;
    }

    // 4.2 写入压缩 JSON 头
    uint32_t writeSize = (hdrCompField > 0) ? headerCompSize : headerSize;
    if (std::fwrite(compJson.data(), 1, writeSize, fp) != writeSize) {
        fprintf(stderr, "[ahpx][writer] 写入 JSON 头失败\n");
        std::fclose(fp);
        return false;
    }

    // 4.3 写入数据块
    for (const auto& cb : compBlocks) {
        if (std::fwrite(cb.data.data(), 1, cb.compSize, fp) != cb.compSize) {
            fprintf(stderr, "[ahpx][writer] 写入块 '%s' 失败\n", cb.id);
            std::fclose(fp);
            return false;
        }
    }

    std::fclose(fp);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    fprintf(stderr, "[ahpx][writer] 写入完成: %s (%.3f s)\n", path.c_str(), elapsed);

    return true;
}

} // namespace ahpx
