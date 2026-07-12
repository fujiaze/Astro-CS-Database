#include "lod_manager.h"
#include "astro_image_io.h"
#include "../healpix_stack/ahps_reader.h"
#include "../healpix_stack/healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

namespace lod {

// ============================================================================
// 平台辅助函数 (复用 stack_db.cpp 的模式)
// ============================================================================

// UTF-8 路径转 wstring (Windows)
static std::wstring utf8ToWstring(const std::string& s) {
    if (s.empty()) return std::wstring();
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    if (len > 0) ws.resize(len - 1);
    return ws;
#else
    return std::wstring(s.begin(), s.end());
#endif
}

// 创建目录 (递归)
static bool makeDir(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    std::wstring wpath = utf8ToWstring(path);
    std::wstring cur;
    for (size_t i = 0; i < wpath.size(); i++) {
        wchar_t c = wpath[i];
        cur += c;
        if (c == L'\\' || c == L'/' || i == wpath.size() - 1) {
            DWORD attr = GetFileAttributesW(cur.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES) {
                CreateDirectoryW(cur.c_str(), nullptr);
            }
        }
    }
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    std::string cmd = "mkdir -p \"" + path + "\"";
    return (std::system(cmd.c_str()) == 0);
#endif
}

// 判断路径是否存在
static bool pathExists(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = utf8ToWstring(path);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
#endif
}

// UTF-8 路径文件打开
static FILE* fopenUtf8(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::wstring wpath = utf8ToWstring(path);
    std::wstring wmode(mode, mode + std::strlen(mode));
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// ============================================================================
// 简单 JSON 解析 (复用 stack_db.cpp 的模式)
// ============================================================================

static bool jsonGetNumber(const std::string& json, const std::string& key, double* out) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\t'||json[pos]=='\n')) pos++;
    char* end = nullptr;
    double v = std::strtod(json.c_str() + pos, &end);
    if (end == json.c_str() + pos) return false;
    *out = v;
    return true;
}

static bool jsonGetBool(const std::string& json, const std::string& key, bool* out) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\t')) pos++;
    if (pos + 4 <= json.size() && std::strncmp(json.c_str()+pos, "true", 4) == 0) { *out = true; return true; }
    if (pos + 5 <= json.size() && std::strncmp(json.c_str()+pos, "false", 5) == 0) { *out = false; return true; }
    return false;
}

static std::vector<std::string> jsonGetStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    pos++;
    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] == '"') {
            size_t start = pos + 1;
            size_t end = start;
            while (end < json.size() && json[end] != '"') {
                if (json[end] == '\\' && end + 1 < json.size()) end += 2;
                else end++;
            }
            result.push_back(json.substr(start, end - start));
            pos = end + 1;
        } else {
            pos++;
        }
        while (pos < json.size() && json[pos] != ']' && json[pos] != '"') pos++;
    }
    return result;
}

// ============================================================================
// LodManager 实现
// ============================================================================

LodManager::LodManager() {
    // 默认初始化 4 级 LOD (假设数据层 nside=32768)
    initDefaultLevels(32768);
}

LodManager::~LodManager() {
}

// --------------------------------------------------------------------------
// initDefaultLevels - 初始化默认 LOD 层级
//
// 默认 4 级 (数据层 nside=32768):
//   Level 0: nside=512   (lz4, 快速加载)
//   Level 1: nside=2048  (lz4, 导航层)
//   Level 2: nside=8192  (zstd level 3, 中精度)
//   Level 3: nside=32768 (zstd level 5, 数据层)
// --------------------------------------------------------------------------
void LodManager::initDefaultLevels(int dataNside) {
    m_levels.clear();

    // 从数据层 nside 递推 4 级 (每级 nside /= 4)
    // Level 3 = 数据层, Level 2 = dataNside/4, Level 1 = dataNside/16, Level 0 = dataNside/64
    int nside3 = dataNside;           // 数据层
    int nside2 = dataNside / 4;       // 中精度
    int nside1 = dataNside / 16;      // 导航层
    int nside0 = dataNside / 64;      // 全景层

    m_levels.push_back({nside0, COMP_LZ4});      // Level 0: 快速加载
    m_levels.push_back({nside1, COMP_LZ4});      // Level 1: 快速加载
    m_levels.push_back({nside2, COMP_ZSTD_3});   // Level 2: 中等压缩
    m_levels.push_back({nside3, COMP_ZSTD_5});   // Level 3: 高压缩 (数据层)
}

const std::vector<LodLevel>& LodManager::getLevels() const {
    return m_levels;
}

void LodManager::setLevels(const std::vector<LodLevel>& levels) {
    m_levels = levels;
}

