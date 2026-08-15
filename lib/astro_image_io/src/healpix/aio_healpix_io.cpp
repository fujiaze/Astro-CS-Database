// aio_healpix_io.cpp - HEALPix 存储格式 (.hiss / .hcsd) 读写实现
// 功能: 实现 .hiss 单帧存储和 .hcsd 天球数据库的读写 API
// 用途: 替代旧版 .ahpx/.ahps 格式，支持稀疏存储和子叶块索引按需加载
// 依赖: libzstd (JSON 头压缩), 纯 C++17 标准库
//
// 2026-07-16 按 architecture-refactor spec G1 从 lib/healpix_db/healpix_io/ 合并入 aio
// 原 healpix_io.cpp 已归档到 lib/healpix_db/healpix_io/archive/
// API 前缀从 hiss_/hcsd_/hio_ 改为 aio_hiss_/aio_hcsd_/aio_hio_

#include "aio_healpix_io.h"

// WP-E 步骤8: aio_hiss_write/read 改造成新 HissWriter/HissReader 后端
#include "../../include/hiss_format.h"
#include "../hiss_tile_model.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>
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

// 构建 .hiss/.hcsd JSON 头: 合并 nside/nested/n_pix/has_snr/snr_format 与调用者的 meta_json
// meta_json 应为 JSON 对象字符串 (如 {"filter":"Lum",...}), 不含 nside/nested/n_pix/has_snr/snr_format
// snr_format: 0=逐像素 (旧), 1=稀疏控制点 (新); has_snr=false 时不写入 snr_format
// snr_n_points: 稀疏控制点数 (仅 snr_format=1 时写入)
static std::string hio_build_json(uint32_t nside, int nested, uint64_t n_pix,
                                  bool has_snr, int snr_format, uint32_t snr_n_points,
                                  const char* meta_json) {
    std::string result;
    result.reserve(256);
    result += "{\"nside\":";
    result += std::to_string(nside);
    result += ",\"nested\":";
    result += (nested ? "true" : "false");
    result += ",\"n_pix\":";
    result += std::to_string(n_pix);
    result += ",\"has_snr\":";
    result += (has_snr ? "true" : "false");

    // snr_format 字段 (仅 has_snr=true 时写入, 旧文件无此字段默认 0)
    if (has_snr) {
        result += ",\"snr_format\":";
        result += std::to_string(snr_format);
        if (snr_format == 1) {
            result += ",\"snr_n_points\":";
            result += std::to_string(snr_n_points);
        }
    }

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

AIO_EXPORT int aio_hiss_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const float* snr,
                       const char* meta_json) {
    // WP-E 步骤8: 改造成新 HissWriter 后端 (保持 C API 签名不变)
    (void)snr;       // 旧API的逐像素SNR在新格式中不直接写入 (新格式用稀疏控制点)
    (void)meta_json; // 新HissMetadata有自己的JSON序列化, 忽略旧meta_json

    if (!path || (n_pix > 0 && (!ipix || !pixel))) {
        fprintf(stderr, "[hio] hiss_write: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    if (!nested) {
        fprintf(stderr, "[hio] hiss_write: HISS 要求 NESTED ordering, RING 不支持\n");
        return HIO_ERR_PARAM;
    }

    fprintf(stderr, "[hio] hiss_write (新HissWriter后端): path=%s nside=%u n_pix=%llu\n",
            path, nside, (unsigned long long)n_pix);

    if (n_pix == 0) {
        // 空数据, 仍创建文件 (含 Header, 无 Tile)
        hiss::HissGridSpec grid;
        grid.nside = nside;
        grid.tile_nside = hiss::compute_tile_nside(nside);
        grid.ordering = 1;
        grid.radesys = 0;
        grid.pixfrac = 1.0;
        hiss::HissMetadata hmeta;
        hmeta.nside = nside;
        hmeta.tile_nside = grid.tile_nside;
        hiss::HissWriter writer;
        if (writer.open(path, grid, hmeta) != 0) return HIO_ERR_FILE;
        if (writer.finalize() != 0) return HIO_ERR_FILE;
        return HIO_OK;
    }

    // 1. 计算 Tile 几何 (02_FROZEN §11)
    uint32_t depth = hiss::compute_tile_depth(nside);
    uint32_t tile_nside = hiss::compute_tile_nside(nside);
    uint32_t n_leaf_per_tile = 1u << (2 * depth);
    int shift = 2 * (int)depth;
    // A_p = 4π / (12 * NSIDE²) (球面度)
    const double kPi = 3.14159265358979323846;
    double A_p = 4.0 * kPi / (12.0 * (double)nside * (double)nside);

    // 2. 按 Tile 父像素分组 (NESTED 位运算)
    struct TileGroup {
        uint64_t parent_ipix = 0;
        std::vector<std::pair<uint32_t, float>> pixels;
    };
    std::map<uint64_t, TileGroup> tile_groups;
    for (uint64_t i = 0; i < n_pix; i++) {
        uint64_t gipix = ipix[i];
        uint64_t parent = (shift > 0) ? (gipix >> shift) : gipix;
        uint32_t local  = (shift > 0) ? (uint32_t)(gipix & ((1ULL << shift) - 1)) : 0;
        tile_groups[parent].parent_ipix = parent;
        tile_groups[parent].pixels.push_back({local, pixel[i]});
    }

    // 3. 构造 HissGridSpec + HissMetadata
    hiss::HissGridSpec grid;
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;  // NESTED
    grid.radesys = 0;   // ICRS
    grid.pixfrac = 1.0;

    hiss::HissMetadata hmeta;
    hmeta.nside = nside;
    hmeta.tile_nside = tile_nside;
    hmeta.ordering = 1;
    hmeta.radesys = 0;
    hmeta.pixfrac = 1.0;
    hmeta.photappl = 0;
    std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ADU");

    // 4. HissWriter 写入
    hiss::HissWriter writer;
    if (writer.open(path, grid, hmeta) != 0) {
        fprintf(stderr, "[hio] hiss_write: HissWriter.open 失败\n");
        return HIO_ERR_FILE;
    }

    for (const auto& [parent_ipix, tg] : tile_groups) {
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside = tile_nside;
        acc.parent_ipix = parent_ipix;
        acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf_per_tile);
        for (const auto& [local, sig] : tg.pixels) {
            if (local < n_leaf_per_tile) {
                acc.pixels[local].sum_flux = sig;
                acc.pixels[local].sum_area = A_p;  // 旧API无support, 设全覆盖
            }
        }
        if (writer.add_tile(parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
            fprintf(stderr, "[hio] hiss_write: HissWriter.add_tile 失败\n");
            writer.cancel();
            return HIO_ERR_FILE;
        }
    }

    if (writer.finalize() != 0) {
        fprintf(stderr, "[hio] hiss_write: HissWriter.finalize 失败\n");
        return HIO_ERR_FILE;
    }

    fprintf(stderr, "[hio] hiss_write: 写入完成 (新HissWriter后端, %zu Tile): %s\n",
            tile_groups.size(), path);
    return HIO_OK;
}

// ============================================================================
// .hiss 读取实现
// ============================================================================

AIO_EXPORT int aio_hiss_read(const char* path, uint32_t* nside, int* nested,
                      uint64_t* n_pix, uint64_t** ipix,
                      float** pixel, float** snr, char** meta_json) {
    // WP-E 步骤8: 改造成新 HissReader 后端 (保持 C API 签名不变)
    // 新格式为 XISF 式 Header + attachments, 旧格式 (Magic=HISS + JSON + 平铺数组) 不再支持读取
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
    if (snr) *snr = nullptr;  // 新格式 SNR 为稀疏控制点, 不输出逐像素

    // 1. 用 HissReader 打开文件
    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 2. 获取网格规格和元数据
    hiss::HissGridSpec grid = reader.grid();
    hiss::HissMetadata hmeta = reader.metadata();

    *nside = grid.nside;
    *nested = (grid.ordering == 1) ? 1 : 0;

    // 3. 序列化 meta_json (使用 HissMetadata::to_json)
    std::string jsonStr = hmeta.to_json();
    *meta_json = (char*)std::malloc(jsonStr.size() + 1);
    if (!*meta_json) {
        fprintf(stderr, "[hio] hiss_read: 分配 meta_json 内存失败\n");
        return HIO_ERR_MEM;
    }
    std::memcpy(*meta_json, jsonStr.c_str(), jsonStr.size() + 1);

    // 4. 遍历所有 Tile, 读取 signal (展开到 n_leaf_per_tile), 收集有效像素
    // 有效像素定义: signal != 0.0f (有累计通量)
    // 新格式 signal = 累计通量 (步骤7), 无贡献像素 signal=0.0f 自然跳过
    const std::vector<hiss::HissTile>& tiles = reader.tiles();
    if (tiles.empty()) {
        // 空 Tile (无数据), 仅返回 Header 信息
        *n_pix = 0;
        fprintf(stderr, "[hio] hiss_read: 无 Tile (空文件): %s\n", path);
        return HIO_OK;
    }

    // 计算 Tile 几何 (用于 local_ipix → global_ipix 转换)
    uint32_t depth = hiss::compute_tile_depth(grid.nside);
    int shift = 2 * (int)depth;

    // 收集所有有效像素 (signal != 0.0f)
    std::vector<uint64_t> out_ipix;
    std::vector<float>    out_pixel;
    // 预估: 假设平均每个 Tile 有 50% 有效像素
    uint64_t est_total = (uint64_t)tiles.size() * (1ULL << (2 * depth)) / 2;
    out_ipix.reserve(est_total > 0 ? est_total : 1024);
    out_pixel.reserve(est_total > 0 ? est_total : 1024);

    for (const auto& tile : tiles) {
        std::vector<float> signal;
        std::vector<uint8_t> support;
        int ret = reader.read_tile(tile.parent_ipix, signal, support);
        if (ret != 0) {
            fprintf(stderr, "[hio] hiss_read: read_tile 失败 parent=%llu ret=%d\n",
                    (unsigned long long)tile.parent_ipix, ret);
            std::free(*meta_json);
            *meta_json = nullptr;
            return HIO_ERR_FILE;
        }

        // signal 已展开到 n_leaf_per_tile (Reader 自动展开 BITMAP/SPARSE)
        // global_ipix = (parent_ipix << shift) | local_ipix
        for (size_t local = 0; local < signal.size(); local++) {
            // 仅返回有数据的像素 (signal != 0.0f 或 support > 0)
            if (signal[local] != 0.0f || (local < support.size() && support[local] > 0)) {
                uint64_t global_ipix = ((uint64_t)tile.parent_ipix << shift) | (uint64_t)local;
                out_ipix.push_back(global_ipix);
                out_pixel.push_back(signal[local]);
            }
        }
    }

    // 5. 分配输出数组 (malloc, 调用者负责 free)
    *n_pix = (uint64_t)out_ipix.size();
    if (*n_pix > 0) {
        *ipix = (uint64_t*)std::malloc((size_t)(*n_pix) * sizeof(uint64_t));
        if (!*ipix) {
            fprintf(stderr, "[hio] hiss_read: 分配 ipix 内存失败\n");
            std::free(*meta_json);
            *meta_json = nullptr;
            return HIO_ERR_MEM;
        }
        std::memcpy(*ipix, out_ipix.data(), (size_t)(*n_pix) * sizeof(uint64_t));

        *pixel = (float*)std::malloc((size_t)(*n_pix) * sizeof(float));
        if (!*pixel) {
            fprintf(stderr, "[hio] hiss_read: 分配 pixel 内存失败\n");
            std::free(*ipix);
            *ipix = nullptr;
            std::free(*meta_json);
            *meta_json = nullptr;
            return HIO_ERR_MEM;
        }
        std::memcpy(*pixel, out_pixel.data(), (size_t)(*n_pix) * sizeof(float));
    }

    fprintf(stderr, "[hio] hiss_read (新HissReader后端): nside=%u nested=%d n_pix=%llu tiles=%zu: %s\n",
            *nside, *nested, (unsigned long long)*n_pix, tiles.size(), path);
    return HIO_OK;
}

// ============================================================================
// WP-H 步骤14: aio_hiss_inspect - 只读 Header (不加载 Tile 数据)
// 用于 CLI 诊断输出和 Browser 首次打开
// ============================================================================

AIO_EXPORT int aio_hiss_inspect(const char* path,
                                  uint32_t* nside,
                                  uint32_t* tile_nside,
                                  uint32_t* depth,
                                  uint32_t* n_leaf_per_tile,
                                  uint64_t* n_tiles,
                                  uint64_t* n_pix_total,
                                  char** meta_json,
                                  uint64_t** tile_ipix_list) {
    if (!path || !nside || !tile_nside || !depth || !n_leaf_per_tile ||
        !n_tiles || !n_pix_total || !meta_json) {
        fprintf(stderr, "[hio] hiss_inspect: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    // 初始化输出
    *nside = 0;
    *tile_nside = 0;
    *depth = 0;
    *n_leaf_per_tile = 0;
    *n_tiles = 0;
    *n_pix_total = 0;
    *meta_json = nullptr;
    if (tile_ipix_list) *tile_ipix_list = nullptr;

    // 用 HissReader 打开文件 (只读 Header + Tile 目录, 不加载 Tile 数据)
    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_inspect: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    hiss::HissGridSpec grid = reader.grid();
    hiss::HissMetadata hmeta = reader.metadata();
    const std::vector<hiss::HissTile>& tiles = reader.tiles();

    *nside = grid.nside;
    *tile_nside = grid.tile_nside;
    *depth = hiss::compute_tile_depth(grid.nside);
    *n_leaf_per_tile = 1u << (2 * (*depth));  // 4^depth
    *n_tiles = (uint64_t)tiles.size();
    // n_pix_total: 全部 Tile 展开后的像素数 (含无效像素)
    *n_pix_total = (uint64_t)tiles.size() * (*n_leaf_per_tile);

    // 序列化 meta_json
    std::string jsonStr = hmeta.to_json();
    *meta_json = (char*)std::malloc(jsonStr.size() + 1);
    if (!*meta_json) {
        fprintf(stderr, "[hio] hiss_inspect: 分配 meta_json 内存失败\n");
        return HIO_ERR_MEM;
    }
    std::memcpy(*meta_json, jsonStr.c_str(), jsonStr.size() + 1);

    // WP-H 步骤14: 可选输出 Tile 目录 (parent_ipix 列表)
    if (tile_ipix_list && !tiles.empty()) {
        *tile_ipix_list = (uint64_t*)std::malloc(tiles.size() * sizeof(uint64_t));
        if (!*tile_ipix_list) {
            fprintf(stderr, "[hio] hiss_inspect: 分配 tile_ipix_list 内存失败\n");
            std::free(*meta_json);
            *meta_json = nullptr;
            return HIO_ERR_MEM;
        }
        for (size_t i = 0; i < tiles.size(); ++i) {
            (*tile_ipix_list)[i] = tiles[i].parent_ipix;
        }
    }

    fprintf(stderr, "[hio] hiss_inspect: nside=%u tile_nside=%u depth=%u n_leaf=%u n_tiles=%llu: %s\n",
            *nside, *tile_nside, *depth, *n_leaf_per_tile,
            (unsigned long long)*n_tiles, path);
    return HIO_OK;
}

// ============================================================================
// WP-H 步骤14: aio_hiss_read_tile_signal - 按 Tile 读取 signal (FP32)
// ============================================================================

AIO_EXPORT int aio_hiss_read_tile_signal(const char* path, uint64_t parent_ipix,
                                           float** signal, uint32_t* n_signal) {
    if (!path || !signal || !n_signal) {
        fprintf(stderr, "[hio] hiss_read_tile_signal: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *signal = nullptr;
    *n_signal = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_signal: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    std::vector<float> sig_vec;
    int ret = reader.read_tile_signal(parent_ipix, sig_vec);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_signal: read_tile_signal 失败 ret=%d parent=%llu\n",
                ret, (unsigned long long)parent_ipix);
        return HIO_ERR_FILE;
    }

    if (sig_vec.empty()) {
        return HIO_OK;
    }

    *n_signal = (uint32_t)sig_vec.size();
    *signal = (float*)std::malloc(sig_vec.size() * sizeof(float));
    if (!*signal) {
        fprintf(stderr, "[hio] hiss_read_tile_signal: 内存分配失败\n");
        return HIO_ERR_MEM;
    }
    std::memcpy(*signal, sig_vec.data(), sig_vec.size() * sizeof(float));
    return HIO_OK;
}

// ============================================================================
// Session API — Verify 单句柄遍历全部 Tile
// ============================================================================

AIO_EXPORT void* aio_hiss_open_session(const char* path,
                                         uint32_t* nside, uint32_t* tile_nside,
                                         uint64_t* n_tiles) {
    if (!path) return nullptr;
    auto* reader = new (std::nothrow) hiss::HissReader();
    if (!reader) return nullptr;
    if (reader->open(path) != 0) {
        delete reader;
        return nullptr;
    }
    if (nside) *nside = reader->grid().nside;
    if (tile_nside) *tile_nside = reader->grid().tile_nside;
    if (n_tiles) *n_tiles = reader->tiles().size();
    return reader;
}

AIO_EXPORT int aio_hiss_read_tile_signal_session(void* session, uint64_t parent_ipix,
                                                   float** signal, uint32_t* n_signal) {
    if (!session || !signal || !n_signal) return HIO_ERR_PARAM;
    *signal = nullptr; *n_signal = 0;
    auto* reader = static_cast<hiss::HissReader*>(session);
    std::vector<float> v;
    int ret = reader->read_tile_signal(parent_ipix, v);
    if (ret != 0) return HIO_ERR_FILE;
    if (v.empty()) return HIO_OK;
    *n_signal = (uint32_t)v.size();
    *signal = (float*)std::malloc(v.size() * sizeof(float));
    if (!*signal) return HIO_ERR_MEM;
    std::memcpy(*signal, v.data(), v.size() * sizeof(float));
    return HIO_OK;
}

AIO_EXPORT int aio_hiss_read_tile_signal_f64_session(void* session, uint64_t parent_ipix,
                                                       double** signal, uint32_t* n_signal) {
    if (!session || !signal || !n_signal) return HIO_ERR_PARAM;
    *signal = nullptr; *n_signal = 0;
    auto* reader = static_cast<hiss::HissReader*>(session);
    std::vector<double> v;
    int ret = reader->read_tile_signal_f64(parent_ipix, v);
    if (ret != 0) return HIO_ERR_FILE;
    if (v.empty()) return HIO_OK;
    *n_signal = (uint32_t)v.size();
    *signal = (double*)std::malloc(v.size() * sizeof(double));
    if (!*signal) return HIO_ERR_MEM;
    std::memcpy(*signal, v.data(), v.size() * sizeof(double));
    return HIO_OK;
}

AIO_EXPORT int aio_hiss_read_tile_support_session(void* session, uint64_t parent_ipix,
                                                    uint8_t** support, uint32_t* n_support) {
    if (!session || !support || !n_support) return HIO_ERR_PARAM;
    *support = nullptr; *n_support = 0;
    auto* reader = static_cast<hiss::HissReader*>(session);
    std::vector<uint8_t> v;
    int ret = reader->read_tile_support(parent_ipix, v);
    if (ret != 0) return HIO_ERR_FILE;
    if (v.empty()) return HIO_OK;
    *n_support = (uint32_t)v.size();
    *support = (uint8_t*)std::malloc(v.size());
    if (!*support) return HIO_ERR_MEM;
    std::memcpy(*support, v.data(), v.size());
    return HIO_OK;
}

AIO_EXPORT int aio_hiss_read_tile_snr_session(void* session, uint64_t parent_ipix,
                                                uint8_t** snr_out, uint32_t* n_points) {
    if (!session || !snr_out || !n_points) return HIO_ERR_PARAM;
    *snr_out = nullptr; *n_points = 0;
    auto* reader = static_cast<hiss::HissReader*>(session);
    hiss::HissSnrBlock snr;
    int ret = reader->read_tile_snr(parent_ipix, snr);
    if (ret != 0) return HIO_ERR_FILE;
    if (snr.points.empty()) return HIO_OK;
    *n_points = (uint32_t)snr.points.size();
    *snr_out = (uint8_t*)std::malloc(snr.points.size() * 8);
    if (!*snr_out) return HIO_ERR_MEM;
    for (size_t i = 0; i < snr.points.size(); i++) {
        uint32_t local_ipix = snr.points[i].local_ipix;
        float snr_val = snr.points[i].snr;
        uint8_t* p = *snr_out + i * 8;
        std::memcpy(p, &local_ipix, 4);
        std::memcpy(p + 4, &snr_val, 4);
    }
    return HIO_OK;
}

AIO_EXPORT int aio_hiss_read_tile_snr_f64_session(void* session, uint64_t parent_ipix,
                                                    uint8_t** snr_out, uint32_t* n_points) {
    if (!session || !snr_out || !n_points) return HIO_ERR_PARAM;
    *snr_out = nullptr; *n_points = 0;
    auto* reader = static_cast<hiss::HissReader*>(session);
    hiss::HissSnrBlockF64 snr;
    int ret = reader->read_tile_snr_f64(parent_ipix, snr);
    if (ret != 0) return HIO_ERR_FILE;
    if (snr.points.empty()) return HIO_OK;
    *n_points = (uint32_t)snr.points.size();
    *snr_out = (uint8_t*)std::malloc(snr.points.size() * 12);
    if (!*snr_out) return HIO_ERR_MEM;
    for (size_t i = 0; i < snr.points.size(); i++) {
        uint32_t local_ipix = snr.points[i].local_ipix;
        double snr_val = snr.points[i].snr;
        uint8_t* p = *snr_out + i * 12;
        std::memcpy(p, &local_ipix, 4);
        std::memcpy(p + 4, &snr_val, 8);
    }
    return HIO_OK;
}

AIO_EXPORT void aio_hiss_close_session(void* session) {
    if (session) delete static_cast<hiss::HissReader*>(session);
}

// ============================================================================
// aio_hiss_read_tile_signal_f64 - 按 Tile 读取 signal (FP64)
// ============================================================================

AIO_EXPORT int aio_hiss_read_tile_signal_f64(const char* path, uint64_t parent_ipix,
                                               double** signal, uint32_t* n_signal) {
    if (!path || !signal || !n_signal) {
        fprintf(stderr, "[hio] hiss_read_tile_signal_f64: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *signal = nullptr;
    *n_signal = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_signal_f64: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    std::vector<double> sig_vec;
    int ret = reader.read_tile_signal_f64(parent_ipix, sig_vec);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_signal_f64: read_tile_signal_f64 失败 ret=%d parent=%llu\n",
                ret, (unsigned long long)parent_ipix);
        return HIO_ERR_FILE;
    }

    if (sig_vec.empty()) {
        return HIO_OK;
    }

    *n_signal = (uint32_t)sig_vec.size();
    *signal = (double*)std::malloc(sig_vec.size() * sizeof(double));
    if (!*signal) {
        fprintf(stderr, "[hio] hiss_read_tile_signal_f64: 内存分配失败\n");
        return HIO_ERR_MEM;
    }
    std::memcpy(*signal, sig_vec.data(), sig_vec.size() * sizeof(double));
    return HIO_OK;
}

// ============================================================================
// WP-H 步骤14: aio_hiss_read_tile_support - 按 Tile 读取 support
// ============================================================================

AIO_EXPORT int aio_hiss_read_tile_support(const char* path, uint64_t parent_ipix,
                                            uint8_t** support, uint32_t* n_support) {
    if (!path || !support || !n_support) {
        fprintf(stderr, "[hio] hiss_read_tile_support: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *support = nullptr;
    *n_support = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_support: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    std::vector<uint8_t> sup_vec;
    int ret = reader.read_tile_support(parent_ipix, sup_vec);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_support: read_tile_support 失败 ret=%d parent=%llu\n",
                ret, (unsigned long long)parent_ipix);
        return HIO_ERR_FILE;
    }

    if (sup_vec.empty()) {
        return HIO_OK;
    }

    *n_support = (uint32_t)sup_vec.size();
    *support = (uint8_t*)std::malloc(sup_vec.size());
    if (!*support) {
        fprintf(stderr, "[hio] hiss_read_tile_support: 内存分配失败\n");
        return HIO_ERR_MEM;
    }
    std::memcpy(*support, sup_vec.data(), sup_vec.size());
    return HIO_OK;
}

// ============================================================================
// WP-H 步骤14: aio_hiss_read_tile_snr - 按 Tile 读取 SNR 控制点
// 返回紧凑二进制: n_points * 8 字节, 每点 local_ipix(uint32 LE) + snr(float32 LE)
// ============================================================================

AIO_EXPORT int aio_hiss_read_tile_snr(const char* path, uint64_t parent_ipix,
                                        uint8_t** snr_out, uint32_t* n_points) {
    if (!path || !snr_out || !n_points) {
        fprintf(stderr, "[hio] hiss_read_tile_snr: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *snr_out = nullptr;
    *n_points = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_snr: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    hiss::HissSnrBlock snr_block;
    int ret = reader.read_tile_snr(parent_ipix, snr_block);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_snr: read_tile_snr 失败 ret=%d parent=%llu\n",
                ret, (unsigned long long)parent_ipix);
        return HIO_ERR_FILE;
    }

    if (snr_block.points.empty()) {
        return HIO_OK;  // 无 SNR 控制点
    }

    *n_points = (uint32_t)snr_block.points.size();
    *snr_out = (uint8_t*)std::malloc(snr_block.points.size() * 8);
    if (!*snr_out) {
        fprintf(stderr, "[hio] hiss_read_tile_snr: 内存分配失败\n");
        return HIO_ERR_MEM;
    }
    // 序列化为 local_ipix(uint32 LE) + snr(float32 LE)
    for (size_t i = 0; i < snr_block.points.size(); i++) {
        uint32_t local_ipix = snr_block.points[i].local_ipix;
        float snr_val = snr_block.points[i].snr;
        uint8_t* p = *snr_out + i * 8;
        std::memcpy(p, &local_ipix, 4);      // LE
        std::memcpy(p + 4, &snr_val, 4);     // LE
    }
    return HIO_OK;
}

// ============================================================================
// aio_hiss_read_tile_snr_f64 - 读取 FP64 SNR 控制点 (snr_dtype=1 文件)
// 返回紧凑二进制: n_points * 12 字节, 每点 local_ipix(uint32 LE) + snr(float64 LE)
// f32 文件返回错误 (禁止静默转换)
// ============================================================================
AIO_EXPORT int aio_hiss_read_tile_snr_f64(const char* path, uint64_t parent_ipix,
                                            uint8_t** snr_out, uint32_t* n_points) {
    if (!path || !snr_out || !n_points) {
        fprintf(stderr, "[hio] hiss_read_tile_snr_f64: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *snr_out = nullptr;
    *n_points = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_snr_f64: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    hiss::HissSnrBlockF64 snr_block;
    int ret = reader.read_tile_snr_f64(parent_ipix, snr_block);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_read_tile_snr_f64: read_tile_snr_f64 失败 ret=%d parent=%llu\n",
                ret, (unsigned long long)parent_ipix);
        return HIO_ERR_FILE;
    }

    if (snr_block.points.empty()) {
        return HIO_OK;  // 无 SNR 控制点
    }

    *n_points = (uint32_t)snr_block.points.size();
    *snr_out = (uint8_t*)std::malloc(snr_block.points.size() * 12);
    if (!*snr_out) {
        fprintf(stderr, "[hio] hiss_read_tile_snr_f64: 内存分配失败\n");
        return HIO_ERR_MEM;
    }
    for (size_t i = 0; i < snr_block.points.size(); i++) {
        uint32_t local_ipix = snr_block.points[i].local_ipix;
        double snr_val = snr_block.points[i].snr;
        uint8_t* p = *snr_out + i * 12;
        std::memcpy(p, &local_ipix, 4);   // LE
        std::memcpy(p + 4, &snr_val, 8);  // LE (f64)
    }
    return HIO_OK;
}

// ============================================================================
// WP-H 步骤14: aio_hiss_query_pixel - 通过 ra/dec 查询像素值
// 与 HissReader::query_pixel 一致
// ============================================================================

AIO_EXPORT int aio_hiss_query_pixel(const char* path, double ra, double dec,
                                      float* signal, uint8_t* support) {
    if (!path || !signal || !support) {
        fprintf(stderr, "[hio] hiss_query_pixel: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *signal = 0.0f;
    *support = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_query_pixel: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    int ret = reader.query_pixel(ra, dec, signal, support);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_query_pixel: query_pixel 失败 ret=%d ra=%.4f dec=%.4f\n",
                ret, ra, dec);
        return HIO_ERR_FILE;
    }
    return HIO_OK;
}

// ============================================================================
// aio_hiss_query_pixel_f64 - 通过 ra/dec 查询像素值 (FP64 版本)
// 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
// ============================================================================

AIO_EXPORT int aio_hiss_query_pixel_f64(const char* path, double ra, double dec,
                                          double* signal, uint8_t* support) {
    if (!path || !signal || !support) {
        fprintf(stderr, "[hio] hiss_query_pixel_f64: 无效参数\n");
        return HIO_ERR_PARAM;
    }
    *signal = 0.0;
    *support = 0;

    hiss::HissReader reader;
    if (reader.open(path) != 0) {
        fprintf(stderr, "[hio] hiss_query_pixel_f64: HissReader.open 失败: %s\n", path);
        return HIO_ERR_FILE;
    }

    int ret = reader.query_pixel_f64(ra, dec, signal, support);
    if (ret != 0) {
        fprintf(stderr, "[hio] hiss_query_pixel_f64: query_pixel_f64 失败 ret=%d ra=%.4f dec=%.4f\n",
                ret, ra, dec);
        return HIO_ERR_FILE;
    }
    return HIO_OK;
}

// ============================================================================
// .hcsd 写入实现 (含子叶块索引构建)
// ============================================================================

AIO_EXPORT int aio_hcsd_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const char* meta_json) {
    if (!path || (n_pix > 0 && (!ipix || !pixel))) {
        fprintf(stderr, "[hio] hcsd_write: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    fprintf(stderr, "[hio] hcsd_write: path=%s nside=%u nested=%d n_pix=%llu\n",
            path, nside, nested, (unsigned long long)n_pix);

    // 1. 构建 JSON 头 (hcsd 无 snr 通道, snr_format=0)
    std::string jsonStr = hio_build_json(nside, nested, n_pix, false, 0, 0, meta_json);
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

AIO_EXPORT int aio_hcsd_read(const char* path, uint32_t* nside, int* nested,
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

AIO_EXPORT int aio_hcsd_read_leaf(const char* path, uint64_t leaf_ipix_at_nside64,
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

AIO_EXPORT void aio_hio_free(void* ptr) {
    if (ptr) std::free(ptr);
}

// ============================================================================
// .hiss 稀疏 SNR 模型写入 (snr_format=1)
//
// 二进制布局 (在 ipix + pixel 数组之后):
// [n_points: uint32]
// [points: n_points × 20B {ra_f64, dec_f64, snr_f32}]
// [snr_phot: f64][median_snr: f64][idw_power: f64]
// ============================================================================

AIO_EXPORT int aio_hiss_write_snr_model(const char* path, uint32_t nside, int nested,
                                  uint64_t n_pix, const uint64_t* ipix,
                                  const float* pixel,
                                  const HioSnrModel* snr_model,
                                  const char* meta_json) {
    if (!path || (n_pix > 0 && (!ipix || !pixel))) {
        fprintf(stderr, "[hio] hiss_write_snr_model: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    bool has_snr = (snr_model != nullptr && snr_model->n_points > 0);
    uint32_t snr_n_points = has_snr ? snr_model->n_points : 0;

    fprintf(stderr, "[hio] hiss_write_snr_model: path=%s nside=%u n_pix=%llu has_snr=%d n_points=%u\n",
            path, nside, (unsigned long long)n_pix, has_snr, snr_n_points);

    // 1. 构建 JSON 头 (snr_format=1, has_snr=has_snr)
    std::string jsonStr = hio_build_json(nside, nested, n_pix, has_snr, 1, snr_n_points, meta_json);
    uint32_t jsonLen = (uint32_t)jsonStr.size();

    // 2. 压缩 JSON 头
    std::vector<uint8_t> compJson;
    size_t compJsonLen = hio_zstd_compress(jsonStr.data(), jsonLen, compJson, ZSTD_LEVEL);
    if (compJsonLen == 0) {
        fprintf(stderr, "[hio] hiss_write_snr_model: JSON 头压缩失败\n");
        return HIO_ERR_ZSTD;
    }
    uint32_t compLen = (uint32_t)compJsonLen;

    // 3. 打开文件
    FILE* fp = hio_fopen_utf8(path, "wb");
    if (!fp) {
        fprintf(stderr, "[hio] hiss_write_snr_model: 无法创建文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 4. 写入 Magic
    if (std::fwrite(HISS_MAGIC, 1, 4, fp) != 4) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 5. 写入 JSON 头长度 + 压缩 JSON
    uint32_t hdr[2] = {jsonLen, compLen};
    if (std::fwrite(hdr, 4, 2, fp) != 2) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::fwrite(compJson.data(), 1, compLen, fp) != compLen) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }

    // 6. 写入 ipix + pixel 数组
    if (n_pix > 0) {
        size_t ipixBytes = (size_t)n_pix * sizeof(uint64_t);
        if (std::fwrite(ipix, 1, ipixBytes, fp) != ipixBytes) {
            fprintf(stderr, "[hio] hiss_write_snr_model: 写入 ipix 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
        size_t pixelBytes = (size_t)n_pix * sizeof(float);
        if (std::fwrite(pixel, 1, pixelBytes, fp) != pixelBytes) {
            fprintf(stderr, "[hio] hiss_write_snr_model: 写入 pixel 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    // 7. 写入稀疏 SNR 模型 (仅 has_snr=true)
    if (has_snr) {
        // n_points: uint32
        if (std::fwrite(&snr_n_points, sizeof(uint32_t), 1, fp) != 1) {
            fprintf(stderr, "[hio] hiss_write_snr_model: 写入 n_points 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
        // points: n_points × 24B
        size_t pointsBytes = (size_t)snr_n_points * sizeof(HioSnrControlPoint);
        if (std::fwrite(snr_model->points, 1, pointsBytes, fp) != pointsBytes) {
            fprintf(stderr, "[hio] hiss_write_snr_model: 写入 points 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
        // snr_phot, median_snr, idw_power: 3 × f64
        double scalars[3] = {snr_model->snr_phot, snr_model->median_snr, snr_model->idw_power};
        if (std::fwrite(scalars, sizeof(double), 3, fp) != 3) {
            fprintf(stderr, "[hio] hiss_write_snr_model: 写入 scalars 失败\n");
            std::fclose(fp);
            return HIO_ERR_FILE;
        }
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hiss_write_snr_model: 写入完成: %s (has_snr=%d n_points=%u)\n",
            path, has_snr, snr_n_points);
    return HIO_OK;
}

// ============================================================================
// .hiss 稀疏 SNR 模型读取 (snr_format=1)
// ============================================================================

AIO_EXPORT int aio_hiss_read_snr_model(const char* path, uint32_t* nside, int* nested,
                                 uint64_t* n_pix, uint64_t** ipix,
                                 float** pixel, HioSnrModel** snr_model,
                                 char** meta_json) {
    if (!path || !nside || !nested || !n_pix || !ipix || !pixel || !snr_model || !meta_json) {
        fprintf(stderr, "[hio] hiss_read_snr_model: 无效参数\n");
        return HIO_ERR_PARAM;
    }

    // 初始化输出
    *nside = 0;
    *nested = 0;
    *n_pix = 0;
    *ipix = nullptr;
    *pixel = nullptr;
    *snr_model = nullptr;
    *meta_json = nullptr;

    FILE* fp = hio_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "[hio] hiss_read_snr_model: 无法打开文件: %s\n", path);
        return HIO_ERR_FILE;
    }

    // 1. 读取并验证 Magic
    char magic[4];
    if (std::fread(magic, 1, 4, fp) != 4) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    if (std::memcmp(magic, HISS_MAGIC, 4) != 0) {
        fprintf(stderr, "[hio] hiss_read_snr_model: Magic 不匹配 (期望 HISS)\n");
        std::fclose(fp);
        return HIO_ERR_MAGIC;
    }

    // 2. 读取 JSON 头
    uint32_t hdr[2];
    if (std::fread(hdr, 4, 2, fp) != 2) {
        std::fclose(fp);
        return HIO_ERR_FILE;
    }
    uint32_t uncompLen = hdr[0];
    uint32_t compLen = hdr[1];

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
            fprintf(stderr, "[hio] hiss_read_snr_model: JSON 解压失败\n");
            std::fclose(fp);
            return HIO_ERR_ZSTD;
        }
    }

    // 3. 解析 JSON 头
    uint32_t jsonNside = 0;
    int jsonNested = 0;
    uint64_t jsonNPix = 0;
    int jsonHasSnr = 0;
    uint32_t jsonSnrFormat = 0;
    uint32_t jsonSnrNPoints = 0;
    if (!hio_parse_json_uint32(jsonStr, "nside", jsonNside) ||
        !hio_parse_json_bool(jsonStr, "nested", jsonNested) ||
        !hio_parse_json_uint64(jsonStr, "n_pix", jsonNPix)) {
        fprintf(stderr, "[hio] hiss_read_snr_model: JSON 解析失败\n");
        std::fclose(fp);
        return HIO_ERR_JSON;
    }
    hio_parse_json_bool(jsonStr, "has_snr", jsonHasSnr);
    hio_parse_json_uint32(jsonStr, "snr_format", jsonSnrFormat);
    hio_parse_json_uint32(jsonStr, "snr_n_points", jsonSnrNPoints);

    *nside = jsonNside;
    *nested = jsonNested;
    *n_pix = jsonNPix;

    *meta_json = (char*)std::malloc(jsonStr.size() + 1);
    if (!*meta_json) {
        std::fclose(fp);
        return HIO_ERR_MEM;
    }
    std::memcpy(*meta_json, jsonStr.c_str(), jsonStr.size() + 1);

    fprintf(stderr, "[hio] hiss_read_snr_model: nside=%u n_pix=%llu has_snr=%d snr_format=%u n_points=%u\n",
            *nside, (unsigned long long)*n_pix, jsonHasSnr, jsonSnrFormat, jsonSnrNPoints);

    // 4. 读取 ipix + pixel
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

    // 5. 读取 SNR 通道
    if (jsonHasSnr) {
        if (jsonSnrFormat == 1) {
            // 稀疏控制点格式
            if (jsonSnrNPoints == 0) {
                fprintf(stderr, "[hio] hiss_read_snr_model: 警告 snr_format=1 但 n_points=0\n");
            } else {
                // 读取 n_points
                uint32_t nPoints = 0;
                if (std::fread(&nPoints, sizeof(uint32_t), 1, fp) != 1) {
                    fprintf(stderr, "[hio] hiss_read_snr_model: 读取 n_points 失败\n");
                    std::free(*pixel); *pixel = nullptr;
                    std::free(*ipix); *ipix = nullptr;
                    std::free(*meta_json); *meta_json = nullptr;
                    std::fclose(fp);
                    return HIO_ERR_FILE;
                }
                if (nPoints != jsonSnrNPoints) {
                    fprintf(stderr, "[hio] hiss_read_snr_model: 警告 n_points 不一致 (json=%u 实际=%u)\n",
                            jsonSnrNPoints, nPoints);
                }

                // 分配 HioSnrModel
                HioSnrModel* model = (HioSnrModel*)std::malloc(sizeof(HioSnrModel));
                if (!model) {
                    std::free(*pixel); *pixel = nullptr;
                    std::free(*ipix); *ipix = nullptr;
                    std::free(*meta_json); *meta_json = nullptr;
                    std::fclose(fp);
                    return HIO_ERR_MEM;
                }
                model->n_points = nPoints;
                model->points = (HioSnrControlPoint*)std::malloc((size_t)nPoints * sizeof(HioSnrControlPoint));
                if (!model->points) {
                    std::free(model);
                    std::free(*pixel); *pixel = nullptr;
                    std::free(*ipix); *ipix = nullptr;
                    std::free(*meta_json); *meta_json = nullptr;
                    std::fclose(fp);
                    return HIO_ERR_MEM;
                }

                // 读取 points
                size_t pointsBytes = (size_t)nPoints * sizeof(HioSnrControlPoint);
                if (std::fread(model->points, 1, pointsBytes, fp) != pointsBytes) {
                    fprintf(stderr, "[hio] hiss_read_snr_model: 读取 points 失败\n");
                    std::free(model->points);
                    std::free(model);
                    std::free(*pixel); *pixel = nullptr;
                    std::free(*ipix); *ipix = nullptr;
                    std::free(*meta_json); *meta_json = nullptr;
                    std::fclose(fp);
                    return HIO_ERR_FILE;
                }

                // 读取 scalars (snr_phot, median_snr, idw_power)
                double scalars[3] = {0};
                if (std::fread(scalars, sizeof(double), 3, fp) != 3) {
                    fprintf(stderr, "[hio] hiss_read_snr_model: 读取 scalars 失败\n");
                    std::free(model->points);
                    std::free(model);
                    std::free(*pixel); *pixel = nullptr;
                    std::free(*ipix); *ipix = nullptr;
                    std::free(*meta_json); *meta_json = nullptr;
                    std::fclose(fp);
                    return HIO_ERR_FILE;
                }
                model->snr_phot = scalars[0];
                model->median_snr = scalars[1];
                model->idw_power = scalars[2];

                *snr_model = model;
                fprintf(stderr, "[hio] hiss_read_snr_model: 读取稀疏模型 n_points=%u snr_phot=%.4f median=%.4f power=%.2f\n",
                        nPoints, model->snr_phot, model->median_snr, model->idw_power);
            }
        } else {
            // snr_format=0: 逐像素格式, 本函数不读取 (跳过)
            size_t snrBytes = (size_t)(*n_pix) * sizeof(float);
            fprintf(stderr, "[hio] hiss_read_snr_model: snr_format=0, 跳过逐像素 snr %zu 字节\n", snrBytes);
            if (std::fseek(fp, (long)snrBytes, SEEK_CUR) != 0) {
                std::free(*pixel); *pixel = nullptr;
                std::free(*ipix); *ipix = nullptr;
                std::free(*meta_json); *meta_json = nullptr;
                std::fclose(fp);
                return HIO_ERR_FILE;
            }
        }
    }

    std::fclose(fp);
    fprintf(stderr, "[hio] hiss_read_snr_model: 读取完成: %s\n", path);
    return HIO_OK;
}

// ============================================================================
// 释放 HioSnrModel
// ============================================================================

AIO_EXPORT void aio_hio_free_snr_model(HioSnrModel* model) {
    if (!model) return;
    if (model->points) {
        std::free(model->points);
        model->points = nullptr;
    }
    model->n_points = 0;
    std::free(model);
}
