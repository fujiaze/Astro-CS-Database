#include "ahps_writer.h"
#include "astro_image_io.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ahps {

// ============================================================================
// UTF-8 路径文件打开辅助
// ============================================================================
FILE* AhpsWriter::openFile(const std::string& path, const char* mode) {
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path.c_str(), mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wpath_len);
    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// ============================================================================
// AhpsWriter 实现
// ============================================================================
AhpsWriter::AhpsWriter()
    : m_nside(0)
    , m_tileNside(0)
    , m_tileIpix(0)
    , m_bandCount(0) {
}

AhpsWriter::~AhpsWriter() {
}

void AhpsWriter::setNside(int nside)                 { m_nside = nside; }
void AhpsWriter::setTileNside(int tileNside)         { m_tileNside = tileNside; }
void AhpsWriter::setTileIpix(int64_t tileIpix)       { m_tileIpix = tileIpix; }
void AhpsWriter::setBandCount(int bandCount)         { m_bandCount = bandCount; }
void AhpsWriter::setHeaderJson(const std::string& j) { m_headerJson = j; }
void AhpsWriter::setPixelIndices(const std::vector<uint64_t>& indices) {
    m_pixelIndices = indices;
}
void AhpsWriter::setBandStats(int bandIndex, const std::vector<PixelStats>& stats) {
    m_bandStats[bandIndex] = stats;
}

// ----------------------------------------------------------------------------
// 分块压缩写入: 把原始数据按 CHUNK_SIZE 切分, 每块独立压缩后写入文件
// ----------------------------------------------------------------------------
bool AhpsWriter::writeChunked(FILE* fp, const void* src, size_t elemSize,
                              size_t totalElems, int zstdLevel,
                              std::vector<ChunkIndex>& outChunks) {
    outChunks.clear();
    if (totalElems == 0) return true;

    const uint8_t* base = (const uint8_t*)src;
    size_t numChunks = (totalElems + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (size_t c = 0; c < numChunks; c++) {
        size_t start = c * CHUNK_SIZE;
        size_t cnt = std::min((size_t)CHUNK_SIZE, totalElems - start);
        size_t srcBytes = cnt * elemSize;
        const uint8_t* srcPtr = base + start * elemSize;

        ChunkIndex ci;
        std::memset(&ci, 0, sizeof(ci));
        ci.rawCount = (uint32_t)cnt;

        // 尝试 ZSTD 压缩 (通过 AIO C API)
        size_t bound = aio_compress_bound(srcBytes, 1/*ZSTD*/);
        std::vector<uint8_t> compBuf(bound);
        size_t compSize = aio_compress(srcPtr, srcBytes,
                                       compBuf.data(), bound, 1/*ZSTD*/, zstdLevel);
        uint8_t usedCodec = (uint8_t)Codec::ZSTD;
        if (compSize == 0 || compSize >= srcBytes) {
            // 回退不压缩
            usedCodec = (uint8_t)Codec::NONE;
            compSize = srcBytes;
            compBuf.resize(srcBytes);
            std::memcpy(compBuf.data(), srcPtr, srcBytes);
        }

        long pos = std::ftell(fp);
        if (pos < 0) {
            fprintf(stderr, "[ahps][writer] ftell 失败\n");
            return false;
        }
        ci.offset = (uint64_t)pos;
        ci.size = compSize;
        ci.codec = usedCodec;

        if (std::fwrite(compBuf.data(), 1, compSize, fp) != compSize) {
            fprintf(stderr, "[ahps][writer] chunk %zu 写入失败\n", c);
            return false;
        }
        outChunks.push_back(ci);
    }
    return true;
}

// ----------------------------------------------------------------------------
// 构建 chunk 对象 JSON
// ----------------------------------------------------------------------------
static std::string chunkToJson(const ChunkIndex& ci) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"offset\":%llu,\"size\":%llu,\"rawCount\":%u,\"codec\":%u}",
        (unsigned long long)ci.offset,
        (unsigned long long)ci.size,
        ci.rawCount, ci.codec);
    return std::string(buf);
}