// --------------------------------------------------------------------------
// compToCodec - 压缩级别 → codec + zstd level
// --------------------------------------------------------------------------
void LodManager::compToCodec(int comp, uint8_t& codec, int& zstdLevel) const {
    switch (comp) {
        case COMP_LZ4:
            codec = CODEC_LZ4;
            zstdLevel = 0;
            break;
        case COMP_ZSTD_3:
            codec = CODEC_ZSTD;
            zstdLevel = 3;
            break;
        case COMP_ZSTD_5:
            codec = CODEC_ZSTD;
            zstdLevel = 5;
            break;
        default:
            codec = CODEC_NONE;
            zstdLevel = 0;
            break;
    }
}

// --------------------------------------------------------------------------
// loadDbConfig - 从 meta.json 读取数据库配置
// --------------------------------------------------------------------------
bool LodManager::loadDbConfig(const std::string& dbPath, DbConfig& config) {
    std::string metaPath = dbPath + "/meta.json";
    FILE* fp = fopenUtf8(metaPath, "rb");
    if (!fp) {
        fprintf(stderr, "[lod][manager] 无法读取 meta.json: %s\n", metaPath.c_str());
        return false;
    }

    // 读取全部内容
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(fp);
        fprintf(stderr, "[lod][manager] meta.json 为空: %s\n", metaPath.c_str());
        return false;
    }
    std::string json((size_t)sz, '\0');
    std::fread(&json[0], 1, (size_t)sz, fp);
    std::fclose(fp);

    // 解析字段
    double v;
    if (jsonGetNumber(json, "nsideData", &v)) config.nsideData = (int)v;
    if (jsonGetNumber(json, "tileNside", &v)) config.tileNside = (int)v;
    bool b;
    if (jsonGetBool(json, "nested", &b)) config.nested = b;
    config.bands = jsonGetStringArray(json, "bands");

    // 默认值
    if (config.nsideData <= 0) config.nsideData = 32768;
    if (config.tileNside <= 0) config.tileNside = 512;
    if (config.bands.empty())  config.bands = {"L", "R", "G", "B", "Ha", "OIII"};

    fprintf(stderr, "[lod][manager] 数据库配置: nsideData=%d tileNside=%d bands=%zu nested=%d\n",
            config.nsideData, config.tileNside, config.bands.size(), config.nested ? 1 : 0);

    return true;
}

// --------------------------------------------------------------------------
// lodTilePath - LOD tile 文件路径
//   {dbPath}/tiles/nside_{N}/tile_{ipix}_b{B}.ahpl
// --------------------------------------------------------------------------
std::string LodManager::lodTilePath(const std::string& dbPath, int nside,
                                     int64_t tileIpix, int bandIndex) const {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "nside_%d", nside);
    std::string dir = dbPath + "/tiles/" + std::string(buf);
    std::snprintf(buf, sizeof(buf), "tile_%lld_b%d.ahpl",
                  (long long)tileIpix, bandIndex);
    return dir + "/" + std::string(buf);
}

// --------------------------------------------------------------------------
// listDataTiles - 扫描数据层 tile 文件列表
//   扫描 {dbPath}/tiles/nside_{nsideData}/ 目录下的 tile_*.ahps 文件
// --------------------------------------------------------------------------
std::vector<int64_t> LodManager::listDataTiles(const std::string& dbPath,
                                                const DbConfig& config) const {
    std::vector<int64_t> result;

    char nsBuf[64];
    std::snprintf(nsBuf, sizeof(nsBuf), "nside_%d", config.nsideData);
    std::string dir = dbPath + "/tiles/" + nsBuf;

    if (!pathExists(dir)) {
        fprintf(stderr, "[lod][manager] 数据层目录不存在: %s\n", dir.c_str());
        return result;
    }

#ifdef _WIN32
    // Windows: 用 FindFirstFileW 扫描
    std::wstring pattern = utf8ToWstring(dir + "/tile_*.ahps");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[lod][manager] 无数据层 tile 文件: %s\n", dir.c_str());
        return result;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        // 从文件名提取 tileIpix: tile_{ipix}.ahps
        int len = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nullptr, 0, nullptr, nullptr);
        std::string fname(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, &fname[0], len, nullptr, nullptr);
        if (len > 0) fname.resize(len - 1);

        // 解析 tileIpix
        if (fname.substr(0, 5) == "tile_") {
            size_t dotPos = fname.find('.');
            if (dotPos != std::string::npos) {
                std::string numStr = fname.substr(5, dotPos - 5);
                char* end = nullptr;
                long long ipix = std::strtoll(numStr.c_str(), &end, 10);
                if (end != numStr.c_str()) {
                    result.push_back((int64_t)ipix);
                }
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    // POSIX: 用 opendir/scandir (简化实现)
    DIR* d = opendir(dir.c_str());
    if (!d) return result;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string fname(ent->d_name);
        if (fname.substr(0, 5) == "tile_") {
            size_t dotPos = fname.find('.');
            if (dotPos != std::string::npos) {
                std::string numStr = fname.substr(5, dotPos - 5);
                char* end = nullptr;
                long long ipix = std::strtoll(numStr.c_str(), &end, 10);
                if (end != numStr.c_str()) {
                    result.push_back((int64_t)ipix);
                }
            }
        }
    }
    closedir(d);
#endif

    // 排序
    std::sort(result.begin(), result.end());

    fprintf(stderr, "[lod][manager] 发现 %zu 个数据层 tile\n", result.size());
    return result;
}

