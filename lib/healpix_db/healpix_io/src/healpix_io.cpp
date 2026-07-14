// healpix_io.cpp - HEALPix 存储格式 (.hiss / .hcsd) 读写实现
// 功能: 实现 .hiss 单帧存储和 .hcsd 天球数据库的读写 API
// 用途: 替代旧版 .ahpx/.ahps 格式，支持稀疏存储和子叶块索引按需加载
// 依赖: libzstd (JSON 头压缩), 纯 C++17 标准库

#include "healpix_io.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#include <zstd.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// 常量定义
// ============================================================================

static const char HISS_MAGIC[4] = {'H', 'I', 'S', 'S'};
static const char HCSD_MAGIC[4] = {'H', 'C', 'S', 'D'};

// 子叶块索引常量
static const uint64_t N_LEAVES = 49152;       // 12 * 64^2
static const uint64_t LEAF_INDEX_SIZE = 49152 * 24; // 子叶索引表总字节

// zstd 压缩级别
static const int ZSTD_LEVEL = 5;

// 错误码
static const int HIO_OK              = 0;
static const int HIO_ERR_PARAM       = -1;
static const int HIO_ERR_FILE        = -2;
static const int HIO_ERR_MAGIC       = -3;
static const int HIO_ERR_ZSTD        = -4;
static const int HIO_ERR_JSON        = -5;
static const int HIO_ERR_MEM         = -6;
static const int HIO_ERR_BOUNDS      = -7;

// ============================================================================
// UTF-8 路径文件打开辅助 (Windows 下支持中文路径)
// ============================================================================
static FILE* hio_fopen_utf8(const char* path, const char* mode) {
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
// zstd 压缩/解压辅助
// ============================================================================

// 压缩数据, 返回压缩后字节, 失败返回 0
static size_t hio_zstd_compress(const void* src, size_t srcSize,
                                std::vector<uint8_t>& out, int level) {
    size_t bound = ZSTD_compressBound(srcSize);
    out.resize(bound);
    size_t compSize = ZSTD_compress(out.data(), bound, src, srcSize, level);
    if (ZSTD_isError(compSize)) {
        fprintf(stderr, "[hio] ZSTD_compress 失败: %s\n",
                ZSTD_getErrorName(compSize));
        return 0;
    }
    out.resize(compSize);
    return compSize;
}

// 解压数据, 返回解压后字节, 失败返回 0
static size_t hio_zstd_decompress(const void* src, size_t srcSize,
                                  void* dst, size_t dstCapacity) {
    size_t outSize = ZSTD_decompress(dst, dstCapacity, src, srcSize);
    if (ZSTD_isError(outSize)) {
        fprintf(stderr, "[hio] ZSTD_decompress 失败: %s\n",
                ZSTD_getErrorName(outSize));
        return 0;
    }
    return outSize;
}

// ============================================================================
// JSON 辅助函数 (简单字符串操作, 无外部 JSON 库依赖)
// ============================================================================

// 构建 .hiss/.hcsd JSON 头: 合并 nside/nested/n_pix 与调用者的 meta_json
// meta_json 应为 JSON 对象字符串 (如 {"filter":"Lum",...}), 不含 nside/nested/n_pix
static std::string hio_build_json(uint32_t nside, int nested, uint64_t n_pix,
                                  const char* meta_json) {
    std::string result;
    result.reserve(256);
    result += "{\"nside\":";
    result += std::to_string(nside);
    result += ",\"nested\":";
    result += (nested ? "true" : "false");
    result += ",\"n_pix\":";
    result += std::to_string(n_pix);

    if (meta_json && meta_json[0] != '\0') {
        std::string meta(meta_json);
        // 找到第一个 '{' 之后的内容, 去掉末尾的 '}'
        size_t start = meta.find('{');
        if (start != std::string::npos) {
            start++; // 跳过 '{'
            // 找到最后一个 '}'
            size_t end = meta.rfind('}');
            if (end != std::string::npos && end > start) {
                std::string inner = meta.substr(start, end - start);
                // 去除首尾空白
                size_t is = inner.find_first_not_of(" \t\n\r");
                size_t ie = inner.find_last_not_of(" \t\n\r");
                if (is != std::string::npos && ie != std::string::npos && is <= ie) {
                    result += ",";
                    result += inner.substr(is, ie - is + 1);
                }
            }
        }
    }

    result += "}";
    return result;
}

// 从 JSON 字符串中解析 uint32 值
static bool hio_parse_json_uint32(const std::string& json, const char* key, uint32_t& val) {
    std::string search = std::string("\"") + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return false;
    pos++;
    // 跳过空白
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] < '0' || json[pos] > '9') return false;
    val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (uint32_t)(json[pos] - '0');
        pos++;
    }
    return true;
}

