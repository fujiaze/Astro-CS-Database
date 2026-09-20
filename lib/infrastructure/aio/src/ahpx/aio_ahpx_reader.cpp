#include "aio_ahpx_reader.h"
#include "../aio_compressor.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace aio::ahpx {

// ============================================================================
// 测试用故障注入 (先例: P2-002/P3-002/AIO-001 的 ASTROCS_*_FAULT)
// 仅供回归锁证明"能红"; 正常实现不读该变量。
//   accept_legacy - 跳过"旧格式含已作废 weight 字段/块即拒绝"守卫
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
// 简单 JSON 字符串解析辅助 (避免引入 nlohmann/json 依赖)
// ============================================================================

// 跳过空白字符, 返回跳过后的位置
static size_t skipWs(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            pos++;
        } else {
            break;
        }
    }
    return pos;
}

// 在 JSON 字符串中查找键的位置 (找 "key" 后面跟 ':')
// 从 startPos 开始搜索, 返回键的值开始位置 (':' 之后的第一个非空白字符)
// 未找到返回 std::string::npos
static size_t findKeyValue(const std::string& s, const std::string& key, size_t startPos = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = s.find(pattern, startPos);
    if (pos == std::string::npos) return std::string::npos;
    pos += pattern.size();
    // 跳过空白和冒号
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != ':') return std::string::npos;
    pos++;
    return skipWs(s, pos);
}

// 从值起始位置提取字符串值 (去掉引号, 处理转义)
// valStart 指向开头的 '"'
// 返回字符串内容, 并通过 outEnd 输出结束位置+1
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
            // 简单转义处理
            char next = s[i + 1];
            switch (next) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                default:   result += next; break;
            }
            i += 2;
        } else if (c == '"') {
            i++;
            break;
        } else {
            result += c;
            i++;
        }
    }
    if (outEnd) *outEnd = i;
    return result;
}

// 从值起始位置提取数字 (整数或浮点)
// valStart 指向数字第一个字符
// 返回数字值, 并通过 outEnd 输出结束位置
static double extractNumber(const std::string& s, size_t valStart, size_t* outEnd) {
    size_t i = valStart;
    // 跳过可能的负号
    if (i < s.size() && s[i] == '-') i++;
    while (i < s.size()) {
        char c = s[i];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            i++;
        } else {
            break;
        }
    }
    std::string numStr = s.substr(valStart, i - valStart);
    if (outEnd) *outEnd = i;
    if (numStr.empty()) return 0.0;
    return std::strtod(numStr.c_str(), nullptr);
}

// 提取值 (自动判断字符串或数字)
// valStart 指向值开始位置
// 如果是字符串, outIsString=true, 返回字符串内容
// 如果是数字, outIsString=false, 返回数字的字符串表示
static std::string extractValue(const std::string& s, size_t valStart, size_t* outEnd) {
    if (valStart >= s.size()) {
        if (outEnd) *outEnd = valStart;
        return "";
    }
    char c = s[valStart];
    if (c == '"') {
        return extractString(s, valStart, outEnd);
    }
    // 数字或 true/false/null
    size_t i = valStart;
    while (i < s.size()) {
        char ch = s[i];
        if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\t' ||
            ch == '\n' || ch == '\r') {
            break;
        }
        i++;
    }
    if (outEnd) *outEnd = i;
    return s.substr(valStart, i - valStart);
}

// ============================================================================
// AhpxReader 实现
// ============================================================================

AhpxReader::AhpxReader()
    : m_fp(nullptr)
    , m_version(0)
    , m_headerSize(0)
    , m_headerCompSize(0)
    , m_blockCount(0) {
}

AhpxReader::~AhpxReader() {
    close();
}

