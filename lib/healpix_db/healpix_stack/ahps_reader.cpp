#include "ahps_reader.h"
#include "../ahpx_io/compressor.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ahps {

// ============================================================================
// UTF-8 路径文件打开辅助 (Windows 下支持中文路径)
// ============================================================================
FILE* AhpsReader::openFile(const std::string& path, const char* mode) {
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
// 简单 JSON 字符串解析辅助 (避免引入外部 JSON 库)
// ============================================================================
static size_t skipWs(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
        else break;
    }
    return pos;
}

// 查找 "key": 后的值起始位置
static size_t findKeyValue(const std::string& s, const std::string& key, size_t startPos = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = s.find(pattern, startPos);
    if (pos == std::string::npos) return std::string::npos;
    pos += pattern.size();
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != ':') return std::string::npos;
    pos++;
    return skipWs(s, pos);
}

static double extractNumber(const std::string& s, size_t valStart, size_t* outEnd) {
    size_t i = valStart;
    if (i < s.size() && s[i] == '-') i++;
    while (i < s.size()) {
        char c = s[i];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') i++;
        else break;
    }
    std::string numStr = s.substr(valStart, i - valStart);
    if (outEnd) *outEnd = i;
    if (numStr.empty()) return 0.0;
    return std::strtod(numStr.c_str(), nullptr);
}

// 提取字符串值 (valStart 指向 '"')
static std::string extractString(const std::string& s, size_t valStart, size_t* outEnd) {
    if (valStart >= s.size() || s[valStart] != '"') {
        if (outEnd) *outEnd = valStart;
        return "";
    }
    size_t i = valStart + 1;
    std::string result;
    while (i < s.size()) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            switch (next) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"';  break;
                case '\\': result += '\\'; break;
                default:   result += next; break;
            }
            i += 2;
        } else if (c == '"') {
            i++; break;
        } else {
            result += c; i++;
        }
    }
    if (outEnd) *outEnd = i;
    return result;
}

// 解析一个 chunk 对象字符串 {"offset":..,"size":..,"rawCount":..,"codec":..}
static bool parseChunkObj(const std::string& objStr, ChunkIndex& out) {
    size_t p;
    p = findKeyValue(objStr, "offset");
    if (p != std::string::npos) out.offset = (uint64_t)extractNumber(objStr, p, nullptr);
    p = findKeyValue(objStr, "size");
    if (p != std::string::npos) out.size = (uint64_t)extractNumber(objStr, p, nullptr);
    p = findKeyValue(objStr, "rawCount");
    if (p != std::string::npos) out.rawCount = (uint32_t)extractNumber(objStr, p, nullptr);
    p = findKeyValue(objStr, "codec");
    if (p != std::string::npos) out.codec = (uint8_t)extractNumber(objStr, p, nullptr);
    return true;
}

// 解析 chunk 数组 JSON: [{...},{...},...]
static std::vector<ChunkIndex> parseChunkArray(const std::string& s, size_t arrStart) {
    std::vector<ChunkIndex> result;
    if (arrStart >= s.size() || s[arrStart] != '[') return result;
    size_t pos = arrStart + 1;
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != ']') {
        if (s[pos] != '{') { pos++; continue; }
        // 匹配 {...}
        int depth = 0;
        size_t objStart = pos;
        size_t objEnd = pos;
        while (objEnd < s.size()) {
            char c = s[objEnd];
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) { objEnd++; break; } }
            objEnd++;
        }
        std::string objStr = s.substr(objStart, objEnd - objStart);
        ChunkIndex ci;
        std::memset(&ci, 0, sizeof(ci));
        if (parseChunkObj(objStr, ci)) result.push_back(ci);
        pos = objEnd;
        pos = skipWs(s, pos);
        if (pos < s.size() && s[pos] == ',') { pos++; pos = skipWs(s, pos); }
    }
    return result;
}