static std::string chunksToJson(const std::vector<ChunkIndex>& chunks) {
    std::string s = "[";
    for (size_t i = 0; i < chunks.size(); i++) {
        if (i > 0) s += ",";
        s += chunkToJson(chunks[i]);
    }
    s += "]";
    return s;
}

// ----------------------------------------------------------------------------
// 构建 JSON 头
// ----------------------------------------------------------------------------
std::string AhpsWriter::buildHeaderJson(const std::vector<ChunkIndex>& pixelChunks,
                                        const std::vector<std::vector<ChunkIndex>>& bandChunks,
                                        int zstdLevel) {
    std::string json = "{";
    // 数据层信息
    char info[128];
    std::snprintf(info, sizeof(info),
        "\"nside\":%d,\"tileNside\":%d,\"tileIpix\":%lld,\"pixelCount\":%lld,\"bandCount\":%d,\"zstdLevel\":%d",
        m_nside, m_tileNside, (long long)m_tileIpix,
        (long long)m_pixelIndices.size(), m_bandCount, zstdLevel);
    json += info;

    // 用户自定义 headerJson 注入 (若有)
    if (!m_headerJson.empty()) {
        std::string userJson = m_headerJson;
        // 去掉首尾的 { }
        while (!userJson.empty() && (userJson.front() == ' ' || userJson.front() == '\t')) userJson.erase(0,1);
        while (!userJson.empty() && (userJson.back() == ' ' || userJson.back() == '\t')) userJson.pop_back();
        if (!userJson.empty() && userJson.front() == '{') userJson.erase(0,1);
        if (!userJson.empty() && userJson.back() == '}') userJson.pop_back();
        // 去除末尾空白
        while (!userJson.empty() && (userJson.back()==' '||userJson.back()=='\t'||userJson.back()=='\n'||userJson.back()=='\r')) userJson.pop_back();
        if (!userJson.empty()) {
            json += ",";
            json += userJson;
        }
    }

    // pixelChunks
    json += ",\"pixelChunks\":";
    json += chunksToJson(pixelChunks);

    // bandChunks
    json += ",\"bandChunks\":[";
    for (int b = 0; b < m_bandCount; b++) {
        if (b > 0) json += ",";
        if (b < (int)bandChunks.size()) json += chunksToJson(bandChunks[b]);
        else json += "[]";
    }
    json += "]";

    json += "}";
    return json;
}