// --------------------------------------------------------------------------
// readLevelData - 读取某层数据
//
// nside == config.nsideData: 读取数据层 .ahps (使用 AhpsReader)
// nside != config.nsideData: 读取 LOD 层 .ahpl (使用自定义读取)
//
// 返回 FinePixel 列表: ipix = 全局像素号, value = sum/weightSum, weight = weightSum
// --------------------------------------------------------------------------
std::vector<FinePixel> LodManager::readLevelData(
    const std::string& dbPath,
    int bandIndex, int nside,
    int64_t tileIpix,
    const DbConfig& config
) {
    std::vector<FinePixel> result;

    if (nside == config.nsideData) {
        // ---- 读取数据层 .ahps ----
        char nsBuf[64];
        std::snprintf(nsBuf, sizeof(nsBuf), "nside_%d", config.nsideData);
        char tileBuf[128];
        std::snprintf(tileBuf, sizeof(tileBuf), "tile_%lld.ahps", (long long)tileIpix);
        std::string tilePath = dbPath + "/tiles/" + nsBuf + "/" + tileBuf;

        if (!pathExists(tilePath)) {
            // tile 不存在, 返回空 (该区域无数据)
            return result;
        }

        // 使用 AhpsReader 读取
        ahps::AhpsReader reader;
        if (!reader.open(tilePath)) {
            fprintf(stderr, "[lod][manager] 无法打开数据层 tile: %s\n", tilePath.c_str());
            return result;
        }

        // 校验 bandIndex
        if (bandIndex < 0 || bandIndex >= reader.getBandCount()) {
            fprintf(stderr, "[lod][manager] bandIndex %d 超出范围 (bandCount=%d)\n",
                    bandIndex, reader.getBandCount());
            reader.close();
            return result;
        }

        // 读取像素索引
        std::vector<uint64_t> indices = reader.readPixelIndices();
        if (indices.empty()) {
            reader.close();
            return result;
        }

        // 读取波段统计量
        std::vector<ahps::PixelStats> stats = reader.readBandStats(bandIndex);
        if (stats.size() != indices.size()) {
            fprintf(stderr, "[lod][manager] 像素索引数(%zu) != 统计量数(%zu)\n",
                    indices.size(), stats.size());
            reader.close();
            return result;
        }

        // 转换为 FinePixel
        result.reserve(indices.size());
        for (size_t i = 0; i < indices.size(); i++) {
            FinePixel fp;
            fp.ipix = indices[i];
            if (stats[i].weightSum > 0.0f) {
                fp.value  = stats[i].sum / stats[i].weightSum;
                fp.weight = stats[i].weightSum;
            } else {
                fp.value  = 0.0f;
                fp.weight = 0.0f;
            }
            result.push_back(fp);
        }

        reader.close();

        fprintf(stderr, "[lod][manager] 读取数据层 tile: ipix=%lld 像素数=%zu\n",
                (long long)tileIpix, result.size());

    } else {
        // ---- 读取 LOD 层 .ahpl ----
        std::vector<CoarsePixel> coarseData;
        if (!readLodTile(dbPath, bandIndex, nside, tileIpix, coarseData, config)) {
            return result;
        }

        // 转换 CoarsePixel → FinePixel (用于下一级降采样)
        result.reserve(coarseData.size());
        for (const auto& cp : coarseData) {
            FinePixel fp;
            fp.ipix   = cp.ipix;
            fp.value  = cp.value;
            fp.weight = cp.weight;
            result.push_back(fp);
        }

        fprintf(stderr, "[lod][manager] 读取 LOD 层 tile: nside=%d ipix=%lld 像素数=%zu\n",
                nside, (long long)tileIpix, result.size());
    }

    return result;
}

