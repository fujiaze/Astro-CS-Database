#include "aio_ahpx_writer.h"
#include "../aio_compressor.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace aio::ahpx {

// ============================================================================
// 测试用故障注入 (先例: P2-002/P3-002/AIO-001 的 ASTROCS_*_FAULT)
// 仅供回归锁证明"能红"; 正常实现不读该变量。
//   writer_accept_legacy_meta - 跳过"元数据含已作废 weight 字段即拒绝"守卫
//   writer_drop_snr           - 跳过 snr 块写出
// ============================================================================
static bool ahpxFault(const char* name) {
    const char* v = std::getenv("ASTROCS_AHPX_FAULT");
    return v && std::string(v) == name;
}

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
    , m_hasSnr(false) {
}

AhpxWriter::~AhpxWriter() {
}

void AhpxWriter::setMetadata(const std::string& json) {
    m_metadataJson = json;
}

void AhpxWriter::setPixels(const float* data, int width, int height, int channels) {
    if (!data || width <= 0 || height <= 0 || channels <= 0) {
        fprintf(stderr, "[aio][ahpx][writer] setPixels: 无效参数 (data=%p w=%d h=%d c=%d)\n",
                (const void*)data, width, height, channels);
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
        fprintf(stderr, "[aio][ahpx][writer] setSnr: 无效参数 (data=%p w=%d h=%d)\n",
                (const void*)data, width, height);
        return;
    }
    size_t count = (size_t)width * height;
    m_snr.assign(data, data + count);
    m_hasSnr = true;
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
        fprintf(stderr, "[aio][ahpx][writer] compressAndWriteBlock: 无效参数\n");
        return false;
    }

    std::vector<uint8_t> compBuf;
    size_t compSize = 0;
    uint8_t usedCodec = codec;

    if (codec == (uint8_t)Codec::ZSTD) {
        // ZSTD 压缩
        size_t bound = aio::compressBoundZstd(srcSize);
        compBuf.resize(bound);
        compSize = aio::compressZstd(srcData, srcSize, compBuf.data(), bound, level);
        if (compSize == 0) {
            fprintf(stderr, "[aio][ahpx][writer] 块 '%s' ZSTD 压缩失败, 回退到不压缩\n", id);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        } else if (compSize >= srcSize) {
            // 压缩后比原始还大, 回退到不压缩
            fprintf(stderr, "[aio][ahpx][writer] 块 '%s' 压缩后更大 (%zu > %zu), 回退到不压缩\n",
                    id, compSize, srcSize);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        }
    } else if (codec == (uint8_t)Codec::LZ4) {
        // LZ4 压缩
        size_t bound = aio::compressBoundLz4(srcSize);
        compBuf.resize(bound);
        compSize = aio::compressLz4(srcData, srcSize, compBuf.data(), bound);
        if (compSize == 0) {
            fprintf(stderr, "[aio][ahpx][writer] 块 '%s' LZ4 压缩失败, 回退到不压缩\n", id);
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcSize;
            compBuf.resize(srcSize);
            std::memcpy(compBuf.data(), srcData, srcSize);
        } else if (compSize >= srcSize) {
            fprintf(stderr, "[aio][ahpx][writer] 块 '%s' LZ4 压缩后更大 (%zu > %zu), 回退到不压缩\n",
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
        fprintf(stderr, "[aio][ahpx][writer] ftell 失败\n");
        return false;
    }
    outOffset = (uint64_t)curPos;
    outCompSize = compSize;

    // 写入压缩数据
    if (std::fwrite(compBuf.data(), 1, compSize, fp) != compSize) {
        fprintf(stderr, "[aio][ahpx][writer] 块 '%s' 写入失败\n", id);
        return false;
    }

    fprintf(stderr, "[aio][ahpx][writer] 块 '%s': offset=%llu compSize=%llu codec=%u (原始=%zu)\n",
            id, (unsigned long long)outOffset, (unsigned long long)outCompSize,
            usedCodec, srcSize);

    // 注意: 返回的 outCompSize 和 outOffset 正确, 但 codec 需要调用方记录
    // 这里通过修改 compBlock 在 write 中处理
    (void)usedCodec; // 在 write 中单独处理
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

std::string AhpxWriter::injectBlocksIntoJson(const std::vector<BlockIndex>& blocks) {
    // 构建 blocks JSON 片段
    std::string blocksJson = buildBlocksJson(blocks);

    // 如果元数据 JSON 为空, 创建一个基础结构
    if (m_metadataJson.empty()) {
        std::string result = "{";
        result += "\"image\":{\"width\":" + std::to_string(m_width);
        result += ",\"height\":" + std::to_string(m_height);
        result += ",\"channels\":" + std::to_string(m_channels) + "}";
        result += ",\"blocks\":" + blocksJson;
        result += "}";
        return result;
    }

    // 已有 JSON: 移除末尾的 '}', 追加 blocks 字段
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

    // 追加 blocks 字段 (如果没有); 逗号由当前末尾字符决定 (禁前导逗号)
    if (!hasBlocks) {
        while (!result.empty() && (result.back() == ' ' || result.back() == '\t' ||
               result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        if (!result.empty() && result.back() != '{') {
            result += ",";
        }
        result += "\"blocks\":" + blocksJson;
    }

    result += "}";
    return result;
}

bool AhpxWriter::write(const std::string& path, const AhpxWriteConfig& config) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // -------- 1. 验证输入 --------
    if (m_pixels.empty() || m_width <= 0 || m_height <= 0 || m_channels <= 0) {
        fprintf(stderr, "[aio][ahpx][writer] 未设置像素数据或几何无效\n");
        return false;
    }

    // 已作废字段守卫 (变更 AHPX-WEIGHT-RETIRE-20260920; ASTROCS_DESIGN §2.1/A44):
    // 调用方元数据携带旧 "weight" 字段 ⇒ 拒绝写出 (既不产出读侧必拒的文件,
    // 也不静默丢弃调用方数据)。
    if (!ahpxFault("writer_accept_legacy_meta") &&
        hasJsonKey(m_metadataJson, RETIRED_WEIGHT_FIELD)) {
        fprintf(stderr, "[aio][ahpx][writer] 拒绝写出: 元数据含已作废的 \"%s\" 字段 "
                        "(权重模式已作废, 变更 AHPX-WEIGHT-RETIRE-20260920)\n",
                RETIRED_WEIGHT_FIELD);
        return false;
    }

    fprintf(stderr, "[aio][ahpx][writer] 开始写入: %s (w=%d h=%d c=%d pixels=%zu snr=%d)\n",
            path.c_str(), m_width, m_height, m_channels, m_pixels.size(),
            m_hasSnr ? 1 : 0);

    // -------- 2. 压缩所有数据块 (在内存中) --------
    std::vector<CompBlock> compBlocks;

    // 2.1 压缩 pixel 块
    {
        CompBlock cb;
        std::memset(cb.id, 0, sizeof(cb.id));
        std::strncpy(cb.id, "pixel", sizeof(cb.id) - 1);
        cb.codec = (uint8_t)Codec::ZSTD;
        cb.level = config.zstdLevel;

        size_t srcBytes = m_pixels.size() * sizeof(float);
        size_t bound = aio::compressBoundZstd(srcBytes);
        cb.data.resize(bound);
        size_t compSize = aio::compressZstd(m_pixels.data(), srcBytes,
                                       cb.data.data(), bound, config.zstdLevel);
        if (compSize == 0 || compSize >= srcBytes) {
            // 回退到不压缩
            cb.codec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            cb.data.resize(srcBytes);
            std::memcpy(cb.data.data(), m_pixels.data(), srcBytes);
        }
        cb.compSize = compSize;
        compBlocks.push_back(std::move(cb));
        fprintf(stderr, "[aio][ahpx][writer] pixel 块压缩完成: %zu -> %zu bytes (codec=%u)\n",
                srcBytes, compSize, compBlocks.back().codec);
    }

    // 2.2 压缩 snr 块 (如果有)
    if (!ahpxFault("writer_drop_snr") && m_hasSnr && !m_snr.empty()) {
        CompBlock cb;
        std::memset(cb.id, 0, sizeof(cb.id));
        std::strncpy(cb.id, "snr", sizeof(cb.id) - 1);
        cb.codec = (uint8_t)Codec::ZSTD;
        cb.level = config.zstdLevel;

        size_t srcBytes = m_snr.size() * sizeof(float);
        size_t bound = aio::compressBoundZstd(srcBytes);
        cb.data.resize(bound);
        size_t compSize = aio::compressZstd(m_snr.data(), srcBytes,
                                       cb.data.data(), bound, config.zstdLevel);
        if (compSize == 0 || compSize >= srcBytes) {
            cb.codec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            cb.data.resize(srcBytes);
            std::memcpy(cb.data.data(), m_snr.data(), srcBytes);
        }
        cb.compSize = compSize;
        compBlocks.push_back(std::move(cb));
        fprintf(stderr, "[aio][ahpx][writer] snr 块压缩完成: %zu -> %zu bytes (codec=%u)\n",
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
        finalJson = injectBlocksIntoJson(blkIdx);
        headerSize = (uint32_t)finalJson.size();

        // 压缩 JSON 头
        if (config.zstdLevel > 0) {
            size_t bound = aio::compressBoundZstd(headerSize);
            compJson.resize(bound);
            size_t compSize = aio::compressZstd(finalJson.data(), headerSize,
                                           compJson.data(), bound, config.zstdLevel);
            if (compSize == 0 || compSize >= headerSize) {
                // 回退到不压缩
                compJson.assign(finalJson.begin(), finalJson.end());
                compSize = headerSize;
                fprintf(stderr, "[aio][ahpx][writer] JSON 头压缩失败/更大, 回退到不压缩\n");
            }
            compJson.resize(compSize);
            uint32_t newHeaderCompSize = (uint32_t)compSize;

            // 检查是否收敛
            if (newHeaderCompSize == headerCompSize) {
                break;
            }
            headerCompSize = newHeaderCompSize;
        } else {
            // 不压缩 JSON
            compJson.assign(finalJson.begin(), finalJson.end());
            headerCompSize = headerSize;
            break;
        }

        fprintf(stderr, "[aio][ahpx][writer] 迭代 %d: headerCompSize=%u headerSize=%u\n",
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

        finalJson = injectBlocksIntoJson(blkIdx);
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

    fprintf(stderr, "[aio][ahpx][writer] 头信息: headerSize=%u compSize=%u blockCount=%u\n",
            headerSize, headerCompSize, blockCount);

    // -------- 4. 写入文件 --------
    FILE* fp = openFile(path, "wb");
    if (!fp) {
        fprintf(stderr, "[aio][ahpx][writer] 无法创建文件: %s\n", path.c_str());
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
        fprintf(stderr, "[aio][ahpx][writer] 写入固定头失败\n");
        std::fclose(fp);
        return false;
    }

    // 4.2 写入压缩 JSON 头
    uint32_t writeSize = (hdrCompField > 0) ? headerCompSize : headerSize;
    if (std::fwrite(compJson.data(), 1, writeSize, fp) != writeSize) {
        fprintf(stderr, "[aio][ahpx][writer] 写入 JSON 头失败\n");
        std::fclose(fp);
        return false;
    }

    // 4.3 写入数据块
    for (const auto& cb : compBlocks) {
        if (std::fwrite(cb.data.data(), 1, cb.compSize, fp) != cb.compSize) {
            fprintf(stderr, "[aio][ahpx][writer] 写入块 '%s' 失败\n", cb.id);
            std::fclose(fp);
            return false;
        }
    }

    std::fclose(fp);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    fprintf(stderr, "[aio][ahpx][writer] 写入完成: %s (%.3f s)\n", path.c_str(), elapsed);

    return true;
}

} // namespace aio::ahpx
