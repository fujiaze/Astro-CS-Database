#include "hp_stack_api.h"
#include "stack_db.h"
#include "stack_engine.h"
#include "ahps_reader.h"
#include "ahps_writer.h"
#include "ahps_format.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ============================================================================
// 简单 JSON 解析辅助 (避免外部依赖)
// ============================================================================

size_t skipWs(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
        else break;
    }
    return pos;
}

// 提取字符串值 (pos 指向 '"')
std::string extractStr(const std::string& s, size_t& pos) {
    std::string r;
    if (pos >= s.size() || s[pos] != '"') return r;
    pos++;
    while (pos < s.size()) {
        char c = s[pos];
        if (c == '\\' && pos + 1 < s.size()) {
            char n = s[pos + 1];
            switch (n) {
                case 'n': r += '\n'; break;
                case 't': r += '\t'; break;
                case 'r': r += '\r'; break;
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                default: r += n; break;
            }
            pos += 2;
        } else if (c == '"') {
            pos++;
            break;
        } else {
            r += c;
            pos++;
        }
    }
    return r;
}

// 提取数字 (pos 指向数字起始)
double extractNum(const std::string& s, size_t& pos) {
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size()) {
        char c = s[pos];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') pos++;
        else break;
    }
    return std::strtod(s.c_str() + start, nullptr);
}

// 查找 "key": 后的值位置
size_t findKey(const std::string& s, const std::string& key, size_t start = 0) {
    std::string pat = "\"" + key + "\"";
    size_t pos = s.find(pat, start);
    if (pos == std::string::npos) return std::string::npos;
    pos += pat.size();
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != ':') return std::string::npos;
    pos++;
    return skipWs(s, pos);
}

// 跳过一个 JSON 值 (对象/数组/字符串/数字), 返回值结束后的位置
size_t skipValue(const std::string& s, size_t pos) {
    pos = skipWs(s, pos);
    if (pos >= s.size()) return pos;
    char c = s[pos];
    if (c == '"') {
        // 字符串
        extractStr(s, pos);
        return pos;
    }
    if (c == '{' || c == '[') {
        char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        while (pos < s.size()) {
            char ch = s[pos];
            if (ch == '"') { extractStr(s, pos); continue; }
            if (ch == open) depth++;
            else if (ch == close) { depth--; if (depth == 0) { pos++; break; } }
            pos++;
        }
        return pos;
    }
    // 数字 / true / false / null
    while (pos < s.size()) {
        char ch = s[pos];
        if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') break;
        pos++;
    }
    return pos;
}

// 从 DrizzlePixel 对象字符串解析
ahps::DrizzlePixel parseDrizzlePixel(const std::string& objStr) {
    ahps::DrizzlePixel dp;
    dp.healpixPix = 0; dp.value = 0; dp.snr = 0; dp.weight = 1.0f;

    size_t p = findKey(objStr, "healpixPix");
    if (p != std::string::npos) {
        size_t pp = p;
        dp.healpixPix = (uint64_t)extractNum(objStr, pp);
    }
    p = findKey(objStr, "value");
    if (p != std::string::npos) { size_t pp = p; dp.value = (float)extractNum(objStr, pp); }
    p = findKey(objStr, "snr");
    if (p != std::string::npos) { size_t pp = p; dp.snr = (float)extractNum(objStr, pp); }
    p = findKey(objStr, "weight");
    if (p != std::string::npos) { size_t pp = p; dp.weight = (float)extractNum(objStr, pp); }
    return dp;
}

// 从 pixels 数组字符串解析多个 DrizzlePixel
std::vector<ahps::DrizzlePixel> parsePixelArray(const std::string& s, size_t arrPos) {
    std::vector<ahps::DrizzlePixel> result;
    if (arrPos >= s.size() || s[arrPos] != '[') return result;
    size_t pos = arrPos + 1;
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != ']') {
        if (s[pos] == '{') {
            // 匹配 {...}
            int depth = 0;
            size_t start = pos;
            while (pos < s.size()) {
                char c = s[pos];
                if (c == '"') { extractStr(s, pos); continue; }
                if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) { pos++; break; } }
                pos++;
            }
            std::string objStr = s.substr(start, pos - start);
            result.push_back(parseDrizzlePixel(objStr));
        } else {
            pos++;
        }
        pos = skipWs(s, pos);
        if (pos < s.size() && s[pos] == ',') { pos++; pos = skipWs(s, pos); }
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// C API 实现
// ============================================================================