// --------------------------------------------------------------------------
// writeLevelData - 写入降采样结果到 LOD 层 (.ahpl)
//
// .ahpl 文件格式:
//   [固定头 34B][压缩数据块]
//
// 数据块 (压缩前):
//   像素索引: PixelCount * uint64
//   数值:     PixelCount * float
//   权重:     PixelCount * float
//   计数:     PixelCount * uint16
// --------------------------------------------------------------------------
int LodManager::writeLevelData(
    const std::string& dbPath,
    int bandIndex,
    int nside, int64_t tileIpix,
    const std::vector<CoarsePixel>& data,
    const DbConfig& config
) {
    // 构造文件路径
    std::string filePath = lodTilePath(dbPath, nside, tileIpix, bandIndex);

    // 确保目录存在
    std::string dir = dbPath + "/tiles/nside_" + std::to_string(nside);
    if (!pathExists(dir)) {
        if (!makeDir(dir)) {
            fprintf(stderr, "[lod][manager] 无法创建目录: %s\n", dir.c_str());
            return -1;
        }
    }

    // 打开文件 (二进制写)
    FILE* fp = fopenUtf8(filePath, "wb");
    if (!fp) {
        fprintf(stderr, "[lod][manager] 无法创建文件: %s\n", filePath.c_str());
        return -1;
    }

    // 查找该层的压缩配置
    uint8_t codec = CODEC_NONE;
    int zstdLevel = 0;
    for (const auto& lv : m_levels) {
        if (lv.nside == nside) {
            compToCodec(lv.compression, codec, zstdLevel);
            break;
        }
    }

    // ---- 构造原始数据块 ----
    uint32_t pixelCount = (uint32_t)data.size();
    size_t rawSize = pixelCount * PIXEL_RECORD_SIZE;

    std::vector<uint8_t> rawBuf(rawSize);
    size_t offset = 0;

    // 写入像素索引 (uint64)
    for (const auto& cp : data) {
        uint64_t ipix = cp.ipix;
        std::memcpy(&rawBuf[offset], &ipix, 8);
        offset += 8;
    }
    // 写入值 (float)
    for (const auto& cp : data) {
        float val = cp.value;
        std::memcpy(&rawBuf[offset], &val, 4);
        offset += 4;
    }
    // 写入权重 (float)
    for (const auto& cp : data) {
        float w = cp.weight;
        std::memcpy(&rawBuf[offset], &w, 4);
        offset += 4;
    }
    // 写入计数 (uint16)
    for (const auto& cp : data) {
        uint16_t cnt = cp.count;
        std::memcpy(&rawBuf[offset], &cnt, 2);
        offset += 2;
    }

    // ---- 压缩数据块 ----
    std::vector<uint8_t> compBuf;
    size_t compSize = rawSize;
    uint8_t actualCodec = CODEC_NONE;

    if (codec == CODEC_ZSTD) {
        size_t bound = aio_compress_bound(rawSize, 1);
        compBuf.resize(bound);
        compSize = aio_compress(rawBuf.data(), rawSize, compBuf.data(), bound, 1, zstdLevel);
        if (compSize > 0 && compSize < rawSize) {
            actualCodec = CODEC_ZSTD;
        } else {
            actualCodec = CODEC_NONE;
            compSize = rawSize;
        }
    } else if (codec == CODEC_LZ4) {
        size_t bound = aio_compress_bound(rawSize, 2);
        compBuf.resize(bound);
        compSize = aio_compress(rawBuf.data(), rawSize, compBuf.data(), bound, 2, 0);
        if (compSize > 0 && compSize < rawSize) {
            actualCodec = CODEC_LZ4;
        } else {
            actualCodec = CODEC_NONE;
            compSize = rawSize;
        }
    } else {
        // 不压缩
        actualCodec = CODEC_NONE;
        compSize = rawSize;
    }

    // ---- 写入固定头 (34 字节) ----
    // Magic(4) + Version(2) + Nside(4) + TileNside(4) + TileIpix(8) +
    // BandIndex(4) + PixelCount(4) + Compression(1) + Reserved(3)
    uint8_t header[HEADER_FIXED_SIZE];
    size_t hpos = 0;

    std::memcpy(&header[hpos], MAGIC, 4); hpos += 4;
    uint16_t ver = VERSION;
    std::memcpy(&header[hpos], &ver, 2); hpos += 2;
    int32_t nsideVal = nside;
    std::memcpy(&header[hpos], &nsideVal, 4); hpos += 4;
    int32_t tileNs = config.tileNside;
    std::memcpy(&header[hpos], &tileNs, 4); hpos += 4;
    int64_t tileIp = tileIpix;
    std::memcpy(&header[hpos], &tileIp, 8); hpos += 8;
    int32_t bandIdx = bandIndex;
    std::memcpy(&header[hpos], &bandIdx, 4); hpos += 4;
    std::memcpy(&header[hpos], &pixelCount, 4); hpos += 4;
    header[hpos] = actualCodec; hpos += 1;
    header[hpos++] = 0; header[hpos++] = 0; header[hpos++] = 0; // reserved

    std::fwrite(header, 1, HEADER_FIXED_SIZE, fp);

    // ---- 写入数据块 ----
    if (actualCodec == CODEC_NONE) {
        std::fwrite(rawBuf.data(), 1, rawSize, fp);
    } else {
        std::fwrite(compBuf.data(), 1, compSize, fp);
    }

    std::fclose(fp);

    fprintf(stderr, "[lod][manager] 写入 LOD tile: %s (nside=%d ipix=%lld 像素=%u 原始=%zu 压缩=%zu codec=%d)\n",
            filePath.c_str(), nside, (long long)tileIpix, pixelCount,
            rawSize, compSize, (int)actualCodec);

    return 0;
}