bool AhpxReader::open(const std::string& path) {
    // 防止重复打开 (close() 不清 m_rejectReason, 故在此显式清零)
    close();
    m_rejectReason.clear();
    m_path = path;

    m_fp = ahpx_fopen_utf8(path.c_str(), "rb");
    if (!m_fp) {
        fprintf(stderr, "[aio][ahpx][reader] 无法打开文件: %s\n", path.c_str());
        return false;
    }

    // -------- 读取固定头 (18 字节) --------
    // Magic(4) + Version(2) + HeaderSize(4) + HeaderCompSize(4) + BlockCount(4)
    unsigned char fixedHeader[HEADER_FIXED_SIZE];
    if (std::fread(fixedHeader, 1, HEADER_FIXED_SIZE, m_fp) != HEADER_FIXED_SIZE) {
        fprintf(stderr, "[aio][ahpx][reader] 读取固定头失败 (文件过短): %s\n", path.c_str());
        close();
        return false;
    }

    // 校验 Magic
    if (std::memcmp(fixedHeader, MAGIC, 4) != 0) {
        fprintf(stderr, "[aio][ahpx][reader] Magic 校验失败: %s\n", path.c_str());
        close();
        return false;
    }

    // 解析版本号 (小端序)
    m_version = (uint16_t)fixedHeader[4] | ((uint16_t)fixedHeader[5] << 8);
    if (m_version != VERSION) {
        fprintf(stderr, "[aio][ahpx][reader] 版本不兼容 (文件=%u, 支持=%u): %s\n",
                m_version, VERSION, path.c_str());
        close();
        return false;
    }

    // 解析 HeaderSize, HeaderCompSize, BlockCount (小端序)
    const unsigned char* p = fixedHeader + 6;
    m_headerSize     = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    m_headerCompSize = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    m_blockCount     = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    fprintf(stderr, "[aio][ahpx][reader] 头信息: version=%u headerSize=%u compSize=%u blockCount=%u\n",
            m_version, m_headerSize, m_headerCompSize, m_blockCount);

    // -------- 读取 JSON 头 --------
    uint32_t readSize = (m_headerCompSize > 0) ? m_headerCompSize : m_headerSize;
    if (readSize == 0) {
        fprintf(stderr, "[aio][ahpx][reader] JSON 头大小为 0: %s\n", path.c_str());
        close();
        return false;
    }

    // 16MB 上限, 防止异常文件耗尽内存
    constexpr uint32_t HEADER_READ_MAX = 16 * 1024 * 1024;
    if (readSize > HEADER_READ_MAX) {
        fprintf(stderr, "[aio][ahpx][reader] JSON 头过大 (%u > %u): %s\n",
                readSize, HEADER_READ_MAX, path.c_str());
        close();
        return false;
    }

    std::vector<uint8_t> rawJson(readSize);
    if (std::fread(rawJson.data(), 1, readSize, m_fp) != readSize) {
        fprintf(stderr, "[aio][ahpx][reader] 读取 JSON 头失败: %s\n", path.c_str());
        close();
        return false;
    }

    // 解压 (如果压缩了)
    if (m_headerCompSize > 0) {
        m_headerJson.resize(m_headerSize);
        size_t decompressed = aio::decompressZstd(rawJson.data(), readSize,
                                             &m_headerJson[0], m_headerSize);
        if (decompressed != m_headerSize) {
            fprintf(stderr, "[aio][ahpx][reader] JSON 头解压失败: 期望 %u, 实际 %zu\n",
                    m_headerSize, decompressed);
            close();
            return false;
        }
    } else {
        // 未压缩
        m_headerJson.assign(reinterpret_cast<const char*>(rawJson.data()), readSize);
    }

    // -------- 解析 JSON 头 --------
    if (!parseHeader()) {
        if (!m_rejectReason.empty()) {
            // 显式拒绝 (旧格式/格式违规): 原因由 parseHeader 写入, 此处带上路径复述
            fprintf(stderr, "[aio][ahpx][reader] 拒绝读取: %s (%s)\n",
                    path.c_str(), m_rejectReason.c_str());
        } else {
            fprintf(stderr, "[aio][ahpx][reader] JSON 头解析失败: %s\n", path.c_str());
        }
        close();
        return false;
    }

    fprintf(stderr, "[aio][ahpx][reader] 成功打开: %s (blocks=%zu)\n",
            path.c_str(), m_blocks.size());
    return true;
}

const std::string& AhpxReader::getHeaderJson() const {
    return m_headerJson;
}