// ----------------------------------------------------------------------------
// 写入文件
// ----------------------------------------------------------------------------
bool AhpsWriter::write(const std::string& path, int zstdLevel) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // 校验
    if (m_nside <= 0 || m_tileNside <= 0 || m_bandCount <= 0) {
        fprintf(stderr, "[ahps][writer] 参数无效 (nside=%d tileNside=%d bandCount=%d)\n",
                m_nside, m_tileNside, m_bandCount);
        return false;
    }
    if (m_pixelIndices.empty()) {
        fprintf(stderr, "[ahps][writer] 像素索引为空\n");
        return false;
    }

    // 检查所有波段统计量已设置
    for (int b = 0; b < m_bandCount; b++) {
        auto it = m_bandStats.find(b);
        if (it == m_bandStats.end()) {
            fprintf(stderr, "[ahps][writer] 波段 %d 统计量未设置\n", b);
            return false;
        }
        if (it->second.size() != m_pixelIndices.size()) {
            fprintf(stderr, "[ahps][writer] 波段 %d 统计量数量(%zu) != 像素数(%zu)\n",
                    b, it->second.size(), m_pixelIndices.size());
            return false;
        }
    }

    fprintf(stderr, "[ahps][writer] 开始写入: %s (nside=%d tileIpix=%lld pixels=%zu bands=%d)\n",
            path.c_str(), m_nside, (long long)m_tileIpix,
            m_pixelIndices.size(), m_bandCount);

    // -------- 1. 在内存中压缩所有数据块 --------
    // 用临时内存缓冲区存储所有压缩后的 chunk 数据
    struct ChunkData {
        std::vector<uint8_t> data;
        ChunkIndex index;
    };
    std::vector<ChunkData> pixelChunkData;
    std::vector<std::vector<ChunkData>> bandChunkData(m_bandCount);

    // 像素索引块
    {
        size_t totalElems = m_pixelIndices.size();
        const void* src = m_pixelIndices.data();
        size_t elemSize = sizeof(uint64_t);
        size_t numChunks = (totalElems + CHUNK_SIZE - 1) / CHUNK_SIZE;
        pixelChunkData.resize(numChunks);
        for (size_t c = 0; c < numChunks; c++) {
            size_t start = c * CHUNK_SIZE;
            size_t cnt = std::min((size_t)CHUNK_SIZE, totalElems - start);
            size_t srcBytes = cnt * elemSize;
            const uint8_t* srcPtr = (const uint8_t*)src + start * elemSize;

            size_t bound = aio_compress_bound(srcBytes, 1/*ZSTD*/);
            std::vector<uint8_t> compBuf(bound);
            size_t compSize = aio_compress(srcPtr, srcBytes, compBuf.data(), bound, 1/*ZSTD*/, zstdLevel);
            uint8_t usedCodec = (uint8_t)Codec::ZSTD;
            if (compSize == 0 || compSize >= srcBytes) {
                usedCodec = (uint8_t)Codec::NONE;
                compSize = srcBytes;
                compBuf.resize(srcBytes);
                std::memcpy(compBuf.data(), srcPtr, srcBytes);
            } else {
                // ZSTD 压缩成功: resize 为实际压缩大小, 确保 data.size() == index.size
                // 否则写入文件时会多写 bound 字节, 导致后续 chunk offset 偏移
                compBuf.resize(compSize);
            }
            pixelChunkData[c].data = std::move(compBuf);
            pixelChunkData[c].index.rawCount = (uint32_t)cnt;
            pixelChunkData[c].index.size = compSize;
            pixelChunkData[c].index.codec = usedCodec;
            // offset 待计算
        }
    }

    // 各波段统计块
    for (int b = 0; b < m_bandCount; b++) {
        const auto& stats = m_bandStats[b];
        size_t totalElems = stats.size();
        size_t elemSize = sizeof(PixelStats);
        size_t numChunks = (totalElems + CHUNK_SIZE - 1) / CHUNK_SIZE;
        bandChunkData[b].resize(numChunks);
        for (size_t c = 0; c < numChunks; c++) {
            size_t start = c * CHUNK_SIZE;
            size_t cnt = std::min((size_t)CHUNK_SIZE, totalElems - start);
            size_t srcBytes = cnt * elemSize;
            const uint8_t* srcPtr = (const uint8_t*)stats.data() + start * elemSize;

            size_t bound = aio_compress_bound(srcBytes, 1/*ZSTD*/);
            std::vector<uint8_t> compBuf(bound);
            size_t compSize = aio_compress(srcPtr, srcBytes, compBuf.data(), bound, 1/*ZSTD*/, zstdLevel);
            uint8_t usedCodec = (uint8_t)Codec::ZSTD;
            if (compSize == 0 || compSize >= srcBytes) {
                usedCodec = (uint8_t)Codec::NONE;
                compSize = srcBytes;
                compBuf.resize(srcBytes);
                std::memcpy(compBuf.data(), srcPtr, srcBytes);
            } else {
                // ZSTD 压缩成功: resize 为实际压缩大小, 确保 data.size() == index.size
                compBuf.resize(compSize);
            }
            bandChunkData[b][c].data = std::move(compBuf);
            bandChunkData[b][c].index.rawCount = (uint32_t)cnt;
            bandChunkData[b][c].index.size = compSize;
            bandChunkData[b][c].index.codec = usedCodec;
        }
    }

    // -------- 2. 迭代计算 offset 与 JSON 头大小 (直到收敛) --------
    // JSON 头包含 chunk 的 offset, offset 依赖 headerCompSize, 而 headerCompSize 依赖 JSON 内容
    // 迭代直到 headerSize 稳定 (JSON 头不压缩写入, 避免 size 振荡)
    uint32_t headerSize = 0;
    std::vector<ChunkIndex> pixelChunks;
    std::vector<std::vector<ChunkIndex>> bandChunks(m_bandCount);
    std::string finalJson;

    for (int iter = 0; iter < 20; iter++) {
        // 计算各 chunk offset
        uint64_t curOffset = HEADER_FIXED_SIZE + headerSize;
        pixelChunks.clear();
        for (const auto& cd : pixelChunkData) {
            ChunkIndex ci = cd.index;
            ci.offset = curOffset;
            pixelChunks.push_back(ci);
            curOffset += ci.size;
        }
        bandChunks.assign(m_bandCount, {});
        for (int b = 0; b < m_bandCount; b++) {
            for (const auto& cd : bandChunkData[b]) {
                ChunkIndex ci = cd.index;
                ci.offset = curOffset;
                bandChunks[b].push_back(ci);
                curOffset += ci.size;
            }
        }

        finalJson = buildHeaderJson(pixelChunks, bandChunks, zstdLevel);
        uint32_t newSize = (uint32_t)finalJson.size();
        if (newSize == headerSize) break;  // 收敛
        headerSize = newSize;
        fprintf(stderr, "[ahps][writer] 迭代 %d: headerSize=%u\n", iter, headerSize);
    }

    // JSON 头不压缩 (headerCompSize=0 表示不压缩)
    uint32_t headerCompSize = 0;

    // -------- 3. 写入文件 --------
    FILE* fp = openFile(path, "wb");
    if (!fp) {
        fprintf(stderr, "[ahps][writer] 无法创建文件: %s\n", path.c_str());
        return false;
    }

    // 3.1 固定头 (38 字节, 小端序)
    unsigned char hdr[HEADER_FIXED_SIZE];
    std::memcpy(hdr, MAGIC, 4);
    hdr[4] = (unsigned char)(VERSION & 0xFF);
    hdr[5] = (unsigned char)((VERSION >> 8) & 0xFF);
    // Nside
    auto putU32 = [](unsigned char* d, uint32_t v) {
        d[0] = (unsigned char)(v & 0xFF);
        d[1] = (unsigned char)((v >> 8) & 0xFF);
        d[2] = (unsigned char)((v >> 16) & 0xFF);
        d[3] = (unsigned char)((v >> 24) & 0xFF);
    };
    auto putU64 = [](unsigned char* d, uint64_t v) {
        for (int i = 0; i < 8; i++) d[i] = (unsigned char)((v >> (i*8)) & 0xFF);
    };
    unsigned char* hp = hdr + 6;
    putU32(hp, (uint32_t)m_nside);      hp += 4;
    putU32(hp, (uint32_t)m_tileNside);  hp += 4;
    putU64(hp, (uint64_t)m_tileIpix);   hp += 8;
    putU32(hp, (uint32_t)m_pixelIndices.size()); hp += 4;
    putU32(hp, (uint32_t)m_bandCount);  hp += 4;
    putU32(hp, headerSize);             hp += 4;
    putU32(hp, headerCompSize);         hp += 4;

    if (std::fwrite(hdr, 1, HEADER_FIXED_SIZE, fp) != HEADER_FIXED_SIZE) {
        fprintf(stderr, "[ahps][writer] 写入固定头失败\n");
        std::fclose(fp);
        return false;
    }

    // 3.2 JSON 头 (未压缩)
    if (std::fwrite(finalJson.data(), 1, headerSize, fp) != headerSize) {
        fprintf(stderr, "[ahps][writer] 写入 JSON 头失败\n");
        std::fclose(fp);
        return false;
    }

    // 3.3 像素索引块
    for (const auto& cd : pixelChunkData) {
        if (std::fwrite(cd.data.data(), 1, cd.data.size(), fp) != cd.data.size()) {
            fprintf(stderr, "[ahps][writer] 写入像素索引块失败\n");
            std::fclose(fp);
            return false;
        }
    }

    // 3.4 各波段统计块
    for (int b = 0; b < m_bandCount; b++) {
        for (const auto& cd : bandChunkData[b]) {
            if (std::fwrite(cd.data.data(), 1, cd.data.size(), fp) != cd.data.size()) {
                fprintf(stderr, "[ahps][writer] 写入波段 %d 统计块失败\n", b);
                std::fclose(fp);
                return false;
            }
        }
    }

    std::fclose(fp);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    fprintf(stderr, "[ahps][writer] 写入完成: %s (headerSize=%u 耗时 %.3f s)\n",
            path.c_str(), headerSize, elapsed);
    return true;
}

} // namespace ahps
