#include "stack_db.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace ahps {

// ============================================================================
// 辅助函数
// ============================================================================

// UTF-8 路径转 wstring (Windows)
static std::wstring utf8ToWstring(const std::string& s) {
    if (s.empty()) return std::wstring();
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    if (len > 0) ws.resize(len - 1);  // 去掉末尾 \0
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
    // 递归创建: CreateDirectory 不会自动创建父目录, 用 SHCreateDirectory 或逐级创建
    // 这里逐级创建
    std::wstring cur;
    for (size_t i = 0; i < wpath.size(); i++) {
        wchar_t c = wpath[i];
        cur += c;
        if (c == L'\\' || c == L'/' || i == wpath.size() - 1) {
            DWORD attr = GetFileAttributesW(cur.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES) {
                if (!CreateDirectoryW(cur.c_str(), nullptr)) {
                    DWORD err = GetLastError();
                    if (err != ERROR_ALREADY_EXISTS) {
                        // 继续尝试, 父目录可能在后续创建
                    }
                }
            }
        }
    }
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    // 简单 mkdir -p 不可移植, 用递归
    std::string cmd = "mkdir -p \"" + path + "\"";
    return (std::system(cmd.c_str()) == 0);
#endif
}

// 判断文件/目录是否存在
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

// 简单 JSON: 提取数字值
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

// 简单 JSON: 提取字符串数组 (如 ["L","R",...])
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
        // 跳到下一个
        while (pos < json.size() && json[pos] != ']' && json[pos] != '"') pos++;
    }
    return result;
}

// 简单 JSON: 提取整数数组
static std::vector<int> jsonGetIntArray(const std::string& json, const std::string& key) {
    std::vector<int> result;
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    pos++;
    while (pos < json.size() && json[pos] != ']') {
        while (pos < json.size() && (json[pos]==' '||json[pos]==',')) pos++;
        if (pos >= json.size() || json[pos] == ']') break;
        char* end = nullptr;
        long v = std::strtol(json.c_str() + pos, &end, 10);
        if (end == json.c_str() + pos) { pos++; continue; }
        result.push_back((int)v);
        pos = (size_t)(end - json.c_str());
    }
    return result;
}

// ============================================================================
// StackDatabase 实现
// ============================================================================
std::string StackDatabase::tilesDir() const {
    return m_dbPath + "/tiles";
}

std::string StackDatabase::tilePath(int nside, int64_t tileIpix) const {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "nside_%d", nside);
    return tilesDir() + "/" + std::string(buf) + "/tile_" + std::to_string(tileIpix) + ".ahps";
}

bool StackDatabase::ensureDirectories() {
    if (!pathExists(m_dbPath)) {
        if (!makeDir(m_dbPath)) {
            fprintf(stderr, "[ahps][db] 无法创建数据库目录: %s\n", m_dbPath.c_str());
            return false;
        }
    }
    std::string td = tilesDir();
    if (!pathExists(td)) {
        if (!makeDir(td)) {
            fprintf(stderr, "[ahps][db] 无法创建 tiles 目录: %s\n", td.c_str());
            return false;
        }
    }
    // 数据层 nside 子目录
    char nsBuf[64];
    std::snprintf(nsBuf, sizeof(nsBuf), "nside_%d", m_config.nsideData);
    std::string dataDir = td + "/" + nsBuf;
    if (!pathExists(dataDir)) {
        if (!makeDir(dataDir)) {
            fprintf(stderr, "[ahps][db] 无法创建数据层目录: %s\n", dataDir.c_str());
            return false;
        }
    }
    // LOD 层子目录
    for (int n : m_config.nsideLod) {
        if (n == m_config.nsideData) continue;
        char b[64];
        std::snprintf(b, sizeof(b), "nside_%d", n);
        std::string d = td + "/" + b;
        if (!pathExists(d)) makeDir(d);
    }
    return true;
}

bool StackDatabase::saveMeta() {
    std::string metaPath = m_dbPath + "/meta.json";
    FILE* fp = fopenUtf8(metaPath, "wb");
    if (!fp) {
        fprintf(stderr, "[ahps][db] 无法写入 meta.json: %s\n", metaPath.c_str());
        return false;
    }

    std::string json = "{\n";
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "  \"nsideData\": %d,\n  \"tileNside\": %d,\n  \"sigmaClipLow\": %g,\n  \"sigmaClipHigh\": %g,\n  \"nested\": %s,\n",
        m_config.nsideData, m_config.tileNside,
        m_config.sigmaClipLow, m_config.sigmaClipHigh,
        m_config.nested ? "true" : "false");
    json += buf;

    // nsideLod 数组
    json += "  \"nsideLod\": [";
    for (size_t i = 0; i < m_config.nsideLod.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(m_config.nsideLod[i]);
    }
    json += "],\n";

    // bands 数组
    json += "  \"bands\": [";
    for (size_t i = 0; i < m_config.bands.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + m_config.bands[i] + "\"";
    }
    json += "]\n";

    json += "}\n";

    std::fwrite(json.data(), 1, json.size(), fp);
    std::fclose(fp);
    fprintf(stderr, "[ahps][db] meta.json 已保存: %s\n", metaPath.c_str());
    return true;
}