const std::string& AhpxReader::getRejectReason() const {
    return m_rejectReason;
}

const std::vector<BlockIndex>& AhpxReader::getBlocks() const {
    return m_blocks;
}

const BlockIndex* AhpxReader::findBlock(const std::string& id) const {
    for (const auto& blk : m_blocks) {
        if (id == blk.id) {
            return &blk;
        }
    }
    return nullptr;
}

bool AhpxReader::parseHeader() {
    m_blocks.clear();

    // 旧格式显式拒绝 (变更 AHPX-WEIGHT-RETIRE-20260920; ASTROCS_DESIGN §2.1 /
    // GAP_AUDIT §9.73 A44): 头 JSON 的 "weight" 字段是已作废权重模式
    // (SCALAR/GRID/PIXEL) 的载体, 本格式不承载权重 ⇒ fail-closed, 禁静默忽略。
    if (!ahpxFault("accept_legacy") && hasJsonKey(m_headerJson, RETIRED_WEIGHT_FIELD)) {
        m_rejectReason = std::string("头 JSON 含已作废的 \"") + RETIRED_WEIGHT_FIELD +
                         "\" 字段 (权重模式已作废, 变更 AHPX-WEIGHT-RETIRE-20260920); "
                         "HiPS 只承载帧级 SNR 与稀疏相对 SNR 比值";
        fprintf(stderr, "[aio][ahpx][reader] 拒绝读取: %s\n", m_rejectReason.c_str());
        return false;
    }

    // 查找 "blocks" 数组
    size_t pos = findKeyValue(m_headerJson, "blocks");
    if (pos == std::string::npos) {
        fprintf(stderr, "[aio][ahpx][reader] JSON 头中未找到 blocks 字段\n");
        // blocks 可能为空, 不算错误
        return true;
    }

    // pos 应指向 '['
    if (pos >= m_headerJson.size() || m_headerJson[pos] != '[') {
        fprintf(stderr, "[aio][ahpx][reader] blocks 不是数组\n");
        return true;
    }

    pos++; // 跳过 '['
    pos = skipWs(m_headerJson, pos);

    while (pos < m_headerJson.size() && m_headerJson[pos] != ']') {
        // 每个元素是一个对象 '{...}'
        if (m_headerJson[pos] != '{') {
            pos++;
            continue;
        }

        // 找到对应的 '}' (简单匹配, 不处理嵌套字符串中的括号)
        int depth = 0;
        size_t objStart = pos;
        size_t objEnd = pos;
        while (objEnd < m_headerJson.size()) {
            char c = m_headerJson[objEnd];
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) {
                    objEnd++;
                    break;
                }
            }
            objEnd++;
        }

        std::string objStr = m_headerJson.substr(objStart, objEnd - objStart);

        // 解析单个 block 对象
        BlockIndex blk;
        std::memset(&blk, 0, sizeof(blk));

        // id
        size_t idPos = findKeyValue(objStr, "id");
        if (idPos != std::string::npos) {
            std::string idVal = extractValue(objStr, idPos, nullptr);
            // 拷贝到 char[32], 截断保护
            std::strncpy(blk.id, idVal.c_str(), sizeof(blk.id) - 1);
            blk.id[sizeof(blk.id) - 1] = '\0';
        }

        // offset
        size_t offPos = findKeyValue(objStr, "offset");
        if (offPos != std::string::npos) {
            blk.offset = (uint64_t)extractNumber(objStr, offPos, nullptr);
        }

        // size
        size_t sizePos = findKeyValue(objStr, "size");
        if (sizePos != std::string::npos) {
            blk.size = (uint64_t)extractNumber(objStr, sizePos, nullptr);
        }

        // codec
        size_t codecPos = findKeyValue(objStr, "codec");
        if (codecPos != std::string::npos) {
            blk.codec = (uint8_t)extractNumber(objStr, codecPos, nullptr);
        }

        // level
        size_t levelPos = findKeyValue(objStr, "level");
        if (levelPos != std::string::npos) {
            blk.level = (int)extractNumber(objStr, levelPos, nullptr);
        }

        m_blocks.push_back(blk);

        pos = objEnd;
        pos = skipWs(m_headerJson, pos);

        // 跳过逗号
        if (pos < m_headerJson.size() && m_headerJson[pos] == ',') {
            pos++;
            pos = skipWs(m_headerJson, pos);
        }
    }

    // 同名数据块同样拒绝 (旧文件必带 "weight" 块; 防手工构造只改块不改键)
    for (const auto& blk : m_blocks) {
        if (!ahpxFault("accept_legacy") && std::strcmp(blk.id, RETIRED_WEIGHT_FIELD) == 0) {
            m_rejectReason = std::string("数据块 \"") + RETIRED_WEIGHT_FIELD +
                             "\" 属于已作废格式 (权重模式已作废, 变更 AHPX-WEIGHT-RETIRE-20260920)";
            fprintf(stderr, "[aio][ahpx][reader] 拒绝读取: %s\n", m_rejectReason.c_str());
            return false;
        }
    }

    return true;
}