// ============================================================================
// AhpsReader 实现
// ============================================================================
AhpsReader::AhpsReader()
    : m_fp(nullptr)
    , m_nside(0)
    , m_tileNside(0)
    , m_tileIpix(0)
    , m_pixelCount(0)
    , m_bandCount(0)
    , m_headerSize(0)
    , m_headerCompSize(0) {
}

AhpsReader::~AhpsReader() {
    close();
}

bool AhpsReader::open(const std::string& path) {
    close();
    m_path = path;
    m_fp = openFile(path, "rb");
    if (!m_fp) {
        fprintf(stderr, "[ahps][reader] 无法打开文件: %s\n", path.c_str());
        return false;
    }

    // -------- 读取固定头 (38 字节) --------
    unsigned char hdr[HEADER_FIXED_SIZE];
    if (std::fread(hdr, 1, HEADER_FIXED_SIZE, m_fp) != HEADER_FIXED_SIZE) {
        fprintf(stderr, "[ahps][reader] 读取固定头失败: %s\n", path.c_str());
        close();
        return false;
    }
    if (std::memcmp(hdr, MAGIC, 4) != 0) {
        fprintf(stderr, "[ahps][reader] Magic 校验失败: %s\n", path.c_str());
        close();
        return false;
    }
    // Version (小端)
    uint16_t ver = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
    if (ver != VERSION) {
        fprintf(stderr, "[ahps][reader] 版本不兼容 (文件=%u 支持=%u)\n", ver, VERSION);
        close();
        return false;
    }
    const unsigned char* p = hdr + 6;
    m_nside      = (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    p += 4;
    m_tileNside  = (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    p += 4;
    m_tileIpix   = (int64_t)((uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
                             ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
                             ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56));
    p += 8;
    m_pixelCount = (int64_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    p += 4;
    m_bandCount  = (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    p += 4;
    m_headerSize     = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    m_headerCompSize = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    fprintf(stderr, "[ahps][reader] 头: nside=%d tileNside=%d tileIpix=%lld pixelCount=%lld bandCount=%d hdrSize=%u compSize=%u\n",
            m_nside, m_tileNside, (long long)m_tileIpix, (long long)m_pixelCount,
            m_bandCount, m_headerSize, m_headerCompSize);

    // -------- 读取 JSON 头 --------
    uint32_t readSize = (m_headerCompSize > 0) ? m_headerCompSize : m_headerSize;
    if (readSize == 0) {
        fprintf(stderr, "[ahps][reader] JSON 头大小为 0\n");
        close();
        return false;
    }
    std::vector<uint8_t> rawJson(readSize);
    if (std::fread(rawJson.data(), 1, readSize, m_fp) != readSize) {
        fprintf(stderr, "[ahps][reader] 读取 JSON 头失败\n");
        close();
        return false;
    }
    if (m_headerCompSize > 0) {
        m_headerJson.resize(m_headerSize);
        size_t got = ahpx::decompressZstd(rawJson.data(), readSize, &m_headerJson[0], m_headerSize);
        if (got != m_headerSize) {
            fprintf(stderr, "[ahps][reader] JSON 头解压失败: 期望 %u 实际 %zu\n", m_headerSize, got);
            close();
            return false;
        }
    } else {
        m_headerJson.assign(reinterpret_cast<const char*>(rawJson.data()), readSize);
    }

    // -------- 解析 JSON 头 (chunk 索引) --------
    if (!parseHeader()) {
        fprintf(stderr, "[ahps][reader] JSON 头解析失败\n");
        close();
        return false;
    }

    fprintf(stderr, "[ahps][reader] 成功打开: %s (pixelChunks=%zu bandCount=%d)\n",
            path.c_str(), m_pixelChunks.size(), m_bandCount);
    return true;
}

bool AhpsReader::parseHeader() {
    m_pixelChunks.clear();
    m_bandChunks.clear();
    m_bandChunks.resize(m_bandCount);

    // 解析 "pixelChunks" 数组
    size_t pos = findKeyValue(m_headerJson, "pixelChunks");
    if (pos != std::string::npos && pos < m_headerJson.size() && m_headerJson[pos] == '[') {
        m_pixelChunks = parseChunkArray(m_headerJson, pos);
    }

    // 解析 "bandChunks": [[...],[...],...]
    size_t bp = findKeyValue(m_headerJson, "bandChunks");
    if (bp != std::string::npos && bp < m_headerJson.size() && m_headerJson[bp] == '[') {
        size_t arrPos = bp + 1;
        arrPos = skipWs(m_headerJson, arrPos);
        int bi = 0;
        while (arrPos < m_headerJson.size() && m_headerJson[arrPos] != ']' && bi < m_bandCount) {
            if (m_headerJson[arrPos] == '[') {
                auto chunks = parseChunkArray(m_headerJson, arrPos);
                m_bandChunks[bi] = chunks;
                bi++;
                // 跳过该子数组
                int depth = 0;
                while (arrPos < m_headerJson.size()) {
                    char c = m_headerJson[arrPos];
                    if (c == '[') depth++;
                    else if (c == ']') { depth--; if (depth == 0) { arrPos++; break; } }
                    arrPos++;
                }
            } else {
                arrPos++;
            }
            arrPos = skipWs(m_headerJson, arrPos);
            if (arrPos < m_headerJson.size() && m_headerJson[arrPos] == ',') { arrPos++; arrPos = skipWs(m_headerJson, arrPos); }
        }
    }
    return true;
}

int  AhpsReader::getNside() const      { return m_nside; }
int  AhpsReader::getTileNside() const   { return m_tileNside; }
int64_t AhpsReader::getTileIpix() const { return m_tileIpix; }
int64_t AhpsReader::getPixelCount() const { return m_pixelCount; }
int  AhpsReader::getBandCount() const   { return m_bandCount; }
const std::string& AhpsReader::getHeaderJson() const { return m_headerJson; }

bool AhpsReader::readRaw(uint64_t offset, uint64_t size, void* dst) {
    if (!m_fp) return false;
    if (std::fseek(m_fp, (long)offset, SEEK_SET) != 0) {
        fprintf(stderr, "[ahps][reader] fseek 失败 (offset=%llu)\n", (unsigned long long)offset);
        return false;
    }
    size_t got = std::fread(dst, 1, size, m_fp);
    if (got != size) {
        fprintf(stderr, "[ahps][reader] fread 不完整 (期望 %llu 实际 %zu)\n",
                (unsigned long long)size, got);
        return false;
    }
    return true;
}

std::vector<uint8_t> AhpsReader::readChunk(const ChunkIndex& chunk) {
    std::vector<uint8_t> result;
    if (chunk.size == 0) return result;
    std::vector<uint8_t> compData(chunk.size);
    if (!readRaw(chunk.offset, chunk.size, compData.data())) return result;

    if (chunk.codec == (uint8_t)Codec::NONE) {
        result = std::move(compData);
    } else if (chunk.codec == (uint8_t)Codec::ZSTD) {
        // 循环扩容解压
        size_t est = chunk.size * 8;
        if (est < 1024) est = 1024;
        size_t got = 0;
        for (int a = 0; a < 8; a++) {
            result.resize(est);
            got = ahpx::decompressZstd(compData.data(), compData.size(), result.data(), est);
            if (got > 0) break;
            est *= 4;
        }
        if (got == 0) { result.clear(); return result; }
        result.resize(got);
    } else if (chunk.codec == (uint8_t)Codec::LZ4) {
        size_t est = chunk.size * 8;
        if (est < 1024) est = 1024;
        size_t got = 0;
        for (int a = 0; a < 8; a++) {
            result.resize(est);
            got = ahpx::decompressLz4(compData.data(), compData.size(), result.data(), est);
            if (got > 0) break;
            est *= 4;
        }
        if (got == 0) { result.clear(); return result; }
        result.resize(got);
    } else {
        fprintf(stderr, "[ahps][reader] 未知 codec=%u\n", chunk.codec);
    }
    return result;
}

std::vector<uint64_t> AhpsReader::readPixelIndices() {
    std::vector<uint64_t> result;
    if (m_pixelChunks.empty()) {
        fprintf(stderr, "[ahps][reader] readPixelIndices: 无像素索引块\n");
        return result;
    }
    // 拼接所有 chunk 的解压数据
    std::vector<uint8_t> all;
    for (const auto& ci : m_pixelChunks) {
        auto part = readChunk(ci);
        if (part.empty()) {
            fprintf(stderr, "[ahps][reader] 像素索引 chunk 解压失败\n");
            return result;
        }
        all.insert(all.end(), part.begin(), part.end());
    }
    size_t expectBytes = (size_t)m_pixelCount * sizeof(uint64_t);
    if (all.size() != expectBytes) {
        fprintf(stderr, "[ahps][reader] 像素索引大小不匹配 (期望 %zu 实际 %zu)\n",
                expectBytes, all.size());
        return result;
    }
    result.resize(m_pixelCount);
    std::memcpy(result.data(), all.data(), all.size());
    return result;
}

std::vector<PixelStats> AhpsReader::readBandStats(int bandIndex) {
    std::vector<PixelStats> result;
    if (bandIndex < 0 || bandIndex >= m_bandCount) {
        fprintf(stderr, "[ahps][reader] readBandStats: 波段越界 %d (bandCount=%d)\n",
                bandIndex, m_bandCount);
        return result;
    }
    if ((int)m_bandChunks.size() <= bandIndex || m_bandChunks[bandIndex].empty()) {
        fprintf(stderr, "[ahps][reader] readBandStats: 波段 %d 无 chunk 索引\n", bandIndex);
        return result;
    }
    std::vector<uint8_t> all;
    for (const auto& ci : m_bandChunks[bandIndex]) {
        auto part = readChunk(ci);
        if (part.empty()) {
            fprintf(stderr, "[ahps][reader] 波段 %d chunk 解压失败\n", bandIndex);
            return result;
        }
        all.insert(all.end(), part.begin(), part.end());
    }
    size_t expectBytes = (size_t)m_pixelCount * sizeof(PixelStats);
    if (all.size() != expectBytes) {
        fprintf(stderr, "[ahps][reader] 波段 %d 统计量大小不匹配 (期望 %zu 实际 %zu)\n",
                bandIndex, expectBytes, all.size());
        return result;
    }
    result.resize(m_pixelCount);
    std::memcpy(result.data(), all.data(), all.size());
    return result;
}

bool AhpsReader::readBandValues(int bandIndex,
                                std::vector<float>& outValues,
                                std::vector<float>& outVariance) {
    outValues.clear();
    outVariance.clear();
    auto stats = readBandStats(bandIndex);
    if (stats.empty()) return false;

    outValues.resize(stats.size());
    outVariance.resize(stats.size());
    for (size_t i = 0; i < stats.size(); i++) {
        const PixelStats& s = stats[i];
        if (s.weightSum > 0.0f && s.count > 0) {
            float val = s.sum / s.weightSum;
            outValues[i] = val;
            // 加权方差 = E[x²] - E[x]² = (sumSq/WSum) - val²
            float e_x2 = s.sumSq / s.weightSum;
            float var = e_x2 - val * val;
            if (var < 0.0f) var = 0.0f;
            outVariance[i] = var;
        } else {
            outValues[i] = 0.0f;
            outVariance[i] = 0.0f;
        }
    }
    return true;
}

void AhpsReader::close() {
    if (m_fp) { std::fclose(m_fp); m_fp = nullptr; }
    m_path.clear();
    m_headerJson.clear();
    m_pixelChunks.clear();
    m_bandChunks.clear();
    m_nside = 0;
    m_tileNside = 0;
    m_tileIpix = 0;
    m_pixelCount = 0;
    m_bandCount = 0;
    m_headerSize = 0;
    m_headerCompSize = 0;
}

} // namespace ahps