// --------------------------------------------------------------------------
// readLodTile - 读取 LOD 层 tile (.ahpl)
// --------------------------------------------------------------------------
bool LodManager::readLodTile(
    const std::string& dbPath,
    int bandIndex,
    int nside, int64_t tileIpix,
    std::vector<CoarsePixel>& outData,
    const DbConfig& config
) {
    outData.clear();

    std::string filePath = lodTilePath(dbPath, nside, tileIpix, bandIndex);
    if (!pathExists(filePath)) {
        return false;  // 文件不存在 (正常情况, 不是错误)
    }

    FILE* fp = fopenUtf8(filePath, "rb");
    if (!fp) {
        fprintf(stderr, "[lod][manager] 无法打开 LOD tile: %s\n", filePath.c_str());
        return false;
    }

    // 读取固定头
    uint8_t header[HEADER_FIXED_SIZE];
    if (std::fread(header, 1, HEADER_FIXED_SIZE, fp) != HEADER_FIXED_SIZE) {
        fprintf(stderr, "[lod][manager] LOD tile 头读取失败: %s\n", filePath.c_str());
        std::fclose(fp);
        return false;
    }

    // 解析头
    size_t hpos = 0;
    char magic[4];
    std::memcpy(magic, &header[hpos], 4); hpos += 4;
    if (std::memcmp(magic, MAGIC, 4) != 0) {
        fprintf(stderr, "[lod][manager] LOD tile Magic 不匹配: %s\n", filePath.c_str());
        std::fclose(fp);
        return false;
    }
    hpos += 2;  // version 跳过
    int32_t fileNside;
    std::memcpy(&fileNside, &header[hpos], 4); hpos += 4;
    hpos += 4;  // tileNside 跳过
    int64_t fileTileIpix;
    std::memcpy(&fileTileIpix, &header[hpos], 8); hpos += 8;
    int32_t fileBand;
    std::memcpy(&fileBand, &header[hpos], 4); hpos += 4;
    uint32_t pixelCount;
    std::memcpy(&pixelCount, &header[hpos], 4); hpos += 4;
    uint8_t codec = header[hpos]; hpos += 1;

    if (fileNside != nside || fileTileIpix != tileIpix || fileBand != bandIndex) {
        fprintf(stderr, "[lod][manager] LOD tile 头不匹配: nside(%d/%d) ipix(%lld/%lld) band(%d/%d)\n",
                fileNside, nside, (long long)fileTileIpix, (long long)tileIpix, fileBand, bandIndex);
        std::fclose(fp);
        return false;
    }

    if (pixelCount == 0) {
        std::fclose(fp);
        return true;  // 空 tile
    }

    // 读取压缩数据块
    size_t rawSize = (size_t)pixelCount * PIXEL_RECORD_SIZE;
    std::vector<uint8_t> rawBuf(rawSize);

    if (codec == CODEC_NONE) {
        if (std::fread(rawBuf.data(), 1, rawSize, fp) != rawSize) {
            fprintf(stderr, "[lod][manager] LOD tile 数据读取失败: %s\n", filePath.c_str());
            std::fclose(fp);
            return false;
        }
    } else {
        // 读取压缩数据 (剩余部分)
        std::fseek(fp, 0, SEEK_END);
        long fileSize = std::ftell(fp);
        long compSize = fileSize - (long)HEADER_FIXED_SIZE;
        std::fseek(fp, (long)HEADER_FIXED_SIZE, SEEK_SET);

        std::vector<uint8_t> compBuf(compSize);
        if (std::fread(compBuf.data(), 1, compSize, fp) != (size_t)compSize) {
            fprintf(stderr, "[lod][manager] 压缩数据读取失败: %s\n", filePath.c_str());
            std::fclose(fp);
            return false;
        }

        if (codec == CODEC_ZSTD) {
            size_t decSize = aio_decompress(compBuf.data(), compSize, rawBuf.data(), rawSize, 1);
            if (decSize != rawSize) {
                fprintf(stderr, "[lod][manager] ZSTD 解压大小不匹配: %zu != %zu\n", decSize, rawSize);
                std::fclose(fp);
                return false;
            }
        } else if (codec == CODEC_LZ4) {
            size_t decSize = aio_decompress(compBuf.data(), compSize, rawBuf.data(), rawSize, 2);
            if (decSize != rawSize) {
                fprintf(stderr, "[lod][manager] LZ4 解压大小不匹配: %zu != %zu\n", decSize, rawSize);
                std::fclose(fp);
                return false;
            }
        }
    }

    std::fclose(fp);

    // 解析像素数据
    outData.resize(pixelCount);
    size_t offset = 0;

    // 读取像素索引
    for (uint32_t i = 0; i < pixelCount; i++) {
        uint64_t ipix;
        std::memcpy(&ipix, &rawBuf[offset], 8);
        offset += 8;
        outData[i].ipix = ipix;
    }
    // 读取值
    for (uint32_t i = 0; i < pixelCount; i++) {
        float val;
        std::memcpy(&val, &rawBuf[offset], 4);
        offset += 4;
        outData[i].value = val;
    }
    // 读取权重
    for (uint32_t i = 0; i < pixelCount; i++) {
        float w;
        std::memcpy(&w, &rawBuf[offset], 4);
        offset += 4;
        outData[i].weight = w;
    }
    // 读取计数
    for (uint32_t i = 0; i < pixelCount; i++) {
        uint16_t cnt;
        std::memcpy(&cnt, &rawBuf[offset], 2);
        offset += 2;
        outData[i].count = cnt;
    }

    return true;
}