bool AhpxReader::getImageInfo(int* width, int* height, int* channels) const {
    return parseImageInfo(width, height, channels);
}

bool AhpxReader::parseImageInfo(int* width, int* height, int* channels) const {
    // 查找 "image" 对象
    size_t imgPos = findKeyValue(m_headerJson, "image");
    if (imgPos == std::string::npos) {
        return false;
    }
    // imgPos 应指向 '{'
    if (imgPos >= m_headerJson.size() || m_headerJson[imgPos] != '{') {
        return false;
    }

    // 提取 image 对象的字符串范围
    int depth = 0;
    size_t objEnd = imgPos;
    while (objEnd < m_headerJson.size()) {
        char c = m_headerJson[objEnd];
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) { objEnd++; break; }
        }
        objEnd++;
    }
    std::string imgObj = m_headerJson.substr(imgPos, objEnd - imgPos);

    bool ok = true;
    if (width) {
        size_t p = findKeyValue(imgObj, "width");
        if (p == std::string::npos) { ok = false; }
        else *width = (int)extractNumber(imgObj, p, nullptr);
    }
    if (height) {
        size_t p = findKeyValue(imgObj, "height");
        if (p == std::string::npos) { ok = false; }
        else *height = (int)extractNumber(imgObj, p, nullptr);
    }
    if (channels) {
        size_t p = findKeyValue(imgObj, "channels");
        if (p == std::string::npos) {
            // channels 可能可选, 默认 1
            *channels = 1;
        } else {
            *channels = (int)extractNumber(imgObj, p, nullptr);
        }
    }
    return ok;
}

bool AhpxReader::readRaw(uint64_t offset, uint64_t size, void* dst) {
    if (!m_fp) return false;
    // 定位到偏移
    if (std::fseek(m_fp, (long)offset, SEEK_SET) != 0) {
        fprintf(stderr, "[aio][ahpx][reader] fseek 失败 (offset=%llu)\n",
                (unsigned long long)offset);
        return false;
    }
    // 读取数据
    size_t bytesRead = std::fread(dst, 1, size, m_fp);
    if (bytesRead != size) {
        fprintf(stderr, "[aio][ahpx][reader] fread 不完整 (期望 %llu, 实际 %zu)\n",
                (unsigned long long)size, bytesRead);
        return false;
    }
    return true;
}