bool StackDatabase::loadMeta() {
    std::string metaPath = m_dbPath + "/meta.json";
    FILE* fp = fopenUtf8(metaPath, "rb");
    if (!fp) {
        fprintf(stderr, "[ahps][db] 无法读取 meta.json: %s\n", metaPath.c_str());
        return false;
    }
    // 读取全部
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(fp); return false; }
    std::string json((size_t)sz, '\0');
    std::fread(&json[0], 1, sz, fp);
    std::fclose(fp);

    double v;
    if (jsonGetNumber(json, "nsideData", &v)) m_config.nsideData = (int)v;
    if (jsonGetNumber(json, "tileNside", &v)) m_config.tileNside = (int)v;
    if (jsonGetNumber(json, "sigmaClipLow", &v)) m_config.sigmaClipLow = v;
    if (jsonGetNumber(json, "sigmaClipHigh", &v)) m_config.sigmaClipHigh = v;
    bool b;
    if (jsonGetBool(json, "nested", &b)) m_config.nested = b;
    m_config.nsideLod = jsonGetIntArray(json, "nsideLod");
    m_config.bands = jsonGetStringArray(json, "bands");
    if (m_config.nsideLod.empty()) m_config.nsideLod = {512, 2048, 8192, 32768};
    if (m_config.bands.empty()) m_config.bands = {"L", "R", "G", "B", "Ha", "OIII"};

    fprintf(stderr, "[ahps][db] meta.json 已加载: nsideData=%d tileNside=%d bands=%zu nested=%d\n",
            m_config.nsideData, m_config.tileNside, m_config.bands.size(), m_config.nested ? 1 : 0);
    return true;
}

StackDatabase* StackDatabase::create(const std::string& dbPath, const StackDbConfig& config) {
    StackDatabase* db = new StackDatabase();
    db->m_dbPath = dbPath;
    db->m_config = config;
    if (!db->ensureDirectories()) {
        delete db;
        return nullptr;
    }
    if (!db->saveMeta()) {
        delete db;
        return nullptr;
    }
    fprintf(stderr, "[ahps][db] 数据库已创建: %s\n", dbPath.c_str());
    return db;
}

StackDatabase* StackDatabase::open(const std::string& dbPath) {
    StackDatabase* db = new StackDatabase();
    db->m_dbPath = dbPath;
    if (!db->loadMeta()) {
        delete db;
        return nullptr;
    }
    if (!db->ensureDirectories()) {
        delete db;
        return nullptr;
    }
    fprintf(stderr, "[ahps][db] 数据库已打开: %s\n", dbPath.c_str());
    return db;
}

StackDatabase::~StackDatabase() {
}

const StackDbConfig& StackDatabase::getConfig() const { return m_config; }
const std::string& StackDatabase::getPath() const { return m_dbPath; }

std::string StackDatabase::findTile(int nside, int64_t tileIpix) const {
    std::string p = tilePath(nside, tileIpix);
    if (pathExists(p)) return p;
    return "";
}

std::vector<std::string> StackDatabase::listTiles() const {
    std::vector<std::string> result;
    std::string td = tilesDir();
    if (!pathExists(td)) return result;
#ifdef _WIN32
    // 用 FindFirstFileW 递归扫描
    std::wstring pattern = utf8ToWstring(td + "/*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return result;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            const wchar_t* name = fd.cFileName;
            if (name[0] == L'.') continue;
            // 子目录 (nside_X), 扫描其下 .ahps
            std::wstring subdir = utf8ToWstring(td) + L"/" + name;
            std::wstring subpat = subdir + L"/*.ahps";
            WIN32_FIND_DATAW fd2;
            HANDLE h2 = FindFirstFileW(subpat.c_str(), &fd2);
            if (h2 == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    // 转 UTF-8 路径
                    int len = WideCharToMultiByte(CP_UTF8, 0, subdir.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string subdir8(len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, subdir.c_str(), -1, &subdir8[0], len, nullptr, nullptr);
                    if (len > 0) subdir8.resize(len - 1);
                    int len2 = WideCharToMultiByte(CP_UTF8, 0, (std::wstring(fd2.cFileName)).c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string fname(len2, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, (std::wstring(fd2.cFileName)).c_str(), -1, &fname[0], len2, nullptr, nullptr);
                    if (len2 > 0) fname.resize(len2 - 1);
                    result.push_back(subdir8 + "/" + fname);
                }
            } while (FindNextFileW(h2, &fd2));
            FindClose(h2);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    // POSIX: 用 opendir (简化, 实际可递归)
    // 此处省略, 主要平台为 Windows
#endif
    return result;
}

AhpsWriter* StackDatabase::getOrCreateTileWriter(int64_t tileIpix) {
    int nside = m_config.nsideData;
    std::string p = tilePath(nside, tileIpix);
    AhpsWriter* w = new AhpsWriter();
    w->setNside(nside);
    w->setTileNside(m_config.tileNside);
    w->setTileIpix(tileIpix);
    w->setBandCount((int)m_config.bands.size());
    // 如果文件已存在, 读取已有像素数据供合并 (此处仅创建空 writer, 合并由 engine 处理)
    (void)p;
    return w;
}

AhpsReader* StackDatabase::openTileReader(int64_t tileIpix) {
    int nside = m_config.nsideData;
    std::string p = findTile(nside, tileIpix);
    if (p.empty()) {
        fprintf(stderr, "[ahps][db] tile 不存在: nside=%d ipix=%lld\n",
                nside, (long long)tileIpix);
        return nullptr;
    }
    AhpsReader* r = new AhpsReader();
    if (!r->open(p)) {
        delete r;
        return nullptr;
    }
    return r;
}

} // namespace ahps