// --------------------------------------------------------------------------
// generateFull - 从堆栈数据库生成完整 LOD 金字塔
//
// 流程:
//   1. 读取 meta.json 配置
//   2. 扫描数据层 tile 列表
//   3. 对每个 tile, 从数据层逐级降采样到各 LOD 层
//   4. 使用 OpenMP 并行处理不同 tile
// --------------------------------------------------------------------------
int LodManager::generateFull(const std::string& dbPath, int bandIndex) {
    fprintf(stderr, "[lod][manager] === generateFull 开始: dbPath=%s band=%d ===\n",
            dbPath.c_str(), bandIndex);

    // 1. 读取数据库配置
    DbConfig config;
    if (!loadDbConfig(dbPath, config)) {
        return -1;
    }

    // 校验 bandIndex
    if (bandIndex < 0 || bandIndex >= (int)config.bands.size()) {
        fprintf(stderr, "[lod][manager] bandIndex %d 超出范围 (bands=%zu)\n",
                bandIndex, config.bands.size());
        return -1;
    }

    // 2. 根据数据层 nside 初始化 LOD 层级
    initDefaultLevels(config.nsideData);

    // 3. 扫描数据层 tile
    std::vector<int64_t> tiles = listDataTiles(dbPath, config);
    if (tiles.empty()) {
        fprintf(stderr, "[lod][manager] 数据层无 tile, 无需生成 LOD\n");
        return 0;
    }

    // 4. 逐 tile 生成 LOD 金字塔
    int tileCount = (int)tiles.size();

    fprintf(stderr, "[lod][manager] 开始处理 %d 个 tile (band=%s)\n",
            tileCount, config.bands[bandIndex].c_str());

    #pragma omp parallel for schedule(dynamic)
    for (int t = 0; t < tileCount; t++) {
        int64_t tileIpix = tiles[t];

        // 从最细 LOD 层 (非数据层) 开始, 逐级降采样
        // m_levels 按从粗到细排列: [0]=最粗, [N-1]=最细(数据层)
        // 从最细的非数据层开始, 向粗层逐级降采样
        int finestLevel = (int)m_levels.size() - 1;  // 数据层

        // 读取数据层像素作为最细源
        std::vector<FinePixel> finePixels = readLevelData(
            dbPath, bandIndex, m_levels[finestLevel].nside, tileIpix, config);

        if (finePixels.empty()) {
            // 该 tile 无数据, 跳过
            #pragma omp critical
            {
                fprintf(stderr, "[lod][manager] tile %lld 无数据, 跳过\n", (long long)tileIpix);
            }
            continue;
        }

        // 逐级降采样: 从 finestLevel-1 到 0
        std::vector<FinePixel> currentFine = finePixels;
        int currentNside = m_levels[finestLevel].nside;

        for (int lv = finestLevel - 1; lv >= 0; lv--) {
            int coarseNside = m_levels[lv].nside;

            // 降采样
            std::vector<CoarsePixel> coarsePixels =
                m_downsampler.downsample(currentFine, currentNside, coarseNside);

            if (coarsePixels.empty()) {
                #pragma omp critical
                {
                    fprintf(stderr, "[lod][manager] tile %lld level %d 降采样结果为空\n",
                            (long long)tileIpix, lv);
                }
                break;
            }

            // 写入 LOD tile
            #pragma omp critical
            {
                writeLevelData(dbPath, bandIndex, coarseNside, tileIpix, coarsePixels, config);
            }

            // 为下一级降采样准备: CoarsePixel → FinePixel
            currentFine.clear();
            currentFine.reserve(coarsePixels.size());
            for (const auto& cp : coarsePixels) {
                FinePixel fp;
                fp.ipix   = cp.ipix;
                fp.value  = cp.value;
                fp.weight = cp.weight;
                currentFine.push_back(fp);
            }
            currentNside = coarseNside;
        }
    }

    fprintf(stderr, "[lod][manager] === generateFull 完成: %d 个 tile ===\n", tileCount);
    return 0;
}

