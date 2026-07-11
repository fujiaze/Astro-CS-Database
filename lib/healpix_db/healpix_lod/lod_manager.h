#ifndef LOD_MANAGER_H
#define LOD_MANAGER_H

// ============================================================================
// LodManager - LOD 金字塔管理器
//
// 功能:
//   1. generateFull     - 从堆栈数据库生成完整 LOD 金字塔
//   2. updateIncremental- 增量更新 (数据层某区域变化后更新 LOD)
//   3. computeOnDemand  - 按需计算 (浏览器请求某层某区域, 若不存在则实时降采样)
//
// 数据库结构 (复用 healpix_stack 的目录布局):
//   {dbPath}/meta.json                          - 数据库配置
//   {dbPath}/tiles/nside_{N}/tile_{ipix}.ahps   - 数据层 tile (.ahps 格式)
//   {dbPath}/tiles/nside_{N}/tile_{ipix}_b{B}.ahpl - LOD 层 tile (.ahpl 格式)
//
// LOD 生成链:
//   Level 3 (nside=32768, 数据层) → read .ahps
//   Level 2 (nside=8192)          → downsample from Level 3, write .ahpl
//   Level 1 (nside=2048)          → downsample from Level 2, write .ahpl
//   Level 0 (nside=512)           → downsample from Level 1, write .ahpl
// ============================================================================

#include "lod_format.h"
#include "lod_downsample.h"

#include <string>
#include <vector>

namespace lod {

// --------------------------------------------------------------------------
// LOD tile 数据 (用于浏览器渲染)
// --------------------------------------------------------------------------
struct LodTileData {
    int                      nside;      // 像素 nside
    int64_t                  tileIpix;   // tile 索引
    std::vector<uint64_t>    pixels;     // 像素号数组
    std::vector<float>       values;     // 值数组
    std::vector<float>       weights;    // 权重数组
    std::vector<uint16_t>    counts;     // 计数数组
};

// --------------------------------------------------------------------------
// LodManager - LOD 金字塔管理器
// --------------------------------------------------------------------------
class LodManager {
public:
    LodManager();
    ~LodManager();

    // 从堆栈数据库生成完整 LOD 金字塔
    // dbPath: 堆栈数据库路径
    // bandIndex: 波段索引 (0..N-1)
    // 返回: 0=成功, -1=失败
    int generateFull(const std::string& dbPath, int bandIndex);

    // 增量更新: 数据层某区域变化后更新 LOD
    // dbPath: 堆栈数据库路径
    // bandIndex: 波段
    // changedTiles: 变化的 tile ipix 列表 (数据层 tile_nside 级别)
    // 返回: 0=成功, -1=失败
    int updateIncremental(const std::string& dbPath, int bandIndex,
                          const std::vector<int64_t>& changedTiles);

    // 按需计算: 浏览器请求某层某区域, 若不存在则实时降采样
    // dbPath: 数据库路径
    // bandIndex: 波段
    // level: LOD 层级 (0..N-1)
    // tileIpix: 请求的 tile
    // 返回: LodTileData 指针 (调用方 delete), 失败返回 nullptr
    LodTileData* computeOnDemand(const std::string& dbPath, int bandIndex,
                                  int level, int64_t tileIpix);

    // 获取 LOD 层级配置
    const std::vector<LodLevel>& getLevels() const;

    // 设置自定义 LOD 层级
    void setLevels(const std::vector<LodLevel>& levels);

private:
    std::vector<LodLevel> m_levels;
    LodDownsampler        m_downsampler;

    // 数据库配置 (从 meta.json 读取)
    struct DbConfig {
        int                   nsideData  = 32768;  // 数据层 nside
        int                   tileNside  = 512;    // 分区 nside
        std::vector<std::string> bands;           // 波段名列表
        bool                  nested     = true;   // HEALpix 排列
    };

    // 读取数据库配置
    bool loadDbConfig(const std::string& dbPath, DbConfig& config);

    // 初始化默认 LOD 层级
    void initDefaultLevels(int dataNside);

    // 读取某层数据 (数据层 .ahps 或 LOD 层 .ahpl)
    // nside == nsideData 时读取数据层 .ahps, 否则读取 LOD 层 .ahpl
    std::vector<FinePixel> readLevelData(const std::string& dbPath,
                                          int bandIndex, int nside,
                                          int64_t tileIpix,
                                          const DbConfig& config);

    // 写入降采样结果到 LOD 层 (.ahpl)
    int writeLevelData(const std::string& dbPath, int bandIndex,
                       int nside, int64_t tileIpix,
                       const std::vector<CoarsePixel>& data,
                       const DbConfig& config);

    // 读取 LOD 层 tile (.ahpl)
    bool readLodTile(const std::string& dbPath, int bandIndex,
                     int nside, int64_t tileIpix,
                     std::vector<CoarsePixel>& outData,
                     const DbConfig& config);

    // LOD tile 文件路径构造
    std::string lodTilePath(const std::string& dbPath, int nside,
                            int64_t tileIpix, int bandIndex) const;

    // 扫描数据层 tile 列表
    std::vector<int64_t> listDataTiles(const std::string& dbPath,
                                        const DbConfig& config) const;

    // 压缩级别 → codec + zstd level
    void compToCodec(int comp, uint8_t& codec, int& zstdLevel) const;
};

} // namespace lod

#endif // LOD_MANAGER_H
