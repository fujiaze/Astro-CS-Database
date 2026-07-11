#include "hp_lod_api.h"
#include "lod_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// C API 导出层实现
//
// 将 LodManager 的 C++ 接口包装为 extern "C" 函数, 供 Python ctypes 调用
// ============================================================================

// UTF-8 路径文件打开辅助 (Windows 下支持中文路径)
static FILE* fopenUtf8Api(const std::string& path, const char* mode) {
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path.c_str(), mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wpath_len);
    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    if (wmode_len <= 0) return std::fopen(path.c_str(), mode);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// 模块级单例 (线程安全: 每次调用创建临时实例, 避免跨线程状态)
// 由于 LOD 操作通常批量执行, 临时创建开销可忽略

// --------------------------------------------------------------------------
// hp_lod_generate_full - 生成完整 LOD 金字塔
// --------------------------------------------------------------------------
LOD_EXPORT int hp_lod_generate_full(const char* dbPath, int bandIndex) {
    if (!dbPath) {
        fprintf(stderr, "[lod][api] hp_lod_generate_full: dbPath 为空\n");
        return -1;
    }

    lod::LodManager manager;
    return manager.generateFull(std::string(dbPath), bandIndex);
}

// --------------------------------------------------------------------------
// hp_lod_update_incremental - 增量更新
// changedTilesJson: JSON 数组 [tileIpix, ...]
// --------------------------------------------------------------------------
LOD_EXPORT int hp_lod_update_incremental(const char* dbPath, int bandIndex,
                                          const char* changedTilesJson) {
    if (!dbPath || !changedTilesJson) {
        fprintf(stderr, "[lod][api] hp_lod_update_incremental: 参数为空\n");
        return -1;
    }

    // 解析 JSON 数组: [tileIpix, tileIpix, ...]
    std::string jsonStr(changedTilesJson);
    std::vector<int64_t> changedTiles;

    // 简单 JSON 数组解析
    size_t pos = jsonStr.find('[');
    if (pos == std::string::npos) {
        fprintf(stderr, "[lod][api] changedTilesJson 不是有效数组\n");
        return -1;
    }
    pos++;
    while (pos < jsonStr.size() && jsonStr[pos] != ']') {
        // 跳过空白和逗号
        while (pos < jsonStr.size() &&
               (jsonStr[pos] == ' ' || jsonStr[pos] == ',' ||
                jsonStr[pos] == '\n' || jsonStr[pos] == '\t')) {
            pos++;
        }
        if (pos >= jsonStr.size() || jsonStr[pos] == ']') break;

        // 解析整数
        char* end = nullptr;
        long long v = std::strtoll(jsonStr.c_str() + pos, &end, 10);
        if (end == jsonStr.c_str() + pos) {
            pos++;
            continue;
        }
        changedTiles.push_back((int64_t)v);
        pos = (size_t)(end - jsonStr.c_str());
    }

    fprintf(stderr, "[lod][api] 增量更新: %zu 个 tile\n", changedTiles.size());

    lod::LodManager manager;
    return manager.updateIncremental(std::string(dbPath), bandIndex, changedTiles);
}