// --------------------------------------------------------------------------
// updateIncremental - 增量更新
//
// 当数据层某区域变化后, 只重算受影响 tile 的 LOD
// --------------------------------------------------------------------------
int LodManager::updateIncremental(const std::string& dbPath, int bandIndex,
                                   const std::vector<int64_t>& changedTiles) {
    fprintf(stderr, "[lod][manager] === updateIncremental 开始: dbPath=%s band=%d changedTiles=%zu ===\n",
            dbPath.c_str(), bandIndex, changedTiles.size());

    if (changedTiles.empty()) {
        fprintf(stderr, "[lod][manager] 无变化的 tile, 跳过\n");
        return 0;
    }

    // 读取数据库配置
    DbConfig config;
    if (!loadDbConfig(dbPath, config)) {
        return -1;
    }

    if (bandIndex < 0 || bandIndex >= (int)config.bands.size()) {
        fprintf(stderr, "[lod][manager] bandIndex %d 超出范围\n", bandIndex);
        return -1;
    }

    initDefaultLevels(config.nsideData);

    int finestLevel = (int)m_levels.size() - 1;
    int tileCount = (int)changedTiles.size();

    // 并行处理变化的 tile
    #pragma omp parallel for schedule(dynamic)
    for (int t = 0; t < tileCount; t++) {
        int64_t tileIpix = changedTiles[t];

        // 重新读取数据层像素
        std::vector<FinePixel> finePixels = readLevelData(
            dbPath, bandIndex, m_levels[finestLevel].nside, tileIpix, config);

        if (finePixels.empty()) {
            #pragma omp critical
            {
                fprintf(stderr, "[lod][manager] 增量更新: tile %lld 无数据, 跳过\n",
                        (long long)tileIpix);
            }
            continue;
        }

        // 逐级降采样 (与 generateFull 相同)
        std::vector<FinePixel> currentFine = finePixels;
        int currentNside = m_levels[finestLevel].nside;

        for (int lv = finestLevel - 1; lv >= 0; lv--) {
            int coarseNside = m_levels[lv].nside;

            std::vector<CoarsePixel> coarsePixels =
                m_downsampler.downsampleIncremental(currentFine, currentNside, coarseNside);

            if (coarsePixels.empty()) break;

            #pragma omp critical
            {
                writeLevelData(dbPath, bandIndex, coarseNside, tileIpix, coarsePixels, config);
            }

            // 准备下一级
            currentFine.clear();
            currentFine.reserve(coarsePixels.size());
            for (const auto& cp : coarsePixels) {
                FinePixel fp;
                fp.ipix   = cp.ipix;
                fp.value  = cp.value;
                fp.weight = cp.weight;
                currentFine.push_back(fp);
            }
            currentNside = coarseNside;
        }
    }

    fprintf(stderr, "[lod][manager] === updateIncremental 完成 ===\n");
    return 0;
}