// ---- 数据库管理 ----

HP_STACK_EXPORT StackDatabase* hp_stack_db_create(const char* dbPath, const char* configJson) {
    if (!dbPath) return nullptr;
    ahps::StackDbConfig config;
    if (configJson) {
        std::string json(configJson);
        double v;
        if (findKey(json, "nsideData") != std::string::npos) {
            size_t p = findKey(json, "nsideData");
            config.nsideData = (int)extractNum(json, p);
        }
        if (findKey(json, "tileNside") != std::string::npos) {
            size_t p = findKey(json, "tileNside");
            config.tileNside = (int)extractNum(json, p);
        }
        if (findKey(json, "sigmaClipLow") != std::string::npos) {
            size_t p = findKey(json, "sigmaClipLow");
            config.sigmaClipLow = extractNum(json, p);
        }
        if (findKey(json, "sigmaClipHigh") != std::string::npos) {
            size_t p = findKey(json, "sigmaClipHigh");
            config.sigmaClipHigh = extractNum(json, p);
        }
        if (findKey(json, "nested") != std::string::npos) {
            size_t p = findKey(json, "nested");
            // true/false
            if (p + 4 <= json.size() && std::strncmp(json.c_str() + p, "true", 4) == 0)
                config.nested = true;
            else if (p + 5 <= json.size() && std::strncmp(json.c_str() + p, "false", 5) == 0)
                config.nested = false;
        }
        // bands 数组
        size_t bp = findKey(json, "bands");
        if (bp != std::string::npos && bp < json.size() && json[bp] == '[') {
            config.bands.clear();
            size_t pos = bp + 1;
            pos = skipWs(json, pos);
            while (pos < json.size() && json[pos] != ']') {
                if (json[pos] == '"') {
                    std::string name = extractStr(json, pos);
                    if (!name.empty()) config.bands.push_back(name);
                } else {
                    pos++;
                }
                pos = skipWs(json, pos);
                if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
            }
        }
        // nsideLod 数组
        size_t lp = findKey(json, "nsideLod");
        if (lp != std::string::npos && lp < json.size() && json[lp] == '[') {
            config.nsideLod.clear();
            size_t pos = lp + 1;
            pos = skipWs(json, pos);
            while (pos < json.size() && json[pos] != ']') {
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) pos++;
                if (pos >= json.size() || json[pos] == ']') break;
                config.nsideLod.push_back((int)extractNum(json, pos));
                while (pos < json.size() && json[pos] != ',' && json[pos] != ']') pos++;
            }
        }
    }
    ahps::StackDatabase* db = ahps::StackDatabase::create(dbPath, config);
    return reinterpret_cast<StackDatabase*>(db);
}

HP_STACK_EXPORT StackDatabase* hp_stack_db_open(const char* dbPath) {
    if (!dbPath) return nullptr;
    ahps::StackDatabase* db = ahps::StackDatabase::open(dbPath);
    return reinterpret_cast<StackDatabase*>(db);
}

HP_STACK_EXPORT void hp_stack_db_close(StackDatabase* db) {
    if (!db) return;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    delete d;
}

// ---- 堆栈更新 ----

HP_STACK_EXPORT int hp_stack_update_global(StackDatabase* db, const char* framesJson) {
    if (!db || !framesJson) return -1;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    std::string json(framesJson);

    // 解析 JSON 数组: [{"pixels":[...]}, {"pixels":[...]}, ...]
    std::vector<std::vector<ahps::DrizzlePixel>> frameData;

    size_t pos = skipWs(json, 0);
    if (pos >= json.size() || json[pos] != '[') {
        fprintf(stderr, "[hp_stack] update_global: framesJson 不是数组\n");
        return -1;
    }
    pos++;
    pos = skipWs(json, pos);
    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] == '{') {
            int depth = 0;
            size_t start = pos;
            while (pos < json.size()) {
                char c = json[pos];
                if (c == '"') { extractStr(json, pos); continue; }
                if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) { pos++; break; } }
                pos++;
            }
            std::string frameObj = json.substr(start, pos - start);
            // 解析 pixels 数组
            size_t pp = findKey(frameObj, "pixels");
            if (pp != std::string::npos) {
                auto pixels = parsePixelArray(frameObj, pp);
                frameData.push_back(std::move(pixels));
            }
        } else {
            pos++;
        }
        pos = skipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
    }

    ahps::StackEngine engine(d->getConfig());
    return engine.updateGlobal(d, frameData);
}