// --------------------------------------------------------------------------
// hp_lod_compute_on_demand - 按需计算
// 返回 JSON: {"nside":..,"tileIpix":..,"pixels":[..],"values":[..],"weights":[..],"counts":[..]}
// --------------------------------------------------------------------------
LOD_EXPORT char* hp_lod_compute_on_demand(const char* dbPath, int bandIndex,
                                            int level, int64_t tileIpix) {
    if (!dbPath) {
        fprintf(stderr, "[lod][api] hp_lod_compute_on_demand: dbPath 为空\n");
        return nullptr;
    }

    lod::LodManager manager;
    lod::LodTileData* tileData = manager.computeOnDemand(
        std::string(dbPath), bandIndex, level, tileIpix);

    if (!tileData) {
        return nullptr;
    }

    // 构造 JSON 字符串
    // 格式: {"nside":N,"tileIpix":T,"pixelCount":C,
    //        "pixels":[p0,p1,...],"values":[v0,v1,...],
    //        "weights":[w0,w1,...],"counts":[c0,c1,...]}

    std::string json;
    json.reserve(1024 + tileData->pixels.size() * 40);

    // 头部
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"nside\":%d,\"tileIpix\":%lld,\"pixelCount\":%zu,",
        tileData->nside, (long long)tileData->tileIpix, tileData->pixels.size());
    json += buf;

    // pixels 数组
    json += "\"pixels\":[";
    for (size_t i = 0; i < tileData->pixels.size(); i++) {
        if (i > 0) json += ",";
        std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)tileData->pixels[i]);
        json += buf;
    }
    json += "],";

    // values 数组
    json += "\"values\":[";
    for (size_t i = 0; i < tileData->values.size(); i++) {
        if (i > 0) json += ",";
        // 用 %.7g 保证 float 精度, 同时避免过长
        std::snprintf(buf, sizeof(buf), "%.7g", (double)tileData->values[i]);
        json += buf;
    }
    json += "],";

    // weights 数组
    json += "\"weights\":[";
    for (size_t i = 0; i < tileData->weights.size(); i++) {
        if (i > 0) json += ",";
        std::snprintf(buf, sizeof(buf), "%.7g", (double)tileData->weights[i]);
        json += buf;
    }
    json += "],";

    // counts 数组
    json += "\"counts\":[";
    for (size_t i = 0; i < tileData->counts.size(); i++) {
        if (i > 0) json += ",";
        std::snprintf(buf, sizeof(buf), "%u", (unsigned)tileData->counts[i]);
        json += buf;
    }
    json += "]}";

    // 释放 LodTileData
    delete tileData;

    // 复制到 malloc 内存 (供 C 调用方释放)
    char* result = (char*)std::malloc(json.size() + 1);
    if (result) {
        std::memcpy(result, json.c_str(), json.size() + 1);
    }

    fprintf(stderr, "[lod][api] 按需计算返回: %zu 字节 JSON\n", json.size());
    return result;
}

// --------------------------------------------------------------------------
// hp_lod_free_string - 释放 API 返回的字符串
// --------------------------------------------------------------------------
LOD_EXPORT void hp_lod_free_string(char* str) {
    if (str) {
        std::free(str);
    }
}

// --------------------------------------------------------------------------
// hp_lod_get_level_count - 获取 LOD 层级数
// --------------------------------------------------------------------------
LOD_EXPORT int hp_lod_get_level_count(const char* dbPath) {
    if (!dbPath) {
        fprintf(stderr, "[lod][api] hp_lod_get_level_count: dbPath 为空\n");
        return -1;
    }

    // 读取 meta.json 获取 nsideData, 初始化默认层级
    lod::LodManager manager;

    // 尝试读取配置以获取正确的 nsideData
    // LodManager 的 loadDbConfig 是 private, 但 initDefaultLevels 已在构造时调用
    // 如果需要从 meta.json 读取, 可以通过 generateFull 间接触发
    // 这里简化处理: 直接返回默认层级数
    // (实际使用时, Python 端应先调用 generateFull 或读取 meta.json)

    // 读取 meta.json 以获取实际 nsideData
    std::string metaPath = std::string(dbPath) + "/meta.json";
    FILE* fp = fopenUtf8Api(metaPath, "rb");

    if (fp) {
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (sz > 0) {
            std::string jsonStr((size_t)sz, '\0');
            std::fread(&jsonStr[0], 1, (size_t)sz, fp);
            std::fclose(fp);

            // 解析 nsideData
            std::string pat = "\"nsideData\"";
            size_t pos = jsonStr.find(pat);
            if (pos != std::string::npos) {
                pos = jsonStr.find(':', pos);
                if (pos != std::string::npos) {
                    pos++;
                    char* end = nullptr;
                    double v = std::strtod(jsonStr.c_str() + pos, &end);
                    if (end != jsonStr.c_str() + pos) {
                        manager.setLevels({});
                        // 重新初始化
                        // 使用公开接口: setLevels
                        std::vector<lod::LodLevel> levels;
                        int nsideData = (int)v;
                        levels.push_back({nsideData / 64, lod::COMP_LZ4});
                        levels.push_back({nsideData / 16, lod::COMP_LZ4});
                        levels.push_back({nsideData / 4, lod::COMP_ZSTD_3});
                        levels.push_back({nsideData, lod::COMP_ZSTD_5});
                        manager.setLevels(levels);
                    }
                }
            }
        } else {
            std::fclose(fp);
        }
    }

    return (int)manager.getLevels().size();
}