// 从 JSON 字符串中解析 uint64 值
static bool hio_parse_json_uint64(const std::string& json, const char* key, uint64_t& val) {
    std::string search = std::string("\"") + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] < '0' || json[pos] > '9') return false;
    val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (uint64_t)(json[pos] - '0');
        pos++;
    }
    return true;
}

// 从 JSON 字符串中解析 bool 值
static bool hio_parse_json_bool(const std::string& json, const char* key, int& val) {
    std::string search = std::string("\"") + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos + 4 <= json.size() && json.compare(pos, 4, "true") == 0) {
        val = 1;
        return true;
    }
    if (pos + 5 <= json.size() && json.compare(pos, 5, "false") == 0) {
        val = 0;
        return true;
    }
    return false;
}

// ============================================================================
// 子叶块索引辅助
// ============================================================================

// 计算 nside 到 nside=64 的位移量: shift = 2 * log2(nside / 64)
// 对于 nside=8192: shift = 2 * 7 = 14
static int hio_compute_leaf_shift(uint32_t nside) {
    int shift = 0;
    uint32_t temp = nside;
    while (temp > 64) {
        shift += 2;
        temp >>= 1;
    }
    return shift;
}

// 子叶索引项 (24 字节, 小端序)
#pragma pack(push, 1)
struct LeafIndexEntry {
    uint64_t leaf_ipix;     // 子叶在 nside=64 下的 ipix
    uint64_t data_offset;   // 子叶数据在 ipix 数组区内的字节偏移
    uint64_t data_length;   // 子叶包含的像素数量
};
#pragma pack(pop)
static_assert(sizeof(LeafIndexEntry) == 24, "LeafIndexEntry must be 24 bytes");

// ============================================================================
// .hiss 写入实现
// ============================================================================