std::vector<uint8_t> AhpxReader::readBlockByIndex(const BlockIndex* blk) {
    std::vector<uint8_t> result;
    if (!blk) return result;

    // 读取压缩数据
    std::vector<uint8_t> compData(blk->size);
    if (!readRaw(blk->offset, blk->size, compData.data())) {
        return result;
    }

    // 根据 codec 解压
    if (blk->codec == (uint8_t)Codec::NONE) {
        // 不压缩
        result = std::move(compData);
    } else if (blk->codec == (uint8_t)Codec::ZSTD) {
        // ZSTD 解压: 高压缩率数据可能需要远大于压缩前 8 倍的缓冲区
        // 循环扩容直到解压成功或达到尝试上限
        size_t estSize = blk->size * 8;
        if (estSize < 1024) estSize = 1024;
        size_t decompSize = 0;
        for (int attempt = 0; attempt < 8; attempt++) {
            result.resize(estSize);
            decompSize = aio::decompressZstd(compData.data(), compData.size(),
                                        result.data(), estSize);
            if (decompSize > 0) break;
            estSize *= 4;  // 扩大 4 倍重试
        }
        if (decompSize == 0) {
            fprintf(stderr, "[aio][ahpx][reader] 块 '%s' ZSTD 解压失败\n", blk->id);
            result.clear();
            return result;
        }
        result.resize(decompSize);
    } else if (blk->codec == (uint8_t)Codec::LZ4) {
        // LZ4 解压: 同样循环扩容
        size_t estSize = blk->size * 8;
        if (estSize < 1024) estSize = 1024;
        size_t decompSize = 0;
        for (int attempt = 0; attempt < 8; attempt++) {
            result.resize(estSize);
            decompSize = aio::decompressLz4(compData.data(), compData.size(),
                                       result.data(), estSize);
            if (decompSize > 0) break;
            estSize *= 4;
        }
        if (decompSize == 0) {
            fprintf(stderr, "[aio][ahpx][reader] 块 '%s' LZ4 解压失败\n", blk->id);
            result.clear();
            return result;
        }
        result.resize(decompSize);
    } else {
        fprintf(stderr, "[aio][ahpx][reader] 块 '%s' 未知 codec=%u\n", blk->id, blk->codec);
        result.clear();
    }

    return result;
}

std::vector<uint8_t> AhpxReader::readBlock(const std::string& id) {
    const BlockIndex* blk = findBlock(id);
    if (!blk) {
        fprintf(stderr, "[aio][ahpx][reader] 块 '%s' 未找到\n", id.c_str());
        return std::vector<uint8_t>();
    }
    return readBlockByIndex(blk);
}

std::vector<float> AhpxReader::readPixels() {
    std::vector<float> result;

    // 获取图像几何信息
    int w, h, c;
    if (!getImageInfo(&w, &h, &c)) {
        fprintf(stderr, "[aio][ahpx][reader] readPixels: 无法获取图像几何信息\n");
        return result;
    }
    if (w <= 0 || h <= 0 || c <= 0) {
        fprintf(stderr, "[aio][ahpx][reader] readPixels: 无效几何 (w=%d h=%d c=%d)\n", w, h, c);
        return result;
    }

    // 读取 pixel 块
    std::vector<uint8_t> rawData = readBlock("pixel");
    if (rawData.empty()) {
        fprintf(stderr, "[aio][ahpx][reader] readPixels: pixel 块为空\n");
        return result;
    }

    // 验证大小
    size_t expectedSize = (size_t)w * h * c * sizeof(float);
    if (rawData.size() != expectedSize) {
        fprintf(stderr, "[aio][ahpx][reader] readPixels: 大小不匹配 (期望 %zu, 实际 %zu)\n",
                expectedSize, rawData.size());
        return result;
    }

    // 转换为 float 数组 (直接拷贝, 假定主机序一致)
    size_t floatCount = rawData.size() / sizeof(float);
    result.resize(floatCount);
    std::memcpy(result.data(), rawData.data(), rawData.size());

    return result;
}

std::vector<float> AhpxReader::readSnr() {
    std::vector<float> result;

    // 读取 snr 块
    std::vector<uint8_t> rawData = readBlock("snr");
    if (rawData.empty()) {
        fprintf(stderr, "[aio][ahpx][reader] readSnr: snr 块为空\n");
        return result;
    }

    // 转换为 float 数组
    size_t floatCount = rawData.size() / sizeof(float);
    result.resize(floatCount);
    std::memcpy(result.data(), rawData.data(), rawData.size());

    return result;
}

void AhpxReader::close() {
    if (m_fp) {
        std::fclose(m_fp);
        m_fp = nullptr;
    }
    m_path.clear();
    m_headerJson.clear();
    m_blocks.clear();
    m_version = 0;
    m_headerSize = 0;
    m_headerCompSize = 0;
    m_blockCount = 0;
}

} // namespace aio::ahpx