HP_STACK_EXPORT int hp_stack_update_range(StackDatabase* db, const char* fileRangeJson) {
    if (!db || !fileRangeJson) return -1;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    std::string json(fileRangeJson);

    // 解析 JSON 对象: {"filepath1":[pixels...], "filepath2":[pixels...], ...}
    std::map<std::string, std::vector<ahps::DrizzlePixel>> fileRange;

    size_t pos = skipWs(json, 0);
    if (pos >= json.size() || json[pos] != '{') {
        fprintf(stderr, "[hp_stack] update_range: fileRangeJson 不是对象\n");
        return -1;
    }
    pos++;
    pos = skipWs(json, pos);
    while (pos < json.size() && json[pos] != '}') {
        // key (文件路径字符串)
        if (json[pos] != '"') { pos++; continue; }
        std::string key = extractStr(json, pos);
        pos = skipWs(json, pos);
        if (pos >= json.size() || json[pos] != ':') { pos++; continue; }
        pos++;
        pos = skipWs(json, pos);
        // value (pixels 数组)
        if (pos < json.size() && json[pos] == '[') {
            auto pixels = parsePixelArray(json, pos);
            fileRange[key] = std::move(pixels);
            pos = skipValue(json, pos);
        } else {
            pos = skipValue(json, pos);
        }
        pos = skipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
    }

    ahps::StackEngine engine(d->getConfig());
    return engine.updateRange(d, fileRange);
}

// ---- 读取堆栈数据 ----

HP_STACK_EXPORT char* hp_stack_read_tile(StackDatabase* db, int64_t tileIpix) {
    if (!db) return nullptr;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);

    ahps::AhpsReader* reader = d->openTileReader(tileIpix);
    if (!reader) return nullptr;

    auto pixels = reader->readPixelIndices();
    int bandCount = reader->getBandCount();
    int64_t pixCount = reader->getPixelCount();

    // 构建 JSON
    std::string json = "{";
    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
        "\"tileIpix\":%lld,\"nside\":%d,\"tileNside\":%d,\"pixelCount\":%lld,\"bandCount\":%d,",
        (long long)tileIpix, reader->getNside(), reader->getTileNside(),
        (long long)pixCount, bandCount);
    json += hdr;

    // pixels 数组
    json += "\"pixels\":[";
    for (size_t i = 0; i < pixels.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(pixels[i]);
    }
    json += "],";

    // bands 数组
    json += "\"bands\":[";
    for (int b = 0; b < bandCount; b++) {
        if (b > 0) json += ",";
        std::vector<float> values, variance;
        auto stats = reader->readBandStats(b);
        json += "{\"values\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            float val = (stats[i].weightSum > 0.0f) ? (stats[i].sum / stats[i].weightSum) : 0.0f;
            json += std::to_string(val);
        }
        json += "],\"variance\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            float var = 0.0f;
            if (stats[i].weightSum > 0.0f) {
                float val = stats[i].sum / stats[i].weightSum;
                float e2 = stats[i].sumSq / stats[i].weightSum;
                var = e2 - val * val;
                if (var < 0.0f) var = 0.0f;
            }
            json += std::to_string(var);
        }
        json += "],\"counts\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            json += std::to_string((int)stats[i].count);
        }
        json += "]}";
    }
    json += "]}";

    delete reader;

    // malloc 分配并拷贝
    char* out = (char*)std::malloc(json.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, json.c_str(), json.size() + 1);
    return out;
}

HP_STACK_EXPORT void hp_stack_free_string(char* str) {
    if (str) std::free(str);
}

// ---- HEALpix 工具函数 ----

HP_STACK_EXPORT int64_t hp_radec2pix(int nside, int nested, double ra_deg, double dec_deg) {
    healpix::HealpixCore core(nside, nested != 0);
    return core.radec2pix(ra_deg, dec_deg);
}

HP_STACK_EXPORT void hp_pix2radec(int nside, int nested, int64_t ipix,
                                  double* ra_deg, double* dec_deg) {
    healpix::HealpixCore core(nside, nested != 0);
    double ra, dec;
    core.pix2radec(ipix, &ra, &dec);
    if (ra_deg) *ra_deg = ra;
    if (dec_deg) *dec_deg = dec;
}

HP_STACK_EXPORT double hp_pixel_resolution_arcsec(int nside) {
    healpix::HealpixCore core(nside, true);
    return core.pixelResolutionArcsec();
}