// --------------------------------------------------------------------------
// computeOnDemand - 按需计算
//
// 浏览器请求某层某区域:
//   1. 若 LOD tile 已存在, 直接读取返回
//   2. 若不存在, 从最接近的更细层 (或数据层) 实时降采样
// --------------------------------------------------------------------------
LodTileData* LodManager::computeOnDemand(const std::string& dbPath, int bandIndex,
                                          int level, int64_t tileIpix) {
    fprintf(stderr, "[lod][manager] === computeOnDemand: level=%d tile=%lld band=%d ===\n",
            level, (long long)tileIpix, bandIndex);

    // 读取配置
    DbConfig config;
    if (!loadDbConfig(dbPath, config)) {
        return nullptr;
    }

    if (bandIndex < 0 || bandIndex >= (int)config.bands.size()) {
        fprintf(stderr, "[lod][manager] bandIndex %d 超出范围\n", bandIndex);
        return nullptr;
    }

    initDefaultLevels(config.nsideData);

    // 校验 level
    if (level < 0 || level >= (int)m_levels.size()) {
        fprintf(stderr, "[lod][manager] level %d 超出范围 (0..%d)\n",
                level, (int)m_levels.size() - 1);
        return nullptr;
    }

    int targetNside = m_levels[level].nside;

    // 1. 检查 LOD tile 是否已存在
    std::vector<CoarsePixel> tileData;
    if (readLodTile(dbPath, bandIndex, targetNside, tileIpix, tileData, config)) {
        // tile 已存在, 直接返回
        LodTileData* result = new LodTileData();
        result->nside = targetNside;
        result->tileIpix = tileIpix;
        result->pixels.resize(tileData.size());
        result->values.resize(tileData.size());
        result->weights.resize(tileData.size());
        result->counts.resize(tileData.size());
        for (size_t i = 0; i < tileData.size(); i++) {
            result->pixels[i]  = tileData[i].ipix;
            result->values[i]  = tileData[i].value;
            result->weights[i] = tileData[i].weight;
            result->counts[i]  = tileData[i].count;
        }
        fprintf(stderr, "[lod][manager] 按需计算: tile 已存在, 直接返回 (%zu 像素)\n",
                tileData.size());
        return result;
    }

    // 2. tile 不存在, 从更细的层实时降采样
    // 找到最接近的可用源层
    int sourceLevel = -1;
    std::vector<FinePixel> sourcePixels;

    // 从 targetLevel+1 向上查找 (更细的层)
    for (int lv = level + 1; lv < (int)m_levels.size(); lv++) {
        int srcNside = m_levels[lv].nside;
        sourcePixels = readLevelData(dbPath, bandIndex, srcNside, tileIpix, config);
        if (!sourcePixels.empty()) {
            sourceLevel = lv;
            break;
        }
    }

    if (sourcePixels.empty()) {
        fprintf(stderr, "[lod][manager] 按需计算: 无可用源数据 (tile=%lld)\n",
                (long long)tileIpix);
        return nullptr;
    }

    // 逐级降采样到目标层
    int srcNside = m_levels[sourceLevel].nside;
    std::vector<FinePixel> currentFine = sourcePixels;
    int currentNside = srcNside;

    for (int lv = sourceLevel - 1; lv >= level; lv--) {
        int coarseNside = m_levels[lv].nside;
        std::vector<CoarsePixel> coarsePixels =
            m_downsampler.downsample(currentFine, currentNside, coarseNside);

        if (coarsePixels.empty()) {
            fprintf(stderr, "[lod][manager] 按需计算: 降采样失败 (level %d→%d)\n",
                    lv + 1, lv);
            return nullptr;
        }

        // 如果到达目标层, 返回结果
        if (lv == level) {
            LodTileData* result = new LodTileData();
            result->nside = coarseNside;
            result->tileIpix = tileIpix;
            result->pixels.resize(coarsePixels.size());
            result->values.resize(coarsePixels.size());
            result->weights.resize(coarsePixels.size());
            result->counts.resize(coarsePixels.size());
            for (size_t i = 0; i < coarsePixels.size(); i++) {
                result->pixels[i]  = coarsePixels[i].ipix;
                result->values[i]  = coarsePixels[i].value;
                result->weights[i] = coarsePixels[i].weight;
                result->counts[i]  = coarsePixels[i].count;
            }
            fprintf(stderr, "[lod][manager] 按需计算完成: %zu 像素 (level=%d nside=%d)\n",
                    coarsePixels.size(), level, coarseNside);

            // 可选: 缓存写入文件 (供后续请求复用)
            writeLevelData(dbPath, bandIndex, coarseNside, tileIpix, coarsePixels, config);

            return result;
        }

        // 继续降采样
        currentFine.clear();
        currentFine.reserve(coarsePixels.size());
        for (const auto& cp : coarsePixels) {
            FinePixel fp;
            fp.ipix   = cp.ipix;
            fp.value  = cp.value;
            fp.weight = cp.weight;
            currentFine.push_back(fp);
        }
        currentNside = coarseNside;
    }

    // 不应到达此处
    fprintf(stderr, "[lod][manager] 按需计算: 意外退出\n");
    return nullptr;
}

} // namespace lod
