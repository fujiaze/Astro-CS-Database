#ifndef STACK_DB_H
#define STACK_DB_H

#include "ahps_reader.h"
#include "ahps_writer.h"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace ahps {

// ============================================================================
// 堆栈数据库配置
// ============================================================================
struct StackDbConfig {
    int                   nsideData = 32768;              // 数据层 nside
    std::vector<int>      nsideLod = {512, 2048, 8192, 32768}; // LOD 层级
    std::vector<std::string> bands = {"L", "R", "G", "B", "Ha", "OIII"}; // 波段名
    int                   tileNside = 512;                // 分区 nside
    double                sigmaClipLow = 3.0;             // sigma-clip 低端阈值
    double                sigmaClipHigh = 3.0;            // sigma-clip 高端阈值
    bool                  nested = true;                  // HEALpix 排列
};

// ============================================================================
// StackDatabase - 堆栈数据库管理器
//
// 目录结构:
//   {dbPath}/meta.json              - 数据库配置
//   {dbPath}/tiles/nside_{N}/tile_{ipix}.ahps  - 各 tile 文件
// ============================================================================

class StackDatabase {
public:
    // 创建新数据库 (写入 meta.json)
    static StackDatabase* create(const std::string& dbPath, const StackDbConfig& config);
    // 打开已有数据库 (读取 meta.json)
    static StackDatabase* open(const std::string& dbPath);

    ~StackDatabase();

    const StackDbConfig& getConfig() const;
    const std::string& getPath() const;

    // 查找 tile 文件路径, 不存在返回空字符串
    std::string findTile(int nside, int64_t tileIpix) const;

    // 列出所有 tile 文件路径
    std::vector<std::string> listTiles() const;

    // 获取/创建 tile 写入器 (若 tile 不存在则创建空文件占位)
    // 调用方负责释放返回的 AhpsWriter*
    AhpsWriter* getOrCreateTileWriter(int64_t tileIpix);

    // 读取 tile (返回 AhpsReader, 调用方负责释放)
    AhpsReader* openTileReader(int64_t tileIpix);

    // 更新 meta.json
    bool saveMeta();

private:
    std::string   m_dbPath;
    StackDbConfig m_config;

    bool loadMeta();
    bool ensureDirectories();

    // tile 文件路径构造
    std::string tilePath(int nside, int64_t tileIpix) const;
    // tiles 根目录
    std::string tilesDir() const;
};

} // namespace ahps

#endif // STACK_DB_H