HIO_API int hiss_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const char* meta_json) {
    // 参数校验
    if (!path || (n_pix > 0 && (!ipix || !pixel))) {
        fprintf(stderr, "[hio] hiss_write: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    fprintf(stderr, "[hio] hiss_write: path=%s nside=%u nested=%d n_pix=%llu\n",
            path, nside, nested, (unsigned long long)n_pix);

    // 1. 构建 JSON 头
    std::string jsonStr = hio_build_json(nside, nested, n_pix, meta_json);
    uint32_t jsonLen = (uint32_t)jsonStr.size();

    // 2. 用 zstd 压缩 JSON 头
    std::vector<uint8_t> compJson;
    size_t compJsonLen = hio_zstd_compress(jsonStr.data(), jsonLen, compJson, ZSTD_LEVEL);
    if (compJsonLen == 0) {
        fprintf(stderr, "[hio] hiss_write: JSON 头压缩失败\n");
        return HIO_ERR_ZSTD;
    }
    uint32_t compLen = (uint32_t)compJsonLen;

    fprintf(stderr, "[hio] hiss_write: JSON 头 %u -> %u 字节 (zstd level=%d)\n",
            jsonLen, compLen, ZSTD_LEVEL);

    // 3. 打开文件
    FILE* fp = hio_fopen_utf8(path, "wb");
    if (!fp) {
        fprintf(stderr, "[hio] hiss_write: 无法创建文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 4. 写入 Magic (4 字节)
    if (std::fwrite(HISS_MAGIC, 1, 4, fp) != 4) {
        fprintf(stderr, "[hio] hiss_write: 写入 Magic 失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 5. 写入 uncompressed_len (uint32) + compressed_len (uint32)
    uint32_t hdr[2] = {jsonLen, compLen};
    if (std::fwrite(hdr, 4, 2, fp) != 2) {
        fprintf(stderr, "[hio] hiss_write: 写入 JSON 长度失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6. 写入压缩后的 JSON 头
    if (std::fwrite(compJson.data(), 1, compLen, fp) != compLen) {
        fprintf(stderr, "[hio] hiss_write: 写入压缩 JSON 失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 7. 写入 ipix 数组 (n_pix * 8 字节)
    if (n_pix > 0) {
        size_t ipixBytes = (size_t)n_pix * sizeof(uint64_t);
        if (std::fwrite(ipix, 1, ipixBytes, fp) != ipixBytes) {
            fprintf(stderr, "[hio] hiss_write: 写入 ipix 数组失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }

        // 8. 写入 pixel 数组 (n_pix * 4 字节)
        size_t pixelBytes = (size_t)n_pix * sizeof(float);
        if (std::fwrite(pixel, 1, pixelBytes, fp) != pixelBytes) {
            fprintf(stderr, "[hio] hiss_write: 写入 pixel 数组失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::fclose(fp);

    fprintf(stderr, "[hio] hiss_write: 写入完成: %s\n", path);
    return HIO_OK;
}

// ============================================================================
// .hiss 读取实现
// ============================================================================

HIO_API int hiss_read(const char* path, uint32_t* nside, int* nested,
                      uint64_t* n_pix, uint64_t** ipix,
                      float** pixel, char** meta_json) {
    if (!path || !nside || !nested || !n_pix || !ipix || !pixel || !meta_json) {
        fprintf(stderr, "[hio] hiss_read: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    // 初始化输出
    *nside = 0;
    *nested = 0;
    *n_pix = 0;
    *ipix = nullptr;
    *pixel = nullptr;
    *meta_json = nullptr;

    FILE* fp = hio_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "[hio] hiss_read: 无法打开文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 1. 读取并验证 Magic
    char magic[4];
    if (std::fread(magic, 1, 4, fp) != 4) {
        fprintf(stderr, "[hio] hiss_read: 读取 Magic 失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::memcmp(magic, HISS_MAGIC, 4) != 0) {
        fprintf(stderr, "[hio] hiss_read: Magic 不匹配 (期望 HISS)\n");
        std::fclose(fp);
        return HIO_ERR_MAGIC;
    }

    // 2. 读取 JSON 头长度
    uint32_t hdr[2];
    if (std::fread(hdr, 4, 2, fp) != 2) {
        fprintf(stderr, "[hio] hiss_read: 读取 JSON 长度失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    uint32_t uncompLen = hdr[0];
    uint32_t compLen = hdr[1];

    // 3. 读取并解压 JSON 头
    std::vector<uint8_t> compBuf(compLen);
    if (compLen > 0) {
        if (std::fread(compBuf.data(), 1, compLen, fp) != compLen) {
            fprintf(stderr, "[hio] hiss_read: 读取压缩 JSON 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::string jsonStr;
    jsonStr.resize(uncompLen);
    if (uncompLen > 0) {
        size_t decompSize = hio_zstd_decompress(compBuf.data(), compLen,
                                                &jsonStr[0], uncompLen);
        if (decompSize == 0 || decompSize != uncompLen) {
            fprintf(stderr, "[hio] hiss_read: JSON 解压失败 (期望 %u, 实际 %zu)\n",
                    uncompLen, decompSize);
            std::fclose(fp);
            return HIO_ERR_ZSTD;
        }
    }

    // 4. 解析 JSON 头
    uint32_t jsonNside = 0;
    int jsonNested = 0;
    uint64_t jsonNPix = 0;
    if (!hio_parse_json_uint32(jsonStr, "nside", jsonNside) ||
        !hio_parse_json_bool(jsonStr, "nested", jsonNested) ||
        !hio_parse_json_uint64(jsonStr, "n_pix", jsonNPix)) {
        fprintf(stderr, "[hio] hiss_read: JSON 解析失败 (缺少必填字段)\n");
        std::fclose(fp);
        return HIO_ERR_JSON;
    }

    *nside = jsonNside;
    *nested = jsonNested;
    *n_pix = jsonNPix;

    // 分配 meta_json (包含完整 JSON 字符串, 含 null 终止)
    *meta_json = (char*)std::malloc(jsonStr.size() + 1);
    if (!*meta_json) {
        fprintf(stderr, "[hio] hiss_read: 分配 meta_json 内存失败\n");
        std::fclose(fp);
        return HIO_ERR_MEM;
    }
    std::memcpy(*meta_json, jsonStr.c_str(), jsonStr.size() + 1);

    fprintf(stderr, "[hio] hiss_read: nside=%u nested=%d n_pix=%llu\n",
            *nside, *nested, (unsigned long long)*n_pix);

    // 5. 读取 ipix 数组
    if (*n_pix > 0) {
        *ipix = (uint64_t*)std::malloc((size_t)(*n_pix) * sizeof(uint64_t));
        if (!*ipix) {
            fprintf(stderr, "[hio] hiss_read: 分配 ipix 内存失败\n");
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_MEM;
        }
        size_t ipixBytes = (size_t)(*n_pix) * sizeof(uint64_t);
        if (std::fread(*ipix, 1, ipixBytes, fp) != ipixBytes) {
            fprintf(stderr, "[hio] hiss_read: 读取 ipix 数组失败\n");
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_FILE;
        }

        // 6. 读取 pixel 数组
        *pixel = (float*)std::malloc((size_t)(*n_pix) * sizeof(float));
        if (!*pixel) {
            fprintf(stderr, "[hio] hiss_read: 分配 pixel 内存失败\n");
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_MEM;
        }
        size_t pixelBytes = (size_t)(*n_pix) * sizeof(float);
        if (std::fread(*pixel, 1, pixelBytes, fp) != pixelBytes) {
            fprintf(stderr, "[hio] hiss_read: 读取 pixel 数组失败\n");
            std::free(*pixel);
            *pixel = nullptr;
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hiss_read: 读取完成: %s\n", path);
    return HIO_OK;
}

// ============================================================================
// .hcsd 写入实现 (含子叶块索引构建)
// ============================================================================

HIO_API int hcsd_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const char* meta_json) {
    if (!path || (n_pix > 0 && (!ipix || !pixel))) {
        fprintf(stderr, "[hio] hcsd_write: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    fprintf(stderr, "[hio] hcsd_write: path=%s nside=%u nested=%d n_pix=%llu\n",
            path, nside, nested, (unsigned long long)n_pix);

    // 1. 构建 JSON 头
    std::string jsonStr = hio_build_json(nside, nested, n_pix, meta_json);
    uint32_t jsonLen = (uint32_t)jsonStr.size();

    // 2. 压缩 JSON 头
    std::vector<uint8_t> compJson;
    size_t compJsonLen = hio_zstd_compress(jsonStr.data(), jsonLen, compJson, ZSTD_LEVEL);
    if (compJsonLen == 0) {
        fprintf(stderr, "[hio] hcsd_write: JSON 头压缩失败\n");
        return HIO_ERR_ZSTD;
    }
    uint32_t compLen = (uint32_t)compJsonLen;

    // 3. 计算子叶位移量
    int shift = hio_compute_leaf_shift(nside);
    fprintf(stderr, "[hio] hcsd_write: 子叶位移 shift=%d (nside=%u)\n", shift, nside);

    // 4. 构建排序后的 ipix + pixel (按 leaf_ipix 升序, 子叶内按 ipix 升序)
    std::vector<uint64_t> sortedIpix;
    std::vector<float> sortedPixel;
    std::vector<LeafIndexEntry> leafIndex;

    if (n_pix > 0) {
        // 创建排序索引
        std::vector<size_t> order((size_t)n_pix);
        for (size_t i = 0; i < n_pix; i++) order[i] = i;

        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            uint64_t la = ipix[a] >> shift;
            uint64_t lb = ipix[b] >> shift;
            if (la != lb) return la < lb;
            return ipix[a] < ipix[b];
        });

        // 复制排序后的数据
        sortedIpix.resize((size_t)n_pix);
        sortedPixel.resize((size_t)n_pix);
        for (size_t i = 0; i < n_pix; i++) {
            sortedIpix[i] = ipix[order[i]];
            sortedPixel[i] = pixel[order[i]];
        }

        // 5. 构建子叶索引表 (49152 项, 每项 24 字节)
        leafIndex.resize(N_LEAVES);
        // 初始化: 每项 leaf_ipix = 索引值, data_offset = 0, data_length = 0
        for (uint64_t i = 0; i < N_LEAVES; i++) {
            leafIndex[i].leaf_ipix = i;
            leafIndex[i].data_offset = 0;
            leafIndex[i].data_length = 0;
        }

        // 填充非空子叶
        uint64_t curLeaf = sortedIpix[0] >> shift;
        uint64_t curOffset = 0;   // 字节偏移 (相对 ipix 数组起始)
        uint64_t curCount = 0;

        for (size_t i = 0; i < n_pix; i++) {
            uint64_t leaf = sortedIpix[i] >> shift;
            if (leaf != curLeaf) {
                // 写入前一个子叶
                if (curLeaf < N_LEAVES && curCount > 0) {
                    leafIndex[curLeaf].leaf_ipix = curLeaf;
                    leafIndex[curLeaf].data_offset = curOffset;
                    leafIndex[curLeaf].data_length = curCount;
                }
                curLeaf = leaf;
                curOffset = (uint64_t)i * sizeof(uint64_t);
                curCount = 1;
            } else {
                curCount++;
            }
        }
        // 写入最后一个子叶
        if (curCount > 0 && curLeaf < N_LEAVES) {
            leafIndex[curLeaf].leaf_ipix = curLeaf;
            leafIndex[curLeaf].data_offset = curOffset;
            leafIndex[curLeaf].data_length = curCount;
        }

        // 统计非空子叶数
        uint64_t nonEmpty = 0;
        for (uint64_t i = 0; i < N_LEAVES; i++) {
            if (leafIndex[i].data_length > 0) nonEmpty++;
        }
        fprintf(stderr, "[hio] hcsd_write: 非空子叶 %llu / %llu\n",
                (unsigned long long)nonEmpty, (unsigned long long)N_LEAVES);
    } else {
        // n_pix = 0: 初始化空索引表
        leafIndex.resize(N_LEAVES);
        for (uint64_t i = 0; i < N_LEAVES; i++) {
            leafIndex[i].leaf_ipix = i;
            leafIndex[i].data_offset = 0;
            leafIndex[i].data_length = 0;
        }
    }

    // 6. 打开文件并写入
    FILE* fp = hio_fopen_utf8(path, "wb");
    if (!fp) {
        fprintf(stderr, "[hio] hcsd_write: 无法创建文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 6.1 写入 Magic
    if (std::fwrite(HCSD_MAGIC, 1, 4, fp) != 4) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6.2 写入 JSON 头长度 + 压缩 JSON 头
    uint32_t hdr[2] = {jsonLen, compLen};
    if (std::fwrite(hdr, 4, 2, fp) != 2) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::fwrite(compJson.data(), 1, compLen, fp) != compLen) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6.3 写入子叶块索引表 (49152 * 24 字节)
    if (std::fwrite(leafIndex.data(), sizeof(LeafIndexEntry), N_LEAVES, fp) != N_LEAVES) {
        fprintf(stderr, "[hio] hcsd_write: 写入子叶索引表失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6.4 写入排序后的 ipix 数组
    if (n_pix > 0) {
        size_t ipixBytes = (size_t)n_pix * sizeof(uint64_t);
        if (std::fwrite(sortedIpix.data(), 1, ipixBytes, fp) != ipixBytes) {
            fprintf(stderr, "[hio] hcsd_write: 写入 ipix 数组失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }

        // 6.5 写入 pixel 数组
        size_t pixelBytes = (size_t)n_pix * sizeof(float);
        if (std::fwrite(sortedPixel.data(), 1, pixelBytes, fp) != pixelBytes) {
            fprintf(stderr, "[hio] hcsd_write: 写入 pixel 数组失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hcsd_write: 写入完成: %s\n", path);
    return HIO_OK;
}

// ============================================================================
// .hcsd 全量读取实现
// ============================================================================

HIO_API int hcsd_read(const char* path, uint32_t* nside, int* nested,
                      uint64_t* n_pix, uint64_t** ipix,
                      float** pixel, char** meta_json) {
    if (!path || !nside || !nested || !n_pix || !ipix || !pixel || !meta_json) {
        fprintf(stderr, "[hio] hcsd_read: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    *nside = 0;
    *nested = 0;
    *n_pix = 0;
    *ipix = nullptr;
    *pixel = nullptr;
    *meta_json = nullptr;

    FILE* fp = hio_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "[hio] hcsd_read: 无法打开文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 1. 读取并验证 Magic
    char magic[4];
    if (std::fread(magic, 1, 4, fp) != 4) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::memcmp(magic, HCSD_MAGIC, 4) != 0) {
        fprintf(stderr, "[hio] hcsd_read: Magic 不匹配 (期望 HCSD)\n");
        std::fclose(fp);
        return HIO_ERR_MAGIC;
    }

    // 2. 读取 JSON 头长度
    uint32_t hdr[2];
    if (std::fread(hdr, 4, 2, fp) != 2) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    uint32_t uncompLen = hdr[0];
    uint32_t compLen = hdr[1];

    // 3. 读取并解压 JSON 头
    std::vector<uint8_t> compBuf(compLen);
    if (compLen > 0) {
        if (std::fread(compBuf.data(), 1, compLen, fp) != compLen) {
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::string jsonStr;
    jsonStr.resize(uncompLen);
    if (uncompLen > 0) {
        size_t decompSize = hio_zstd_decompress(compBuf.data(), compLen,
                                                &jsonStr[0], uncompLen);
        if (decompSize == 0 || decompSize != uncompLen) {
            fprintf(stderr, "[hio] hcsd_read: JSON 解压失败\n");
            std::fclose(fp);
            return HIO_ERR_ZSTD;
        }
    }

    // 4. 解析 JSON 头
    uint32_t jsonNside = 0;
    int jsonNested = 0;
    uint64_t jsonNPix = 0;
    if (!hio_parse_json_uint32(jsonStr, "nside", jsonNside) ||
        !hio_parse_json_bool(jsonStr, "nested", jsonNested) ||
        !hio_parse_json_uint64(jsonStr, "n_pix", jsonNPix)) {
        fprintf(stderr, "[hio] hcsd_read: JSON 解析失败\n");
        std::fclose(fp);
        return HIO_ERR_JSON;
    }

    *nside = jsonNside;
    *nested = jsonNested;
    *n_pix = jsonNPix;

    *meta_json = (char*)std::malloc(jsonStr.size() + 1);
    if (!*meta_json) {
        std::fclose(fp);
        return HIO_ERR_MEM;
    }
    std::memcpy(*meta_json, jsonStr.c_str(), jsonStr.size() + 1);

    fprintf(stderr, "[hio] hcsd_read: nside=%u nested=%d n_pix=%llu\n",
            *nside, *nested, (unsigned long long)*n_pix);

    // 5. 跳过子叶块索引表 (49152 * 24 字节)
    if (std::fseek(fp, (long)LEAF_INDEX_SIZE, SEEK_CUR) != 0) {
        fprintf(stderr, "[hio] hcsd_read: 跳过子叶索引表失败\n");
        std::free(*meta_json);
        *meta_json = nullptr;
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6. 读取 ipix 数组
    if (*n_pix > 0) {
        *ipix = (uint64_t*)std::malloc((size_t)(*n_pix) * sizeof(uint64_t));
        if (!*ipix) {
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_MEM;
        }
        size_t ipixBytes = (size_t)(*n_pix) * sizeof(uint64_t);
        if (std::fread(*ipix, 1, ipixBytes, fp) != ipixBytes) {
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_FILE;
        }

        // 7. 读取 pixel 数组
        *pixel = (float*)std::malloc((size_t)(*n_pix) * sizeof(float));
        if (!*pixel) {
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_MEM;
        }
        size_t pixelBytes = (size_t)(*n_pix) * sizeof(float);
        if (std::fread(*pixel, 1, pixelBytes, fp) != pixelBytes) {
            std::free(*pixel);
            *pixel = nullptr;
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hcsd_read: 读取完成: %s\n", path);
    return HIO_OK;
}

// ============================================================================
// .hcsd 按子叶读取实现
// ============================================================================

HIO_API int hcsd_read_leaf(const char* path, uint64_t leaf_ipix_at_nside64,
                           uint64_t* n_pix, uint64_t** ipix, float** pixel) {
    if (!path || !n_pix || !ipix || !pixel) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    *n_pix = 0;
    *ipix = nullptr;
    *pixel = nullptr;

    // 校验 leaf_ipix 范围
    if (leaf_ipix_at_nside64 >= N_LEAVES) {
        fprintf(stderr, "[hio] hcsd_read_leaf: leaf_ipix %llu 超出范围 [0, %llu)\n",
                (unsigned long long)leaf_ipix_at_nside64,
                (unsigned long long)N_LEAVES);
        return HIO_ERR_BOUNDS;
    }

    FILE* fp = hio_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 无法打开文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 1. 读取并验证 Magic
    char magic[4];
    if (std::fread(magic, 1, 4, fp) != 4) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::memcmp(magic, HCSD_MAGIC, 4) != 0) {
        fprintf(stderr, "[hio] hcsd_read_leaf: Magic 不匹配 (期望 HCSD)\n");
        std::fclose(fp);
        return HIO_ERR_MAGIC;
    }

    // 2. 读取 JSON 头长度
    uint32_t hdr[2];
    if (std::fread(hdr, 4, 2, fp) != 2) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    uint32_t uncompLen = hdr[0];
    uint32_t compLen = hdr[1];

    // 3. 读取并解压 JSON 头 (获取 n_pix)
    std::vector<uint8_t> compBuf(compLen);
    if (compLen > 0) {
        if (std::fread(compBuf.data(), 1, compLen, fp) != compLen) {
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::string jsonStr;
    jsonStr.resize(uncompLen);
    if (uncompLen > 0) {
        size_t decompSize = hio_zstd_decompress(compBuf.data(), compLen,
                                                &jsonStr[0], uncompLen);
        if (decompSize == 0 || decompSize != uncompLen) {
            fprintf(stderr, "[hio] hcsd_read_leaf: JSON 解压失败\n");
            std::fclose(fp);
            return HIO_ERR_ZSTD;
        }
    }

    uint64_t totalNPix = 0;
    if (!hio_parse_json_uint64(jsonStr, "n_pix", totalNPix)) {
        fprintf(stderr, "[hio] hcsd_read_leaf: JSON 解析 n_pix 失败\n");
        std::fclose(fp);
        return HIO_ERR_JSON;
    }

    // 4. 读取子叶索引表中第 leaf_ipix_at_nside64 项
    // 索引表紧跟在 JSON 头之后, 每项 24 字节
    long indexEntryPos = (long)(12 + compLen + leaf_ipix_at_nside64 * sizeof(LeafIndexEntry));
    if (std::fseek(fp, indexEntryPos, SEEK_SET) != 0) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 定位索引项失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    LeafIndexEntry entry;
    if (std::fread(&entry, sizeof(LeafIndexEntry), 1, fp) != 1) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 读取索引项失败\n");
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    fprintf(stderr, "[hio] hcsd_read_leaf: leaf=%llu offset=%llu length=%llu\n",
            (unsigned long long)entry.leaf_ipix,
            (unsigned long long)entry.data_offset,
            (unsigned long long)entry.data_length);

    // 5. 如果子叶为空, 返回空数据
    if (entry.data_length == 0) {
        *n_pix = 0;
        *ipix = nullptr;
        *pixel = nullptr;
        std::fclose(fp);
        fprintf(stderr, "[hio] hcsd_read_leaf: 子叶 %llu 为空\n",
                (unsigned long long)leaf_ipix_at_nside64);
        return HIO_OK;
    }

    // 6. 计算数据区位置
    // ipix 数组起始 = 12 + compLen + LEAF_INDEX_SIZE
    // pixel 数组起始 = ipix 数组起始 + totalNPix * 8
    long ipixArrayStart = (long)(12 + compLen + LEAF_INDEX_SIZE);
    long pixelArrayStart = ipixArrayStart + (long)(totalNPix * sizeof(uint64_t));

    // 子叶 ipix 数据位置
    long leafIpixPos = ipixArrayStart + (long)entry.data_offset;
    // 子叶 pixel 数据位置: data_offset 是 ipix 数组中的字节偏移, 对应 pixel 偏移 = data_offset / 8 * 4
    long leafPixelPos = pixelArrayStart + (long)(entry.data_offset / sizeof(uint64_t) * sizeof(float));

    // 7. 读取子叶 ipix 数据
    *n_pix = entry.data_length;
    *ipix = (uint64_t*)std::malloc((size_t)entry.data_length * sizeof(uint64_t));
    if (!*ipix) {
        std::fclose(fp);
        return HIO_ERR_MEM;
    }

    if (std::fseek(fp, leafIpixPos, SEEK_SET) != 0) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 定位 ipix 数据失败\n");
        std::free(*ipix);
        *ipix = nullptr;
        *n_pix = 0;
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    size_t ipixBytes = (size_t)entry.data_length * sizeof(uint64_t);
    if (std::fread(*ipix, 1, ipixBytes, fp) != ipixBytes) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 读取 ipix 数据失败\n");
        std::free(*ipix);
        *ipix = nullptr;
        *n_pix = 0;
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 8. 读取子叶 pixel 数据
    *pixel = (float*)std::malloc((size_t)entry.data_length * sizeof(float));
    if (!*pixel) {
        std::free(*ipix);
        *ipix = nullptr;
        *n_pix = 0;
        std::fclose(fp);
        return HIO_ERR_MEM;
    }

    if (std::fseek(fp, leafPixelPos, SEEK_SET) != 0) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 定位 pixel 数据失败\n");
        std::free(*ipix);
        *ipix = nullptr;
        std::free(*pixel);
        *pixel = nullptr;
        *n_pix = 0;
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    size_t pixelBytes = (size_t)entry.data_length * sizeof(float);
    if (std::fread(*pixel, 1, pixelBytes, fp) != pixelBytes) {
        fprintf(stderr, "[hio] hcsd_read_leaf: 读取 pixel 数据失败\n");
        std::free(*ipix);
        *ipix = nullptr;
        std::free(*pixel);
        *pixel = nullptr;
        *n_pix = 0;
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hcsd_read_leaf: 读取子叶 %llu 完成, n_pix=%llu\n",
            (unsigned long long)leaf_ipix_at_nside64,
            (unsigned long long)*n_pix);
    return HIO_OK;
}

// ============================================================================
// 内存释放
// ============================================================================

HIO_API void hio_free(void* ptr) {
    if (ptr) std::free(ptr);
}
